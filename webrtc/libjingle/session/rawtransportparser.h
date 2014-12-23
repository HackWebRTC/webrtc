/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_SESSION_RAWTRANSPORTPARSER_H_
#define WEBRTC_LIBJINGLE_SESSION_RAWTRANSPORTPARSER_H_

#include <string>

#include "webrtc/p2p/base/constants.h"
#include "webrtc/libjingle/session/constants.h"
#include "webrtc/libjingle/session/transportparser.h"

namespace cricket {

class RawTransportParser : public TransportParser {
 public:
  RawTransportParser() {}

  virtual bool ParseCandidates(SignalingProtocol protocol,
                               const buzz::XmlElement* elem,
                               const CandidateTranslator* translator,
                               Candidates* candidates,
                               ParseError* error);
  virtual bool WriteCandidates(SignalingProtocol protocol,
                               const Candidates& candidates,
                               const CandidateTranslator* translator,
                               XmlElements* candidate_elems,
                               WriteError* error);
 private:
  // Parses the given element, which should describe the address to use for a
  // given channel.  This will return false and signal an error if the address
  // or channel name is bad.
  bool ParseRawAddress(const buzz::XmlElement* elem,
                       rtc::SocketAddress* addr,
                       ParseError* error);

  DISALLOW_EVIL_CONSTRUCTORS(RawTransportParser);
};

}  // namespace cricket

#endif  // WEBRTC_LIBJINGLE_SESSION_RAWTRANSPORTPARSER_H_
