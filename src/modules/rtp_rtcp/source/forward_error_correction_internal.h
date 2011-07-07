/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "typedefs.h"

namespace webrtc {

// Packet mask size in bytes (L bit is set).
const WebRtc_UWord8 kMaskSizeLBitSet = 6;
// Packet mask size in bytes (L bit is cleared).
const WebRtc_UWord8 kMaskSizeLBitClear = 2;

namespace internal {

 /**
  * Returns an array of packet masks. The mask of a single FEC packet
  * corresponds to a number of mask bytes. The mask indicates which
  * media packets should be protected by the FEC packet.

  * \param[in]  numMediaPackets The number of media packets to protect.
  *                              [1, maxMediaPackets].
  * \param[in]  numFecPackets   The number of FEC packets which will be generated.
  *                              [1, numMediaPackets].
  * \param[in]  numImpPackets   The number of important packets.
  *                              [0, numMediaPackets].
  *                             numImpPackets = 0 is the equal protection scenario.
  * \param[out] packetMask      A pointer to hold the packet mask array, of size
  *                              numFecPackets * "number of mask bytes".
  */
void GeneratePacketMasks(const WebRtc_UWord32 numMediaPackets,
                         const WebRtc_UWord32 numFecPackets,
                         const WebRtc_UWord32 numImpPackets,
                         WebRtc_UWord8* packetMask);


} // namespace internal
} // namespace webrtc
