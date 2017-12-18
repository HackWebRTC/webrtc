/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/voe_base_impl.h"

#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_device/audio_device_impl.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "voice_engine/channel.h"
#include "voice_engine/include/voe_errors.h"
#include "voice_engine/voice_engine_impl.h"

namespace webrtc {

VoEBase* VoEBase::GetInterface(VoiceEngine* voiceEngine) {
  if (nullptr == voiceEngine) {
    return nullptr;
  }
  VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
  s->AddRef();
  return s;
}

VoEBaseImpl::VoEBaseImpl(voe::SharedData* shared)
    : shared_(shared) {}

VoEBaseImpl::~VoEBaseImpl() {
  TerminateInternal();
}

int VoEBaseImpl::Init(
    AudioDeviceModule* audio_device,
    AudioProcessing* audio_processing,
    const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory) {
  RTC_DCHECK(audio_device);
  rtc::CritScope cs(shared_->crit_sec());
  if (shared_->process_thread()) {
    shared_->process_thread()->Start();
  }

  shared_->set_audio_device(audio_device);

  RTC_DCHECK(decoder_factory);
  decoder_factory_ = decoder_factory;

  return 0;
}

void VoEBaseImpl::Terminate() {
  rtc::CritScope cs(shared_->crit_sec());
  TerminateInternal();
}

int VoEBaseImpl::CreateChannel() {
  return CreateChannel(ChannelConfig());
}

int VoEBaseImpl::CreateChannel(const ChannelConfig& config) {
  rtc::CritScope cs(shared_->crit_sec());
  ChannelConfig config_copy(config);
  config_copy.acm_config.decoder_factory = decoder_factory_;
  voe::ChannelOwner channel_owner =
      shared_->channel_manager().CreateChannel(config_copy);
  return InitializeChannel(&channel_owner);
}

int VoEBaseImpl::InitializeChannel(voe::ChannelOwner* channel_owner) {
  if (channel_owner->channel()->SetEngineInformation(
          *shared_->process_thread(), *shared_->audio_device(),
          shared_->encoder_queue()) != 0) {
    RTC_LOG(LS_ERROR)
        << "CreateChannel() failed to associate engine and channel."
           " Destroying channel.";
    shared_->channel_manager().DestroyChannel(
        channel_owner->channel()->ChannelId());
    return -1;
  } else if (channel_owner->channel()->Init() != 0) {
    RTC_LOG(LS_ERROR)
        << "CreateChannel() failed to initialize channel. Destroying"
           " channel.";
    shared_->channel_manager().DestroyChannel(
        channel_owner->channel()->ChannelId());
    return -1;
  }
  return channel_owner->channel()->ChannelId();
}

int VoEBaseImpl::DeleteChannel(int channel) {
  rtc::CritScope cs(shared_->crit_sec());
  {
    voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == nullptr) {
      RTC_LOG(LS_ERROR) << "DeleteChannel() failed to locate channel";
      return -1;
    }
  }

  shared_->channel_manager().DestroyChannel(channel);
  return 0;
}

void VoEBaseImpl::TerminateInternal() {
  // Delete any remaining channel objects
  shared_->channel_manager().DestroyAllChannels();

  if (shared_->process_thread()) {
    shared_->process_thread()->Stop();
  }

  shared_->set_audio_device(nullptr);
}
}  // namespace webrtc
