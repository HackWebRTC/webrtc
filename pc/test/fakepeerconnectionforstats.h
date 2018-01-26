/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKEPEERCONNECTIONFORSTATS_H_
#define PC_TEST_FAKEPEERCONNECTIONFORSTATS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "media/base/fakemediaengine.h"
#include "pc/test/fakedatachannelprovider.h"
#include "pc/test/fakepeerconnectionbase.h"

namespace webrtc {

// Fake VoiceMediaChannel where the result of GetStats can be configured.
class FakeVoiceMediaChannelForStats : public cricket::FakeVoiceMediaChannel {
 public:
  FakeVoiceMediaChannelForStats()
      : cricket::FakeVoiceMediaChannel(nullptr, cricket::AudioOptions()) {}

  void SetStats(const cricket::VoiceMediaInfo& voice_info) {
    stats_ = voice_info;
  }

  // VoiceMediaChannel overrides.
  bool GetStats(cricket::VoiceMediaInfo* info) override {
    if (stats_) {
      *info = *stats_;
      return true;
    }
    return false;
  }

 private:
  rtc::Optional<cricket::VoiceMediaInfo> stats_;
};

// Fake VideoMediaChannel where the result of GetStats can be configured.
class FakeVideoMediaChannelForStats : public cricket::FakeVideoMediaChannel {
 public:
  FakeVideoMediaChannelForStats()
      : cricket::FakeVideoMediaChannel(nullptr, cricket::VideoOptions()) {}

  void SetStats(const cricket::VideoMediaInfo& video_info) {
    stats_ = video_info;
  }

  // VideoMediaChannel overrides.
  bool GetStats(cricket::VideoMediaInfo* info) override {
    if (stats_) {
      *info = *stats_;
      return true;
    }
    return false;
  }

 private:
  rtc::Optional<cricket::VideoMediaInfo> stats_;
};

constexpr bool kDefaultRtcpMuxRequired = true;
constexpr bool kDefaultSrtpRequired = true;

// This class is intended to be fed into the StatsCollector and
// RTCStatsCollector so that the stats functionality can be unit tested.
// Individual tests can configure this fake as needed to simulate scenarios
// under which to test the stats collectors.
class FakePeerConnectionForStats : public FakePeerConnectionBase {
 public:
  // TODO(steveanton): Add support for specifying separate threads to test
  // multi-threading correctness.
  FakePeerConnectionForStats()
      : network_thread_(rtc::Thread::Current()),
        worker_thread_(rtc::Thread::Current()),
        signaling_thread_(rtc::Thread::Current()) {}

  FakeVoiceMediaChannelForStats* AddVoiceChannel(
      const std::string& mid,
      const std::string& transport_name,
      const cricket::VoiceMediaInfo& voice_info) {
    RTC_DCHECK(!voice_channel_);
    auto voice_media_channel = rtc::MakeUnique<FakeVoiceMediaChannelForStats>();
    auto* voice_media_channel_ptr = voice_media_channel.get();
    voice_channel_ = rtc::MakeUnique<cricket::VoiceChannel>(
        worker_thread_, network_thread_, signaling_thread_, nullptr,
        std::move(voice_media_channel), mid, kDefaultRtcpMuxRequired,
        kDefaultSrtpRequired);
    voice_channel_->set_transport_name_for_testing(transport_name);
    voice_media_channel_ptr->SetStats(voice_info);
    return voice_media_channel_ptr;
  }

  FakeVideoMediaChannelForStats* AddVideoChannel(
      const std::string& mid,
      const std::string& transport_name,
      const cricket::VideoMediaInfo& video_stats) {
    RTC_DCHECK(!video_channel_);
    auto video_media_channel = rtc::MakeUnique<FakeVideoMediaChannelForStats>();
    auto video_media_channel_ptr = video_media_channel.get();
    video_channel_ = rtc::MakeUnique<cricket::VideoChannel>(
        worker_thread_, network_thread_, signaling_thread_,
        std::move(video_media_channel), mid, kDefaultRtcpMuxRequired,
        kDefaultSrtpRequired);
    video_channel_->set_transport_name_for_testing(transport_name);
    video_media_channel_ptr->SetStats(video_stats);
    return video_media_channel_ptr;
  }

  void AddLocalTrack(uint32_t ssrc, const std::string& track_id) {
    local_track_id_by_ssrc_[ssrc] = track_id;
  }

  void AddRemoteTrack(uint32_t ssrc, const std::string& track_id) {
    remote_track_id_by_ssrc_[ssrc] = track_id;
  }

  void AddSctpDataChannel(const std::string& label) {
    AddSctpDataChannel(label, InternalDataChannelInit());
  }

  void AddSctpDataChannel(const std::string& label,
                          const InternalDataChannelInit& init) {
    sctp_data_channels_.push_back(DataChannel::Create(
        &data_channel_provider_, cricket::DCT_SCTP, label, init));
  }

  void SetTransportStats(const std::string& transport_name,
                         const cricket::TransportChannelStats& channel_stats) {
    cricket::TransportStats transport_stats;
    transport_stats.transport_name = transport_name;
    transport_stats.channel_stats.push_back(channel_stats);
    transport_stats_by_name_[transport_name] = transport_stats;
  }

  void SetCallStats(const Call::Stats& call_stats) { call_stats_ = call_stats; }

  void SetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate> certificate) {
    local_certificates_by_transport_[transport_name] = certificate;
  }

  void SetRemoteCertificate(const std::string& transport_name,
                            std::unique_ptr<rtc::SSLCertificate> certificate) {
    remote_certificates_by_transport_[transport_name] = std::move(certificate);
  }

  // PeerConnectionInternal overrides.

  rtc::Thread* network_thread() const override { return network_thread_; }

  rtc::Thread* worker_thread() const override { return worker_thread_; }

  rtc::Thread* signaling_thread() const override { return signaling_thread_; }

  cricket::VoiceChannel* voice_channel() const override {
    return voice_channel_.get();
  }

  cricket::VideoChannel* video_channel() const override {
    return video_channel_.get();
  }

  bool GetLocalTrackIdBySsrc(uint32_t ssrc, std::string* track_id) override {
    auto it = local_track_id_by_ssrc_.find(ssrc);
    if (it != local_track_id_by_ssrc_.end()) {
      *track_id = it->second;
      return true;
    } else {
      return false;
    }
  }

  bool GetRemoteTrackIdBySsrc(uint32_t ssrc, std::string* track_id) override {
    auto it = remote_track_id_by_ssrc_.find(ssrc);
    if (it != remote_track_id_by_ssrc_.end()) {
      *track_id = it->second;
      return true;
    } else {
      return false;
    }
  }

  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels()
      const override {
    return sctp_data_channels_;
  }

  std::unique_ptr<SessionStats> GetSessionStats_s() override {
    std::set<std::string> transport_names;
    if (voice_channel_) {
      transport_names.insert(voice_channel_->transport_name());
    }
    if (video_channel_) {
      transport_names.insert(video_channel_->transport_name());
    }
    return GetSessionStatsForTransports(transport_names);
  }

  std::unique_ptr<SessionStats> GetSessionStats(
      const ChannelNamePairs& channel_name_pairs) override {
    std::set<std::string> transport_names;
    if (channel_name_pairs.voice) {
      transport_names.insert(channel_name_pairs.voice.value().transport_name);
    }
    if (channel_name_pairs.video) {
      transport_names.insert(channel_name_pairs.video.value().transport_name);
    }
    if (channel_name_pairs.data) {
      transport_names.insert(channel_name_pairs.data.value().transport_name);
    }
    return GetSessionStatsForTransports(transport_names);
  }

  Call::Stats GetCallStats() override { return call_stats_; }

  bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) override {
    auto it = local_certificates_by_transport_.find(transport_name);
    if (it != local_certificates_by_transport_.end()) {
      *certificate = it->second;
      return true;
    } else {
      return false;
    }
  }

  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate(
      const std::string& transport_name) override {
    auto it = remote_certificates_by_transport_.find(transport_name);
    if (it != remote_certificates_by_transport_.end()) {
      return it->second->GetUniqueReference();
    } else {
      return nullptr;
    }
  }

 private:
  std::unique_ptr<SessionStats> GetSessionStatsForTransports(
      const std::set<std::string>& transport_names) {
    auto stats = rtc::MakeUnique<SessionStats>();
    for (const std::string& transport_name : transport_names) {
      stats->transport_stats[transport_name] =
          GetTransportStatsByName(transport_name);
    }
    return stats;
  }

  cricket::TransportStats GetTransportStatsByName(
      const std::string& transport_name) {
    auto it = transport_stats_by_name_.find(transport_name);
    if (it != transport_stats_by_name_.end()) {
      // If specific transport stats have been specified, return those.
      return it->second;
    }
    // Otherwise, generate some dummy stats.
    cricket::TransportChannelStats channel_stats;
    channel_stats.component = 1;
    cricket::TransportStats transport_stats;
    transport_stats.transport_name = transport_name;
    transport_stats.channel_stats.push_back(channel_stats);
    return transport_stats;
  }

  rtc::Thread* const network_thread_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const signaling_thread_;

  FakeDataChannelProvider data_channel_provider_;

  std::unique_ptr<cricket::VoiceChannel> voice_channel_;
  std::unique_ptr<cricket::VideoChannel> video_channel_;
  std::map<uint32_t, std::string> local_track_id_by_ssrc_;
  std::map<uint32_t, std::string> remote_track_id_by_ssrc_;

  std::vector<rtc::scoped_refptr<DataChannel>> sctp_data_channels_;

  std::map<std::string, cricket::TransportStats> transport_stats_by_name_;

  Call::Stats call_stats_;

  std::map<std::string, rtc::scoped_refptr<rtc::RTCCertificate>>
      local_certificates_by_transport_;
  std::map<std::string, std::unique_ptr<rtc::SSLCertificate>>
      remote_certificates_by_transport_;
};

}  // namespace webrtc

#endif  // PC_TEST_FAKEPEERCONNECTIONFORSTATS_H_
