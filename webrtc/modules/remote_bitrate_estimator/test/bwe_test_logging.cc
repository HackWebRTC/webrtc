/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_logging.h"

#if BWE_TEST_LOGGING_COMPILE_TIME_ENABLE

#include <algorithm>
#include <cstdarg>
#include <cstdio>

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"

namespace webrtc {
namespace testing {
namespace bwe {

Logging Logging::g_Logging;

Logging::Context::Context(uint32_t name, int64_t timestamp_ms, bool enabled) {
  const size_t kBufferSize = 16;
  char string_buffer[kBufferSize] = {0};
#if defined(_MSC_VER) && defined(_WIN32)
  _snprintf(string_buffer, kBufferSize - 1, "%08x", name);
#else
  snprintf(string_buffer, kBufferSize, "%08x", name);
#endif
  Logging::GetInstance()->PushState(string_buffer, timestamp_ms, enabled);
}

Logging::Context::Context(const std::string& name, int64_t timestamp_ms,
                          bool enabled) {
  Logging::GetInstance()->PushState(name.c_str(), timestamp_ms, enabled);
}

Logging::Context::Context(const char* name, int64_t timestamp_ms,
                          bool enabled) {
  Logging::GetInstance()->PushState(name, timestamp_ms, enabled);
}

Logging::Context::~Context() {
  Logging::GetInstance()->PopState();
}

Logging* Logging::GetInstance() {
  return &g_Logging;
}

void Logging::Log(const char format[], ...) {
  CriticalSectionScoped cs(crit_sect_.get());
  ThreadMap::iterator it = thread_map_.find(ThreadWrapper::GetThreadId());
  assert(it != thread_map_.end());
  const State& state = it->second.top();
  if (state.enabled) {
    printf("%s\t", state.tag.c_str());
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
  }
}

void Logging::Plot(double value) {
  CriticalSectionScoped cs(crit_sect_.get());
  ThreadMap::iterator it = thread_map_.find(ThreadWrapper::GetThreadId());
  assert(it != thread_map_.end());
  const State& state = it->second.top();
  if (state.enabled) {
    printf("PLOT\t%s\t%f\t%f\n", state.tag.c_str(), state.timestamp_ms * 0.001,
           value);
  }
}

Logging::Logging()
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      thread_map_() {
}

void Logging::PushState(const char append_to_tag[], int64_t timestamp_ms,
                        bool enabled) {
  assert(append_to_tag);
  CriticalSectionScoped cs(crit_sect_.get());
  std::stack<State>* stack = &thread_map_[ThreadWrapper::GetThreadId()];
  if (stack->empty()) {
    State new_state(append_to_tag, std::max(static_cast<int64_t>(0),
                                            timestamp_ms), enabled);
    stack->push(new_state);
  } else {
    stack->push(stack->top());
    State* state = &stack->top();
    if (state->tag != "" && std::string(append_to_tag) != "") {
      state->tag.append("_");
    }
    state->tag.append(append_to_tag);
    state->timestamp_ms = std::max(timestamp_ms, state->timestamp_ms);
    state->enabled = enabled && state->enabled;
  }
}

void Logging::PopState() {
  CriticalSectionScoped cs(crit_sect_.get());
  ThreadMap::iterator it = thread_map_.find(ThreadWrapper::GetThreadId());
  assert(it != thread_map_.end());
  int64_t newest_timestamp_ms = it->second.top().timestamp_ms;
  it->second.pop();
  if (it->second.empty()) {
    thread_map_.erase(it);
  } else {
    State* state = &it->second.top();
    // Update time so that next log/plot will use the latest time seen so far
    // in this call tree.
    state->timestamp_ms = std::max(state->timestamp_ms, newest_timestamp_ms);
  }
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // BWE_TEST_LOGGING_COMPILE_TIME_ENABLE
