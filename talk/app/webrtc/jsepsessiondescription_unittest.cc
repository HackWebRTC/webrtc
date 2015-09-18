/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#include <string>

#include "talk/app/webrtc/jsepicecandidate.h"
#include "talk/app/webrtc/jsepsessiondescription.h"
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/constants.h"
#include "webrtc/p2p/base/sessiondescription.h"
#include "talk/session/media/mediasession.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/stringencode.h"

using webrtc::IceCandidateCollection;
using webrtc::IceCandidateInterface;
using webrtc::JsepIceCandidate;
using webrtc::JsepSessionDescription;
using webrtc::SessionDescriptionInterface;
using rtc::scoped_ptr;

static const char kCandidateUfrag[] = "ufrag";
static const char kCandidatePwd[] = "pwd";
static const char kCandidateUfragVoice[] = "ufrag_voice";
static const char kCandidatePwdVoice[] = "pwd_voice";
static const char kCandidateUfragVideo[] = "ufrag_video";
static const char kCandidatePwdVideo[] = "pwd_video";

// This creates a session description with both audio and video media contents.
// In SDP this is described by two m lines, one audio and one video.
static cricket::SessionDescription* CreateCricketSessionDescription() {
  cricket::SessionDescription* desc(new cricket::SessionDescription());
  // AudioContentDescription
  scoped_ptr<cricket::AudioContentDescription> audio(
      new cricket::AudioContentDescription());

  // VideoContentDescription
  scoped_ptr<cricket::VideoContentDescription> video(
      new cricket::VideoContentDescription());

  audio->AddCodec(cricket::AudioCodec(103, "ISAC", 16000, 0, 0, 0));
  desc->AddContent(cricket::CN_AUDIO, cricket::NS_JINGLE_RTP,
                   audio.release());

  video->AddCodec(cricket::VideoCodec(120, "VP8", 640, 480, 30, 0));
  desc->AddContent(cricket::CN_VIDEO, cricket::NS_JINGLE_RTP,
                   video.release());

  EXPECT_TRUE(desc->AddTransportInfo(
      cricket::TransportInfo(
                             cricket::CN_AUDIO,
                             cricket::TransportDescription(
                                 std::vector<std::string>(),
                                 kCandidateUfragVoice, kCandidatePwdVoice,
                                 cricket::ICEMODE_FULL,
                                 cricket::CONNECTIONROLE_NONE,
                                 NULL, cricket::Candidates()))));
  EXPECT_TRUE(desc->AddTransportInfo(
      cricket::TransportInfo(cricket::CN_VIDEO,
                             cricket::TransportDescription(
                                 std::vector<std::string>(),
                                 kCandidateUfragVideo, kCandidatePwdVideo,
                                 cricket::ICEMODE_FULL,
                                 cricket::CONNECTIONROLE_NONE,
                                 NULL, cricket::Candidates()))));
  return desc;
}

class JsepSessionDescriptionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    int port = 1234;
    rtc::SocketAddress address("127.0.0.1", port++);
    cricket::Candidate candidate(cricket::ICE_CANDIDATE_COMPONENT_RTP, "udp",
                                 address, 1, "", "", "local", 0, "1");
    candidate_ = candidate;
    const std::string session_id =
        rtc::ToString(rtc::CreateRandomId64());
    const std::string session_version =
        rtc::ToString(rtc::CreateRandomId());
    jsep_desc_.reset(new JsepSessionDescription("dummy"));
    ASSERT_TRUE(jsep_desc_->Initialize(CreateCricketSessionDescription(),
        session_id, session_version));
  }

  std::string Serialize(const SessionDescriptionInterface* desc) {
    std::string sdp;
    EXPECT_TRUE(desc->ToString(&sdp));
    EXPECT_FALSE(sdp.empty());
    return sdp;
  }

  SessionDescriptionInterface* DeSerialize(const std::string& sdp) {
    JsepSessionDescription* desc(new JsepSessionDescription("dummy"));
    EXPECT_TRUE(desc->Initialize(sdp, NULL));
    return desc;
  }

  cricket::Candidate candidate_;
  rtc::scoped_ptr<JsepSessionDescription> jsep_desc_;
};

// Test that number_of_mediasections() returns the number of media contents in
// a session description.
TEST_F(JsepSessionDescriptionTest, CheckSessionDescription) {
  EXPECT_EQ(2u, jsep_desc_->number_of_mediasections());
}

// Test that we can add a candidate to a session description.
TEST_F(JsepSessionDescriptionTest, AddCandidateWithoutMid) {
  JsepIceCandidate jsep_candidate("", 0, candidate_);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  const IceCandidateCollection* ice_candidates = jsep_desc_->candidates(0);
  ASSERT_TRUE(ice_candidates != NULL);
  EXPECT_EQ(1u, ice_candidates->count());
  const IceCandidateInterface* ice_candidate = ice_candidates->at(0);
  ASSERT_TRUE(ice_candidate != NULL);
  candidate_.set_username(kCandidateUfragVoice);
  candidate_.set_password(kCandidatePwdVoice);
  EXPECT_TRUE(ice_candidate->candidate().IsEquivalent(candidate_));
  EXPECT_EQ(0, ice_candidate->sdp_mline_index());
  EXPECT_EQ(0u, jsep_desc_->candidates(1)->count());
}

TEST_F(JsepSessionDescriptionTest, AddCandidateWithMid) {
  // mid and m-line index don't match, in this case mid is preferred.
  JsepIceCandidate jsep_candidate("video", 0, candidate_);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  EXPECT_EQ(0u, jsep_desc_->candidates(0)->count());
  const IceCandidateCollection* ice_candidates = jsep_desc_->candidates(1);
  ASSERT_TRUE(ice_candidates != NULL);
  EXPECT_EQ(1u, ice_candidates->count());
  const IceCandidateInterface* ice_candidate = ice_candidates->at(0);
  ASSERT_TRUE(ice_candidate != NULL);
  candidate_.set_username(kCandidateUfragVideo);
  candidate_.set_password(kCandidatePwdVideo);
  EXPECT_TRUE(ice_candidate->candidate().IsEquivalent(candidate_));
  // The mline index should have been updated according to mid.
  EXPECT_EQ(1, ice_candidate->sdp_mline_index());
}

TEST_F(JsepSessionDescriptionTest, AddCandidateAlreadyHasUfrag) {
  candidate_.set_username(kCandidateUfrag);
  candidate_.set_password(kCandidatePwd);
  JsepIceCandidate jsep_candidate("audio", 0, candidate_);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  const IceCandidateCollection* ice_candidates = jsep_desc_->candidates(0);
  ASSERT_TRUE(ice_candidates != NULL);
  EXPECT_EQ(1u, ice_candidates->count());
  const IceCandidateInterface* ice_candidate = ice_candidates->at(0);
  ASSERT_TRUE(ice_candidate != NULL);
  candidate_.set_username(kCandidateUfrag);
  candidate_.set_password(kCandidatePwd);
  EXPECT_TRUE(ice_candidate->candidate().IsEquivalent(candidate_));

  EXPECT_EQ(0u, jsep_desc_->candidates(1)->count());
}

// Test that we can not add a candidate if there is no corresponding media
// content in the session description.
TEST_F(JsepSessionDescriptionTest, AddBadCandidate) {
  JsepIceCandidate bad_candidate1("", 55, candidate_);
  EXPECT_FALSE(jsep_desc_->AddCandidate(&bad_candidate1));

  JsepIceCandidate bad_candidate2("some weird mid", 0, candidate_);
  EXPECT_FALSE(jsep_desc_->AddCandidate(&bad_candidate2));
}

// Tests that repeatedly adding the same candidate, with or without credentials,
// does not increase the number of candidates in the description.
TEST_F(JsepSessionDescriptionTest, AddCandidateDuplicates) {
  JsepIceCandidate jsep_candidate("", 0, candidate_);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  EXPECT_EQ(1u, jsep_desc_->candidates(0)->count());

  // Add the same candidate again.  It should be ignored.
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  EXPECT_EQ(1u, jsep_desc_->candidates(0)->count());

  // Create a new candidate, identical except that the ufrag and pwd are now
  // populated.
  candidate_.set_username(kCandidateUfragVoice);
  candidate_.set_password(kCandidatePwdVoice);
  JsepIceCandidate jsep_candidate_with_credentials("", 0, candidate_);

  // This should also be identified as redundant and ignored.
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate_with_credentials));
  EXPECT_EQ(1u, jsep_desc_->candidates(0)->count());
}

// Test that we can serialize a JsepSessionDescription and deserialize it again.
TEST_F(JsepSessionDescriptionTest, SerializeDeserialize) {
  std::string sdp = Serialize(jsep_desc_.get());

  scoped_ptr<SessionDescriptionInterface> parsed_jsep_desc(DeSerialize(sdp));
  EXPECT_EQ(2u, parsed_jsep_desc->number_of_mediasections());

  std::string parsed_sdp = Serialize(parsed_jsep_desc.get());
  EXPECT_EQ(sdp, parsed_sdp);
}

// Tests that we can serialize and deserialize a JsepSesssionDescription
// with candidates.
TEST_F(JsepSessionDescriptionTest, SerializeDeserializeWithCandidates) {
  std::string sdp = Serialize(jsep_desc_.get());

  // Add a candidate and check that the serialized result is different.
  JsepIceCandidate jsep_candidate("audio", 0, candidate_);
  EXPECT_TRUE(jsep_desc_->AddCandidate(&jsep_candidate));
  std::string sdp_with_candidate = Serialize(jsep_desc_.get());
  EXPECT_NE(sdp, sdp_with_candidate);

  scoped_ptr<SessionDescriptionInterface> parsed_jsep_desc(
      DeSerialize(sdp_with_candidate));
  std::string parsed_sdp_with_candidate = Serialize(parsed_jsep_desc.get());

  EXPECT_EQ(sdp_with_candidate, parsed_sdp_with_candidate);
}
