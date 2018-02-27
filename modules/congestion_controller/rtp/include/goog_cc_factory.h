/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_RTP_INCLUDE_GOOG_CC_FACTORY_H_
#define MODULES_CONGESTION_CONTROLLER_RTP_INCLUDE_GOOG_CC_FACTORY_H_
#include "modules/congestion_controller/rtp/network_control/include/network_control.h"

namespace webrtc {
class Clock;
class RtcEventLog;

class GoogCcNetworkControllerFactory
    : public NetworkControllerFactoryInterface {
 public:
  explicit GoogCcNetworkControllerFactory(RtcEventLog*);
  NetworkControllerInterface::uptr Create(
      NetworkControllerObserver* observer) override;
  TimeDelta GetProcessInterval() const override;

 private:
  RtcEventLog* const event_log_;
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_RTP_INCLUDE_GOOG_CC_FACTORY_H_
