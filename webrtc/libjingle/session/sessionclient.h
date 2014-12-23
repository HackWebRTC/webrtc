/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_SESSIONCLIENT_H_
#define WEBRTC_P2P_BASE_SESSIONCLIENT_H_

#include "webrtc/libjingle/session/constants.h"
#include "webrtc/p2p/base/constants.h"

namespace buzz {
class XmlElement;
}

namespace cricket {

struct ParseError;
struct WriteError;
class Session;
class ContentDescription;

class ContentParser {
 public:
  virtual bool ParseContent(SignalingProtocol protocol,
                            const buzz::XmlElement* elem,
                            ContentDescription** content,
                            ParseError* error) = 0;
  // If not IsWriteable, then a given content should be "skipped" when
  // writing in the given protocol, as if it didn't exist.  We assume
  // most things are writeable.  We do this to avoid strange cases
  // like data contents in Gingle, which aren't writable.
  virtual bool IsWritable(SignalingProtocol protocol,
                          const ContentDescription* content) {
    return true;
  }
  virtual bool WriteContent(SignalingProtocol protocol,
                            const ContentDescription* content,
                            buzz::XmlElement** elem,
                            WriteError* error) = 0;
  virtual ~ContentParser() {}
};

// A SessionClient exists in 1-1 relation with each session.  The implementor
// of this interface is the one that understands *what* the two sides are
// trying to send to one another.  The lower-level layers only know how to send
// data; they do not know what is being sent.
class SessionClient : public ContentParser {
 public:
  // Notifies the client of the creation / destruction of sessions of this type.
  //
  // IMPORTANT: The SessionClient, in its handling of OnSessionCreate, must
  // create whatever channels are indicate in the description.  This is because
  // the remote client may already be attempting to connect those channels. If
  // we do not create our channel right away, then connection may fail or be
  // delayed.
  virtual void OnSessionCreate(Session* session, bool received_initiate) = 0;
  virtual void OnSessionDestroy(Session* session) = 0;

  virtual bool ParseContent(SignalingProtocol protocol,
                            const buzz::XmlElement* elem,
                            ContentDescription** content,
                            ParseError* error) = 0;
  virtual bool WriteContent(SignalingProtocol protocol,
                            const ContentDescription* content,
                            buzz::XmlElement** elem,
                            WriteError* error) = 0;
 protected:
  // The SessionClient interface explicitly does not include destructor
  virtual ~SessionClient() { }
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_SESSIONCLIENT_H_
