/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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
 */


#ifndef TALK_APP_WEBRTC_VOICEENGINE_H_
#define TALK_APP_WEBRTC_VOICEENGINE_H_

#include "talk/base/common.h"
#include "common_types.h"
#include "voice_engine/main/interface/voe_base.h"
#include "voice_engine/main/interface/voe_codec.h"
#include "voice_engine/main/interface/voe_errors.h"
#include "voice_engine/main/interface/voe_file.h"
#include "voice_engine/main/interface/voe_hardware.h"
#include "voice_engine/main/interface/voe_network.h"
#include "voice_engine/main/interface/voe_rtp_rtcp.h"
#include "voice_engine/main/interface/voe_video_sync.h"
#include "voice_engine/main/interface/voe_volume_control.h"

namespace webrtc {

// Tracing helpers, for easy logging when WebRTC calls fail.
// Example: "LOG_RTCERR1(StartSend, channel);" produces the trace
//          "StartSend(1) failed, err=XXXX"
// The method GetLastRtcError must be defined in the calling scope.
#define LOG_RTCERR0(func) \
    LOG_RTCERR0_EX(func, GetLastRtcError())
#define LOG_RTCERR1(func, a1) \
    LOG_RTCERR1_EX(func, a1, GetLastRtcError())
#define LOG_RTCERR2(func, a1, a2) \
    LOG_RTCERR2_EX(func, a1, a2, GetLastRtcError())
#define LOG_RTCERR3(func, a1, a2, a3) \
    LOG_RTCERR3_EX(func, a1, a2, a3, GetLastRtcError())
#define LOG_RTCERR0_EX(func, err) LOG(WARNING) \
    << "" << #func << "() failed, err=" << err
#define LOG_RTCERR1_EX(func, a1, err) LOG(WARNING) \
    << "" << #func << "(" << a1 << ") failed, err=" << err
#define LOG_RTCERR2_EX(func, a1, a2, err) LOG(WARNING) \
    << "" << #func << "(" << a1 << ", " << a2 << ") failed, err=" \
    << err
#define LOG_RTCERR3_EX(func, a1, a2, a3, err) LOG(WARNING) \
    << "" << #func << "(" << a1 << ", " << a2 << ", " << a3 \
    << ") failed, err=" << err

// automatically handles lifetime of WebRtc VoiceEngine
class scoped_webrtc_engine {
 public:
  explicit scoped_webrtc_engine(VoiceEngine* e) : ptr(e) {}
  // VERIFY, to ensure that there are no leaks at shutdown
  ~scoped_webrtc_engine() { if (ptr) VERIFY(VoiceEngine::Delete(ptr)); }
  VoiceEngine* get() const { return ptr; }
 private:
  VoiceEngine* ptr;
};

// scoped_ptr class to handle obtaining and releasing WebRTC interface pointers
template<class T>
class scoped_rtc_ptr {
 public:
  explicit scoped_rtc_ptr(const scoped_webrtc_engine& e)
      : ptr(T::GetInterface(e.get())) {}
  template <typename E>
  explicit scoped_rtc_ptr(E* engine) : ptr(T::GetInterface(engine)) {}
  explicit scoped_rtc_ptr(T* p) : ptr(p) {}
  ~scoped_rtc_ptr() { if (ptr) ptr->Release(); }
  T* operator->() const { return ptr; }
  T* get() const { return ptr; }

  // Queries the engine for the wrapped type and releases the current pointer.
  template <typename E>
  void reset(E* engine) {
    reset();
    if (engine)
      ptr = T::GetInterface(engine);
  }

  // Releases the current pointer.
  void reset() {
    if (ptr) {
      ptr->Release();
      ptr = NULL;
    }
  }

 private:
  T* ptr;
};

// Utility class for aggregating the various WebRTC interface.
// Fake implementations can also be injected for testing.
class RtcWrapper {
 public:
  RtcWrapper()
      : engine_(VoiceEngine::Create()),
        base_(engine_), codec_(engine_), file_(engine_),
        hw_(engine_), network_(engine_), rtp_(engine_),
        sync_(engine_), volume_(engine_) {

  }
  RtcWrapper(VoEBase* base, VoECodec* codec, VoEFile* file,
              VoEHardware* hw, VoENetwork* network,
              VoERTP_RTCP* rtp, VoEVideoSync* sync,
              VoEVolumeControl* volume)
      : engine_(NULL),
        base_(base), codec_(codec), file_(file),
        hw_(hw), network_(network), rtp_(rtp),
        sync_(sync), volume_(volume) {

  }
  virtual ~RtcWrapper() {}
  VoiceEngine* engine() { return engine_.get(); }
  VoEBase* base() { return base_.get(); }
  VoECodec* codec() { return codec_.get(); }
  VoEFile* file() { return file_.get(); }
  VoEHardware* hw() { return hw_.get(); }
  VoENetwork* network() { return network_.get(); }
  VoERTP_RTCP* rtp() { return rtp_.get(); }
  VoEVideoSync* sync() { return sync_.get(); }
  VoEVolumeControl* volume() { return volume_.get(); }
  int error() { return base_->LastError(); }

 private:
  scoped_webrtc_engine engine_;
  scoped_rtc_ptr<VoEBase> base_;
  scoped_rtc_ptr<VoECodec> codec_;
  scoped_rtc_ptr<VoEFile> file_;
  scoped_rtc_ptr<VoEHardware> hw_;
  scoped_rtc_ptr<VoENetwork> network_;
  scoped_rtc_ptr<VoERTP_RTCP> rtp_;
  scoped_rtc_ptr<VoEVideoSync> sync_;
  scoped_rtc_ptr<VoEVolumeControl> volume_;
};
} //namespace webrtc

#endif  // TALK_APP_WEBRTC_VOICEENGINE_H_
