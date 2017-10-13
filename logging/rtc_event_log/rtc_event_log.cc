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
#include "logging/rtc_event_log/events/rtc_event_logging_started.h"
#include "logging/rtc_event_log/events/rtc_event_logging_stopped.h"
#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/safe_conversions.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

#ifdef ENABLE_RTC_EVENT_LOG

namespace {
constexpr size_t kMaxEventsInHistory = 10000;
// The config-history is supposed to be unbounded, but needs to have some bound
// to prevent an attack via unreasonable memory use.
constexpr size_t kMaxEventsInConfigHistory = 1000;

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

  // TODO(eladalon): We should change these name to reflect that what we're
  // actually starting/stopping is the output of the log, not the log itself.
  bool StartLogging(std::unique_ptr<RtcEventLogOutput> output) override;
  void StopLogging() override;

  void Log(std::unique_ptr<RtcEvent> event) override;

 private:
  // Appends an event to the output protobuf string, returning true on success.
  // Fails and returns false in case the limit on output size prevents the
  // event from being added; in this case, the output string is left unchanged.
  // The event is encoded before being appended.
  // We could have avoided this, because the output repeats the check, but this
  // way, we minimize the number of lock acquisitions, task switches, etc.,
  // that might be associated with each call to RtcEventLogOutput::Write().
  bool AppendEventToString(const RtcEvent& event,
                           std::string* output_string) RTC_WARN_UNUSED_RESULT;

  void LogToMemory(std::unique_ptr<RtcEvent> event) RTC_RUN_ON(&task_queue_);

  void LogEventsFromMemoryToOutput() RTC_RUN_ON(&task_queue_);
  void LogToOutput(std::unique_ptr<RtcEvent> event) RTC_RUN_ON(&task_queue_);
  void StopOutput() RTC_RUN_ON(&task_queue_);

  void WriteToOutput(const std::string& output_string) RTC_RUN_ON(&task_queue_);

  void StopLoggingInternal() RTC_RUN_ON(&task_queue_);

  // Make sure that the event log is "managed" - created/destroyed, as well
  // as started/stopped - from the same thread/task-queue.
  rtc::SequencedTaskChecker owner_sequence_checker_;

  // History containing all past configuration events.
  std::deque<std::unique_ptr<RtcEvent>> config_history_
      RTC_ACCESS_ON(task_queue_);

  // History containing the most recent (non-configuration) events (~10s).
  std::deque<std::unique_ptr<RtcEvent>> history_ RTC_ACCESS_ON(task_queue_);

  size_t max_size_bytes_ RTC_ACCESS_ON(task_queue_);
  size_t written_bytes_ RTC_ACCESS_ON(task_queue_);

  std::unique_ptr<RtcEventLogEncoder> event_encoder_ RTC_ACCESS_ON(task_queue_);
  std::unique_ptr<RtcEventLogOutput> event_output_ RTC_ACCESS_ON(task_queue_);

  // Keep this last to ensure it destructs first, or else tasks living on the
  // queue might access other members after they've been torn down.
  rtc::TaskQueue task_queue_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtcEventLogImpl);
};

RtcEventLogImpl::RtcEventLogImpl(
    std::unique_ptr<RtcEventLogEncoder> event_encoder)
    : max_size_bytes_(std::numeric_limits<decltype(max_size_bytes_)>::max()),
      written_bytes_(0),
      event_encoder_(std::move(event_encoder)),
      task_queue_("rtc_event_log") {}

RtcEventLogImpl::~RtcEventLogImpl() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&owner_sequence_checker_);

  // If we're logging to the output, this will stop that. Blocking function.
  StopLogging();

  int count = std::atomic_fetch_sub(&rtc_event_log_count, 1) - 1;
  RTC_DCHECK_GE(count, 0);
}

bool RtcEventLogImpl::StartLogging(std::unique_ptr<RtcEventLogOutput> output) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&owner_sequence_checker_);

  if (!output->IsActive()) {
    return false;
  }

  LOG(LS_INFO) << "Starting WebRTC event log.";

  // |start_event| captured by value. This is done here because we want the
  // timestamp to reflect when StartLogging() was called; not the queueing
  // delay of the TaskQueue.
  // This is a bit inefficient - especially since we copy again to get it
  // to comply with LogToOutput()'s signature - but it's a small problem.
  RtcEventLoggingStarted start_event;

  auto start = [this, start_event](std::unique_ptr<RtcEventLogOutput> output) {
    RTC_DCHECK_RUN_ON(&task_queue_);
    RTC_DCHECK(output->IsActive());
    event_output_ = std::move(output);
    LogToOutput(rtc::MakeUnique<RtcEventLoggingStarted>(start_event));
    LogEventsFromMemoryToOutput();
  };

  task_queue_.PostTask(rtc::MakeUnique<ResourceOwningTask<RtcEventLogOutput>>(
      std::move(output), start));

  return true;
}

void RtcEventLogImpl::StopLogging() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&owner_sequence_checker_);

  LOG(LS_INFO) << "Stopping WebRTC event log.";

  rtc::Event output_stopped(true, false);

  task_queue_.PostTask([this, &output_stopped]() {
    RTC_DCHECK_RUN_ON(&task_queue_);
    StopLoggingInternal();
    output_stopped.Set();
  });

  output_stopped.Wait(rtc::Event::kForever);

  LOG(LS_INFO) << "WebRTC event log successfully stopped.";
}

void RtcEventLogImpl::Log(std::unique_ptr<RtcEvent> event) {
  RTC_DCHECK(event);

  auto event_handler = [this](std::unique_ptr<RtcEvent> unencoded_event) {
    RTC_DCHECK_RUN_ON(&task_queue_);
    if (event_output_) {
      LogToOutput(std::move(unencoded_event));
    } else {
      LogToMemory(std::move(unencoded_event));
    }
  };

  task_queue_.PostTask(rtc::MakeUnique<ResourceOwningTask<RtcEvent>>(
      std::move(event), event_handler));
}

bool RtcEventLogImpl::AppendEventToString(const RtcEvent& event,
                                          std::string* output_string) {
  RTC_DCHECK_RUN_ON(&task_queue_);

  std::string encoded_event = event_encoder_->Encode(event);

  bool appended;
  size_t potential_new_size =
      written_bytes_ + output_string->size() + encoded_event.length();
  if (potential_new_size <= max_size_bytes_) {
    // TODO(eladalon): This is inefficient; fix this in a separate CL.
    *output_string += encoded_event;
    appended = true;
  } else {
    appended = false;
  }

  return appended;
}

void RtcEventLogImpl::LogToMemory(std::unique_ptr<RtcEvent> event) {
  RTC_DCHECK(!event_output_);

  std::deque<std::unique_ptr<RtcEvent>>& container =
      event->IsConfigEvent() ? config_history_ : history_;
  const size_t container_max_size =
      event->IsConfigEvent() ? kMaxEventsInHistory : kMaxEventsInConfigHistory;

  if (container.size() >= container_max_size) {
    container.pop_front();
  }
  container.push_back(std::move(event));
}

void RtcEventLogImpl::LogEventsFromMemoryToOutput() {
  RTC_DCHECK(event_output_ && event_output_->IsActive());

  std::string output_string;

  // Serialize the config information for all old streams, including streams
  // which were already logged to previous outputs.
  bool appended = true;
  for (auto& event : config_history_) {
    if (!AppendEventToString(*event, &output_string)) {
      appended = false;
      break;
    }
  }

  // Serialize the events in the event queue.
  while (appended && !history_.empty()) {
    appended = AppendEventToString(*history_.front(), &output_string);
    if (appended) {
      // Known issue - if writing to the output fails, these events will have
      // been lost. If we try to open a new output, these events will be missing
      // from it.
      history_.pop_front();
    }
  }

  WriteToOutput(output_string);

  if (!appended) {
    // Successful partial write to the output. Some events could not be written;
    // the output should be closed, to avoid gaps.
    StopOutput();
  }
}

void RtcEventLogImpl::LogToOutput(std::unique_ptr<RtcEvent> event) {
  RTC_DCHECK(event_output_ && event_output_->IsActive());

  std::string output_string;

  bool appended = AppendEventToString(*event, &output_string);

  if (event->IsConfigEvent()) {
    // Config events need to be kept in memory too, so that they may be
    // rewritten into future outputs, too.
    config_history_.push_back(std::move(event));
  }

  if (!appended) {
    if (!event->IsConfigEvent()) {
      // This event will not fit into the output; push it into |history_|
      // instead, so that it might be logged into the next output (if any).
      history_.push_back(std::move(event));
    }
    StopOutput();
    return;
  }

  WriteToOutput(output_string);
}

void RtcEventLogImpl::StopOutput() {
  max_size_bytes_ = std::numeric_limits<decltype(max_size_bytes_)>::max();
  written_bytes_ = 0;
  event_output_.reset();
}

void RtcEventLogImpl::StopLoggingInternal() {
  if (event_output_) {
    RTC_DCHECK(event_output_->IsActive());
    event_output_->Write(
        event_encoder_->Encode(*rtc::MakeUnique<RtcEventLoggingStopped>()));
  }
  StopOutput();
}

void RtcEventLogImpl::WriteToOutput(const std::string& output_string) {
  RTC_DCHECK(event_output_ && event_output_->IsActive());
  if (!event_output_->Write(output_string)) {
    LOG(LS_ERROR) << "Failed to write RTC event to output.";
    // The first failure closes the output.
    RTC_DCHECK(!event_output_->IsActive());
    StopOutput();  // Clean-up.
    return;
  }
  written_bytes_ += output_string.size();
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
