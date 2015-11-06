/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/audio/audio_receive_stream.h"

#include <string>

#include "webrtc/audio/audio_state.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_neteq_stats.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"

namespace webrtc {
std::string AudioReceiveStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{remote_ssrc: " << remote_ssrc;
  ss << ", local_ssrc: " << local_ssrc;
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1) {
      ss << ", ";
    }
  }
  ss << ']';
  ss << '}';
  return ss.str();
}

std::string AudioReceiveStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{rtp: " << rtp.ToString();
  ss << ", receive_transport: "
     << (receive_transport ? "(Transport)" : "nullptr");
  ss << ", rtcp_send_transport: "
     << (rtcp_send_transport ? "(Transport)" : "nullptr");
  ss << ", voe_channel_id: " << voe_channel_id;
  if (!sync_group.empty()) {
    ss << ", sync_group: " << sync_group;
  }
  ss << ", combined_audio_video_bwe: "
     << (combined_audio_video_bwe ? "true" : "false");
  ss << '}';
  return ss.str();
}

namespace internal {
AudioReceiveStream::AudioReceiveStream(
    RemoteBitrateEstimator* remote_bitrate_estimator,
    const webrtc::AudioReceiveStream::Config& config,
    const rtc::scoped_refptr<webrtc::AudioState>& audio_state)
    : remote_bitrate_estimator_(remote_bitrate_estimator),
      config_(config),
      audio_state_(audio_state),
      rtp_header_parser_(RtpHeaderParser::Create()) {
  LOG(LS_INFO) << "AudioReceiveStream: " << config_.ToString();
  RTC_DCHECK_NE(config_.voe_channel_id, -1);
  RTC_DCHECK(remote_bitrate_estimator_);
  RTC_DCHECK(audio_state_.get());
  RTC_DCHECK(rtp_header_parser_);
  for (const auto& ext : config.rtp.extensions) {
    // One-byte-extension local identifiers are in the range 1-14 inclusive.
    RTC_DCHECK_GE(ext.id, 1);
    RTC_DCHECK_LE(ext.id, 14);
    if (ext.name == RtpExtension::kAudioLevel) {
      RTC_CHECK(rtp_header_parser_->RegisterRtpHeaderExtension(
          kRtpExtensionAudioLevel, ext.id));
    } else if (ext.name == RtpExtension::kAbsSendTime) {
      RTC_CHECK(rtp_header_parser_->RegisterRtpHeaderExtension(
          kRtpExtensionAbsoluteSendTime, ext.id));
    } else if (ext.name == RtpExtension::kTransportSequenceNumber) {
      RTC_CHECK(rtp_header_parser_->RegisterRtpHeaderExtension(
          kRtpExtensionTransportSequenceNumber, ext.id));
    } else {
      RTC_NOTREACHED() << "Unsupported RTP extension.";
    }
  }
}

AudioReceiveStream::~AudioReceiveStream() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "~AudioReceiveStream: " << config_.ToString();
}

webrtc::AudioReceiveStream::Stats AudioReceiveStream::GetStats() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  webrtc::AudioReceiveStream::Stats stats;
  stats.remote_ssrc = config_.rtp.remote_ssrc;
  internal::AudioState* audio_state =
      static_cast<internal::AudioState*>(audio_state_.get());
  VoiceEngine* voice_engine = audio_state->voice_engine();
  ScopedVoEInterface<VoECodec> codec(voice_engine);
  ScopedVoEInterface<VoENetEqStats> neteq(voice_engine);
  ScopedVoEInterface<VoERTP_RTCP> rtp(voice_engine);
  ScopedVoEInterface<VoEVideoSync> sync(voice_engine);
  ScopedVoEInterface<VoEVolumeControl> volume(voice_engine);
  unsigned int ssrc = 0;
  webrtc::CallStatistics call_stats = {0};
  webrtc::CodecInst codec_inst = {0};
  // Only collect stats if we have seen some traffic with the SSRC.
  if (rtp->GetRemoteSSRC(config_.voe_channel_id, ssrc) == -1 ||
      rtp->GetRTCPStatistics(config_.voe_channel_id, call_stats) == -1 ||
      codec->GetRecCodec(config_.voe_channel_id, codec_inst) == -1) {
    return stats;
  }

  stats.bytes_rcvd = call_stats.bytesReceived;
  stats.packets_rcvd = call_stats.packetsReceived;
  stats.packets_lost = call_stats.cumulativeLost;
  stats.fraction_lost = Q8ToFloat(call_stats.fractionLost);
  if (codec_inst.pltype != -1) {
    stats.codec_name = codec_inst.plname;
  }
  stats.ext_seqnum = call_stats.extendedMax;
  if (codec_inst.plfreq / 1000 > 0) {
    stats.jitter_ms = call_stats.jitterSamples / (codec_inst.plfreq / 1000);
  }
  {
    int jitter_buffer_delay_ms = 0;
    int playout_buffer_delay_ms = 0;
    sync->GetDelayEstimate(config_.voe_channel_id, &jitter_buffer_delay_ms,
                           &playout_buffer_delay_ms);
    stats.delay_estimate_ms = jitter_buffer_delay_ms + playout_buffer_delay_ms;
  }
  {
    unsigned int level = 0;
    if (volume->GetSpeechOutputLevelFullRange(config_.voe_channel_id, level) !=
        -1) {
      stats.audio_level = static_cast<int32_t>(level);
    }
  }

  webrtc::NetworkStatistics ns = {0};
  if (neteq->GetNetworkStatistics(config_.voe_channel_id, ns) != -1) {
    // Get jitter buffer and total delay (alg + jitter + playout) stats.
    stats.jitter_buffer_ms = ns.currentBufferSize;
    stats.jitter_buffer_preferred_ms = ns.preferredBufferSize;
    stats.expand_rate = Q14ToFloat(ns.currentExpandRate);
    stats.speech_expand_rate = Q14ToFloat(ns.currentSpeechExpandRate);
    stats.secondary_decoded_rate = Q14ToFloat(ns.currentSecondaryDecodedRate);
    stats.accelerate_rate = Q14ToFloat(ns.currentAccelerateRate);
    stats.preemptive_expand_rate = Q14ToFloat(ns.currentPreemptiveRate);
  }

  webrtc::AudioDecodingCallStats ds;
  if (neteq->GetDecodingCallStatistics(config_.voe_channel_id, &ds) != -1) {
    stats.decoding_calls_to_silence_generator = ds.calls_to_silence_generator;
    stats.decoding_calls_to_neteq = ds.calls_to_neteq;
    stats.decoding_normal = ds.decoded_normal;
    stats.decoding_plc = ds.decoded_plc;
    stats.decoding_cng = ds.decoded_cng;
    stats.decoding_plc_cng = ds.decoded_plc_cng;
  }

  stats.capture_start_ntp_time_ms = call_stats.capture_start_ntp_time_ms_;

  return stats;
}

const webrtc::AudioReceiveStream::Config& AudioReceiveStream::config() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return config_;
}

void AudioReceiveStream::Start() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

void AudioReceiveStream::Stop() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

void AudioReceiveStream::SignalNetworkState(NetworkState state) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
}

bool AudioReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  // TODO(solenberg): Tests call this function on a network thread, libjingle
  // calls on the worker thread. We should move towards always using a network
  // thread. Then this check can be enabled.
  // RTC_DCHECK(!thread_checker_.CalledOnValidThread());
  return false;
}

bool AudioReceiveStream::DeliverRtp(const uint8_t* packet,
                                    size_t length,
                                    const PacketTime& packet_time) {
  // TODO(solenberg): Tests call this function on a network thread, libjingle
  // calls on the worker thread. We should move towards always using a network
  // thread. Then this check can be enabled.
  // RTC_DCHECK(!thread_checker_.CalledOnValidThread());
  RTPHeader header;

  if (!rtp_header_parser_->Parse(packet, length, &header)) {
    return false;
  }

  // Only forward if the parsed header has absolute sender time. RTP timestamps
  // may have different rates for audio and video and shouldn't be mixed.
  if (config_.combined_audio_video_bwe &&
      header.extension.hasAbsoluteSendTime) {
    int64_t arrival_time_ms = TickTime::MillisecondTimestamp();
    if (packet_time.timestamp >= 0)
      arrival_time_ms = (packet_time.timestamp + 500) / 1000;
    size_t payload_size = length - header.headerLength;
    remote_bitrate_estimator_->IncomingPacket(arrival_time_ms, payload_size,
                                              header, false);
  }
  return true;
}
}  // namespace internal
}  // namespace webrtc
