/*
 * libjingle
 * Copyright 2010 Google Inc.
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

/*
 * Documentation is in mediamessages.h.
 */

#include "talk/session/media/mediamessages.h"

#include "talk/base/logging.h"
#include "talk/base/stringencode.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/parsing.h"
#include "talk/session/media/mediasessionclient.h"
#include "talk/xmllite/xmlelement.h"

namespace cricket {

namespace {

// NOTE: There is no check here for duplicate streams, so check before
// adding.
void AddStream(std::vector<StreamParams>* streams, const StreamParams& stream) {
  streams->push_back(stream);
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

// Builds a <view> element according to the following spec:
// goto/jinglemuc
buzz::XmlElement* CreateViewElem(const std::string& name,
                                 const std::string& type) {
  buzz::XmlElement* view_elem =
      new buzz::XmlElement(QN_JINGLE_DRAFT_VIEW, true);
  view_elem->AddAttr(QN_NAME, name);
  view_elem->SetAttr(QN_TYPE, type);
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
  AddXmlAttr(view_elem, QN_SSRC, view.selector.ssrc);

  buzz::XmlElement* params_elem = new buzz::XmlElement(QN_JINGLE_DRAFT_PARAMS);
  AddXmlAttr(params_elem, QN_WIDTH, view.width);
  AddXmlAttr(params_elem, QN_HEIGHT, view.height);
  AddXmlAttr(params_elem, QN_FRAMERATE, view.framerate);
  AddXmlAttr(params_elem, QN_PREFERENCE, view.preference);
  view_elem->AddElement(params_elem);

  return view_elem;
}

}  //  namespace

bool MediaStreams::GetAudioStream(
    const StreamSelector& selector, StreamParams* stream) {
  return GetStream(audio_, selector, stream);
}

bool MediaStreams::GetVideoStream(
    const StreamSelector& selector, StreamParams* stream) {
  return GetStream(video_, selector, stream);
}

bool MediaStreams::GetDataStream(
    const StreamSelector& selector, StreamParams* stream) {
  return GetStream(data_, selector, stream);
}

void MediaStreams::CopyFrom(const MediaStreams& streams) {
  audio_ = streams.audio_;
  video_ = streams.video_;
  data_ = streams.data_;
}

void MediaStreams::AddAudioStream(const StreamParams& stream) {
  AddStream(&audio_, stream);
}

void MediaStreams::AddVideoStream(const StreamParams& stream) {
  AddStream(&video_, stream);
}

void MediaStreams::AddDataStream(const StreamParams& stream) {
  AddStream(&data_, stream);
}

bool MediaStreams::RemoveAudioStream(
    const StreamSelector& selector) {
  return RemoveStream(&audio_, selector);
}

bool MediaStreams::RemoveVideoStream(
    const StreamSelector& selector) {
  return RemoveStream(&video_, selector);
}

bool MediaStreams::RemoveDataStream(
    const StreamSelector& selector) {
  return RemoveStream(&data_, selector);
}

bool IsJingleViewRequest(const buzz::XmlElement* action_elem) {
  return action_elem->FirstNamed(QN_JINGLE_DRAFT_VIEW) != NULL;
}

bool ParseStaticVideoView(const buzz::XmlElement* view_elem,
                          StaticVideoView* view,
                          ParseError* error) {
  uint32 ssrc;
  if (!ParseSsrc(view_elem->Attr(QN_SSRC), &ssrc)) {
    return BadParse("Invalid or missing view ssrc.", error);
  }
  view->selector = StreamSelector(ssrc);

  const buzz::XmlElement* params_elem =
      view_elem->FirstNamed(QN_JINGLE_DRAFT_PARAMS);
  if (params_elem) {
    view->width = GetXmlAttr(params_elem, QN_WIDTH, 0);
    view->height = GetXmlAttr(params_elem, QN_HEIGHT, 0);
    view->framerate = GetXmlAttr(params_elem, QN_FRAMERATE, 0);
    view->preference = GetXmlAttr(params_elem, QN_PREFERENCE, 0);
  } else {
    return BadParse("Missing view params.", error);
  }

  return true;
}

bool ParseJingleViewRequest(const buzz::XmlElement* action_elem,
                            ViewRequest* view_request,
                            ParseError* error) {
  for (const buzz::XmlElement* view_elem =
           action_elem->FirstNamed(QN_JINGLE_DRAFT_VIEW);
       view_elem != NULL;
       view_elem = view_elem->NextNamed(QN_JINGLE_DRAFT_VIEW)) {
    std::string type = view_elem->Attr(QN_TYPE);
    if (STR_JINGLE_DRAFT_VIEW_TYPE_NONE == type) {
      view_request->static_video_views.clear();
      return true;
    } else if (STR_JINGLE_DRAFT_VIEW_TYPE_STATIC == type) {
      StaticVideoView static_video_view(StreamSelector(0), 0, 0, 0);
      if (!ParseStaticVideoView(view_elem, &static_video_view, error)) {
        return false;
      }
      view_request->static_video_views.push_back(static_video_view);
    } else {
      LOG(LS_INFO) << "Ingnoring unknown view type: " << type;
    }
  }
  return true;
}

bool WriteJingleViewRequest(const std::string& content_name,
                            const ViewRequest& request,
                            XmlElements* elems,
                            WriteError* error) {
  if (request.static_video_views.empty()) {
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

bool ParseSsrcAsLegacyStream(const buzz::XmlElement* desc_elem,
                             std::vector<StreamParams>* streams,
                             ParseError* error) {
  const std::string ssrc_str = desc_elem->Attr(QN_SSRC);
  if (!ssrc_str.empty()) {
    uint32 ssrc;
    if (!ParseSsrc(ssrc_str, &ssrc)) {
      return BadParse("Missing or invalid ssrc.", error);
    }

    streams->push_back(StreamParams::CreateLegacy(ssrc));
  }
  return true;
}

bool ParseSsrcs(const buzz::XmlElement* parent_elem,
                std::vector<uint32>* ssrcs,
                ParseError* error) {
  for (const buzz::XmlElement* ssrc_elem =
           parent_elem->FirstNamed(QN_JINGLE_DRAFT_SSRC);
       ssrc_elem != NULL;
       ssrc_elem = ssrc_elem->NextNamed(QN_JINGLE_DRAFT_SSRC)) {
    uint32 ssrc;
    if (!ParseSsrc(ssrc_elem->BodyText(), &ssrc)) {
      return BadParse("Missing or invalid ssrc.", error);
    }

    ssrcs->push_back(ssrc);
  }
  return true;
}

bool ParseSsrcGroups(const buzz::XmlElement* parent_elem,
                     std::vector<SsrcGroup>* ssrc_groups,
                     ParseError* error) {
  for (const buzz::XmlElement* group_elem =
           parent_elem->FirstNamed(QN_JINGLE_DRAFT_SSRC_GROUP);
       group_elem != NULL;
       group_elem = group_elem->NextNamed(QN_JINGLE_DRAFT_SSRC_GROUP)) {
    std::string semantics = group_elem->Attr(QN_SEMANTICS);
    std::vector<uint32> ssrcs;
    if (!ParseSsrcs(group_elem, &ssrcs, error)) {
      return false;
    }
    ssrc_groups->push_back(SsrcGroup(semantics, ssrcs));
  }
  return true;
}

bool ParseJingleStream(const buzz::XmlElement* stream_elem,
                       std::vector<StreamParams>* streams,
                       ParseError* error) {
  StreamParams stream;
  // We treat the nick as a stream groupid.
  stream.groupid = stream_elem->Attr(QN_NICK);
  stream.id = stream_elem->Attr(QN_NAME);
  stream.type = stream_elem->Attr(QN_TYPE);
  stream.display = stream_elem->Attr(QN_DISPLAY);
  stream.cname = stream_elem->Attr(QN_CNAME);
  if (!ParseSsrcs(stream_elem, &(stream.ssrcs), error)) {
    return false;
  }
  std::vector<SsrcGroup> ssrc_groups;
  if (!ParseSsrcGroups(stream_elem, &(stream.ssrc_groups), error)) {
    return false;
  }
  streams->push_back(stream);
  return true;
}

bool ParseJingleRtpHeaderExtensions(const buzz::XmlElement* parent_elem,
                                    std::vector<RtpHeaderExtension>* hdrexts,
                                    ParseError* error) {
  for (const buzz::XmlElement* hdrext_elem =
           parent_elem->FirstNamed(QN_JINGLE_RTP_HDREXT);
       hdrext_elem != NULL;
       hdrext_elem = hdrext_elem->NextNamed(QN_JINGLE_RTP_HDREXT)) {
    std::string uri = hdrext_elem->Attr(QN_URI);
    int id = GetXmlAttr(hdrext_elem, QN_ID, 0);
    if (id <= 0) {
      return BadParse("Invalid RTP header extension id.", error);
    }
    hdrexts->push_back(RtpHeaderExtension(uri, id));
  }
  return true;
}

bool HasJingleStreams(const buzz::XmlElement* desc_elem) {
  const buzz::XmlElement* streams_elem =
      desc_elem->FirstNamed(QN_JINGLE_DRAFT_STREAMS);
  return (streams_elem != NULL);
}

bool ParseJingleStreams(const buzz::XmlElement* desc_elem,
                        std::vector<StreamParams>* streams,
                        ParseError* error) {
  const buzz::XmlElement* streams_elem =
      desc_elem->FirstNamed(QN_JINGLE_DRAFT_STREAMS);
  if (streams_elem == NULL) {
    return BadParse("Missing streams element.", error);
  }
  for (const buzz::XmlElement* stream_elem =
           streams_elem->FirstNamed(QN_JINGLE_DRAFT_STREAM);
       stream_elem != NULL;
       stream_elem = stream_elem->NextNamed(QN_JINGLE_DRAFT_STREAM)) {
    if (!ParseJingleStream(stream_elem, streams, error)) {
      return false;
    }
  }
  return true;
}

void WriteSsrcs(const std::vector<uint32>& ssrcs,
                buzz::XmlElement* parent_elem) {
  for (std::vector<uint32>::const_iterator ssrc = ssrcs.begin();
       ssrc != ssrcs.end(); ++ssrc) {
    buzz::XmlElement* ssrc_elem =
        new buzz::XmlElement(QN_JINGLE_DRAFT_SSRC, false);
    SetXmlBody(ssrc_elem, *ssrc);

    parent_elem->AddElement(ssrc_elem);
  }
}

void WriteSsrcGroups(const std::vector<SsrcGroup>& groups,
                     buzz::XmlElement* parent_elem) {
  for (std::vector<SsrcGroup>::const_iterator group = groups.begin();
       group != groups.end(); ++group) {
    buzz::XmlElement* group_elem =
        new buzz::XmlElement(QN_JINGLE_DRAFT_SSRC_GROUP, false);
    AddXmlAttrIfNonEmpty(group_elem, QN_SEMANTICS, group->semantics);
    WriteSsrcs(group->ssrcs, group_elem);

    parent_elem->AddElement(group_elem);
  }
}

void WriteJingleStream(const StreamParams& stream,
                       buzz::XmlElement* parent_elem) {
  buzz::XmlElement* stream_elem =
      new buzz::XmlElement(QN_JINGLE_DRAFT_STREAM, false);
  // We treat the nick as a stream groupid.
  AddXmlAttrIfNonEmpty(stream_elem, QN_NICK, stream.groupid);
  AddXmlAttrIfNonEmpty(stream_elem, QN_NAME, stream.id);
  AddXmlAttrIfNonEmpty(stream_elem, QN_TYPE, stream.type);
  AddXmlAttrIfNonEmpty(stream_elem, QN_DISPLAY, stream.display);
  AddXmlAttrIfNonEmpty(stream_elem, QN_CNAME, stream.cname);
  WriteSsrcs(stream.ssrcs, stream_elem);
  WriteSsrcGroups(stream.ssrc_groups, stream_elem);

  parent_elem->AddElement(stream_elem);
}

void WriteJingleStreams(const std::vector<StreamParams>& streams,
                        buzz::XmlElement* parent_elem) {
  buzz::XmlElement* streams_elem =
      new buzz::XmlElement(QN_JINGLE_DRAFT_STREAMS, true);
  for (std::vector<StreamParams>::const_iterator stream = streams.begin();
       stream != streams.end(); ++stream) {
    WriteJingleStream(*stream, streams_elem);
  }

  parent_elem->AddElement(streams_elem);
}

void WriteJingleRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& hdrexts,
    buzz::XmlElement* parent_elem) {
  for (std::vector<RtpHeaderExtension>::const_iterator hdrext = hdrexts.begin();
       hdrext != hdrexts.end(); ++hdrext) {
    buzz::XmlElement* hdrext_elem =
      new buzz::XmlElement(QN_JINGLE_RTP_HDREXT, false);
    AddXmlAttr(hdrext_elem, QN_URI, hdrext->uri);
    AddXmlAttr(hdrext_elem, QN_ID, hdrext->id);
    parent_elem->AddElement(hdrext_elem);
  }
}


}  // namespace cricket
