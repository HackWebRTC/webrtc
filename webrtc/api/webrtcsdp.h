/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contain functions for parsing and serializing SDP messages.
// Related RFC/draft including:
// * RFC 4566 - SDP
// * RFC 5245 - ICE
// * RFC 3388 - Grouping of Media Lines in SDP
// * RFC 4568 - SDP Security Descriptions for Media Streams
// * draft-lennox-mmusic-sdp-source-selection-02 -
//   Mechanisms for Media Source Selection in SDP

#ifndef WEBRTC_API_WEBRTCSDP_H_
#define WEBRTC_API_WEBRTCSDP_H_

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

#endif  // WEBRTC_API_WEBRTCSDP_H_
