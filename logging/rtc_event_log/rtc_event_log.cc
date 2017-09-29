/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log.h"

#include <atomic>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
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
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/event.h"
#include "rtc_base/ignore_wundef.h"
#include "rtc_base/logging.h"
#include "rtc_base/protobuf_utils.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/file_wrapper.h"
#include "typedefs.h"  // NOLINT(build/include)

#ifdef ENABLE_RTC_EVENT_LOG
// *.pb.h files are generated at build-time by the protobuf compiler.
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#else
#include "logging/rtc_event_log/rtc_event_log.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()
#endif

namespace webrtc {

#ifdef ENABLE_RTC_EVENT_LOG

namespace {
const int kEventsInHistory = 10000;

bool IsConfigEvent(const rtclog::Event& event) {
  rtclog::Event_EventType event_type = event.type();
  return event_type == rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT ||
         event_type == rtclog::Event::VIDEO_SENDER_CONFIG_EVENT ||
         event_type == rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT ||
         event_type == rtclog::Event::AUDIO_SENDER_CONFIG_EVENT;
}

// Observe a limit on the number of concurrent logs, so as not to run into
// OS-imposed limits on open files and/or threads/task-queues.
// TODO(eladalon): Known issue - there's a race over |rtc_event_log_count|.
std::atomic<int> rtc_event_log_count(0);

// TODO(eladalon): This class exists because C++11 doesn't allow transferring a
// unique_ptr to a lambda (a copy constructor is required). We should get
// rid of this when we move to C++14.
template <typename T>
class ResourceOwningTask final : public rtc::QueuedTask {
 public:
  ResourceOwningTask(std::unique_ptr<T> resource,
                     std::function<void(std::unique_ptr<T>)> handler)
      : resource_(std::move(resource)), handler_(handler) {}

  bool Run() override {
    handler_(std::move(resource_));
    return true;
  }

 private:
  std::unique_ptr<T> resource_;
  std::function<void(std::unique_ptr<T>)> handler_;
};

class RtcEventLogImpl final : public RtcEventLog {
 public:
  RtcEventLogImpl();
  ~RtcEventLogImpl() override;

  bool StartLogging(const std::string& file_name,
                    int64_t max_size_bytes) override;
  bool StartLogging(rtc::PlatformFile platform_file,
                    int64_t max_size_bytes) override;
  void StopLogging() override;
  void LogVideoReceiveStreamConfig(const rtclog::StreamConfig& config) override;
  void LogVideoSendStreamConfig(const rtclog::StreamConfig& config) override;
  void LogAudioReceiveStreamConfig(const rtclog::StreamConfig& config) override;
  void LogAudioSendStreamConfig(const rtclog::StreamConfig& config) override;
  // TODO(terelius): This can be removed as soon as the interface has been
  // updated.
  void LogRtpHeader(PacketDirection direction,
                    const uint8_t* header,
                    size_t packet_length) override;
  // TODO(terelius): This can be made private, non-virtual as soon as the
  // interface has been updated.
  void LogRtpHeader(PacketDirection direction,
                    const uint8_t* header,
                    size_t packet_length,
                    int probe_cluster_id) override;
  void LogIncomingRtpHeader(const RtpPacketReceived& packet) override;
  void LogOutgoingRtpHeader(const RtpPacketToSend& packet,
                            int probe_cluster_id) override;
  // TODO(terelius): This can be made private, non-virtual as soon as the
  // interface has been updated.
  void LogRtcpPacket(PacketDirection direction,
                     const uint8_t* packet,
                     size_t length) override;
  void LogIncomingRtcpPacket(rtc::ArrayView<const uint8_t> packet) override;
  void LogOutgoingRtcpPacket(rtc::ArrayView<const uint8_t> packet) override;
  void LogAudioPlayout(uint32_t ssrc) override;
  void LogLossBasedBweUpdate(int32_t bitrate_bps,
                             uint8_t fraction_loss,
                             int32_t total_packets) override;
  void LogDelayBasedBweUpdate(int32_t bitrate_bps,
                              BandwidthUsage detector_state) override;
  void LogAudioNetworkAdaptation(
      const AudioEncoderRuntimeConfig& config) override;
  void LogProbeClusterCreated(int id,
                              int bitrate_bps,
                              int min_probes,
                              int min_bytes) override;
  void LogProbeResultSuccess(int id, int bitrate_bps) override;
  void LogProbeResultFailure(int id,
                             ProbeFailureReason failure_reason) override;

 private:
  void StartLoggingInternal(std::unique_ptr<FileWrapper> file,
                            int64_t max_size_bytes);

  void StoreEvent(std::unique_ptr<rtclog::Event> event);
  void LogProbeResult(int id,
                      rtclog::BweProbeResult::ResultType result,
                      int bitrate_bps);

  // Appends an event to the output protobuf string, returning true on success.
  // Fails and returns false in case the limit on output size prevents the
  // event from being added; in this case, the output string is left unchanged.
  bool AppendEventToString(rtclog::Event* event,
                           ProtoString* output_string) RTC_WARN_UNUSED_RESULT;

  void LogToMemory(std::unique_ptr<rtclog::Event> event);

  void StartLogFile();
  void LogToFile(std::unique_ptr<rtclog::Event> event);
  void StopLogFile(int64_t stop_time);

  // Make sure that the event log is "managed" - created/destroyed, as well
  // as started/stopped - from the same thread/task-queue.
  rtc::SequencedTaskChecker owner_sequence_checker_;

  // History containing all past configuration events.
  std::vector<std::unique_ptr<rtclog::Event>> config_history_
      RTC_ACCESS_ON(task_queue_);

  // History containing the most recent (non-configuration) events (~10s).
  std::deque<std::unique_ptr<rtclog::Event>> history_
      RTC_ACCESS_ON(task_queue_);

  std::unique_ptr<FileWrapper> file_ RTC_ACCESS_ON(task_queue_);

  size_t max_size_bytes_ RTC_ACCESS_ON(task_queue_);
  size_t written_bytes_ RTC_ACCESS_ON(task_queue_);

  // Keep this last to ensure it destructs first, or else tasks living on the
  // queue might access other members after they've been torn down.
  rtc::TaskQueue task_queue_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtcEventLogImpl);
};

rtclog::VideoReceiveConfig_RtcpMode ConvertRtcpMode(RtcpMode rtcp_mode) {
  switch (rtcp_mode) {
    case RtcpMode::kCompound:
      return rtclog::VideoReceiveConfig::RTCP_COMPOUND;
    case RtcpMode::kReducedSize:
      return rtclog::VideoReceiveConfig::RTCP_REDUCEDSIZE;
    case RtcpMode::kOff:
      RTC_NOTREACHED();
      return rtclog::VideoReceiveConfig::RTCP_COMPOUND;
  }
  RTC_NOTREACHED();
  return rtclog::VideoReceiveConfig::RTCP_COMPOUND;
}

rtclog::DelayBasedBweUpdate::DetectorState ConvertDetectorState(
    BandwidthUsage state) {
  switch (state) {
    case BandwidthUsage::kBwNormal:
      return rtclog::DelayBasedBweUpdate::BWE_NORMAL;
    case BandwidthUsage::kBwUnderusing:
      return rtclog::DelayBasedBweUpdate::BWE_UNDERUSING;
    case BandwidthUsage::kBwOverusing:
      return rtclog::DelayBasedBweUpdate::BWE_OVERUSING;
  }
  RTC_NOTREACHED();
  return rtclog::DelayBasedBweUpdate::BWE_NORMAL;
}

rtclog::BweProbeResult::ResultType ConvertProbeResultType(
    ProbeFailureReason failure_reason) {
  switch (failure_reason) {
    case kInvalidSendReceiveInterval:
      return rtclog::BweProbeResult::INVALID_SEND_RECEIVE_INTERVAL;
    case kInvalidSendReceiveRatio:
      return rtclog::BweProbeResult::INVALID_SEND_RECEIVE_RATIO;
    case kTimeout:
      return rtclog::BweProbeResult::TIMEOUT;
  }
  RTC_NOTREACHED();
  return rtclog::BweProbeResult::SUCCESS;
}

RtcEventLogImpl::RtcEventLogImpl()
    : file_(FileWrapper::Create()),
      max_size_bytes_(std::numeric_limits<decltype(max_size_bytes_)>::max()),
      written_bytes_(0),
      task_queue_("rtc_event_log") {}

RtcEventLogImpl::~RtcEventLogImpl() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&owner_sequence_checker_);

  // If we're logging to the file, this will stop that. Blocking function.
  StopLogging();

  int count = std::atomic_fetch_sub(&rtc_event_log_count, 1) - 1;
  RTC_DCHECK_GE(count, 0);
}

bool RtcEventLogImpl::StartLogging(const std::string& file_name,
                                   int64_t max_size_bytes) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&owner_sequence_checker_);

  auto file = rtc::WrapUnique<FileWrapper>(FileWrapper::Create());
  if (!file->OpenFile(file_name.c_str(), false)) {
    LOG(LS_ERROR) << "Can't open file. WebRTC event log not started.";
    return false;
  }

  StartLoggingInternal(std::move(file), max_size_bytes);

  return true;
}

bool RtcEventLogImpl::StartLogging(rtc::PlatformFile platform_file,
                                   int64_t max_size_bytes) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&owner_sequence_checker_);

  auto file = rtc::WrapUnique<FileWrapper>(FileWrapper::Create());
  FILE* file_handle = rtc::FdopenPlatformFileForWriting(platform_file);
  if (!file_handle) {
    LOG(LS_ERROR) << "Can't open file. WebRTC event log not started.";
    // Even though we failed to open a FILE*, the platform_file is still open
    // and needs to be closed.
    if (!rtc::ClosePlatformFile(platform_file)) {
      LOG(LS_ERROR) << "Can't close file.";
    }
    return false;
  }
  if (!file->OpenFromFileHandle(file_handle)) {
    LOG(LS_ERROR) << "Can't open file. WebRTC event log not started.";
    return false;
  }

  StartLoggingInternal(std::move(file), max_size_bytes);

  return true;
}

void RtcEventLogImpl::StopLogging() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&owner_sequence_checker_);

  LOG(LS_INFO) << "Stopping WebRTC event log.";

  const int64_t stop_time = rtc::TimeMicros();

  rtc::Event file_finished(true, false);

  task_queue_.PostTask([this, stop_time, &file_finished]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (file_->is_open()) {
      StopLogFile(stop_time);
    }
    file_finished.Set();
  });

  file_finished.Wait(rtc::Event::kForever);

  LOG(LS_INFO) << "WebRTC event log successfully stopped.";
}

void RtcEventLogImpl::LogVideoReceiveStreamConfig(
    const rtclog::StreamConfig& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT);

  rtclog::VideoReceiveConfig* receiver_config =
      event->mutable_video_receiver_config();
  receiver_config->set_remote_ssrc(config.remote_ssrc);
  receiver_config->set_local_ssrc(config.local_ssrc);

  // TODO(perkj): Add field for rsid.
  receiver_config->set_rtcp_mode(ConvertRtcpMode(config.rtcp_mode));
  receiver_config->set_remb(config.remb);

  for (const auto& e : config.rtp_extensions) {
    rtclog::RtpHeaderExtension* extension =
        receiver_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  for (const auto& d : config.codecs) {
    rtclog::DecoderConfig* decoder = receiver_config->add_decoders();
    decoder->set_name(d.payload_name);
    decoder->set_payload_type(d.payload_type);
    if (d.rtx_payload_type != 0) {
      rtclog::RtxMap* rtx = receiver_config->add_rtx_map();
      rtx->set_payload_type(d.payload_type);
      rtx->mutable_config()->set_rtx_ssrc(config.rtx_ssrc);
      rtx->mutable_config()->set_rtx_payload_type(d.rtx_payload_type);
    }
  }
  StoreEvent(std::move(event));
}

void RtcEventLogImpl::LogVideoSendStreamConfig(
    const rtclog::StreamConfig& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::VIDEO_SENDER_CONFIG_EVENT);

  rtclog::VideoSendConfig* sender_config = event->mutable_video_sender_config();

  // TODO(perkj): rtclog::VideoSendConfig should only contain one SSRC.
  sender_config->add_ssrcs(config.local_ssrc);
  if (config.rtx_ssrc != 0) {
    sender_config->add_rtx_ssrcs(config.rtx_ssrc);
  }

  for (const auto& e : config.rtp_extensions) {
    rtclog::RtpHeaderExtension* extension =
        sender_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  // TODO(perkj): rtclog::VideoSendConfig should contain many possible codec
  // configurations.
  for (const auto& codec : config.codecs) {
    sender_config->set_rtx_payload_type(codec.rtx_payload_type);
    rtclog::EncoderConfig* encoder = sender_config->mutable_encoder();
    encoder->set_name(codec.payload_name);
    encoder->set_payload_type(codec.payload_type);

    if (config.codecs.size() > 1) {
      LOG(WARNING) << "LogVideoSendStreamConfig currently only supports one "
                   << "codec. Logging codec :" << codec.payload_name;
      break;
    }
  }

  StoreEvent(std::move(event));
}

void RtcEventLogImpl::LogAudioReceiveStreamConfig(
    const rtclog::StreamConfig& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT);

  rtclog::AudioReceiveConfig* receiver_config =
      event->mutable_audio_receiver_config();
  receiver_config->set_remote_ssrc(config.remote_ssrc);
  receiver_config->set_local_ssrc(config.local_ssrc);

  for (const auto& e : config.rtp_extensions) {
    rtclog::RtpHeaderExtension* extension =
        receiver_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }
  StoreEvent(std::move(event));
}

void RtcEventLogImpl::LogAudioSendStreamConfig(
    const rtclog::StreamConfig& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::AUDIO_SENDER_CONFIG_EVENT);

  rtclog::AudioSendConfig* sender_config = event->mutable_audio_sender_config();

  sender_config->set_ssrc(config.local_ssrc);

  for (const auto& e : config.rtp_extensions) {
    rtclog::RtpHeaderExtension* extension =
        sender_config->add_header_extensions();
    extension->set_name(e.uri);
    extension->set_id(e.id);
  }

  StoreEvent(std::move(event));
}

void RtcEventLogImpl::LogIncomingRtpHeader(const RtpPacketReceived& packet) {
  LogRtpHeader(kIncomingPacket, packet.data(), packet.size(),
               PacedPacketInfo::kNotAProbe);
}

void RtcEventLogImpl::LogOutgoingRtpHeader(const RtpPacketToSend& packet,
                                           int probe_cluster_id) {
  LogRtpHeader(kOutgoingPacket, packet.data(), packet.size(), probe_cluster_id);
}

void RtcEventLogImpl::LogRtpHeader(PacketDirection direction,
                                   const uint8_t* header,
                                   size_t packet_length) {
  LogRtpHeader(direction, header, packet_length, PacedPacketInfo::kNotAProbe);
}

void RtcEventLogImpl::LogRtpHeader(PacketDirection direction,
                                   const uint8_t* header,
                                   size_t packet_length,
                                   int probe_cluster_id) {
  // Read header length (in bytes) from packet data.
  if (packet_length < 12u) {
    return;  // Don't read outside the packet.
  }
  const bool x = (header[0] & 0x10) != 0;
  const uint8_t cc = header[0] & 0x0f;
  size_t header_length = 12u + cc * 4u;

  if (x) {
    if (packet_length < 12u + cc * 4u + 4u) {
      return;  // Don't read outside the packet.
    }
    size_t x_len = ByteReader<uint16_t>::ReadBigEndian(header + 14 + cc * 4);
    header_length += (x_len + 1) * 4;
  }

  std::unique_ptr<rtclog::Event> rtp_event(new rtclog::Event());
  rtp_event->set_timestamp_us(rtc::TimeMicros());
  rtp_event->set_type(rtclog::Event::RTP_EVENT);
  rtp_event->mutable_rtp_packet()->set_incoming(direction == kIncomingPacket);
  rtp_event->mutable_rtp_packet()->set_packet_length(packet_length);
  rtp_event->mutable_rtp_packet()->set_header(header, header_length);
  if (probe_cluster_id != PacedPacketInfo::kNotAProbe)
    rtp_event->mutable_rtp_packet()->set_probe_cluster_id(probe_cluster_id);
  StoreEvent(std::move(rtp_event));
}

void RtcEventLogImpl::LogIncomingRtcpPacket(
    rtc::ArrayView<const uint8_t> packet) {
  LogRtcpPacket(kIncomingPacket, packet.data(), packet.size());
}

void RtcEventLogImpl::LogOutgoingRtcpPacket(
    rtc::ArrayView<const uint8_t> packet) {
  LogRtcpPacket(kOutgoingPacket, packet.data(), packet.size());
}

void RtcEventLogImpl::LogRtcpPacket(PacketDirection direction,
                                    const uint8_t* packet,
                                    size_t length) {
  std::unique_ptr<rtclog::Event> rtcp_event(new rtclog::Event());
  rtcp_event->set_timestamp_us(rtc::TimeMicros());
  rtcp_event->set_type(rtclog::Event::RTCP_EVENT);
  rtcp_event->mutable_rtcp_packet()->set_incoming(direction == kIncomingPacket);

  rtcp::CommonHeader header;
  const uint8_t* block_begin = packet;
  const uint8_t* packet_end = packet + length;
  RTC_DCHECK(length <= IP_PACKET_SIZE);
  uint8_t buffer[IP_PACKET_SIZE];
  uint32_t buffer_length = 0;
  while (block_begin < packet_end) {
    if (!header.Parse(block_begin, packet_end - block_begin)) {
      break;  // Incorrect message header.
    }
    const uint8_t* next_block = header.NextPacket();
    uint32_t block_size = next_block - block_begin;
    switch (header.type()) {
      case rtcp::SenderReport::kPacketType:
      case rtcp::ReceiverReport::kPacketType:
      case rtcp::Bye::kPacketType:
      case rtcp::ExtendedJitterReport::kPacketType:
      case rtcp::Rtpfb::kPacketType:
      case rtcp::Psfb::kPacketType:
      case rtcp::ExtendedReports::kPacketType:
        // We log sender reports, receiver reports, bye messages
        // inter-arrival jitter, third-party loss reports, payload-specific
        // feedback and extended reports.
        memcpy(buffer + buffer_length, block_begin, block_size);
        buffer_length += block_size;
        break;
      case rtcp::Sdes::kPacketType:
      case rtcp::App::kPacketType:
      default:
        // We don't log sender descriptions, application defined messages
        // or message blocks of unknown type.
        break;
    }

    block_begin += block_size;
  }
  rtcp_event->mutable_rtcp_packet()->set_packet_data(buffer, buffer_length);
  StoreEvent(std::move(rtcp_event));
}

void RtcEventLogImpl::LogAudioPlayout(uint32_t ssrc) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::AUDIO_PLAYOUT_EVENT);
  auto playout_event = event->mutable_audio_playout_event();
  playout_event->set_local_ssrc(ssrc);
  StoreEvent(std::move(event));
}

void RtcEventLogImpl::LogLossBasedBweUpdate(int32_t bitrate_bps,
                                            uint8_t fraction_loss,
                                            int32_t total_packets) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::LOSS_BASED_BWE_UPDATE);
  auto bwe_event = event->mutable_loss_based_bwe_update();
  bwe_event->set_bitrate_bps(bitrate_bps);
  bwe_event->set_fraction_loss(fraction_loss);
  bwe_event->set_total_packets(total_packets);
  StoreEvent(std::move(event));
}

void RtcEventLogImpl::LogDelayBasedBweUpdate(int32_t bitrate_bps,
                                             BandwidthUsage detector_state) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::DELAY_BASED_BWE_UPDATE);
  auto bwe_event = event->mutable_delay_based_bwe_update();
  bwe_event->set_bitrate_bps(bitrate_bps);
  bwe_event->set_detector_state(ConvertDetectorState(detector_state));
  StoreEvent(std::move(event));
}

void RtcEventLogImpl::LogAudioNetworkAdaptation(
    const AudioEncoderRuntimeConfig& config) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::AUDIO_NETWORK_ADAPTATION_EVENT);
  auto audio_network_adaptation = event->mutable_audio_network_adaptation();
  if (config.bitrate_bps)
    audio_network_adaptation->set_bitrate_bps(*config.bitrate_bps);
  if (config.frame_length_ms)
    audio_network_adaptation->set_frame_length_ms(*config.frame_length_ms);
  if (config.uplink_packet_loss_fraction) {
    audio_network_adaptation->set_uplink_packet_loss_fraction(
        *config.uplink_packet_loss_fraction);
  }
  if (config.enable_fec)
    audio_network_adaptation->set_enable_fec(*config.enable_fec);
  if (config.enable_dtx)
    audio_network_adaptation->set_enable_dtx(*config.enable_dtx);
  if (config.num_channels)
    audio_network_adaptation->set_num_channels(*config.num_channels);
  StoreEvent(std::move(event));
}

void RtcEventLogImpl::LogProbeClusterCreated(int id,
                                             int bitrate_bps,
                                             int min_probes,
                                             int min_bytes) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::BWE_PROBE_CLUSTER_CREATED_EVENT);

  auto probe_cluster = event->mutable_probe_cluster();
  probe_cluster->set_id(id);
  probe_cluster->set_bitrate_bps(bitrate_bps);
  probe_cluster->set_min_packets(min_probes);
  probe_cluster->set_min_bytes(min_bytes);
  StoreEvent(std::move(event));
}

void RtcEventLogImpl::LogProbeResultSuccess(int id, int bitrate_bps) {
  LogProbeResult(id, rtclog::BweProbeResult::SUCCESS, bitrate_bps);
}

void RtcEventLogImpl::LogProbeResultFailure(int id,
                                            ProbeFailureReason failure_reason) {
  rtclog::BweProbeResult::ResultType result =
      ConvertProbeResultType(failure_reason);
  LogProbeResult(id, result, -1);
}

void RtcEventLogImpl::LogProbeResult(int id,
                                     rtclog::BweProbeResult::ResultType result,
                                     int bitrate_bps) {
  std::unique_ptr<rtclog::Event> event(new rtclog::Event());
  event->set_timestamp_us(rtc::TimeMicros());
  event->set_type(rtclog::Event::BWE_PROBE_RESULT_EVENT);

  auto probe_result = event->mutable_probe_result();
  probe_result->set_id(id);
  probe_result->set_result(result);
  if (result == rtclog::BweProbeResult::SUCCESS)
    probe_result->set_bitrate_bps(bitrate_bps);
  StoreEvent(std::move(event));
}

void RtcEventLogImpl::StartLoggingInternal(std::unique_ptr<FileWrapper> file,
                                           int64_t max_size_bytes) {
  LOG(LS_INFO) << "Starting WebRTC event log.";

  max_size_bytes = (max_size_bytes <= 0)
                       ? std::numeric_limits<decltype(max_size_bytes)>::max()
                       : max_size_bytes;
  auto file_handler = [this,
                       max_size_bytes](std::unique_ptr<FileWrapper> file) {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (!file_->is_open()) {
      max_size_bytes_ = max_size_bytes;
      file_ = std::move(file);
      StartLogFile();
    } else {
      // Already started. Ignore message and close file handle.
      file->CloseFile();
    }
  };
  task_queue_.PostTask(rtc::MakeUnique<ResourceOwningTask<FileWrapper>>(
      std::move(file), file_handler));
}

void RtcEventLogImpl::StoreEvent(std::unique_ptr<rtclog::Event> event) {
  RTC_DCHECK(event);

  auto event_handler = [this](std::unique_ptr<rtclog::Event> rtclog_event) {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (file_->is_open()) {
      LogToFile(std::move(rtclog_event));
    } else {
      LogToMemory(std::move(rtclog_event));
    }
  };

  task_queue_.PostTask(rtc::MakeUnique<ResourceOwningTask<rtclog::Event>>(
      std::move(event), event_handler));
}

bool RtcEventLogImpl::AppendEventToString(rtclog::Event* event,
                                          ProtoString* output_string) {
  RTC_DCHECK_RUN_ON(&task_queue_);

  // Even though we're only serializing a single event during this call, what
  // we intend to get is a list of events, with a tag and length preceding
  // each actual event. To produce that, we serialize a list of a single event.
  // If we later serialize additional events, the resulting ProtoString will
  // be a proper concatenation of all those events.

  rtclog::EventStream event_stream;
  event_stream.add_stream();

  // As a tweak, we swap the new event into the event-stream, write that to
  // file, then swap back. This saves on some copying.
  rtclog::Event* output_event = event_stream.mutable_stream(0);
  output_event->Swap(event);

  bool appended;
  size_t potential_new_size =
      written_bytes_ + output_string->size() + event_stream.ByteSize();
  if (potential_new_size <= max_size_bytes_) {
    event_stream.AppendToString(output_string);
    appended = true;
  } else {
    appended = false;
  }

  // When the function returns, the original Event will be unchanged.
  output_event->Swap(event);

  return appended;
}

void RtcEventLogImpl::LogToMemory(std::unique_ptr<rtclog::Event> event) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  RTC_DCHECK(!file_->is_open());

  if (IsConfigEvent(*event.get())) {
    config_history_.push_back(std::move(event));
  } else {
    history_.push_back(std::move(event));
    if (history_.size() > kEventsInHistory) {
      history_.pop_front();
    }
  }
}

void RtcEventLogImpl::StartLogFile() {
  RTC_DCHECK_RUN_ON(&task_queue_);
  RTC_DCHECK(file_->is_open());

  ProtoString output_string;

  // Create and serialize the LOG_START event.
  // The timestamp used will correspond to when logging has started. The log
  // may contain events earlier than the LOG_START event. (In general, the
  // timestamps in the log are not monotonic.)
  rtclog::Event start_event;
  start_event.set_timestamp_us(rtc::TimeMicros());
  start_event.set_type(rtclog::Event::LOG_START);
  bool appended = AppendEventToString(&start_event, &output_string);

  // Serialize the config information for all old streams, including streams
  // which were already logged to previous files.
  for (auto& event : config_history_) {
    if (!appended) {
      break;
    }
    appended = AppendEventToString(event.get(), &output_string);
  }

  // Serialize the events in the event queue.
  while (appended && !history_.empty()) {
    appended = AppendEventToString(history_.front().get(), &output_string);
    if (appended) {
      // Known issue - if writing to the file fails, these events will have
      // been lost. If we try to open a new file, these events will be missing
      // from it.
      history_.pop_front();
    }
  }

  // Write to file.
  if (!file_->Write(output_string.data(), output_string.size())) {
    LOG(LS_ERROR) << "FileWrapper failed to write WebRtcEventLog file.";
    // The current FileWrapper implementation closes the file on error.
    RTC_DCHECK(!file_->is_open());
    return;
  }
  written_bytes_ += output_string.size();

  if (!appended) {
    RTC_DCHECK(file_->is_open());
    StopLogFile(rtc::TimeMicros());
  }
}

void RtcEventLogImpl::LogToFile(std::unique_ptr<rtclog::Event> event) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  RTC_DCHECK(file_->is_open());

  ProtoString output_string;

  bool appended = AppendEventToString(event.get(), &output_string);

  if (IsConfigEvent(*event.get())) {
    config_history_.push_back(std::move(event));
  }

  if (!appended) {
    RTC_DCHECK(file_->is_open());
    history_.push_back(std::move(event));
    StopLogFile(rtc::TimeMicros());
    return;
  }

  // Write string to file.
  if (file_->Write(output_string.data(), output_string.size())) {
    written_bytes_ += output_string.size();
  } else {
    LOG(LS_ERROR) << "FileWrapper failed to write WebRtcEventLog file.";
    // The current FileWrapper implementation closes the file on error.
    RTC_DCHECK(!file_->is_open());
  }
}

void RtcEventLogImpl::StopLogFile(int64_t stop_time) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  RTC_DCHECK(file_->is_open());

  ProtoString output_string;

  rtclog::Event end_event;
  end_event.set_timestamp_us(stop_time);
  end_event.set_type(rtclog::Event::LOG_END);
  bool appended = AppendEventToString(&end_event, &output_string);

  if (appended) {
    if (!file_->Write(output_string.data(), output_string.size())) {
      LOG(LS_ERROR) << "FileWrapper failed to write WebRtcEventLog file.";
      // The current FileWrapper implementation closes the file on error.
      RTC_DCHECK(!file_->is_open());
    }
    written_bytes_ += output_string.size();
  }

  max_size_bytes_ = std::numeric_limits<decltype(max_size_bytes_)>::max();
  written_bytes_ = 0;

  file_->CloseFile();
  RTC_DCHECK(!file_->is_open());
}

}  // namespace

#endif  // ENABLE_RTC_EVENT_LOG

// RtcEventLog member functions.
std::unique_ptr<RtcEventLog> RtcEventLog::Create() {
#ifdef ENABLE_RTC_EVENT_LOG
    // TODO(eladalon): Known issue - there's a race over |rtc_event_log_count|.
  constexpr int kMaxLogCount = 5;
  int count = 1 + std::atomic_fetch_add(&rtc_event_log_count, 1);
  if (count > kMaxLogCount) {
    LOG(LS_WARNING) << "Denied creation of additional WebRTC event logs. "
                    << count - 1 << " logs open already.";
    std::atomic_fetch_sub(&rtc_event_log_count, 1);
    return CreateNull();
  }
  return rtc::MakeUnique<RtcEventLogImpl>();
#else
  return CreateNull();
#endif  // ENABLE_RTC_EVENT_LOG
}

std::unique_ptr<RtcEventLog> RtcEventLog::CreateNull() {
  return std::unique_ptr<RtcEventLog>(new RtcEventLogNullImpl());
}

}  // namespace webrtc
