/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VOICE_ENGINE_VOE_BASE_IMPL_H_
#define VOICE_ENGINE_VOE_BASE_IMPL_H_

#include "voice_engine/include/voe_base.h"

namespace webrtc {

class VoEBaseImpl : public VoEBase {
 public:
  int Init(
      AudioDeviceModule* audio_device,
      AudioProcessing* audio_processing,
      const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory) override {
    return 0;
  }
  void Terminate() override {}

  int CreateChannel(const ChannelConfig& config) override { return 1; }
  int DeleteChannel(int channel) override { return 0; }

 protected:
  VoEBaseImpl();
  ~VoEBaseImpl() override;
};

}  // namespace webrtc

#endif  // VOICE_ENGINE_VOE_BASE_IMPL_H_
