/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_UNIT_OPERATORS_H_
#define MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_UNIT_OPERATORS_H_

#include "modules/congestion_controller/network_control/units/data_rate.h"
#include "modules/congestion_controller/network_control/units/data_size.h"
#include "modules/congestion_controller/network_control/units/time_delta.h"

namespace webrtc {
DataRate operator/(const DataSize& size, const TimeDelta& duration);
TimeDelta operator/(const DataSize& size, const DataRate& rate);
DataSize operator*(const DataRate& rate, const TimeDelta& duration);
DataSize operator*(const TimeDelta& duration, const DataRate& rate);
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_NETWORK_CONTROL_UNITS_UNIT_OPERATORS_H_
