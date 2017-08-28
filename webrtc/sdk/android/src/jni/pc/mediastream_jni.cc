/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

JNI_FUNCTION_DECLARATION(jboolean,
                         MediaStream_nativeAddAudioTrack,
                         JNIEnv* jni,
                         jclass,
                         jlong pointer,
                         jlong j_audio_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->AddTrack(
      reinterpret_cast<AudioTrackInterface*>(j_audio_track_pointer));
}

JNI_FUNCTION_DECLARATION(jboolean,
                         MediaStream_nativeAddVideoTrack,
                         JNIEnv* jni,
                         jclass,
                         jlong pointer,
                         jlong j_video_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->AddTrack(
      reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer));
}

JNI_FUNCTION_DECLARATION(jboolean,
                         MediaStream_nativeRemoveAudioTrack,
                         JNIEnv* jni,
                         jclass,
                         jlong pointer,
                         jlong j_audio_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->RemoveTrack(
      reinterpret_cast<AudioTrackInterface*>(j_audio_track_pointer));
}

JNI_FUNCTION_DECLARATION(jboolean,
                         MediaStream_nativeRemoveVideoTrack,
                         JNIEnv* jni,
                         jclass,
                         jlong pointer,
                         jlong j_video_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->RemoveTrack(
      reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer));
}

JNI_FUNCTION_DECLARATION(jstring,
                         MediaStream_nativeLabel,
                         JNIEnv* jni,
                         jclass,
                         jlong j_p) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<MediaStreamInterface*>(j_p)->label());
}

JNI_FUNCTION_DECLARATION(void, MediaStream_free, JNIEnv*, jclass, jlong j_p) {
  CHECK_RELEASE(reinterpret_cast<MediaStreamInterface*>(j_p));
}

}  // namespace jni
}  // namespace webrtc
