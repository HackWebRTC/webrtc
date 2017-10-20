/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/peerconnectionwrapper.h"

#include <memory>
#include <string>
#include <utility>

#include "api/jsepsessiondescription.h"
#include "media/base/fakevideocapturer.h"
#include "pc/sdputils.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

namespace {
const uint32_t kWaitTimeout = 10000U;
}

PeerConnectionWrapper::PeerConnectionWrapper(
    rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
    rtc::scoped_refptr<PeerConnectionInterface> pc,
    std::unique_ptr<MockPeerConnectionObserver> observer)
    : pc_factory_(pc_factory), pc_(pc), observer_(std::move(observer)) {
  RTC_DCHECK(pc_factory_);
  RTC_DCHECK(pc_);
  RTC_DCHECK(observer_);
  observer_->SetPeerConnectionInterface(pc_.get());
}

PeerConnectionWrapper::~PeerConnectionWrapper() = default;

PeerConnectionFactoryInterface* PeerConnectionWrapper::pc_factory() {
  return pc_factory_.get();
}

PeerConnectionInterface* PeerConnectionWrapper::pc() {
  return pc_.get();
}

MockPeerConnectionObserver* PeerConnectionWrapper::observer() {
  return observer_.get();
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateOffer() {
  return CreateOffer(PeerConnectionInterface::RTCOfferAnswerOptions());
}

std::unique_ptr<SessionDescriptionInterface> PeerConnectionWrapper::CreateOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  return CreateSdp([this, options](CreateSessionDescriptionObserver* observer) {
    pc()->CreateOffer(observer, options);
  });
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateOfferAndSetAsLocal() {
  auto offer = CreateOffer();
  if (!offer) {
    return nullptr;
  }
  EXPECT_TRUE(SetLocalDescription(CloneSessionDescription(offer.get())));
  return offer;
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswer() {
  return CreateAnswer(PeerConnectionInterface::RTCOfferAnswerOptions());
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  return CreateSdp([this, options](CreateSessionDescriptionObserver* observer) {
    pc()->CreateAnswer(observer, options);
  });
}

std::unique_ptr<SessionDescriptionInterface>
PeerConnectionWrapper::CreateAnswerAndSetAsLocal() {
  auto answer = CreateAnswer();
  if (!answer) {
    return nullptr;
  }
  EXPECT_TRUE(SetLocalDescription(CloneSessionDescription(answer.get())));
  return answer;
}

std::unique_ptr<SessionDescriptionInterface> PeerConnectionWrapper::CreateSdp(
    std::function<void(CreateSessionDescriptionObserver*)> fn) {
  rtc::scoped_refptr<MockCreateSessionDescriptionObserver> observer(
      new rtc::RefCountedObject<MockCreateSessionDescriptionObserver>());
  fn(observer);
  EXPECT_EQ_WAIT(true, observer->called(), kWaitTimeout);
  return observer->MoveDescription();
}

bool PeerConnectionWrapper::SetLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc) {
  return SetSdp([this, &desc](SetSessionDescriptionObserver* observer) {
    pc()->SetLocalDescription(observer, desc.release());
  });
}

bool PeerConnectionWrapper::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc) {
  return SetSdp([this, &desc](SetSessionDescriptionObserver* observer) {
    pc()->SetRemoteDescription(observer, desc.release());
  });
}

bool PeerConnectionWrapper::SetSdp(
    std::function<void(SetSessionDescriptionObserver*)> fn) {
  rtc::scoped_refptr<MockSetSessionDescriptionObserver> observer(
      new rtc::RefCountedObject<MockSetSessionDescriptionObserver>());
  fn(observer);
  if (pc()->signaling_state() != PeerConnectionInterface::kClosed) {
    EXPECT_EQ_WAIT(true, observer->called(), kWaitTimeout);
  }
  return observer->result();
}

void PeerConnectionWrapper::AddAudioStream(const std::string& stream_label,
                                           const std::string& track_label) {
  auto stream = pc_factory()->CreateLocalMediaStream(stream_label);
  auto audio_track = pc_factory()->CreateAudioTrack(track_label, nullptr);
  EXPECT_TRUE(pc()->AddTrack(audio_track, {stream}));
  EXPECT_TRUE_WAIT(observer()->renegotiation_needed_, kWaitTimeout);
  observer()->renegotiation_needed_ = false;
}

void PeerConnectionWrapper::AddVideoStream(const std::string& stream_label,
                                           const std::string& track_label) {
  auto stream = pc_factory()->CreateLocalMediaStream(stream_label);
  auto video_source = pc_factory()->CreateVideoSource(
      rtc::MakeUnique<cricket::FakeVideoCapturer>());
  auto video_track = pc_factory()->CreateVideoTrack(track_label, video_source);
  EXPECT_TRUE(pc()->AddTrack(video_track, {stream}));
  EXPECT_TRUE_WAIT(observer()->renegotiation_needed_, kWaitTimeout);
  observer()->renegotiation_needed_ = false;
}

void PeerConnectionWrapper::AddAudioVideoStream(
    const std::string& stream_label,
    const std::string& audio_track_label,
    const std::string& video_track_label) {
  auto stream = pc_factory()->CreateLocalMediaStream(stream_label);
  auto audio_track = pc_factory()->CreateAudioTrack(audio_track_label, nullptr);
  EXPECT_TRUE(pc()->AddTrack(audio_track, {stream}));
  auto video_source = pc_factory()->CreateVideoSource(
      rtc::MakeUnique<cricket::FakeVideoCapturer>());
  auto video_track =
      pc_factory()->CreateVideoTrack(video_track_label, video_source);
  EXPECT_TRUE(pc()->AddTrack(video_track, {stream}));
  EXPECT_TRUE_WAIT(observer()->renegotiation_needed_, kWaitTimeout);
  observer()->renegotiation_needed_ = false;
}

bool PeerConnectionWrapper::IsIceGatheringDone() {
  return observer()->ice_complete_;
}

}  // namespace webrtc
