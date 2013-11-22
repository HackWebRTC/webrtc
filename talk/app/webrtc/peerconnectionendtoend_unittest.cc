/*
 * libjingle
 * Copyright 2013, Google Inc.
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

#include "talk/app/webrtc/test/peerconnectiontestwrapper.h"
#include "talk/base/gunit.h"
#include "talk/base/logging.h"
#include "talk/base/ssladapter.h"
#include "talk/base/sslstreamadapter.h"
#include "talk/base/stringencode.h"
#include "talk/base/stringutils.h"

using webrtc::FakeConstraints;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionInterface;

namespace {

const char kExternalGiceUfrag[] = "1234567890123456";
const char kExternalGicePwd[] = "123456789012345678901234";

void RemoveLinesFromSdp(const std::string& line_start,
                               std::string* sdp) {
  const char kSdpLineEnd[] = "\r\n";
  size_t ssrc_pos = 0;
  while ((ssrc_pos = sdp->find(line_start, ssrc_pos)) !=
      std::string::npos) {
    size_t end_ssrc = sdp->find(kSdpLineEnd, ssrc_pos);
    sdp->erase(ssrc_pos, end_ssrc - ssrc_pos + strlen(kSdpLineEnd));
  }
}

// Add |newlines| to the |message| after |line|.
void InjectAfter(const std::string& line,
                 const std::string& newlines,
                 std::string* message) {
  const std::string tmp = line + newlines;
  talk_base::replace_substrs(line.c_str(), line.length(),
                             tmp.c_str(), tmp.length(), message);
}

void Replace(const std::string& line,
             const std::string& newlines,
             std::string* message) {
  talk_base::replace_substrs(line.c_str(), line.length(),
                             newlines.c_str(), newlines.length(), message);
}

void UseExternalSdes(std::string* sdp) {
  // Remove current crypto specification.
  RemoveLinesFromSdp("a=crypto", sdp);
  RemoveLinesFromSdp("a=fingerprint", sdp);
  // Add external crypto.
  const char kAudioSdes[] =
      "a=crypto:1 AES_CM_128_HMAC_SHA1_80 "
      "inline:PS1uQCVeeCFCanVmcjkpPywjNWhcYD0mXXtxaVBR\r\n";
  const char kVideoSdes[] =
      "a=crypto:1 AES_CM_128_HMAC_SHA1_80 "
      "inline:d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj\r\n";
  const char kDataSdes[] =
      "a=crypto:1 AES_CM_128_HMAC_SHA1_80 "
      "inline:NzB4d1BINUAvLEw6UzF3WSJ+PSdFcGdUJShpX1Zj\r\n";
  InjectAfter("a=mid:audio\r\n", kAudioSdes, sdp);
  InjectAfter("a=mid:video\r\n", kVideoSdes, sdp);
  InjectAfter("a=mid:data\r\n", kDataSdes, sdp);
}

void UseGice(std::string* sdp) {
  InjectAfter("t=0 0\r\n", "a=ice-options:google-ice\r\n", sdp);

  std::string ufragline = "a=ice-ufrag:";
  std::string pwdline = "a=ice-pwd:";
  RemoveLinesFromSdp(ufragline, sdp);
  RemoveLinesFromSdp(pwdline, sdp);
  ufragline.append(kExternalGiceUfrag);
  ufragline.append("\r\n");
  pwdline.append(kExternalGicePwd);
  pwdline.append("\r\n");
  const std::string ufrag_pwd = ufragline + pwdline;

  InjectAfter("a=mid:audio\r\n", ufrag_pwd, sdp);
  InjectAfter("a=mid:video\r\n", ufrag_pwd, sdp);
  InjectAfter("a=mid:data\r\n", ufrag_pwd, sdp);
}

void RemoveBundle(std::string* sdp) {
  RemoveLinesFromSdp("a=group:BUNDLE", sdp);
}

}  // namespace

class PeerConnectionEndToEndTest
    : public sigslot::has_slots<>,
      public testing::Test {
 public:
  PeerConnectionEndToEndTest()
      : caller_(new talk_base::RefCountedObject<PeerConnectionTestWrapper>(
                    "caller")),
        callee_(new talk_base::RefCountedObject<PeerConnectionTestWrapper>(
                    "callee")) {
    talk_base::InitializeSSL(NULL);
  }

  void CreatePcs() {
    CreatePcs(NULL);
  }

  void CreatePcs(const MediaConstraintsInterface* pc_constraints) {
    EXPECT_TRUE(caller_->CreatePc(pc_constraints));
    EXPECT_TRUE(callee_->CreatePc(pc_constraints));
    PeerConnectionTestWrapper::Connect(caller_.get(), callee_.get());
  }

  void GetAndAddUserMedia() {
    FakeConstraints audio_constraints;
    FakeConstraints video_constraints;
    GetAndAddUserMedia(true, audio_constraints, true, video_constraints);
  }

  void GetAndAddUserMedia(bool audio, FakeConstraints audio_constraints,
                          bool video, FakeConstraints video_constraints) {
    caller_->GetAndAddUserMedia(audio, audio_constraints,
                                video, video_constraints);
    callee_->GetAndAddUserMedia(audio, audio_constraints,
                                video, video_constraints);
  }

  void Negotiate() {
    caller_->CreateOffer(NULL);
  }

  void WaitForCallEstablished() {
    caller_->WaitForCallEstablished();
    callee_->WaitForCallEstablished();
  }

  void SetupLegacySdpConverter() {
    caller_->SignalOnSdpCreated.connect(
      this, &PeerConnectionEndToEndTest::ConvertToLegacySdp);
    callee_->SignalOnSdpCreated.connect(
      this, &PeerConnectionEndToEndTest::ConvertToLegacySdp);
  }

  void ConvertToLegacySdp(std::string* sdp) {
    UseExternalSdes(sdp);
    UseGice(sdp);
    RemoveBundle(sdp);
    LOG(LS_INFO) << "ConvertToLegacySdp: " << *sdp;
  }

  void SetupGiceConverter() {
    caller_->SignalOnIceCandidateCreated.connect(
      this, &PeerConnectionEndToEndTest::AddGiceCredsToCandidate);
    callee_->SignalOnIceCandidateCreated.connect(
      this, &PeerConnectionEndToEndTest::AddGiceCredsToCandidate);
  }

  void AddGiceCredsToCandidate(std::string* sdp) {
    std::string gice_creds = " username ";
    gice_creds.append(kExternalGiceUfrag);
    gice_creds.append(" password ");
    gice_creds.append(kExternalGicePwd);
    gice_creds.append("\r\n");
    Replace("\r\n", gice_creds, sdp);
    LOG(LS_INFO) << "AddGiceCredsToCandidate: " << *sdp;
  }

  ~PeerConnectionEndToEndTest() {
    talk_base::CleanupSSL();
  }

 protected:
  talk_base::scoped_refptr<PeerConnectionTestWrapper> caller_;
  talk_base::scoped_refptr<PeerConnectionTestWrapper> callee_;
};

// Disable for TSan v2, see
// https://code.google.com/p/webrtc/issues/detail?id=1205 for details.
#if !defined(THREAD_SANITIZER)

TEST_F(PeerConnectionEndToEndTest, Call) {
  CreatePcs();
  GetAndAddUserMedia();
  Negotiate();
  WaitForCallEstablished();
}

TEST_F(PeerConnectionEndToEndTest, CallWithLegacySdp) {
  FakeConstraints pc_constraints;
  pc_constraints.AddMandatory(MediaConstraintsInterface::kEnableDtlsSrtp,
                              false);
  CreatePcs(&pc_constraints);
  SetupLegacySdpConverter();
  SetupGiceConverter();
  GetAndAddUserMedia();
  Negotiate();
  WaitForCallEstablished();
}

#endif // if !defined(THREAD_SANITIZER)
