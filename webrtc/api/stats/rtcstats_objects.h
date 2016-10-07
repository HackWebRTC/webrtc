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

// https://www.w3.org/TR/webrtc/#rtcicecandidatetype-enum
struct RTCIceCandidateType {
  static const char* kHost;
  static const char* kSrflx;
  static const char* kPrflx;
  static const char* kRelay;
};

// https://w3c.github.io/webrtc-stats/#icecandidate-dict*
class RTCIceCandidateStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCIceCandidateStats(const RTCIceCandidateStats& other);
  ~RTCIceCandidateStats() override;

  RTCStatsMember<std::string> ip;
  RTCStatsMember<int32_t> port;
  RTCStatsMember<std::string> protocol;
  // TODO(hbos): Support enum types? "RTCStatsMember<RTCIceCandidateType>"?
  RTCStatsMember<std::string> candidate_type;
  RTCStatsMember<int32_t> priority;
  RTCStatsMember<std::string> url;

 protected:
  RTCIceCandidateStats(const std::string& id, int64_t timestamp_us);
  RTCIceCandidateStats(std::string&& id, int64_t timestamp_us);
};

// In the spec both local and remote varieties are of type RTCIceCandidateStats.
// But here we define them as subclasses of |RTCIceCandidateStats| because the
// |kType| need to be different ("RTCStatsType type") in the local/remote case.
// https://w3c.github.io/webrtc-stats/#rtcstatstype-str*
class RTCLocalIceCandidateStats final : public RTCIceCandidateStats {
 public:
  static const char kType[];
  RTCLocalIceCandidateStats(const std::string& id, int64_t timestamp_us);
  RTCLocalIceCandidateStats(std::string&& id, int64_t timestamp_us);
  const char* type() const override;
};

class RTCRemoteIceCandidateStats final : public RTCIceCandidateStats {
 public:
  static const char kType[];
  RTCRemoteIceCandidateStats(const std::string& id, int64_t timestamp_us);
  RTCRemoteIceCandidateStats(std::string&& id, int64_t timestamp_us);
  const char* type() const override;
};

// https://w3c.github.io/webrtc-stats/#certificatestats-dict*
class RTCCertificateStats final : public RTCStats {
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
class RTCPeerConnectionStats final : public RTCStats {
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
