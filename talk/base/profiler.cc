/*
 * libjingle
 * Copyright 2013, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/base/profiler.h"

#include <math.h>

#include "talk/base/timeutils.h"

namespace {

// When written to an ostream, FormattedTime chooses an appropriate scale and
// suffix for a time value given in seconds.
class FormattedTime {
 public:
  explicit FormattedTime(double t) : time_(t) {}
  double time() const { return time_; }
 private:
  double time_;
};

std::ostream& operator<<(std::ostream& stream, const FormattedTime& time) {
  if (time.time() < 1.0) {
    stream << (time.time() * 1000.0) << "ms";
  } else {
    stream << time.time() << 's';
  }
  return stream;
}

}  // namespace

namespace talk_base {

ProfilerEvent::ProfilerEvent()
    : total_time_(0.0),
      mean_(0.0),
      sum_of_squared_differences_(0.0),
      start_count_(0),
      event_count_(0) {
}

void ProfilerEvent::Start() {
  if (start_count_ == 0) {
    current_start_time_ = TimeNanos();
  }
  ++start_count_;
}

void ProfilerEvent::Stop() {
  uint64 stop_time = TimeNanos();
  --start_count_;
  ASSERT(start_count_ >= 0);
  if (start_count_ == 0) {
    double elapsed = static_cast<double>(stop_time - current_start_time_) /
        kNumNanosecsPerSec;
    total_time_ += elapsed;
    if (event_count_ == 0) {
      minimum_ = maximum_ = elapsed;
    } else {
      minimum_ = _min(minimum_, elapsed);
      maximum_ = _max(maximum_, elapsed);
    }
    // Online variance and mean algorithm: http://en.wikipedia.org/wiki/
    // Algorithms_for_calculating_variance#Online_algorithm
    ++event_count_;
    double delta = elapsed - mean_;
    mean_ = mean_ + delta / event_count_;
    sum_of_squared_differences_ += delta * (elapsed - mean_);
  }
}

double ProfilerEvent::standard_deviation() const {
    if (event_count_ <= 1) return 0.0;
    return sqrt(sum_of_squared_differences_ / (event_count_ - 1.0));
}

Profiler* Profiler::Instance() {
  LIBJINGLE_DEFINE_STATIC_LOCAL(Profiler, instance, ());
  return &instance;
}

void Profiler::StartEvent(const std::string& event_name) {
  events_[event_name].Start();
}

void Profiler::StopEvent(const std::string& event_name) {
  events_[event_name].Stop();
}

void Profiler::ReportToLog(const char* file, int line,
                           LoggingSeverity severity_to_use,
                           const std::string& event_prefix) {
  if (!LogMessage::Loggable(severity_to_use)) {
    return;
  }
  { // Output first line.
    LogMessage msg(file, line, severity_to_use);
    msg.stream() << "=== Profile report ";
    if (event_prefix.empty()) {
      msg.stream() << "(prefix: '" << event_prefix << "') ";
    }
    msg.stream() << "===";
  }
  typedef std::map<std::string, ProfilerEvent>::const_iterator iterator;
  for (iterator it = events_.begin(); it != events_.end(); ++it) {
    if (event_prefix.empty() || it->first.find(event_prefix) == 0) {
      LogMessage(file, line, severity_to_use).stream()
          << it->first << " " << it->second;
    }
  }
  LogMessage(file, line, severity_to_use).stream()
      << "=== End profile report ===";
}

void Profiler::ReportAllToLog(const char* file, int line,
                           LoggingSeverity severity_to_use) {
  ReportToLog(file, line, severity_to_use, "");
}

const ProfilerEvent* Profiler::GetEvent(const std::string& event_name) const {
  std::map<std::string, ProfilerEvent>::const_iterator it =
      events_.find(event_name);
  return (it == events_.end()) ? NULL : &it->second;
}

bool Profiler::Clear() {
  bool result = true;
  // Clear all events that aren't started.
  std::map<std::string, ProfilerEvent>::iterator it = events_.begin();
  while (it != events_.end()) {
    if (it->second.is_started()) {
      ++it;  // Can't clear started events.
      result = false;
    } else {
      events_.erase(it++);
    }
  }
  return result;
}

std::ostream& operator<<(std::ostream& stream,
                         const ProfilerEvent& profiler_event) {
  stream << "count=" << profiler_event.event_count()
         << " total=" << FormattedTime(profiler_event.total_time())
         << " mean=" << FormattedTime(profiler_event.mean())
         << " min=" << FormattedTime(profiler_event.minimum())
         << " max=" << FormattedTime(profiler_event.maximum())
         << " sd=" << profiler_event.standard_deviation();
  return stream;
}

}  // namespace talk_base
