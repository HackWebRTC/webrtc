/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_ANDROIDVIDEOTRACKSOURCE_H_
#define WEBRTC_API_ANDROIDVIDEOTRACKSOURCE_H_

#include "webrtc/api/android/jni/native_handle_impl.h"
#include "webrtc/api/android/jni/surfacetexturehelper_jni.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/notifier.h"
#include "webrtc/base/asyncinvoker.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/base/timestampaligner.h"
#include "webrtc/common_video/include/i420_buffer_pool.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/media/base/videoadapter.h"
#include "webrtc/media/base/videobroadcaster.h"
#include "webrtc/media/base/videosinkinterface.h"

namespace webrtc {

class AndroidVideoTrackSource : public Notifier<VideoTrackSourceInterface> {
 public:
  AndroidVideoTrackSource(rtc::Thread* signaling_thread,
                          JNIEnv* jni,
                          jobject j_egl_context,
                          bool is_screencast = false);

  bool is_screencast() const override { return is_screencast_; }

  // Indicates that the encoder should denoise video before encoding it.
  // If it is not set, the default configuration is used which is different
  // depending on video codec.
  rtc::Optional<bool> needs_denoising() const override {
    return rtc::Optional<bool>(false);
  }

  // Returns false if no stats are available, e.g, for a remote
  // source, or a source which has not seen its first frame yet.
  // Should avoid blocking.
  bool GetStats(Stats* stats) override;

  // Called by the native capture observer
  void SetState(SourceState state);

  SourceState state() const override { return state_; }

  bool remote() const override { return false; }

  void AddOrUpdateSink(rtc::VideoSinkInterface<cricket::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<cricket::VideoFrame>* sink) override;

  void OnByteBufferFrameCaptured(const void* frame_data,
                                 int length,
                                 int width,
                                 int height,
                                 int rotation,
                                 int64_t timestamp_ns);

  void OnTextureFrameCaptured(int width,
                              int height,
                              int rotation,
                              int64_t timestamp_ns,
                              const webrtc_jni::NativeHandleImpl& handle);

  void OnOutputFormatRequest(int width, int height, int fps);

  rtc::scoped_refptr<webrtc_jni::SurfaceTextureHelper>
  surface_texture_helper() {
    return surface_texture_helper_;
  }

 private:
  rtc::Thread* signaling_thread_;
  rtc::AsyncInvoker invoker_;
  rtc::ThreadChecker worker_thread_checker_;
  rtc::ThreadChecker camera_thread_checker_;
  rtc::CriticalSection stats_crit_;
  rtc::Optional<Stats> stats_ GUARDED_BY(stats_crit_);
  SourceState state_;
  rtc::VideoBroadcaster broadcaster_;
  rtc::TimestampAligner timestamp_aligner_;
  cricket::VideoAdapter video_adapter_;
  rtc::CriticalSection apply_rotation_crit_;
  bool apply_rotation_ GUARDED_BY(apply_rotation_crit_);
  webrtc::NV12ToI420Scaler nv12toi420_scaler_;
  webrtc::I420BufferPool buffer_pool_;
  rtc::scoped_refptr<webrtc_jni::SurfaceTextureHelper> surface_texture_helper_;
  const bool is_screencast_;

  void OnFrame(const cricket::VideoFrame& frame, int width, int height);

  void OnSinkWantsChanged(const rtc::VideoSinkWants& wants);

  bool AdaptFrame(int width,
                  int height,
                  int64_t camera_time_us,
                  int* out_width,
                  int* out_height,
                  int* crop_width,
                  int* crop_height,
                  int* crop_x,
                  int* crop_y,
                  int64_t* translated_camera_time_us);
};

}  // namespace webrtc

#endif  // WEBRTC_API_ANDROIDVIDEOTRACKSOURCE_H_
