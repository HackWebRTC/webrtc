/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/androidvideotracksource.h"

#include <utility>

#include "api/videosourceproxy.h"
#include "rtc_base/logging.h"
#include "sdk/android/generated_video_jni/jni/VideoSource_jni.h"

namespace webrtc {
namespace jni {

namespace {
// MediaCodec wants resolution to be divisible by 2.
const int kRequiredResolutionAlignment = 2;

VideoRotation jintToVideoRotation(jint rotation) {
  RTC_DCHECK(rotation == 0 || rotation == 90 || rotation == 180 ||
             rotation == 270);
  return static_cast<VideoRotation>(rotation);
}

AndroidVideoTrackSource* AndroidVideoTrackSourceFromJavaProxy(jlong j_proxy) {
  auto* proxy_source = reinterpret_cast<VideoTrackSourceProxy*>(j_proxy);
  return reinterpret_cast<AndroidVideoTrackSource*>(proxy_source->internal());
}

}  // namespace

AndroidVideoTrackSource::AndroidVideoTrackSource(
    rtc::Thread* signaling_thread,
    JNIEnv* jni,
    bool is_screencast)
    : AdaptedVideoTrackSource(kRequiredResolutionAlignment),
      signaling_thread_(signaling_thread),
      is_screencast_(is_screencast) {
  RTC_LOG(LS_INFO) << "AndroidVideoTrackSource ctor";
  camera_thread_checker_.DetachFromThread();
}
AndroidVideoTrackSource::~AndroidVideoTrackSource() = default;

bool AndroidVideoTrackSource::is_screencast() const {
  return is_screencast_;
}

rtc::Optional<bool> AndroidVideoTrackSource::needs_denoising() const {
  return false;
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

AndroidVideoTrackSource::SourceState AndroidVideoTrackSource::state() const {
  return state_;
}

bool AndroidVideoTrackSource::remote() const {
  return false;
}

void AndroidVideoTrackSource::OnFrameCaptured(
    JNIEnv* jni,
    int width,
    int height,
    int64_t timestamp_ns,
    VideoRotation rotation,
    const JavaRef<jobject>& j_video_frame_buffer) {
  RTC_DCHECK(camera_thread_checker_.CalledOnValidThread());

  int64_t camera_time_us = timestamp_ns / rtc::kNumNanosecsPerMicrosec;
  int64_t translated_camera_time_us =
      timestamp_aligner_.TranslateTimestamp(camera_time_us, rtc::TimeMicros());

  int adapted_width;
  int adapted_height;
  int crop_width;
  int crop_height;
  int crop_x;
  int crop_y;

  if (!AdaptFrame(width, height, camera_time_us, &adapted_width,
                  &adapted_height, &crop_width, &crop_height, &crop_x,
                  &crop_y)) {
    return;
  }

  rtc::scoped_refptr<VideoFrameBuffer> buffer =
      AndroidVideoBuffer::Create(jni, j_video_frame_buffer)
          ->CropAndScale(jni, crop_x, crop_y, crop_width, crop_height,
                         adapted_width, adapted_height);

  // AdaptedVideoTrackSource handles applying rotation for I420 frames.
  if (apply_rotation() && rotation != kVideoRotation_0) {
    buffer = buffer->ToI420();
  }

  OnFrame(VideoFrame(buffer, rotation, translated_camera_time_us));
}

void AndroidVideoTrackSource::OnOutputFormatRequest(int width,
                                                    int height,
                                                    int fps) {
  cricket::VideoFormat format(width, height,
                              cricket::VideoFormat::FpsToInterval(fps), 0);
  video_adapter()->OnOutputFormatRequest(format);
}

static void JNI_VideoSource_OnFrameCaptured(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    jlong j_source,
    jint j_width,
    jint j_height,
    jint j_rotation,
    jlong j_timestamp_ns,
    const JavaParamRef<jobject>& j_video_frame_buffer) {
  AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->OnFrameCaptured(jni, j_width, j_height, j_timestamp_ns,
                          jintToVideoRotation(j_rotation),
                          j_video_frame_buffer);
}

static void JNI_VideoSource_CapturerStarted(JNIEnv* jni,
                                            const JavaParamRef<jclass>&,
                                            jlong j_source,
                                            jboolean j_success) {
  RTC_LOG(LS_INFO) << "AndroidVideoTrackSourceObserve_nativeCapturerStarted";
  AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->SetState(j_success ? AndroidVideoTrackSource::SourceState::kLive
                             : AndroidVideoTrackSource::SourceState::kEnded);
}

static void JNI_VideoSource_CapturerStopped(JNIEnv* jni,
                                            const JavaParamRef<jclass>&,
                                            jlong j_source) {
  RTC_LOG(LS_INFO) << "AndroidVideoTrackSourceObserve_nativeCapturerStopped";
  AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->SetState(AndroidVideoTrackSource::SourceState::kEnded);
}

static void JNI_VideoSource_AdaptOutputFormat(JNIEnv* jni,
                                              const JavaParamRef<jclass>&,
                                              jlong j_source,
                                              jint j_width,
                                              jint j_height,
                                              jint j_fps) {
  RTC_LOG(LS_INFO) << "VideoSource_nativeAdaptOutputFormat";
  AndroidVideoTrackSource* source =
      AndroidVideoTrackSourceFromJavaProxy(j_source);
  source->OnOutputFormatRequest(j_width, j_height, j_fps);
}

}  // namespace jni
}  // namespace webrtc
