/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "api/jsep.h"
#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "pc/mediastream.h"
#include "pc/mediastreamtrack.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/mockpeerconnectionobservers.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread.h"

// This file contains tests for RTP Media API-related behavior of
// |webrtc::PeerConnection|, see https://w3c.github.io/webrtc-pc/#rtp-media-api.

namespace {

// TODO(hbos): Consolidate fake track classes. https://crbug.com/webrtc/8369
class FakeAudioMediaStreamTrack
    : public rtc::RefCountedObject<
          webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>> {
 public:
  explicit FakeAudioMediaStreamTrack(const std::string& id)
      : rtc::RefCountedObject<
            webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>>(id) {}

  std::string kind() const override {
    return webrtc::MediaStreamTrackInterface::kAudioKind;
  }

  webrtc::AudioSourceInterface* GetSource() const override { return nullptr; }

  void AddSink(webrtc::AudioTrackSinkInterface* sink) override {}

  void RemoveSink(webrtc::AudioTrackSinkInterface* sink) override {}

  bool GetSignalLevel(int* level) override {
    RTC_NOTREACHED();
    return false;
  }

  rtc::scoped_refptr<webrtc::AudioProcessorInterface> GetAudioProcessor()
      override {
    RTC_NOTREACHED();
    return nullptr;
  }
};

class PeerConnectionRtpTest : public testing::Test {
 public:
  PeerConnectionRtpTest()
      : pc_factory_(webrtc::CreatePeerConnectionFactory(
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            FakeAudioCaptureModule::Create(),
            nullptr,
            nullptr)) {}

  std::unique_ptr<webrtc::PeerConnectionWrapper> CreatePeerConnection() {
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    auto observer = rtc::MakeUnique<webrtc::MockPeerConnectionObserver>();
    auto pc = pc_factory_->CreatePeerConnection(config, nullptr, nullptr,
                                                observer.get());
    return std::unique_ptr<webrtc::PeerConnectionWrapper>(
        new webrtc::PeerConnectionWrapper(pc_factory_, pc,
                                          std::move(observer)));
  }

 protected:
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;
};

TEST_F(PeerConnectionRtpTest, AddTrackWithoutStreamFiresOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<FakeAudioMediaStreamTrack> audio_track(
      new FakeAudioMediaStreamTrack("audio_track"));
  EXPECT_TRUE(caller->pc()->AddTrack(audio_track.get(), {}));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  // TODO(deadbeef): When no stream is handled correctly we would expect
  // |add_track_events_[0].streams| to be empty. https://crbug.com/webrtc/7933
  ASSERT_EQ(1u, callee->observer()->add_track_events_[0].streams.size());
  EXPECT_TRUE(
      callee->observer()->add_track_events_[0].streams[0]->FindAudioTrack(
          "audio_track"));
}

TEST_F(PeerConnectionRtpTest, AddTrackWithStreamFiresOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<FakeAudioMediaStreamTrack> audio_track(
      new FakeAudioMediaStreamTrack("audio_track"));
  auto stream = webrtc::MediaStream::Create("audio_stream");
  EXPECT_TRUE(caller->pc()->AddTrack(audio_track.get(), {stream.get()}));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  ASSERT_EQ(1u, callee->observer()->add_track_events_[0].streams.size());
  EXPECT_EQ("audio_stream",
            callee->observer()->add_track_events_[0].streams[0]->label());
  EXPECT_TRUE(
      callee->observer()->add_track_events_[0].streams[0]->FindAudioTrack(
          "audio_track"));
}

TEST_F(PeerConnectionRtpTest, RemoveTrackWithoutStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<FakeAudioMediaStreamTrack> audio_track(
      new FakeAudioMediaStreamTrack("audio_track"));
  auto sender = caller->pc()->AddTrack(audio_track.get(), {});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

TEST_F(PeerConnectionRtpTest, RemoveTrackWithStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<FakeAudioMediaStreamTrack> audio_track(
      new FakeAudioMediaStreamTrack("audio_track"));
  auto stream = webrtc::MediaStream::Create("audio_stream");
  auto sender = caller->pc()->AddTrack(audio_track.get(), {stream.get()});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

TEST_F(PeerConnectionRtpTest, RemoveTrackWithSharedStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<FakeAudioMediaStreamTrack> audio_track1(
      new FakeAudioMediaStreamTrack("audio_track1"));
  rtc::scoped_refptr<FakeAudioMediaStreamTrack> audio_track2(
      new FakeAudioMediaStreamTrack("audio_track2"));
  auto stream = webrtc::MediaStream::Create("shared_audio_stream");
  std::vector<webrtc::MediaStreamInterface*> streams{stream.get()};
  auto sender1 = caller->pc()->AddTrack(audio_track1.get(), streams);
  auto sender2 = caller->pc()->AddTrack(audio_track2.get(), streams);
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(2u, callee->observer()->add_track_events_.size());

  // Remove "audio_track1".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender1));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(2u, callee->observer()->add_track_events_.size());
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>>{
          callee->observer()->add_track_events_[0].receiver},
      callee->observer()->remove_track_events_);

  // Remove "audio_track2".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender2));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(2u, callee->observer()->add_track_events_.size());
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

}  // namespace
