/*
 * libjingle
 * Copyright 2012, Google Inc.
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
#ifndef TALK_APP_WEBRTC_TEST_FAKECONSTRAINTS_H_
#define TALK_APP_WEBRTC_TEST_FAKECONSTRAINTS_H_

#include <string>
#include <vector>

#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/base/stringencode.h"

namespace webrtc {

class FakeConstraints : public webrtc::MediaConstraintsInterface {
 public:
  FakeConstraints() { }
  virtual ~FakeConstraints() { }

  virtual const Constraints& GetMandatory() const {
    return mandatory_;
  }

  virtual const Constraints& GetOptional() const {
    return optional_;
  }

  template <class T>
  void AddMandatory(const std::string& key, const T& value) {
    mandatory_.push_back(Constraint(key, talk_base::ToString<T>(value)));
  }

  template <class T>
  void SetMandatory(const std::string& key, const T& value) {
    std::string value_str;
    if (mandatory_.FindFirst(key, &value_str)) {
      for (Constraints::iterator iter = mandatory_.begin();
           iter != mandatory_.end(); ++iter) {
        if (iter->key == key) {
          mandatory_.erase(iter);
          break;
        }
      }
    }
    mandatory_.push_back(Constraint(key, talk_base::ToString<T>(value)));
  }

  template <class T>
  void AddOptional(const std::string& key, const T& value) {
    optional_.push_back(Constraint(key, talk_base::ToString<T>(value)));
  }

  void SetMandatoryMinAspectRatio(double ratio) {
    SetMandatory(MediaConstraintsInterface::kMinAspectRatio, ratio);
  }

  void SetMandatoryMinWidth(int width) {
    SetMandatory(MediaConstraintsInterface::kMinWidth, width);
  }

  void SetMandatoryMinHeight(int height) {
    SetMandatory(MediaConstraintsInterface::kMinHeight, height);
  }

  void SetOptionalMaxWidth(int width) {
    AddOptional(MediaConstraintsInterface::kMaxWidth, width);
  }

  void SetMandatoryMaxFrameRate(int frame_rate) {
    SetMandatory(MediaConstraintsInterface::kMaxFrameRate, frame_rate);
  }

  void SetMandatoryReceiveAudio(bool enable) {
    SetMandatory(MediaConstraintsInterface::kOfferToReceiveAudio, enable);
  }

  void SetMandatoryReceiveVideo(bool enable) {
    SetMandatory(MediaConstraintsInterface::kOfferToReceiveVideo, enable);
  }

  void SetMandatoryUseRtpMux(bool enable) {
    SetMandatory(MediaConstraintsInterface::kUseRtpMux, enable);
  }

  void SetMandatoryIceRestart(bool enable) {
    SetMandatory(MediaConstraintsInterface::kIceRestart, enable);
  }

  void SetAllowRtpDataChannels() {
    SetMandatory(MediaConstraintsInterface::kEnableRtpDataChannels, true);
  }

  void SetOptionalVAD(bool enable) {
    AddOptional(MediaConstraintsInterface::kVoiceActivityDetection, enable);
  }

  void SetAllowDtlsSctpDataChannels() {
    SetMandatory(MediaConstraintsInterface::kEnableDtlsSrtp, true);
  }

 private:
  Constraints mandatory_;
  Constraints optional_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_TEST_FAKECONSTRAINTS_H_
