// libjingle
// Copyright 2004 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef TALK_SESSION_PHONE_FAKEWEBRTCVIDEOCAPTUREMODULE_H_
#define TALK_SESSION_PHONE_FAKEWEBRTCVIDEOCAPTUREMODULE_H_

#include <vector>

#include "talk/media/base/testutils.h"
#include "talk/media/webrtc/fakewebrtcdeviceinfo.h"
#include "talk/media/webrtc/webrtcvideocapturer.h"

class FakeWebRtcVcmFactory;

// Fake class for mocking out webrtc::VideoCaptureModule.
class FakeWebRtcVideoCaptureModule : public webrtc::VideoCaptureModule {
 public:
  FakeWebRtcVideoCaptureModule(FakeWebRtcVcmFactory* factory, int32_t id)
      : factory_(factory),
        id_(id),
        callback_(NULL),
        running_(false),
        delay_(0) {
  }
  virtual int32_t TimeUntilNextProcess() OVERRIDE {
    return 0;
  }
  virtual int32_t Process() OVERRIDE {
    return 0;
  }
  virtual int32_t ChangeUniqueId(const int32_t id) OVERRIDE {
    id_ = id;
    return 0;
  }
  virtual void RegisterCaptureDataCallback(
      webrtc::VideoCaptureDataCallback& callback) OVERRIDE {
    callback_ = &callback;
  }
  virtual void DeRegisterCaptureDataCallback() OVERRIDE { callback_ = NULL; }
  virtual void RegisterCaptureCallback(
      webrtc::VideoCaptureFeedBack& callback) OVERRIDE {
    // Not implemented.
  }
  virtual void DeRegisterCaptureCallback() OVERRIDE {
    // Not implemented.
  }
  virtual void SetCaptureDelay(int32_t delay) OVERRIDE { delay_ = delay; }
  virtual int32_t CaptureDelay() OVERRIDE { return delay_; }
  virtual void EnableFrameRateCallback(const bool enable) OVERRIDE {
    // not implemented
  }
  virtual void EnableNoPictureAlarm(const bool enable) OVERRIDE {
    // not implemented
  }
  virtual int32_t StartCapture(
      const webrtc::VideoCaptureCapability& cap) OVERRIDE {
    if (running_) return -1;
    cap_ = cap;
    running_ = true;
    return 0;
  }
  virtual int32_t StopCapture() OVERRIDE {
    running_ = false;
    return 0;
  }
  virtual const char* CurrentDeviceName() const OVERRIDE {
    return NULL;  // not implemented
  }
  virtual bool CaptureStarted() OVERRIDE {
    return running_;
  }
  virtual int32_t CaptureSettings(
      webrtc::VideoCaptureCapability& settings) OVERRIDE {
    if (!running_) return -1;
    settings = cap_;
    return 0;
  }

  virtual int32_t SetCaptureRotation(
      webrtc::VideoCaptureRotation rotation) OVERRIDE {
    return -1;  // not implemented
  }
  virtual VideoCaptureEncodeInterface* GetEncodeInterface(
      const webrtc::VideoCodec& codec) OVERRIDE {
    return NULL;  // not implemented
  }
  virtual int32_t AddRef() OVERRIDE {
    return 0;
  }
  virtual int32_t Release() OVERRIDE {
    delete this;
    return 0;
  }

  bool SendFrame(int w, int h) {
    if (!running_) return false;
    webrtc::I420VideoFrame sample;
    // Setting stride based on width.
    if (sample.CreateEmptyFrame(w, h, w, (w + 1) / 2, (w + 1) / 2) < 0) {
      return false;
    }
    if (callback_) {
      callback_->OnIncomingCapturedFrame(id_, sample);
    }
    return true;
  }

  const webrtc::VideoCaptureCapability& cap() const {
    return cap_;
  }

 private:
  // Ref-counted, use Release() instead.
  ~FakeWebRtcVideoCaptureModule();

  FakeWebRtcVcmFactory* factory_;
  int id_;
  webrtc::VideoCaptureDataCallback* callback_;
  bool running_;
  webrtc::VideoCaptureCapability cap_;
  int delay_;
};

#endif  // TALK_SESSION_PHONE_FAKEWEBRTCVIDEOCAPTUREMODULE_H_
