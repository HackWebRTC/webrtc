/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_PACKETTRANSPORTINTERFACE_H_
#define WEBRTC_P2P_BASE_PACKETTRANSPORTINTERFACE_H_

namespace cricket {
class TransportChannel;
}

namespace rtc {
typedef cricket::TransportChannel PacketTransportInterface;
}

#endif  // WEBRTC_P2P_BASE_PACKETTRANSPORTINTERFACE_H_
