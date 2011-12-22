/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_external_codec_impl.h"

#include "engine_configurations.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/include/vie_errors.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_channel_manager.h"
#include "video_engine/vie_encoder.h"
#include "video_engine/vie_impl.h"

namespace webrtc {

ViEExternalCodec* ViEExternalCodec::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
  if (video_engine == NULL) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);
  ViEExternalCodecImpl* vie_external_codec_impl = vie_impl;
  // Increase ref count.
  (*vie_external_codec_impl)++;
  return vie_external_codec_impl;
#else
  return NULL;
#endif
}

int ViEExternalCodecImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, instance_id_,
               "ViEExternalCodec::Release()");
  // Decrease ref count.
  (*this)--;

  WebRtc_Word32 ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, instance_id_,
                 "ViEExternalCodec release too many times");
    SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, instance_id_,
               "ViEExternalCodec reference count: %d", ref_count);
  return ref_count;
}

int ViEExternalCodecImpl::RegisterExternalSendCodec(const int video_channel,
                                                    const unsigned char pl_type,
                                                    VideoEncoder* encoder) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s channel %d pl_type %d encoder 0x%x", __FUNCTION__,
               video_channel, pl_type, encoder);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Invalid argument video_channel %u. Does it exist?",
                 __FUNCTION__, video_channel);
    SetLastError(kViECodecInvalidArgument);
    return -1;
  }
  if (!encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Invalid argument Encoder 0x%x.", __FUNCTION__, encoder);
    SetLastError(kViECodecInvalidArgument);
    return -1;
  }

  if (vie_encoder->RegisterExternalEncoder(encoder, pl_type) != 0) {
    SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViEExternalCodecImpl::DeRegisterExternalSendCodec(
  const int video_channel, const unsigned char pl_type) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s channel %d pl_type %d", __FUNCTION__, video_channel,
               pl_type);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEEncoder* vie_encoder = cs.Encoder(video_channel);
  if (!vie_encoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Invalid argument video_channel %u. Does it exist?",
                 __FUNCTION__, video_channel);
    SetLastError(kViECodecInvalidArgument);
    return -1;
  }

  if (vie_encoder->DeRegisterExternalEncoder(pl_type) != 0) {
    SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViEExternalCodecImpl::RegisterExternalReceiveCodec(
    const int video_channel,
    const unsigned int pl_type,
    VideoDecoder* decoder,
    bool decoder_render,
    int render_delay) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s channel %d pl_type %d decoder 0x%x, decoder_render %d, "
               "renderDelay %d", __FUNCTION__, video_channel, pl_type, decoder,
               decoder_render, render_delay);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Invalid argument video_channel %u. Does it exist?",
                 __FUNCTION__, video_channel);
    SetLastError(kViECodecInvalidArgument);
    return -1;
  }
  if (!decoder) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Invalid argument decoder 0x%x.", __FUNCTION__, decoder);
    SetLastError(kViECodecInvalidArgument);
    return -1;
  }

  if (vie_channel->RegisterExternalDecoder(pl_type, decoder, decoder_render,
                                           render_delay) != 0) {
    SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

int ViEExternalCodecImpl::DeRegisterExternalReceiveCodec(
const int video_channel, const unsigned char pl_type) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_),
               "%s channel %d pl_type %u", __FUNCTION__, video_channel,
               pl_type);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (!vie_channel) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: Invalid argument video_channel %u. Does it exist?",
                 __FUNCTION__, video_channel);
    SetLastError(kViECodecInvalidArgument);
    return -1;
  }
  if (vie_channel->DeRegisterExternalDecoder(pl_type) != 0) {
    SetLastError(kViECodecUnknownError);
    return -1;
  }
  return 0;
}

}  // namespace webrtc
