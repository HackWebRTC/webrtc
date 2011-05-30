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

#include "talk/session/phone/mediamessages.h"

#include "talk/base/stringencode.h"
#include "talk/p2p/base/constants.h"
#include "talk/session/phone/mediasessionclient.h"
#include "talk/xmllite/xmlelement.h"

namespace cricket {

const NamedSource* GetFirstSourceByNick(const NamedSources& sources,
                                        const std::string& nick) {
  for (NamedSources::const_iterator source = sources.begin();
       source != sources.end(); ++source) {
    if (source->nick == nick) {
      return &*source;
    }
  }
  return NULL;
}

const NamedSource* GetSourceBySsrc(const NamedSources& sources, uint32 ssrc) {
  for (NamedSources::const_iterator source = sources.begin();
       source != sources.end(); ++source) {
    if (source->ssrc == ssrc) {
      return &*source;
    }
  }
  return NULL;
}

const NamedSource* MediaSources::GetFirstAudioSourceByNick(
    const std::string& nick) {
  return GetFirstSourceByNick(audio, nick);
}

const NamedSource* MediaSources::GetFirstVideoSourceByNick(
    const std::string& nick) {
  return GetFirstSourceByNick(video, nick);
}

const NamedSource* MediaSources::GetAudioSourceBySsrc(uint32 ssrc) {
  return GetSourceBySsrc(audio, ssrc);
}

const NamedSource* MediaSources::GetVideoSourceBySsrc(uint32 ssrc) {
  return GetSourceBySsrc(video, ssrc);
}

// NOTE: There is no check here for duplicate sources, so check before
// adding.
void AddSource(NamedSources* sources, const NamedSource& source) {
  sources->push_back(source);
}

void MediaSources::AddAudioSource(const NamedSource& source) {
  AddSource(&audio, source);
}

void MediaSources::AddVideoSource(const NamedSource& source) {
  AddSource(&video, source);
}

void RemoveSourceBySsrc(NamedSources* sources, uint32 ssrc) {
  for (NamedSources::iterator source = sources->begin();
       source != sources->end(); ) {
    if (source->ssrc == ssrc) {
      source = sources->erase(source);
    } else {
      ++source;
    }
  }
}

void MediaSources::RemoveAudioSourceBySsrc(uint32 ssrc) {
  RemoveSourceBySsrc(&audio, ssrc);
}

void MediaSources::RemoveVideoSourceBySsrc(uint32 ssrc) {
  RemoveSourceBySsrc(&video, ssrc);
}

bool ParseSsrc(const std::string& string, uint32* ssrc) {
  return talk_base::FromString(string, ssrc);
}

bool ParseSsrc(const buzz::XmlElement* element, uint32* ssrc) {
  if (element == NULL) {
    return false;
  }
  return ParseSsrc(element->BodyText(), ssrc);
}

bool ParseNamedSource(const buzz::XmlElement* source_elem,
                      NamedSource* named_source,
                      ParseError* error) {
  named_source->nick = source_elem->Attr(QN_JINGLE_DRAFT_SOURCE_NICK);
  if (named_source->nick.empty()) {
    return BadParse("Missing or invalid nick.", error);
  }

  named_source->name = source_elem->Attr(QN_JINGLE_DRAFT_SOURCE_NAME);
  named_source->usage = source_elem->Attr(QN_JINGLE_DRAFT_SOURCE_USAGE);
  named_source->removed =
      (STR_JINGLE_DRAFT_SOURCE_STATE_REMOVED ==
       source_elem->Attr(QN_JINGLE_DRAFT_SOURCE_STATE));

  const buzz::XmlElement* ssrc_elem =
      source_elem->FirstNamed(QN_JINGLE_DRAFT_SOURCE_SSRC);
  if (ssrc_elem != NULL && !ssrc_elem->BodyText().empty()) {
    uint32 ssrc;
    if (!ParseSsrc(ssrc_elem->BodyText(), &ssrc)) {
      return BadParse("Missing or invalid ssrc.", error);
    }
    named_source->SetSsrc(ssrc);
  }

  return true;
}

bool IsSourcesNotify(const buzz::XmlElement* action_elem) {
  return action_elem->FirstNamed(QN_JINGLE_DRAFT_NOTIFY) != NULL;
}

bool ParseSourcesNotify(const buzz::XmlElement* action_elem,
                        const SessionDescription* session_description,
                        MediaSources* sources,
                        ParseError* error) {
  for (const buzz::XmlElement* notify_elem
           = action_elem->FirstNamed(QN_JINGLE_DRAFT_NOTIFY);
       notify_elem != NULL;
       notify_elem = notify_elem->NextNamed(QN_JINGLE_DRAFT_NOTIFY)) {
    std::string content_name = notify_elem->Attr(QN_JINGLE_DRAFT_CONTENT_NAME);
    for (const buzz::XmlElement* source_elem
             = notify_elem->FirstNamed(QN_JINGLE_DRAFT_SOURCE);
         source_elem != NULL;
         source_elem = source_elem->NextNamed(QN_JINGLE_DRAFT_SOURCE)) {
      NamedSource named_source;
      if (!ParseNamedSource(source_elem, &named_source, error)) {
        return false;
      }

      if (session_description == NULL) {
        return BadParse("unknown content name: " + content_name, error);
      }
      const ContentInfo* content =
          FindContentInfoByName(session_description->contents(), content_name);
      if (content == NULL) {
        return BadParse("unknown content name: " + content_name, error);
      }

      if (IsAudioContent(content)) {
        sources->audio.push_back(named_source);
      } else if (IsVideoContent(content)) {
        sources->video.push_back(named_source);
      }
    }
  }

  return true;
}

buzz::XmlElement* CreateViewElem(const std::string& name,
                                 const std::string& type) {
  buzz::XmlElement* view_elem =
      new buzz::XmlElement(QN_JINGLE_DRAFT_VIEW, true);
  view_elem->AddAttr(QN_JINGLE_DRAFT_CONTENT_NAME, name);
  view_elem->SetAttr(QN_JINGLE_DRAFT_VIEW_TYPE, type);
  return view_elem;
}

buzz::XmlElement* CreateVideoViewElem(const std::string& content_name,
                                      const std::string& type) {
  return CreateViewElem(content_name, type);
}

buzz::XmlElement* CreateNoneVideoViewElem(const std::string& content_name) {
  return CreateVideoViewElem(content_name, STR_JINGLE_DRAFT_VIEW_TYPE_NONE);
}

buzz::XmlElement* CreateStaticVideoViewElem(const std::string& content_name,
                                            const StaticVideoView& view) {
  buzz::XmlElement* view_elem =
      CreateVideoViewElem(content_name, STR_JINGLE_DRAFT_VIEW_TYPE_STATIC);
  AddXmlAttr(view_elem, QN_JINGLE_DRAFT_VIEW_SSRC, view.ssrc);

  buzz::XmlElement* params_elem = new buzz::XmlElement(
      QN_JINGLE_DRAFT_VIEW_PARAMS);
  AddXmlAttr(params_elem, QN_JINGLE_DRAFT_VIEW_PARAMS_WIDTH, view.width);
  AddXmlAttr(params_elem, QN_JINGLE_DRAFT_VIEW_PARAMS_HEIGHT, view.height);
  AddXmlAttr(params_elem, QN_JINGLE_DRAFT_VIEW_PARAMS_FRAMERATE,
             view.framerate);
  AddXmlAttr(params_elem, QN_JINGLE_DRAFT_VIEW_PARAMS_PREFERENCE,
             view.preference);
  view_elem->AddElement(params_elem);

  return view_elem;
}

bool WriteViewRequest(const std::string& content_name,
                      const ViewRequest& request,
                      XmlElements* elems,
                      WriteError* error) {
  if (request.static_video_views.size() == 0) {
    elems->push_back(CreateNoneVideoViewElem(content_name));
  } else {
    for (StaticVideoViews::const_iterator view =
             request.static_video_views.begin();
         view != request.static_video_views.end(); ++view) {
      elems->push_back(CreateStaticVideoViewElem(content_name, *view));
    }
  }
  return true;
}

}  // namespace cricket
