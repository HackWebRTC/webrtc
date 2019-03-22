/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_TIME_CONTROLLER_REAL_TIME_CONTROLLER_H_
#define TEST_TIME_CONTROLLER_REAL_TIME_CONTROLLER_H_

#include <memory>

#include "test/time_controller/time_controller.h"

namespace webrtc {
class RealTimeController : public TimeController {
 public:
  Clock* GetClock() override;
  TaskQueueFactory* GetTaskQueueFactory() override;
  std::unique_ptr<ProcessThread> CreateProcessThread(
      const char* thread_name) override;
  void Sleep(TimeDelta duration) override;
  void InvokeWithControlledYield(std::function<void()> closure) override;
};

}  // namespace webrtc

#endif  // TEST_TIME_CONTROLLER_REAL_TIME_CONTROLLER_H_
