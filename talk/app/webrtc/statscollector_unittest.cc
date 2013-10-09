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
#include "talk/app/webrtc/videotrack.h"
#include "talk/base/base64.h"
#include "talk/base/fakesslidentity.h"
#include "talk/base/gunit.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/devices/fakedevicemanager.h"
#include "talk/p2p/base/fakesession.h"
#include "talk/session/media/channelmanager.h"
#include "testing/base/public/gmock.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::ReturnNull;
using testing::SetArgPointee;

namespace cricket {

class ChannelManager;
class FakeDeviceManager;

}  // namespace cricket

namespace {

// Error return values
const char kNotFound[] = "NOT FOUND";
const char kNoReports[] = "NO REPORTS";

class MockWebRtcSession : public webrtc::WebRtcSession {
 public:
  explicit MockWebRtcSession(cricket::ChannelManager* channel_manager)
    : WebRtcSession(channel_manager, talk_base::Thread::Current(),
                    talk_base::Thread::Current(), NULL, NULL) {
  }
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
  MOCK_METHOD1(GetStats, bool(cricket::VideoMediaInfo*));
};

bool GetValue(const webrtc::StatsReport* report,
              const std::string& name,
              std::string* value) {
  webrtc::StatsReport::Values::const_iterator it = report->values.begin();
  for (; it != report->values.end(); ++it) {
    if (it->name == name) {
      *value = it->value;
      return true;
    }
  }
  return false;
}

std::string ExtractStatsValue(const std::string& type,
                              const webrtc::StatsReports& reports,
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
const webrtc::StatsReport* FindNthReportByType(
    const webrtc::StatsReports& reports, const std::string& type, int n) {
  for (size_t i = 0; i < reports.size(); ++i) {
    if (reports[i].type == type) {
      n--;
      if (n == 0)
        return &reports[i];
    }
  }
  return NULL;
}

const webrtc::StatsReport* FindReportById(const webrtc::StatsReports& reports,
                                          const std::string& id) {
  for (size_t i = 0; i < reports.size(); ++i) {
    if (reports[i].id == id) {
      return &reports[i];
    }
  }
  return NULL;
}

std::string ExtractSsrcStatsValue(webrtc::StatsReports reports,
                                  const std::string& name) {
  return ExtractStatsValue(
      webrtc::StatsReport::kStatsReportTypeSsrc, reports, name);
}

std::string ExtractBweStatsValue(webrtc::StatsReports reports,
                                  const std::string& name) {
  return ExtractStatsValue(
      webrtc::StatsReport::kStatsReportTypeBwe, reports, name);
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

void CheckCertChainReports(const webrtc::StatsReports& reports,
                           const std::vector<std::string>& ders,
                           const std::string& start_id) {
  std::string certificate_id = start_id;
  size_t i = 0;
  while (true) {
    const webrtc::StatsReport* report = FindReportById(reports, certificate_id);
    ASSERT_TRUE(report != NULL);
    std::string der_base64;
    EXPECT_TRUE(GetValue(
        report, webrtc::StatsReport::kStatsValueNameDer, &der_base64));
    std::string der = talk_base::Base64::Decode(der_base64,
                                                talk_base::Base64::DO_STRICT);
    EXPECT_EQ(ders[i], der);
    ++i;
    if (!GetValue(
        report, webrtc::StatsReport::kStatsValueNameIssuerId, &certificate_id))
      break;
  }
  EXPECT_EQ(ders.size(), i);
}

class StatsCollectorTest : public testing::Test {
 protected:
  StatsCollectorTest()
    : media_engine_(new cricket::FakeMediaEngine),
      channel_manager_(
          new cricket::ChannelManager(media_engine_,
                                      new cricket::FakeDeviceManager(),
                                      talk_base::Thread::Current())),
      session_(channel_manager_.get()) {
    // By default, we ignore session GetStats calls.
    EXPECT_CALL(session_, GetStats(_)).WillRepeatedly(Return(false));
  }

  void TestCertificateReports(const talk_base::FakeSSLCertificate& local_cert,
                              const std::vector<std::string>& local_ders,
                              const talk_base::FakeSSLCertificate& remote_cert,
                              const std::vector<std::string>& remote_ders) {
    webrtc::StatsCollector stats;  // Implementation under test.
    webrtc::StatsReports reports;  // returned values.
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
    EXPECT_CALL(session_, video_channel())
      .WillRepeatedly(ReturnNull());

    stats.UpdateStats();

    stats.GetStats(NULL, &reports);

    const webrtc::StatsReport* channel_report = FindNthReportByType(
        reports, webrtc::StatsReport::kStatsReportTypeComponent, 1);
    EXPECT_TRUE(channel_report != NULL);

    // Check local certificate chain.
    std::string local_certificate_id = ExtractStatsValue(
        webrtc::StatsReport::kStatsReportTypeComponent,
        reports,
        webrtc::StatsReport::kStatsValueNameLocalCertificateId);
    EXPECT_NE(kNotFound, local_certificate_id);
    CheckCertChainReports(reports, local_ders, local_certificate_id);

    // Check remote certificate chain.
    std::string remote_certificate_id = ExtractStatsValue(
        webrtc::StatsReport::kStatsReportTypeComponent,
        reports,
        webrtc::StatsReport::kStatsValueNameRemoteCertificateId);
    EXPECT_NE(kNotFound, remote_certificate_id);
    CheckCertChainReports(reports, remote_ders, remote_certificate_id);
  }
  cricket::FakeMediaEngine* media_engine_;
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
  MockWebRtcSession session_;
};

// This test verifies that 64-bit counters are passed successfully.
TEST_F(StatsCollectorTest, BytesCounterHandles64Bits) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel;
  cricket::VideoChannel video_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, "", false, NULL);
  webrtc::StatsReports reports;  // returned values.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  const uint32 kSsrcOfTrack = 1234;
  const std::string kNameOfTrack("somename");
  // The number of bytes must be larger than 0xFFFFFFFF for this test.
  const int64 kBytesSent = 12345678901234LL;
  const std::string kBytesSentString("12345678901234");

  stats.set_session(&session_);
  talk_base::scoped_refptr<webrtc::MediaStream> stream(
      webrtc::MediaStream::Create("streamlabel"));
  stream->AddTrack(webrtc::VideoTrack::Create(kNameOfTrack, NULL));
  stats.AddStream(stream);

  // Construct a stats value to read.
  video_sender_info.ssrcs.push_back(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(session_, video_channel())
    .WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(*media_channel, GetStats(_))
    .WillOnce(DoAll(SetArgPointee<0>(stats_read),
                    Return(true)));
  EXPECT_CALL(session_, GetTrackIdBySsrc(kSsrcOfTrack, _))
    .WillOnce(DoAll(SetArgPointee<1>(kNameOfTrack),
                    Return(true)));
  stats.UpdateStats();
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
  webrtc::StatsReports reports;  // returned values.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  // Set up an SSRC just to test that we get both kinds of stats back: SSRC and
  // BWE.
  const uint32 kSsrcOfTrack = 1234;
  const std::string kNameOfTrack("somename");
  const int64 kBytesSent = 12345678901234LL;
  const std::string kBytesSentString("12345678901234");

  stats.set_session(&session_);
  talk_base::scoped_refptr<webrtc::MediaStream> stream(
      webrtc::MediaStream::Create("streamlabel"));
  stream->AddTrack(webrtc::VideoTrack::Create(kNameOfTrack, NULL));
  stats.AddStream(stream);

  // Construct a stats value to read.
  video_sender_info.ssrcs.push_back(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);
  cricket::BandwidthEstimationInfo bwe;
  const int kTargetEncBitrate = 123456;
  const std::string kTargetEncBitrateString("123456");
  bwe.target_enc_bitrate = kTargetEncBitrate;
  stats_read.bw_estimations.push_back(bwe);

  EXPECT_CALL(session_, video_channel())
    .WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(*media_channel, GetStats(_))
    .WillOnce(DoAll(SetArgPointee<0>(stats_read),
                    Return(true)));
  EXPECT_CALL(session_, GetTrackIdBySsrc(kSsrcOfTrack, _))
    .WillOnce(DoAll(SetArgPointee<1>(kNameOfTrack),
                    Return(true)));
  stats.UpdateStats();
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
  webrtc::StatsReports reports;  // returned values.
  stats.set_session(&session_);
  EXPECT_CALL(session_, video_channel())
    .WillRepeatedly(ReturnNull());
  stats.UpdateStats();
  stats.GetStats(NULL, &reports);
  const webrtc::StatsReport* session_report = FindNthReportByType(
      reports, webrtc::StatsReport::kStatsReportTypeSession, 1);
  EXPECT_FALSE(session_report == NULL);
}

// This test verifies that only one object of type "googSession" exists
// in the returned stats.
TEST_F(StatsCollectorTest, OnlyOneSessionObjectExists) {
  webrtc::StatsCollector stats;  // Implementation under test.
  webrtc::StatsReports reports;  // returned values.
  stats.set_session(&session_);
  EXPECT_CALL(session_, video_channel())
    .WillRepeatedly(ReturnNull());
  stats.UpdateStats();
  stats.UpdateStats();
  stats.GetStats(NULL, &reports);
  const webrtc::StatsReport* session_report = FindNthReportByType(
      reports, webrtc::StatsReport::kStatsReportTypeSession, 1);
  EXPECT_FALSE(session_report == NULL);
  session_report = FindNthReportByType(
      reports, webrtc::StatsReport::kStatsReportTypeSession, 2);
  EXPECT_EQ(NULL, session_report);
}

// This test verifies that the empty track report exists in the returned stats
// without calling StatsCollector::UpdateStats.
TEST_F(StatsCollectorTest, TrackObjectExistsWithoutUpdateStats) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel;
  cricket::VideoChannel video_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, "", false, NULL);
  const std::string kTrackId("somename");
  talk_base::scoped_refptr<webrtc::MediaStream> stream(
      webrtc::MediaStream::Create("streamlabel"));
  talk_base::scoped_refptr<webrtc::VideoTrack> track =
      webrtc::VideoTrack::Create(kTrackId, NULL);
  stream->AddTrack(track);
  stats.AddStream(stream);

  stats.set_session(&session_);

  webrtc::StatsReports reports;

  // Verfies the existence of the track report.
  stats.GetStats(NULL, &reports);
  EXPECT_EQ((size_t)1, reports.size());
  EXPECT_EQ(std::string(webrtc::StatsReport::kStatsReportTypeTrack),
            reports[0].type);

  std::string trackValue =
      ExtractStatsValue(webrtc::StatsReport::kStatsReportTypeTrack,
                        reports,
                        webrtc::StatsReport::kStatsValueNameTrackId);
  EXPECT_EQ(kTrackId, trackValue);
}

// This test verifies that the empty track report exists in the returned stats
// when StatsCollector::UpdateStats is called with ssrc stats.
TEST_F(StatsCollectorTest, TrackAndSsrcObjectExistAfterUpdateSsrcStats) {
  webrtc::StatsCollector stats;  // Implementation under test.
  MockVideoMediaChannel* media_channel = new MockVideoMediaChannel;
  cricket::VideoChannel video_channel(talk_base::Thread::Current(),
      media_engine_, media_channel, &session_, "", false, NULL);
  const std::string kTrackId("somename");
  talk_base::scoped_refptr<webrtc::MediaStream> stream(
      webrtc::MediaStream::Create("streamlabel"));
  talk_base::scoped_refptr<webrtc::VideoTrack> track =
      webrtc::VideoTrack::Create(kTrackId, NULL);
  stream->AddTrack(track);
  stats.AddStream(stream);

  stats.set_session(&session_);

  webrtc::StatsReports reports;

  // Constructs an ssrc stats update.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  const uint32 kSsrcOfTrack = 1234;
  const int64 kBytesSent = 12345678901234LL;

  // Construct a stats value to read.
  video_sender_info.ssrcs.push_back(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(session_, video_channel())
    .WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(*media_channel, GetStats(_))
    .WillOnce(DoAll(SetArgPointee<0>(stats_read),
                    Return(true)));
  EXPECT_CALL(session_, GetTrackIdBySsrc(kSsrcOfTrack, _))
    .WillOnce(DoAll(SetArgPointee<1>(kTrackId),
                    Return(true)));

  stats.UpdateStats();
  stats.GetStats(NULL, &reports);
  // |reports| should contain one session report, one track report, and one ssrc
  // report.
  EXPECT_EQ((size_t)3, reports.size());
  const webrtc::StatsReport* track_report = FindNthReportByType(
      reports, webrtc::StatsReport::kStatsReportTypeTrack, 1);
  EXPECT_FALSE(track_report == NULL);

  stats.GetStats(track, &reports);
  // |reports| should contain one session report, one track report, and one ssrc
  // report.
  EXPECT_EQ((size_t)3, reports.size());
  track_report = FindNthReportByType(
      reports, webrtc::StatsReport::kStatsReportTypeTrack, 1);
  EXPECT_FALSE(track_report == NULL);

  std::string ssrc_id = ExtractSsrcStatsValue(
      reports, webrtc::StatsReport::kStatsValueNameSsrc);
  EXPECT_EQ(talk_base::ToString<uint32>(kSsrcOfTrack), ssrc_id);

  std::string track_id = ExtractSsrcStatsValue(
      reports, webrtc::StatsReport::kStatsValueNameTrackId);
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
  const std::string kTrackId("somename");
  talk_base::scoped_refptr<webrtc::MediaStream> stream(
      webrtc::MediaStream::Create("streamlabel"));
  talk_base::scoped_refptr<webrtc::VideoTrack> track =
      webrtc::VideoTrack::Create(kTrackId, NULL);
  stream->AddTrack(track);
  stats.AddStream(stream);

  stats.set_session(&session_);

  webrtc::StatsReports reports;

  // Constructs an ssrc stats update.
  cricket::VideoSenderInfo video_sender_info;
  cricket::VideoMediaInfo stats_read;
  const uint32 kSsrcOfTrack = 1234;
  const int64 kBytesSent = 12345678901234LL;

  // Construct a stats value to read.
  video_sender_info.ssrcs.push_back(1234);
  video_sender_info.bytes_sent = kBytesSent;
  stats_read.senders.push_back(video_sender_info);

  EXPECT_CALL(session_, video_channel())
    .WillRepeatedly(Return(&video_channel));
  EXPECT_CALL(*media_channel, GetStats(_))
    .WillRepeatedly(DoAll(SetArgPointee<0>(stats_read),
                          Return(true)));
  EXPECT_CALL(session_, GetTrackIdBySsrc(kSsrcOfTrack, _))
    .WillOnce(DoAll(SetArgPointee<1>(kTrackId),
                    Return(true)));

  // Instruct the session to return stats containing the transport channel.
  const std::string kTransportName("trspname");
  cricket::SessionStats session_stats;
  cricket::TransportStats transport_stats;
  cricket::TransportChannelStats channel_stats;
  channel_stats.component = 1;
  transport_stats.content_name = kTransportName;
  transport_stats.channel_stats.push_back(channel_stats);

  session_stats.transport_stats[kTransportName] = transport_stats;
  session_stats.proxy_to_transport[kVcName] = kTransportName;
  EXPECT_CALL(session_, GetStats(_))
    .WillRepeatedly(DoAll(SetArgPointee<0>(session_stats),
                          Return(true)));

  stats.UpdateStats();
  stats.GetStats(NULL, &reports);
  std::string transport_id = ExtractStatsValue(
      webrtc::StatsReport::kStatsReportTypeSsrc,
      reports,
      webrtc::StatsReport::kStatsValueNameTransportId);
  ASSERT_NE(kNotFound, transport_id);
  const webrtc::StatsReport* transport_report = FindReportById(reports,
                                                               transport_id);
  ASSERT_FALSE(transport_report == NULL);
}

// This test verifies that all chained certificates are correctly
// reported
TEST_F(StatsCollectorTest, DISABLED_ChainedCertificateReportsCreated) {
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
TEST_F(StatsCollectorTest, DISABLED_ChainlessCertificateReportsCreated) {
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
TEST_F(StatsCollectorTest, DISABLED_NoTransport) {
  webrtc::StatsCollector stats;  // Implementation under test.
  webrtc::StatsReports reports;  // returned values.
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
  EXPECT_CALL(session_, video_channel())
    .WillRepeatedly(ReturnNull());

  stats.UpdateStats();
  stats.GetStats(NULL, &reports);

  // Check that the local certificate is absent.
  std::string local_certificate_id = ExtractStatsValue(
      webrtc::StatsReport::kStatsReportTypeComponent,
      reports,
      webrtc::StatsReport::kStatsValueNameLocalCertificateId);
  ASSERT_EQ(kNotFound, local_certificate_id);

  // Check that the remote certificate is absent.
  std::string remote_certificate_id = ExtractStatsValue(
      webrtc::StatsReport::kStatsReportTypeComponent,
      reports,
      webrtc::StatsReport::kStatsValueNameRemoteCertificateId);
  ASSERT_EQ(kNotFound, remote_certificate_id);
}

// This test verifies that the stats are generated correctly when the transport
// does not have any certificates.
TEST_F(StatsCollectorTest, DISABLED_NoCertificates) {
  webrtc::StatsCollector stats;  // Implementation under test.
  webrtc::StatsReports reports;  // returned values.
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
  EXPECT_CALL(session_, video_channel())
    .WillRepeatedly(ReturnNull());

  stats.UpdateStats();
  stats.GetStats(NULL, &reports);

  // Check that the local certificate is absent.
  std::string local_certificate_id = ExtractStatsValue(
      webrtc::StatsReport::kStatsReportTypeComponent,
      reports,
      webrtc::StatsReport::kStatsValueNameLocalCertificateId);
  ASSERT_EQ(kNotFound, local_certificate_id);

  // Check that the remote certificate is absent.
  std::string remote_certificate_id = ExtractStatsValue(
      webrtc::StatsReport::kStatsReportTypeComponent,
      reports,
      webrtc::StatsReport::kStatsValueNameRemoteCertificateId);
  ASSERT_EQ(kNotFound, remote_certificate_id);
}


}  // namespace
