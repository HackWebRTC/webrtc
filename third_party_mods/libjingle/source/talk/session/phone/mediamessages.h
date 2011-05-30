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

#ifndef TALK_SESSION_PHONE_MEDIAMESSAGES_H_
#define TALK_SESSION_PHONE_MEDIAMESSAGES_H_

#include <string>
#include <vector>
#include "talk/base/basictypes.h"
#include "talk/p2p/base/parsing.h"
#include "talk/p2p/base/sessiondescription.h"

namespace cricket {

struct NamedSource {
  NamedSource() : ssrc(0), ssrc_set(false), removed(false) {}

  void SetSsrc(uint32 ssrc) {
    this->ssrc = ssrc;
    this->ssrc_set = true;
  }

  std::string nick;
  std::string name;
  std::string usage;
  uint32 ssrc;
  bool ssrc_set;
  bool removed;
};

typedef std::vector<NamedSource> NamedSources;

class MediaSources {
 public:
  const NamedSource* GetAudioSourceBySsrc(uint32 ssrc);
  const NamedSource* GetVideoSourceBySsrc(uint32 ssrc);
  // TODO: Remove once all senders use excplict remove by ssrc.
  const NamedSource* GetFirstAudioSourceByNick(const std::string& nick);
  const NamedSource* GetFirstVideoSourceByNick(const std::string& nick);
  void AddAudioSource(const NamedSource& source);
  void AddVideoSource(const NamedSource& source);
  void RemoveAudioSourceBySsrc(uint32 ssrc);
  void RemoveVideoSourceBySsrc(uint32 ssrc);
  NamedSources audio;
  NamedSources video;
};

struct StaticVideoView {
  StaticVideoView(uint32 ssrc, int width, int height, int framerate)
      : ssrc(ssrc),
        width(width),
        height(height),
        framerate(framerate),
        preference(0) {}

  uint32 ssrc;
  int width;
  int height;
  int framerate;
  int preference;
};

typedef std::vector<StaticVideoView> StaticVideoViews;

struct ViewRequest {
  StaticVideoViews static_video_views;
};

bool WriteViewRequest(const std::string& content_name,
                      const ViewRequest& view,
                      XmlElements* elems,
                      WriteError* error);

bool IsSourcesNotify(const buzz::XmlElement* action_elem);
// The session_description is needed to map content_name => media type.
bool ParseSourcesNotify(const buzz::XmlElement* action_elem,
                        const SessionDescription* session_description,
                        MediaSources* sources,
                        ParseError* error);
}  // namespace cricket

#endif  // TALK_SESSION_PHONE_MEDIAMESSAGES_H_
