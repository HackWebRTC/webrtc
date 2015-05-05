/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/video_receive_stream.h"

#include <stdlib.h>

#include <string>

#include "webrtc/base/checks.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/video/receive_statistics_proxy.h"
#include "webrtc/video_encoder.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_receive_stream.h"

namespace webrtc {
std::string VideoReceiveStream::Decoder::ToString() const {
  std::stringstream ss;
  ss << "{decoder: " << (decoder != nullptr ? "(VideoDecoder)" : "nullptr");
  ss << ", payload_type: " << payload_type;
  ss << ", payload_name: " << payload_name;
  ss << ", is_renderer: " << (is_renderer ? "yes" : "no");
  ss << ", expected_delay_ms: " << expected_delay_ms;
  ss << '}';

  return ss.str();
}

std::string VideoReceiveStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{decoders: [";
  for (size_t i = 0; i < decoders.size(); ++i) {
    ss << decoders[i].ToString();
    if (i != decoders.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << ", rtp: " << rtp.ToString();
  ss << ", renderer: " << (renderer != nullptr ? "(renderer)" : "nullptr");
  ss << ", render_delay_ms: " << render_delay_ms;
  ss << ", audio_channel_id: " << audio_channel_id;
  ss << ", pre_decode_callback: "
     << (pre_decode_callback != nullptr ? "(EncodedFrameObserver)" : "nullptr");
  ss << ", pre_render_callback: "
     << (pre_render_callback != nullptr ? "(I420FrameCallback)" : "nullptr");
  ss << ", target_delay_ms: " << target_delay_ms;
  ss << '}';

  return ss.str();
}

std::string VideoReceiveStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{remote_ssrc: " << remote_ssrc;
  ss << ", local_ssrc: " << local_ssrc;
  ss << ", rtcp_mode: " << (rtcp_mode == newapi::kRtcpCompound
                                ? "kRtcpCompound"
                                : "kRtcpReducedSize");
  ss << ", rtcp_xr: ";
  ss << "{receiver_reference_time_report: "
     << (rtcp_xr.receiver_reference_time_report ? "on" : "off");
  ss << '}';
  ss << ", remb: " << (remb ? "on" : "off");
  ss << ", nack: {rtp_history_ms: " << nack.rtp_history_ms << '}';
  ss << ", fec: " << fec.ToString();
  ss << ", rtx: {";
  for (auto& kv : rtx) {
    ss << kv.first << " -> ";
    ss << "{ssrc: " << kv.second.ssrc;
    ss << ", payload_type: " << kv.second.payload_type;
    ss << '}';
  }
  ss << '}';
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << '}';
  return ss.str();
}

namespace internal {
namespace {

VideoCodec CreateDecoderVideoCodec(const VideoReceiveStream::Decoder& decoder) {
  VideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.plType = decoder.payload_type;
  strcpy(codec.plName, decoder.payload_name.c_str());
  if (decoder.payload_name == "VP8") {
    codec.codecType = kVideoCodecVP8;
  } else if (decoder.payload_name == "H264") {
    codec.codecType = kVideoCodecH264;
  } else {
    codec.codecType = kVideoCodecGeneric;
  }

  if (codec.codecType == kVideoCodecVP8) {
    codec.codecSpecific.VP8 = VideoEncoder::GetDefaultVp8Settings();
  } else if (codec.codecType == kVideoCodecH264) {
    codec.codecSpecific.H264 = VideoEncoder::GetDefaultH264Settings();
  }

  codec.width = 320;
  codec.height = 180;
  codec.startBitrate = codec.minBitrate = codec.maxBitrate =
      Call::Config::kDefaultStartBitrateBps / 1000;

  return codec;
}
}  // namespace

VideoReceiveStream::VideoReceiveStream(webrtc::VideoEngine* video_engine,
                                       ChannelGroup* channel_group,
                                       const VideoReceiveStream::Config& config,
                                       newapi::Transport* transport,
                                       webrtc::VoiceEngine* voice_engine,
                                       int base_channel)
    : transport_adapter_(transport),
      encoded_frame_proxy_(config.pre_decode_callback),
      config_(config),
      clock_(Clock::GetRealTimeClock()),
      channel_group_(channel_group),
      voe_sync_interface_(nullptr),
      channel_(-1) {
  video_engine_base_ = ViEBase::GetInterface(video_engine);
  video_engine_base_->CreateReceiveChannel(channel_, base_channel);
  DCHECK(channel_ != -1);

  vie_channel_ = video_engine_base_->GetChannel(channel_);

  // TODO(pbos): This is not fine grained enough...
  vie_channel_->SetNACKStatus(config_.rtp.nack.rtp_history_ms > 0);
  vie_channel_->SetKeyFrameRequestMethod(kKeyFrameReqPliRtcp);
  SetRtcpMode(config_.rtp.rtcp_mode);

  DCHECK(config_.rtp.remote_ssrc != 0);
  // TODO(pbos): What's an appropriate local_ssrc for receive-only streams?
  DCHECK(config_.rtp.local_ssrc != 0);
  DCHECK(config_.rtp.remote_ssrc != config_.rtp.local_ssrc);

  vie_channel_->SetSSRC(config_.rtp.local_ssrc, kViEStreamTypeNormal, 0);
  // TODO(pbos): Support multiple RTX, per video payload.
  Config::Rtp::RtxMap::const_iterator it = config_.rtp.rtx.begin();
  for (; it != config_.rtp.rtx.end(); ++it) {
    DCHECK(it->second.ssrc != 0);
    DCHECK(it->second.payload_type != 0);

    vie_channel_->SetRemoteSSRCType(kViEStreamTypeRtx, it->second.ssrc);
    vie_channel_->SetRtxReceivePayloadType(it->second.payload_type, it->first);
  }

  // TODO(pbos): Remove channel_group_ usage from VideoReceiveStream. This
  // should be configured in call.cc.
  channel_group_->SetChannelRembStatus(false, config_.rtp.remb, vie_channel_);

  for (size_t i = 0; i < config_.rtp.extensions.size(); ++i) {
    const std::string& extension = config_.rtp.extensions[i].name;
    int id = config_.rtp.extensions[i].id;
    // One-byte-extension local identifiers are in the range 1-14 inclusive.
    DCHECK_GE(id, 1);
    DCHECK_LE(id, 14);
    if (extension == RtpExtension::kTOffset) {
      CHECK_EQ(0, vie_channel_->SetReceiveTimestampOffsetStatus(true, id));
    } else if (extension == RtpExtension::kAbsSendTime) {
      CHECK_EQ(0, vie_channel_->SetReceiveAbsoluteSendTimeStatus(true, id));
    } else if (extension == RtpExtension::kVideoRotation) {
      CHECK_EQ(0, vie_channel_->SetReceiveVideoRotationStatus(true, id));
    } else {
      RTC_NOTREACHED() << "Unsupported RTP extension.";
    }
  }

  vie_channel_->RegisterSendTransport(&transport_adapter_);

  if (config_.rtp.fec.ulpfec_payload_type != -1) {
    // ULPFEC without RED doesn't make sense.
    DCHECK(config_.rtp.fec.red_payload_type != -1);
    VideoCodec codec;
    memset(&codec, 0, sizeof(codec));
    codec.codecType = kVideoCodecULPFEC;
    strcpy(codec.plName, "ulpfec");
    codec.plType = config_.rtp.fec.ulpfec_payload_type;
    CHECK_EQ(0, vie_channel_->SetReceiveCodec(codec));
  }
  if (config_.rtp.fec.red_payload_type != -1) {
    VideoCodec codec;
    memset(&codec, 0, sizeof(codec));
    codec.codecType = kVideoCodecRED;
    strcpy(codec.plName, "red");
    codec.plType = config_.rtp.fec.red_payload_type;
    CHECK_EQ(0, vie_channel_->SetReceiveCodec(codec));
    if (config_.rtp.fec.red_rtx_payload_type != -1) {
      vie_channel_->SetRtxReceivePayloadType(
          config_.rtp.fec.red_rtx_payload_type,
          config_.rtp.fec.red_payload_type);
    }
  }

  if (config.rtp.rtcp_xr.receiver_reference_time_report)
    vie_channel_->SetRtcpXrRrtrStatus(true);

  stats_proxy_.reset(
      new ReceiveStatisticsProxy(config_.rtp.remote_ssrc, clock_));

  vie_channel_->RegisterReceiveChannelRtcpStatisticsCallback(
      stats_proxy_.get());
  vie_channel_->RegisterReceiveChannelRtpStatisticsCallback(stats_proxy_.get());
  vie_channel_->RegisterRtcpPacketTypeCounterObserver(stats_proxy_.get());
  vie_channel_->RegisterCodecObserver(stats_proxy_.get());

  vie_channel_->RegisterReceiveStatisticsProxy(stats_proxy_.get());

  DCHECK(!config_.decoders.empty());
  for (size_t i = 0; i < config_.decoders.size(); ++i) {
    const Decoder& decoder = config_.decoders[i];
    CHECK_EQ(0, vie_channel_->RegisterExternalDecoder(
                    decoder.payload_type, decoder.decoder, decoder.is_renderer,
                    decoder.expected_delay_ms));

    VideoCodec codec = CreateDecoderVideoCodec(decoder);

    CHECK_EQ(0, vie_channel_->SetReceiveCodec(codec));
  }

  // Register a renderer without a window handle, at depth 0, that covers the
  // entire rendered area (0->1 both axes). This registers a renderer that
  // renders the entire video.
  incoming_video_stream_.reset(new IncomingVideoStream(channel_));
  incoming_video_stream_->SetExpectedRenderDelay(config.render_delay_ms);
  incoming_video_stream_->SetExternalCallback(this);
  vie_channel_->SetIncomingVideoStream(incoming_video_stream_.get());

  if (voice_engine && config_.audio_channel_id != -1) {
    voe_sync_interface_ = VoEVideoSync::GetInterface(voice_engine);
    vie_channel_->SetVoiceChannel(config.audio_channel_id, voe_sync_interface_);
  }

  if (config.pre_decode_callback)
    vie_channel_->RegisterPreDecodeImageCallback(&encoded_frame_proxy_);
  vie_channel_->RegisterPreRenderCallback(this);
}

VideoReceiveStream::~VideoReceiveStream() {
  incoming_video_stream_->Stop();
  vie_channel_->RegisterPreRenderCallback(nullptr);
  vie_channel_->RegisterPreDecodeImageCallback(nullptr);

  for (size_t i = 0; i < config_.decoders.size(); ++i)
    vie_channel_->DeRegisterExternalDecoder(config_.decoders[i].payload_type);

  vie_channel_->DeregisterSendTransport();

  if (voe_sync_interface_ != nullptr) {
    vie_channel_->SetVoiceChannel(-1, nullptr);
    voe_sync_interface_->Release();
  }
  vie_channel_->RegisterCodecObserver(nullptr);
  vie_channel_->RegisterReceiveChannelRtpStatisticsCallback(nullptr);
  vie_channel_->RegisterReceiveChannelRtcpStatisticsCallback(nullptr);
  vie_channel_->RegisterRtcpPacketTypeCounterObserver(nullptr);
  video_engine_base_->DeleteChannel(channel_);
  video_engine_base_->Release();
}

void VideoReceiveStream::Start() {
  transport_adapter_.Enable();
  incoming_video_stream_->Start();
  vie_channel_->StartReceive();
}

void VideoReceiveStream::Stop() {
  incoming_video_stream_->Stop();
  vie_channel_->StopReceive();
  transport_adapter_.Disable();
}

VideoReceiveStream::Stats VideoReceiveStream::GetStats() const {
  return stats_proxy_->GetStats();
}

bool VideoReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return vie_channel_->ReceivedRTCPPacket(packet, length) == 0;
}

bool VideoReceiveStream::DeliverRtp(const uint8_t* packet, size_t length) {
  return vie_channel_->ReceivedRTPPacket(packet, length, PacketTime()) == 0;
}

void VideoReceiveStream::FrameCallback(I420VideoFrame* video_frame) {
  stats_proxy_->OnDecodedFrame();

  if (config_.pre_render_callback)
    config_.pre_render_callback->FrameCallback(video_frame);
}

int VideoReceiveStream::RenderFrame(const uint32_t /*stream_id*/,
                                    const I420VideoFrame& video_frame) {
  // TODO(pbos): Wire up config_.render->IsTextureSupported() and convert if not
  // supported. Or provide methods for converting a texture frame in
  // I420VideoFrame.

  if (config_.renderer != nullptr)
    config_.renderer->RenderFrame(
        video_frame,
        video_frame.render_time_ms() - clock_->TimeInMilliseconds());

  stats_proxy_->OnRenderedFrame();

  return 0;
}

void VideoReceiveStream::SignalNetworkState(Call::NetworkState state) {
  if (state == Call::kNetworkUp)
    SetRtcpMode(config_.rtp.rtcp_mode);
  if (state == Call::kNetworkDown)
    vie_channel_->SetRTCPMode(kRtcpOff);
}

void VideoReceiveStream::SetRtcpMode(newapi::RtcpMode mode) {
  switch (mode) {
    case newapi::kRtcpCompound:
      vie_channel_->SetRTCPMode(kRtcpCompound);
      break;
    case newapi::kRtcpReducedSize:
      vie_channel_->SetRTCPMode(kRtcpNonCompound);
      break;
  }
}
}  // namespace internal
}  // namespace webrtc
