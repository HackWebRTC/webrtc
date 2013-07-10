/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#ifndef TALK_P2P_BASE_SESSIONCLIENT_H_
#define TALK_P2P_BASE_SESSIONCLIENT_H_

#include "talk/p2p/base/constants.h"

namespace buzz {
class XmlElement;
}

namespace cricket {

struct ParseError;
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

#endif  // TALK_P2P_BASE_SESSIONCLIENT_H_
