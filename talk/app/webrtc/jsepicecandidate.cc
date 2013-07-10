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

#include "talk/app/webrtc/jsepicecandidate.h"

#include <vector>

#include "talk/app/webrtc/webrtcsdp.h"
#include "talk/base/stringencode.h"

namespace webrtc {

IceCandidateInterface* CreateIceCandidate(const std::string& sdp_mid,
                                          int sdp_mline_index,
                                          const std::string& sdp) {
  return CreateIceCandidate(sdp_mid, sdp_mline_index, sdp, NULL);
}

IceCandidateInterface* CreateIceCandidate(const std::string& sdp_mid,
                                          int sdp_mline_index,
                                          const std::string& sdp,
                                          SdpParseError* error) {
  JsepIceCandidate* jsep_ice = new JsepIceCandidate(sdp_mid, sdp_mline_index);
  if (!jsep_ice->Initialize(sdp, error)) {
    delete jsep_ice;
    return NULL;
  }
  return jsep_ice;
}

JsepIceCandidate::JsepIceCandidate(const std::string& sdp_mid,
                                   int sdp_mline_index)
    : sdp_mid_(sdp_mid),
      sdp_mline_index_(sdp_mline_index) {
}

JsepIceCandidate::JsepIceCandidate(const std::string& sdp_mid,
                                   int sdp_mline_index,
                                   const cricket::Candidate& candidate)
    : sdp_mid_(sdp_mid),
      sdp_mline_index_(sdp_mline_index),
      candidate_(candidate) {
}

JsepIceCandidate::~JsepIceCandidate() {
}

bool JsepIceCandidate::Initialize(const std::string& sdp, SdpParseError* err) {
  return SdpDeserializeCandidate(sdp, this, err);
}

bool JsepIceCandidate::ToString(std::string* out) const {
  if (!out)
    return false;
  *out = SdpSerializeCandidate(*this);
  return !out->empty();
}

JsepCandidateCollection::~JsepCandidateCollection() {
  for (std::vector<JsepIceCandidate*>::iterator it = candidates_.begin();
       it != candidates_.end(); ++it) {
    delete *it;
  }
}

bool JsepCandidateCollection::HasCandidate(
    const IceCandidateInterface* candidate) const {
  bool ret = false;
  for (std::vector<JsepIceCandidate*>::const_iterator it = candidates_.begin();
      it != candidates_.end(); ++it) {
    if ((*it)->sdp_mid() == candidate->sdp_mid() &&
        (*it)->sdp_mline_index() == candidate->sdp_mline_index() &&
        (*it)->candidate().IsEquivalent(candidate->candidate())) {
      ret = true;
      break;
    }
  }
  return ret;
}

}  // namespace webrtc
