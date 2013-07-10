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
                    NULL, NULL, NULL) {
  }
  MOCK_METHOD0(video_channel, cricket::VideoChannel*());
  MOCK_METHOD2(GetTrackIdBySsrc, bool(uint32, std::string*));
  MOCK_METHOD1(GetStats, bool(cricket::SessionStats*));
};

class MockVideoMediaChannel : public cricket::FakeVideoMediaChannel {
 public:
  MockVideoMediaChannel()
    : cricket::FakeVideoMediaChannel(NULL) {
  }
  // MOCK_METHOD0(transport_channel, cricket::TransportChannel*());
  MOCK_METHOD1(GetStats, bool(cricket::VideoMediaInfo*));
};

std::string ExtractStatsValue(const std::string& type,
                              webrtc::StatsReports reports,
                              const std::string name) {
  if (reports.empty()) {
    return kNoReports;
  }
  for (size_t i = 0; i < reports.size(); ++i) {
    if (reports[i].type != type)
      continue;
    webrtc::StatsReport::Values::const_iterator it =
        reports[i].values.begin();
    for (; it != reports[i].values.end(); ++it) {
      if (it->name == name) {
        return it->value;
      }
    }
  }

  return kNotFound;
}

// Finds the |n|-th report of type |type| in |reports|.
// |n| starts from 1 for finding the first report.
const webrtc::StatsReport* FindNthReportByType(webrtc::StatsReports reports,
                                               const std::string& type,
                                               int n) {
  for (size_t i = 0; i < reports.size(); ++i) {
    if (reports[i].type == type) {
      n--;
      if (n == 0)
        return &reports[i];
    }
  }
  return NULL;
}

const webrtc::StatsReport* FindReportById(webrtc::StatsReports reports,
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

}  // namespace
