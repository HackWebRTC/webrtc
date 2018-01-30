/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_AUDIOMONITOR_H_
#define PC_AUDIOMONITOR_H_

#include <vector>
#include <utility>

// For ConnectionInfo/ConnectionInfos
#include "p2p/base/port.h"

namespace cricket {

struct AudioInfo {
  int input_level;
  int output_level;
  typedef std::vector<std::pair<uint32_t, int> > StreamList;
  StreamList active_streams;  // ssrcs contributing to output_level
};

}  // namespace cricket

#endif  // PC_AUDIOMONITOR_H_
