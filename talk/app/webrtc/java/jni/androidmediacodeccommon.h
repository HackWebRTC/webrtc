/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef TALK_APP_WEBRTC_JAVA_JNI_ANDROIDMEDIACODECCOMMON_H_
#define TALK_APP_WEBRTC_JAVA_JNI_ANDROIDMEDIACODECCOMMON_H_

#include <android/log.h>
#include "talk/app/webrtc/java/jni/classreferenceholder.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/logging.h"
#include "webrtc/system_wrappers/include/tick_util.h"

namespace webrtc_jni {

// Uncomment this define to enable verbose logging for every encoded/decoded
// video frame.
//#define TRACK_BUFFER_TIMING

#define TAG "MediaCodecVideo"
#ifdef TRACK_BUFFER_TIMING
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#endif
#define ALOGD LOG_TAG(rtc::LS_INFO, TAG)
#define ALOGW LOG_TAG(rtc::LS_WARNING, TAG)
#define ALOGE LOG_TAG(rtc::LS_ERROR, TAG)

// Color formats supported by encoder - should mirror supportedColorList
// from MediaCodecVideoEncoder.java
enum COLOR_FORMATTYPE {
  COLOR_FormatYUV420Planar = 0x13,
  COLOR_FormatYUV420SemiPlanar = 0x15,
  COLOR_QCOM_FormatYUV420SemiPlanar = 0x7FA30C00,
  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  // This format is presumably similar to COLOR_FormatYUV420SemiPlanar,
  // but requires some (16, 32?) byte alignment.
  COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04
};

// Arbitrary interval to poll the codec for new outputs.
enum { kMediaCodecPollMs = 10 };
// Media codec maximum output buffer ready timeout.
enum { kMediaCodecTimeoutMs = 1000 };
// Interval to print codec statistics (bitrate, fps, encoding/decoding time).
enum { kMediaCodecStatisticsIntervalMs = 3000 };
// Maximum amount of pending frames for VP8 decoder.
enum { kMaxPendingFramesVp8 = 1 };
// Maximum amount of pending frames for H.264 decoder.
enum { kMaxPendingFramesH264 = 30 };
// Maximum amount of decoded frames for which per-frame logging is enabled.
enum { kMaxDecodedLogFrames = 5 };

static inline int64_t GetCurrentTimeMs() {
  return webrtc::TickTime::Now().Ticks() / 1000000LL;
}

static inline void AllowBlockingCalls() {
  rtc::Thread* current_thread = rtc::Thread::Current();
  if (current_thread != NULL)
    current_thread->SetAllowBlockingCalls(true);
}

// Return the (singleton) Java Enum object corresponding to |index|;
// |state_class_fragment| is something like "MediaSource$State".
static inline jobject JavaEnumFromIndex(
    JNIEnv* jni, const std::string& state_class_fragment, int index) {
  const std::string state_class = "org/webrtc/" + state_class_fragment;
  return JavaEnumFromIndex(jni, FindClass(jni, state_class.c_str()),
                           state_class, index);
}

// Checks for any Java exception, prints stack backtrace and clears
// currently thrown exception.
static inline bool CheckException(JNIEnv* jni) {
  if (jni->ExceptionCheck()) {
    ALOGE << "Java JNI exception.";
    jni->ExceptionDescribe();
    jni->ExceptionClear();
    return true;
  }
  return false;
}

}  // namespace webrtc_jni

#endif  // TALK_APP_WEBRTC_JAVA_JNI_ANDROIDMEDIACODECCOMMON_H_
