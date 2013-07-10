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

#ifndef TALK_SESSION_MEDIA_SSRCMUXFILTER_H_
#define TALK_SESSION_MEDIA_SSRCMUXFILTER_H_

#include <vector>

#include "talk/base/basictypes.h"
#include "talk/media/base/streamparams.h"

namespace cricket {

// This class maintains list of recv SSRC's destined for cricket::BaseChannel.
// In case of single RTP session and single transport channel, all session
// ( or media) channels share a common transport channel. Hence they all get
// SignalReadPacket when packet received on transport channel. This requires
// cricket::BaseChannel to know all the valid sources, else media channel
// will decode invalid packets.
class SsrcMuxFilter {
 public:
  SsrcMuxFilter();
  ~SsrcMuxFilter();

  // Whether the rtp mux is active for a sdp session.
  // Returns true if the filter contains a stream.
  bool IsActive() const;
  // Determines packet belongs to valid cricket::BaseChannel.
  bool DemuxPacket(const char* data, size_t len, bool rtcp);
  // Adding a valid source to the filter.
  bool AddStream(const StreamParams& stream);
  // Removes source from the filter.
  bool RemoveStream(uint32 ssrc);
  // Utility method added for unitest.
  bool FindStream(uint32 ssrc) const;

 private:
  std::vector<StreamParams> streams_;
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_SSRCMUXFILTER_H_
