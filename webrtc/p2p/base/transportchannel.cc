/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sstream>
#include "webrtc/p2p/base/common.h"
#include "webrtc/p2p/base/transportchannel.h"

namespace cricket {

std::string TransportChannel::ToString() const {
  const char READABLE_ABBREV[2] = { '_', 'R' };
  const char WRITABLE_ABBREV[2] = { '_', 'W' };
  std::stringstream ss;
  ss << "Channel[" << transport_name_ << "|" << component_ << "|"
     << READABLE_ABBREV[readable_] << WRITABLE_ABBREV[writable_] << "]";
  return ss.str();
}

void TransportChannel::set_readable(bool readable) {
  if (readable_ != readable) {
    readable_ = readable;
    SignalReadableState(this);
  }
}

void TransportChannel::set_receiving(bool receiving) {
  if (receiving_ == receiving) {
    return;
  }
  receiving_ = receiving;
  SignalReceivingState(this);
}

void TransportChannel::set_writable(bool writable) {
  if (writable_ != writable) {
    LOG_J(LS_VERBOSE, this) << "set_writable from:" << writable_ << " to "
                            << writable;
    writable_ = writable;
    if (writable_) {
      SignalReadyToSend(this);
    }
    SignalWritableState(this);
  }
}

}  // namespace cricket
