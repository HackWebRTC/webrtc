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


#ifndef TALK_APP_WEBRTC_VIDEOENGINE_H_
#define TALK_APP_WEBRTC_VIDEOENGINE_H_

#include "talk/base/common.h"
#include "common_types.h"
#include "video_engine/main/interface/vie_base.h"
#include "video_engine/main/interface/vie_capture.h"
#include "video_engine/main/interface/vie_codec.h"
#include "video_engine/main/interface/vie_errors.h"
#include "video_engine/main/interface/vie_image_process.h"
#include "video_engine/main/interface/vie_network.h"
#include "video_engine/main/interface/vie_render.h"
#include "video_engine/main/interface/vie_rtp_rtcp.h"

namespace webrtc {

// all tracing macros should go to a common file

// automatically handles lifetime of VideoEngine
class scoped_video_engine {
 public:
  explicit scoped_video_engine(VideoEngine* e) : ptr(e) {}
  // VERIFY, to ensure that there are no leaks at shutdown
  ~scoped_video_engine() {
    if (ptr) {
      VideoEngine::Delete(ptr);
    }
  }
  VideoEngine* get() const { return ptr; }
 private:
  VideoEngine* ptr;
};

// scoped_ptr class to handle obtaining and releasing VideoEngine
// interface pointers
template<class T> class scoped_video_ptr {
 public:
  explicit scoped_video_ptr(const scoped_video_engine& e)
       : ptr(T::GetInterface(e.get())) {}
  explicit scoped_video_ptr(T* p) : ptr(p) {}
  ~scoped_video_ptr() { if (ptr) ptr->Release(); }
  T* operator->() const { return ptr; }
  T* get() const { return ptr; }
 private:
  T* ptr;
};

// Utility class for aggregating the various WebRTC interface.
// Fake implementations can also be injected for testing.
class VideoEngineWrapper {
 public:
  VideoEngineWrapper()
      : engine_(VideoEngine::Create()),
        base_(engine_), codec_(engine_), capture_(engine_),
        network_(engine_), render_(engine_), rtp_(engine_),
        image_(engine_) {
  }

  VideoEngineWrapper(ViEBase* base, ViECodec* codec, ViECapture* capture,
              ViENetwork* network, ViERender* render,
              ViERTP_RTCP* rtp, ViEImageProcess* image)
      : engine_(NULL),
        base_(base), codec_(codec), capture_(capture),
        network_(network), render_(render), rtp_(rtp),
        image_(image) {
  }

  virtual ~VideoEngineWrapper() {}
  VideoEngine* engine() { return engine_.get(); }
  ViEBase* base() { return base_.get(); }
  ViECodec* codec() { return codec_.get(); }
  ViECapture* capture() { return capture_.get(); }
  ViENetwork* network() { return network_.get(); }
  ViERender* render() { return render_.get(); }
  ViERTP_RTCP* rtp() { return rtp_.get(); }
  ViEImageProcess* sync() { return image_.get(); }
  int error() { return base_->LastError(); }

 private:
  scoped_video_engine engine_;
  scoped_video_ptr<ViEBase> base_;
  scoped_video_ptr<ViECodec> codec_;
  scoped_video_ptr<ViECapture> capture_;
  scoped_video_ptr<ViENetwork> network_;
  scoped_video_ptr<ViERender> render_;
  scoped_video_ptr<ViERTP_RTCP> rtp_;
  scoped_video_ptr<ViEImageProcess> image_;
};

} //namespace webrtc

#endif  // TALK_APP_WEBRTC_VOICEENGINE_H_
