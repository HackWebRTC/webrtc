/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/internal/video_call.h"

#include <cassert>
#include <cstring>
#include <map>
#include <vector>

#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/internal/video_receive_stream.h"
#include "webrtc/video_engine/internal/video_send_stream.h"
#include "webrtc/video_engine/new_include/common.h"
#include "webrtc/video_engine/new_include/video_engine.h"

namespace webrtc {
namespace internal {

VideoCall::VideoCall(webrtc::VideoEngine* video_engine,
                     newapi::Transport* send_transport)
    : send_transport(send_transport), video_engine_(video_engine) {
  assert(video_engine != NULL);
  assert(send_transport != NULL);

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine_);
  assert(rtp_rtcp_ != NULL);

  codec_ = ViECodec::GetInterface(video_engine_);
  assert(codec_ != NULL);
}

VideoCall::~VideoCall() {
  rtp_rtcp_->Release();
  codec_->Release();
}

newapi::PacketReceiver* VideoCall::Receiver() { return this; }

std::vector<VideoCodec> VideoCall::GetVideoCodecs() {
  std::vector<VideoCodec> codecs;

  VideoCodec codec;
  for (size_t i = 0; i < static_cast<size_t>(codec_->NumberOfCodecs()); ++i) {
    if (codec_->GetCodec(i, codec) == 0) {
      codecs.push_back(codec);
    }
  }
  return codecs;
}

void VideoCall::GetDefaultSendConfig(
    newapi::VideoSendStreamConfig* send_stream_config) {
  *send_stream_config = newapi::VideoSendStreamConfig();
  codec_->GetCodec(0, send_stream_config->codec);
}

newapi::VideoSendStream* VideoCall::CreateSendStream(
    const newapi::VideoSendStreamConfig& send_stream_config) {
  assert(send_stream_config.rtp.ssrcs.size() > 0);
  assert(send_stream_config.codec.numberOfSimulcastStreams == 0 ||
         send_stream_config.codec.numberOfSimulcastStreams ==
             send_stream_config.rtp.ssrcs.size());
  VideoSendStream* send_stream =
      new VideoSendStream(send_transport, video_engine_, send_stream_config);
  for (size_t i = 0; i < send_stream_config.rtp.ssrcs.size(); ++i) {
    uint32_t ssrc = send_stream_config.rtp.ssrcs[i];
    // SSRC must be previously unused!
    assert(send_ssrcs_[ssrc] == NULL &&
           receive_ssrcs_.find(ssrc) == receive_ssrcs_.end());
    send_ssrcs_[ssrc] = send_stream;
  }
  return send_stream;
}

newapi::SendStreamState* VideoCall::DestroySendStream(
    newapi::VideoSendStream* send_stream) {
  if (send_stream == NULL) {
    return NULL;
  }
  // TODO(pbos): Remove it properly! Free the SSRCs!
  delete static_cast<VideoSendStream*>(send_stream);

  // TODO(pbos): Return its previous state
  return NULL;
}

void VideoCall::GetDefaultReceiveConfig(
    newapi::VideoReceiveStreamConfig* receive_stream_config) {
  // TODO(pbos): This is not the default config.
  *receive_stream_config = newapi::VideoReceiveStreamConfig();
}

newapi::VideoReceiveStream* VideoCall::CreateReceiveStream(
    const newapi::VideoReceiveStreamConfig& receive_stream_config) {
  assert(receive_ssrcs_[receive_stream_config.rtp.ssrc] == NULL);

  VideoReceiveStream* receive_stream = new VideoReceiveStream(
      video_engine_, receive_stream_config, send_transport);

  receive_ssrcs_[receive_stream_config.rtp.ssrc] = receive_stream;

  return receive_stream;
}

void VideoCall::DestroyReceiveStream(
    newapi::VideoReceiveStream* receive_stream) {
  if (receive_stream == NULL) {
    return;
  }
  // TODO(pbos): Remove its SSRCs!
  delete static_cast<VideoReceiveStream*>(receive_stream);
}

uint32_t VideoCall::SendBitrateEstimate() {
  // TODO(pbos): Return send-bitrate estimate
  return 0;
}

uint32_t VideoCall::ReceiveBitrateEstimate() {
  // TODO(pbos): Return receive-bitrate estimate
  return 0;
}

bool VideoCall::DeliverRtcp(ModuleRTPUtility::RTPHeaderParser* rtp_parser,
                            const void* packet, size_t length) {
  // TODO(pbos): Figure out what channel needs it actually.
  //             Do NOT broadcast! Also make sure it's a valid packet.
  bool rtcp_delivered = false;
  for (std::map<uint32_t, newapi::VideoReceiveStream*>::iterator it =
           receive_ssrcs_.begin();
       it != receive_ssrcs_.end(); ++it) {
    if (static_cast<VideoReceiveStream*>(it->second)
            ->DeliverRtcp(packet, length)) {
      rtcp_delivered = true;
    }
  }
  return rtcp_delivered;
}

bool VideoCall::DeliverRtp(ModuleRTPUtility::RTPHeaderParser* rtp_parser,
                           const void* packet, size_t length) {
  WebRtcRTPHeader rtp_header;

  // TODO(pbos): ExtensionMap if there are extensions
  if (!rtp_parser->Parse(rtp_header)) {
    // TODO(pbos): Should this error be reported and trigger something?
    return false;
  }

  uint32_t ssrc = rtp_header.header.ssrc;
  if (receive_ssrcs_.find(ssrc) == receive_ssrcs_.end()) {
    // TODO(pbos): Log some warning, SSRC without receiver.
    return false;
  }

  VideoReceiveStream* receiver =
      static_cast<VideoReceiveStream*>(receive_ssrcs_[ssrc]);
  return receiver->DeliverRtp(packet, length);
}

bool VideoCall::DeliverPacket(const void* packet, size_t length) {
  // TODO(pbos): Respect the constness of packet.
  ModuleRTPUtility::RTPHeaderParser rtp_parser(
      const_cast<uint8_t*>(static_cast<const uint8_t*>(packet)), length);

  if (rtp_parser.RTCP()) {
    return DeliverRtcp(&rtp_parser, packet, length);
  }

  return DeliverRtp(&rtp_parser, packet, length);
}

}  // namespace internal
}  // namespace webrtc
