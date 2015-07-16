/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#include "talk/app/webrtc/jsepsessiondescription.h"

#include "talk/app/webrtc/webrtcsdp.h"
#include "talk/session/media/mediasession.h"
#include "webrtc/base/stringencode.h"

using rtc::scoped_ptr;
using cricket::SessionDescription;

namespace webrtc {

static const char* kSupportedTypes[] = {
    JsepSessionDescription::kOffer,
    JsepSessionDescription::kPrAnswer,
    JsepSessionDescription::kAnswer
};

static bool IsTypeSupported(const std::string& type) {
  bool type_supported = false;
  for (size_t i = 0; i < ARRAY_SIZE(kSupportedTypes); ++i) {
    if (kSupportedTypes[i] == type) {
      type_supported = true;
      break;
    }
  }
  return type_supported;
}

const char SessionDescriptionInterface::kOffer[] = "offer";
const char SessionDescriptionInterface::kPrAnswer[] = "pranswer";
const char SessionDescriptionInterface::kAnswer[] = "answer";

const int JsepSessionDescription::kDefaultVideoCodecId = 100;
// This is effectively a max value of the frame rate. 30 is default from camera.
const int JsepSessionDescription::kDefaultVideoCodecFramerate = 60;
const char JsepSessionDescription::kDefaultVideoCodecName[] = "VP8";
// Used as default max video codec size before we have it in signaling.
#if defined(ANDROID) || defined(WEBRTC_IOS)
// Limit default max video codec size for Android to avoid
// HW VP8 codec initialization failure for resolutions higher
// than 1280x720 or 720x1280.
// Same patch for iOS to support 720P in portrait mode.
const int JsepSessionDescription::kMaxVideoCodecWidth = 1280;
const int JsepSessionDescription::kMaxVideoCodecHeight = 1280;
#else
const int JsepSessionDescription::kMaxVideoCodecWidth = 1920;
const int JsepSessionDescription::kMaxVideoCodecHeight = 1080;
#endif
const int JsepSessionDescription::kDefaultVideoCodecPreference = 1;

SessionDescriptionInterface* CreateSessionDescription(const std::string& type,
                                                      const std::string& sdp,
                                                      SdpParseError* error) {
  if (!IsTypeSupported(type)) {
    return NULL;
  }

  JsepSessionDescription* jsep_desc = new JsepSessionDescription(type);
  if (!jsep_desc->Initialize(sdp, error)) {
    delete jsep_desc;
    return NULL;
  }
  return jsep_desc;
}

JsepSessionDescription::JsepSessionDescription(const std::string& type)
    : type_(type) {
}

JsepSessionDescription::~JsepSessionDescription() {}

bool JsepSessionDescription::Initialize(
    cricket::SessionDescription* description,
    const std::string& session_id,
    const std::string& session_version) {
  if (!description)
    return false;

  session_id_ = session_id;
  session_version_ = session_version;
  description_.reset(description);
  candidate_collection_.resize(number_of_mediasections());
  return true;
}

bool JsepSessionDescription::Initialize(const std::string& sdp,
                                        SdpParseError* error) {
  return SdpDeserialize(sdp, this, error);
}

bool JsepSessionDescription::AddCandidate(
    const IceCandidateInterface* candidate) {
  if (!candidate || candidate->sdp_mline_index() < 0)
    return false;
  size_t mediasection_index = 0;
  if (!GetMediasectionIndex(candidate, &mediasection_index)) {
    return false;
  }
  if (mediasection_index >= number_of_mediasections())
    return false;
  const std::string& content_name =
      description_->contents()[mediasection_index].name;
  const cricket::TransportInfo* transport_info =
      description_->GetTransportInfoByName(content_name);
  if (!transport_info) {
    return false;
  }

  cricket::Candidate updated_candidate = candidate->candidate();
  if (updated_candidate.username().empty()) {
    updated_candidate.set_username(transport_info->description.ice_ufrag);
  }
  if (updated_candidate.password().empty()) {
    updated_candidate.set_password(transport_info->description.ice_pwd);
  }

  scoped_ptr<JsepIceCandidate> updated_candidate_wrapper(
      new JsepIceCandidate(candidate->sdp_mid(),
                           static_cast<int>(mediasection_index),
                           updated_candidate));
  if (!candidate_collection_[mediasection_index].HasCandidate(
          updated_candidate_wrapper.get()))
    candidate_collection_[mediasection_index].add(
        updated_candidate_wrapper.release());

  return true;
}

size_t JsepSessionDescription::number_of_mediasections() const {
  if (!description_)
    return 0;
  return description_->contents().size();
}

const IceCandidateCollection* JsepSessionDescription::candidates(
    size_t mediasection_index) const {
  if (mediasection_index >= candidate_collection_.size())
    return NULL;
  return &candidate_collection_[mediasection_index];
}

bool JsepSessionDescription::ToString(std::string* out) const {
  if (!description_ || !out)
    return false;
  *out = SdpSerialize(*this);
  return !out->empty();
}

bool JsepSessionDescription::GetMediasectionIndex(
    const IceCandidateInterface* candidate,
    size_t* index) {
  if (!candidate || !index) {
    return false;
  }
  *index = static_cast<size_t>(candidate->sdp_mline_index());
  if (description_ && !candidate->sdp_mid().empty()) {
    bool found = false;
    // Try to match the sdp_mid with content name.
    for (size_t i = 0; i < description_->contents().size(); ++i) {
      if (candidate->sdp_mid() == description_->contents().at(i).name) {
        *index = i;
        found = true;
        break;
      }
    }
    if (!found) {
      // If the sdp_mid is presented but we can't find a match, we consider
      // this as an error.
      return false;
    }
  }
  return true;
}

}  // namespace webrtc
