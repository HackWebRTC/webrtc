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
  if (StopSend() != 0) {
    return -1;
  }
  if (StopPlayout() != 0) {
    return -1;
  }
  return 0;
}

int VoEBaseImpl::StartPlayout(int channel) {
  rtc::CritScope cs(shared_->crit_sec());
  voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == nullptr) {
    RTC_LOG(LS_ERROR) << "StartPlayout() failed to locate channel";
    return -1;
  }
  if (channelPtr->Playing()) {
    return 0;
  }
  if (StartPlayout() != 0) {
    RTC_LOG(LS_ERROR) << "StartPlayout() failed to start playout";
    return -1;
  }
  return channelPtr->StartPlayout();
}

int VoEBaseImpl::StopPlayout(int channel) {
  rtc::CritScope cs(shared_->crit_sec());
  voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == nullptr) {
    RTC_LOG(LS_ERROR) << "StopPlayout() failed to locate channel";
    return -1;
  }
  if (channelPtr->StopPlayout() != 0) {
    RTC_LOG_F(LS_WARNING) << "StopPlayout() failed to stop playout for channel "
                          << channel;
  }
  return StopPlayout();
}

int VoEBaseImpl::StartSend(int channel) {
  rtc::CritScope cs(shared_->crit_sec());
  voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == nullptr) {
    RTC_LOG(LS_ERROR) << "StartSend() failed to locate channel";
    return -1;
  }
  if (channelPtr->Sending()) {
    return 0;
  }
  if (StartSend() != 0) {
    RTC_LOG(LS_ERROR) << "StartSend() failed to start recording";
    return -1;
  }
  return channelPtr->StartSend();
}

int VoEBaseImpl::StopSend(int channel) {
  rtc::CritScope cs(shared_->crit_sec());
  voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == nullptr) {
    RTC_LOG(LS_ERROR) << "StopSend() failed to locate channel";
    return -1;
  }
  channelPtr->StopSend();
  return StopSend();
}

int32_t VoEBaseImpl::StartPlayout() {
  if (!shared_->audio_device()->Playing()) {
    if (shared_->audio_device()->InitPlayout() != 0) {
      RTC_LOG_F(LS_ERROR) << "Failed to initialize playout";
      return -1;
    }
    if (playout_enabled_ && shared_->audio_device()->StartPlayout() != 0) {
      RTC_LOG_F(LS_ERROR) << "Failed to start playout";
      return -1;
    }
  }
  return 0;
}

int32_t VoEBaseImpl::StopPlayout() {
  if (!playout_enabled_) {
    return 0;
  }
  // Stop audio-device playing if no channel is playing out.
  if (shared_->NumOfPlayingChannels() == 0) {
    if (shared_->audio_device()->StopPlayout() != 0) {
      RTC_LOG(LS_ERROR) << "StopPlayout() failed to stop playout";
      return -1;
    }
  }
  return 0;
}

int32_t VoEBaseImpl::StartSend() {
  if (!shared_->audio_device()->Recording()) {
    if (shared_->audio_device()->InitRecording() != 0) {
      RTC_LOG_F(LS_ERROR) << "Failed to initialize recording";
      return -1;
    }
    if (recording_enabled_ && shared_->audio_device()->StartRecording() != 0) {
      RTC_LOG_F(LS_ERROR) << "Failed to start recording";
      return -1;
    }
  }
  return 0;
}

int32_t VoEBaseImpl::StopSend() {
  if (!recording_enabled_) {
    return 0;
  }
  // Stop audio-device recording if no channel is recording.
  if (shared_->NumOfSendingChannels() == 0) {
    if (shared_->audio_device()->StopRecording() != 0) {
      RTC_LOG(LS_ERROR) << "StopSend() failed to stop recording";
      return -1;
    }
  }

  return 0;
}

int32_t VoEBaseImpl::SetPlayout(bool enabled) {
  RTC_LOG(INFO) << "SetPlayout(" << enabled << ")";
  if (playout_enabled_ == enabled) {
    return 0;
  }
  playout_enabled_ = enabled;
  if (shared_->NumOfPlayingChannels() == 0) {
    // If there are no channels attempting to play out yet, there's nothing to
    // be done; we should be in a "not playing out" state either way.
    return 0;
  }
  int32_t ret;
  if (enabled) {
    ret = shared_->audio_device()->StartPlayout();
    if (ret != 0) {
      RTC_LOG(LS_ERROR) << "SetPlayout(true) failed to start playout";
    }
  } else {
    ret = shared_->audio_device()->StopPlayout();
    if (ret != 0) {
      RTC_LOG(LS_ERROR) << "SetPlayout(false) failed to stop playout";
    }
  }
  return ret;
}

int32_t VoEBaseImpl::SetRecording(bool enabled) {
  RTC_LOG(INFO) << "SetRecording(" << enabled << ")";
  if (recording_enabled_ == enabled) {
    return 0;
  }
  recording_enabled_ = enabled;
  if (shared_->NumOfSendingChannels() == 0) {
    // If there are no channels attempting to record out yet, there's nothing to
    // be done; we should be in a "not recording" state either way.
    return 0;
  }
  int32_t ret;
  if (enabled) {
    ret = shared_->audio_device()->StartRecording();
    if (ret != 0) {
      RTC_LOG(LS_ERROR) << "SetRecording(true) failed to start recording";
    }
  } else {
    ret = shared_->audio_device()->StopRecording();
    if (ret != 0) {
      RTC_LOG(LS_ERROR) << "SetRecording(false) failed to stop recording";
    }
  }
  return ret;
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
