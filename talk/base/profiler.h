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

// A simple wall-clock profiler for instrumented code.
// Example:
//   void MyLongFunction() {
//     PROFILE_F();  // Time the execution of this function.
//     // Do something
//     {  // Time just what is in this scope.
//       PROFILE("My event");
//       // Do something else
//     }
//   }
// Another example:
//   void StartAsyncProcess() {
//     PROFILE_START("My async event");
//     DoSomethingAsyncAndThenCall(&Callback);
//   }
//   void Callback() {
//     PROFILE_STOP("My async event");
//     // Handle callback.
//   }

#ifndef TALK_BASE_PROFILER_H_
#define TALK_BASE_PROFILER_H_

#include <map>
#include <string>

#include "talk/base/basictypes.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/sharedexclusivelock.h"

// Profiling could be switched via a build flag, but for now, it's always on.
#define ENABLE_PROFILING

#ifdef ENABLE_PROFILING

#define UV_HELPER2(x) _uv_ ## x
#define UV_HELPER(x) UV_HELPER2(x)
#define UNIQUE_VAR UV_HELPER(__LINE__)

// Profiles the current scope.
#define PROFILE(msg) talk_base::ProfilerScope UNIQUE_VAR(msg)
// When placed at the start of a function, profiles the current function.
#define PROFILE_F() PROFILE(__FUNCTION__)
// Reports current timings to the log at severity |sev|.
#define PROFILE_DUMP_ALL(sev) \
  talk_base::Profiler::Instance()->ReportAllToLog(__FILE__, __LINE__, sev)
// Reports current timings for all events whose names are prefixed by |prefix|
// to the log at severity |sev|. Using a unique event name as |prefix| will
// report only that event.
#define PROFILE_DUMP(sev, prefix) \
  talk_base::Profiler::Instance()->ReportToLog(__FILE__, __LINE__, sev, prefix)
// Starts and stops a profile event. Useful when an event is not easily
// captured within a scope (eg, an async call with a callback when done).
#define PROFILE_START(msg) talk_base::Profiler::Instance()->StartEvent(msg)
#define PROFILE_STOP(msg) talk_base::Profiler::Instance()->StopEvent(msg)
// TODO(ryanpetrie): Consider adding PROFILE_DUMP_EVERY(sev, iterations)

#undef UV_HELPER2
#undef UV_HELPER
#undef UNIQUE_VAR

#else  // ENABLE_PROFILING

#define PROFILE(msg) (void)0
#define PROFILE_F() (void)0
#define PROFILE_DUMP_ALL(sev) (void)0
#define PROFILE_DUMP(sev, prefix) (void)0
#define PROFILE_START(msg) (void)0
#define PROFILE_STOP(msg) (void)0

#endif  // ENABLE_PROFILING

namespace talk_base {

// Tracks information for one profiler event.
class ProfilerEvent {
 public:
  ProfilerEvent();
  void Start();
  void Stop();
  void Stop(uint64 stop_time);
  double standard_deviation() const;
  double total_time() const { return total_time_; }
  double mean() const { return mean_; }
  double minimum() const { return minimum_; }
  double maximum() const { return maximum_; }
  int event_count() const { return event_count_; }
  bool is_started() const { return start_count_ > 0; }

 private:
  uint64 current_start_time_;
  double total_time_;
  double mean_;
  double sum_of_squared_differences_;
  double minimum_;
  double maximum_;
  int start_count_;
  int event_count_;
};

// Singleton that owns ProfilerEvents and reports results. Prefer to use
// macros, defined above, rather than directly calling Profiler methods.
class Profiler {
 public:
  void StartEvent(const std::string& event_name);
  void StopEvent(const std::string& event_name);
  void ReportToLog(const char* file, int line, LoggingSeverity severity_to_use,
                   const std::string& event_prefix);
  void ReportAllToLog(const char* file, int line,
                      LoggingSeverity severity_to_use);
  const ProfilerEvent* GetEvent(const std::string& event_name) const;
  // Clears all _stopped_ events. Returns true if _all_ events were cleared.
  bool Clear();

  static Profiler* Instance();
 private:
  Profiler() {}

  typedef std::map<std::string, ProfilerEvent> EventMap;
  EventMap events_;
  mutable SharedExclusiveLock lock_;

  DISALLOW_COPY_AND_ASSIGN(Profiler);
};

// Starts an event on construction and stops it on destruction.
// Used by PROFILE macro.
class ProfilerScope {
 public:
  explicit ProfilerScope(const std::string& event_name)
      : event_name_(event_name) {
    Profiler::Instance()->StartEvent(event_name_);
  }
  ~ProfilerScope() {
    Profiler::Instance()->StopEvent(event_name_);
  }
 private:
  std::string event_name_;

  DISALLOW_COPY_AND_ASSIGN(ProfilerScope);
};

std::ostream& operator<<(std::ostream& stream,
                         const ProfilerEvent& profiler_event);

}  // namespace talk_base

#endif  // TALK_BASE_PROFILER_H_
