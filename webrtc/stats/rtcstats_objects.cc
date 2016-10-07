/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/stats/rtcstats_objects.h"

namespace webrtc {

const char* RTCIceCandidateType::kHost = "host";
const char* RTCIceCandidateType::kSrflx = "srflx";
const char* RTCIceCandidateType::kPrflx = "prflx";
const char* RTCIceCandidateType::kRelay = "relay";

WEBRTC_RTCSTATS_IMPL(RTCIceCandidateStats, RTCStats, "ice-candidate",
    &ip,
    &port,
    &protocol,
    &candidate_type,
    &priority,
    &url);

RTCIceCandidateStats::RTCIceCandidateStats(
    const std::string& id, int64_t timestamp_us)
    : RTCIceCandidateStats(std::string(id), timestamp_us) {
}

RTCIceCandidateStats::RTCIceCandidateStats(
    std::string&& id, int64_t timestamp_us)
    : RTCStats(std::move(id), timestamp_us),
      ip("ip"),
      port("port"),
      protocol("protocol"),
      candidate_type("candidateType"),
      priority("priority"),
      url("url") {
}

RTCIceCandidateStats::RTCIceCandidateStats(const RTCIceCandidateStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      ip(other.ip),
      port(other.port),
      protocol(other.protocol),
      candidate_type(other.candidate_type),
      priority(other.priority),
      url(other.url) {
}

RTCIceCandidateStats::~RTCIceCandidateStats() {
}

const char RTCLocalIceCandidateStats::kType[] = "local-candidate";

RTCLocalIceCandidateStats::RTCLocalIceCandidateStats(
    const std::string& id, int64_t timestamp_us)
    : RTCIceCandidateStats(id, timestamp_us) {
}

RTCLocalIceCandidateStats::RTCLocalIceCandidateStats(
    std::string&& id, int64_t timestamp_us)
    : RTCIceCandidateStats(std::move(id), timestamp_us) {
}

const char* RTCLocalIceCandidateStats::type() const {
  return kType;
}

const char RTCRemoteIceCandidateStats::kType[] = "remote-candidate";

RTCRemoteIceCandidateStats::RTCRemoteIceCandidateStats(
    const std::string& id, int64_t timestamp_us)
    : RTCIceCandidateStats(id, timestamp_us) {
}

RTCRemoteIceCandidateStats::RTCRemoteIceCandidateStats(
    std::string&& id, int64_t timestamp_us)
    : RTCIceCandidateStats(std::move(id), timestamp_us) {
}

const char* RTCRemoteIceCandidateStats::type() const {
  return kType;
}

WEBRTC_RTCSTATS_IMPL(RTCCertificateStats, RTCStats, "certificate",
    &fingerprint,
    &fingerprint_algorithm,
    &base64_certificate,
    &issuer_certificate_id);

RTCCertificateStats::RTCCertificateStats(
    const std::string& id, int64_t timestamp_us)
    : RTCCertificateStats(std::string(id), timestamp_us) {
}

RTCCertificateStats::RTCCertificateStats(
    std::string&& id, int64_t timestamp_us)
    : RTCStats(std::move(id), timestamp_us),
      fingerprint("fingerprint"),
      fingerprint_algorithm("fingerprintAlgorithm"),
      base64_certificate("base64Certificate"),
      issuer_certificate_id("issuerCertificateId") {
}

RTCCertificateStats::RTCCertificateStats(
    const RTCCertificateStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      fingerprint(other.fingerprint),
      fingerprint_algorithm(other.fingerprint_algorithm),
      base64_certificate(other.base64_certificate),
      issuer_certificate_id(other.issuer_certificate_id) {
}

RTCCertificateStats::~RTCCertificateStats() {
}

WEBRTC_RTCSTATS_IMPL(RTCPeerConnectionStats, RTCStats, "peer-connection",
    &data_channels_opened,
    &data_channels_closed);

RTCPeerConnectionStats::RTCPeerConnectionStats(
    const std::string& id, int64_t timestamp_us)
    : RTCPeerConnectionStats(std::string(id), timestamp_us) {
}

RTCPeerConnectionStats::RTCPeerConnectionStats(
    std::string&& id, int64_t timestamp_us)
    : RTCStats(std::move(id), timestamp_us),
      data_channels_opened("dataChannelsOpened"),
      data_channels_closed("dataChannelsClosed") {
}

RTCPeerConnectionStats::RTCPeerConnectionStats(
    const RTCPeerConnectionStats& other)
    : RTCStats(other.id(), other.timestamp_us()),
      data_channels_opened(other.data_channels_opened),
      data_channels_closed(other.data_channels_closed) {
}

RTCPeerConnectionStats::~RTCPeerConnectionStats() {
}

}  // namespace webrtc
