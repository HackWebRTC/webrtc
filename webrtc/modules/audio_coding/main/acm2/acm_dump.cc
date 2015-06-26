/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/acm2/acm_dump.h"

#include <deque>

#include "webrtc/base/checks.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/file_wrapper.h"

// Files generated at build-time by the protobuf compiler.
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_coding/dump.pb.h"
#else
#include "webrtc/audio_coding/dump.pb.h"
#endif

namespace webrtc {

// Noop implementation if flag is not set
#ifndef RTC_AUDIOCODING_DEBUG_DUMP
class AcmDumpImpl final : public AcmDump {
 public:
  void StartLogging(const std::string& file_name, int duration_ms) override{};
  void LogRtpPacket(bool incoming,
                    const uint8_t* packet,
                    size_t length) override{};
  void LogDebugEvent(DebugEvent event_type,
                     const std::string& event_message) override{};
  void LogDebugEvent(DebugEvent event_type) override{};
};
#else

class AcmDumpImpl final : public AcmDump {
 public:
  AcmDumpImpl();

  void StartLogging(const std::string& file_name, int duration_ms) override;
  void LogRtpPacket(bool incoming,
                    const uint8_t* packet,
                    size_t length) override;
  void LogDebugEvent(DebugEvent event_type,
                     const std::string& event_message) override;
  void LogDebugEvent(DebugEvent event_type) override;

 private:
  // This function is identical to LogDebugEvent, but requires holding the lock.
  void LogDebugEventLocked(DebugEvent event_type,
                           const std::string& event_message)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);
  // Stops logging and clears the stored data and buffers.
  void Clear() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  // Adds a new event to the logfile if logging is active, or adds it to the
  // list of recent log events otherwise.
  void HandleEvent(ACMDumpEvent* event) EXCLUSIVE_LOCKS_REQUIRED(crit_);
  // Writes the event to the file. Note that this will destroy the state of the
  // input argument.
  void StoreToFile(ACMDumpEvent* event) EXCLUSIVE_LOCKS_REQUIRED(crit_);
  // Adds the event to the list of recent events, and removes any events that
  // are too old and no longer fall in the time window.
  void AddRecentEvent(const ACMDumpEvent& event)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Amount of time in microseconds to record log events, before starting the
  // actual log.
  const int recent_log_duration_us = 10000000;

  rtc::scoped_ptr<webrtc::CriticalSectionWrapper> crit_;
  rtc::scoped_ptr<webrtc::FileWrapper> file_ GUARDED_BY(crit_);
  rtc::scoped_ptr<ACMDumpEventStream> stream_ GUARDED_BY(crit_);
  std::deque<ACMDumpEvent> recent_log_events_ GUARDED_BY(crit_);
  bool currently_logging_ GUARDED_BY(crit_);
  int64_t start_time_us_ GUARDED_BY(crit_);
  int64_t duration_us_ GUARDED_BY(crit_);
  const webrtc::Clock* const clock_;
};

namespace {

// Convert from AcmDump's debug event enum (runtime format) to the corresponding
// protobuf enum (serialized format).
ACMDumpDebugEvent_EventType convertDebugEvent(AcmDump::DebugEvent event_type) {
  switch (event_type) {
    case AcmDump::DebugEvent::kLogStart:
      return ACMDumpDebugEvent::LOG_START;
    case AcmDump::DebugEvent::kLogEnd:
      return ACMDumpDebugEvent::LOG_END;
    case AcmDump::DebugEvent::kAudioPlayout:
      return ACMDumpDebugEvent::AUDIO_PLAYOUT;
  }
  return ACMDumpDebugEvent::UNKNOWN_EVENT;
}

}  // Anonymous namespace.

// AcmDumpImpl member functions.
AcmDumpImpl::AcmDumpImpl()
    : crit_(webrtc::CriticalSectionWrapper::CreateCriticalSection()),
      file_(webrtc::FileWrapper::Create()),
      stream_(new webrtc::ACMDumpEventStream()),
      currently_logging_(false),
      start_time_us_(0),
      duration_us_(0),
      clock_(webrtc::Clock::GetRealTimeClock()) {
}

void AcmDumpImpl::StartLogging(const std::string& file_name, int duration_ms) {
  CriticalSectionScoped lock(crit_.get());
  Clear();
  if (file_->OpenFile(file_name.c_str(), false) != 0) {
    return;
  }
  // Add LOG_START event to the recent event list. This call will also remove
  // any events that are too old from the recent event list.
  LogDebugEventLocked(DebugEvent::kLogStart, "");
  currently_logging_ = true;
  start_time_us_ = clock_->TimeInMicroseconds();
  duration_us_ = static_cast<int64_t>(duration_ms) * 1000;
  // Write all the recent events to the log file.
  for (auto&& event : recent_log_events_) {
    StoreToFile(&event);
  }
  recent_log_events_.clear();
}

void AcmDumpImpl::LogRtpPacket(bool incoming,
                               const uint8_t* packet,
                               size_t length) {
  CriticalSectionScoped lock(crit_.get());
  ACMDumpEvent rtp_event;
  const int64_t timestamp = clock_->TimeInMicroseconds();
  rtp_event.set_timestamp_us(timestamp);
  rtp_event.set_type(webrtc::ACMDumpEvent::RTP_EVENT);
  rtp_event.mutable_packet()->set_direction(
      incoming ? ACMDumpRTPPacket::INCOMING : ACMDumpRTPPacket::OUTGOING);
  rtp_event.mutable_packet()->set_rtp_data(packet, length);
  HandleEvent(&rtp_event);
}

void AcmDumpImpl::LogDebugEvent(DebugEvent event_type,
                                const std::string& event_message) {
  CriticalSectionScoped lock(crit_.get());
  LogDebugEventLocked(event_type, event_message);
}

void AcmDumpImpl::LogDebugEvent(DebugEvent event_type) {
  CriticalSectionScoped lock(crit_.get());
  LogDebugEventLocked(event_type, "");
}

void AcmDumpImpl::LogDebugEventLocked(DebugEvent event_type,
                                      const std::string& event_message) {
  ACMDumpEvent event;
  int64_t timestamp = clock_->TimeInMicroseconds();
  event.set_timestamp_us(timestamp);
  event.set_type(webrtc::ACMDumpEvent::DEBUG_EVENT);
  auto debug_event = event.mutable_debug_event();
  debug_event->set_type(convertDebugEvent(event_type));
  debug_event->set_message(event_message);
  HandleEvent(&event);
}

void AcmDumpImpl::Clear() {
  if (file_->Open()) {
    file_->CloseFile();
  }
  currently_logging_ = false;
  stream_->Clear();
}

void AcmDumpImpl::HandleEvent(ACMDumpEvent* event) {
  if (currently_logging_) {
    if (clock_->TimeInMicroseconds() < start_time_us_ + duration_us_) {
      StoreToFile(event);
    } else {
      LogDebugEventLocked(DebugEvent::kLogEnd, "");
      Clear();
      AddRecentEvent(*event);
    }
  } else {
    AddRecentEvent(*event);
  }
}

void AcmDumpImpl::StoreToFile(ACMDumpEvent* event) {
  // Reuse the same object at every log event.
  if (stream_->stream_size() < 1) {
    stream_->add_stream();
  }
  DCHECK_EQ(stream_->stream_size(), 1);
  stream_->mutable_stream(0)->Swap(event);

  std::string dump_buffer;
  stream_->SerializeToString(&dump_buffer);
  file_->Write(dump_buffer.data(), dump_buffer.size());
}

void AcmDumpImpl::AddRecentEvent(const ACMDumpEvent& event) {
  recent_log_events_.push_back(event);
  while (recent_log_events_.front().timestamp_us() <
         event.timestamp_us() - recent_log_duration_us) {
    recent_log_events_.pop_front();
  }
}

#endif  // RTC_AUDIOCODING_DEBUG_DUMP

// AcmDump member functions.
rtc::scoped_ptr<AcmDump> AcmDump::Create() {
  return rtc::scoped_ptr<AcmDump>(new AcmDumpImpl());
}

bool AcmDump::ParseAcmDump(const std::string& file_name,
                           ACMDumpEventStream* result) {
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

}  // namespace webrtc
