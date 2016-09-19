/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/stats/test/rtcteststats.h"

namespace webrtc {

const char RTCTestStats::kType[] = "test-stats";

RTCTestStats::RTCTestStats(const std::string& id, int64_t timestamp_us)
    : RTCStats(id, timestamp_us),
      m_int32("mInt32"),
      m_uint32("mUint32"),
      m_int64("mInt64"),
      m_uint64("mUint64"),
      m_double("mDouble"),
      m_string("mString"),
      m_sequence_int32("mSequenceInt32"),
      m_sequence_uint32("mSequenceUint32"),
      m_sequence_int64("mSequenceInt64"),
      m_sequence_uint64("mSequenceUint64"),
      m_sequence_double("mSequenceDouble"),
      m_sequence_string("mSequenceString") {
}

}  // namespace webrtc
