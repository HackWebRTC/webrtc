/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_SESSION_P2PTRANSPORTPARSER_H_
#define WEBRTC_LIBJINGLE_SESSION_P2PTRANSPORTPARSER_H_

#include <string>
#include "webrtc/libjingle/session/transportparser.h"

namespace cricket {

class P2PTransportParser : public TransportParser {
 public:
  P2PTransportParser() {}
  // Translator may be null, in which case ParseCandidates should
  // return false if there are candidates to parse.  We can't not call
  // ParseCandidates because there's no way to know ahead of time if
  // there are candidates or not.

  // Jingle-specific functions; can be used with either ICE, GICE, or HYBRID.
  virtual bool ParseTransportDescription(const buzz::XmlElement* elem,
                                         const CandidateTranslator* translator,
                                         TransportDescription* desc,
                                         ParseError* error);
  virtual bool WriteTransportDescription(const TransportDescription& desc,
                                         const CandidateTranslator* translator,
                                         buzz::XmlElement** elem,
                                         WriteError* error);

  // Legacy Gingle functions; only can be used with GICE.
  virtual bool ParseGingleCandidate(const buzz::XmlElement* elem,
                                    const CandidateTranslator* translator,
                                    Candidate* candidate,
                                    ParseError* error);
  virtual bool WriteGingleCandidate(const Candidate& candidate,
                                    const CandidateTranslator* translator,
                                    buzz::XmlElement** elem,
                                    WriteError* error);

 private:
  bool ParseCandidate(TransportProtocol proto,
                      const buzz::XmlElement* elem,
                      const CandidateTranslator* translator,
                      Candidate* candidate,
                      ParseError* error);
  bool WriteCandidate(TransportProtocol proto,
                      const Candidate& candidate,
                      const CandidateTranslator* translator,
                      buzz::XmlElement* elem,
                      WriteError* error);
  bool VerifyUsernameFormat(TransportProtocol proto,
                            const std::string& username,
                            ParseError* error);

  DISALLOW_EVIL_CONSTRUCTORS(P2PTransportParser);
};

}  // namespace cricket

#endif  // WEBRTC_LIBJINGLE_SESSION_P2PTRANSPORTPARSER_H_
