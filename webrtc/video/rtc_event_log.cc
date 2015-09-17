/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/rtc_event_log.h"

#include <deque>

#include "webrtc/base/checks.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/call.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/file_wrapper.h"

#ifdef ENABLE_RTC_EVENT_LOG
// Files generated at build-time by the protobuf compiler.
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/video/rtc_event_log.pb.h"
#else
#include "webrtc/video/rtc_event_log.pb.h"
#endif
#endif

namespace webrtc {

#ifndef ENABLE_RTC_EVENT_LOG

// No-op implementation if flag is not set.
class RtcEventLogImpl final : public RtcEventLog {
 public:
  void StartLogging(const std::string& file_name, int duration_ms) override {}
  void StopLogging(void) override {}
  void LogVideoReceiveStreamConfig(
      const VideoReceiveStream::Config& config) override {}
  void LogVideoSendStreamConfig(
      const VideoSendStream::Config& config) override {}
  void LogRtpHeader(bool incoming,
                    MediaType media_type,
                    const uint8_t* header,
                    size_t packet_length) override {}
  void LogRtcpPacket(bool incoming,
                     MediaType media_type,
                     const uint8_t* packet,
                     size_t length) override {}
  void LogDebugEvent(DebugEvent event_type) override {}
};

#else  // ENABLE_RTC_EVENT_LOG is defined

class RtcEventLogImpl final : public RtcEventLog {
 public:
  RtcEventLogImpl();

  void StartLogging(const std::string& file_name, int duration_ms) override;
  void StopLogging() override;
  void LogVideoReceiveStreamConfig(
      const VideoReceiveStream::Config& config) override;
  void LogVideoSendStreamConfig(const VideoSendStream::Config& config) override;
  void LogRtpHeader(bool incoming,
                    MediaType media_type,
                    const uint8_t* header,
                    size_t packet_length) override;
  void LogRtcpPacket(bool incoming,
                     MediaType media_type,
                     const uint8_t* packet,
                     size_t length) override;
  void LogDebugEvent(DebugEvent event_type) override;

 private:
  // Stops logging and clears the stored data and buffers.
  void StopLoggingLocked() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  // Adds a new event to the logfile if logging is active, or adds it to the
  // list of recent log events otherwise.
  void HandleEvent(rtclog::Event* event) EXCLUSIVE_LOCKS_REQUIRED(crit_);
  // Writes the event to the file. Note that this will destroy the state of the
  // input argument.
  void StoreToFile(rtclog::Event* event) EXCLUSIVE_LOCKS_REQUIRED(crit_);
  // Adds the event to the list of recent events, and removes any events that
  // are too old and no longer fall in the time window.
  void AddRecentEvent(const rtclog::Event& event)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Amount of time in microseconds to record log events, before starting the
  // actual log.
  const int recent_log_duration_us = 10000000;

  rtc::CriticalSection crit_;
  rtc::scoped_ptr<FileWrapper> file_ GUARDED_BY(crit_);
  rtclog::EventStream stream_ GUARDED_BY(crit_);
  std::deque<rtclog::Event> recent_log_events_ GUARDED_BY(crit_);
  bool currently_logging_ GUARDED_BY(crit_);
  int64_t start_time_us_ GUARDED_BY(crit_);
  int64_t duration_us_ GUARDED_BY(crit_);
  const Clock* const clock_;
};

namespace {
// The functions in this namespace convert enums from the runtime format
// that the rest of the WebRtc project can use, to the corresponding
// serialized enum which is defined by the protobuf.

// Do not add default return values to the conversion functions in this
// unnamed namespace. The intention is to make the compiler warn if anyone
// adds unhandled new events/modes/etc.

rtclog::DebugEvent_EventType ConvertDebugEvent(
    RtcEventLog::DebugEvent event_type) {
  switch (event_type) {
    case RtcEventLog::DebugEvent::kLogStart:
      return rtclog::DebugEvent::LOG_START;
    case RtcEventLog::DebugEvent::kLogEnd:
      return rtclog::DebugEvent::LOG_END;
    case RtcEventLog::DebugEvent::kAudioPlayout:
      return rtclog::DebugEvent::AUDIO_PLAYOUT;
  }
  RTC_NOTREACHED();
  return rtclog::DebugEvent::UNKNOWN_EVENT;
}

rtclog::VideoReceiveConfig_RtcpMode ConvertRtcpMode(
    newapi::RtcpMode rtcp_mode) {
  switch (rtcp_mode) {
    case newapi::kRtcpCompound:
      return rtclog::VideoReceiveConfig::RTCP_COMPOUND;
    case newapi::kRtcpReducedSize:
      return rtclog::VideoReceiveConfig::RTCP_REDUCEDSIZE;
  }
  RTC_NOTREACHED();
  return rtclog::VideoReceiveConfig::RTCP_COMPOUND;
}

rtclog::MediaType ConvertMediaType(MediaType media_type) {
  switch (media_type) {
    case MediaType::ANY:
      return rtclog::MediaType::ANY;
    case MediaType::AUDIO:
      return rtclog::MediaType::AUDIO;
    case MediaType::VIDEO:
      return rtclog::MediaType::VIDEO;
    case MediaType::DATA:
      return rtclog::MediaType::DATA;
  }
  RTC_NOTREACHED();
  return rtclog::ANY;
}

}  // namespace

// RtcEventLogImpl member functions.
RtcEventLogImpl::RtcEventLogImpl()
    : file_(FileWrapper::Create()),
      stream_(),
      currently_logging_(false),
      start_time_us_(0),
      duration_us_(0),
      clock_(Clock::GetRealTimeClock()) {
}

void RtcEventLogImpl::StartLogging(const std::string& file_name,
                                   int duration_ms) {
  rtc::CritScope lock(&crit_);
  if (currently_logging_) {
    StopLoggingLocked();
  }
  if (file_->OpenFile(file_name.c_str(), false) != 0) {
    return;
  }
  currently_logging_ = true;
  start_time_us_ = clock_->TimeInMicroseconds();
  duration_us_ = static_cast<int64_t>(duration_ms) * 1000;
  // Write all the recent events to the log file, ignoring any old events.
  for (auto& event : recent_log_events_) {
    if (event.timestamp_us() >= start_time_us_ - recent_log_duration_us) {
      StoreToFile(&event);
    }
  }
  recent_log_events_.clear();
  // Write a LOG_START event to the file.
  rtclog::Event start_event;
  start_event.set_timestamp_us(start_time_us_);
  start_event.set_type(rtclog::Event::DEBUG_EVENT);
  auto debug_event = start_event.mutable_debug_event();
  debug_event->set_type(ConvertDebugEvent(DebugEvent::kLogStart));
  StoreToFile(&start_event);
}

void RtcEventLogImpl::StopLogging() {
  rtc::CritScope lock(&crit_);
  StopLoggingLocked();
}

void RtcEventLogImpl::LogVideoReceiveStreamConfig(
    const VideoReceiveStream::Config& config) {
  rtc::CritScope lock(&crit_);

  rtclog::Event event;
  const int64_t timestamp = clock_->TimeInMicroseconds();
  event.set_timestamp_us(timestamp);
  event.set_type(rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT);

  rtclog::VideoReceiveConfig* receiver_config =
      event.mutable_video_receiver_config();
  receiver_config->set_remote_ssrc(config.rtp.remote_ssrc);
  receiver_config->set_local_ssrc(config.rtp.local_ssrc);

  receiver_config->set_rtcp_mode(ConvertRtcpMode(config.rtp.rtcp_mode));

  receiver_config->set_receiver_reference_time_report(
      config.rtp.rtcp_xr.receiver_reference_time_report);
  receiver_config->set_remb(config.rtp.remb);

  for (const auto& kv : config.rtp.rtx) {
    rtclog::RtxMap* rtx = receiver_config->add_rtx_map();
    rtx->set_payload_type(kv.first);
    rtx->mutable_config()->set_rtx_ssrc(kv.second.ssrc);
    rtx->mutable_config()->set_rtx_payload_type(kv.second.payload_type);
  }

  for (const auto& e : config.rtp.extensions) {
    rtclog::RtpHeaderExtension* extension =
        receiver_config->add_header_extensions();
    extension->set_name(e.name);
    extension->set_id(e.id);
  }

  for (const auto& d : config.decoders) {
    rtclog::DecoderConfig* decoder = receiver_config->add_decoders();
    decoder->set_name(d.payload_name);
    decoder->set_payload_type(d.payload_type);
  }
  // TODO(terelius): We should use a separate event queue for config events.
  // The current approach of storing the configuration together with the
  // RTP events causes the configuration information to be removed 10s
  // after the ReceiveStream is created.
  HandleEvent(&event);
}

void RtcEventLogImpl::LogVideoSendStreamConfig(
    const VideoSendStream::Config& config) {
  rtc::CritScope lock(&crit_);

  rtclog::Event event;
  const int64_t timestamp = clock_->TimeInMicroseconds();
  event.set_timestamp_us(timestamp);
  event.set_type(rtclog::Event::VIDEO_SENDER_CONFIG_EVENT);

  rtclog::VideoSendConfig* sender_config = event.mutable_video_sender_config();

  for (const auto& ssrc : config.rtp.ssrcs) {
    sender_config->add_ssrcs(ssrc);
  }

  for (const auto& e : config.rtp.extensions) {
    rtclog::RtpHeaderExtension* extension =
        sender_config->add_header_extensions();
    extension->set_name(e.name);
    extension->set_id(e.id);
  }

  for (const auto& rtx_ssrc : config.rtp.rtx.ssrcs) {
    sender_config->add_rtx_ssrcs(rtx_ssrc);
  }
  sender_config->set_rtx_payload_type(config.rtp.rtx.payload_type);

  sender_config->set_c_name(config.rtp.c_name);

  rtclog::EncoderConfig* encoder = sender_config->mutable_encoder();
  encoder->set_name(config.encoder_settings.payload_name);
  encoder->set_payload_type(config.encoder_settings.payload_type);

  // TODO(terelius): We should use a separate event queue for config events.
  // The current approach of storing the configuration together with the
  // RTP events causes the configuration information to be removed 10s
  // after the ReceiveStream is created.
  HandleEvent(&event);
}

void RtcEventLogImpl::LogRtpHeader(bool incoming,
                                   MediaType media_type,
                                   const uint8_t* header,
                                   size_t packet_length) {
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

  rtc::CritScope lock(&crit_);
  rtclog::Event rtp_event;
  const int64_t timestamp = clock_->TimeInMicroseconds();
  rtp_event.set_timestamp_us(timestamp);
  rtp_event.set_type(rtclog::Event::RTP_EVENT);
  rtp_event.mutable_rtp_packet()->set_incoming(incoming);
  rtp_event.mutable_rtp_packet()->set_type(ConvertMediaType(media_type));
  rtp_event.mutable_rtp_packet()->set_packet_length(packet_length);
  rtp_event.mutable_rtp_packet()->set_header(header, header_length);
  HandleEvent(&rtp_event);
}

void RtcEventLogImpl::LogRtcpPacket(bool incoming,
                                    MediaType media_type,
                                    const uint8_t* packet,
                                    size_t length) {
  rtc::CritScope lock(&crit_);
  rtclog::Event rtcp_event;
  const int64_t timestamp = clock_->TimeInMicroseconds();
  rtcp_event.set_timestamp_us(timestamp);
  rtcp_event.set_type(rtclog::Event::RTCP_EVENT);
  rtcp_event.mutable_rtcp_packet()->set_incoming(incoming);
  rtcp_event.mutable_rtcp_packet()->set_type(ConvertMediaType(media_type));
  rtcp_event.mutable_rtcp_packet()->set_packet_data(packet, length);
  HandleEvent(&rtcp_event);
}

void RtcEventLogImpl::LogDebugEvent(DebugEvent event_type) {
  rtc::CritScope lock(&crit_);
  rtclog::Event event;
  const int64_t timestamp = clock_->TimeInMicroseconds();
  event.set_timestamp_us(timestamp);
  event.set_type(rtclog::Event::DEBUG_EVENT);
  auto debug_event = event.mutable_debug_event();
  debug_event->set_type(ConvertDebugEvent(event_type));
  HandleEvent(&event);
}

void RtcEventLogImpl::StopLoggingLocked() {
  if (currently_logging_) {
    currently_logging_ = false;
    // Create a LogEnd debug event
    rtclog::Event event;
    int64_t timestamp = clock_->TimeInMicroseconds();
    event.set_timestamp_us(timestamp);
    event.set_type(rtclog::Event::DEBUG_EVENT);
    auto debug_event = event.mutable_debug_event();
    debug_event->set_type(ConvertDebugEvent(DebugEvent::kLogEnd));
    // Store the event and close the file
    RTC_DCHECK(file_->Open());
    StoreToFile(&event);
    file_->CloseFile();
  }
  RTC_DCHECK(!file_->Open());
  stream_.Clear();
}

void RtcEventLogImpl::HandleEvent(rtclog::Event* event) {
  if (currently_logging_) {
    if (clock_->TimeInMicroseconds() < start_time_us_ + duration_us_) {
      StoreToFile(event);
      return;
    }
    StopLoggingLocked();
  }
  AddRecentEvent(*event);
}

void RtcEventLogImpl::StoreToFile(rtclog::Event* event) {
  // Reuse the same object at every log event.
  if (stream_.stream_size() < 1) {
    stream_.add_stream();
  }
  RTC_DCHECK_EQ(stream_.stream_size(), 1);
  stream_.mutable_stream(0)->Swap(event);
  // TODO(terelius): Doesn't this create a new EventStream per event?
  // Is this guaranteed to work e.g. in future versions of protobuf?
  std::string dump_buffer;
  stream_.SerializeToString(&dump_buffer);
  file_->Write(dump_buffer.data(), dump_buffer.size());
}

void RtcEventLogImpl::AddRecentEvent(const rtclog::Event& event) {
  recent_log_events_.push_back(event);
  while (recent_log_events_.front().timestamp_us() <
         event.timestamp_us() - recent_log_duration_us) {
    recent_log_events_.pop_front();
  }
}

bool RtcEventLog::ParseRtcEventLog(const std::string& file_name,
                                   rtclog::EventStream* result) {
  char tmp_buffer[1024];
  int bytes_read = 0;
  rtc::scoped_ptr<FileWrapper> dump_file(FileWrapper::Create());
  if (dump_file->OpenFile(file_name.c_str(), true) != 0) {
    return false;
  }
  std::string dump_buffer;
  while ((bytes_read = dump_file->Read(tmp_buffer, sizeof(tmp_buffer))) > 0) {
    dump_buffer.append(tmp_buffer, bytes_read);
  }
  dump_file->CloseFile();
  return result->ParseFromString(dump_buffer);
}

#endif  // ENABLE_RTC_EVENT_LOG

// RtcEventLog member functions.
rtc::scoped_ptr<RtcEventLog> RtcEventLog::Create() {
  return rtc::scoped_ptr<RtcEventLog>(new RtcEventLogImpl());
}
}  // namespace webrtc
