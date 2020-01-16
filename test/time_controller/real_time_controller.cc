/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/time_controller/real_time_controller.h"

#include "api/task_queue/default_task_queue_factory.h"
#include "rtc_base/null_socket_server.h"
#include "system_wrappers/include/sleep.h"

namespace webrtc {
RealTimeController::RealTimeController()
    : task_queue_factory_(CreateDefaultTaskQueueFactory()) {}

Clock* RealTimeController::GetClock() {
  return Clock::GetRealTimeClock();
}

TaskQueueFactory* RealTimeController::GetTaskQueueFactory() {
  return task_queue_factory_.get();
}

std::unique_ptr<ProcessThread> RealTimeController::CreateProcessThread(
    const char* thread_name) {
  return ProcessThread::Create(thread_name);
}

std::unique_ptr<rtc::Thread> RealTimeController::CreateThread(
    const std::string& name,
    std::unique_ptr<rtc::SocketServer> socket_server) {
  if (!socket_server)
    socket_server = std::make_unique<rtc::NullSocketServer>();
  auto res = std::make_unique<rtc::Thread>(std::move(socket_server));
  res->SetName(name, nullptr);
  res->Start();
  return res;
}

rtc::Thread* RealTimeController::GetMainThread() {
  return rtc::Thread::Current();
}

void RealTimeController::AdvanceTime(TimeDelta duration) {
  GetMainThread()->ProcessMessages(duration.ms());
}

RealTimeController* GlobalRealTimeController() {
  static RealTimeController* time_controller = new RealTimeController();
  return time_controller;
}

}  // namespace webrtc
