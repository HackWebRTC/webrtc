/*
 * libjingle
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

#include <stdio.h>

#include "talk/app/webrtc/statscollector.h"

#include "talk/app/webrtc/mediastream.h"
#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/mediastreamtrack.h"
#include "talk/app/webrtc/videotrack.h"
#include "talk/base/base64.h"
#include "talk/base/fakesslidentity.h"
#include "talk/base/gunit.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/devices/fakedevicemanager.h"
#include "talk/p2p/base/fakesession.h"
#include "talk/session/media/channelmanager.h"
#include "testing/base/public/gmock.h"

using cricket::StatsOptions;
using testing::_;
using testing::DoAll;
using testing::Field;
using testing::Return;
using testing::ReturnNull;
using testing::SetArgPointee;
using webrtc::PeerConnectionInterface;
using webrtc::StatsReport;
using webrtc::StatsReports;

namespace cricket {

class ChannelManager;
class FakeDeviceManager;

}  // namespace cricket

namespace {

// Error return values
const char kNotFound[] = "NOT FOUND";
const char kNoReports[] = "NO REPORTS";

// Constant names for track identification.
const char kTrackId[] = "somename";
const char kAudioTrackId[] = "audio_track_id";
const uint32 kSsrcOfTrack = 1234;

class MockWebRtcSession : public webrtc::WebRtcSession {
 public:
  explicit MockWebRtcSession(cricket::ChannelManager* channel_manager)
    : WebRtcSession(channel_manager, talk_base::Thread::Current(),
                    talk_base::Thread::Current(), NULL, NULL) {
  }
  MOCK_METHOD0(voice_channel, cricket::VoiceChannel*());
  MOCK_METHOD0(video_channel, cricket::VideoChannel*());
  MOCK_METHOD2(GetTrackIdBySsrc, bool(uint32, std::string*));
  MOCK_METHOD1(GetStats, bool(cricket::SessionStats*));
  MOCK_METHOD1(GetTransport, cricket::Transport*(const std::string&));
};

class MockVideoMediaChannel : public cricket::FakeVideoMediaChannel {
 public:
  MockVideoMediaChannel()
    : cricket::FakeVideoMediaChannel(NULL) {
  }
  // MOCK_METHOD0(transport_channel, cricket::TransportChannel*());
  MOCK_METHOD2(GetStats, bool(const StatsOptions&, cricket::VideoMediaInfo*));
};

class MockVoiceMediaChannel : public cricket::FakeVoiceMediaChannel {
 public:
  MockVoiceMediaChannel() : cricket::FakeVoiceMediaChannel(NULL) {
  }
  MOCK_METHOD1(GetStats, bool(cricket::VoiceMediaInfo*));
};

class FakeAudioProcessor : public webrtc::AudioProcessorInterface {
 public:
  FakeAudioProcessor() {}
  ~FakeAudioProcessor() {}

 private:
  virtual void GetStats(
      AudioProcessorInterface::AudioProcessorStats* stats) OVERRIDE {
    stats->typing_noise_detected = true;
    stats->echo_return_loss = 2;
    stats->echo_return_loss_enhancement = 3;
    stats->echo_delay_median_ms = 4;
    stats->aec_quality_min = 5.1f;
    stats->echo_delay_std_ms = 6;
  }
};

class FakeLocalAudioTrack
    : public webrtc::MediaStreamTrack<webrtc::AudioTrackInterface> {
 public:
  explicit FakeLocalAudioTrack(const std::string& id)
      : webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>(id),
        processor_(new talk_base::RefCountedObject<FakeAudioProcessor>()) {}
  std::string kind() const OVERRIDE {
    return "audio";
  }
  virtual webrtc::AudioSourceInterface* GetSource() const OVERRIDE {
    return NULL;
  }
  virtual void AddSink(webrtc::AudioTrackSinkInterface* sink) OVERRIDE {}
  virtual void RemoveSink(webrtc::AudioTrackSinkInterface* sink) OVERRIDE {}
  virtual bool GetSignalLevel(int* level) OVERRIDE {
    *level = 1;
    return true;
  }
  virtual webrtc::AudioProcessorInterface* GetAudioProcessor() OVERRIDE {
    return processor_.get();
  }

 private:
  talk_base::scoped_refptr<FakeAudioProcessor> processor_;
};

bool GetValue(const StatsReport* report,
              const std::string& name,
              std::string* value) {
  StatsReport::Values::const_iterator it = report->values.begin();
  for (; it != report->values.end(); ++it) {
    if (it->name == name) {
      *value = it->value;
      return true;
    }
  }
  return false;
}

std::string ExtractStatsValue(const std::string& type,
                              const StatsReports& reports,
                              const std::string name) {
  if (reports.empty()) {
    return kNoReports;
  }
  for (size_t i = 0; i < reports.size(); ++i) {
    if (reports[i].type != type)
      continue;
    std::string ret;
    if (GetValue(&reports[i], name, &ret)) {
      return ret;
    }
  }

  return kNotFound;
}

// Finds the |n|-th report of type |type| in |reports|.
// |n| starts from 1 for finding the first report.
const StatsReport* FindNthReportByType(
    const StatsReports& reports, const std::string& type, int n) {
  for (size_t i = 0; i < reports.size(); ++i) {
    if (reports[i].type == type) {
      n--;
      if (n == 0)
        return &reports[i];
    }
  }
  return NULL;
}

const StatsReport* FindReportById(const StatsReports& reports,
                                  const std::string& id) {
  for (size_t i = 0; i < reports.size(); ++i) {
    if (reports[i].id == id) {
      return &reports[i];
    }
  }
  return NULL;
}

std::string ExtractSsrcStatsValue(StatsReports reports,
                                  const std::string& name) {
  return ExtractStatsValue(
      StatsReport::kStatsReportTypeSsrc, reports, name);
}

std::string ExtractBweStatsValue(StatsReports reports,
                                  const std::string& name) {
  return ExtractStatsValue(
      StatsReport::kStatsReportTypeBwe, reports, name);
}

std::string DerToPem(const std::string& der) {
  return talk_base::SSLIdentity::DerToPem(
        talk_base::kPemTypeCertificate,
        reinterpret_cast<const unsigned char*>(der.c_str()),
        der.length());
}

std::vector<std::string> DersToPems(
    const std::vector<std::string>& ders) {
  std::vector<std::string> pems(ders.size());
  std::transform(ders.begin(), ders.end(), pems.begin(), DerToPem);
  return pems;
}

void CheckCertChainReports(const StatsReports& reports,
                           const std::vector<std::string>& ders,
                           const std::string& start_id) {
  std::string certificate_id = start_id;
  size_t i = 0;
  while (true) {
    const StatsReport* report = FindReportById(reports, certificate_id);
    ASSERT_TRUE(report != NULL);

    std::string der_base64;
    EXPECT_TRUE(GetValue(
        report, StatsReport::kStatsValueNameDer, &der_base64));
    std::string der = talk_base::Base64::Decode(der_base64,
                                                talk_base::Base64::DO_STRICT);
    EXPECT_EQ(ders[i], der);

    std::string fingerprint_algorithm;
    EXPECT_TRUE(GetValue(
        report,
        StatsReport::kStatsValueNameFingerprintAlgorithm,
        &fingerprint_algorithm));
    // The digest algorithm for a FakeSSLCertificate is always SHA-1.
    std::string sha_1_str = talk_base::DIGEST_SHA_1;
    EXPECT_EQ(sha_1_str, fingerprint_algorithm);

    std::string dummy_fingerprint;  // Value is not checked.
    EXPECT_TRUE(GetValue(
        report,
        StatsReport::kStatsValueNameFingerprint,
        &dummy_fingerprint));

    ++i;
    if (!GetValue(
        report, StatsReport::kStatsValueNameIssuerId, &certificate_id))
      break;
  }
  EXPECT_EQ(ders.size(), i);
}

void VerifyVoiceSenderInfoReport(const StatsReport* report,
                                 const cricket::VoiceSenderInfo& sinfo) {
  std::string value_in_report;
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameCodecName, &value_in_report));
  EXPECT_EQ(sinfo.codec_name, value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameBytesSent, &value_in_report));
  EXPECT_EQ(talk_base::ToString<int64>(sinfo.bytes_sent), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNamePacketsSent, &value_in_report));
  EXPECT_EQ(talk_base::ToString<int>(sinfo.packets_sent), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameRtt, &value_in_report));
  EXPECT_EQ(talk_base::ToString<int>(sinfo.rtt_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameRtt, &value_in_report));
  EXPECT_EQ(talk_base::ToString<int>(sinfo.rtt_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameJitterReceived, &value_in_report));
  EXPECT_EQ(talk_base::ToString<int>(sinfo.jitter_ms), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameEchoCancellationQualityMin,
      &value_in_report));
  EXPECT_EQ(talk_base::ToString<float>(sinfo.aec_quality_min), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameEchoDelayMedian, &value_in_report));
  EXPECT_EQ(talk_base::ToString<int>(sinfo.echo_delay_median_ms),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameEchoDelayStdDev, &value_in_report));
  EXPECT_EQ(talk_base::ToString<int>(sinfo.echo_delay_std_ms),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameEchoReturnLoss, &value_in_report));
  EXPECT_EQ(talk_base::ToString<int>(sinfo.echo_return_loss),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameEchoReturnLossEnhancement,
      &value_in_report));
  EXPECT_EQ(talk_base::ToString<int>(sinfo.echo_return_loss_enhancement),
            value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameAudioInputLevel, &value_in_report));
  EXPECT_EQ(talk_base::ToString<int>(sinfo.audio_level), value_in_report);
  EXPECT_TRUE(GetValue(
      report, StatsReport::kStatsValueNameTypingNoiseState, &value_in_report));
  std::string typing_detected = sinfo.typing_noise_detected ? "true" : "false";
  EXPECT_EQ(typing_detected, value_in_report);
}

class StatsCollectorTest : public testing::Test {
 protected:
  StatsCollectorTest()
    : media_engine_(new cricket::FakeMediaEngine),
      channel_manager_(
          new cricket::ChannelManager(media_engine_,
                                      new cricket::FakeDeviceManager(),
                                      talk_base::Thread::Current())),
      session_(channel_manager_.get()),
      track_id_(kTrackId) {
    // By default, we ignore session GetStats calls.
    EXPECT_CALL(session_, GetStats(_)).WillRepeatedly(Return(false));
  }

  // This creates a standard setup with a transport called "trspname"
  // having one transport channel
  // and the specified virtual connection name.
  void InitSessionStats(const std::string vc_name) {
    const std::string kTransportName("trspname");
    cricket::TransportStats transport_stats;
    cricket::TransportChannelStats channel_stats;
    channel_stats.component = 1;
    transport_stats.content_name = kTransportName;
    transport_stats.channel_stats.push_back(channel_stats);

    session_stats_.transport_stats[kTransportName] = transport_stats;
    session_stats_.proxy_to_transport[vc_name] = kTransportName;
  }

  // Adds a track with a given SSRC into the stats.
  void AddVideoTrackStats() {
    stream_ = webrtc::MediaStream::Create("streamlabel");
    track_= webrtc::VideoTrack::Create(kTrackId, NULL);
    stream_->AddTrack(track_);
    EXPECT_CALL(session_, GetTrackIdBySsrc(kSsrcOfTrack, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(track_id_),
                            Return(true)));
  }

  // Adds a local audio track with a given SSRC into the stats.
  void AddLocalAudioTrackStats() {
    if (stream_ == NULL)
      stream_ = webrtc::MediaStream::Create("streamlabel");

    audio_track_ =
        new talk_base::RefCountedObject<FakeLocalAudioTrack>(kAudioTrackId);
    stream_->AddTrack(audio_track_);
    EXPECT_CALL(session_, GetTrackIdBySsrc(kSsrcOfTrack, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(kAudioTrackId),
                              Return(true)));
  }

  void TestCertificateReports(const talk_base::FakeSSLCertificate& local_cert,
                              const std::vector<std::string>& local_ders,
                              const talk_base::FakeSSLCertificate& remote_cert,
                              const std::vector<std::string>& remote_ders) {
    webrtc::StatsCollector stats;  // Implementation under test.
    StatsReports reports;  // returned values.
    stats.set_session(&session_);

    // Fake stats to process.
    cricket::TransportChannelStats channel_stats;
    channel_stats.component = 1;

    cricket::TransportStats transport_stats;
    transport_stats.content_name = "audio";
    transport_stats.channel_stats.push_back(channel_stats);

    cricket::SessionStats session_stats;
    session_stats.transport_stats[transport_stats.content_name] =
        transport_stats;

    // Fake certificates to report.
    talk_base::FakeSSLIdentity local_identity(local_cert);
    talk_base::scoped_ptr<talk_base::FakeSSLCertificate> remote_cert_copy(
        remote_cert.GetReference());

    // Fake transport object.
    talk_base::scoped_ptr<cricket::FakeTransport> transport(
        new cricket::FakeTransport(
            session_.signaling_thread(),
            session_.worker_thread(),
            transport_stats.content_name));
    transport->SetIdentity(&local_identity);
    cricket::FakeTransportChannel* channel =
        static_cast<cricket::FakeTransportChannel*>(
            transport->CreateChannel(channel_stats.component));
    EXPECT_FALSE(channel == NULL);
    channel->SetRemoteCertificate(remote_cert_copy.get());

    // Configure MockWebRtcSession
    EXPECT_CALL(session_, GetTransport(transport_stats.content_name))
      .WillOnce(Return(transport.get()));
    EXPECT_CALL(session_, GetStats(_))
      .WillOnce(DoAll(SetArgPointee<0>(session_stats),
                      Return(true)));
    EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
    EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());

    stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);

    stats.GetStats(NULL, &reports);

    const StatsReport* channel_report = FindNthReportByType(
        reports, StatsReport::kStatsReportTypeComponent, 1);
    EXPECT_TRUE(channel_report != NULL);

    // Check local certificate chain.
    std::string local_certificate_id = ExtractStatsValue(
        StatsReport::kStatsReportTypeComponent,
        reports,
        StatsReport::kStatsValueNameLocalCertificateId);
    if (local_ders.size() > 0) {
      EXPECT_NE(kNotFound, local_certificate_id);
      CheckCertChainReports(reports, local_ders, local_certificate_id);
    } else {
      EXPECT_EQ(kNotFound, local_certificate_id);
    }

    // Check remote certificate chain.
    std::string remote_certificate_id = ExtractStatsValue(
        StatsReport::kStatsReportTypeComponent,
        reports,
        StatsReport::kStatsValueNameRemoteCertificateId);
    if (remote_ders.size() > 0) {
      EXPECT_NE(kNotFound, remote_certificate_id);
      CheckCertChainReports(reports, remote_ders, remote_certificate_id);
    } else {
      EXPECT_EQ(kNotFound, remote_certificate_id);
    }
  }

  cricket::FakeMediaEngine* media_engine_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
  MockWebRtcSession session_;
  cricket::SessionStats session_stats_;
  talk_base::scoped_refptr<webrtc::MediaStream> stream_;
  talk_base::scoped_refptr<webrtc::VideoTrack> track_;
  talk_base::scoped_refptr<FakeLocalAudioTrack> audio_track_;
  std::string track_id_;
};

// This test verifies that 64-bit counters are passed successfully.
TEST_F(StatsCollectorTest, BytesCounterHandles64Bits) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel;
  cricket::VideoChannel video_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, "", false, NULL);
  StatsReports reports;  // returned values.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  // The number of bytes must be larger than 0xFFFFFFFF for this test.
  const int64 kBytesSent = 12345678901234LL;
  const std::string kBytesSentString("12345678901234");

  stats.set_session(&session_);
  AddVideoTrackStats();
  stats.AddStream(stream_);

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_, _))
    .WillOnce(DoAll(SetArgPointee<1>(stats_read),
                    Return(true)));
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  std::string result = ExtractSsrcStatsValue(reports, "bytesSent");
  EXPECT_EQ(kBytesSentString, result);
}

// Test that BWE information is reported via stats.
TEST_F(StatsCollectorTest, BandwidthEstimationInfoIsReported) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel;
  cricket::VideoChannel video_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, "", false, NULL);
  StatsReports reports;  // returned values.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  // Set up an SSRC just to test that we get both kinds of stats back: SSRC and
  // BWE.
  const int64 kBytesSent = 12345678901234LL;
  const std::string kBytesSentString("12345678901234");

  stats.set_session(&session_);
  AddVideoTrackStats();
  stats.AddStream(stream_);

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);
  cricket::BandwidthEstimationInfo bwe;
  const int kTargetEncBitrate = 123456;
  const std::string kTargetEncBitrateString("123456");
  bwe.target_enc_bitrate = kTargetEncBitrate;
  stats_read.bw_estimations.push_back(bwe);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_, _))
    .WillOnce(DoAll(SetArgPointee<1>(stats_read),
                    Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  std::string result = ExtractSsrcStatsValue(reports, "bytesSent");
  EXPECT_EQ(kBytesSentString, result);
  result = ExtractBweStatsValue(reports, "googTargetEncBitrate");
  EXPECT_EQ(kTargetEncBitrateString, result);
}

// This test verifies that an object of type "googSession" always
// exists in the returned stats.
TEST_F(StatsCollectorTest, SessionObjectExists) {
  webrtc::StatsCollector stats;  // Implementation under test.
  StatsReports reports;  // returned values.
  stats.set_session(&session_);
  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  const StatsReport* session_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSession, 1);
  EXPECT_FALSE(session_report == NULL);
}

// This test verifies that only one object of type "googSession" exists
// in the returned stats.
TEST_F(StatsCollectorTest, OnlyOneSessionObjectExists) {
  webrtc::StatsCollector stats;  // Implementation under test.
  StatsReports reports;  // returned values.
  stats.set_session(&session_);
  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  const StatsReport* session_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSession, 1);
  EXPECT_FALSE(session_report == NULL);
  session_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSession, 2);
  EXPECT_EQ(NULL, session_report);
}

// This test verifies that the empty track report exists in the returned stats
// without calling StatsCollector::UpdateStats.
TEST_F(StatsCollectorTest, TrackObjectExistsWithoutUpdateStats) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel;
  cricket::VideoChannel video_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, "", false, NULL);
  AddVideoTrackStats();
  stats.AddStream(stream_);

  stats.set_session(&session_);

  StatsReports reports;

  // Verfies the existence of the track report.
  stats.GetStats(NULL, &reports);
  EXPECT_EQ((size_t)1, reports.size());
  EXPECT_EQ(std::string(StatsReport::kStatsReportTypeTrack),
            reports[0].type);

  std::string trackValue =
      ExtractStatsValue(StatsReport::kStatsReportTypeTrack,
                        reports,
                        StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kTrackId, trackValue);
}

// This test verifies that the empty track report exists in the returned stats
// when StatsCollector::UpdateStats is called with ssrc stats.
TEST_F(StatsCollectorTest, TrackAndSsrcObjectExistAfterUpdateSsrcStats) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel;
  cricket::VideoChannel video_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, "", false, NULL);
  AddVideoTrackStats();
  stats.AddStream(stream_);

  stats.set_session(&session_);

  StatsReports reports;

  // Constructs an ssrc stats update.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  const int64 kBytesSent = 12345678901234LL;

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_, _))
    .WillOnce(DoAll(SetArgPointee<1>(stats_read),
                    Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  // |reports| should contain at least one session report, one track report,
  // and one ssrc report.
  EXPECT_LE((size_t)3, reports.size());
  const StatsReport* track_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeTrack, 1);
  EXPECT_FALSE(track_report == NULL);

  stats.GetStats(track_, &reports);
  // |reports| should contain at least one session report, one track report,
  // and one ssrc report.
  EXPECT_LE((size_t)3, reports.size());
  track_report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeTrack, 1);
  EXPECT_FALSE(track_report == NULL);

  std::string ssrc_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameSsrc);
  EXPECT_EQ(talk_base::ToString<uint32>(kSsrcOfTrack), ssrc_id);

  std::string track_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kTrackId, track_id);
}

// This test verifies that an SSRC object has the identifier of a Transport
// stats object, and that this transport stats object exists in stats.
TEST_F(StatsCollectorTest, TransportObjectLinkedFromSsrcObject) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel;
  // The content_name known by the video channel.
  const std::string kVcName("vcname");
  cricket::VideoChannel video_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, kVcName, false, NULL);
  AddVideoTrackStats();
  stats.AddStream(stream_);

  stats.set_session(&session_);

  StatsReports reports;

  // Constructs an ssrc stats update.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  const int64 kBytesSent = 12345678901234LL;

  // Construct a stats value to read.
  video_sender_info.add_ssrc(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(stats_read),
                          Return(true)));

  InitSessionStats(kVcName);
  EXPECT_CALL(session_, GetStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                            Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  std::string transport_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeSsrc,
      reports,
      StatsReport::kStatsValueNameTransportId);
  ASSERT_NE(kNotFound, transport_id);
  const StatsReport* transport_report = FindReportById(reports,
                                                       transport_id);
  ASSERT_FALSE(transport_report == NULL);
}

// This test verifies that a remote stats object will not be created for
// an outgoing SSRC where remote stats are not returned.
TEST_F(StatsCollectorTest, RemoteSsrcInfoIsAbsent) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel;
  // The content_name known by the video channel.
  const std::string kVcName("vcname");
  cricket::VideoChannel video_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, kVcName, false, NULL);
  AddVideoTrackStats();
  stats.AddStream(stream_);

  stats.set_session(&session_);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  StatsReports reports;
  stats.GetStats(NULL, &reports);
  const StatsReport* remote_report = FindNthReportByType(reports,
      StatsReport::kStatsReportTypeRemoteSsrc, 1);
  EXPECT_TRUE(remote_report == NULL);
}

// This test verifies that a remote stats object will be created for
// an outgoing SSRC where stats are returned.
TEST_F(StatsCollectorTest, RemoteSsrcInfoIsPresent) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel;
  // The content_name known by the video channel.
  const std::string kVcName("vcname");
  cricket::VideoChannel video_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, kVcName, false, NULL);
  AddVideoTrackStats();
  stats.AddStream(stream_);

  stats.set_session(&session_);

  StatsReports reports;

  // Instruct the session to return stats containing the transport channel.
  InitSessionStats(kVcName);
  EXPECT_CALL(session_, GetStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                            Return(true)));

  // Constructs an ssrc stats update.
  cricket::VideoMediaInfo stats_read;

  cricket::SsrcReceiverInfo remote_ssrc_stats;
  remote_ssrc_stats.timestamp = 12345.678;
  remote_ssrc_stats.ssrc = kSsrcOfTrack;
  cricket::VideoSenderInfo video_sender_info;
  video_sender_info.add_ssrc(kSsrcOfTrack);
  video_sender_info.remote_stats.push_back(remote_ssrc_stats);
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_, _))
    .WillRepeatedly(DoAll(SetArgPointee<1>(stats_read),
                          Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);
  const StatsReport* remote_report = FindNthReportByType(reports,
      StatsReport::kStatsReportTypeRemoteSsrc, 1);
  EXPECT_FALSE(remote_report == NULL);
  EXPECT_NE(0, remote_report->timestamp);
}

// This test verifies that all chained certificates are correctly
// reported
TEST_F(StatsCollectorTest, ChainedCertificateReportsCreated) {
  // Build local certificate chain.
  std::vector<std::string> local_ders(5);
  local_ders[0] = "These";
  local_ders[1] = "are";
  local_ders[2] = "some";
  local_ders[3] = "der";
  local_ders[4] = "values";
  talk_base::FakeSSLCertificate local_cert(DersToPems(local_ders));

  // Build remote certificate chain
  std::vector<std::string> remote_ders(4);
  remote_ders[0] = "A";
  remote_ders[1] = "non-";
  remote_ders[2] = "intersecting";
  remote_ders[3] = "set";
  talk_base::FakeSSLCertificate remote_cert(DersToPems(remote_ders));

  TestCertificateReports(local_cert, local_ders, remote_cert, remote_ders);
}

// This test verifies that all certificates without chains are correctly
// reported.
TEST_F(StatsCollectorTest, ChainlessCertificateReportsCreated) {
  // Build local certificate.
  std::string local_der = "This is the local der.";
  talk_base::FakeSSLCertificate local_cert(DerToPem(local_der));

  // Build remote certificate.
  std::string remote_der = "This is somebody else's der.";
  talk_base::FakeSSLCertificate remote_cert(DerToPem(remote_der));

  TestCertificateReports(local_cert, std::vector<std::string>(1, local_der),
                         remote_cert, std::vector<std::string>(1, remote_der));
}

// This test verifies that the stats are generated correctly when no
// transport is present.
TEST_F(StatsCollectorTest, NoTransport) {
  webrtc::StatsCollector stats;  // Implementation under test.
  StatsReports reports;  // returned values.
  stats.set_session(&session_);

  // Fake stats to process.
  cricket::TransportChannelStats channel_stats;
  channel_stats.component = 1;

  cricket::TransportStats transport_stats;
  transport_stats.content_name = "audio";
  transport_stats.channel_stats.push_back(channel_stats);

  cricket::SessionStats session_stats;
  session_stats.transport_stats[transport_stats.content_name] =
      transport_stats;

  // Configure MockWebRtcSession
  EXPECT_CALL(session_, GetTransport(transport_stats.content_name))
    .WillOnce(ReturnNull());
  EXPECT_CALL(session_, GetStats(_))
    .WillOnce(DoAll(SetArgPointee<0>(session_stats),
                    Return(true)));

  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);

  // Check that the local certificate is absent.
  std::string local_certificate_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameLocalCertificateId);
  ASSERT_EQ(kNotFound, local_certificate_id);

  // Check that the remote certificate is absent.
  std::string remote_certificate_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameRemoteCertificateId);
  ASSERT_EQ(kNotFound, remote_certificate_id);
}

// This test verifies that the stats are generated correctly when the transport
// does not have any certificates.
TEST_F(StatsCollectorTest, NoCertificates) {
  webrtc::StatsCollector stats;  // Implementation under test.
  StatsReports reports;  // returned values.
  stats.set_session(&session_);

  // Fake stats to process.
  cricket::TransportChannelStats channel_stats;
  channel_stats.component = 1;

  cricket::TransportStats transport_stats;
  transport_stats.content_name = "audio";
  transport_stats.channel_stats.push_back(channel_stats);

  cricket::SessionStats session_stats;
  session_stats.transport_stats[transport_stats.content_name] =
      transport_stats;

  // Fake transport object.
  talk_base::scoped_ptr<cricket::FakeTransport> transport(
      new cricket::FakeTransport(
          session_.signaling_thread(),
          session_.worker_thread(),
          transport_stats.content_name));

  // Configure MockWebRtcSession
  EXPECT_CALL(session_, GetTransport(transport_stats.content_name))
    .WillOnce(Return(transport.get()));
  EXPECT_CALL(session_, GetStats(_))
    .WillOnce(DoAll(SetArgPointee<0>(session_stats),
                    Return(true)));
  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);

  // Check that the local certificate is absent.
  std::string local_certificate_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameLocalCertificateId);
  ASSERT_EQ(kNotFound, local_certificate_id);

  // Check that the remote certificate is absent.
  std::string remote_certificate_id = ExtractStatsValue(
      StatsReport::kStatsReportTypeComponent,
      reports,
      StatsReport::kStatsValueNameRemoteCertificateId);
  ASSERT_EQ(kNotFound, remote_certificate_id);
}

// This test verifies that a remote certificate with an unsupported digest
// algorithm is correctly ignored.
TEST_F(StatsCollectorTest, UnsupportedDigestIgnored) {
  // Build a local certificate.
  std::string local_der = "This is the local der.";
  talk_base::FakeSSLCertificate local_cert(DerToPem(local_der));

  // Build a remote certificate with an unsupported digest algorithm.
  std::string remote_der = "This is somebody else's der.";
  talk_base::FakeSSLCertificate remote_cert(DerToPem(remote_der));
  remote_cert.set_digest_algorithm("foobar");

  TestCertificateReports(local_cert, std::vector<std::string>(1, local_der),
                         remote_cert, std::vector<std::string>());
}

// Verifies the correct optons are passed to the VideoMediaChannel when using
// verbose output level.
TEST_F(StatsCollectorTest, StatsOutputLevelVerbose) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel;
  cricket::VideoChannel video_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, "", false, NULL);
  stats.set_session(&session_);

  StatsReports reports;  // returned values.
  cricket::VideoMediaInfo stats_read;
  cricket::BandwidthEstimationInfo bwe;
  bwe.total_received_propagation_delta_ms = 10;
  bwe.recent_received_propagation_delta_ms.push_back(100);
  bwe.recent_received_propagation_delta_ms.push_back(200);
  bwe.recent_received_packet_group_arrival_time_ms.push_back(1000);
  bwe.recent_received_packet_group_arrival_time_ms.push_back(2000);
  stats_read.bw_estimations.push_back(bwe);

  EXPECT_CALL(session_, video_channel())
    .WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(ReturnNull());

  StatsOptions options;
  options.include_received_propagation_stats = true;
  EXPECT_CALL(*media_channel, GetStats(
      Field(&StatsOptions::include_received_propagation_stats, true),
      _))
    .WillOnce(DoAll(SetArgPointee<1>(stats_read),
                    Return(true)));

  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelDebug);
  stats.GetStats(NULL, &reports);
  std::string result = ExtractBweStatsValue(
      reports, "googReceivedPacketGroupPropagationDeltaSumDebug");
  EXPECT_EQ("10", result);
  result = ExtractBweStatsValue(
      reports, "googReceivedPacketGroupPropagationDeltaDebug");
  EXPECT_EQ("[100, 200]", result);
  result = ExtractBweStatsValue(
      reports, "googReceivedPacketGroupArrivalTimeDebug");
  EXPECT_EQ("[1000, 2000]", result);
}

// This test verifies that a local stats object can get statistics via
// AudioTrackInterface::GetStats() method.
TEST_F(StatsCollectorTest, GetStatsFromLocalAudioTrack) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVoiceMediaChannel* media_channel = new MockVoiceMediaChannel();
  // The content_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, kVcName, false);
  AddLocalAudioTrackStats();
  stats.AddStream(stream_);
  stats.AddLocalAudioTrack(audio_track_.get(), kSsrcOfTrack);

  stats.set_session(&session_);

  // Instruct the session to return stats containing the transport channel.
  InitSessionStats(kVcName);
  EXPECT_CALL(session_, GetStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                            Return(true)));

  cricket::VoiceSenderInfo voice_sender_info;
  // Contents won't be modified by the AudioTrackInterface::GetStats().
  voice_sender_info.add_ssrc(kSsrcOfTrack);
  voice_sender_info.codec_name = "fake_codec";
  voice_sender_info.bytes_sent = 100;
  voice_sender_info.packets_sent = 101;
  voice_sender_info.rtt_ms = 102;
  voice_sender_info.fraction_lost = 103;
  voice_sender_info.jitter_ms = 104;
  voice_sender_info.packets_lost = 105;
  voice_sender_info.ext_seqnum = 106;

  // Contents will be modified by the AudioTrackInterface::GetStats().
  voice_sender_info.audio_level = 107;
  voice_sender_info.echo_return_loss = 108;;
  voice_sender_info.echo_return_loss_enhancement = 109;
  voice_sender_info.echo_delay_median_ms = 110;
  voice_sender_info.echo_delay_std_ms = 111;
  voice_sender_info.aec_quality_min = 112.0f;
  voice_sender_info.typing_noise_detected = false;

  // Constructs an ssrc stats update.
  cricket::VoiceMediaInfo stats_read;
  stats_read.senders.push_back(voice_sender_info);

  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(Return(&voice_channel));
  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(stats_read),
                            Return(true)));

  StatsReports reports;  // returned values.
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);

  // Verfy the existence of the track report.
  const StatsReport* report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSsrc, 1);
  EXPECT_FALSE(report == NULL);
  std::string track_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kAudioTrackId, track_id);
  std::string ssrc_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameSsrc);
  EXPECT_EQ(talk_base::ToString<uint32>(kSsrcOfTrack), ssrc_id);

  // Verifies the values in the track report.
  audio_track_->GetSignalLevel(&voice_sender_info.audio_level);
  webrtc::AudioProcessorInterface::AudioProcessorStats audio_processor_stats;
  audio_track_->GetAudioProcessor()->GetStats(&audio_processor_stats);
  voice_sender_info.typing_noise_detected =
      audio_processor_stats.typing_noise_detected;
  voice_sender_info.echo_return_loss = audio_processor_stats.echo_return_loss;
  voice_sender_info.echo_return_loss_enhancement =
      audio_processor_stats.echo_return_loss_enhancement;
  voice_sender_info.echo_delay_median_ms =
      audio_processor_stats.echo_delay_median_ms;
  voice_sender_info.aec_quality_min = audio_processor_stats.aec_quality_min;
  voice_sender_info.echo_delay_std_ms = audio_processor_stats.echo_delay_std_ms;
  VerifyVoiceSenderInfoReport(report, voice_sender_info);

  // Verify we get the same result by passing a track to GetStats().
  StatsReports track_reports;  // returned values.
  stats.GetStats(audio_track_.get(), &track_reports);
  const StatsReport* track_report = FindNthReportByType(
      track_reports, StatsReport::kStatsReportTypeSsrc, 1);
  EXPECT_FALSE(track_report == NULL);
  track_id = ExtractSsrcStatsValue(track_reports,
                                   StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kAudioTrackId, track_id);
  ssrc_id = ExtractSsrcStatsValue(track_reports,
                                  StatsReport::kStatsValueNameSsrc);
  EXPECT_EQ(talk_base::ToString<uint32>(kSsrcOfTrack), ssrc_id);
  VerifyVoiceSenderInfoReport(track_report, voice_sender_info);
}

// This test verifies that a local stats object won't update its statistics
// after a RemoveLocalAudioTrack() call.
TEST_F(StatsCollectorTest, GetStatsAfterRemoveAudioStream) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVoiceMediaChannel* media_channel = new MockVoiceMediaChannel();
  // The content_name known by the voice channel.
  const std::string kVcName("vcname");
  cricket::VoiceChannel voice_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, kVcName, false);
  AddLocalAudioTrackStats();
  stats.AddStream(stream_);
  stats.AddLocalAudioTrack(audio_track_.get(), kSsrcOfTrack);

  stats.set_session(&session_);

  // Instruct the session to return stats containing the transport channel.
  InitSessionStats(kVcName);
  EXPECT_CALL(session_, GetStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats_),
                            Return(true)));

  stats.RemoveLocalAudioTrack(audio_track_.get(), kSsrcOfTrack);
  cricket::VoiceSenderInfo voice_sender_info;
  // Contents won't be modified by the AudioTrackInterface::GetStats().
  voice_sender_info.add_ssrc(kSsrcOfTrack);
  voice_sender_info.codec_name = "fake_codec";
  voice_sender_info.bytes_sent = 100;
  voice_sender_info.packets_sent = 101;
  voice_sender_info.rtt_ms = 102;
  voice_sender_info.fraction_lost = 103;
  voice_sender_info.jitter_ms = 104;
  voice_sender_info.packets_lost = 105;
  voice_sender_info.ext_seqnum = 106;

  // Contents will be modified by the AudioTrackInterface::GetStats().
  voice_sender_info.audio_level = 107;
  voice_sender_info.echo_return_loss = 108;;
  voice_sender_info.echo_return_loss_enhancement = 109;
  voice_sender_info.echo_delay_median_ms = 110;
  voice_sender_info.echo_delay_std_ms = 111;
  voice_sender_info.aec_quality_min = 112;
  voice_sender_info.typing_noise_detected = false;

  // Constructs an ssrc stats update.
  cricket::VoiceMediaInfo stats_read;
  stats_read.senders.push_back(voice_sender_info);

  EXPECT_CALL(session_, voice_channel()).WillRepeatedly(Return(&voice_channel));
  EXPECT_CALL(session_, video_channel()).WillRepeatedly(ReturnNull());
  EXPECT_CALL(*media_channel, GetStats(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(stats_read),
                            Return(true)));

  StatsReports reports;  // returned values.
  stats.UpdateStats(PeerConnectionInterface::kStatsOutputLevelStandard);
  stats.GetStats(NULL, &reports);

  // The report will exist since we don't remove them in RemoveStream().
  const StatsReport* report = FindNthReportByType(
      reports, StatsReport::kStatsReportTypeSsrc, 1);
  EXPECT_FALSE(report == NULL);
  std::string track_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kAudioTrackId, track_id);
  std::string ssrc_id = ExtractSsrcStatsValue(
      reports, StatsReport::kStatsValueNameSsrc);
  EXPECT_EQ(talk_base::ToString<uint32>(kSsrcOfTrack), ssrc_id);

  // Verifies the values in the track report, no value will be changed by the
  // AudioTrackInterface::GetSignalValue() and
  // AudioProcessorInterface::AudioProcessorStats::GetStats();
  VerifyVoiceSenderInfoReport(report, voice_sender_info);
}

}  // namespace
