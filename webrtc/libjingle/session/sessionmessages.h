/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LIBJINGLE_SESSION_SESSIONMESSAGES_H_
#define WEBRTC_LIBJINGLE_SESSION_SESSIONMESSAGES_H_

#include <map>
#include <string>
#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/libjingle/session/constants.h"
#include "webrtc/libjingle/session/parsing.h"
#include "webrtc/libjingle/session/transportparser.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/constants.h"
#include "webrtc/p2p/base/sessiondescription.h"  // Needed to delete contents.
#include "webrtc/p2p/base/transport.h"
#include "webrtc/p2p/base/transportinfo.h"

namespace cricket {

struct ParseError;
struct WriteError;
class Candidate;
class ContentParser;
class TransportParser;

typedef std::vector<Candidate> Candidates;
typedef std::map<std::string, ContentParser*> ContentParserMap;
typedef std::map<std::string, TransportParser*> TransportParserMap;

enum ActionType {
  ACTION_UNKNOWN,

  ACTION_SESSION_INITIATE,
  ACTION_SESSION_INFO,
  ACTION_SESSION_ACCEPT,
  ACTION_SESSION_REJECT,
  ACTION_SESSION_TERMINATE,

  ACTION_TRANSPORT_INFO,
  ACTION_TRANSPORT_ACCEPT,

  ACTION_DESCRIPTION_INFO,
};

// Abstraction of a <jingle> element within an <iq> stanza, per XMPP
// standard XEP-166.  Can be serialized into multiple protocols,
// including the standard (Jingle) and the draft standard (Gingle).
// In general, used to communicate actions related to a p2p session,
// such accept, initiate, terminate, etc.

struct SessionMessage {
  SessionMessage() : action_elem(NULL), stanza(NULL) {}

  SessionMessage(SignalingProtocol protocol, ActionType type,
                 const std::string& sid, const std::string& initiator) :
      protocol(protocol), type(type), sid(sid), initiator(initiator),
      action_elem(NULL), stanza(NULL) {}

  std::string id;
  std::string from;
  std::string to;
  SignalingProtocol protocol;
  ActionType type;
  std::string sid;  // session id
  std::string initiator;

  // Used for further parsing when necessary.
  // Represents <session> or <jingle>.
  const buzz::XmlElement* action_elem;
  // Mostly used for debugging.
  const buzz::XmlElement* stanza;
};

// TODO: Break up this class so we don't have to typedef it into
// different classes.
struct ContentMessage {
  ContentMessage() : owns_contents(false) {}

  ~ContentMessage() {
    if (owns_contents) {
      for (ContentInfos::iterator content = contents.begin();
           content != contents.end(); content++) {
        delete content->description;
      }
    }
  }

  // Caller takes ownership of contents.
  ContentInfos ClearContents() {
    ContentInfos out;
    contents.swap(out);
    owns_contents = false;
    return out;
  }

  bool owns_contents;
  ContentInfos contents;
  TransportInfos transports;
  ContentGroups groups;
};

typedef ContentMessage SessionInitiate;
typedef ContentMessage SessionAccept;
// Note that a DescriptionInfo does not have TransportInfos.
typedef ContentMessage DescriptionInfo;

struct SessionTerminate {
  SessionTerminate() {}

  explicit SessionTerminate(const std::string& reason) :
      reason(reason) {}

  std::string reason;
  std::string debug_reason;
};

struct SessionRedirect {
  std::string target;
};

// Content name => translator
typedef std::map<std::string, CandidateTranslator*> CandidateTranslatorMap;

bool IsSessionMessage(const buzz::XmlElement* stanza);
bool ParseSessionMessage(const buzz::XmlElement* stanza,
                         SessionMessage* msg,
                         ParseError* error);
// Will return an error if there is more than one content type.
bool ParseContentType(SignalingProtocol protocol,
                      const buzz::XmlElement* action_elem,
                      std::string* content_type,
                      ParseError* error);
void WriteSessionMessage(const SessionMessage& msg,
                         const XmlElements& action_elems,
                         buzz::XmlElement* stanza);
bool ParseSessionInitiate(SignalingProtocol protocol,
                          const buzz::XmlElement* action_elem,
                          const ContentParserMap& content_parsers,
                          const TransportParserMap& transport_parsers,
                          const CandidateTranslatorMap& translators,
                          SessionInitiate* init,
                          ParseError* error);
bool WriteSessionInitiate(SignalingProtocol protocol,
                          const ContentInfos& contents,
                          const TransportInfos& tinfos,
                          const ContentParserMap& content_parsers,
                          const TransportParserMap& transport_parsers,
                          const CandidateTranslatorMap& translators,
                          const ContentGroups& groups,
                          XmlElements* elems,
                          WriteError* error);
bool ParseSessionAccept(SignalingProtocol protocol,
                        const buzz::XmlElement* action_elem,
                        const ContentParserMap& content_parsers,
                        const TransportParserMap& transport_parsers,
                        const CandidateTranslatorMap& translators,
                        SessionAccept* accept,
                        ParseError* error);
bool WriteSessionAccept(SignalingProtocol protocol,
                        const ContentInfos& contents,
                        const TransportInfos& tinfos,
                        const ContentParserMap& content_parsers,
                        const TransportParserMap& transport_parsers,
                        const CandidateTranslatorMap& translators,
                        const ContentGroups& groups,
                        XmlElements* elems,
                        WriteError* error);
bool ParseSessionTerminate(SignalingProtocol protocol,
                           const buzz::XmlElement* action_elem,
                           SessionTerminate* term,
                           ParseError* error);
void WriteSessionTerminate(SignalingProtocol protocol,
                           const SessionTerminate& term,
                           XmlElements* elems);
bool ParseDescriptionInfo(SignalingProtocol protocol,
                          const buzz::XmlElement* action_elem,
                          const ContentParserMap& content_parsers,
                          const TransportParserMap& transport_parsers,
                          const CandidateTranslatorMap& translators,
                          DescriptionInfo* description_info,
                          ParseError* error);
bool WriteDescriptionInfo(SignalingProtocol protocol,
                          const ContentInfos& contents,
                          const ContentParserMap& content_parsers,
                          XmlElements* elems,
                          WriteError* error);
// Since a TransportInfo is not a transport-info message, and a
// transport-info message is just a collection of TransportInfos, we
// say Parse/Write TransportInfos for transport-info messages.
bool ParseTransportInfos(SignalingProtocol protocol,
                         const buzz::XmlElement* action_elem,
                         const ContentInfos& contents,
                         const TransportParserMap& trans_parsers,
                         const CandidateTranslatorMap& translators,
                         TransportInfos* tinfos,
                         ParseError* error);
bool WriteTransportInfos(SignalingProtocol protocol,
                         const TransportInfos& tinfos,
                         const TransportParserMap& trans_parsers,
                         const CandidateTranslatorMap& translators,
                         XmlElements* elems,
                         WriteError* error);
// Handles both Gingle and Jingle syntax.
bool FindSessionRedirect(const buzz::XmlElement* stanza,
                         SessionRedirect* redirect);
}  // namespace cricket

#endif  // WEBRTC_LIBJINGLE_SESSION_SESSIONMESSAGES_H_
