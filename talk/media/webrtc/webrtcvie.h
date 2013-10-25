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


#ifndef TALK_MEDIA_WEBRTCVIE_H_
#define TALK_MEDIA_WEBRTCVIE_H_

#include "talk/base/common.h"
#include "talk/media/webrtc/webrtccommon.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_capture/include/video_capture.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_render/include/video_render.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_errors.h"
#include "webrtc/video_engine/include/vie_external_codec.h"
#include "webrtc/video_engine/include/vie_image_process.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/new_include/frame_callback.h"

namespace cricket {

// all tracing macros should go to a common file

// automatically handles lifetime of VideoEngine
class scoped_vie_engine {
 public:
  explicit scoped_vie_engine(webrtc::VideoEngine* e) : ptr(e) {}
  // VERIFY, to ensure that there are no leaks at shutdown
  ~scoped_vie_engine() {
    if (ptr) {
      webrtc::VideoEngine::Delete(ptr);
    }
  }
  webrtc::VideoEngine* get() const { return ptr; }
 private:
  webrtc::VideoEngine* ptr;
};

// scoped_ptr class to handle obtaining and releasing VideoEngine
// interface pointers
template<class T> class scoped_vie_ptr {
 public:
  explicit scoped_vie_ptr(const scoped_vie_engine& e)
       : ptr(T::GetInterface(e.get())) {}
  explicit scoped_vie_ptr(T* p) : ptr(p) {}
  ~scoped_vie_ptr() { if (ptr) ptr->Release(); }
  T* operator->() const { return ptr; }
  T* get() const { return ptr; }
 private:
  T* ptr;
};

// Utility class for aggregating the various WebRTC interface.
// Fake implementations can also be injected for testing.
class ViEWrapper {
 public:
  ViEWrapper()
      : engine_(webrtc::VideoEngine::Create()),
        base_(engine_), codec_(engine_), capture_(engine_),
        network_(engine_), render_(engine_), rtp_(engine_),
        image_(engine_), ext_codec_(engine_) {
  }

  ViEWrapper(webrtc::ViEBase* base, webrtc::ViECodec* codec,
             webrtc::ViECapture* capture, webrtc::ViENetwork* network,
             webrtc::ViERender* render, webrtc::ViERTP_RTCP* rtp,
             webrtc::ViEImageProcess* image,
             webrtc::ViEExternalCodec* ext_codec)
      : engine_(NULL),
        base_(base),
        codec_(codec),
        capture_(capture),
        network_(network),
        render_(render),
        rtp_(rtp),
        image_(image),
        ext_codec_(ext_codec) {
  }

  virtual ~ViEWrapper() {}
  webrtc::VideoEngine* engine() { return engine_.get(); }
  webrtc::ViEBase* base() { return base_.get(); }
  webrtc::ViECodec* codec() { return codec_.get(); }
  webrtc::ViECapture* capture() { return capture_.get(); }
  webrtc::ViENetwork* network() { return network_.get(); }
  webrtc::ViERender* render() { return render_.get(); }
  webrtc::ViERTP_RTCP* rtp() { return rtp_.get(); }
  webrtc::ViEImageProcess* image() { return image_.get(); }
  webrtc::ViEExternalCodec* ext_codec() { return ext_codec_.get(); }
  int error() { return base_->LastError(); }

 private:
  scoped_vie_engine engine_;
  scoped_vie_ptr<webrtc::ViEBase> base_;
  scoped_vie_ptr<webrtc::ViECodec> codec_;
  scoped_vie_ptr<webrtc::ViECapture> capture_;
  scoped_vie_ptr<webrtc::ViENetwork> network_;
  scoped_vie_ptr<webrtc::ViERender> render_;
  scoped_vie_ptr<webrtc::ViERTP_RTCP> rtp_;
  scoped_vie_ptr<webrtc::ViEImageProcess> image_;
  scoped_vie_ptr<webrtc::ViEExternalCodec> ext_codec_;
};

// Adds indirection to static WebRtc functions, allowing them to be mocked.
class ViETraceWrapper {
 public:
  virtual ~ViETraceWrapper() {}

  virtual int SetTraceFilter(const unsigned int filter) {
    return webrtc::VideoEngine::SetTraceFilter(filter);
  }
  virtual int SetTraceFile(const char* fileNameUTF8) {
    return webrtc::VideoEngine::SetTraceFile(fileNameUTF8);
  }
  virtual int SetTraceCallback(webrtc::TraceCallback* callback) {
    return webrtc::VideoEngine::SetTraceCallback(callback);
  }
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTCVIE_H_
