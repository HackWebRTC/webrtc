/*
 * libjingle
 * Copyright 2011, Google Inc.
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

// This file contain functions for parsing and serializing SDP messages.
// Related RFC/draft including:
// * RFC 4566 - SDP
// * RFC 5245 - ICE
// * RFC 3388 - Grouping of Media Lines in SDP
// * RFC 4568 - SDP Security Descriptions for Media Streams
// * draft-lennox-mmusic-sdp-source-selection-02 -
//   Mechanisms for Media Source Selection in SDP

#ifndef TALK_APP_WEBRTC_WEBRTCSDP_H_
#define TALK_APP_WEBRTC_WEBRTCSDP_H_

#include <string>

namespace webrtc {

class IceCandidateInterface;
class JsepIceCandidate;
class JsepSessionDescription;
struct SdpParseError;

// Serializes the passed in JsepSessionDescription.
// Serialize SessionDescription including candidates if
// JsepSessionDescription has candidates.
// jdesc - The JsepSessionDescription object to be serialized.
// return - SDP string serialized from the arguments.
std::string SdpSerialize(const JsepSessionDescription& jdesc);

// Serializes the passed in IceCandidateInterface to a SDP string.
// candidate - The candidate to be serialized.
std::string SdpSerializeCandidate(const IceCandidateInterface& candidate);

// Deserializes the passed in SDP string to a JsepSessionDescription.
// message - SDP string to be Deserialized.
// jdesc - The JsepSessionDescription deserialized from the SDP string.
// error - The detail error information when parsing fails.
// return - true on success, false on failure.
bool SdpDeserialize(const std::string& message,
                    JsepSessionDescription* jdesc,
                    SdpParseError* error);

// Deserializes the passed in SDP string to one JsepIceCandidate.
// The first line must be a=candidate line and only the first line will be
// parsed.
// message - The SDP string to be Deserialized.
// candidates - The JsepIceCandidate from the SDP string.
// error - The detail error information when parsing fails.
// return - true on success, false on failure.
bool SdpDeserializeCandidate(const std::string& message,
                             JsepIceCandidate* candidate,
                             SdpParseError* error);
}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_WEBRTCSDP_H_
