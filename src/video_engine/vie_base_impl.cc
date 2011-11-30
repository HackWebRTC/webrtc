/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "critical_section_wrapper.h"
#include "engine_configurations.h"
#include "rtp_rtcp.h"
#include "stdio.h"
#include "trace.h"
#include "video_coding.h"
#include "video_processing.h"
#include "video_render.h"
#include "vie_base_impl.h"
#include "vie_channel.h"
#include "vie_channel_manager.h"
#include "vie_defines.h"
#include "vie_encoder.h"
#include "vie_errors.h"
#include "vie_impl.h"
#include "vie_input_manager.h"
#include "vie_performance_monitor.h"
#include "vie_shared_data.h"

namespace webrtc {

ViEBase* ViEBase::GetInterface(VideoEngine* video_engine) {
  if (!video_engine) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);
  ViEBaseImpl* vie_base_impl = vie_impl;
  (*vie_base_impl)++;  // Increase ref count.

  return vie_base_impl;
}

int ViEBaseImpl::Release() {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, instance_id_,
               "ViEBase::Release()");
  (*this)--;  // Decrease ref count.

  WebRtc_Word32 ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, instance_id_,
                 "ViEBase release too many times");
    SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, instance_id_,
               "ViEBase reference count: %d", ref_count);
  return ref_count;
}

ViEBaseImpl::ViEBaseImpl() {
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, instance_id_,
               "ViEBaseImpl::ViEBaseImpl() Ctor");
}

ViEBaseImpl::~ViEBaseImpl() {
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, instance_id_,
               "ViEBaseImpl::ViEBaseImpl() Dtor");
}

int ViEBaseImpl::Init() {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, instance_id_,
               "Init");
  if (Initialized()) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, instance_id_,
                 "Init called twice");
    return 0;
  }

  SetInitialized();
  return 0;
}

int ViEBaseImpl::SetVoiceEngine(VoiceEngine* voice_engine) {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s", __FUNCTION__);
  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }

  if (channel_manager_.SetVoiceEngine(voice_engine) != 0) {
    SetLastError(kViEBaseVoEFailure);
    return -1;
  }
  return 0;
}

int ViEBaseImpl::CreateChannel(int& video_channel) {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s", __FUNCTION__);

  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }

  if (channel_manager_.CreateChannel(video_channel) == -1) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s: Could not create channel", __FUNCTION__);
    video_channel = -1;
    SetLastError(kViEBaseChannelCreationFailed);
    return -1;
  }
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s: channel created: %d", __FUNCTION__, video_channel);
  return 0;
}

int ViEBaseImpl::CreateChannel(int& video_channel, int original_channel) {
  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }

  ViEChannelManagerScoped cs(channel_manager_);
  if (!cs.Channel(original_channel)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s - original_channel does not exist.", __FUNCTION__,
                 instance_id_);
    SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }

  if (channel_manager_.CreateChannel(video_channel,
                                     original_channel) == -1) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s: Could not create channel", __FUNCTION__);
    video_channel = -1;
    SetLastError(kViEBaseChannelCreationFailed);
    return -1;
  }
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s: channel created: %d", __FUNCTION__, video_channel);
  return 0;
}

int ViEBaseImpl::DeleteChannel(const int video_channel) {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s(%d)", __FUNCTION__, video_channel);

  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }

  {
    ViEChannelManagerScoped cs(channel_manager_);
    ViEChannel* vie_channel = cs.Channel(video_channel);
    if (!vie_channel) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(instance_id_), "%s: channel %d doesn't exist",
                   __FUNCTION__, video_channel);
      SetLastError(kViEBaseInvalidChannelId);
      return -1;
    }

    // Deregister the ViEEncoder if no other channel is using it.
    ViEEncoder* vie_encoder = cs.Encoder(video_channel);
    if (cs.ChannelUsingViEEncoder(video_channel) == false) {
      ViEInputManagerScoped is(input_manager_);
      ViEFrameProviderBase* provider = is.FrameProvider(vie_encoder);
      if (provider) {
        provider->DeregisterFrameCallback(vie_encoder);
      }
    }
  }

  if (channel_manager_.DeleteChannel(video_channel) == -1) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s: Could not delete channel %d", __FUNCTION__,
                 video_channel);
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s: channel deleted: %d", __FUNCTION__, video_channel);
  return 0;
}

int ViEBaseImpl::ConnectAudioChannel(const int video_channel,
                                     const int audio_channel) {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s(%d)", __FUNCTION__, video_channel);

  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }

  ViEChannelManagerScoped cs(channel_manager_);
  if (!cs.Channel(video_channel)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s: channel %d doesn't exist", __FUNCTION__, video_channel);
    SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }

  if (channel_manager_.ConnectVoiceChannel(video_channel, audio_channel) != 0) {
    SetLastError(kViEBaseVoEFailure);
    return -1;
  }
  return 0;
}

int ViEBaseImpl::DisconnectAudioChannel(const int video_channel) {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s(%d)", __FUNCTION__, video_channel);
  if (!Initialized()) {
    SetLastError(kViENotInitialized);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s - ViE instance %d not initialized", __FUNCTION__,
                 instance_id_);
    return -1;
  }
  ViEChannelManagerScoped cs(channel_manager_);
  if (!cs.Channel(video_channel)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(instance_id_),
                 "%s: channel %d doesn't exist", __FUNCTION__, video_channel);
    SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }

  if (channel_manager_.DisconnectVoiceChannel(video_channel) != 0) {
    SetLastError(kViEBaseVoEFailure);
    return -1;
  }
  return 0;
}

int ViEBaseImpl::StartSend(const int video_channel) {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(instance_id_, video_channel), "%s(channel: %d)",
               __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(instance_id_, video_channel),
                 "%s: Channel %d does not exist", __FUNCTION__, video_channel);
    SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    assert(false);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(instance_id_, video_channel),
                 "%s: Could not find encoder for channel %d", __FUNCTION__,
                 video_channel);
    return -1;
  }

  // Pause and trigger a key frame.
  vie_encoder->Pause();
  WebRtc_Word32 error = vie_channel->StartSend();
  if (error != 0) {
    vie_encoder->Restart();
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(instance_id_, video_channel),
                 "%s: Could not start sending on channel %d", __FUNCTION__,
                 video_channel);
    if (error == kViEBaseAlreadySending) {
      SetLastError(kViEBaseAlreadySending);
    }
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  vie_encoder->SendKeyFrame();
  vie_encoder->Restart();
  return 0;
}

int ViEBaseImpl::StopSend(const int video_channel) {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(instance_id_, video_channel), "%s(channel: %d)",
               __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(instance_id_, video_channel),
                 "%s: Channel %d does not exist", __FUNCTION__, video_channel);
    SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }

  WebRtc_Word32 error = vie_channel->StopSend();
  if (error != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(instance_id_, video_channel),
                 "%s: Could not stop sending on channel %d", __FUNCTION__,
                 video_channel);
    if (error == kViEBaseNotSending) {
      SetLastError(kViEBaseNotSending);
    } else {
      SetLastError(kViEBaseUnknownError);
    }
    return -1;
  }
  return 0;
}

int ViEBaseImpl::StartReceive(const int video_channel) {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(instance_id_, video_channel), "%s(channel: %d)",
               __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(instance_id_, video_channel),
                 "%s: Channel %d does not exist", __FUNCTION__, video_channel);
    SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }
  if (vie_channel->Receiving()) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(instance_id_, video_channel),
                 "%s: Channel %d already receive.", __FUNCTION__,
                 video_channel);
    SetLastError(kViEBaseAlreadyReceiving);
    return -1;
  }
  if (vie_channel->StartReceive() != 0) {
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  return 0;
}

int ViEBaseImpl::StopReceive(const int video_channel) {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(instance_id_, video_channel), "%s(channel: %d)",
               __FUNCTION__, video_channel);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                 ViEId(instance_id_, video_channel),
                 "%s: Channel %d does not exist", __FUNCTION__, video_channel);
    SetLastError(kViEBaseInvalidChannelId);
    return -1;
  }
  if (vie_channel->StopReceive() != 0) {
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  return 0;
}

int ViEBaseImpl::RegisterObserver(ViEBaseObserver& observer) {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s", __FUNCTION__);
  if (vie_performance_monitor_.ViEBaseObserverRegistered()) {
    SetLastError(kViEBaseObserverAlreadyRegistered);
    return -1;
  }
  return vie_performance_monitor_.Init(&observer);
}

int ViEBaseImpl::DeregisterObserver() {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
               "%s", __FUNCTION__);

  if (!vie_performance_monitor_.ViEBaseObserverRegistered()) {
    SetLastError(kViEBaseObserverNotRegistered);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, instance_id_,
                 "%s No observer registered.", __FUNCTION__);
    return -1;
  }
  vie_performance_monitor_.Terminate();
  return 0;
}

int ViEBaseImpl::GetVersion(char version[1024]) {
  WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, ViEId(instance_id_),
               "GetVersion(version=?)");
  assert(kViEVersionMaxMessageSize == 1024);

  if (!version) {
    SetLastError(kViEBaseInvalidArgument);
    return -1;
  }

  char version_buf[kViEVersionMaxMessageSize];
  char* version_ptr = version_buf;

  WebRtc_Word32 len = 0;  // Does not include NULL termination.
  WebRtc_Word32 acc_len = 0;

  len = AddViEVersion(version_ptr);
  if (len == -1) {
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  version_ptr += len;
  acc_len += len;
  assert(acc_len < kViEVersionMaxMessageSize);

  len = AddBuildInfo(version_ptr);
  if (len == -1) {
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  version_ptr += len;
  acc_len += len;
  assert(acc_len < kViEVersionMaxMessageSize);

#ifdef WEBRTC_EXTERNAL_TRANSPORT
  len = AddExternalTransportBuild(version_ptr);
  if (len == -1) {
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  version_ptr += len;
  acc_len += len;
  assert(acc_len < kViEVersionMaxMessageSize);
#endif

  len = AddVCMVersion(version_ptr);
  if (len == -1) {
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  version_ptr += len;
  acc_len += len;
  assert(acc_len < kViEVersionMaxMessageSize);

#ifndef WEBRTC_EXTERNAL_TRANSPORT
  len = AddSocketModuleVersion(version_ptr);
  if (len == -1) {
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  version_ptr += len;
  acc_len += len;
  assert(acc_len < kViEVersionMaxMessageSize);
#endif

  len = AddRtpRtcpModuleVersion(version_ptr);
  if (len == -1) {
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  version_ptr += len;
  acc_len += len;
  assert(acc_len < kViEVersionMaxMessageSize);

  len = AddVideoCaptureVersion(version_ptr);
  if (len == -1) {
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  version_ptr += len;
  acc_len += len;
  assert(acc_len < kViEVersionMaxMessageSize);

  len = AddRenderVersion(version_ptr);
  if (len == -1) {
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  version_ptr += len;
  acc_len += len;
  assert(acc_len < kViEVersionMaxMessageSize);

  len = AddVideoProcessingVersion(version_ptr);
  if (len == -1) {
    SetLastError(kViEBaseUnknownError);
    return -1;
  }
  version_ptr += len;
  acc_len += len;
  assert(acc_len < kViEVersionMaxMessageSize);

  memcpy(version, version_buf, acc_len);
  version[acc_len] = '\0';

  WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
               ViEId(instance_id_), "GetVersion() => %s", version);
  return 0;
}

int ViEBaseImpl::LastError() {
  return LastErrorInternal();
}

WebRtc_Word32 ViEBaseImpl::AddBuildInfo(char* str) const {
  return sprintf(str, "Build: %s\n", BUILDINFO);
}

WebRtc_Word32 ViEBaseImpl::AddViEVersion(char* str) const {
  return sprintf(str, "VideoEngine 3.1.0\n");
}

#ifdef WEBRTC_EXTERNAL_TRANSPORT
WebRtc_Word32 ViEBaseImpl::AddExternalTransportBuild(char* str) const {
  return sprintf(str, "External transport build\n");
}
#endif

WebRtc_Word32 ViEBaseImpl::AddModuleVersion(webrtc::Module* module,
                                            char* str) const {
  WebRtc_Word8 version[kViEMaxModuleVersionSize];
  WebRtc_UWord32 remaining_buffer_in_bytes(kViEMaxModuleVersionSize);
  WebRtc_UWord32 position(0);
  if (module && module->Version(version, remaining_buffer_in_bytes, position)
      == 0) {
    return sprintf(str, "%s\n", version);
  }
  return -1;
}

WebRtc_Word32 ViEBaseImpl::AddVCMVersion(char* str) const {
  webrtc::VideoCodingModule* vcm_ptr =
    webrtc::VideoCodingModule::Create(instance_id_);
  int len = AddModuleVersion(vcm_ptr, str);
  webrtc::VideoCodingModule::Destroy(vcm_ptr);
  return len;
}

WebRtc_Word32 ViEBaseImpl::AddVideoCaptureVersion(char* str) const {
  return 0;
}

WebRtc_Word32 ViEBaseImpl::AddVideoProcessingVersion(char* str) const {
  webrtc::VideoProcessingModule* video_ptr =
    webrtc::VideoProcessingModule::Create(instance_id_);
  int len = AddModuleVersion(video_ptr, str);
  webrtc::VideoProcessingModule::Destroy(video_ptr);
  return len;
}
WebRtc_Word32 ViEBaseImpl::AddRenderVersion(char* str) const {
  return 0;
}

#ifndef WEBRTC_EXTERNAL_TRANSPORT
WebRtc_Word32 ViEBaseImpl::AddSocketModuleVersion(char* str) const {
  WebRtc_UWord8 num_sock_threads(1);
  UdpTransport* transport = UdpTransport::Create(instance_id_,
                                                 num_sock_threads);
  int len = AddModuleVersion(transport, str);
  UdpTransport::Destroy(transport);
  return len;
}
#endif

WebRtc_Word32 ViEBaseImpl::AddRtpRtcpModuleVersion(char* str) const {
  RtpRtcp* rtp_rtcp = RtpRtcp::CreateRtpRtcp(-1, true);
  int len = AddModuleVersion(rtp_rtcp, str);
  RtpRtcp::DestroyRtpRtcp(rtp_rtcp);
  return len;
}

}  // namespace webrtc
