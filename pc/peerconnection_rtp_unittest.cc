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

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
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

const uint32_t kDefaultTimeout = 10000u;

template <typename MethodFunctor>
class OnSuccessObserver : public rtc::RefCountedObject<
                              webrtc::SetRemoteDescriptionObserverInterface> {
 public:
  explicit OnSuccessObserver(MethodFunctor on_success)
      : on_success_(std::move(on_success)) {}

  // webrtc::SetRemoteDescriptionObserverInterface implementation.
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    RTC_CHECK(error.ok());
    on_success_();
  }

 private:
  MethodFunctor on_success_;
};

class PeerConnectionRtpTest : public testing::Test {
 public:
  PeerConnectionRtpTest()
      : pc_factory_(webrtc::CreatePeerConnectionFactory(
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            FakeAudioCaptureModule::Create(),
            webrtc::CreateBuiltinAudioEncoderFactory(),
            webrtc::CreateBuiltinAudioDecoderFactory(),
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

// These tests cover |webrtc::PeerConnectionObserver| callbacks firing upon
// setting the remote description.
class PeerConnectionRtpCallbacksTest : public PeerConnectionRtpTest {};

TEST_F(PeerConnectionRtpCallbacksTest, AddTrackWithoutStreamFiresOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  EXPECT_TRUE(caller->pc()->AddTrack(audio_track.get(), {}));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  // TODO(hbos): When "no stream" is handled correctly we would expect
  // |add_track_events_[0].streams| to be empty. https://crbug.com/webrtc/7933
  auto& add_track_event = callee->observer()->add_track_events_[0];
  ASSERT_EQ(add_track_event.streams.size(), 1u);
  EXPECT_TRUE(add_track_event.streams[0]->FindAudioTrack("audio_track"));
  EXPECT_EQ(add_track_event.streams, add_track_event.receiver->streams());
}

TEST_F(PeerConnectionRtpCallbacksTest, AddTrackWithStreamFiresOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  auto stream = webrtc::MediaStream::Create("audio_stream");
  EXPECT_TRUE(caller->pc()->AddTrack(audio_track.get(), {stream.get()}));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  auto& add_track_event = callee->observer()->add_track_events_[0];
  ASSERT_EQ(add_track_event.streams.size(), 1u);
  EXPECT_EQ("audio_stream", add_track_event.streams[0]->label());
  EXPECT_TRUE(add_track_event.streams[0]->FindAudioTrack("audio_track"));
  EXPECT_EQ(add_track_event.streams, add_track_event.receiver->streams());
}

TEST_F(PeerConnectionRtpCallbacksTest,
       RemoveTrackWithoutStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  auto sender = caller->pc()->AddTrack(audio_track.get(), {});
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

TEST_F(PeerConnectionRtpCallbacksTest,
       RemoveTrackWithStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  auto stream = webrtc::MediaStream::Create("audio_stream");
  auto sender = caller->pc()->AddTrack(audio_track.get(), {stream.get()});
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

TEST_F(PeerConnectionRtpCallbacksTest,
       RemoveTrackWithSharedStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track1(
      pc_factory_->CreateAudioTrack("audio_track1", nullptr));
  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track2(
      pc_factory_->CreateAudioTrack("audio_track2", nullptr));
  auto stream = webrtc::MediaStream::Create("shared_audio_stream");
  std::vector<webrtc::MediaStreamInterface*> streams{stream.get()};
  auto sender1 = caller->pc()->AddTrack(audio_track1.get(), streams);
  auto sender2 = caller->pc()->AddTrack(audio_track2.get(), streams);
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);

  // Remove "audio_track1".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender1));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>>{
          callee->observer()->add_track_events_[0].receiver},
      callee->observer()->remove_track_events_);

  // Remove "audio_track2".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender2));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

// These tests examine the state of the peer connection as a result of
// performing SetRemoteDescription().
class PeerConnectionRtpObserverTest : public PeerConnectionRtpTest {};

TEST_F(PeerConnectionRtpObserverTest, AddSenderWithoutStreamAddsReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  EXPECT_TRUE(caller->pc()->AddTrack(audio_track.get(), {}));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  EXPECT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver_added = callee->pc()->GetReceivers()[0];
  EXPECT_EQ("audio_track", receiver_added->track()->id());
  // TODO(hbos): When "no stream" is handled correctly we would expect
  // |receiver_added->streams()| to be empty. https://crbug.com/webrtc/7933
  EXPECT_EQ(receiver_added->streams().size(), 1u);
  EXPECT_TRUE(receiver_added->streams()[0]->FindAudioTrack("audio_track"));
}

TEST_F(PeerConnectionRtpObserverTest, AddSenderWithStreamAddsReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  auto stream = webrtc::MediaStream::Create("audio_stream");
  EXPECT_TRUE(caller->pc()->AddTrack(audio_track.get(), {stream}));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  EXPECT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver_added = callee->pc()->GetReceivers()[0];
  EXPECT_EQ("audio_track", receiver_added->track()->id());
  EXPECT_EQ(receiver_added->streams().size(), 1u);
  EXPECT_EQ("audio_stream", receiver_added->streams()[0]->label());
  EXPECT_TRUE(receiver_added->streams()[0]->FindAudioTrack("audio_track"));
}

TEST_F(PeerConnectionRtpObserverTest,
       RemoveSenderWithoutStreamRemovesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  auto sender = caller->pc()->AddTrack(audio_track.get(), {});
  ASSERT_TRUE(sender);
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  ASSERT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver = callee->pc()->GetReceivers()[0];
  ASSERT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  EXPECT_EQ(callee->pc()->GetReceivers().size(), 0u);
}

TEST_F(PeerConnectionRtpObserverTest, RemoveSenderWithStreamRemovesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  auto stream = webrtc::MediaStream::Create("audio_stream");
  auto sender = caller->pc()->AddTrack(audio_track.get(), {stream});
  ASSERT_TRUE(sender);
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  ASSERT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver = callee->pc()->GetReceivers()[0];
  ASSERT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  EXPECT_EQ(callee->pc()->GetReceivers().size(), 0u);
}

TEST_F(PeerConnectionRtpObserverTest,
       RemoveSenderWithSharedStreamRemovesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track1(
      pc_factory_->CreateAudioTrack("audio_track1", nullptr));
  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track2(
      pc_factory_->CreateAudioTrack("audio_track2", nullptr));
  auto stream = webrtc::MediaStream::Create("shared_audio_stream");
  std::vector<webrtc::MediaStreamInterface*> streams{stream.get()};
  auto sender1 = caller->pc()->AddTrack(audio_track1.get(), streams);
  auto sender2 = caller->pc()->AddTrack(audio_track2.get(), streams);
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));

  ASSERT_EQ(callee->pc()->GetReceivers().size(), 2u);
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver1;
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver2;
  if (callee->pc()->GetReceivers()[0]->track()->id() == "audio_track1") {
    receiver1 = callee->pc()->GetReceivers()[0];
    receiver2 = callee->pc()->GetReceivers()[1];
  } else {
    receiver1 = callee->pc()->GetReceivers()[1];
    receiver2 = callee->pc()->GetReceivers()[0];
  }
  EXPECT_EQ("audio_track1", receiver1->track()->id());
  EXPECT_EQ("audio_track2", receiver2->track()->id());

  // Remove "audio_track1".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender1));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  // Only |receiver2| should remain.
  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>>{receiver2},
      callee->pc()->GetReceivers());

  // Remove "audio_track2".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender2));
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(),
                                   static_cast<webrtc::RTCError*>(nullptr)));
  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  EXPECT_EQ(callee->pc()->GetReceivers().size(), 0u);
}

// Invokes SetRemoteDescription() twice in a row without synchronizing the two
// calls and examine the state of the peer connection inside the callbacks to
// ensure that the second call does not occur prematurely, contaminating the
// state of the peer connection of the first callback.
TEST_F(PeerConnectionRtpObserverTest,
       StatesCorrelateWithSetRemoteDescriptionCall) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
      pc_factory_->CreateAudioTrack("audio_track", nullptr));
  // Create SDP for adding a track and for removing it. This will be used in the
  // first and second SetRemoteDescription() calls.
  auto sender = caller->pc()->AddTrack(audio_track.get(), {});
  auto srd1_sdp = caller->CreateOfferAndSetAsLocal();
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  auto srd2_sdp = caller->CreateOfferAndSetAsLocal();

  // In the first SetRemoteDescription() callback, check that we have a
  // receiver for the track.
  auto pc = callee->pc();
  bool srd1_callback_called = false;
  auto srd1_callback = [&srd1_callback_called, &pc]() {
    EXPECT_EQ(pc->GetReceivers().size(), 1u);
    srd1_callback_called = true;
  };

  // In the second SetRemoteDescription() callback, check that the receiver has
  // been removed.
  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  // https://crbug.com/webrtc/7600
  bool srd2_callback_called = false;
  auto srd2_callback = [&srd2_callback_called, &pc]() {
    EXPECT_TRUE(pc->GetReceivers().empty());
    srd2_callback_called = true;
  };

  // Invoke SetRemoteDescription() twice in a row without synchronizing the two
  // calls. The callbacks verify that the two calls are synchronized, as in, the
  // effects of the second SetRemoteDescription() call must not have happened by
  // the time the first callback is invoked. If it has then the receiver that is
  // added as a result of the first SetRemoteDescription() call will already
  // have been removed as a result of the second SetRemoteDescription() call
  // when the first callback is invoked.
  callee->pc()->SetRemoteDescription(
      std::move(srd1_sdp),
      new OnSuccessObserver<decltype(srd1_callback)>(srd1_callback));
  callee->pc()->SetRemoteDescription(
      std::move(srd2_sdp),
      new OnSuccessObserver<decltype(srd2_callback)>(srd2_callback));
  EXPECT_TRUE_WAIT(srd1_callback_called, kDefaultTimeout);
  EXPECT_TRUE_WAIT(srd2_callback_called, kDefaultTimeout);
}

// Tests for the legacy SetRemoteDescription() function signature.
class PeerConnectionRtpLegacyObserverTest : public PeerConnectionRtpTest {};

// Sanity test making sure the callback is invoked.
TEST_F(PeerConnectionRtpLegacyObserverTest, OnSuccess) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  std::string error;
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(), &error));
}

// Verifies legacy behavior: The observer is not called if if the peer
// connection is destroyed because the asynchronous callback is executed in the
// peer connection's message handler.
TEST_F(PeerConnectionRtpLegacyObserverTest,
       ObserverNotCalledIfPeerConnectionDereferenced) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::MockSetSessionDescriptionObserver> observer =
      new rtc::RefCountedObject<webrtc::MockSetSessionDescriptionObserver>();

  auto offer = caller->CreateOfferAndSetAsLocal();
  callee->pc()->SetRemoteDescription(observer, offer.release());
  callee = nullptr;
  rtc::Thread::Current()->ProcessMessages(0);
  EXPECT_FALSE(observer->called());
}

}  // namespace
