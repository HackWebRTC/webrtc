/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <jni.h>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/sdk/android/src/jni/classreferenceholder.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"
#include "webrtc/sdk/android/src/jni/native_handle_impl.h"

namespace webrtc {
namespace jni {

namespace {

class VideoSinkWrapper : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  VideoSinkWrapper(JNIEnv* jni, jobject j_sink);
  ~VideoSinkWrapper() override {}

 private:
  void OnFrame(const VideoFrame& frame) override;

  jmethodID j_on_frame_method_;

  const JavaVideoFrameFactory java_video_frame_factory_;
  const ScopedGlobalRef<jobject> j_sink_;
};

VideoSinkWrapper::VideoSinkWrapper(JNIEnv* jni, jobject j_sink)
    : java_video_frame_factory_(jni), j_sink_(jni, j_sink) {
  jclass j_video_sink_class = FindClass(jni, "org/webrtc/VideoSink");
  j_on_frame_method_ = jni->GetMethodID(j_video_sink_class, "onFrame",
                                        "(Lorg/webrtc/VideoFrame;)V");
}

void VideoSinkWrapper::OnFrame(const VideoFrame& frame) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jni->CallVoidMethod(*j_sink_, j_on_frame_method_,
                      java_video_frame_factory_.ToJavaFrame(jni, frame));
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_webrtc_VideoTrack_nativeAddSink(JNIEnv* jni,
                                         jclass,
                                         jlong j_native_track,
                                         jlong j_native_sink) {
  reinterpret_cast<VideoTrackInterface*>(j_native_track)
      ->AddOrUpdateSink(
          reinterpret_cast<rtc::VideoSinkInterface<VideoFrame>*>(j_native_sink),
          rtc::VideoSinkWants());
}

extern "C" JNIEXPORT void JNICALL
Java_org_webrtc_VideoTrack_nativeRemoveSink(JNIEnv* jni,
                                            jclass,
                                            jlong j_native_track,
                                            jlong j_native_sink) {
  reinterpret_cast<VideoTrackInterface*>(j_native_track)
      ->RemoveSink(reinterpret_cast<rtc::VideoSinkInterface<VideoFrame>*>(
          j_native_sink));
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_webrtc_VideoTrack_nativeWrapSink(JNIEnv* jni, jclass, jobject sink) {
  return jlongFromPointer(new VideoSinkWrapper(jni, sink));
}

extern "C" JNIEXPORT void JNICALL
Java_org_webrtc_VideoTrack_nativeFreeSink(JNIEnv* jni,
                                          jclass,
                                          jlong j_native_sink) {
  delete reinterpret_cast<rtc::VideoSinkInterface<VideoFrame>*>(j_native_sink);
}

}  // namespace jni
}  // namespace webrtc
