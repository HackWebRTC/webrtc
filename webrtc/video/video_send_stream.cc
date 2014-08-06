/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/video_send_stream.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_external_codec.h"
#include "webrtc/video_engine/include/vie_image_process.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
std::string
VideoSendStream::Config::EncoderSettings::ToString() const {
  std::stringstream ss;
  ss << "{payload_name: " << payload_name;
  ss << ", payload_type: " << payload_type;
  if (encoder != NULL)
    ss << ", encoder: " << (encoder != NULL ? "(encoder)" : "NULL");
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::Rtp::Rtx::ToString()
    const {
  std::stringstream ss;
  ss << "{ssrcs: {";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << "}, {";
  }
  ss << '}';

  ss << ", payload_type: " << payload_type;
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{ssrcs: {";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << "}, {";
  }
  ss << '}';

  ss << ", max_packet_size: " << max_packet_size;
  if (min_transmit_bitrate_bps != 0)
    ss << ", min_transmit_bitrate_bps: " << min_transmit_bitrate_bps;

  ss << ", extensions: {";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << "}, {";
  }
  ss << '}';

  if (nack.rtp_history_ms != 0)
    ss << ", nack.rtp_history_ms: " << nack.rtp_history_ms;
  if (fec.ulpfec_payload_type != -1 || fec.red_payload_type != -1)
    ss << ", fec: " << fec.ToString();
  if (rtx.payload_type != 0 || !rtx.ssrcs.empty())
    ss << ", rtx: " << rtx.ToString();
  if (c_name != "")
    ss << ", c_name: " << c_name;
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{encoder_settings: " << encoder_settings.ToString();
  ss << ", rtp: " << rtp.ToString();
  if (pre_encode_callback != NULL)
    ss << ", (pre_encode_callback)";
  if (post_encode_callback != NULL)
    ss << ", (post_encode_callback)";
  if (local_renderer != NULL) {
    ss << ", (local_renderer, render_delay_ms: " << render_delay_ms << ")";
  }
  if (target_delay_ms > 0)
    ss << ", target_delay_ms: " << target_delay_ms;
  if (suspend_below_min_bitrate)
    ss << ", suspend_below_min_bitrate: on";
  ss << '}';
  return ss.str();
}

namespace internal {
VideoSendStream::VideoSendStream(
    newapi::Transport* transport,
    CpuOveruseObserver* overuse_observer,
    webrtc::VideoEngine* video_engine,
    const VideoSendStream::Config& config,
    const std::vector<VideoStream> video_streams,
    const void* encoder_settings,
    const std::map<uint32_t, RtpState>& suspended_ssrcs,
    int base_channel,
    int start_bitrate_bps)
    : transport_adapter_(transport),
      encoded_frame_proxy_(config.post_encode_callback),
      config_(config),
      start_bitrate_bps_(start_bitrate_bps),
      suspended_ssrcs_(suspended_ssrcs),
      external_codec_(NULL),
      channel_(-1),
      stats_proxy_(config) {
  video_engine_base_ = ViEBase::GetInterface(video_engine);
  video_engine_base_->CreateChannel(channel_, base_channel);
  assert(channel_ != -1);
  assert(start_bitrate_bps_ > 0);

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine);
  assert(rtp_rtcp_ != NULL);

  assert(config_.rtp.ssrcs.size() > 0);

  assert(config_.rtp.min_transmit_bitrate_bps >= 0);
  rtp_rtcp_->SetMinTransmitBitrate(channel_,
                                   config_.rtp.min_transmit_bitrate_bps / 1000);

  for (size_t i = 0; i < config_.rtp.extensions.size(); ++i) {
    const std::string& extension = config_.rtp.extensions[i].name;
    int id = config_.rtp.extensions[i].id;
    if (extension == RtpExtension::kTOffset) {
      if (rtp_rtcp_->SetSendTimestampOffsetStatus(channel_, true, id) != 0)
        abort();
    } else if (extension == RtpExtension::kAbsSendTime) {
      if (rtp_rtcp_->SetSendAbsoluteSendTimeStatus(channel_, true, id) != 0)
        abort();
    } else {
      abort();  // Unsupported extension.
    }
  }

  rtp_rtcp_->SetRembStatus(channel_, true, false);

  // Enable NACK, FEC or both.
  if (config_.rtp.fec.red_payload_type != -1) {
    assert(config_.rtp.fec.ulpfec_payload_type != -1);
    if (config_.rtp.nack.rtp_history_ms > 0) {
      rtp_rtcp_->SetHybridNACKFECStatus(
          channel_,
          true,
          static_cast<unsigned char>(config_.rtp.fec.red_payload_type),
          static_cast<unsigned char>(config_.rtp.fec.ulpfec_payload_type));
    } else {
      rtp_rtcp_->SetFECStatus(
          channel_,
          true,
          static_cast<unsigned char>(config_.rtp.fec.red_payload_type),
          static_cast<unsigned char>(config_.rtp.fec.ulpfec_payload_type));
    }
  } else {
    rtp_rtcp_->SetNACKStatus(channel_, config_.rtp.nack.rtp_history_ms > 0);
  }

  ConfigureSsrcs();

  char rtcp_cname[ViERTP_RTCP::KMaxRTCPCNameLength];
  assert(config_.rtp.c_name.length() < ViERTP_RTCP::KMaxRTCPCNameLength);
  strncpy(rtcp_cname, config_.rtp.c_name.c_str(), sizeof(rtcp_cname) - 1);
  rtcp_cname[sizeof(rtcp_cname) - 1] = '\0';

  rtp_rtcp_->SetRTCPCName(channel_, rtcp_cname);

  capture_ = ViECapture::GetInterface(video_engine);
  capture_->AllocateExternalCaptureDevice(capture_id_, external_capture_);
  capture_->ConnectCaptureDevice(capture_id_, channel_);

  network_ = ViENetwork::GetInterface(video_engine);
  assert(network_ != NULL);

  network_->RegisterSendTransport(channel_, transport_adapter_);
  // 28 to match packet overhead in ModuleRtpRtcpImpl.
  network_->SetMTU(channel_,
                   static_cast<unsigned int>(config_.rtp.max_packet_size + 28));

  assert(config.encoder_settings.encoder != NULL);
  assert(config.encoder_settings.payload_type >= 0);
  assert(config.encoder_settings.payload_type <= 127);
  external_codec_ = ViEExternalCodec::GetInterface(video_engine);
  if (external_codec_->RegisterExternalSendCodec(
          channel_,
          config.encoder_settings.payload_type,
          config.encoder_settings.encoder,
          false) != 0) {
    abort();
  }

  codec_ = ViECodec::GetInterface(video_engine);
  if (!ReconfigureVideoEncoder(video_streams, encoder_settings))
    abort();

  if (overuse_observer)
    video_engine_base_->RegisterCpuOveruseObserver(channel_, overuse_observer);

  video_engine_base_->RegisterSendSideDelayObserver(channel_, &stats_proxy_);

  image_process_ = ViEImageProcess::GetInterface(video_engine);
  image_process_->RegisterPreEncodeCallback(channel_,
                                            config_.pre_encode_callback);
  if (config_.post_encode_callback) {
    image_process_->RegisterPostEncodeImageCallback(channel_,
                                                    &encoded_frame_proxy_);
  }

  if (config_.suspend_below_min_bitrate)
    codec_->SuspendBelowMinBitrate(channel_);

  rtp_rtcp_->RegisterSendChannelRtcpStatisticsCallback(channel_,
                                                       &stats_proxy_);
  rtp_rtcp_->RegisterSendChannelRtpStatisticsCallback(channel_,
                                                      &stats_proxy_);
  rtp_rtcp_->RegisterSendBitrateObserver(channel_, &stats_proxy_);
  rtp_rtcp_->RegisterSendFrameCountObserver(channel_, &stats_proxy_);

  codec_->RegisterEncoderObserver(channel_, stats_proxy_);
  capture_->RegisterObserver(capture_id_, stats_proxy_);
}

VideoSendStream::~VideoSendStream() {
  capture_->DeregisterObserver(capture_id_);
  codec_->DeregisterEncoderObserver(channel_);

  rtp_rtcp_->DeregisterSendFrameCountObserver(channel_, &stats_proxy_);
  rtp_rtcp_->DeregisterSendBitrateObserver(channel_, &stats_proxy_);
  rtp_rtcp_->DeregisterSendChannelRtpStatisticsCallback(channel_,
                                                        &stats_proxy_);
  rtp_rtcp_->DeregisterSendChannelRtcpStatisticsCallback(channel_,
                                                         &stats_proxy_);

  image_process_->DeRegisterPreEncodeCallback(channel_);

  network_->DeregisterSendTransport(channel_);

  capture_->DisconnectCaptureDevice(channel_);
  capture_->ReleaseCaptureDevice(capture_id_);

  external_codec_->DeRegisterExternalSendCodec(
      channel_, config_.encoder_settings.payload_type);

  video_engine_base_->DeleteChannel(channel_);

  image_process_->Release();
  video_engine_base_->Release();
  capture_->Release();
  codec_->Release();
  if (external_codec_)
    external_codec_->Release();
  network_->Release();
  rtp_rtcp_->Release();
}

void VideoSendStream::SwapFrame(I420VideoFrame* frame) {
  // TODO(pbos): Local rendering should not be done on the capture thread.
  if (config_.local_renderer != NULL)
    config_.local_renderer->RenderFrame(*frame, 0);

  external_capture_->SwapFrame(frame);
}

VideoSendStreamInput* VideoSendStream::Input() { return this; }

void VideoSendStream::Start() {
  transport_adapter_.Enable();
  video_engine_base_->StartSend(channel_);
  video_engine_base_->StartReceive(channel_);
}

void VideoSendStream::Stop() {
  video_engine_base_->StopSend(channel_);
  video_engine_base_->StopReceive(channel_);
  transport_adapter_.Disable();
}

bool VideoSendStream::ReconfigureVideoEncoder(
    const std::vector<VideoStream>& streams,
    const void* encoder_settings) {
  assert(!streams.empty());
  assert(config_.rtp.ssrcs.size() >= streams.size());

  VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(video_codec));
  if (config_.encoder_settings.payload_name == "VP8") {
    video_codec.codecType = kVideoCodecVP8;
  } else if (config_.encoder_settings.payload_name == "H264") {
    video_codec.codecType = kVideoCodecH264;
  } else {
    video_codec.codecType = kVideoCodecGeneric;
  }

  if (video_codec.codecType == kVideoCodecVP8) {
    video_codec.codecSpecific.VP8.resilience = kResilientStream;
    video_codec.codecSpecific.VP8.numberOfTemporalLayers = 1;
    video_codec.codecSpecific.VP8.denoisingOn = true;
    video_codec.codecSpecific.VP8.errorConcealmentOn = false;
    video_codec.codecSpecific.VP8.automaticResizeOn = false;
    video_codec.codecSpecific.VP8.frameDroppingOn = true;
    video_codec.codecSpecific.VP8.keyFrameInterval = 3000;
  } else if (video_codec.codecType == kVideoCodecH264) {
    video_codec.codecSpecific.H264.profile = kProfileBase;
    video_codec.codecSpecific.H264.frameDroppingOn = true;
    video_codec.codecSpecific.H264.keyFrameInterval = 3000;
  }

  if (video_codec.codecType == kVideoCodecVP8) {
    if (encoder_settings != NULL) {
      video_codec.codecSpecific.VP8 =
          *reinterpret_cast<const VideoCodecVP8*>(encoder_settings);
    }
  } else {
    // TODO(pbos): Support encoder_settings codec-agnostically.
    assert(encoder_settings == NULL);
  }

  strncpy(video_codec.plName,
          config_.encoder_settings.payload_name.c_str(),
          kPayloadNameSize - 1);
  video_codec.plName[kPayloadNameSize - 1] = '\0';
  video_codec.plType = config_.encoder_settings.payload_type;
  video_codec.numberOfSimulcastStreams =
      static_cast<unsigned char>(streams.size());
  video_codec.minBitrate = streams[0].min_bitrate_bps / 1000;
  assert(streams.size() <= kMaxSimulcastStreams);
  for (size_t i = 0; i < streams.size(); ++i) {
    SimulcastStream* sim_stream = &video_codec.simulcastStream[i];
    assert(streams[i].width > 0);
    assert(streams[i].height > 0);
    assert(streams[i].max_framerate > 0);
    // Different framerates not supported per stream at the moment.
    assert(streams[i].max_framerate == streams[0].max_framerate);
    assert(streams[i].min_bitrate_bps >= 0);
    assert(streams[i].target_bitrate_bps >= streams[i].min_bitrate_bps);
    assert(streams[i].max_bitrate_bps >= streams[i].target_bitrate_bps);
    assert(streams[i].max_qp >= 0);

    sim_stream->width = static_cast<unsigned short>(streams[i].width);
    sim_stream->height = static_cast<unsigned short>(streams[i].height);
    sim_stream->minBitrate = streams[i].min_bitrate_bps / 1000;
    sim_stream->targetBitrate = streams[i].target_bitrate_bps / 1000;
    sim_stream->maxBitrate = streams[i].max_bitrate_bps / 1000;
    sim_stream->qpMax = streams[i].max_qp;
    // TODO(pbos): Implement mapping for temporal layers.
    assert(streams[i].temporal_layers.empty());

    video_codec.width = std::max(video_codec.width,
                                 static_cast<unsigned short>(streams[i].width));
    video_codec.height = std::max(
        video_codec.height, static_cast<unsigned short>(streams[i].height));
    video_codec.minBitrate =
        std::min(video_codec.minBitrate,
                 static_cast<unsigned int>(streams[i].min_bitrate_bps / 1000));
    video_codec.maxBitrate += streams[i].max_bitrate_bps / 1000;
    video_codec.qpMax = std::max(video_codec.qpMax,
                                 static_cast<unsigned int>(streams[i].max_qp));
  }
  video_codec.startBitrate =
      static_cast<unsigned int>(start_bitrate_bps_) / 1000;

  if (video_codec.minBitrate < kViEMinCodecBitrate)
    video_codec.minBitrate = kViEMinCodecBitrate;
  if (video_codec.maxBitrate < kViEMinCodecBitrate)
    video_codec.maxBitrate = kViEMinCodecBitrate;
  if (video_codec.startBitrate < video_codec.minBitrate)
    video_codec.startBitrate = video_codec.minBitrate;
  if (video_codec.startBitrate > video_codec.maxBitrate)
    video_codec.startBitrate = video_codec.maxBitrate;

  if (video_codec.startBitrate < video_codec.minBitrate)
    video_codec.startBitrate = video_codec.minBitrate;
  if (video_codec.startBitrate > video_codec.maxBitrate)
    video_codec.startBitrate = video_codec.maxBitrate;

  assert(streams[0].max_framerate > 0);
  video_codec.maxFramerate = streams[0].max_framerate;

  return codec_->SetSendCodec(channel_, video_codec) == 0;
}

bool VideoSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return network_->ReceivedRTCPPacket(
             channel_, packet, static_cast<int>(length)) == 0;
}

VideoSendStream::Stats VideoSendStream::GetStats() const {
  return stats_proxy_.GetStats();
}

void VideoSendStream::ConfigureSsrcs() {
  for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.ssrcs[i];
    rtp_rtcp_->SetLocalSSRC(
        channel_, ssrc, kViEStreamTypeNormal, static_cast<unsigned char>(i));
    RtpStateMap::iterator it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      rtp_rtcp_->SetRtpStateForSsrc(channel_, ssrc, it->second);
  }

  if (config_.rtp.rtx.ssrcs.empty()) {
    assert(!config_.rtp.rtx.pad_with_redundant_payloads);
    return;
  }

  // Set up RTX.
  assert(config_.rtp.rtx.ssrcs.size() == config_.rtp.ssrcs.size());
  for (size_t i = 0; i < config_.rtp.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.rtx.ssrcs[i];
    rtp_rtcp_->SetLocalSSRC(channel_,
                            config_.rtp.rtx.ssrcs[i],
                            kViEStreamTypeRtx,
                            static_cast<unsigned char>(i));
    RtpStateMap::iterator it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      rtp_rtcp_->SetRtpStateForSsrc(channel_, ssrc, it->second);
  }

  if (config_.rtp.rtx.pad_with_redundant_payloads) {
    rtp_rtcp_->SetPadWithRedundantPayloads(channel_, true);
  }

  assert(config_.rtp.rtx.payload_type >= 0);
  rtp_rtcp_->SetRtxSendPayloadType(channel_, config_.rtp.rtx.payload_type);
}

std::map<uint32_t, RtpState> VideoSendStream::GetRtpStates() const {
  std::map<uint32_t, RtpState> rtp_states;
  for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.ssrcs[i];
    rtp_states[ssrc] = rtp_rtcp_->GetRtpStateForSsrc(channel_, ssrc);
  }

  for (size_t i = 0; i < config_.rtp.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.rtx.ssrcs[i];
    rtp_states[ssrc] = rtp_rtcp_->GetRtpStateForSsrc(channel_, ssrc);
  }

  return rtp_states;
}

}  // namespace internal
}  // namespace webrtc
