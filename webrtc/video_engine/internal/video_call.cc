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

#include <assert.h>
#include <string.h>

#include <map>
#include <vector>

#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/internal/video_receive_stream.h"
#include "webrtc/video_engine/internal/video_send_stream.h"

namespace webrtc {

namespace newapi {
VideoCall* VideoCall::Create(const newapi::VideoCall::Config& config) {
  webrtc::VideoEngine* video_engine = webrtc::VideoEngine::Create();
  assert(video_engine != NULL);

  ViEBase* video_engine_base = ViEBase::GetInterface(video_engine);
  assert(video_engine_base != NULL);
  if (video_engine_base->Init() != 0) {
    abort();
  }
  video_engine_base->Release();

  return new internal::VideoCall(video_engine, config);
}
}  // namespace newapi

namespace internal {

VideoCall::VideoCall(webrtc::VideoEngine* video_engine,
                     const newapi::VideoCall::Config& config)
    : config_(config),
      receive_lock_(RWLockWrapper::CreateRWLock()),
      send_lock_(RWLockWrapper::CreateRWLock()),
      rtp_header_parser_(RtpHeaderParser::Create()),
      video_engine_(video_engine) {
  assert(video_engine != NULL);
  assert(config.send_transport != NULL);

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine_);
  assert(rtp_rtcp_ != NULL);

  codec_ = ViECodec::GetInterface(video_engine_);
  assert(codec_ != NULL);
}

VideoCall::~VideoCall() {
  codec_->Release();
  rtp_rtcp_->Release();
  webrtc::VideoEngine::Delete(video_engine_);
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

VideoSendStream::Config VideoCall::GetDefaultSendConfig() {
  VideoSendStream::Config config;
  codec_->GetCodec(0, config.codec);
  return config;
}

newapi::VideoSendStream* VideoCall::CreateSendStream(
    const newapi::VideoSendStream::Config& config) {
  assert(config.rtp.ssrcs.size() > 0);
  assert(config.codec.numberOfSimulcastStreams == 0 ||
         config.codec.numberOfSimulcastStreams == config.rtp.ssrcs.size());

  VideoSendStream* send_stream = new VideoSendStream(
      config_.send_transport, config_.overuse_detection, video_engine_, config);

  WriteLockScoped write_lock(*send_lock_);
  for (size_t i = 0; i < config.rtp.ssrcs.size(); ++i) {
    assert(send_ssrcs_.find(config.rtp.ssrcs[i]) == send_ssrcs_.end());
    send_ssrcs_[config.rtp.ssrcs[i]] = send_stream;
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

VideoReceiveStream::Config VideoCall::GetDefaultReceiveConfig() {
  return newapi::VideoReceiveStream::Config();
}

newapi::VideoReceiveStream* VideoCall::CreateReceiveStream(
    const newapi::VideoReceiveStream::Config& config) {
  VideoReceiveStream* receive_stream =
      new VideoReceiveStream(video_engine_, config, config_.send_transport);

  WriteLockScoped write_lock(*receive_lock_);
  assert(receive_ssrcs_.find(config.rtp.ssrc) == receive_ssrcs_.end());
  receive_ssrcs_[config.rtp.ssrc] = receive_stream;
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

bool VideoCall::DeliverRtcp(const uint8_t* packet, size_t length) {
  // TODO(pbos): Figure out what channel needs it actually.
  //             Do NOT broadcast! Also make sure it's a valid packet.
  bool rtcp_delivered = false;
  {
    ReadLockScoped read_lock(*receive_lock_);
    for (std::map<uint32_t, VideoReceiveStream*>::iterator it =
             receive_ssrcs_.begin();
         it != receive_ssrcs_.end();
         ++it) {
      if (it->second->DeliverRtcp(static_cast<const uint8_t*>(packet),
                                  length)) {
        rtcp_delivered = true;
      }
    }
  }

  {
    ReadLockScoped read_lock(*send_lock_);
    for (std::map<uint32_t, VideoSendStream*>::iterator it =
             send_ssrcs_.begin();
         it != send_ssrcs_.end();
         ++it) {
      if (it->second->DeliverRtcp(static_cast<const uint8_t*>(packet),
                                  length)) {
        rtcp_delivered = true;
      }
    }
  }
  return rtcp_delivered;
}

bool VideoCall::DeliverRtp(const RTPHeader& header,
                           const uint8_t* packet,
                           size_t length) {
  VideoReceiveStream* receiver;
  {
    ReadLockScoped read_lock(*receive_lock_);
    std::map<uint32_t, VideoReceiveStream*>::iterator it =
        receive_ssrcs_.find(header.ssrc);
    if (it == receive_ssrcs_.end()) {
      // TODO(pbos): Log some warning, SSRC without receiver.
      return false;
    }

    receiver = it->second;
  }
  return receiver->DeliverRtp(static_cast<const uint8_t*>(packet), length);
}

bool VideoCall::DeliverPacket(const uint8_t* packet, size_t length) {
  // TODO(pbos): ExtensionMap if there are extensions.
  if (RtpHeaderParser::IsRtcp(packet, static_cast<int>(length)))
    return DeliverRtcp(packet, length);

  RTPHeader rtp_header;
  if (!rtp_header_parser_->Parse(packet, static_cast<int>(length), &rtp_header))
    return false;

  return DeliverRtp(rtp_header, packet, length);
}

}  // namespace internal
}  // namespace webrtc
