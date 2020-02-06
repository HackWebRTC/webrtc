/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_PLAYOUT_DELAY_ORACLE_H_
#define MODULES_RTP_RTCP_SOURCE_PLAYOUT_DELAY_ORACLE_H_

namespace webrtc {

// TODO(sprang): Remove once downstream usage is gone.
class PlayoutDelayOracle {
 public:
  PlayoutDelayOracle() = default;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_PLAYOUT_DELAY_ORACLE_H_
