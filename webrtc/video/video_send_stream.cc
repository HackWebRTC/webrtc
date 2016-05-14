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

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/modules/video_coding/utility/ivf_file_writer.h"
#include "webrtc/video/call_stats.h"
#include "webrtc/video/video_capture_input.h"
#include "webrtc/video/vie_remb.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

class RtcpIntraFrameObserver;
class TransportFeedbackObserver;

static const int kMinSendSidePacketHistorySize = 600;

namespace {

std::vector<RtpRtcp*> CreateRtpRtcpModules(
    Transport* outgoing_transport,
    RtcpIntraFrameObserver* intra_frame_callback,
    RtcpBandwidthObserver* bandwidth_callback,
    TransportFeedbackObserver* transport_feedback_callback,
    RtcpRttStats* rtt_stats,
    RtpPacketSender* paced_sender,
    TransportSequenceNumberAllocator* transport_sequence_number_allocator,
    SendStatisticsProxy* stats_proxy,
    SendDelayStats* send_delay_stats,
    size_t num_modules) {
  RTC_DCHECK_GT(num_modules, 0u);
  RtpRtcp::Configuration configuration;
  ReceiveStatistics* null_receive_statistics = configuration.receive_statistics;
  configuration.audio = false;
  configuration.receiver_only = false;
  configuration.receive_statistics = null_receive_statistics;
  configuration.outgoing_transport = outgoing_transport;
  configuration.intra_frame_callback = intra_frame_callback;
  configuration.rtt_stats = rtt_stats;
  configuration.rtcp_packet_type_counter_observer = stats_proxy;
  configuration.paced_sender = paced_sender;
  configuration.transport_sequence_number_allocator =
      transport_sequence_number_allocator;
  configuration.send_bitrate_observer = stats_proxy;
  configuration.send_frame_count_observer = stats_proxy;
  configuration.send_side_delay_observer = stats_proxy;
  configuration.send_packet_observer = send_delay_stats;
  configuration.bandwidth_callback = bandwidth_callback;
  configuration.transport_feedback_callback = transport_feedback_callback;

  std::vector<RtpRtcp*> modules;
  for (size_t i = 0; i < num_modules; ++i) {
    RtpRtcp* rtp_rtcp = RtpRtcp::CreateRtpRtcp(configuration);
    rtp_rtcp->SetSendingStatus(false);
    rtp_rtcp->SetSendingMediaStatus(false);
    rtp_rtcp->SetRTCPStatus(RtcpMode::kCompound);
    modules.push_back(rtp_rtcp);
  }
  return modules;
}

}  // namespace

std::string
VideoSendStream::Config::EncoderSettings::ToString() const {
  std::stringstream ss;
  ss << "{payload_name: " << payload_name;
  ss << ", payload_type: " << payload_type;
  ss << ", encoder: " << (encoder ? "(VideoEncoder)" : "nullptr");
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::Rtp::Rtx::ToString()
    const {
  std::stringstream ss;
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", payload_type: " << payload_type;
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << ", rtcp_mode: "
     << (rtcp_mode == RtcpMode::kCompound ? "RtcpMode::kCompound"
                                          : "RtcpMode::kReducedSize");
  ss << ", max_packet_size: " << max_packet_size;
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", nack: {rtp_history_ms: " << nack.rtp_history_ms << '}';
  ss << ", fec: " << fec.ToString();
  ss << ", rtx: " << rtx.ToString();
  ss << ", c_name: " << c_name;
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{encoder_settings: " << encoder_settings.ToString();
  ss << ", rtp: " << rtp.ToString();
  ss << ", pre_encode_callback: "
     << (pre_encode_callback ? "(I420FrameCallback)" : "nullptr");
  ss << ", post_encode_callback: "
     << (post_encode_callback ? "(EncodedFrameObserver)" : "nullptr");
  ss << ", local_renderer: "
     << (local_renderer ? "(VideoRenderer)" : "nullptr");
  ss << ", render_delay_ms: " << render_delay_ms;
  ss << ", target_delay_ms: " << target_delay_ms;
  ss << ", suspend_below_min_bitrate: " << (suspend_below_min_bitrate ? "on"
                                                                      : "off");
  ss << '}';
  return ss.str();
}

namespace {

VideoCodecType PayloadNameToCodecType(const std::string& payload_name) {
  if (payload_name == "VP8")
    return kVideoCodecVP8;
  if (payload_name == "VP9")
    return kVideoCodecVP9;
  if (payload_name == "H264")
    return kVideoCodecH264;
  return kVideoCodecGeneric;
}

bool PayloadTypeSupportsSkippingFecPackets(const std::string& payload_name) {
  switch (PayloadNameToCodecType(payload_name)) {
    case kVideoCodecVP8:
    case kVideoCodecVP9:
      return true;
    case kVideoCodecH264:
    case kVideoCodecGeneric:
      return false;
    case kVideoCodecI420:
    case kVideoCodecRED:
    case kVideoCodecULPFEC:
    case kVideoCodecUnknown:
      RTC_NOTREACHED();
      return false;
  }
  RTC_NOTREACHED();
  return false;
}

// TODO(pbos): Lower these thresholds (to closer to 100%) when we handle
// pipelining encoders better (multiple input frames before something comes
// out). This should effectively turn off CPU adaptations for systems that
// remotely cope with the load right now.
CpuOveruseOptions GetCpuOveruseOptions(bool full_overuse_time) {
  CpuOveruseOptions options;
  if (full_overuse_time) {
    options.low_encode_usage_threshold_percent = 150;
    options.high_encode_usage_threshold_percent = 200;
  }
  return options;
}

VideoCodec VideoEncoderConfigToVideoCodec(const VideoEncoderConfig& config,
                                          const std::string& payload_name,
                                          int payload_type) {
  const std::vector<VideoStream>& streams = config.streams;
  static const int kEncoderMinBitrateKbps = 30;
  RTC_DCHECK(!streams.empty());
  RTC_DCHECK_GE(config.min_transmit_bitrate_bps, 0);

  VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(video_codec));
  video_codec.codecType = PayloadNameToCodecType(payload_name);

  switch (config.content_type) {
    case VideoEncoderConfig::ContentType::kRealtimeVideo:
      video_codec.mode = kRealtimeVideo;
      break;
    case VideoEncoderConfig::ContentType::kScreen:
      video_codec.mode = kScreensharing;
      if (config.streams.size() == 1 &&
          config.streams[0].temporal_layer_thresholds_bps.size() == 1) {
        video_codec.targetBitrate =
            config.streams[0].temporal_layer_thresholds_bps[0] / 1000;
      }
      break;
  }

  switch (video_codec.codecType) {
    case kVideoCodecVP8: {
      if (config.encoder_specific_settings) {
        video_codec.codecSpecific.VP8 = *reinterpret_cast<const VideoCodecVP8*>(
            config.encoder_specific_settings);
      } else {
        video_codec.codecSpecific.VP8 = VideoEncoder::GetDefaultVp8Settings();
      }
      video_codec.codecSpecific.VP8.numberOfTemporalLayers =
          static_cast<unsigned char>(
              streams.back().temporal_layer_thresholds_bps.size() + 1);
      break;
    }
    case kVideoCodecVP9: {
      if (config.encoder_specific_settings) {
        video_codec.codecSpecific.VP9 = *reinterpret_cast<const VideoCodecVP9*>(
            config.encoder_specific_settings);
        if (video_codec.mode == kScreensharing) {
          video_codec.codecSpecific.VP9.flexibleMode = true;
          // For now VP9 screensharing use 1 temporal and 2 spatial layers.
          RTC_DCHECK_EQ(video_codec.codecSpecific.VP9.numberOfTemporalLayers,
                        1);
          RTC_DCHECK_EQ(video_codec.codecSpecific.VP9.numberOfSpatialLayers, 2);
        }
      } else {
        video_codec.codecSpecific.VP9 = VideoEncoder::GetDefaultVp9Settings();
      }
      video_codec.codecSpecific.VP9.numberOfTemporalLayers =
          static_cast<unsigned char>(
              streams.back().temporal_layer_thresholds_bps.size() + 1);
      break;
    }
    case kVideoCodecH264: {
      if (config.encoder_specific_settings) {
        video_codec.codecSpecific.H264 =
            *reinterpret_cast<const VideoCodecH264*>(
                config.encoder_specific_settings);
      } else {
        video_codec.codecSpecific.H264 = VideoEncoder::GetDefaultH264Settings();
      }
      break;
    }
    default:
      // TODO(pbos): Support encoder_settings codec-agnostically.
      RTC_DCHECK(!config.encoder_specific_settings)
          << "Encoder-specific settings for codec type not wired up.";
      break;
  }

  strncpy(video_codec.plName, payload_name.c_str(), kPayloadNameSize - 1);
  video_codec.plName[kPayloadNameSize - 1] = '\0';
  video_codec.plType = payload_type;
  video_codec.numberOfSimulcastStreams =
      static_cast<unsigned char>(streams.size());
  video_codec.minBitrate = streams[0].min_bitrate_bps / 1000;
  if (video_codec.minBitrate < kEncoderMinBitrateKbps)
    video_codec.minBitrate = kEncoderMinBitrateKbps;
  RTC_DCHECK_LE(streams.size(), static_cast<size_t>(kMaxSimulcastStreams));
  if (video_codec.codecType == kVideoCodecVP9) {
    // If the vector is empty, bitrates will be configured automatically.
    RTC_DCHECK(config.spatial_layers.empty() ||
               config.spatial_layers.size() ==
                   video_codec.codecSpecific.VP9.numberOfSpatialLayers);
    RTC_DCHECK_LE(video_codec.codecSpecific.VP9.numberOfSpatialLayers,
                  kMaxSimulcastStreams);
    for (size_t i = 0; i < config.spatial_layers.size(); ++i)
      video_codec.spatialLayers[i] = config.spatial_layers[i];
  }
  for (size_t i = 0; i < streams.size(); ++i) {
    SimulcastStream* sim_stream = &video_codec.simulcastStream[i];
    RTC_DCHECK_GT(streams[i].width, 0u);
    RTC_DCHECK_GT(streams[i].height, 0u);
    RTC_DCHECK_GT(streams[i].max_framerate, 0);
    // Different framerates not supported per stream at the moment.
    RTC_DCHECK_EQ(streams[i].max_framerate, streams[0].max_framerate);
    RTC_DCHECK_GE(streams[i].min_bitrate_bps, 0);
    RTC_DCHECK_GE(streams[i].target_bitrate_bps, streams[i].min_bitrate_bps);
    RTC_DCHECK_GE(streams[i].max_bitrate_bps, streams[i].target_bitrate_bps);
    RTC_DCHECK_GE(streams[i].max_qp, 0);

    sim_stream->width = static_cast<uint16_t>(streams[i].width);
    sim_stream->height = static_cast<uint16_t>(streams[i].height);
    sim_stream->minBitrate = streams[i].min_bitrate_bps / 1000;
    sim_stream->targetBitrate = streams[i].target_bitrate_bps / 1000;
    sim_stream->maxBitrate = streams[i].max_bitrate_bps / 1000;
    sim_stream->qpMax = streams[i].max_qp;
    sim_stream->numberOfTemporalLayers = static_cast<unsigned char>(
        streams[i].temporal_layer_thresholds_bps.size() + 1);

    video_codec.width = std::max(video_codec.width,
                                 static_cast<uint16_t>(streams[i].width));
    video_codec.height = std::max(
        video_codec.height, static_cast<uint16_t>(streams[i].height));
    video_codec.minBitrate =
        std::min(static_cast<uint16_t>(video_codec.minBitrate),
                 static_cast<uint16_t>(streams[i].min_bitrate_bps / 1000));
    video_codec.maxBitrate += streams[i].max_bitrate_bps / 1000;
    video_codec.qpMax = std::max(video_codec.qpMax,
                                 static_cast<unsigned int>(streams[i].max_qp));
  }

  if (video_codec.maxBitrate == 0) {
    // Unset max bitrate -> cap to one bit per pixel.
    video_codec.maxBitrate =
        (video_codec.width * video_codec.height * video_codec.maxFramerate) /
        1000;
  }
  if (video_codec.maxBitrate < kEncoderMinBitrateKbps)
    video_codec.maxBitrate = kEncoderMinBitrateKbps;

  RTC_DCHECK_GT(streams[0].max_framerate, 0);
  video_codec.maxFramerate = streams[0].max_framerate;

  return video_codec;
}

}  // namespace

namespace internal {
VideoSendStream::VideoSendStream(
    int num_cpu_cores,
    ProcessThread* module_process_thread,
    CallStats* call_stats,
    CongestionController* congestion_controller,
    BitrateAllocator* bitrate_allocator,
    SendDelayStats* send_delay_stats,
    VieRemb* remb,
    const VideoSendStream::Config& config,
    const VideoEncoderConfig& encoder_config,
    const std::map<uint32_t, RtpState>& suspended_ssrcs)
    : stats_proxy_(Clock::GetRealTimeClock(),
                   config,
                   encoder_config.content_type),
      encoded_frame_proxy_(config.post_encode_callback),
      config_(config),
      suspended_ssrcs_(suspended_ssrcs),
      module_process_thread_(module_process_thread),
      call_stats_(call_stats),
      congestion_controller_(congestion_controller),
      bitrate_allocator_(bitrate_allocator),
      remb_(remb),
      encoder_thread_(EncoderThreadFunction, this, "EncoderThread"),
      encoder_wakeup_event_(false, false),
      stop_encoder_thread_(0),
      overuse_detector_(
          Clock::GetRealTimeClock(),
          GetCpuOveruseOptions(config.encoder_settings.full_overuse_time),
          this,
          config.post_encode_callback,
          &stats_proxy_),
      vie_encoder_(num_cpu_cores,
                   module_process_thread_,
                   &stats_proxy_,
                   &overuse_detector_),
      encoder_feedback_(Clock::GetRealTimeClock(),
                        config.rtp.ssrcs,
                        &vie_encoder_),
      video_sender_(vie_encoder_.video_sender()),
      bandwidth_observer_(congestion_controller_->GetBitrateController()
                              ->CreateRtcpBandwidthObserver()),
      rtp_rtcp_modules_(CreateRtpRtcpModules(
          config.send_transport,
          &encoder_feedback_,
          bandwidth_observer_.get(),
          congestion_controller_->GetTransportFeedbackObserver(),
          call_stats_->rtcp_rtt_stats(),
          congestion_controller_->pacer(),
          congestion_controller_->packet_router(),
          &stats_proxy_,
          send_delay_stats,
          config_.rtp.ssrcs.size())),
      payload_router_(rtp_rtcp_modules_, config.encoder_settings.payload_type),
      input_(&encoder_wakeup_event_,
             config_.local_renderer,
             &stats_proxy_,
             &overuse_detector_) {
  LOG(LS_INFO) << "VideoSendStream: " << config_.ToString();

  RTC_DCHECK(!config_.rtp.ssrcs.empty());
  RTC_DCHECK(module_process_thread_);
  RTC_DCHECK(call_stats_);
  RTC_DCHECK(congestion_controller_);
  RTC_DCHECK(remb_);


  // RTP/RTCP initialization.
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    module_process_thread_->RegisterModule(rtp_rtcp);
    congestion_controller_->packet_router()->AddRtpModule(rtp_rtcp);
  }

  video_sender_->RegisterProtectionCallback(this);

  for (size_t i = 0; i < config_.rtp.extensions.size(); ++i) {
    const std::string& extension = config_.rtp.extensions[i].name;
    int id = config_.rtp.extensions[i].id;
    // One-byte-extension local identifiers are in the range 1-14 inclusive.
    RTC_DCHECK_GE(id, 1);
    RTC_DCHECK_LE(id, 14);
    RTC_DCHECK(RtpExtension::IsSupportedForVideo(extension));
    for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
      RTC_CHECK_EQ(0, rtp_rtcp->RegisterSendRtpHeaderExtension(
                          StringToRtpExtensionType(extension), id));
    }
  }

  remb_->AddRembSender(rtp_rtcp_modules_[0]);
  rtp_rtcp_modules_[0]->SetREMBStatus(true);

  ConfigureProtection();
  ConfigureSsrcs();

  // TODO(pbos): Should we set CNAME on all RTP modules?
  rtp_rtcp_modules_.front()->SetCNAME(config_.rtp.c_name.c_str());
  // 28 to match packet overhead in ModuleRtpRtcpImpl.
  static const size_t kRtpPacketSizeOverhead = 28;
  RTC_DCHECK_LE(config_.rtp.max_packet_size, 0xFFFFu + kRtpPacketSizeOverhead);
  const uint16_t mtu = static_cast<uint16_t>(config_.rtp.max_packet_size +
                                             kRtpPacketSizeOverhead);
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->RegisterRtcpStatisticsCallback(&stats_proxy_);
    rtp_rtcp->RegisterSendChannelRtpStatisticsCallback(&stats_proxy_);
    rtp_rtcp->SetMaxTransferUnit(mtu);
    rtp_rtcp->RegisterVideoSendPayload(
        config_.encoder_settings.payload_type,
        config_.encoder_settings.payload_name.c_str());
  }

  RTC_DCHECK(config.encoder_settings.encoder);
  RTC_DCHECK_GE(config.encoder_settings.payload_type, 0);
  RTC_DCHECK_LE(config.encoder_settings.payload_type, 127);
  ReconfigureVideoEncoder(encoder_config);

  module_process_thread_->RegisterModule(&overuse_detector_);

  encoder_thread_.Start();
  encoder_thread_.SetPriority(rtc::kHighPriority);
}

VideoSendStream::~VideoSendStream() {
  LOG(LS_INFO) << "~VideoSendStream: " << config_.ToString();

  Stop();

  // Stop the encoder thread permanently.
  rtc::AtomicOps::ReleaseStore(&stop_encoder_thread_, 1);
  encoder_wakeup_event_.Set();
  encoder_thread_.Stop();

  // This needs to happen after stopping the encoder thread,
  // since the encoder thread calls AddObserver.
  bitrate_allocator_->RemoveObserver(this);

  module_process_thread_->DeRegisterModule(&overuse_detector_);

  rtp_rtcp_modules_[0]->SetREMBStatus(false);
  remb_->RemoveRembSender(rtp_rtcp_modules_[0]);

  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    congestion_controller_->packet_router()->RemoveRtpModule(rtp_rtcp);
    module_process_thread_->DeRegisterModule(rtp_rtcp);
    delete rtp_rtcp;
  }
}

bool VideoSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->IncomingRtcpPacket(packet, length);
  return true;
}

void VideoSendStream::Start() {
  if (payload_router_.active())
    return;
  TRACE_EVENT_INSTANT0("webrtc", "VideoSendStream::Start");
  payload_router_.set_active(true);
  // Was not already started, trigger a keyframe.
  vie_encoder_.SendKeyFrame();
  vie_encoder_.Start();
}

void VideoSendStream::Stop() {
  if (!payload_router_.active())
    return;
  TRACE_EVENT_INSTANT0("webrtc", "VideoSendStream::Stop");
  vie_encoder_.Pause();
  payload_router_.set_active(false);
}

VideoCaptureInput* VideoSendStream::Input() {
  return &input_;
}

bool VideoSendStream::EncoderThreadFunction(void* obj) {
  static_cast<VideoSendStream*>(obj)->EncoderProcess();
  // We're done, return false to abort.
  return false;
}

void VideoSendStream::EncoderProcess() {
  RTC_CHECK_EQ(0, vie_encoder_.RegisterExternalEncoder(
                      config_.encoder_settings.encoder,
                      config_.encoder_settings.payload_type,
                      config_.encoder_settings.internal_source));

  while (true) {
    encoder_wakeup_event_.Wait(rtc::Event::kForever);
    if (rtc::AtomicOps::AcquireLoad(&stop_encoder_thread_))
      break;
    rtc::Optional<EncoderSettings> encoder_settings;
    {
      rtc::CritScope lock(&encoder_settings_crit_);
      if (pending_encoder_settings_) {
        encoder_settings = pending_encoder_settings_;
        pending_encoder_settings_ = rtc::Optional<EncoderSettings>();
      }
    }
    if (encoder_settings) {
      encoder_settings->video_codec.startBitrate =
          bitrate_allocator_->AddObserver(
              this, encoder_settings->video_codec.minBitrate * 1000,
              encoder_settings->video_codec.maxBitrate * 1000,
              !config_.suspend_below_min_bitrate) /
          1000;

      payload_router_.SetSendStreams(encoder_settings->streams);
      vie_encoder_.SetEncoder(encoder_settings->video_codec,
                              encoder_settings->min_transmit_bitrate_bps,
                              payload_router_.MaxPayloadLength(), this);

      // vie_encoder_.SetEncoder must be called before this.
      if (config_.suspend_below_min_bitrate)
        video_sender_->SuspendBelowMinBitrate();

      // Clear stats for disabled layers.
      for (size_t i = encoder_settings->streams.size();
           i < config_.rtp.ssrcs.size(); ++i) {
        stats_proxy_.OnInactiveSsrc(config_.rtp.ssrcs[i]);
      }

      // We might've gotten new settings while configuring the encoder settings,
      // restart from the top to see if that's the case before trying to encode
      // a frame (which might correspond to the last frame size).
      encoder_wakeup_event_.Set();
      continue;
    }

    VideoFrame frame;
    if (input_.GetVideoFrame(&frame)) {
      // TODO(perkj): |pre_encode_callback| is only used by tests. Tests should
      // register as a sink to the VideoSource instead.
      if (config_.pre_encode_callback) {
        config_.pre_encode_callback->OnFrame(frame);
      }
      vie_encoder_.EncodeVideoFrame(frame);
    }
  }
  vie_encoder_.DeRegisterExternalEncoder(config_.encoder_settings.payload_type);
}

void VideoSendStream::ReconfigureVideoEncoder(
    const VideoEncoderConfig& config) {
  TRACE_EVENT0("webrtc", "VideoSendStream::(Re)configureVideoEncoder");
  LOG(LS_INFO) << "(Re)configureVideoEncoder: " << config.ToString();
  RTC_DCHECK_GE(config_.rtp.ssrcs.size(), config.streams.size());
  VideoCodec video_codec = VideoEncoderConfigToVideoCodec(
      config, config_.encoder_settings.payload_name,
      config_.encoder_settings.payload_type);
  {
    rtc::CritScope lock(&encoder_settings_crit_);
    pending_encoder_settings_ = rtc::Optional<EncoderSettings>(
        {video_codec, config.min_transmit_bitrate_bps, config.streams});
  }
  encoder_wakeup_event_.Set();
}

VideoSendStream::Stats VideoSendStream::GetStats() {
  return stats_proxy_.GetStats();
}

void VideoSendStream::OveruseDetected() {
  if (config_.overuse_callback)
    config_.overuse_callback->OnLoadUpdate(LoadObserver::kOveruse);
}

void VideoSendStream::NormalUsage() {
  if (config_.overuse_callback)
    config_.overuse_callback->OnLoadUpdate(LoadObserver::kUnderuse);
}

int32_t VideoSendStream::Encoded(const EncodedImage& encoded_image,
                                 const CodecSpecificInfo* codec_specific_info,
                                 const RTPFragmentationHeader* fragmentation) {
  // |encoded_frame_proxy_| forwards frames to |config_.post_encode_callback|;
  encoded_frame_proxy_.Encoded(encoded_image, codec_specific_info,
                               fragmentation);
  int32_t return_value = payload_router_.Encoded(
      encoded_image, codec_specific_info, fragmentation);

  if (kEnableFrameRecording) {
    int layer = codec_specific_info->codecType == kVideoCodecVP8
                    ? codec_specific_info->codecSpecific.VP8.simulcastIdx
                    : 0;
    IvfFileWriter* file_writer;
    {
      if (file_writers_[layer] == nullptr) {
        std::ostringstream oss;
        oss << "send_bitstream_ssrc";
        for (uint32_t ssrc : config_.rtp.ssrcs)
          oss << "_" << ssrc;
        oss << "_layer" << layer << ".ivf";
        file_writers_[layer] =
            IvfFileWriter::Open(oss.str(), codec_specific_info->codecType);
      }
      file_writer = file_writers_[layer].get();
    }
    if (file_writer) {
      bool ok = file_writer->WriteFrame(encoded_image);
      RTC_DCHECK(ok);
    }
  }

  return return_value;
}

void VideoSendStream::ConfigureProtection() {
  // Enable NACK, FEC or both.
  const bool enable_protection_nack = config_.rtp.nack.rtp_history_ms > 0;
  bool enable_protection_fec = config_.rtp.fec.red_payload_type != -1;
  // Payload types without picture ID cannot determine that a stream is complete
  // without retransmitting FEC, so using FEC + NACK for H.264 (for instance) is
  // a waste of bandwidth since FEC packets still have to be transmitted. Note
  // that this is not the case with FLEXFEC.
  if (enable_protection_nack &&
      !PayloadTypeSupportsSkippingFecPackets(
          config_.encoder_settings.payload_name)) {
    LOG(LS_WARNING) << "Transmitting payload type without picture ID using"
                       "NACK+FEC is a waste of bandwidth since FEC packets "
                       "also have to be retransmitted. Disabling FEC.";
    enable_protection_fec = false;
  }

  // Set to valid uint8_ts to be castable later without signed overflows.
  uint8_t payload_type_red = 0;
  uint8_t payload_type_fec = 0;
  // TODO(changbin): Should set RTX for RED mapping in RTP sender in future.
  // Validate payload types. If either RED or FEC payload types are set then
  // both should be. If FEC is enabled then they both have to be set.
  if (enable_protection_fec || config_.rtp.fec.red_payload_type != -1 ||
      config_.rtp.fec.ulpfec_payload_type != -1) {
    RTC_DCHECK_GE(config_.rtp.fec.red_payload_type, 0);
    RTC_DCHECK_GE(config_.rtp.fec.ulpfec_payload_type, 0);
    RTC_DCHECK_LE(config_.rtp.fec.red_payload_type, 127);
    RTC_DCHECK_LE(config_.rtp.fec.ulpfec_payload_type, 127);
    payload_type_red = static_cast<uint8_t>(config_.rtp.fec.red_payload_type);
    payload_type_fec =
        static_cast<uint8_t>(config_.rtp.fec.ulpfec_payload_type);
  } else {
    // Payload types unset.
    RTC_DCHECK_EQ(config_.rtp.fec.red_payload_type, -1);
    RTC_DCHECK_EQ(config_.rtp.fec.ulpfec_payload_type, -1);
  }

  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    // Set NACK.
    rtp_rtcp->SetStorePacketsStatus(
        enable_protection_nack || congestion_controller_->pacer(),
        kMinSendSidePacketHistorySize);
    // Set FEC.
    for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
      rtp_rtcp->SetGenericFECStatus(enable_protection_fec, payload_type_red,
                                    payload_type_fec);
    }
  }

  vie_encoder_.SetProtectionMethod(enable_protection_nack,
                                   enable_protection_fec);
}

void VideoSendStream::ConfigureSsrcs() {
  // Configure regular SSRCs.
  for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.ssrcs[i];
    RtpRtcp* const rtp_rtcp = rtp_rtcp_modules_[i];
    rtp_rtcp->SetSSRC(ssrc);

    // Restore RTP state if previous existed.
    RtpStateMap::iterator it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      rtp_rtcp->SetRtpState(it->second);
  }

  // Set up RTX if available.
  if (config_.rtp.rtx.ssrcs.empty())
    return;

  // Configure RTX SSRCs.
  RTC_DCHECK_EQ(config_.rtp.rtx.ssrcs.size(), config_.rtp.ssrcs.size());
  for (size_t i = 0; i < config_.rtp.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.rtx.ssrcs[i];
    RtpRtcp* const rtp_rtcp = rtp_rtcp_modules_[i];
    rtp_rtcp->SetRtxSsrc(ssrc);
    RtpStateMap::iterator it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      rtp_rtcp->SetRtxState(it->second);
  }

  // Configure RTX payload types.
  RTC_DCHECK_GE(config_.rtp.rtx.payload_type, 0);
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->SetRtxSendPayloadType(config_.rtp.rtx.payload_type,
                                    config_.encoder_settings.payload_type);
    rtp_rtcp->SetRtxSendStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  }
  if (config_.rtp.fec.red_payload_type != -1 &&
      config_.rtp.fec.red_rtx_payload_type != -1) {
    for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
      rtp_rtcp->SetRtxSendPayloadType(config_.rtp.fec.red_rtx_payload_type,
                                      config_.rtp.fec.red_payload_type);
    }
  }
}

std::map<uint32_t, RtpState> VideoSendStream::GetRtpStates() const {
  std::map<uint32_t, RtpState> rtp_states;
  for (size_t i = 0; i < config_.rtp.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.ssrcs[i];
    RTC_DCHECK_EQ(ssrc, rtp_rtcp_modules_[i]->SSRC());
    rtp_states[ssrc] = rtp_rtcp_modules_[i]->GetRtpState();
  }

  for (size_t i = 0; i < config_.rtp.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = config_.rtp.rtx.ssrcs[i];
    rtp_states[ssrc] = rtp_rtcp_modules_[i]->GetRtxState();
  }

  return rtp_states;
}

void VideoSendStream::SignalNetworkState(NetworkState state) {
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->SetRTCPStatus(state == kNetworkUp ? config_.rtp.rtcp_mode
                                                : RtcpMode::kOff);
  }
}

int VideoSendStream::GetPaddingNeededBps() const {
  return vie_encoder_.GetPaddingNeededBps();
}

void VideoSendStream::OnBitrateUpdated(uint32_t bitrate_bps,
                                       uint8_t fraction_loss,
                                       int64_t rtt) {
  payload_router_.SetTargetSendBitrate(bitrate_bps);
  vie_encoder_.OnBitrateUpdated(bitrate_bps, fraction_loss, rtt);
}

int VideoSendStream::ProtectionRequest(const FecProtectionParams* delta_params,
                                       const FecProtectionParams* key_params,
                                       uint32_t* sent_video_rate_bps,
                                       uint32_t* sent_nack_rate_bps,
                                       uint32_t* sent_fec_rate_bps) {
  *sent_video_rate_bps = 0;
  *sent_nack_rate_bps = 0;
  *sent_fec_rate_bps = 0;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    uint32_t not_used = 0;
    uint32_t module_video_rate = 0;
    uint32_t module_fec_rate = 0;
    uint32_t module_nack_rate = 0;
    rtp_rtcp->SetFecParameters(delta_params, key_params);
    rtp_rtcp->BitrateSent(&not_used, &module_video_rate, &module_fec_rate,
                          &module_nack_rate);
    *sent_video_rate_bps += module_video_rate;
    *sent_nack_rate_bps += module_nack_rate;
    *sent_fec_rate_bps += module_fec_rate;
  }
  return 0;
}

}  // namespace internal
}  // namespace webrtc
