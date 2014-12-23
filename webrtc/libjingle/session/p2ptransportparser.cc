/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/libjingle/session/p2ptransportparser.h"

#include <vector>

#include "webrtc/p2p/base/constants.h"
#include "webrtc/libjingle/session/constants.h"
#include "webrtc/libjingle/session/parsing.h"
#include "webrtc/libjingle/session/sessionmanager.h"
#include "webrtc/libjingle/session/sessionmessages.h"
#include "webrtc/libjingle/xmllite/qname.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/base/base64.h"
#include "webrtc/base/common.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"

namespace cricket {

static buzz::XmlElement* NewTransportElement(const std::string& name) {
  return new buzz::XmlElement(buzz::QName(name, LN_TRANSPORT), true);
}

bool P2PTransportParser::ParseTransportDescription(
    const buzz::XmlElement* elem,
    const CandidateTranslator* translator,
    TransportDescription* desc,
    ParseError* error) {
  ASSERT(elem->Name().LocalPart() == LN_TRANSPORT);
  desc->transport_type = elem->Name().Namespace();
  if (desc->transport_type != NS_GINGLE_P2P)
    return BadParse("Unsupported transport type", error);

  for (const buzz::XmlElement* candidate_elem = elem->FirstElement();
       candidate_elem != NULL;
       candidate_elem = candidate_elem->NextElement()) {
    // Only look at local part because the namespace might (eventually)
    // be NS_GINGLE_P2P or NS_JINGLE_ICE_UDP.
    if (candidate_elem->Name().LocalPart() == LN_CANDIDATE) {
      Candidate candidate;
      if (!ParseCandidate(ICEPROTO_GOOGLE, candidate_elem, translator,
                          &candidate, error)) {
        return false;
      }

      desc->candidates.push_back(candidate);
    }
  }
  return true;
}

bool P2PTransportParser::WriteTransportDescription(
    const TransportDescription& desc,
    const CandidateTranslator* translator,
    buzz::XmlElement** out_elem,
    WriteError* error) {
  TransportProtocol proto = TransportProtocolFromDescription(&desc);
  rtc::scoped_ptr<buzz::XmlElement> trans_elem(
      NewTransportElement(desc.transport_type));

  // Fail if we get HYBRID or ICE right now.
  // TODO(juberti): Add ICE and HYBRID serialization.
  if (proto != ICEPROTO_GOOGLE) {
    LOG(LS_ERROR) << "Failed to serialize non-GICE TransportDescription";
    return false;
  }

  for (std::vector<Candidate>::const_iterator iter = desc.candidates.begin();
       iter != desc.candidates.end(); ++iter) {
    rtc::scoped_ptr<buzz::XmlElement> cand_elem(
        new buzz::XmlElement(QN_GINGLE_P2P_CANDIDATE));
    if (!WriteCandidate(proto, *iter, translator, cand_elem.get(), error)) {
      return false;
    }
    trans_elem->AddElement(cand_elem.release());
  }

  *out_elem = trans_elem.release();
  return true;
}

bool P2PTransportParser::ParseGingleCandidate(
    const buzz::XmlElement* elem,
    const CandidateTranslator* translator,
    Candidate* candidate,
    ParseError* error) {
  return ParseCandidate(ICEPROTO_GOOGLE, elem, translator, candidate, error);
}

bool P2PTransportParser::WriteGingleCandidate(
    const Candidate& candidate,
    const CandidateTranslator* translator,
    buzz::XmlElement** out_elem,
    WriteError* error) {
  rtc::scoped_ptr<buzz::XmlElement> elem(
      new buzz::XmlElement(QN_GINGLE_CANDIDATE));
  bool ret = WriteCandidate(ICEPROTO_GOOGLE, candidate, translator, elem.get(),
                            error);
  if (ret) {
    *out_elem = elem.release();
  }
  return ret;
}

bool P2PTransportParser::VerifyUsernameFormat(TransportProtocol proto,
                                              const std::string& username,
                                              ParseError* error) {
  if (proto == ICEPROTO_GOOGLE || proto == ICEPROTO_HYBRID) {
    if (username.size() > GICE_UFRAG_MAX_LENGTH)
      return BadParse("candidate username is too long", error);
    if (!rtc::Base64::IsBase64Encoded(username))
      return BadParse("candidate username has non-base64 encoded characters",
                      error);
  } else if (proto == ICEPROTO_RFC5245) {
    if (username.size() > ICE_UFRAG_MAX_LENGTH)
      return BadParse("candidate username is too long", error);
  }
  return true;
}

bool P2PTransportParser::ParseCandidate(TransportProtocol proto,
                                        const buzz::XmlElement* elem,
                                        const CandidateTranslator* translator,
                                        Candidate* candidate,
                                        ParseError* error) {
  ASSERT(proto == ICEPROTO_GOOGLE);
  ASSERT(translator != NULL);

  if (!elem->HasAttr(buzz::QN_NAME) ||
      !elem->HasAttr(QN_ADDRESS) ||
      !elem->HasAttr(QN_PORT) ||
      !elem->HasAttr(QN_USERNAME) ||
      !elem->HasAttr(QN_PROTOCOL) ||
      !elem->HasAttr(QN_GENERATION)) {
    return BadParse("candidate missing required attribute", error);
  }

  rtc::SocketAddress address;
  if (!ParseAddress(elem, QN_ADDRESS, QN_PORT, &address, error))
    return false;

  std::string channel_name = elem->Attr(buzz::QN_NAME);
  int component = 0;
  if (!translator ||
      !translator->GetComponentFromChannelName(channel_name, &component)) {
    return BadParse("candidate has unknown channel name " + channel_name,
                    error);
  }

  float preference = 0.0;
  if (!GetXmlAttr(elem, QN_PREFERENCE, 0.0f, &preference)) {
    return BadParse("candidate has unknown preference", error);
  }

  candidate->set_component(component);
  candidate->set_address(address);
  candidate->set_username(elem->Attr(QN_USERNAME));
  candidate->set_preference(preference);
  candidate->set_protocol(elem->Attr(QN_PROTOCOL));
  candidate->set_generation_str(elem->Attr(QN_GENERATION));
  if (elem->HasAttr(QN_PASSWORD))
    candidate->set_password(elem->Attr(QN_PASSWORD));
  if (elem->HasAttr(buzz::QN_TYPE))
    candidate->set_type(elem->Attr(buzz::QN_TYPE));
  if (elem->HasAttr(QN_NETWORK))
    candidate->set_network_name(elem->Attr(QN_NETWORK));

  if (!VerifyUsernameFormat(proto, candidate->username(), error))
    return false;

  return true;
}

bool P2PTransportParser::WriteCandidate(TransportProtocol proto,
                                        const Candidate& candidate,
                                        const CandidateTranslator* translator,
                                        buzz::XmlElement* elem,
                                        WriteError* error) {
  ASSERT(proto == ICEPROTO_GOOGLE);
  ASSERT(translator != NULL);

  std::string channel_name;
  if (!translator ||
      !translator->GetChannelNameFromComponent(
          candidate.component(), &channel_name)) {
    return BadWrite("Cannot write candidate because of unknown component.",
                    error);
  }

  elem->SetAttr(buzz::QN_NAME, channel_name);
  elem->SetAttr(QN_ADDRESS, candidate.address().ipaddr().ToString());
  elem->SetAttr(QN_PORT, candidate.address().PortAsString());
  AddXmlAttr(elem, QN_PREFERENCE, candidate.preference());
  elem->SetAttr(QN_USERNAME, candidate.username());
  elem->SetAttr(QN_PROTOCOL, candidate.protocol());
  elem->SetAttr(QN_GENERATION, candidate.generation_str());
  if (!candidate.password().empty())
    elem->SetAttr(QN_PASSWORD, candidate.password());
  elem->SetAttr(buzz::QN_TYPE, candidate.type());
  if (!candidate.network_name().empty())
    elem->SetAttr(QN_NETWORK, candidate.network_name());

  return true;
}

}  // namespace cricket
