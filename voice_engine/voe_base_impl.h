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

#include "modules/include/module_common_types.h"
#include "rtc_base/criticalsection.h"
#include "voice_engine/shared_data.h"

namespace webrtc {

class ProcessThread;

class VoEBaseImpl : public VoEBase {
 public:
  int Init(
      AudioDeviceModule* audio_device,
      AudioProcessing* audio_processing,
      const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory) override;
  void Terminate() override;

  int CreateChannel() override;
  int CreateChannel(const ChannelConfig& config) override;
  int DeleteChannel(int channel) override;

  int StartPlayout(int channel) override;
  int StopPlayout(int channel) override;

  int SetPlayout(bool enabled) override;

 protected:
  VoEBaseImpl(voe::SharedData* shared);
  ~VoEBaseImpl() override;

 private:
  int32_t StartPlayout();
  int32_t StopPlayout();
  void TerminateInternal();

  // Initialize channel by setting Engine Information then initializing
  // channel.
  int InitializeChannel(voe::ChannelOwner* channel_owner);
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;

  AudioFrame audioFrame_;
  voe::SharedData* shared_;
  bool playout_enabled_ = true;
};

}  // namespace webrtc

#endif  // VOICE_ENGINE_VOE_BASE_IMPL_H_
