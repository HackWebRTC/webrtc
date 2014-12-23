/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_SESSION_TRANSPORTPARSER_H_
#define WEBRTC_LIBJINGLE_SESSION_TRANSPORTPARSER_H_

#include <string>
#include <vector>

#include "webrtc/p2p/base/transportinfo.h"

namespace buzz {
class QName;
class XmlElement;
}

namespace cricket {

struct ParseError;
struct WriteError;
class CandidateTranslator;

typedef std::vector<buzz::XmlElement*> XmlElements;

class TransportParser {
 public:
  // The incoming Translator value may be null, in which case
  // ParseCandidates should return false if there are candidates to
  // parse (indicating a failure to parse).  If the Translator is null
  // and there are no candidates to parse, then return true,
  // indicating a successful parse of 0 candidates.

  // Parse or write a transport description, including ICE credentials and
  // any DTLS fingerprint. Since only Jingle has transport descriptions, these
  // functions are only used when serializing to Jingle.
  virtual bool ParseTransportDescription(const buzz::XmlElement* elem,
                                         const CandidateTranslator* translator,
                                         TransportDescription* tdesc,
                                         ParseError* error) = 0;
  virtual bool WriteTransportDescription(const TransportDescription& tdesc,
                                         const CandidateTranslator* translator,
                                         buzz::XmlElement** tdesc_elem,
                                         WriteError* error) = 0;


  // Parse a single candidate. This must be used when parsing Gingle
  // candidates, since there is no enclosing transport description.
  virtual bool ParseGingleCandidate(const buzz::XmlElement* elem,
                                    const CandidateTranslator* translator,
                                    Candidate* candidates,
                                    ParseError* error) = 0;
  virtual bool WriteGingleCandidate(const Candidate& candidate,
                                    const CandidateTranslator* translator,
                                    buzz::XmlElement** candidate_elem,
                                    WriteError* error) = 0;

  // Helper function to parse an element describing an address.  This
  // retrieves the IP and port from the given element and verifies
  // that they look like plausible values.
  bool ParseAddress(const buzz::XmlElement* elem,
                    const buzz::QName& address_name,
                    const buzz::QName& port_name,
                    rtc::SocketAddress* address,
                    ParseError* error);

  virtual ~TransportParser() {}
};

}  // namespace cricket

#endif  // WEBRTC_LIBJINGLE_SESSION_TRANSPORTPARSER_H_
