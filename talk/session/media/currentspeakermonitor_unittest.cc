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
#include "talk/base/thread.h"
#include "talk/session/media/call.h"
#include "talk/session/media/currentspeakermonitor.h"

namespace cricket {

static const uint32 kSsrc1 = 1001;
static const uint32 kSsrc2 = 1002;
static const uint32 kMinTimeBetweenSwitches = 10;
// Due to limited system clock resolution, the CurrentSpeakerMonitor may
// actually require more or less time between switches than that specified
// in the call to set_min_time_between_switches.  To be safe, we sleep for
// 90 ms more than the min time between switches before checking for a switch.
// I am assuming system clocks do not have a coarser resolution than 90 ms.
static const uint32 kSleepTimeBetweenSwitches = 100;

class MockCall : public Call {
 public:
  MockCall() : Call(NULL) {}

  void EmitAudioMonitor(const AudioInfo& info) {
    SignalAudioMonitor(this, info);
  }
};

class CurrentSpeakerMonitorTest : public testing::Test,
    public sigslot::has_slots<> {
 public:
  CurrentSpeakerMonitorTest() {
    call_ = new MockCall();
    monitor_ = new CurrentSpeakerMonitor(call_, NULL);
    // Shrink the minimum time betweeen switches to 10 ms so we don't have to
    // slow down our tests.
    monitor_->set_min_time_between_switches(kMinTimeBetweenSwitches);
    monitor_->SignalUpdate.connect(this, &CurrentSpeakerMonitorTest::OnUpdate);
    current_speaker_ = 0;
    num_changes_ = 0;
    monitor_->Start();
  }

  ~CurrentSpeakerMonitorTest() {
    delete monitor_;
    delete call_;
  }

 protected:
  MockCall* call_;
  CurrentSpeakerMonitor* monitor_;
  int num_changes_;
  uint32 current_speaker_;

  void OnUpdate(CurrentSpeakerMonitor* monitor, uint32 current_speaker) {
    current_speaker_ = current_speaker;
    num_changes_++;
  }
};

static void InitAudioInfo(AudioInfo* info, int input_level, int output_level) {
  info->input_level = input_level;
  info->output_level = output_level;
}

TEST_F(CurrentSpeakerMonitorTest, NoActiveStreams) {
  AudioInfo info;
  InitAudioInfo(&info, 0, 0);
  call_->EmitAudioMonitor(info);

  EXPECT_EQ(current_speaker_, 0U);
  EXPECT_EQ(num_changes_, 0);
}

TEST_F(CurrentSpeakerMonitorTest, MultipleActiveStreams) {
  AudioInfo info;
  InitAudioInfo(&info, 0, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  call_->EmitAudioMonitor(info);

  // No speaker recognized because the initial sample is treated as possibly
  // just noise and disregarded.
  EXPECT_EQ(current_speaker_, 0U);
  EXPECT_EQ(num_changes_, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  call_->EmitAudioMonitor(info);

  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);
}

TEST_F(CurrentSpeakerMonitorTest, RapidSpeakerChange) {
  AudioInfo info;
  InitAudioInfo(&info, 0, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  call_->EmitAudioMonitor(info);

  EXPECT_EQ(current_speaker_, 0U);
  EXPECT_EQ(num_changes_, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  call_->EmitAudioMonitor(info);

  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);

  info.active_streams.push_back(std::make_pair(kSsrc1, 9));
  info.active_streams.push_back(std::make_pair(kSsrc2, 1));
  call_->EmitAudioMonitor(info);

  // We expect no speaker change because of the rapid change.
  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);
}

TEST_F(CurrentSpeakerMonitorTest, SpeakerChange) {
  AudioInfo info;
  InitAudioInfo(&info, 0, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  call_->EmitAudioMonitor(info);

  EXPECT_EQ(current_speaker_, 0U);
  EXPECT_EQ(num_changes_, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  call_->EmitAudioMonitor(info);

  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);

  // Wait so the changes don't come so rapidly.
  talk_base::Thread::SleepMs(kSleepTimeBetweenSwitches);

  info.active_streams.push_back(std::make_pair(kSsrc1, 9));
  info.active_streams.push_back(std::make_pair(kSsrc2, 1));
  call_->EmitAudioMonitor(info);

  EXPECT_EQ(current_speaker_, kSsrc1);
  EXPECT_EQ(num_changes_, 2);
}

TEST_F(CurrentSpeakerMonitorTest, InterwordSilence) {
  AudioInfo info;
  InitAudioInfo(&info, 0, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  call_->EmitAudioMonitor(info);

  EXPECT_EQ(current_speaker_, 0U);
  EXPECT_EQ(num_changes_, 0);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  call_->EmitAudioMonitor(info);

  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 7));
  call_->EmitAudioMonitor(info);

  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);

  // Wait so the changes don't come so rapidly.
  talk_base::Thread::SleepMs(kSleepTimeBetweenSwitches);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 0));
  call_->EmitAudioMonitor(info);

  // Current speaker shouldn't have changed because we treat this as an inter-
  // word silence.
  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 0));
  call_->EmitAudioMonitor(info);

  // Current speaker shouldn't have changed because we treat this as an inter-
  // word silence.
  EXPECT_EQ(current_speaker_, kSsrc2);
  EXPECT_EQ(num_changes_, 1);

  info.active_streams.push_back(std::make_pair(kSsrc1, 3));
  info.active_streams.push_back(std::make_pair(kSsrc2, 0));
  call_->EmitAudioMonitor(info);

  // At this point, we should have concluded that SSRC2 stopped speaking.
  EXPECT_EQ(current_speaker_, kSsrc1);
  EXPECT_EQ(num_changes_, 2);
}

}  // namespace cricket
