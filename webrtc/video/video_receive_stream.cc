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

#include <set>
#include <string>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/utility/ivf_file_writer.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/video/call_stats.h"
#include "webrtc/video/receive_statistics_proxy.h"
#include "webrtc/video/vie_remb.h"
#include "webrtc/video_receive_stream.h"

namespace webrtc {

static const bool kEnableFrameRecording = false;

static bool UseSendSideBwe(const VideoReceiveStream::Config& config) {
  if (!config.rtp.transport_cc)
    return false;
  for (const auto& extension : config.rtp.extensions) {
    if (extension.name == RtpExtension::kTransportSequenceNumber)
      return true;
  }
  return false;
}

std::string VideoReceiveStream::Decoder::ToString() const {
  std::stringstream ss;
  ss << "{decoder: " << (decoder ? "(VideoDecoder)" : "nullptr");
  ss << ", payload_type: " << payload_type;
  ss << ", payload_name: " << payload_name;
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
  ss << ", renderer: " << (renderer ? "(renderer)" : "nullptr");
  ss << ", render_delay_ms: " << render_delay_ms;
  if (!sync_group.empty())
    ss << ", sync_group: " << sync_group;
  ss << ", pre_decode_callback: "
     << (pre_decode_callback ? "(EncodedFrameObserver)" : "nullptr");
  ss << ", pre_render_callback: "
     << (pre_render_callback ? "(I420FrameCallback)" : "nullptr");
  ss << ", target_delay_ms: " << target_delay_ms;
  ss << '}';

  return ss.str();
}

std::string VideoReceiveStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{remote_ssrc: " << remote_ssrc;
  ss << ", local_ssrc: " << local_ssrc;
  ss << ", rtcp_mode: "
     << (rtcp_mode == RtcpMode::kCompound ? "RtcpMode::kCompound"
                                          : "RtcpMode::kReducedSize");
  ss << ", rtcp_xr: ";
  ss << "{receiver_reference_time_report: "
     << (rtcp_xr.receiver_reference_time_report ? "on" : "off");
  ss << '}';
  ss << ", remb: " << (remb ? "on" : "off");
  ss << ", transport_cc: " << (transport_cc ? "on" : "off");
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

namespace {
VideoCodec CreateDecoderVideoCodec(const VideoReceiveStream::Decoder& decoder) {
  VideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.plType = decoder.payload_type;
  strncpy(codec.plName, decoder.payload_name.c_str(), sizeof(codec.plName));
  if (decoder.payload_name == "VP8") {
    codec.codecType = kVideoCodecVP8;
  } else if (decoder.payload_name == "VP9") {
    codec.codecType = kVideoCodecVP9;
  } else if (decoder.payload_name == "H264") {
    codec.codecType = kVideoCodecH264;
  } else {
    codec.codecType = kVideoCodecGeneric;
  }

  if (codec.codecType == kVideoCodecVP8) {
    codec.codecSpecific.VP8 = VideoEncoder::GetDefaultVp8Settings();
  } else if (codec.codecType == kVideoCodecVP9) {
    codec.codecSpecific.VP9 = VideoEncoder::GetDefaultVp9Settings();
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

namespace internal {
VideoReceiveStream::VideoReceiveStream(
    int num_cpu_cores,
    CongestionController* congestion_controller,
    const VideoReceiveStream::Config& config,
    webrtc::VoiceEngine* voice_engine,
    ProcessThread* process_thread,
    CallStats* call_stats,
    VieRemb* remb)
    : transport_adapter_(config.rtcp_send_transport),
      encoded_frame_proxy_(config.pre_decode_callback),
      config_(config),
      process_thread_(process_thread),
      clock_(Clock::GetRealTimeClock()),
      decode_thread_(DecodeThreadFunction, this, "DecodingThread"),
      congestion_controller_(congestion_controller),
      call_stats_(call_stats),
      remb_(remb),
      vcm_(VideoCodingModule::Create(clock_,
                                     nullptr,
                                     nullptr,
                                     this,
                                     this,
                                     this)),
      incoming_video_stream_(0, config.disable_prerenderer_smoothing),
      stats_proxy_(config_, clock_),
      vie_channel_(&transport_adapter_,
                   process_thread,
                   vcm_.get(),
                   congestion_controller_->GetRemoteBitrateEstimator(
                       UseSendSideBwe(config_)),
                   call_stats_->rtcp_rtt_stats(),
                   congestion_controller_->pacer(),
                   congestion_controller_->packet_router()),
      vie_receiver_(vie_channel_.vie_receiver()),
      vie_sync_(vcm_.get()),
      rtp_rtcp_(vie_channel_.rtp_rtcp()) {
  LOG(LS_INFO) << "VideoReceiveStream: " << config_.ToString();

  RTC_DCHECK(process_thread_);
  RTC_DCHECK(congestion_controller_);
  RTC_DCHECK(call_stats_);
  RTC_DCHECK(remb_);
  RTC_CHECK(vie_channel_.Init() == 0);

  // Register the channel to receive stats updates.
  call_stats_->RegisterStatsObserver(vie_channel_.GetStatsObserver());

  // TODO(pbos): This is not fine grained enough...
  vie_channel_.SetProtectionMode(config_.rtp.nack.rtp_history_ms > 0, false, -1,
                                 -1);
  RTC_DCHECK(config_.rtp.rtcp_mode != RtcpMode::kOff)
      << "A stream should not be configured with RTCP disabled. This value is "
         "reserved for internal usage.";
  rtp_rtcp_->SetRTCPStatus(config_.rtp.rtcp_mode);

  RTC_DCHECK(config_.rtp.remote_ssrc != 0);
  // TODO(pbos): What's an appropriate local_ssrc for receive-only streams?
  RTC_DCHECK(config_.rtp.local_ssrc != 0);
  RTC_DCHECK(config_.rtp.remote_ssrc != config_.rtp.local_ssrc);
  rtp_rtcp_->SetSSRC(config_.rtp.local_ssrc);

  // TODO(pbos): Support multiple RTX, per video payload.
  for (const auto& kv : config_.rtp.rtx) {
    RTC_DCHECK(kv.second.ssrc != 0);
    RTC_DCHECK(kv.second.payload_type != 0);

    vie_receiver_->SetRtxSsrc(kv.second.ssrc);
    vie_receiver_->SetRtxPayloadType(kv.second.payload_type, kv.first);
  }
  // TODO(holmer): When Chrome no longer depends on this being false by default,
  // always use the mapping and remove this whole codepath.
  vie_receiver_->SetUseRtxPayloadMappingOnRestore(
      config_.rtp.use_rtx_payload_mapping_on_restore);

  if (config_.rtp.remb) {
    rtp_rtcp_->SetREMBStatus(true);
    remb_->AddReceiveChannel(rtp_rtcp_);
  }

  for (size_t i = 0; i < config_.rtp.extensions.size(); ++i) {
    const std::string& extension = config_.rtp.extensions[i].name;
    int id = config_.rtp.extensions[i].id;
    // One-byte-extension local identifiers are in the range 1-14 inclusive.
    RTC_DCHECK_GE(id, 1);
    RTC_DCHECK_LE(id, 14);
    vie_receiver_->EnableReceiveRtpHeaderExtension(extension, id);
  }

  if (config_.rtp.fec.ulpfec_payload_type != -1) {
    // ULPFEC without RED doesn't make sense.
    RTC_DCHECK(config_.rtp.fec.red_payload_type != -1);
    VideoCodec codec;
    memset(&codec, 0, sizeof(codec));
    codec.codecType = kVideoCodecULPFEC;
    strncpy(codec.plName, "ulpfec", sizeof(codec.plName));
    codec.plType = config_.rtp.fec.ulpfec_payload_type;
    RTC_CHECK(vie_receiver_->SetReceiveCodec(codec));
  }
  if (config_.rtp.fec.red_payload_type != -1) {
    VideoCodec codec;
    memset(&codec, 0, sizeof(codec));
    codec.codecType = kVideoCodecRED;
    strncpy(codec.plName, "red", sizeof(codec.plName));
    codec.plType = config_.rtp.fec.red_payload_type;
    RTC_CHECK(vie_receiver_->SetReceiveCodec(codec));
    if (config_.rtp.fec.red_rtx_payload_type != -1) {
      vie_receiver_->SetRtxPayloadType(config_.rtp.fec.red_rtx_payload_type,
                                       config_.rtp.fec.red_payload_type);
    }
  }

  if (config.rtp.rtcp_xr.receiver_reference_time_report)
    rtp_rtcp_->SetRtcpXrRrtrStatus(true);

  vie_channel_.RegisterReceiveStatisticsProxy(&stats_proxy_);
  vie_receiver_->GetReceiveStatistics()->RegisterRtpStatisticsCallback(
      &stats_proxy_);
  vie_receiver_->GetReceiveStatistics()->RegisterRtcpStatisticsCallback(
      &stats_proxy_);
  // Stats callback for CNAME changes.
  rtp_rtcp_->RegisterRtcpStatisticsCallback(&stats_proxy_);
  vie_channel_.RegisterRtcpPacketTypeCounterObserver(&stats_proxy_);

  RTC_DCHECK(!config_.decoders.empty());
  std::set<int> decoder_payload_types;
  for (const Decoder& decoder : config_.decoders) {
    RTC_CHECK(decoder.decoder);
    RTC_CHECK(decoder_payload_types.find(decoder.payload_type) ==
              decoder_payload_types.end())
        << "Duplicate payload type (" << decoder.payload_type
        << ") for different decoders.";
    decoder_payload_types.insert(decoder.payload_type);
    vcm_->RegisterExternalDecoder(decoder.decoder, decoder.payload_type);

    VideoCodec codec = CreateDecoderVideoCodec(decoder);

    RTC_CHECK(vie_receiver_->SetReceiveCodec(codec));
    RTC_CHECK_EQ(VCM_OK,
                 vcm_->RegisterReceiveCodec(&codec, num_cpu_cores, false));
  }

  vcm_->SetRenderDelay(config.render_delay_ms);
  incoming_video_stream_.SetExpectedRenderDelay(config.render_delay_ms);
  incoming_video_stream_.SetExternalCallback(this);
  vie_channel_.SetIncomingVideoStream(&incoming_video_stream_);
  vie_channel_.RegisterPreRenderCallback(this);

  process_thread_->RegisterModule(vcm_.get());
  process_thread_->RegisterModule(&vie_sync_);
}

VideoReceiveStream::~VideoReceiveStream() {
  LOG(LS_INFO) << "~VideoReceiveStream: " << config_.ToString();
  Stop();

  process_thread_->DeRegisterModule(&vie_sync_);
  process_thread_->DeRegisterModule(vcm_.get());

  // Deregister external decoders so that they are no longer running during
  // destruction. This effectively stops the VCM since the decoder thread is
  // stopped, the VCM is deregistered and no asynchronous decoder threads are
  // running.
  for (const Decoder& decoder : config_.decoders)
    vcm_->RegisterExternalDecoder(nullptr, decoder.payload_type);

  vie_channel_.RegisterPreRenderCallback(nullptr);

  call_stats_->DeregisterStatsObserver(vie_channel_.GetStatsObserver());
  rtp_rtcp_->SetREMBStatus(false);
  remb_->RemoveReceiveChannel(rtp_rtcp_);

  congestion_controller_->GetRemoteBitrateEstimator(UseSendSideBwe(config_))
      ->RemoveStream(vie_receiver_->GetRemoteSsrc());
}

void VideoReceiveStream::Start() {
  if (decode_thread_.IsRunning())
    return;
  transport_adapter_.Enable();
  incoming_video_stream_.Start();
  // Start the decode thread
  decode_thread_.Start();
  decode_thread_.SetPriority(rtc::kHighestPriority);
  vie_receiver_->StartReceive();
}

void VideoReceiveStream::Stop() {
  incoming_video_stream_.Stop();
  vie_receiver_->StopReceive();
  vcm_->TriggerDecoderShutdown();
  decode_thread_.Stop();
  transport_adapter_.Disable();
}

void VideoReceiveStream::SetSyncChannel(VoiceEngine* voice_engine,
                                        int audio_channel_id) {
  if (voice_engine && audio_channel_id != -1) {
    VoEVideoSync* voe_sync_interface = VoEVideoSync::GetInterface(voice_engine);
    vie_sync_.ConfigureSync(audio_channel_id, voe_sync_interface, rtp_rtcp_,
                            vie_receiver_->GetRtpReceiver());
    voe_sync_interface->Release();
    return;
  }
  vie_sync_.ConfigureSync(-1, nullptr, rtp_rtcp_,
                          vie_receiver_->GetRtpReceiver());
}

VideoReceiveStream::Stats VideoReceiveStream::GetStats() const {
  return stats_proxy_.GetStats();
}

bool VideoReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  return vie_receiver_->DeliverRtcp(packet, length);
}

bool VideoReceiveStream::DeliverRtp(const uint8_t* packet,
                                    size_t length,
                                    const PacketTime& packet_time) {
  return vie_receiver_->DeliverRtp(packet, length, packet_time);
}

void VideoReceiveStream::FrameCallback(VideoFrame* video_frame) {
  stats_proxy_.OnDecodedFrame();

  // Post processing is not supported if the frame is backed by a texture.
  if (!video_frame->video_frame_buffer()->native_handle()) {
    if (config_.pre_render_callback)
      config_.pre_render_callback->FrameCallback(video_frame);
  }
}

int VideoReceiveStream::RenderFrame(const uint32_t /*stream_id*/,
                                    const VideoFrame& video_frame) {
  int64_t sync_offset_ms;
  if (vie_sync_.GetStreamSyncOffsetInMs(video_frame, &sync_offset_ms))
    stats_proxy_.OnSyncOffsetUpdated(sync_offset_ms);

  if (config_.renderer)
    config_.renderer->OnFrame(video_frame);

  stats_proxy_.OnRenderedFrame(video_frame.width(), video_frame.height());

  return 0;
}

// TODO(asapersson): Consider moving callback from video_encoder.h or
// creating a different callback.
int32_t VideoReceiveStream::Encoded(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  stats_proxy_.OnPreDecode(encoded_image, codec_specific_info);
  if (config_.pre_decode_callback) {
    // TODO(asapersson): Remove EncodedFrameCallbackAdapter.
    encoded_frame_proxy_.Encoded(
        encoded_image, codec_specific_info, fragmentation);
  }
  if (kEnableFrameRecording) {
    if (!ivf_writer_.get()) {
      RTC_DCHECK(codec_specific_info);
      RtpVideoCodecTypes rtp_codec_type;
      switch (codec_specific_info->codecType) {
        case kVideoCodecVP8:
          rtp_codec_type = kRtpVideoVp8;
          break;
        case kVideoCodecVP9:
          rtp_codec_type = kRtpVideoVp9;
          break;
        case kVideoCodecH264:
          rtp_codec_type = kRtpVideoH264;
          break;
        default:
          rtp_codec_type = kRtpVideoNone;
          RTC_NOTREACHED() << "Unsupported codec "
                           << codec_specific_info->codecType;
      }
      std::ostringstream oss;
      oss << "receive_bitstream_ssrc_" << config_.rtp.remote_ssrc << ".ivf";
      ivf_writer_ = IvfFileWriter::Open(oss.str(), rtp_codec_type);
    }
    if (ivf_writer_.get()) {
      bool ok = ivf_writer_->WriteFrame(encoded_image);
      RTC_DCHECK(ok);
    }
  }

  return 0;
}

void VideoReceiveStream::SignalNetworkState(NetworkState state) {
  rtp_rtcp_->SetRTCPStatus(state == kNetworkUp ? config_.rtp.rtcp_mode
                                               : RtcpMode::kOff);
}

bool VideoReceiveStream::DecodeThreadFunction(void* ptr) {
  static_cast<VideoReceiveStream*>(ptr)->Decode();
  return true;
}

void VideoReceiveStream::Decode() {
  static const int kMaxDecodeWaitTimeMs = 50;
  vcm_->Decode(kMaxDecodeWaitTimeMs);
}

void VideoReceiveStream::SendNack(
    const std::vector<uint16_t>& sequence_numbers) {
  rtp_rtcp_->SendNack(sequence_numbers);
}

void VideoReceiveStream::RequestKeyFrame() {
  rtp_rtcp_->RequestKeyFrame();
}

}  // namespace internal
}  // namespace webrtc
