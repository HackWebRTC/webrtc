/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_new_format.h"

#include <vector>

#include "api/array_view.h"
#include "logging/rtc_event_log/events/rtc_event_alr_state.h"
#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair_config.h"
#include "logging/rtc_event_log/events/rtc_event_probe_cluster_created.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_success.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_video_send_stream_config.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/app.h"
#include "modules/rtp_rtcp/source/rtcp_packet/bye.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_jitter_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/extended_reports.h"
#include "modules/rtp_rtcp/source/rtcp_packet/psfb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "rtc_base/checks.h"
#include "rtc_base/ignore_wundef.h"
#include "rtc_base/logging.h"

// *.pb.h files are generated at build-time by the protobuf compiler.
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log2.pb.h"
#else
#include "logging/rtc_event_log/rtc_event_log2.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()

namespace webrtc {

namespace {
rtclog2::DelayBasedBweUpdates::DetectorState ConvertToProtoFormat(
    BandwidthUsage state) {
  switch (state) {
    case BandwidthUsage::kBwNormal:
      return rtclog2::DelayBasedBweUpdates::BWE_NORMAL;
    case BandwidthUsage::kBwUnderusing:
      return rtclog2::DelayBasedBweUpdates::BWE_UNDERUSING;
    case BandwidthUsage::kBwOverusing:
      return rtclog2::DelayBasedBweUpdates::BWE_OVERUSING;
    case BandwidthUsage::kLast:
      RTC_NOTREACHED();
  }
  RTC_NOTREACHED();
  return rtclog2::DelayBasedBweUpdates::BWE_UNKNOWN_STATE;
}

rtclog2::BweProbeResultFailure::FailureReason ConvertToProtoFormat(
    ProbeFailureReason failure_reason) {
  switch (failure_reason) {
    case ProbeFailureReason::kInvalidSendReceiveInterval:
      return rtclog2::BweProbeResultFailure::INVALID_SEND_RECEIVE_INTERVAL;
    case ProbeFailureReason::kInvalidSendReceiveRatio:
      return rtclog2::BweProbeResultFailure::INVALID_SEND_RECEIVE_RATIO;
    case ProbeFailureReason::kTimeout:
      return rtclog2::BweProbeResultFailure::TIMEOUT;
    case ProbeFailureReason::kLast:
      RTC_NOTREACHED();
  }
  RTC_NOTREACHED();
  return rtclog2::BweProbeResultFailure::UNKNOWN;
}

// Returns true if there are recognized extensions that we should log
// and false if there are no extensions or all extensions are types we don't
// log. The protobuf representation of the header configs is written to
// |proto_config|.
bool ConvertToProtoFormat(const std::vector<RtpExtension>& extensions,
                          rtclog2::RtpHeaderExtensionConfig* proto_config) {
  size_t unknown_extensions = 0;
  for (auto& extension : extensions) {
    if (extension.uri == RtpExtension::kAudioLevelUri) {
      proto_config->set_audio_level_id(extension.id);
    } else if (extension.uri == RtpExtension::kTimestampOffsetUri) {
      proto_config->set_transmission_time_offset_id(extension.id);
    } else if (extension.uri == RtpExtension::kAbsSendTimeUri) {
      proto_config->set_absolute_send_time_id(extension.id);
    } else if (extension.uri == RtpExtension::kTransportSequenceNumberUri) {
      proto_config->set_transport_sequence_number_id(extension.id);
    } else if (extension.uri == RtpExtension::kVideoRotationUri) {
      proto_config->set_video_rotation_id(extension.id);
    } else {
      ++unknown_extensions;
    }
  }
  return unknown_extensions < extensions.size();
}

rtclog2::IceCandidatePairConfig::IceCandidatePairConfigType
ConvertToProtoFormat(IceCandidatePairConfigType type) {
  switch (type) {
    case IceCandidatePairConfigType::kAdded:
      return rtclog2::IceCandidatePairConfig::ADDED;
    case IceCandidatePairConfigType::kUpdated:
      return rtclog2::IceCandidatePairConfig::UPDATED;
    case IceCandidatePairConfigType::kDestroyed:
      return rtclog2::IceCandidatePairConfig::DESTROYED;
    case IceCandidatePairConfigType::kSelected:
      return rtclog2::IceCandidatePairConfig::SELECTED;
    case IceCandidatePairConfigType::kNumValues:
      RTC_NOTREACHED();
  }
  RTC_NOTREACHED();
  return rtclog2::IceCandidatePairConfig::UNKNOWN_CONFIG_TYPE;
}

rtclog2::IceCandidatePairConfig::IceCandidateType ConvertToProtoFormat(
    IceCandidateType type) {
  switch (type) {
    case IceCandidateType::kUnknown:
      return rtclog2::IceCandidatePairConfig::UNKNOWN_CANDIDATE_TYPE;
    case IceCandidateType::kLocal:
      return rtclog2::IceCandidatePairConfig::LOCAL;
    case IceCandidateType::kStun:
      return rtclog2::IceCandidatePairConfig::STUN;
    case IceCandidateType::kPrflx:
      return rtclog2::IceCandidatePairConfig::PRFLX;
    case IceCandidateType::kRelay:
      return rtclog2::IceCandidatePairConfig::RELAY;
    case IceCandidateType::kNumValues:
      RTC_NOTREACHED();
  }
  RTC_NOTREACHED();
  return rtclog2::IceCandidatePairConfig::UNKNOWN_CANDIDATE_TYPE;
}

rtclog2::IceCandidatePairConfig::Protocol ConvertToProtoFormat(
    IceCandidatePairProtocol protocol) {
  switch (protocol) {
    case IceCandidatePairProtocol::kUnknown:
      return rtclog2::IceCandidatePairConfig::UNKNOWN_PROTOCOL;
    case IceCandidatePairProtocol::kUdp:
      return rtclog2::IceCandidatePairConfig::UDP;
    case IceCandidatePairProtocol::kTcp:
      return rtclog2::IceCandidatePairConfig::TCP;
    case IceCandidatePairProtocol::kSsltcp:
      return rtclog2::IceCandidatePairConfig::SSLTCP;
    case IceCandidatePairProtocol::kTls:
      return rtclog2::IceCandidatePairConfig::TLS;
    case IceCandidatePairProtocol::kNumValues:
      RTC_NOTREACHED();
  }
  RTC_NOTREACHED();
  return rtclog2::IceCandidatePairConfig::UNKNOWN_PROTOCOL;
}

rtclog2::IceCandidatePairConfig::AddressFamily ConvertToProtoFormat(
    IceCandidatePairAddressFamily address_family) {
  switch (address_family) {
    case IceCandidatePairAddressFamily::kUnknown:
      return rtclog2::IceCandidatePairConfig::UNKNOWN_ADDRESS_FAMILY;
    case IceCandidatePairAddressFamily::kIpv4:
      return rtclog2::IceCandidatePairConfig::IPV4;
    case IceCandidatePairAddressFamily::kIpv6:
      return rtclog2::IceCandidatePairConfig::IPV6;
    case IceCandidatePairAddressFamily::kNumValues:
      RTC_NOTREACHED();
  }
  RTC_NOTREACHED();
  return rtclog2::IceCandidatePairConfig::UNKNOWN_ADDRESS_FAMILY;
}

rtclog2::IceCandidatePairConfig::NetworkType ConvertToProtoFormat(
    IceCandidateNetworkType network_type) {
  switch (network_type) {
    case IceCandidateNetworkType::kUnknown:
      return rtclog2::IceCandidatePairConfig::UNKNOWN_NETWORK_TYPE;
    case IceCandidateNetworkType::kEthernet:
      return rtclog2::IceCandidatePairConfig::ETHERNET;
    case IceCandidateNetworkType::kLoopback:
      return rtclog2::IceCandidatePairConfig::LOOPBACK;
    case IceCandidateNetworkType::kWifi:
      return rtclog2::IceCandidatePairConfig::WIFI;
    case IceCandidateNetworkType::kVpn:
      return rtclog2::IceCandidatePairConfig::VPN;
    case IceCandidateNetworkType::kCellular:
      return rtclog2::IceCandidatePairConfig::CELLULAR;
    case IceCandidateNetworkType::kNumValues:
      RTC_NOTREACHED();
  }
  RTC_NOTREACHED();
  return rtclog2::IceCandidatePairConfig::UNKNOWN_NETWORK_TYPE;
}

rtclog2::IceCandidatePairEvent::IceCandidatePairEventType ConvertToProtoFormat(
    IceCandidatePairEventType type) {
  switch (type) {
    case IceCandidatePairEventType::kCheckSent:
      return rtclog2::IceCandidatePairEvent::CHECK_SENT;
    case IceCandidatePairEventType::kCheckReceived:
      return rtclog2::IceCandidatePairEvent::CHECK_RECEIVED;
    case IceCandidatePairEventType::kCheckResponseSent:
      return rtclog2::IceCandidatePairEvent::CHECK_RESPONSE_SENT;
    case IceCandidatePairEventType::kCheckResponseReceived:
      return rtclog2::IceCandidatePairEvent::CHECK_RESPONSE_RECEIVED;
    case IceCandidatePairEventType::kNumValues:
      RTC_NOTREACHED();
  }
  RTC_NOTREACHED();
  return rtclog2::IceCandidatePairEvent::UNKNOWN_CHECK_TYPE;
}

// Copies all RTCP blocks except APP, SDES and unknown from |packet| to
// |buffer|. |buffer| must have space for |IP_PACKET_SIZE| bytes. |packet| must
// be at most |IP_PACKET_SIZE| bytes long.
size_t RemoveNonWhitelistedRtcpBlocks(const rtc::Buffer& packet,
                                      uint8_t* buffer) {
  RTC_DCHECK(packet.size() <= IP_PACKET_SIZE);
  RTC_DCHECK(buffer != nullptr);
  rtcp::CommonHeader header;
  const uint8_t* block_begin = packet.data();
  const uint8_t* packet_end = packet.data() + packet.size();
  size_t buffer_length = 0;
  while (block_begin < packet_end) {
    if (!header.Parse(block_begin, packet_end - block_begin)) {
      break;  // Incorrect message header.
    }
    const uint8_t* next_block = header.NextPacket();
    RTC_DCHECK_GT(next_block, block_begin);
    RTC_DCHECK_LE(next_block, packet_end);
    size_t block_size = next_block - block_begin;
    switch (header.type()) {
      case rtcp::Bye::kPacketType:
      case rtcp::ExtendedJitterReport::kPacketType:
      case rtcp::ExtendedReports::kPacketType:
      case rtcp::Psfb::kPacketType:
      case rtcp::ReceiverReport::kPacketType:
      case rtcp::Rtpfb::kPacketType:
      case rtcp::SenderReport::kPacketType:
        // We log sender reports, receiver reports, bye messages
        // inter-arrival jitter, third-party loss reports, payload-specific
        // feedback and extended reports.
        // TODO(terelius): As an optimization, don't copy anything if all blocks
        // in the packet are whitelisted types.
        memcpy(buffer + buffer_length, block_begin, block_size);
        buffer_length += block_size;
        break;
      case rtcp::App::kPacketType:
      case rtcp::Sdes::kPacketType:
      default:
        // We don't log sender descriptions, application defined messages
        // or message blocks of unknown type.
        break;
    }

    block_begin += block_size;
  }
  return buffer_length;
}
}  // namespace

std::string RtcEventLogEncoderNewFormat::EncodeLogStart(int64_t timestamp_us) {
  rtclog2::EventStream event_stream;
  rtclog2::BeginLogEvent* proto_batch = event_stream.add_begin_log_events();
  proto_batch->set_timestamp_ms(timestamp_us / 1000);
  return event_stream.SerializeAsString();
}

std::string RtcEventLogEncoderNewFormat::EncodeLogEnd(int64_t timestamp_us) {
  rtclog2::EventStream event_stream;
  rtclog2::EndLogEvent* proto_batch = event_stream.add_end_log_events();
  proto_batch->set_timestamp_ms(timestamp_us / 1000);
  return event_stream.SerializeAsString();
}

std::string RtcEventLogEncoderNewFormat::EncodeBatch(
    std::deque<std::unique_ptr<RtcEvent>>::const_iterator begin,
    std::deque<std::unique_ptr<RtcEvent>>::const_iterator end) {
  rtclog2::EventStream event_stream;
  std::string encoded_output;

  {
    std::vector<const RtcEventAlrState*> alr_state_events;
    std::vector<const RtcEventAudioNetworkAdaptation*>
        audio_network_adaptation_events;
    std::vector<const RtcEventAudioPlayout*> audio_playout_events;
    std::vector<const RtcEventAudioReceiveStreamConfig*>
        audio_recv_stream_configs;
    std::vector<const RtcEventAudioSendStreamConfig*> audio_send_stream_configs;
    std::vector<const RtcEventBweUpdateDelayBased*> bwe_delay_based_updates;
    std::vector<const RtcEventBweUpdateLossBased*> bwe_loss_based_updates;
    std::vector<const RtcEventProbeClusterCreated*>
        probe_cluster_created_events;
    std::vector<const RtcEventProbeResultFailure*> probe_result_failure_events;
    std::vector<const RtcEventProbeResultSuccess*> probe_result_success_events;
    std::vector<const RtcEventRtcpPacketIncoming*> incoming_rtcp_packets;
    std::vector<const RtcEventRtcpPacketOutgoing*> outgoing_rtcp_packets;
    std::vector<const RtcEventRtpPacketIncoming*> incoming_rtp_packets;
    std::vector<const RtcEventRtpPacketOutgoing*> outgoing_rtp_packets;
    std::vector<const RtcEventVideoReceiveStreamConfig*>
        video_recv_stream_configs;
    std::vector<const RtcEventVideoSendStreamConfig*> video_send_stream_configs;
    std::vector<const RtcEventIceCandidatePairConfig*> ice_candidate_configs;
    std::vector<const RtcEventIceCandidatePair*> ice_candidate_events;

    for (auto it = begin; it != end; ++it) {
      switch ((*it)->GetType()) {
        case RtcEvent::Type::AlrStateEvent: {
          auto* rtc_event =
              static_cast<const RtcEventAlrState* const>(it->get());
          alr_state_events.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::AudioNetworkAdaptation: {
          auto* rtc_event =
              static_cast<const RtcEventAudioNetworkAdaptation* const>(
                  it->get());
          audio_network_adaptation_events.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::AudioPlayout: {
          auto* rtc_event =
              static_cast<const RtcEventAudioPlayout* const>(it->get());
          audio_playout_events.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::AudioReceiveStreamConfig: {
          auto* rtc_event =
              static_cast<const RtcEventAudioReceiveStreamConfig* const>(
                  it->get());
          audio_recv_stream_configs.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::AudioSendStreamConfig: {
          auto* rtc_event =
              static_cast<const RtcEventAudioSendStreamConfig* const>(
                  it->get());
          audio_send_stream_configs.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::BweUpdateDelayBased: {
          auto* rtc_event =
              static_cast<const RtcEventBweUpdateDelayBased* const>(it->get());
          bwe_delay_based_updates.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::BweUpdateLossBased: {
          auto* rtc_event =
              static_cast<const RtcEventBweUpdateLossBased* const>(it->get());
          bwe_loss_based_updates.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::ProbeClusterCreated: {
          auto* rtc_event =
              static_cast<const RtcEventProbeClusterCreated* const>(it->get());
          probe_cluster_created_events.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::ProbeResultFailure: {
          auto* rtc_event =
              static_cast<const RtcEventProbeResultFailure* const>(it->get());
          probe_result_failure_events.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::ProbeResultSuccess: {
          auto* rtc_event =
              static_cast<const RtcEventProbeResultSuccess* const>(it->get());
          probe_result_success_events.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::RtcpPacketIncoming: {
          auto* rtc_event =
              static_cast<const RtcEventRtcpPacketIncoming* const>(it->get());
          incoming_rtcp_packets.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::RtcpPacketOutgoing: {
          auto* rtc_event =
              static_cast<const RtcEventRtcpPacketOutgoing* const>(it->get());
          outgoing_rtcp_packets.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::RtpPacketIncoming: {
          auto* rtc_event =
              static_cast<const RtcEventRtpPacketIncoming* const>(it->get());
          incoming_rtp_packets.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::RtpPacketOutgoing: {
          auto* rtc_event =
              static_cast<const RtcEventRtpPacketOutgoing* const>(it->get());
          outgoing_rtp_packets.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::VideoReceiveStreamConfig: {
          auto* rtc_event =
              static_cast<const RtcEventVideoReceiveStreamConfig* const>(
                  it->get());
          video_recv_stream_configs.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::VideoSendStreamConfig: {
          auto* rtc_event =
              static_cast<const RtcEventVideoSendStreamConfig* const>(
                  it->get());
          video_send_stream_configs.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::IceCandidatePairConfig: {
          auto* rtc_event =
              static_cast<const RtcEventIceCandidatePairConfig* const>(
                  it->get());
          ice_candidate_configs.push_back(rtc_event);
          break;
        }
        case RtcEvent::Type::IceCandidatePairEvent: {
          auto* rtc_event =
              static_cast<const RtcEventIceCandidatePair* const>(it->get());
          ice_candidate_events.push_back(rtc_event);
          break;
        }
      }
    }

    EncodeAlrState(alr_state_events, &event_stream);
    EncodeAudioNetworkAdaptation(audio_network_adaptation_events,
                                 &event_stream);
    EncodeAudioPlayout(audio_playout_events, &event_stream);
    EncodeAudioRecvStreamConfig(audio_recv_stream_configs, &event_stream);
    EncodeAudioSendStreamConfig(audio_send_stream_configs, &event_stream);
    EncodeBweUpdateDelayBased(bwe_delay_based_updates, &event_stream);
    EncodeBweUpdateLossBased(bwe_loss_based_updates, &event_stream);
    EncodeProbeClusterCreated(probe_cluster_created_events, &event_stream);
    EncodeProbeResultFailure(probe_result_failure_events, &event_stream);
    EncodeProbeResultSuccess(probe_result_success_events, &event_stream);
    EncodeRtcpPacketIncoming(incoming_rtcp_packets, &event_stream);
    EncodeRtcpPacketOutgoing(outgoing_rtcp_packets, &event_stream);
    EncodeRtpPacketIncoming(incoming_rtp_packets, &event_stream);
    EncodeRtpPacketOutgoing(outgoing_rtp_packets, &event_stream);
    EncodeVideoRecvStreamConfig(video_recv_stream_configs, &event_stream);
    EncodeVideoSendStreamConfig(video_send_stream_configs, &event_stream);
    EncodeIceCandidatePairConfig(ice_candidate_configs, &event_stream);
    EncodeIceCandidatePairEvent(ice_candidate_events, &event_stream);
  }  // Deallocate the temporary vectors.

  return event_stream.SerializeAsString();
}

void RtcEventLogEncoderNewFormat::EncodeAlrState(
    rtc::ArrayView<const RtcEventAlrState*> batch,
    rtclog2::EventStream* event_stream) {
  for (const RtcEventAlrState* base_event : batch) {
    rtclog2::AlrState* proto_batch = event_stream->add_alr_states();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_in_alr(base_event->in_alr_);
  }
  // TODO(terelius): Should we delta-compress this event type?
}

void RtcEventLogEncoderNewFormat::EncodeAudioNetworkAdaptation(
    rtc::ArrayView<const RtcEventAudioNetworkAdaptation*> batch,
    rtclog2::EventStream* event_stream) {
  if (batch.size() == 0)
    return;
  for (const RtcEventAudioNetworkAdaptation* base_event : batch) {
    rtclog2::AudioNetworkAdaptations* proto_batch =
        event_stream->add_audio_network_adaptations();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    if (base_event->config_->bitrate_bps.has_value())
      proto_batch->set_bitrate_bps(base_event->config_->bitrate_bps.value());
    if (base_event->config_->frame_length_ms.has_value()) {
      proto_batch->set_frame_length_ms(
          base_event->config_->frame_length_ms.value());
    }
    if (base_event->config_->uplink_packet_loss_fraction.has_value()) {
      proto_batch->set_uplink_packet_loss_fraction(
          base_event->config_->uplink_packet_loss_fraction.value());
    }
    if (base_event->config_->enable_fec.has_value())
      proto_batch->set_enable_fec(base_event->config_->enable_fec.value());
    if (base_event->config_->enable_dtx.has_value())
      proto_batch->set_enable_dtx(base_event->config_->enable_dtx.value());
    if (base_event->config_->num_channels.has_value())
      proto_batch->set_num_channels(base_event->config_->num_channels.value());
  }
  // TODO(terelius): Delta-compress rest of batch.
}

void RtcEventLogEncoderNewFormat::EncodeAudioPlayout(
    rtc::ArrayView<const RtcEventAudioPlayout*> batch,
    rtclog2::EventStream* event_stream) {
  if (batch.size() == 0)
    return;
  for (const RtcEventAudioPlayout* base_event : batch) {
    rtclog2::AudioPlayoutEvents* proto_batch =
        event_stream->add_audio_playout_events();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_local_ssrc(base_event->ssrc_);
  }
  // TODO(terelius): Delta-compress rest of batch.
}

void RtcEventLogEncoderNewFormat::EncodeAudioRecvStreamConfig(
    rtc::ArrayView<const RtcEventAudioReceiveStreamConfig*> batch,
    rtclog2::EventStream* event_stream) {
  for (const RtcEventAudioReceiveStreamConfig* base_event : batch) {
    rtclog2::AudioRecvStreamConfig* proto_batch =
        event_stream->add_audio_recv_stream_configs();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_remote_ssrc(base_event->config_->remote_ssrc);
    proto_batch->set_local_ssrc(base_event->config_->local_ssrc);
    if (!base_event->config_->rsid.empty())
      proto_batch->set_rsid(base_event->config_->rsid);

    rtclog2::RtpHeaderExtensionConfig* proto_config =
        proto_batch->mutable_header_extensions();
    bool has_recognized_extensions =
        ConvertToProtoFormat(base_event->config_->rtp_extensions, proto_config);
    if (!has_recognized_extensions)
      proto_batch->clear_header_extensions();
  }
}

void RtcEventLogEncoderNewFormat::EncodeAudioSendStreamConfig(
    rtc::ArrayView<const RtcEventAudioSendStreamConfig*> batch,
    rtclog2::EventStream* event_stream) {
  for (const RtcEventAudioSendStreamConfig* base_event : batch) {
    rtclog2::AudioSendStreamConfig* proto_batch =
        event_stream->add_audio_send_stream_configs();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_ssrc(base_event->config_->local_ssrc);
    if (!base_event->config_->rsid.empty())
      proto_batch->set_rsid(base_event->config_->rsid);

    rtclog2::RtpHeaderExtensionConfig* proto_config =
        proto_batch->mutable_header_extensions();
    bool has_recognized_extensions =
        ConvertToProtoFormat(base_event->config_->rtp_extensions, proto_config);
    if (!has_recognized_extensions)
      proto_batch->clear_header_extensions();
  }
}

void RtcEventLogEncoderNewFormat::EncodeBweUpdateDelayBased(
    rtc::ArrayView<const RtcEventBweUpdateDelayBased*> batch,
    rtclog2::EventStream* event_stream) {
  if (batch.size() == 0)
    return;
  for (const RtcEventBweUpdateDelayBased* base_event : batch) {
    rtclog2::DelayBasedBweUpdates* proto_batch =
        event_stream->add_delay_based_bwe_updates();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_bitrate_bps(base_event->bitrate_bps_);
    proto_batch->set_detector_state(
        ConvertToProtoFormat(base_event->detector_state_));
  }
  // TODO(terelius): Delta-compress rest of batch.
}

void RtcEventLogEncoderNewFormat::EncodeBweUpdateLossBased(
    rtc::ArrayView<const RtcEventBweUpdateLossBased*> batch,
    rtclog2::EventStream* event_stream) {
  if (batch.size() == 0)
    return;
  for (const RtcEventBweUpdateLossBased* base_event : batch) {
    rtclog2::LossBasedBweUpdates* proto_batch =
        event_stream->add_loss_based_bwe_updates();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_bitrate_bps(base_event->bitrate_bps_);
    proto_batch->set_fraction_loss(base_event->fraction_loss_);
    proto_batch->set_total_packets(base_event->total_packets_);
  }
  // TODO(terelius): Delta-compress rest of batch.
}

void RtcEventLogEncoderNewFormat::EncodeProbeClusterCreated(
    rtc::ArrayView<const RtcEventProbeClusterCreated*> batch,
    rtclog2::EventStream* event_stream) {
  for (const RtcEventProbeClusterCreated* base_event : batch) {
    rtclog2::BweProbeCluster* proto_batch = event_stream->add_probe_clusters();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_id(base_event->id_);
    proto_batch->set_bitrate_bps(base_event->bitrate_bps_);
    proto_batch->set_min_packets(base_event->min_probes_);
    proto_batch->set_min_bytes(base_event->min_bytes_);
  }
}

void RtcEventLogEncoderNewFormat::EncodeProbeResultFailure(
    rtc::ArrayView<const RtcEventProbeResultFailure*> batch,
    rtclog2::EventStream* event_stream) {
  for (const RtcEventProbeResultFailure* base_event : batch) {
    rtclog2::BweProbeResultFailure* proto_batch =
        event_stream->add_probe_failure();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_id(base_event->id_);
    proto_batch->set_failure(ConvertToProtoFormat(base_event->failure_reason_));
  }
  // TODO(terelius): Should we delta-compress this event type?
}

void RtcEventLogEncoderNewFormat::EncodeProbeResultSuccess(
    rtc::ArrayView<const RtcEventProbeResultSuccess*> batch,
    rtclog2::EventStream* event_stream) {
  for (const RtcEventProbeResultSuccess* base_event : batch) {
    rtclog2::BweProbeResultSuccess* proto_batch =
        event_stream->add_probe_success();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_id(base_event->id_);
    proto_batch->set_bitrate_bps(base_event->bitrate_bps_);
  }
  // TODO(terelius): Should we delta-compress this event type?
}

void RtcEventLogEncoderNewFormat::EncodeRtcpPacketIncoming(
    rtc::ArrayView<const RtcEventRtcpPacketIncoming*> batch,
    rtclog2::EventStream* event_stream) {
  if (batch.size() == 0)
    return;
  for (const RtcEventRtcpPacketIncoming* base_event : batch) {
    rtclog2::IncomingRtcpPackets* proto_batch =
        event_stream->add_incoming_rtcp_packets();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);

    uint8_t buffer[IP_PACKET_SIZE];
    size_t buffer_length =
        RemoveNonWhitelistedRtcpBlocks(base_event->packet_, buffer);
    proto_batch->set_raw_packet(buffer, buffer_length);
  }
  // TODO(terelius): Delta-compress rest of batch.
}

void RtcEventLogEncoderNewFormat::EncodeRtcpPacketOutgoing(
    rtc::ArrayView<const RtcEventRtcpPacketOutgoing*> batch,
    rtclog2::EventStream* event_stream) {
  if (batch.size() == 0)
    return;
  for (const RtcEventRtcpPacketOutgoing* base_event : batch) {
    rtclog2::OutgoingRtcpPackets* proto_batch =
        event_stream->add_outgoing_rtcp_packets();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);

    uint8_t buffer[IP_PACKET_SIZE];
    size_t buffer_length =
        RemoveNonWhitelistedRtcpBlocks(base_event->packet_, buffer);
    proto_batch->set_raw_packet(buffer, buffer_length);
  }
  // TODO(terelius): Delta-compress rest of batch.
}

void RtcEventLogEncoderNewFormat::EncodeRtpPacketIncoming(
    rtc::ArrayView<const RtcEventRtpPacketIncoming*> batch,
    rtclog2::EventStream* event_stream) {
  if (batch.size() == 0)
    return;
  for (const RtcEventRtpPacketIncoming* base_event : batch) {
    rtclog2::IncomingRtpPackets* proto_batch =
        event_stream->add_incoming_rtp_packets();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_marker(base_event->header_.Marker());
    // TODO(terelius): Is payload type needed?
    proto_batch->set_payload_type(base_event->header_.PayloadType());
    proto_batch->set_sequence_number(base_event->header_.SequenceNumber());
    proto_batch->set_rtp_timestamp(base_event->header_.Timestamp());
    proto_batch->set_ssrc(base_event->header_.Ssrc());
    proto_batch->set_payload_size(base_event->payload_length_);
    proto_batch->set_header_size(base_event->header_length_);
    proto_batch->set_padding_size(base_event->padding_length_);

    // Add header extensions.
    if (base_event->header_.HasExtension<TransmissionOffset>()) {
      int32_t offset;
      base_event->header_.GetExtension<TransmissionOffset>(&offset);
      proto_batch->set_transmission_time_offset(offset);
    }
    if (base_event->header_.HasExtension<AbsoluteSendTime>()) {
      uint32_t sendtime;
      base_event->header_.GetExtension<AbsoluteSendTime>(&sendtime);
      proto_batch->set_absolute_send_time(sendtime);
    }
    if (base_event->header_.HasExtension<TransportSequenceNumber>()) {
      uint16_t seqnum;
      base_event->header_.GetExtension<TransportSequenceNumber>(&seqnum);
      proto_batch->set_transport_sequence_number(seqnum);
    }
    if (base_event->header_.HasExtension<AudioLevel>()) {
      bool voice_activity;
      uint8_t audio_level;
      base_event->header_.GetExtension<AudioLevel>(&voice_activity,
                                                   &audio_level);
      RTC_DCHECK(audio_level < 128);
      if (voice_activity) {
        audio_level += 128;  // Most significant bit indicates voice activity.
      }
      proto_batch->set_audio_level(audio_level);
    }
    if (base_event->header_.HasExtension<VideoOrientation>()) {
      VideoRotation video_rotation;
      base_event->header_.GetExtension<VideoOrientation>(&video_rotation);
      proto_batch->set_video_rotation(
          ConvertVideoRotationToCVOByte(video_rotation));
    }
  }
  // TODO(terelius): Delta-compress rest of batch.
}

void RtcEventLogEncoderNewFormat::EncodeRtpPacketOutgoing(
    rtc::ArrayView<const RtcEventRtpPacketOutgoing*> batch,
    rtclog2::EventStream* event_stream) {
  if (batch.size() == 0)
    return;
  for (const RtcEventRtpPacketOutgoing* base_event : batch) {
    rtclog2::OutgoingRtpPackets* proto_batch =
        event_stream->add_outgoing_rtp_packets();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_marker(base_event->header_.Marker());
    // TODO(terelius): Is payload type needed?
    proto_batch->set_payload_type(base_event->header_.PayloadType());
    proto_batch->set_sequence_number(base_event->header_.SequenceNumber());
    proto_batch->set_rtp_timestamp(base_event->header_.Timestamp());
    proto_batch->set_ssrc(base_event->header_.Ssrc());
    proto_batch->set_payload_size(base_event->payload_length_);
    proto_batch->set_header_size(base_event->header_length_);
    proto_batch->set_padding_size(base_event->padding_length_);

    // Add header extensions.
    if (base_event->header_.HasExtension<TransmissionOffset>()) {
      int32_t offset;
      base_event->header_.GetExtension<TransmissionOffset>(&offset);
      proto_batch->set_transmission_time_offset(offset);
    }
    if (base_event->header_.HasExtension<AbsoluteSendTime>()) {
      uint32_t sendtime;
      base_event->header_.GetExtension<AbsoluteSendTime>(&sendtime);
      proto_batch->set_absolute_send_time(sendtime);
    }
    if (base_event->header_.HasExtension<TransportSequenceNumber>()) {
      uint16_t seqnum;
      base_event->header_.GetExtension<TransportSequenceNumber>(&seqnum);
      proto_batch->set_transport_sequence_number(seqnum);
    }
    if (base_event->header_.HasExtension<AudioLevel>()) {
      bool voice_activity;
      uint8_t audio_level;
      base_event->header_.GetExtension<AudioLevel>(&voice_activity,
                                                   &audio_level);
      RTC_DCHECK(audio_level < 128);
      if (voice_activity) {
        audio_level += 128;  // Most significant bit indicates voice activity.
      }
      proto_batch->set_audio_level(audio_level);
    }
    if (base_event->header_.HasExtension<VideoOrientation>()) {
      VideoRotation video_rotation;
      base_event->header_.GetExtension<VideoOrientation>(&video_rotation);
      proto_batch->set_video_rotation(
          ConvertVideoRotationToCVOByte(video_rotation));
    }
  }
  // TODO(terelius): Delta-compress rest of batch.
}

void RtcEventLogEncoderNewFormat::EncodeVideoRecvStreamConfig(
    rtc::ArrayView<const RtcEventVideoReceiveStreamConfig*> batch,
    rtclog2::EventStream* event_stream) {
  for (const RtcEventVideoReceiveStreamConfig* base_event : batch) {
    rtclog2::VideoRecvStreamConfig* proto_batch =
        event_stream->add_video_recv_stream_configs();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_remote_ssrc(base_event->config_->remote_ssrc);
    proto_batch->set_local_ssrc(base_event->config_->local_ssrc);
    proto_batch->set_rtx_ssrc(base_event->config_->rtx_ssrc);
    if (!base_event->config_->rsid.empty())
      proto_batch->set_rsid(base_event->config_->rsid);

    rtclog2::RtpHeaderExtensionConfig* proto_config =
        proto_batch->mutable_header_extensions();
    bool has_recognized_extensions =
        ConvertToProtoFormat(base_event->config_->rtp_extensions, proto_config);
    if (!has_recognized_extensions)
      proto_batch->clear_header_extensions();
  }
}

void RtcEventLogEncoderNewFormat::EncodeVideoSendStreamConfig(
    rtc::ArrayView<const RtcEventVideoSendStreamConfig*> batch,
    rtclog2::EventStream* event_stream) {
  for (const RtcEventVideoSendStreamConfig* base_event : batch) {
    rtclog2::VideoSendStreamConfig* proto_batch =
        event_stream->add_video_send_stream_configs();
    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_ssrc(base_event->config_->local_ssrc);
    proto_batch->set_rtx_ssrc(base_event->config_->rtx_ssrc);
    if (!base_event->config_->rsid.empty())
      proto_batch->set_rsid(base_event->config_->rsid);

    rtclog2::RtpHeaderExtensionConfig* proto_config =
        proto_batch->mutable_header_extensions();
    bool has_recognized_extensions =
        ConvertToProtoFormat(base_event->config_->rtp_extensions, proto_config);
    if (!has_recognized_extensions)
      proto_batch->clear_header_extensions();
  }
}

void RtcEventLogEncoderNewFormat::EncodeIceCandidatePairConfig(
    rtc::ArrayView<const RtcEventIceCandidatePairConfig*> batch,
    rtclog2::EventStream* event_stream) {
  for (const RtcEventIceCandidatePairConfig* base_event : batch) {
    rtclog2::IceCandidatePairConfig* proto_batch =
        event_stream->add_ice_candidate_configs();

    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);
    proto_batch->set_config_type(ConvertToProtoFormat(base_event->type_));
    proto_batch->set_candidate_pair_id(base_event->candidate_pair_id_);
    const auto& desc = base_event->candidate_pair_desc_;
    proto_batch->set_local_candidate_type(
        ConvertToProtoFormat(desc.local_candidate_type));
    proto_batch->set_local_relay_protocol(
        ConvertToProtoFormat(desc.local_relay_protocol));
    proto_batch->set_local_network_type(
        ConvertToProtoFormat(desc.local_network_type));
    proto_batch->set_local_address_family(
        ConvertToProtoFormat(desc.local_address_family));
    proto_batch->set_remote_candidate_type(
        ConvertToProtoFormat(desc.remote_candidate_type));
    proto_batch->set_remote_address_family(
        ConvertToProtoFormat(desc.remote_address_family));
    proto_batch->set_candidate_pair_protocol(
        ConvertToProtoFormat(desc.candidate_pair_protocol));
  }
  // TODO(terelius): Should we delta-compress this event type?
}

void RtcEventLogEncoderNewFormat::EncodeIceCandidatePairEvent(
    rtc::ArrayView<const RtcEventIceCandidatePair*> batch,
    rtclog2::EventStream* event_stream) {
  for (const RtcEventIceCandidatePair* base_event : batch) {
    rtclog2::IceCandidatePairEvent* proto_batch =
        event_stream->add_ice_candidate_events();

    proto_batch->set_timestamp_ms(base_event->timestamp_us_ / 1000);

    proto_batch->set_event_type(ConvertToProtoFormat(base_event->type_));
    proto_batch->set_candidate_pair_id(base_event->candidate_pair_id_);
  }
  // TODO(terelius): Should we delta-compress this event type?
}

}  // namespace webrtc
