/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/androidvideotracksource.h"

#include <utility>

#include "third_party/libyuv/include/libyuv/rotate.h"

namespace webrtc {

AndroidVideoTrackSource::AndroidVideoTrackSource(rtc::Thread* signaling_thread,
                                                 JNIEnv* jni,
                                                 jobject j_egl_context,
                                                 bool is_screencast)
    : signaling_thread_(signaling_thread),
      surface_texture_helper_(webrtc_jni::SurfaceTextureHelper::create(
          jni,
          "Camera SurfaceTextureHelper",
          j_egl_context)),
      is_screencast_(is_screencast) {
  LOG(LS_INFO) << "AndroidVideoTrackSource ctor";
  worker_thread_checker_.DetachFromThread();
  camera_thread_checker_.DetachFromThread();
}

bool AndroidVideoTrackSource::GetStats(AndroidVideoTrackSource::Stats* stats) {
  rtc::CritScope lock(&stats_crit_);

  if (!stats_) {
    return false;
  }

  *stats = *stats_;
  return true;
}

void AndroidVideoTrackSource::SetState(SourceState state) {
  if (rtc::Thread::Current() != signaling_thread_) {
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, signaling_thread_,
        rtc::Bind(&AndroidVideoTrackSource::SetState, this, state));
    return;
  }

  if (state_ != state) {
    state_ = state;
    FireOnChanged();
  }
}

void AndroidVideoTrackSource::AddOrUpdateSink(
    rtc::VideoSinkInterface<cricket::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());

  broadcaster_.AddOrUpdateSink(sink, wants);
  OnSinkWantsChanged(broadcaster_.wants());
}

void AndroidVideoTrackSource::RemoveSink(
    rtc::VideoSinkInterface<cricket::VideoFrame>* sink) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());

  broadcaster_.RemoveSink(sink);
  OnSinkWantsChanged(broadcaster_.wants());
}

void AndroidVideoTrackSource::OnSinkWantsChanged(
    const rtc::VideoSinkWants& wants) {
  {
    rtc::CritScope lock(&apply_rotation_crit_);
    apply_rotation_ = wants.rotation_applied;
  }

  video_adapter_.OnResolutionRequest(wants.max_pixel_count,
                                     wants.max_pixel_count_step_up);
}

void AndroidVideoTrackSource::OnByteBufferFrameCaptured(const void* frame_data,
                                                        int length,
                                                        int width,
                                                        int height,
                                                        int rotation,
                                                        int64_t timestamp_ns) {
  RTC_DCHECK(camera_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(rotation == 0 || rotation == 90 || rotation == 180 ||
             rotation == 270);

  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;
  int64_t translated_camera_time_us;

  if (!AdaptFrame(width, height, timestamp_ns / rtc::kNumNanosecsPerMicrosec,
                  &adapted_width, &adapted_height, &crop_width, &crop_height,
                  &crop_x, &crop_y, &translated_camera_time_us)) {
    return;
  }

  const uint8_t* y_plane = static_cast<const uint8_t*>(frame_data);
  const uint8_t* uv_plane = y_plane + width * height;
  const int uv_width = (width + 1) / 2;

  RTC_CHECK_GE(length, width * height + 2 * uv_width * ((height + 1) / 2));

  // Can only crop at even pixels.
  crop_x &= ~1;
  crop_y &= ~1;
  // Crop just by modifying pointers.
  y_plane += width * crop_y + crop_x;
  uv_plane += uv_width * crop_y + crop_x;

  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      buffer_pool_.CreateBuffer(adapted_width, adapted_height);

  nv12toi420_scaler_.NV12ToI420Scale(
      y_plane, width,
      uv_plane, uv_width * 2,
      crop_width, crop_height,
      buffer->MutableDataY(), buffer->StrideY(),
      // Swap U and V, since we have NV21, not NV12.
      buffer->MutableDataV(), buffer->StrideV(),
      buffer->MutableDataU(), buffer->StrideU(),
      buffer->width(), buffer->height());

  // Applying rotation is only supported for legacy reasons, and the performance
  // for this path is not critical.
  rtc::CritScope lock(&apply_rotation_crit_);
  if (apply_rotation_ && rotation != 0) {
    rtc::scoped_refptr<I420Buffer> rotated_buffer =
        rotation == 180 ? I420Buffer::Create(buffer->width(), buffer->height())
                        : I420Buffer::Create(buffer->height(), buffer->width());

    libyuv::I420Rotate(
        buffer->DataY(), buffer->StrideY(),
        buffer->DataU(), buffer->StrideU(),
        buffer->DataV(), buffer->StrideV(),
        rotated_buffer->MutableDataY(), rotated_buffer->StrideY(),
        rotated_buffer->MutableDataU(), rotated_buffer->StrideU(),
        rotated_buffer->MutableDataV(), rotated_buffer->StrideV(),
        buffer->width(), buffer->height(),
        static_cast<libyuv::RotationMode>(rotation));

    buffer = rotated_buffer;
  }

  OnFrame(cricket::WebRtcVideoFrame(
              buffer,
              apply_rotation_ ? webrtc::kVideoRotation_0
                              : static_cast<webrtc::VideoRotation>(rotation),
              translated_camera_time_us, 0),
          width, height);
}

void AndroidVideoTrackSource::OnTextureFrameCaptured(
    int width,
    int height,
    int rotation,
    int64_t timestamp_ns,
    const webrtc_jni::NativeHandleImpl& handle) {
  RTC_DCHECK(camera_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(rotation == 0 || rotation == 90 || rotation == 180 ||
             rotation == 270);

  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;
  int64_t translated_camera_time_us;

  if (!AdaptFrame(width, height, timestamp_ns / rtc::kNumNanosecsPerMicrosec,
                  &adapted_width, &adapted_height, &crop_width, &crop_height,
                  &crop_x, &crop_y, &translated_camera_time_us)) {
    surface_texture_helper_->ReturnTextureFrame();
    return;
  }

  webrtc_jni::Matrix matrix = handle.sampling_matrix;

  matrix.Crop(crop_width / static_cast<float>(width),
              crop_height / static_cast<float>(height),
              crop_x / static_cast<float>(width),
              crop_y / static_cast<float>(height));

  rtc::CritScope lock(&apply_rotation_crit_);
  if (apply_rotation_) {
    if (rotation == webrtc::kVideoRotation_90 ||
        rotation == webrtc::kVideoRotation_270) {
      std::swap(adapted_width, adapted_height);
    }
    matrix.Rotate(static_cast<webrtc::VideoRotation>(rotation));
  }

  OnFrame(cricket::WebRtcVideoFrame(
              surface_texture_helper_->CreateTextureFrame(
                  adapted_width, adapted_height,
                  webrtc_jni::NativeHandleImpl(handle.oes_texture_id, matrix)),
              apply_rotation_ ? webrtc::kVideoRotation_0
                              : static_cast<webrtc::VideoRotation>(rotation),
              translated_camera_time_us, 0),
          width, height);
}

void AndroidVideoTrackSource::OnFrame(const cricket::VideoFrame& frame,
                                      int width,
                                      int height) {
  {
    rtc::CritScope lock(&stats_crit_);
    stats_ = rtc::Optional<AndroidVideoTrackSource::Stats>({width, height});
  }

  broadcaster_.OnFrame(frame);
}

void AndroidVideoTrackSource::OnOutputFormatRequest(int width,
                                                    int height,
                                                    int fps) {
  RTC_DCHECK(camera_thread_checker_.CalledOnValidThread());

  cricket::VideoFormat format(width, height,
                              cricket::VideoFormat::FpsToInterval(fps), 0);
  video_adapter_.OnOutputFormatRequest(format);
}

bool AndroidVideoTrackSource::AdaptFrame(int width,
                                         int height,
                                         int64_t camera_time_us,
                                         int* out_width,
                                         int* out_height,
                                         int* crop_width,
                                         int* crop_height,
                                         int* crop_x,
                                         int* crop_y,
                                         int64_t* translated_camera_time_us) {
  RTC_DCHECK(camera_thread_checker_.CalledOnValidThread());

  int64_t system_time_us = rtc::TimeMicros();
  *translated_camera_time_us =
      timestamp_aligner_.TranslateTimestamp(camera_time_us, system_time_us);

  if (!broadcaster_.frame_wanted()) {
    return false;
  }

  if (!video_adapter_.AdaptFrameResolution(
          width, height, camera_time_us * rtc::kNumNanosecsPerMicrosec,
          crop_width, crop_height, out_width, out_height)) {
    // VideoAdapter dropped the frame.
    return false;
  }
  *crop_x = (width - *crop_width) / 2;
  *crop_y = (height - *crop_height) / 2;

  return true;
}

}  // namespace webrtc
