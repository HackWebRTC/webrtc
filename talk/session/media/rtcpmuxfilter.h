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

#ifndef TALK_SESSION_MEDIA_RTCPMUXFILTER_H_
#define TALK_SESSION_MEDIA_RTCPMUXFILTER_H_

#include "webrtc/p2p/base/sessiondescription.h"
#include "webrtc/base/basictypes.h"

namespace cricket {

// RTCP Muxer, as defined in RFC 5761 (http://tools.ietf.org/html/rfc5761)
class RtcpMuxFilter {
 public:
  RtcpMuxFilter();

  // Whether the filter is active, i.e. has RTCP mux been properly negotiated.
  bool IsActive() const;

  // Make the filter active, regardless of the current state.
  void SetActive();

  // Specifies whether the offer indicates the use of RTCP mux.
  bool SetOffer(bool offer_enable, ContentSource src);

  // Specifies whether the provisional answer indicates the use of RTCP mux.
  bool SetProvisionalAnswer(bool answer_enable, ContentSource src);

  // Specifies whether the answer indicates the use of RTCP mux.
  bool SetAnswer(bool answer_enable, ContentSource src);

  // Determines whether the specified packet is RTCP.
  bool DemuxRtcp(const char* data, int len);

 private:
  bool ExpectOffer(bool offer_enable, ContentSource source);
  bool ExpectAnswer(ContentSource source);
  enum State {
    // RTCP mux filter unused.
    ST_INIT,
    // Offer with RTCP mux enabled received.
    // RTCP mux filter is not active.
    ST_RECEIVEDOFFER,
    // Offer with RTCP mux enabled sent.
    // RTCP mux filter can demux incoming packets but is not active.
    ST_SENTOFFER,
    // RTCP mux filter is active but the sent answer is only provisional.
    // When the final answer is set, the state transitions to ST_ACTIVE or
    // ST_INIT.
    ST_SENTPRANSWER,
    // RTCP mux filter is active but the received answer is only provisional.
    // When the final answer is set, the state transitions to ST_ACTIVE or
    // ST_INIT.
    ST_RECEIVEDPRANSWER,
    // Offer and answer set, RTCP mux enabled. It is not possible to de-activate
    // the filter.
    ST_ACTIVE
  };
  State state_;
  bool offer_enable_;
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_RTCPMUXFILTER_H_
