/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_API_STATS_OBSERVER_INTERFACE_H_
#define TEST_PC_E2E_API_STATS_OBSERVER_INTERFACE_H_

#include "absl/strings/string_view.h"
#include "api/stats_types.h"

namespace webrtc {
namespace test {

class StatsObserverInterface {
 public:
  virtual ~StatsObserverInterface() = default;

  // Method called when stats reports are available for the PeerConnection
  // identified by |pc_label|.
  virtual void OnStatsReports(absl::string_view pc_label,
                              const StatsReports& reports) = 0;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_API_STATS_OBSERVER_INTERFACE_H_
