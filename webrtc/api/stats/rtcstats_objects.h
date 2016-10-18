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

// https://w3c.github.io/webrtc-pc/#idl-def-rtcdatachannelstate
struct RTCDataChannelState {
  static const char* kConnecting;
  static const char* kOpen;
  static const char* kClosing;
  static const char* kClosed;
};

// https://w3c.github.io/webrtc-stats/#dom-rtcstatsicecandidatepairstate
struct RTCStatsIceCandidatePairState {
  static const char* kFrozen;
  static const char* kWaiting;
  static const char* kInProgress;
  static const char* kFailed;
  static const char* kSucceeded;
  static const char* kCancelled;
};

// https://w3c.github.io/webrtc-pc/#rtcicecandidatetype-enum
struct RTCIceCandidateType {
  static const char* kHost;
  static const char* kSrflx;
  static const char* kPrflx;
  static const char* kRelay;
};

class RTCIceCandidatePairStats : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCIceCandidatePairStats(const std::string& id, int64_t timestamp_us);
  RTCIceCandidatePairStats(std::string&& id, int64_t timestamp_us);
  RTCIceCandidatePairStats(const RTCIceCandidatePairStats& other);
  ~RTCIceCandidatePairStats() override;

  RTCStatsMember<std::string> transport_id;
  RTCStatsMember<std::string> local_candidate_id;
  RTCStatsMember<std::string> remote_candidate_id;
  // TODO(hbos): Support enum types?
  // "RTCStatsMember<RTCStatsIceCandidatePairState>"?
  RTCStatsMember<std::string> state;
  RTCStatsMember<uint64_t> priority;
  RTCStatsMember<bool> nominated;
  RTCStatsMember<bool> writable;
  RTCStatsMember<bool> readable;
  RTCStatsMember<uint64_t> bytes_sent;
  RTCStatsMember<uint64_t> bytes_received;
  RTCStatsMember<double> total_rtt;
  RTCStatsMember<double> current_rtt;
  RTCStatsMember<double> available_outgoing_bitrate;
  RTCStatsMember<double> available_incoming_bitrate;
  RTCStatsMember<uint64_t> requests_received;
  RTCStatsMember<uint64_t> requests_sent;
  RTCStatsMember<uint64_t> responses_received;
  RTCStatsMember<uint64_t> responses_sent;
  RTCStatsMember<uint64_t> retransmissions_received;
  RTCStatsMember<uint64_t> retransmissions_sent;
  RTCStatsMember<uint64_t> consent_requests_received;
  RTCStatsMember<uint64_t> consent_requests_sent;
  RTCStatsMember<uint64_t> consent_responses_received;
  RTCStatsMember<uint64_t> consent_responses_sent;
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

// https://w3c.github.io/webrtc-stats/#dcstats-dict*
class RTCDataChannelStats final : public RTCStats {
 public:
  WEBRTC_RTCSTATS_DECL();

  RTCDataChannelStats(const std::string& id, int64_t timestamp_us);
  RTCDataChannelStats(std::string&& id, int64_t timestamp_us);
  RTCDataChannelStats(const RTCDataChannelStats& other);
  ~RTCDataChannelStats() override;

  RTCStatsMember<std::string> label;
  RTCStatsMember<std::string> protocol;
  RTCStatsMember<int32_t> datachannelid;
  // TODO(hbos): Support enum types? "RTCStatsMember<RTCDataChannelState>"?
  RTCStatsMember<std::string> state;
  RTCStatsMember<uint32_t> messages_sent;
  RTCStatsMember<uint64_t> bytes_sent;
  RTCStatsMember<uint32_t> messages_received;
  RTCStatsMember<uint64_t> bytes_received;
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
