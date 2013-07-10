/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#include "talk/base/gunit.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/p2p/base/fakesession.h"
#include "talk/session/media/channel.h"
#include "talk/session/media/currentspeakermonitor.h"
#include "talk/session/media/typingmonitor.h"

namespace cricket {

class TypingMonitorTest : public testing::Test {
 protected:
  TypingMonitorTest() : session_(true) {
    vc_.reset(new VoiceChannel(talk_base::Thread::Current(), &engine_,
                               engine_.CreateChannel(), &session_, "", false));
    engine_.GetVoiceChannel(0)->set_time_since_last_typing(1000);

    TypingMonitorOptions settings = {10, 20, 30, 40, 50};
    monitor_.reset(new TypingMonitor(vc_.get(),
                                     talk_base::Thread::Current(),
                                     settings));
  }

  void TearDown() {
    vc_.reset();
  }

  talk_base::scoped_ptr<TypingMonitor> monitor_;
  talk_base::scoped_ptr<VoiceChannel> vc_;
  FakeMediaEngine engine_;
  FakeSession session_;
};

TEST_F(TypingMonitorTest, TestTriggerMute) {
  EXPECT_FALSE(vc_->IsStreamMuted(0));
  EXPECT_FALSE(engine_.GetVoiceChannel(0)->IsStreamMuted(0));

  engine_.GetVoiceChannel(0)->TriggerError(0, VoiceMediaChannel::ERROR_OTHER);
  EXPECT_FALSE(vc_->IsStreamMuted(0));
  EXPECT_FALSE(engine_.GetVoiceChannel(0)->IsStreamMuted(0));

  engine_.GetVoiceChannel(0)->TriggerError(
      0, VoiceMediaChannel::ERROR_REC_TYPING_NOISE_DETECTED);
  EXPECT_TRUE(vc_->IsStreamMuted(0));
  EXPECT_TRUE(engine_.GetVoiceChannel(0)->IsStreamMuted(0));

  EXPECT_TRUE_WAIT(!vc_->IsStreamMuted(0) &&
                   !engine_.GetVoiceChannel(0)->IsStreamMuted(0), 100);
}

TEST_F(TypingMonitorTest, TestResetMonitor) {
  engine_.GetVoiceChannel(0)->set_time_since_last_typing(1000);
  EXPECT_FALSE(vc_->IsStreamMuted(0));
  EXPECT_FALSE(engine_.GetVoiceChannel(0)->IsStreamMuted(0));

  engine_.GetVoiceChannel(0)->TriggerError(
      0, VoiceMediaChannel::ERROR_REC_TYPING_NOISE_DETECTED);
  EXPECT_TRUE(vc_->IsStreamMuted(0));
  EXPECT_TRUE(engine_.GetVoiceChannel(0)->IsStreamMuted(0));

  monitor_.reset();
  EXPECT_FALSE(vc_->IsStreamMuted(0));
  EXPECT_FALSE(engine_.GetVoiceChannel(0)->IsStreamMuted(0));
}

}  // namespace cricket
