/*
 * libjingle
 * Copyright 2012, Google Inc.
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

// Implements the SessionDescriptionInterface.

#ifndef TALK_APP_WEBRTC_JSEPSESSIONDESCRIPTION_H_
#define TALK_APP_WEBRTC_JSEPSESSIONDESCRIPTION_H_

#include <string>
#include <vector>

#include "talk/app/webrtc/jsep.h"
#include "talk/app/webrtc/jsepicecandidate.h"
#include "talk/base/scoped_ptr.h"

namespace cricket {
class SessionDescription;
}

namespace webrtc {

class JsepSessionDescription : public SessionDescriptionInterface {
 public:
  explicit JsepSessionDescription(const std::string& type);
  virtual ~JsepSessionDescription();

  // |error| can be NULL if don't care about the failure reason.
  bool Initialize(const std::string& sdp, SdpParseError* error);

  // Takes ownership of |description|.
  bool Initialize(cricket::SessionDescription* description,
      const std::string& session_id,
      const std::string& session_version);

  virtual cricket::SessionDescription* description() {
    return description_.get();
  }
  virtual const cricket::SessionDescription* description() const {
    return description_.get();
  }
  virtual std::string session_id() const {
    return session_id_;
  }
  virtual std::string session_version() const {
    return session_version_;
  }
  virtual std::string type() const {
    return type_;
  }
  // Allow changing the type. Used for testing.
  void set_type(const std::string& type) { type_ = type; }
  virtual bool AddCandidate(const IceCandidateInterface* candidate);
  virtual size_t number_of_mediasections() const;
  virtual const IceCandidateCollection* candidates(
      size_t mediasection_index) const;
  virtual bool ToString(std::string* out) const;

  // Default video encoder settings. The resolution is the max resolution.
  // TODO(perkj): Implement proper negotiation of video resolution.
  static const int kDefaultVideoCodecId;
  static const int kDefaultVideoCodecFramerate;
  static const char kDefaultVideoCodecName[];
  static const int kMaxVideoCodecWidth;
  static const int kMaxVideoCodecHeight;
  static const int kDefaultVideoCodecPreference;

 private:
  talk_base::scoped_ptr<cricket::SessionDescription> description_;
  std::string session_id_;
  std::string session_version_;
  std::string type_;
  std::vector<JsepCandidateCollection> candidate_collection_;

  bool GetMediasectionIndex(const IceCandidateInterface* candidate,
                            size_t* index);

  DISALLOW_COPY_AND_ASSIGN(JsepSessionDescription);
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_JSEPSESSIONDESCRIPTION_H_
