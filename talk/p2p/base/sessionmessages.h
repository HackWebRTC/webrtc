/*
 * libjingle
 * Copyright 2010, Google Inc.
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

#ifndef TALK_P2P_BASE_SESSIONMESSAGES_H_
#define TALK_P2P_BASE_SESSIONMESSAGES_H_

#include <string>
#include <vector>
#include <map>

#include "talk/base/basictypes.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/parsing.h"
#include "talk/p2p/base/sessiondescription.h"  // Needed to delete contents.
#include "talk/p2p/base/transportinfo.h"
#include "talk/xmllite/xmlelement.h"

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

// Used during parsing and writing to map component to channel name
// and back.  This is primarily for converting old G-ICE candidate
// signalling to new ICE candidate classes.
class CandidateTranslator {
 public:
  virtual bool GetChannelNameFromComponent(
      int component, std::string* channel_name) const = 0;
  virtual bool GetComponentFromChannelName(
      const std::string& channel_name, int* component) const = 0;
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

#endif  // TALK_P2P_BASE_SESSIONMESSAGES_H_
