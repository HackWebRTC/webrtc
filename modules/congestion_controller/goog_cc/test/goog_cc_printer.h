/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_TEST_GOOG_CC_PRINTER_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_TEST_GOOG_CC_PRINTER_H_

#include <deque>
#include <memory>
#include <string>

#include "api/transport/goog_cc_factory.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/congestion_controller/goog_cc/goog_cc_network_control.h"
#include "modules/congestion_controller/test/controller_printer.h"

namespace webrtc {

class FieldLogger {
 public:
  virtual ~FieldLogger() = default;
  virtual const std::string& name() const = 0;
  virtual void WriteValue(RtcEventLogOutput* out) = 0;
};

class GoogCcStatePrinter : public DebugStatePrinter {
 public:
  GoogCcStatePrinter();
  GoogCcStatePrinter(const GoogCcStatePrinter&) = delete;
  GoogCcStatePrinter& operator=(const GoogCcStatePrinter&) = delete;
  ~GoogCcStatePrinter() override;
  void Attach(GoogCcNetworkController*);
  bool Attached() const override;

  void PrintHeaders(RtcEventLogOutput* out) override;
  void PrintValues(RtcEventLogOutput* out) override;

  NetworkControlUpdate GetState(Timestamp at_time) const override;

 private:
  const NetworkStateEstimate& GetEst();
  std::deque<FieldLogger*> CreateLoggers();

  std::deque<std::unique_ptr<FieldLogger>> loggers_;
  GoogCcNetworkController* controller_ = nullptr;
};

class GoogCcDebugFactory : public GoogCcNetworkControllerFactory {
 public:
  explicit GoogCcDebugFactory(GoogCcStatePrinter* printer);
  std::unique_ptr<NetworkControllerInterface> Create(
      NetworkControllerConfig config) override;

 private:
  GoogCcStatePrinter* printer_;
  GoogCcNetworkController* controller_ = nullptr;
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_TEST_GOOG_CC_PRINTER_H_
