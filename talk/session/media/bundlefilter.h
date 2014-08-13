/*
 * libjingle
 * Copyright 2004 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_SESSION_MEDIA_BUNDLEFILTER_H_
#define TALK_SESSION_MEDIA_BUNDLEFILTER_H_

#include <set>
#include <vector>

#include "talk/media/base/streamparams.h"
#include "webrtc/base/basictypes.h"

namespace cricket {

// In case of single RTP session and single transport channel, all session
// ( or media) channels share a common transport channel. Hence they all get
// SignalReadPacket when packet received on transport channel. This requires
// cricket::BaseChannel to know all the valid sources, else media channel
// will decode invalid packets.
//
// This class determines whether a packet is destined for cricket::BaseChannel.
// For rtp packets, this is decided based on the payload type. For rtcp packets,
// this is decided based on the sender ssrc values.
class BundleFilter {
 public:
  BundleFilter();
  ~BundleFilter();

  // Determines packet belongs to valid cricket::BaseChannel.
  bool DemuxPacket(const char* data, size_t len, bool rtcp);

  // Adds the supported payload type.
  void AddPayloadType(int payload_type);

  // Adding a valid source to the filter.
  bool AddStream(const StreamParams& stream);

  // Removes source from the filter.
  bool RemoveStream(uint32 ssrc);

  // Utility methods added for unitest.
  // True if |streams_| is not empty.
  bool HasStreams() const;
  bool FindStream(uint32 ssrc) const;
  bool FindPayloadType(int pl_type) const;
  void ClearAllPayloadTypes();


 private:
  std::set<int> payload_types_;
  std::vector<StreamParams> streams_;
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_BUNDLEFILTER_H_
