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

#include <string>
#include <vector>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
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
namespace internal {

VideoSendStream::VideoSendStream(newapi::Transport* transport,
                                 CpuOveruseObserver* overuse_observer,
                                 webrtc::VideoEngine* video_engine,
                                 const VideoSendStream::Config& config,
                                 int base_channel)
    : transport_adapter_(transport),
      encoded_frame_proxy_(config.post_encode_callback),
      codec_lock_(CriticalSectionWrapper::CreateCriticalSection()),
      config_(config),
      external_codec_(NULL),
      channel_(-1),
      stats_proxy_(new SendStatisticsProxy(config, this)) {
  video_engine_base_ = ViEBase::GetInterface(video_engine);
  video_engine_base_->CreateChannel(channel_, base_channel);
  assert(channel_ != -1);

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine);
  assert(rtp_rtcp_ != NULL);

  assert(config_.rtp.ssrcs.size() > 0);
  if (config_.suspend_below_min_bitrate)
    config_.pacing = true;
  rtp_rtcp_->SetTransmissionSmoothingStatus(channel_, config_.pacing);

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
  if (!ReconfigureVideoEncoder(config_.encoder_settings.streams,
                               config_.encoder_settings.encoder_settings)) {
    abort();
  }

  if (overuse_observer)
    video_engine_base_->RegisterCpuOveruseObserver(channel_, overuse_observer);

  image_process_ = ViEImageProcess::GetInterface(video_engine);
  image_process_->RegisterPreEncodeCallback(channel_,
                                            config_.pre_encode_callback);
  if (config_.post_encode_callback) {
    image_process_->RegisterPostEncodeImageCallback(channel_,
                                                    &encoded_frame_proxy_);
  }

  if (config_.suspend_below_min_bitrate) {
    codec_->SuspendBelowMinBitrate(channel_);
  }

  rtp_rtcp_->RegisterSendChannelRtcpStatisticsCallback(channel_,
                                                       stats_proxy_.get());
  rtp_rtcp_->RegisterSendChannelRtpStatisticsCallback(channel_,
                                                      stats_proxy_.get());
  rtp_rtcp_->RegisterSendBitrateObserver(channel_, stats_proxy_.get());
  rtp_rtcp_->RegisterSendFrameCountObserver(channel_, stats_proxy_.get());

  codec_->RegisterEncoderObserver(channel_, *stats_proxy_);
  capture_->RegisterObserver(capture_id_, *stats_proxy_);
}

VideoSendStream::~VideoSendStream() {
  capture_->DeregisterObserver(capture_id_);
  codec_->DeregisterEncoderObserver(channel_);

  rtp_rtcp_->DeregisterSendFrameCountObserver(channel_, stats_proxy_.get());
  rtp_rtcp_->DeregisterSendBitrateObserver(channel_, stats_proxy_.get());
  rtp_rtcp_->DeregisterSendChannelRtpStatisticsCallback(channel_,
                                                        stats_proxy_.get());
  rtp_rtcp_->DeregisterSendChannelRtcpStatisticsCallback(channel_,
                                                         stats_proxy_.get());

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

void VideoSendStream::PutFrame(const I420VideoFrame& frame) {
  input_frame_.CopyFrame(frame);
  SwapFrame(&input_frame_);
}

void VideoSendStream::SwapFrame(I420VideoFrame* frame) {
  // TODO(pbos): Warn if frame is "too far" into the future, or too old. This
  //             would help detect if frame's being used without NTP.
  //             TO REVIEWER: Is there any good check for this? Should it be
  //             skipped?
  if (frame != &input_frame_)
    input_frame_.SwapFrame(frame);

  // TODO(pbos): Local rendering should not be done on the capture thread.
  if (config_.local_renderer != NULL)
    config_.local_renderer->RenderFrame(input_frame_, 0);

  external_capture_->SwapFrame(&input_frame_);
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
    void* encoder_settings) {
  assert(!streams.empty());
  assert(config_.rtp.ssrcs.size() >= streams.size());
  // TODO(pbos): Wire encoder_settings.
  assert(encoder_settings == NULL);

  // VideoStreams in config_.encoder_settings need to be locked.
  CriticalSectionScoped crit(codec_lock_.get());

  VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(video_codec));
  video_codec.codecType =
      (config_.encoder_settings.payload_name == "VP8" ? kVideoCodecVP8
                                                      : kVideoCodecGeneric);

  if (video_codec.codecType == kVideoCodecVP8) {
    video_codec.codecSpecific.VP8.resilience = kResilientStream;
    video_codec.codecSpecific.VP8.numberOfTemporalLayers = 1;
    video_codec.codecSpecific.VP8.denoisingOn = true;
    video_codec.codecSpecific.VP8.errorConcealmentOn = false;
    video_codec.codecSpecific.VP8.automaticResizeOn = false;
    video_codec.codecSpecific.VP8.frameDroppingOn = true;
    video_codec.codecSpecific.VP8.keyFrameInterval = 3000;
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

  if (video_codec.minBitrate < kViEMinCodecBitrate)
    video_codec.minBitrate = kViEMinCodecBitrate;
  if (video_codec.maxBitrate < kViEMinCodecBitrate)
    video_codec.maxBitrate = kViEMinCodecBitrate;

  video_codec.startBitrate = 300;

  if (video_codec.startBitrate < video_codec.minBitrate)
    video_codec.startBitrate = video_codec.minBitrate;
  if (video_codec.startBitrate > video_codec.maxBitrate)
    video_codec.startBitrate = video_codec.maxBitrate;

  assert(config_.encoder_settings.streams[0].max_framerate > 0);
  video_codec.maxFramerate = config_.encoder_settings.streams[0].max_framerate;

  if (codec_->SetSendCodec(channel_, video_codec) != 0)
    return false;

  for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
    rtp_rtcp_->SetLocalSSRC(channel_,
                            config_.rtp.ssrcs[i],
                            kViEStreamTypeNormal,
                            static_cast<unsigned char>(i));
  }

  config_.encoder_settings.streams = streams;
  config_.encoder_settings.encoder_settings = encoder_settings;

  if (config_.rtp.rtx.ssrcs.empty())
    return true;

  // Set up RTX.
  assert(config_.rtp.rtx.ssrcs.size() == config_.rtp.ssrcs.size());
  for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
    rtp_rtcp_->SetLocalSSRC(channel_,
                            config_.rtp.rtx.ssrcs[i],
                            kViEStreamTypeRtx,
                            static_cast<unsigned char>(i));
  }

  if (config_.rtp.rtx.payload_type != 0)
    rtp_rtcp_->SetRtxSendPayloadType(channel_, config_.rtp.rtx.payload_type);

  return true;
}

bool VideoSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return network_->ReceivedRTCPPacket(
             channel_, packet, static_cast<int>(length)) == 0;
}

VideoSendStream::Stats VideoSendStream::GetStats() const {
  return stats_proxy_->GetStats();
}

bool VideoSendStream::GetSendSideDelay(VideoSendStream::Stats* stats) {
  return codec_->GetSendSideDelay(
      channel_, &stats->avg_delay_ms, &stats->max_delay_ms);
}

std::string VideoSendStream::GetCName() {
  char rtcp_cname[ViERTP_RTCP::KMaxRTCPCNameLength];
  rtp_rtcp_->GetRTCPCName(channel_, rtcp_cname);
  return rtcp_cname;
}

}  // namespace internal
}  // namespace webrtc
