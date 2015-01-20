/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#ifndef TALK_MEDIA_WEBRTCVOE_H_
#define TALK_MEDIA_WEBRTCVOE_H_

#include "talk/media/webrtc/webrtccommon.h"
#include "webrtc/base/common.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_dtmf.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/include/voe_external_media.h"
#include "webrtc/voice_engine/include/voe_file.h"
#include "webrtc/voice_engine/include/voe_hardware.h"
#include "webrtc/voice_engine/include/voe_neteq_stats.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"

namespace cricket {
// automatically handles lifetime of WebRtc VoiceEngine
class scoped_voe_engine {
 public:
  explicit scoped_voe_engine(webrtc::VoiceEngine* e) : ptr(e) {}
  // VERIFY, to ensure that there are no leaks at shutdown
  ~scoped_voe_engine() { if (ptr) VERIFY(webrtc::VoiceEngine::Delete(ptr)); }
  // Releases the current pointer.
  void reset() {
    if (ptr) {
      VERIFY(webrtc::VoiceEngine::Delete(ptr));
      ptr = NULL;
    }
  }
  webrtc::VoiceEngine* get() const { return ptr; }
 private:
  webrtc::VoiceEngine* ptr;
};

// scoped_ptr class to handle obtaining and releasing WebRTC interface pointers
template<class T>
class scoped_voe_ptr {
 public:
  explicit scoped_voe_ptr(const scoped_voe_engine& e)
      : ptr(T::GetInterface(e.get())) {}
  explicit scoped_voe_ptr(T* p) : ptr(p) {}
  ~scoped_voe_ptr() { if (ptr) ptr->Release(); }
  T* operator->() const { return ptr; }
  T* get() const { return ptr; }

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
class VoEWrapper {
 public:
  VoEWrapper()
      : engine_(webrtc::VoiceEngine::Create()), processing_(engine_),
        base_(engine_), codec_(engine_), dtmf_(engine_), file_(engine_),
        hw_(engine_), media_(engine_), neteq_(engine_), network_(engine_),
        rtp_(engine_), sync_(engine_), volume_(engine_) {
  }
  VoEWrapper(webrtc::VoEAudioProcessing* processing,
             webrtc::VoEBase* base,
             webrtc::VoECodec* codec,
             webrtc::VoEDtmf* dtmf,
             webrtc::VoEFile* file,
             webrtc::VoEHardware* hw,
             webrtc::VoEExternalMedia* media,
             webrtc::VoENetEqStats* neteq,
             webrtc::VoENetwork* network,
             webrtc::VoERTP_RTCP* rtp,
             webrtc::VoEVideoSync* sync,
             webrtc::VoEVolumeControl* volume)
      : engine_(NULL),
        processing_(processing),
        base_(base),
        codec_(codec),
        dtmf_(dtmf),
        file_(file),
        hw_(hw),
        media_(media),
        neteq_(neteq),
        network_(network),
        rtp_(rtp),
        sync_(sync),
        volume_(volume) {
  }
  ~VoEWrapper() {}
  webrtc::VoiceEngine* engine() const { return engine_.get(); }
  webrtc::VoEAudioProcessing* processing() const { return processing_.get(); }
  webrtc::VoEBase* base() const { return base_.get(); }
  webrtc::VoECodec* codec() const { return codec_.get(); }
  webrtc::VoEDtmf* dtmf() const { return dtmf_.get(); }
  webrtc::VoEFile* file() const { return file_.get(); }
  webrtc::VoEHardware* hw() const { return hw_.get(); }
  webrtc::VoEExternalMedia* media() const { return media_.get(); }
  webrtc::VoENetEqStats* neteq() const { return neteq_.get(); }
  webrtc::VoENetwork* network() const { return network_.get(); }
  webrtc::VoERTP_RTCP* rtp() const { return rtp_.get(); }
  webrtc::VoEVideoSync* sync() const { return sync_.get(); }
  webrtc::VoEVolumeControl* volume() const { return volume_.get(); }
  int error() { return base_->LastError(); }

 private:
  scoped_voe_engine engine_;
  scoped_voe_ptr<webrtc::VoEAudioProcessing> processing_;
  scoped_voe_ptr<webrtc::VoEBase> base_;
  scoped_voe_ptr<webrtc::VoECodec> codec_;
  scoped_voe_ptr<webrtc::VoEDtmf> dtmf_;
  scoped_voe_ptr<webrtc::VoEFile> file_;
  scoped_voe_ptr<webrtc::VoEHardware> hw_;
  scoped_voe_ptr<webrtc::VoEExternalMedia> media_;
  scoped_voe_ptr<webrtc::VoENetEqStats> neteq_;
  scoped_voe_ptr<webrtc::VoENetwork> network_;
  scoped_voe_ptr<webrtc::VoERTP_RTCP> rtp_;
  scoped_voe_ptr<webrtc::VoEVideoSync> sync_;
  scoped_voe_ptr<webrtc::VoEVolumeControl> volume_;
};

// Adds indirection to static WebRtc functions, allowing them to be mocked.
class VoETraceWrapper {
 public:
  virtual ~VoETraceWrapper() {}

  virtual int SetTraceFilter(const unsigned int filter) {
    return webrtc::VoiceEngine::SetTraceFilter(filter);
  }
  virtual int SetTraceFile(const char* fileNameUTF8) {
    return webrtc::VoiceEngine::SetTraceFile(fileNameUTF8);
  }
  virtual int SetTraceCallback(webrtc::TraceCallback* callback) {
    return webrtc::VoiceEngine::SetTraceCallback(callback);
  }
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTCVOE_H_
