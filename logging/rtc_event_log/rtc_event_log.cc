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

#include "logging/rtc_event_log/encoder/rtc_event_log_encoder_legacy.h"
// TODO(eladalon): Remove events/* when the deprecated functions are removed.
#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_logging_started.h"
#include "logging/rtc_event_log/events/rtc_event_logging_stopped.h"
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
// TODO(eladalon): Remove these when deprecated functions are removed.
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
#include "rtc_base/logging.h"
#include "rtc_base/protobuf_utils.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/timeutils.h"
// TODO(eladalon): Remove this when output is modularized away.
#include "system_wrappers/include/file_wrapper.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

#ifdef ENABLE_RTC_EVENT_LOG

namespace {
const int kEventsInHistory = 10000;

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

std::unique_ptr<RtcEventLogEncoder> CreateEncoder(
    RtcEventLog::EncodingType type) {
  switch (type) {
    case RtcEventLog::EncodingType::Legacy:
      return rtc::MakeUnique<RtcEventLogEncoderLegacy>();
    default:
      LOG(LS_ERROR) << "Unknown RtcEventLog encoder type (" << int(type) << ")";
      RTC_NOTREACHED();
      return std::unique_ptr<RtcEventLogEncoder>(nullptr);
  }
}

class RtcEventLogImpl final : public RtcEventLog {
 public:
  explicit RtcEventLogImpl(std::unique_ptr<RtcEventLogEncoder> event_encoder);
  ~RtcEventLogImpl() override;

  bool StartLogging(const std::string& file_name,
                    int64_t max_size_bytes) override;
  bool StartLogging(rtc::PlatformFile platform_file,
                    int64_t max_size_bytes) override;
  void StopLogging() override;
  void Log(std::unique_ptr<RtcEvent> event) override;
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

  // Appends an event to the output protobuf string, returning true on success.
  // Fails and returns false in case the limit on output size prevents the
  // event from being added; in this case, the output string is left unchanged.
  // The event is encoded before being appended.
  bool AppendEventToString(const RtcEvent& event,
                           ProtoString* output_string) RTC_WARN_UNUSED_RESULT;

  void LogToMemory(std::unique_ptr<RtcEvent> event);

  void StartLogFile();
  void LogToFile(std::unique_ptr<RtcEvent> event);
  void StopLogFile(int64_t stop_time);

  // Make sure that the event log is "managed" - created/destroyed, as well
  // as started/stopped - from the same thread/task-queue.
  rtc::SequencedTaskChecker owner_sequence_checker_;

  // History containing all past configuration events.
  std::vector<std::unique_ptr<RtcEvent>> config_history_
      RTC_ACCESS_ON(task_queue_);

  // History containing the most recent (non-configuration) events (~10s).
  std::deque<std::unique_ptr<RtcEvent>> history_ RTC_ACCESS_ON(task_queue_);

  std::unique_ptr<FileWrapper> file_ RTC_ACCESS_ON(task_queue_);

  size_t max_size_bytes_ RTC_ACCESS_ON(task_queue_);
  size_t written_bytes_ RTC_ACCESS_ON(task_queue_);

  std::unique_ptr<RtcEventLogEncoder> event_encoder_ RTC_ACCESS_ON(task_queue_);

  // Keep this last to ensure it destructs first, or else tasks living on the
  // queue might access other members after they've been torn down.
  rtc::TaskQueue task_queue_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtcEventLogImpl);
};

RtcEventLogImpl::RtcEventLogImpl(
    std::unique_ptr<RtcEventLogEncoder> event_encoder)
    : file_(FileWrapper::Create()),
      max_size_bytes_(std::numeric_limits<decltype(max_size_bytes_)>::max()),
      written_bytes_(0),
      event_encoder_(std::move(event_encoder)),
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

void RtcEventLogImpl::Log(std::unique_ptr<RtcEvent> event) {
  RTC_DCHECK(event);

  auto event_handler = [this](std::unique_ptr<RtcEvent> unencoded_event) {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (file_->is_open()) {
      LogToFile(std::move(unencoded_event));
    } else {
      LogToMemory(std::move(unencoded_event));
    }
  };

  task_queue_.PostTask(rtc::MakeUnique<ResourceOwningTask<RtcEvent>>(
      std::move(event), event_handler));
}

void RtcEventLogImpl::LogVideoReceiveStreamConfig(
    const rtclog::StreamConfig& config) {
  Log(rtc::MakeUnique<RtcEventVideoReceiveStreamConfig>(
      rtc::MakeUnique<rtclog::StreamConfig>(config)));
}

void RtcEventLogImpl::LogVideoSendStreamConfig(
    const rtclog::StreamConfig& config) {
  Log(rtc::MakeUnique<RtcEventVideoSendStreamConfig>(
      rtc::MakeUnique<rtclog::StreamConfig>(config)));
}

void RtcEventLogImpl::LogAudioReceiveStreamConfig(
    const rtclog::StreamConfig& config) {
  Log(rtc::MakeUnique<RtcEventAudioReceiveStreamConfig>(
      rtc::MakeUnique<rtclog::StreamConfig>(config)));
}

void RtcEventLogImpl::LogAudioSendStreamConfig(
    const rtclog::StreamConfig& config) {
  Log(rtc::MakeUnique<RtcEventAudioSendStreamConfig>(
      rtc::MakeUnique<rtclog::StreamConfig>(config)));
}

void RtcEventLogImpl::LogIncomingRtpHeader(const RtpPacketReceived& packet) {
  Log(rtc::MakeUnique<RtcEventRtpPacketIncoming>(packet));
}

void RtcEventLogImpl::LogOutgoingRtpHeader(const RtpPacketToSend& packet,
                                           int probe_cluster_id) {
  Log(rtc::MakeUnique<RtcEventRtpPacketOutgoing>(packet, probe_cluster_id));
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
  // TODO(eladalon): This is highly inefficient. We're only doing this for
  // the deprecated interface. We should remove this soon.
  if (direction == PacketDirection::kIncomingPacket) {
    RtpPacketReceived packet;
    packet.Parse(header, packet_length);
    Log(rtc::MakeUnique<RtcEventRtpPacketIncoming>(packet));
  } else {
    RTC_CHECK_EQ(direction, PacketDirection::kOutgoingPacket);
    RtpPacketToSend packet(nullptr);
    packet.Parse(header, packet_length);
    Log(rtc::MakeUnique<RtcEventRtpPacketOutgoing>(packet, probe_cluster_id));
  }
}

void RtcEventLogImpl::LogIncomingRtcpPacket(
    rtc::ArrayView<const uint8_t> packet) {
  Log(rtc::MakeUnique<RtcEventRtcpPacketIncoming>(packet));
}

void RtcEventLogImpl::LogOutgoingRtcpPacket(
    rtc::ArrayView<const uint8_t> packet) {
  Log(rtc::MakeUnique<RtcEventRtcpPacketOutgoing>(packet));
}

void RtcEventLogImpl::LogRtcpPacket(PacketDirection direction,
                                    const uint8_t* packet,
                                    size_t length) {
  if (direction == PacketDirection::kIncomingPacket) {
    LogIncomingRtcpPacket(rtc::ArrayView<const uint8_t>(packet, length));
  } else {
    RTC_CHECK_EQ(direction, PacketDirection::kOutgoingPacket);
    LogOutgoingRtcpPacket(rtc::ArrayView<const uint8_t>(packet, length));
  }
}

void RtcEventLogImpl::LogAudioPlayout(uint32_t ssrc) {
  Log(rtc::MakeUnique<RtcEventAudioPlayout>(ssrc));
}

void RtcEventLogImpl::LogLossBasedBweUpdate(int32_t bitrate_bps,
                                            uint8_t fraction_loss,
                                            int32_t total_packets) {
  Log(rtc::MakeUnique<RtcEventBweUpdateLossBased>(bitrate_bps, fraction_loss,
                                                  total_packets));
}

void RtcEventLogImpl::LogDelayBasedBweUpdate(int32_t bitrate_bps,
                                             BandwidthUsage detector_state) {
  Log(rtc::MakeUnique<RtcEventBweUpdateDelayBased>(bitrate_bps,
                                                   detector_state));
}

void RtcEventLogImpl::LogAudioNetworkAdaptation(
    const AudioEncoderRuntimeConfig& config) {
  Log(rtc::MakeUnique<RtcEventAudioNetworkAdaptation>(
      rtc::MakeUnique<AudioEncoderRuntimeConfig>(config)));
}

void RtcEventLogImpl::LogProbeClusterCreated(int id,
                                             int bitrate_bps,
                                             int min_probes,
                                             int min_bytes) {
  Log(rtc::MakeUnique<RtcEventProbeClusterCreated>(id, bitrate_bps, min_probes,
                                                   min_bytes));
}

void RtcEventLogImpl::LogProbeResultSuccess(int id, int bitrate_bps) {
  Log(rtc::MakeUnique<RtcEventProbeResultSuccess>(id, bitrate_bps));
}

void RtcEventLogImpl::LogProbeResultFailure(int id,
                                            ProbeFailureReason failure_reason) {
  Log(rtc::MakeUnique<RtcEventProbeResultFailure>(id, failure_reason));
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

bool RtcEventLogImpl::AppendEventToString(const RtcEvent& event,
                                          ProtoString* output_string) {
  RTC_DCHECK_RUN_ON(&task_queue_);

  std::string encoded_event = event_encoder_->Encode(event);

  bool appended;
  size_t potential_new_size =
      written_bytes_ + output_string->size() + encoded_event.length();
  if (potential_new_size <= max_size_bytes_) {
    *output_string += encoded_event;
    appended = true;
  } else {
    appended = false;
  }

  return appended;
}

void RtcEventLogImpl::LogToMemory(std::unique_ptr<RtcEvent> event) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  RTC_DCHECK(!file_->is_open());

  if (event->IsConfigEvent()) {
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
  bool appended = AppendEventToString(RtcEventLoggingStarted(), &output_string);

  // Serialize the config information for all old streams, including streams
  // which were already logged to previous files.
  for (auto& event : config_history_) {
    if (!appended) {
      break;
    }
    appended = AppendEventToString(*event, &output_string);
  }

  // Serialize the events in the event queue.
  while (appended && !history_.empty()) {
    appended = AppendEventToString(*history_.front(), &output_string);
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

void RtcEventLogImpl::LogToFile(std::unique_ptr<RtcEvent> event) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  RTC_DCHECK(file_->is_open());

  ProtoString output_string;

  bool appended = AppendEventToString(*event, &output_string);

  if (event->IsConfigEvent()) {
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

  bool appended = AppendEventToString(RtcEventLoggingStopped(), &output_string);

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
std::unique_ptr<RtcEventLog> RtcEventLog::Create(EncodingType encoding_type) {
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
  auto encoder = CreateEncoder(encoding_type);
  return rtc::MakeUnique<RtcEventLogImpl>(std::move(encoder));
#else
  return CreateNull();
#endif  // ENABLE_RTC_EVENT_LOG
}

std::unique_ptr<RtcEventLog> RtcEventLog::CreateNull() {
  return std::unique_ptr<RtcEventLog>(new RtcEventLogNullImpl());
}

}  // namespace webrtc
