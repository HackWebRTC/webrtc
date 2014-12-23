/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/session/rawtransportparser.h"

#include <string>
#include <vector>

#include "webrtc/libjingle/session/parsing.h"
#include "webrtc/libjingle/xmllite/qname.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/p2p/base/constants.h"

#if defined(FEATURE_ENABLE_PSTN)
namespace cricket {

bool RawTransportParser::ParseCandidates(SignalingProtocol protocol,
                                         const buzz::XmlElement* elem,
                                         const CandidateTranslator* translator,
                                         Candidates* candidates,
                                         ParseError* error) {
  for (const buzz::XmlElement* cand_elem = elem->FirstElement();
       cand_elem != NULL;
       cand_elem = cand_elem->NextElement()) {
    if (cand_elem->Name() == QN_GINGLE_RAW_CHANNEL) {
      if (!cand_elem->HasAttr(buzz::QN_NAME)) {
        return BadParse("no channel name given", error);
      }
      if (NS_GINGLE_RAW != cand_elem->Attr(buzz::QN_NAME)) {
        return BadParse("channel named does not exist", error);
      }
      rtc::SocketAddress addr;
      if (!ParseRawAddress(cand_elem, &addr, error))
        return false;

      Candidate candidate;
      candidate.set_component(1);
      candidate.set_address(addr);
      candidates->push_back(candidate);
    }
  }
  return true;
}

bool RawTransportParser::WriteCandidates(SignalingProtocol protocol,
                                         const Candidates& candidates,
                                         const CandidateTranslator* translator,
                                         XmlElements* candidate_elems,
                                         WriteError* error) {
  for (std::vector<Candidate>::const_iterator
       cand = candidates.begin();
       cand != candidates.end();
       ++cand) {
    ASSERT(cand->component() == 1);
    ASSERT(cand->protocol() == "udp");
    rtc::SocketAddress addr = cand->address();

    buzz::XmlElement* elem = new buzz::XmlElement(QN_GINGLE_RAW_CHANNEL);
    elem->SetAttr(buzz::QN_NAME, NS_GINGLE_RAW);
    elem->SetAttr(QN_ADDRESS, addr.ipaddr().ToString());
    elem->SetAttr(QN_PORT, addr.PortAsString());
    candidate_elems->push_back(elem);
  }
  return true;
}

bool RawTransportParser::ParseRawAddress(const buzz::XmlElement* elem,
                                         rtc::SocketAddress* addr,
                                         ParseError* error) {
  // Make sure the required attributes exist
  if (!elem->HasAttr(QN_ADDRESS) ||
      !elem->HasAttr(QN_PORT)) {
    return BadParse("channel missing required attribute", error);
  }

  // Parse the address.
  if (!ParseAddress(elem, QN_ADDRESS, QN_PORT, addr, error))
    return false;

  return true;
}

}  // namespace cricket
#endif  // defined(FEATURE_ENABLE_PSTN)
