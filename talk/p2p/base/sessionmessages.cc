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

#include "talk/p2p/base/sessionmessages.h"

#include <stdio.h>
#include <string>

#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/stringutils.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/p2ptransport.h"
#include "talk/p2p/base/parsing.h"
#include "talk/p2p/base/sessionclient.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/p2p/base/transport.h"
#include "talk/xmllite/xmlconstants.h"
#include "talk/xmpp/constants.h"

namespace cricket {

ActionType ToActionType(const std::string& type) {
  if (type == GINGLE_ACTION_INITIATE)
    return ACTION_SESSION_INITIATE;
  if (type == GINGLE_ACTION_INFO)
    return ACTION_SESSION_INFO;
  if (type == GINGLE_ACTION_ACCEPT)
    return ACTION_SESSION_ACCEPT;
  if (type == GINGLE_ACTION_REJECT)
    return ACTION_SESSION_REJECT;
  if (type == GINGLE_ACTION_TERMINATE)
    return ACTION_SESSION_TERMINATE;
  if (type == GINGLE_ACTION_CANDIDATES)
    return ACTION_TRANSPORT_INFO;
  if (type == JINGLE_ACTION_SESSION_INITIATE)
    return ACTION_SESSION_INITIATE;
  if (type == JINGLE_ACTION_TRANSPORT_INFO)
    return ACTION_TRANSPORT_INFO;
  if (type == JINGLE_ACTION_TRANSPORT_ACCEPT)
    return ACTION_TRANSPORT_ACCEPT;
  if (type == JINGLE_ACTION_SESSION_INFO)
    return ACTION_SESSION_INFO;
  if (type == JINGLE_ACTION_SESSION_ACCEPT)
    return ACTION_SESSION_ACCEPT;
  if (type == JINGLE_ACTION_SESSION_TERMINATE)
    return ACTION_SESSION_TERMINATE;
  if (type == JINGLE_ACTION_TRANSPORT_INFO)
    return ACTION_TRANSPORT_INFO;
  if (type == JINGLE_ACTION_TRANSPORT_ACCEPT)
    return ACTION_TRANSPORT_ACCEPT;
  if (type == JINGLE_ACTION_DESCRIPTION_INFO)
    return ACTION_DESCRIPTION_INFO;
  if (type == GINGLE_ACTION_UPDATE)
    return ACTION_DESCRIPTION_INFO;

  return ACTION_UNKNOWN;
}

std::string ToJingleString(ActionType type) {
  switch (type) {
    case ACTION_SESSION_INITIATE:
      return JINGLE_ACTION_SESSION_INITIATE;
    case ACTION_SESSION_INFO:
      return JINGLE_ACTION_SESSION_INFO;
    case ACTION_DESCRIPTION_INFO:
      return JINGLE_ACTION_DESCRIPTION_INFO;
    case ACTION_SESSION_ACCEPT:
      return JINGLE_ACTION_SESSION_ACCEPT;
    // Notice that reject and terminate both go to
    // "session-terminate", but there is no "session-reject".
    case ACTION_SESSION_REJECT:
    case ACTION_SESSION_TERMINATE:
      return JINGLE_ACTION_SESSION_TERMINATE;
    case ACTION_TRANSPORT_INFO:
      return JINGLE_ACTION_TRANSPORT_INFO;
    case ACTION_TRANSPORT_ACCEPT:
      return JINGLE_ACTION_TRANSPORT_ACCEPT;
    default:
      return "";
  }
}

std::string ToGingleString(ActionType type) {
  switch (type) {
    case ACTION_SESSION_INITIATE:
      return GINGLE_ACTION_INITIATE;
    case ACTION_SESSION_INFO:
      return GINGLE_ACTION_INFO;
    case ACTION_SESSION_ACCEPT:
      return GINGLE_ACTION_ACCEPT;
    case ACTION_SESSION_REJECT:
      return GINGLE_ACTION_REJECT;
    case ACTION_SESSION_TERMINATE:
      return GINGLE_ACTION_TERMINATE;
    case ACTION_TRANSPORT_INFO:
      return GINGLE_ACTION_CANDIDATES;
    default:
      return "";
  }
}


bool IsJingleMessage(const buzz::XmlElement* stanza) {
  const buzz::XmlElement* jingle = stanza->FirstNamed(QN_JINGLE);
  if (jingle == NULL)
    return false;

  return (jingle->HasAttr(buzz::QN_ACTION) && jingle->HasAttr(QN_SID));
}

bool IsGingleMessage(const buzz::XmlElement* stanza) {
  const buzz::XmlElement* session = stanza->FirstNamed(QN_GINGLE_SESSION);
  if (session == NULL)
    return false;

  return (session->HasAttr(buzz::QN_TYPE) &&
          session->HasAttr(buzz::QN_ID)   &&
          session->HasAttr(QN_INITIATOR));
}

bool IsSessionMessage(const buzz::XmlElement* stanza) {
  return (stanza->Name() == buzz::QN_IQ &&
          stanza->Attr(buzz::QN_TYPE) == buzz::STR_SET &&
          (IsJingleMessage(stanza) ||
           IsGingleMessage(stanza)));
}

bool ParseGingleSessionMessage(const buzz::XmlElement* session,
                               SessionMessage* msg,
                               ParseError* error) {
  msg->protocol = PROTOCOL_GINGLE;
  std::string type_string = session->Attr(buzz::QN_TYPE);
  msg->type = ToActionType(type_string);
  msg->sid = session->Attr(buzz::QN_ID);
  msg->initiator = session->Attr(QN_INITIATOR);
  msg->action_elem = session;

  if (msg->type == ACTION_UNKNOWN)
    return BadParse("unknown action: " + type_string, error);

  return true;
}

bool ParseJingleSessionMessage(const buzz::XmlElement* jingle,
                               SessionMessage* msg,
                               ParseError* error) {
  msg->protocol = PROTOCOL_JINGLE;
  std::string type_string = jingle->Attr(buzz::QN_ACTION);
  msg->type = ToActionType(type_string);
  msg->sid = jingle->Attr(QN_SID);
  msg->initiator = GetXmlAttr(jingle, QN_INITIATOR, buzz::STR_EMPTY);
  msg->action_elem = jingle;

  if (msg->type == ACTION_UNKNOWN)
    return BadParse("unknown action: " + type_string, error);

  return true;
}

bool ParseHybridSessionMessage(const buzz::XmlElement* jingle,
                               SessionMessage* msg,
                               ParseError* error) {
  if (!ParseJingleSessionMessage(jingle, msg, error))
    return false;
  msg->protocol = PROTOCOL_HYBRID;

  return true;
}

bool ParseSessionMessage(const buzz::XmlElement* stanza,
                         SessionMessage* msg,
                         ParseError* error) {
  msg->id = stanza->Attr(buzz::QN_ID);
  msg->from = stanza->Attr(buzz::QN_FROM);
  msg->to = stanza->Attr(buzz::QN_TO);
  msg->stanza = stanza;

  const buzz::XmlElement* jingle = stanza->FirstNamed(QN_JINGLE);
  const buzz::XmlElement* session = stanza->FirstNamed(QN_GINGLE_SESSION);
  if (jingle && session)
    return ParseHybridSessionMessage(jingle, msg, error);
  if (jingle != NULL)
    return ParseJingleSessionMessage(jingle, msg, error);
  if (session != NULL)
    return ParseGingleSessionMessage(session, msg, error);
  return false;
}

buzz::XmlElement* WriteGingleAction(const SessionMessage& msg,
                                    const XmlElements& action_elems) {
  buzz::XmlElement* session = new buzz::XmlElement(QN_GINGLE_SESSION, true);
  session->AddAttr(buzz::QN_TYPE, ToGingleString(msg.type));
  session->AddAttr(buzz::QN_ID, msg.sid);
  session->AddAttr(QN_INITIATOR, msg.initiator);
  AddXmlChildren(session, action_elems);
  return session;
}

buzz::XmlElement* WriteJingleAction(const SessionMessage& msg,
                                    const XmlElements& action_elems) {
  buzz::XmlElement* jingle = new buzz::XmlElement(QN_JINGLE, true);
  jingle->AddAttr(buzz::QN_ACTION, ToJingleString(msg.type));
  jingle->AddAttr(QN_SID, msg.sid);
  if (msg.type == ACTION_SESSION_INITIATE) {
    jingle->AddAttr(QN_INITIATOR, msg.initiator);
  }
  AddXmlChildren(jingle, action_elems);
  return jingle;
}

void WriteSessionMessage(const SessionMessage& msg,
                         const XmlElements& action_elems,
                         buzz::XmlElement* stanza) {
  stanza->SetAttr(buzz::QN_TO, msg.to);
  stanza->SetAttr(buzz::QN_TYPE, buzz::STR_SET);

  if (msg.protocol == PROTOCOL_GINGLE) {
    stanza->AddElement(WriteGingleAction(msg, action_elems));
  } else {
    stanza->AddElement(WriteJingleAction(msg, action_elems));
  }
}


TransportParser* GetTransportParser(const TransportParserMap& trans_parsers,
                                    const std::string& transport_type) {
  TransportParserMap::const_iterator map = trans_parsers.find(transport_type);
  if (map == trans_parsers.end()) {
    return NULL;
  } else {
    return map->second;
  }
}

CandidateTranslator* GetCandidateTranslator(
    const CandidateTranslatorMap& translators,
    const std::string& content_name) {
  CandidateTranslatorMap::const_iterator map = translators.find(content_name);
  if (map == translators.end()) {
    return NULL;
  } else {
    return map->second;
  }
}

bool GetParserAndTranslator(const TransportParserMap& trans_parsers,
                            const CandidateTranslatorMap& translators,
                            const std::string& transport_type,
                            const std::string& content_name,
                            TransportParser** parser,
                            CandidateTranslator** translator,
                            ParseError* error) {
  *parser = GetTransportParser(trans_parsers, transport_type);
  if (*parser == NULL) {
    return BadParse("unknown transport type: " + transport_type, error);
  }
  // Not having a translator isn't fatal when parsing. If this is called for an
  // initiate message, we won't have our proxies set up to do the translation.
  // Fortunately, for the cases where translation is needed, candidates are
  // never sent in initiates.
  *translator = GetCandidateTranslator(translators, content_name);
  return true;
}

bool GetParserAndTranslator(const TransportParserMap& trans_parsers,
                            const CandidateTranslatorMap& translators,
                            const std::string& transport_type,
                            const std::string& content_name,
                            TransportParser** parser,
                            CandidateTranslator** translator,
                            WriteError* error) {
  *parser = GetTransportParser(trans_parsers, transport_type);
  if (*parser == NULL) {
    return BadWrite("unknown transport type: " + transport_type, error);
  }
  *translator = GetCandidateTranslator(translators, content_name);
  if (*translator == NULL) {
    return BadWrite("unknown content name: " + content_name, error);
  }
  return true;
}

bool ParseGingleCandidate(const buzz::XmlElement* candidate_elem,
                          const TransportParserMap& trans_parsers,
                          const CandidateTranslatorMap& translators,
                          const std::string& content_name,
                          Candidates* candidates,
                          ParseError* error) {
  TransportParser* trans_parser;
  CandidateTranslator* translator;
  if (!GetParserAndTranslator(trans_parsers, translators,
                              NS_GINGLE_P2P, content_name,
                              &trans_parser, &translator, error))
    return false;

  Candidate candidate;
  if (!trans_parser->ParseGingleCandidate(
          candidate_elem, translator, &candidate, error)) {
    return false;
  }

  candidates->push_back(candidate);
  return true;
}

bool ParseGingleCandidates(const buzz::XmlElement* parent,
                           const TransportParserMap& trans_parsers,
                           const CandidateTranslatorMap& translators,
                           const std::string& content_name,
                           Candidates* candidates,
                           ParseError* error) {
  for (const buzz::XmlElement* candidate_elem = parent->FirstElement();
       candidate_elem != NULL;
       candidate_elem = candidate_elem->NextElement()) {
    if (candidate_elem->Name().LocalPart() == LN_CANDIDATE) {
      if (!ParseGingleCandidate(candidate_elem, trans_parsers, translators,
                                content_name, candidates, error)) {
        return false;
      }
    }
  }
  return true;
}

bool ParseGingleTransportInfos(const buzz::XmlElement* action_elem,
                               const ContentInfos& contents,
                               const TransportParserMap& trans_parsers,
                               const CandidateTranslatorMap& translators,
                               TransportInfos* tinfos,
                               ParseError* error) {
  bool has_audio = FindContentInfoByName(contents, CN_AUDIO) != NULL;
  bool has_video = FindContentInfoByName(contents, CN_VIDEO) != NULL;

  // If we don't have media, no need to separate the candidates.
  if (!has_audio && !has_video) {
    TransportInfo tinfo(CN_OTHER,
        TransportDescription(NS_GINGLE_P2P, Candidates()));
    if (!ParseGingleCandidates(action_elem, trans_parsers, translators,
                               CN_OTHER, &tinfo.description.candidates,
                               error)) {
      return false;
    }

    tinfos->push_back(tinfo);
    return true;
  }

  // If we have media, separate the candidates.
  TransportInfo audio_tinfo(CN_AUDIO,
                            TransportDescription(NS_GINGLE_P2P, Candidates()));
  TransportInfo video_tinfo(CN_VIDEO,
                            TransportDescription(NS_GINGLE_P2P, Candidates()));
  for (const buzz::XmlElement* candidate_elem = action_elem->FirstElement();
       candidate_elem != NULL;
       candidate_elem = candidate_elem->NextElement()) {
    if (candidate_elem->Name().LocalPart() == LN_CANDIDATE) {
      const std::string& channel_name = candidate_elem->Attr(buzz::QN_NAME);
      if (has_audio &&
          (channel_name == GICE_CHANNEL_NAME_RTP ||
           channel_name == GICE_CHANNEL_NAME_RTCP)) {
        if (!ParseGingleCandidate(
                candidate_elem, trans_parsers,
                translators, CN_AUDIO,
                &audio_tinfo.description.candidates, error)) {
          return false;
        }
      } else if (has_video &&
                 (channel_name == GICE_CHANNEL_NAME_VIDEO_RTP ||
                  channel_name == GICE_CHANNEL_NAME_VIDEO_RTCP)) {
        if (!ParseGingleCandidate(
                candidate_elem, trans_parsers,
                translators, CN_VIDEO,
                &video_tinfo.description.candidates, error)) {
          return false;
        }
      } else {
        return BadParse("Unknown channel name: " + channel_name, error);
      }
    }
  }

  if (has_audio) {
    tinfos->push_back(audio_tinfo);
  }
  if (has_video) {
    tinfos->push_back(video_tinfo);
  }
  return true;
}

bool ParseJingleTransportInfo(const buzz::XmlElement* trans_elem,
                              const std::string& content_name,
                              const TransportParserMap& trans_parsers,
                              const CandidateTranslatorMap& translators,
                              TransportInfo* tinfo,
                              ParseError* error) {
  TransportParser* trans_parser;
  CandidateTranslator* translator;
  if (!GetParserAndTranslator(trans_parsers, translators,
                              trans_elem->Name().Namespace(), content_name,
                              &trans_parser, &translator, error))
    return false;

  TransportDescription tdesc;
  if (!trans_parser->ParseTransportDescription(trans_elem, translator,
                                               &tdesc, error))
    return false;

  *tinfo = TransportInfo(content_name, tdesc);
  return true;
}

bool ParseJingleTransportInfos(const buzz::XmlElement* jingle,
                               const ContentInfos& contents,
                               const TransportParserMap trans_parsers,
                               const CandidateTranslatorMap& translators,
                               TransportInfos* tinfos,
                               ParseError* error) {
  for (const buzz::XmlElement* pair_elem
           = jingle->FirstNamed(QN_JINGLE_CONTENT);
       pair_elem != NULL;
       pair_elem = pair_elem->NextNamed(QN_JINGLE_CONTENT)) {
    std::string content_name;
    if (!RequireXmlAttr(pair_elem, QN_JINGLE_CONTENT_NAME,
                        &content_name, error))
      return false;

    const ContentInfo* content = FindContentInfoByName(contents, content_name);
    if (!content)
      return BadParse("Unknown content name: " + content_name, error);

    const buzz::XmlElement* trans_elem;
    if (!RequireXmlChild(pair_elem, LN_TRANSPORT, &trans_elem, error))
      return false;

    TransportInfo tinfo;
    if (!ParseJingleTransportInfo(trans_elem, content->name,
                                  trans_parsers, translators,
                                  &tinfo, error))
      return false;

    tinfos->push_back(tinfo);
  }

  return true;
}

buzz::XmlElement* NewTransportElement(const std::string& name) {
  return new buzz::XmlElement(buzz::QName(name, LN_TRANSPORT), true);
}

bool WriteGingleCandidates(const Candidates& candidates,
                           const TransportParserMap& trans_parsers,
                           const std::string& transport_type,
                           const CandidateTranslatorMap& translators,
                           const std::string& content_name,
                           XmlElements* elems,
                           WriteError* error) {
  TransportParser* trans_parser;
  CandidateTranslator* translator;
  if (!GetParserAndTranslator(trans_parsers, translators,
                              transport_type, content_name,
                              &trans_parser, &translator, error))
    return false;

  for (size_t i = 0; i < candidates.size(); ++i) {
    talk_base::scoped_ptr<buzz::XmlElement> element;
    if (!trans_parser->WriteGingleCandidate(candidates[i], translator,
                                            element.accept(), error)) {
      return false;
    }

    elems->push_back(element.release());
  }

  return true;
}

bool WriteGingleTransportInfos(const TransportInfos& tinfos,
                               const TransportParserMap& trans_parsers,
                               const CandidateTranslatorMap& translators,
                               XmlElements* elems,
                               WriteError* error) {
  for (TransportInfos::const_iterator tinfo = tinfos.begin();
       tinfo != tinfos.end(); ++tinfo) {
    if (!WriteGingleCandidates(tinfo->description.candidates,
                               trans_parsers, tinfo->description.transport_type,
                               translators, tinfo->content_name,
                               elems, error))
      return false;
  }

  return true;
}

bool WriteJingleTransportInfo(const TransportInfo& tinfo,
                              const TransportParserMap& trans_parsers,
                              const CandidateTranslatorMap& translators,
                              XmlElements* elems,
                              WriteError* error) {
  std::string transport_type = tinfo.description.transport_type;
  TransportParser* trans_parser;
  CandidateTranslator* translator;
  if (!GetParserAndTranslator(trans_parsers, translators,
                              transport_type, tinfo.content_name,
                              &trans_parser, &translator, error))
    return false;

  buzz::XmlElement* trans_elem;
  if (!trans_parser->WriteTransportDescription(tinfo.description, translator,
                                               &trans_elem, error)) {
    return false;
  }

  elems->push_back(trans_elem);
  return true;
}

void WriteJingleContent(const std::string name,
                        const XmlElements& child_elems,
                        XmlElements* elems) {
  buzz::XmlElement* content_elem = new buzz::XmlElement(QN_JINGLE_CONTENT);
  content_elem->SetAttr(QN_JINGLE_CONTENT_NAME, name);
  content_elem->SetAttr(QN_CREATOR, LN_INITIATOR);
  AddXmlChildren(content_elem, child_elems);

  elems->push_back(content_elem);
}

bool WriteJingleTransportInfos(const TransportInfos& tinfos,
                               const TransportParserMap& trans_parsers,
                               const CandidateTranslatorMap& translators,
                               XmlElements* elems,
                               WriteError* error) {
  for (TransportInfos::const_iterator tinfo = tinfos.begin();
       tinfo != tinfos.end(); ++tinfo) {
    XmlElements content_child_elems;
    if (!WriteJingleTransportInfo(*tinfo, trans_parsers, translators,
                                  &content_child_elems, error))

      return false;

    WriteJingleContent(tinfo->content_name, content_child_elems, elems);
  }

  return true;
}

ContentParser* GetContentParser(const ContentParserMap& content_parsers,
                                const std::string& type) {
  ContentParserMap::const_iterator map = content_parsers.find(type);
  if (map == content_parsers.end()) {
    return NULL;
  } else {
    return map->second;
  }
}

bool ParseContentInfo(SignalingProtocol protocol,
                      const std::string& name,
                      const std::string& type,
                      const buzz::XmlElement* elem,
                      const ContentParserMap& parsers,
                      ContentInfos* contents,
                      ParseError* error) {
  ContentParser* parser = GetContentParser(parsers, type);
  if (parser == NULL)
    return BadParse("unknown application content: " + type, error);

  ContentDescription* desc;
  if (!parser->ParseContent(protocol, elem, &desc, error))
    return false;

  contents->push_back(ContentInfo(name, type, desc));
  return true;
}

bool ParseContentType(const buzz::XmlElement* parent_elem,
                      std::string* content_type,
                      const buzz::XmlElement** content_elem,
                      ParseError* error) {
  if (!RequireXmlChild(parent_elem, LN_DESCRIPTION, content_elem, error))
    return false;

  *content_type = (*content_elem)->Name().Namespace();
  return true;
}

bool ParseGingleContentInfos(const buzz::XmlElement* session,
                             const ContentParserMap& content_parsers,
                             ContentInfos* contents,
                             ParseError* error) {
  std::string content_type;
  const buzz::XmlElement* content_elem;
  if (!ParseContentType(session, &content_type, &content_elem, error))
    return false;

  if (content_type == NS_GINGLE_VIDEO) {
    // A parser parsing audio or video content should look at the
    // namespace and only parse the codecs relevant to that namespace.
    // We use this to control which codecs get parsed: first audio,
    // then video.
    talk_base::scoped_ptr<buzz::XmlElement> audio_elem(
        new buzz::XmlElement(QN_GINGLE_AUDIO_CONTENT));
    CopyXmlChildren(content_elem, audio_elem.get());
    if (!ParseContentInfo(PROTOCOL_GINGLE, CN_AUDIO, NS_JINGLE_RTP,
                          audio_elem.get(), content_parsers,
                          contents, error))
      return false;

    if (!ParseContentInfo(PROTOCOL_GINGLE, CN_VIDEO, NS_JINGLE_RTP,
                          content_elem, content_parsers,
                          contents, error))
      return false;
  } else if (content_type == NS_GINGLE_AUDIO) {
    if (!ParseContentInfo(PROTOCOL_GINGLE, CN_AUDIO, NS_JINGLE_RTP,
                          content_elem, content_parsers,
                          contents, error))
      return false;
  } else {
    if (!ParseContentInfo(PROTOCOL_GINGLE, CN_OTHER, content_type,
                          content_elem, content_parsers,
                          contents, error))
      return false;
  }
  return true;
}

bool ParseJingleContentInfos(const buzz::XmlElement* jingle,
                             const ContentParserMap& content_parsers,
                             ContentInfos* contents,
                             ParseError* error) {
  for (const buzz::XmlElement* pair_elem
           = jingle->FirstNamed(QN_JINGLE_CONTENT);
       pair_elem != NULL;
       pair_elem = pair_elem->NextNamed(QN_JINGLE_CONTENT)) {
    std::string content_name;
    if (!RequireXmlAttr(pair_elem, QN_JINGLE_CONTENT_NAME,
                        &content_name, error))
      return false;

    std::string content_type;
    const buzz::XmlElement* content_elem;
    if (!ParseContentType(pair_elem, &content_type, &content_elem, error))
      return false;

    if (!ParseContentInfo(PROTOCOL_JINGLE, content_name, content_type,
                          content_elem, content_parsers,
                          contents, error))
      return false;
  }
  return true;
}

bool ParseJingleGroupInfos(const buzz::XmlElement* jingle,
                           ContentGroups* groups,
                           ParseError* error) {
  for (const buzz::XmlElement* pair_elem
           = jingle->FirstNamed(QN_JINGLE_DRAFT_GROUP);
       pair_elem != NULL;
       pair_elem = pair_elem->NextNamed(QN_JINGLE_DRAFT_GROUP)) {
    std::string group_name;
    if (!RequireXmlAttr(pair_elem, QN_JINGLE_DRAFT_GROUP_TYPE,
                        &group_name, error))
      return false;

    ContentGroup group(group_name);
    for (const buzz::XmlElement* child_elem
             = pair_elem->FirstNamed(QN_JINGLE_CONTENT);
        child_elem != NULL;
        child_elem = child_elem->NextNamed(QN_JINGLE_CONTENT)) {
      std::string content_name;
      if (!RequireXmlAttr(child_elem, QN_JINGLE_CONTENT_NAME,
                          &content_name, error))
        return false;
      group.AddContentName(content_name);
    }
    groups->push_back(group);
  }
  return true;
}

buzz::XmlElement* WriteContentInfo(SignalingProtocol protocol,
                                   const ContentInfo& content,
                                   const ContentParserMap& parsers,
                                   WriteError* error) {
  ContentParser* parser = GetContentParser(parsers, content.type);
  if (parser == NULL) {
    BadWrite("unknown content type: " + content.type, error);
    return NULL;
  }

  buzz::XmlElement* elem = NULL;
  if (!parser->WriteContent(protocol, content.description, &elem, error))
    return NULL;

  return elem;
}

bool IsWritable(SignalingProtocol protocol,
                const ContentInfo& content,
                const ContentParserMap& parsers) {
  ContentParser* parser = GetContentParser(parsers, content.type);
  if (parser == NULL) {
    return false;
  }

  return parser->IsWritable(protocol, content.description);
}

bool WriteGingleContentInfos(const ContentInfos& contents,
                             const ContentParserMap& parsers,
                             XmlElements* elems,
                             WriteError* error) {
  if (contents.size() == 1 ||
      (contents.size() == 2 &&
       !IsWritable(PROTOCOL_GINGLE, contents.at(1), parsers))) {
    if (contents.front().rejected) {
      return BadWrite("Gingle protocol may not reject individual contents.",
                      error);
    }
    buzz::XmlElement* elem = WriteContentInfo(
        PROTOCOL_GINGLE, contents.front(), parsers, error);
    if (!elem)
      return false;

    elems->push_back(elem);
  } else if (contents.size() >= 2 &&
             contents.at(0).type == NS_JINGLE_RTP &&
             contents.at(1).type == NS_JINGLE_RTP) {
     // Special-case audio + video contents so that they are "merged"
     // into one "video" content.
    if (contents.at(0).rejected || contents.at(1).rejected) {
      return BadWrite("Gingle protocol may not reject individual contents.",
                      error);
    }
    buzz::XmlElement* audio = WriteContentInfo(
        PROTOCOL_GINGLE, contents.at(0), parsers, error);
    if (!audio)
      return false;

    buzz::XmlElement* video = WriteContentInfo(
        PROTOCOL_GINGLE, contents.at(1), parsers, error);
    if (!video) {
      delete audio;
      return false;
    }

    CopyXmlChildren(audio, video);
    elems->push_back(video);
    delete audio;
  } else {
    return BadWrite("Gingle protocol may only have one content.", error);
  }

  return true;
}

const TransportInfo* GetTransportInfoByContentName(
    const TransportInfos& tinfos, const std::string& content_name) {
  for (TransportInfos::const_iterator tinfo = tinfos.begin();
       tinfo != tinfos.end(); ++tinfo) {
    if (content_name == tinfo->content_name) {
      return &*tinfo;
    }
  }
  return NULL;
}

bool WriteJingleContents(const ContentInfos& contents,
                         const ContentParserMap& content_parsers,
                         const TransportInfos& tinfos,
                         const TransportParserMap& trans_parsers,
                         const CandidateTranslatorMap& translators,
                         XmlElements* elems,
                         WriteError* error) {
  for (ContentInfos::const_iterator content = contents.begin();
       content != contents.end(); ++content) {
    if (content->rejected) {
      continue;
    }
    const TransportInfo* tinfo =
        GetTransportInfoByContentName(tinfos, content->name);
    if (!tinfo)
      return BadWrite("No transport for content: " + content->name, error);

    XmlElements pair_elems;
    buzz::XmlElement* elem = WriteContentInfo(
        PROTOCOL_JINGLE, *content, content_parsers, error);
    if (!elem)
      return false;
    pair_elems.push_back(elem);

    if (!WriteJingleTransportInfo(*tinfo, trans_parsers, translators,
                                  &pair_elems, error))
      return false;

    WriteJingleContent(content->name, pair_elems, elems);
  }
  return true;
}

bool WriteJingleContentInfos(const ContentInfos& contents,
                             const ContentParserMap& content_parsers,
                             XmlElements* elems,
                             WriteError* error) {
  for (ContentInfos::const_iterator content = contents.begin();
       content != contents.end(); ++content) {
    if (content->rejected) {
      continue;
    }
    XmlElements content_child_elems;
    buzz::XmlElement* elem = WriteContentInfo(
        PROTOCOL_JINGLE, *content, content_parsers, error);
    if (!elem)
      return false;
    content_child_elems.push_back(elem);
    WriteJingleContent(content->name, content_child_elems, elems);
  }
  return true;
}

bool WriteJingleGroupInfo(const ContentInfos& contents,
                          const ContentGroups& groups,
                          XmlElements* elems,
                          WriteError* error) {
  if (!groups.empty()) {
    buzz::XmlElement* pair_elem = new buzz::XmlElement(QN_JINGLE_DRAFT_GROUP);
    pair_elem->SetAttr(QN_JINGLE_DRAFT_GROUP_TYPE, GROUP_TYPE_BUNDLE);

    XmlElements pair_elems;
    for (ContentInfos::const_iterator content = contents.begin();
         content != contents.end(); ++content) {
      buzz::XmlElement* child_elem =
          new buzz::XmlElement(QN_JINGLE_CONTENT, false);
      child_elem->SetAttr(QN_JINGLE_CONTENT_NAME, content->name);
      pair_elems.push_back(child_elem);
    }
    AddXmlChildren(pair_elem, pair_elems);
    elems->push_back(pair_elem);
  }
  return true;
}

bool ParseContentType(SignalingProtocol protocol,
                      const buzz::XmlElement* action_elem,
                      std::string* content_type,
                      ParseError* error) {
  const buzz::XmlElement* content_elem;
  if (protocol == PROTOCOL_GINGLE) {
    if (!ParseContentType(action_elem, content_type, &content_elem, error))
      return false;

    // Internally, we only use NS_JINGLE_RTP.
    if (*content_type == NS_GINGLE_AUDIO ||
        *content_type == NS_GINGLE_VIDEO)
      *content_type = NS_JINGLE_RTP;
  } else {
    const buzz::XmlElement* pair_elem
        = action_elem->FirstNamed(QN_JINGLE_CONTENT);
    if (pair_elem == NULL)
      return BadParse("No contents found", error);

    if (!ParseContentType(pair_elem, content_type, &content_elem, error))
      return false;
  }

  return true;
}

static bool ParseContentMessage(
    SignalingProtocol protocol,
    const buzz::XmlElement* action_elem,
    bool expect_transports,
    const ContentParserMap& content_parsers,
    const TransportParserMap& trans_parsers,
    const CandidateTranslatorMap& translators,
    SessionInitiate* init,
    ParseError* error) {
  init->owns_contents = true;
  if (protocol == PROTOCOL_GINGLE) {
    if (!ParseGingleContentInfos(action_elem, content_parsers,
                                 &init->contents, error))
      return false;

    if (expect_transports &&
        !ParseGingleTransportInfos(action_elem, init->contents,
                                   trans_parsers, translators,
                                   &init->transports, error))
      return false;
  } else {
    if (!ParseJingleContentInfos(action_elem, content_parsers,
                                 &init->contents, error))
      return false;
    if (!ParseJingleGroupInfos(action_elem, &init->groups, error))
      return false;

    if (expect_transports &&
        !ParseJingleTransportInfos(action_elem, init->contents,
                                   trans_parsers, translators,
                                   &init->transports, error))
      return false;
  }

  return true;
}

static bool WriteContentMessage(
    SignalingProtocol protocol,
    const ContentInfos& contents,
    const TransportInfos& tinfos,
    const ContentParserMap& content_parsers,
    const TransportParserMap& transport_parsers,
    const CandidateTranslatorMap& translators,
    const ContentGroups& groups,
    XmlElements* elems,
    WriteError* error) {
  if (protocol == PROTOCOL_GINGLE) {
    if (!WriteGingleContentInfos(contents, content_parsers, elems, error))
      return false;

    if (!WriteGingleTransportInfos(tinfos, transport_parsers, translators,
                                   elems, error))
      return false;
  } else {
    if (!WriteJingleContents(contents, content_parsers,
                             tinfos, transport_parsers, translators,
                             elems, error))
      return false;
    if (!WriteJingleGroupInfo(contents, groups, elems, error))
      return false;
  }

  return true;
}

bool ParseSessionInitiate(SignalingProtocol protocol,
                          const buzz::XmlElement* action_elem,
                          const ContentParserMap& content_parsers,
                          const TransportParserMap& trans_parsers,
                          const CandidateTranslatorMap& translators,
                          SessionInitiate* init,
                          ParseError* error) {
  bool expect_transports = true;
  return ParseContentMessage(protocol, action_elem, expect_transports,
                             content_parsers, trans_parsers, translators,
                             init, error);
}


bool WriteSessionInitiate(SignalingProtocol protocol,
                          const ContentInfos& contents,
                          const TransportInfos& tinfos,
                          const ContentParserMap& content_parsers,
                          const TransportParserMap& transport_parsers,
                          const CandidateTranslatorMap& translators,
                          const ContentGroups& groups,
                          XmlElements* elems,
                          WriteError* error) {
  return WriteContentMessage(protocol, contents, tinfos,
                             content_parsers, transport_parsers, translators,
                             groups,
                             elems, error);
}

bool ParseSessionAccept(SignalingProtocol protocol,
                        const buzz::XmlElement* action_elem,
                        const ContentParserMap& content_parsers,
                        const TransportParserMap& transport_parsers,
                        const CandidateTranslatorMap& translators,
                        SessionAccept* accept,
                        ParseError* error) {
  bool expect_transports = true;
  return ParseContentMessage(protocol, action_elem, expect_transports,
                             content_parsers, transport_parsers, translators,
                             accept, error);
}

bool WriteSessionAccept(SignalingProtocol protocol,
                        const ContentInfos& contents,
                        const TransportInfos& tinfos,
                        const ContentParserMap& content_parsers,
                        const TransportParserMap& transport_parsers,
                        const CandidateTranslatorMap& translators,
                        const ContentGroups& groups,
                        XmlElements* elems,
                        WriteError* error) {
  return WriteContentMessage(protocol, contents, tinfos,
                             content_parsers, transport_parsers, translators,
                             groups,
                             elems, error);
}

bool ParseSessionTerminate(SignalingProtocol protocol,
                           const buzz::XmlElement* action_elem,
                           SessionTerminate* term,
                           ParseError* error) {
  if (protocol == PROTOCOL_GINGLE) {
    const buzz::XmlElement* reason_elem = action_elem->FirstElement();
    if (reason_elem != NULL) {
      term->reason = reason_elem->Name().LocalPart();
      const buzz::XmlElement *debug_elem = reason_elem->FirstElement();
      if (debug_elem != NULL) {
        term->debug_reason = debug_elem->Name().LocalPart();
      }
    }
    return true;
  } else {
    const buzz::XmlElement* reason_elem =
        action_elem->FirstNamed(QN_JINGLE_REASON);
    if (reason_elem) {
      reason_elem = reason_elem->FirstElement();
      if (reason_elem) {
        term->reason = reason_elem->Name().LocalPart();
      }
    }
    return true;
  }
}

void WriteSessionTerminate(SignalingProtocol protocol,
                           const SessionTerminate& term,
                           XmlElements* elems) {
  if (protocol == PROTOCOL_GINGLE) {
    elems->push_back(new buzz::XmlElement(buzz::QName(NS_GINGLE, term.reason)));
  } else {
    if (!term.reason.empty()) {
      buzz::XmlElement* reason_elem = new buzz::XmlElement(QN_JINGLE_REASON);
      reason_elem->AddElement(new buzz::XmlElement(
          buzz::QName(NS_JINGLE, term.reason)));
      elems->push_back(reason_elem);
    }
  }
}

bool ParseDescriptionInfo(SignalingProtocol protocol,
                          const buzz::XmlElement* action_elem,
                          const ContentParserMap& content_parsers,
                          const TransportParserMap& transport_parsers,
                          const CandidateTranslatorMap& translators,
                          DescriptionInfo* description_info,
                          ParseError* error) {
  bool expect_transports = false;
  return ParseContentMessage(protocol, action_elem, expect_transports,
                             content_parsers, transport_parsers, translators,
                             description_info, error);
}

bool WriteDescriptionInfo(SignalingProtocol protocol,
                          const ContentInfos& contents,
                          const ContentParserMap& content_parsers,
                          XmlElements* elems,
                          WriteError* error) {
  if (protocol == PROTOCOL_GINGLE) {
    return WriteGingleContentInfos(contents, content_parsers, elems, error);
  } else {
    return WriteJingleContentInfos(contents, content_parsers, elems, error);
  }
}

bool ParseTransportInfos(SignalingProtocol protocol,
                         const buzz::XmlElement* action_elem,
                         const ContentInfos& contents,
                         const TransportParserMap& trans_parsers,
                         const CandidateTranslatorMap& translators,
                         TransportInfos* tinfos,
                         ParseError* error) {
  if (protocol == PROTOCOL_GINGLE) {
    return ParseGingleTransportInfos(
        action_elem, contents, trans_parsers, translators, tinfos, error);
  } else {
    return ParseJingleTransportInfos(
        action_elem, contents, trans_parsers, translators, tinfos, error);
  }
}

bool WriteTransportInfos(SignalingProtocol protocol,
                         const TransportInfos& tinfos,
                         const TransportParserMap& trans_parsers,
                         const CandidateTranslatorMap& translators,
                         XmlElements* elems,
                         WriteError* error) {
  if (protocol == PROTOCOL_GINGLE) {
    return WriteGingleTransportInfos(tinfos, trans_parsers, translators,
                                     elems, error);
  } else {
    return WriteJingleTransportInfos(tinfos, trans_parsers, translators,
                                     elems, error);
  }
}

bool GetUriTarget(const std::string& prefix, const std::string& str,
                  std::string* after) {
  size_t pos = str.find(prefix);
  if (pos == std::string::npos)
    return false;

  *after = str.substr(pos + prefix.size(), std::string::npos);
  return true;
}

bool FindSessionRedirect(const buzz::XmlElement* stanza,
                         SessionRedirect* redirect) {
  const buzz::XmlElement* error_elem = GetXmlChild(stanza, LN_ERROR);
  if (error_elem == NULL)
    return false;

  const buzz::XmlElement* redirect_elem =
      error_elem->FirstNamed(QN_GINGLE_REDIRECT);
  if (redirect_elem == NULL)
    redirect_elem = error_elem->FirstNamed(buzz::QN_STANZA_REDIRECT);
  if (redirect_elem == NULL)
    return false;

  if (!GetUriTarget(STR_REDIRECT_PREFIX, redirect_elem->BodyText(),
                    &redirect->target))
    return false;

  return true;
}

}  // namespace cricket
