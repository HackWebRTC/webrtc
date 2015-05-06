/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/tcp.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/common.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"

namespace webrtc {
namespace testing {
namespace bwe {

TcpBweReceiver::TcpBweReceiver(int flow_id)
    : BweReceiver(flow_id), last_feedback_ms_(0) {
}

TcpBweReceiver::~TcpBweReceiver() {
}

void TcpBweReceiver::ReceivePacket(int64_t arrival_time_ms,
                                   const MediaPacket& media_packet) {
  acks_.push_back(media_packet.header().sequenceNumber);
}

FeedbackPacket* TcpBweReceiver::GetFeedback(int64_t now_ms) {
  FeedbackPacket* fb =
      new TcpFeedback(flow_id_, now_ms * 1000, last_feedback_ms_, acks_);
  last_feedback_ms_ = now_ms;
  acks_.clear();
  return fb;
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
