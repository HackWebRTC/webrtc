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
 * A collection of functions and types for serializing and
 * deserializing Jingle session messages related to media.
 * Specificially, the <notify> and <view> messages.  They are not yet
 * standardized, but their current documentation can be found at:
 * goto/jinglemuc
 */

#ifndef WEBRTC_LIBJINGLE_SESSION_MEDIA_MEDIAMESSAGES_H_
#define WEBRTC_LIBJINGLE_SESSION_MEDIA_MEDIAMESSAGES_H_

#include <string>
#include <vector>

#include "talk/media/base/mediachannel.h"  // For RtpHeaderExtension
#include "talk/media/base/streamparams.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/libjingle/session/parsing.h"
#include "webrtc/p2p/base/sessiondescription.h"

namespace cricket {

// If the parent element (usually <jingle>) is a jingle view.
bool IsJingleViewRequest(const buzz::XmlElement* action_elem);

// Parses a view request from the parent element (usually
// <jingle>). If it fails, it returns false and fills an error
// message.
bool ParseJingleViewRequest(const buzz::XmlElement* action_elem,
                            ViewRequest* view_request,
                            ParseError* error);

// Serializes a view request to XML.  If it fails, returns false and
// fills in an error message.
bool WriteJingleViewRequest(const std::string& content_name,
                            const ViewRequest& view,
                            XmlElements* elems,
                            WriteError* error);

// TODO(pthatcher): Get rid of legacy source notify and replace with
// description-info as soon as reflector is capable of sending it.
bool IsSourcesNotify(const buzz::XmlElement* action_elem);

// If the given elem has <streams>.
bool HasJingleStreams(const buzz::XmlElement* desc_elem);

// Parses streams from a jingle <description>.  If it fails, returns
// false and fills an error message.
bool ParseJingleStreams(const buzz::XmlElement* desc_elem,
                        std::vector<StreamParams>* streams,
                        ParseError* error);

// Write a <streams> element to the parent_elem.
void WriteJingleStreams(const std::vector<StreamParams>& streams,
                        buzz::XmlElement* parent_elem);

// Parses rtp header extensions from a jingle <description>.  If it
// fails, returns false and fills an error message.
bool ParseJingleRtpHeaderExtensions(
    const buzz::XmlElement* desc_elem,
    std::vector<RtpHeaderExtension>* hdrexts,
    ParseError* error);

// Writes <rtp-hdrext> elements to the parent_elem.
void WriteJingleRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& hdrexts,
    buzz::XmlElement* parent_elem);

}  // namespace cricket

#endif  // WEBRTC_LIBJINGLE_SESSION_MEDIA_MEDIAMESSAGES_H_
