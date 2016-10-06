/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_STATS_RTCSTATS_OBJECTS_H_
#define WEBRTC_API_STATS_RTCSTATS_OBJECTS_H_

#include <string>

#include "webrtc/api/stats/rtcstats.h"

namespace webrtc {

// https://w3c.github.io/webrtc-stats/#certificatestats-dict*
class RTCCertificateStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCCertificateStats(const std::string& id, int64_t timestamp_us);
  RTCCertificateStats(std::string&& id, int64_t timestamp_us);
  RTCCertificateStats(const RTCCertificateStats& other);
  ~RTCCertificateStats() override;

  RTCStatsMember<std::string> fingerprint;
  RTCStatsMember<std::string> fingerprint_algorithm;
  RTCStatsMember<std::string> base64_certificate;
  RTCStatsMember<std::string> issuer_certificate_id;
};

// https://w3c.github.io/webrtc-stats/#pcstats-dict*
// TODO(hbos): Tracking bug crbug.com/636818
class RTCPeerConnectionStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCPeerConnectionStats(const std::string& id, int64_t timestamp_us);
  RTCPeerConnectionStats(std::string&& id, int64_t timestamp_us);
  RTCPeerConnectionStats(const RTCPeerConnectionStats& other);
  ~RTCPeerConnectionStats() override;

  RTCStatsMember<uint32_t> data_channels_opened;
  RTCStatsMember<uint32_t> data_channels_closed;
};

}  // namespace webrtc

#endif  // WEBRTC_API_STATS_RTCSTATS_OBJECTS_H_
