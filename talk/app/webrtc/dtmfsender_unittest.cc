/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#include "talk/app/webrtc/dtmfsender.h"

#include <set>
#include <string>
#include <vector>

#include "talk/app/webrtc/audiotrack.h"
#include "talk/base/gunit.h"
#include "talk/base/logging.h"
#include "talk/base/timeutils.h"

using webrtc::AudioTrackInterface;
using webrtc::AudioTrack;
using webrtc::DtmfProviderInterface;
using webrtc::DtmfSender;
using webrtc::DtmfSenderObserverInterface;

static const char kTestAudioLabel[] = "test_audio_track";
static const int kMaxWaitMs = 3000;

class FakeDtmfObserver : public DtmfSenderObserverInterface {
 public:
  FakeDtmfObserver() : completed_(false) {}

  // Implements DtmfSenderObserverInterface.
  virtual void OnToneChange(const std::string& tone) OVERRIDE {
    LOG(LS_VERBOSE) << "FakeDtmfObserver::OnToneChange '" << tone << "'.";
    tones_.push_back(tone);
    if (tone.empty()) {
      completed_ = true;
    }
  }

  // getters
  const std::vector<std::string>& tones() const {
    return tones_;
  }
  bool completed() const {
    return completed_;
  }

 private:
  std::vector<std::string> tones_;
  bool completed_;
};

class FakeDtmfProvider : public DtmfProviderInterface {
 public:
  struct DtmfInfo {
    DtmfInfo(int code, int duration, int gap)
      : code(code),
        duration(duration),
        gap(gap) {}
    int code;
    int duration;
    int gap;
  };

  FakeDtmfProvider() : last_insert_dtmf_call_(0) {}

  ~FakeDtmfProvider() {
    SignalDestroyed();
  }

  // Implements DtmfProviderInterface.
  virtual bool CanInsertDtmf(const std::string&  track_label) OVERRIDE {
    return (can_insert_dtmf_tracks_.count(track_label) != 0);
  }

  virtual bool InsertDtmf(const std::string& track_label,
                          int code, int duration) OVERRIDE {
    int gap = 0;
    // TODO(ronghuawu): Make the timer (basically the talk_base::TimeNanos)
    // mockable and use a fake timer in the unit tests.
    if (last_insert_dtmf_call_ > 0) {
      gap = static_cast<int>(talk_base::Time() - last_insert_dtmf_call_);
    }
    last_insert_dtmf_call_ = talk_base::Time();

    LOG(LS_VERBOSE) << "FakeDtmfProvider::InsertDtmf code=" << code
                    << " duration=" << duration
                    << " gap=" << gap << ".";
    dtmf_info_queue_.push_back(DtmfInfo(code, duration, gap));
    return true;
  }

  virtual sigslot::signal0<>* GetOnDestroyedSignal() {
    return &SignalDestroyed;
  }

  // getter and setter
  const std::vector<DtmfInfo>& dtmf_info_queue() const {
    return dtmf_info_queue_;
  }

  // helper functions
  void AddCanInsertDtmfTrack(const std::string& label) {
    can_insert_dtmf_tracks_.insert(label);
  }
  void RemoveCanInsertDtmfTrack(const std::string& label) {
    can_insert_dtmf_tracks_.erase(label);
  }

 private:
  std::set<std::string> can_insert_dtmf_tracks_;
  std::vector<DtmfInfo> dtmf_info_queue_;
  int64 last_insert_dtmf_call_;
  sigslot::signal0<> SignalDestroyed;
};

class DtmfSenderTest : public testing::Test {
 protected:
  DtmfSenderTest()
      : track_(AudioTrack::Create(kTestAudioLabel, NULL)),
        observer_(new talk_base::RefCountedObject<FakeDtmfObserver>()),
        provider_(new FakeDtmfProvider()) {
    provider_->AddCanInsertDtmfTrack(kTestAudioLabel);
    dtmf_ = DtmfSender::Create(track_, talk_base::Thread::Current(),
                               provider_.get());
    dtmf_->RegisterObserver(observer_.get());
  }

  ~DtmfSenderTest() {
    if (dtmf_.get()) {
      dtmf_->UnregisterObserver();
    }
  }

  // Constructs a list of DtmfInfo from |tones|, |duration| and
  // |inter_tone_gap|.
  void GetDtmfInfoFromString(const std::string& tones, int duration,
                             int inter_tone_gap,
                             std::vector<FakeDtmfProvider::DtmfInfo>* dtmfs) {
    // Init extra_delay as -inter_tone_gap - duration to ensure the first
    // DtmfInfo's gap field will be 0.
    int extra_delay = -1 * (inter_tone_gap + duration);

    std::string::const_iterator it = tones.begin();
    for (; it != tones.end(); ++it) {
      char tone = *it;
      int code = 0;
      webrtc::GetDtmfCode(tone, &code);
      if (tone == ',') {
        extra_delay = 2000;  // 2 seconds
      } else {
        dtmfs->push_back(FakeDtmfProvider::DtmfInfo(code, duration,
                         duration + inter_tone_gap + extra_delay));
        extra_delay = 0;
      }
    }
  }

  void VerifyExpectedState(AudioTrackInterface* track,
                          const std::string& tones,
                          int duration, int inter_tone_gap) {
    EXPECT_EQ(track, dtmf_->track());
    EXPECT_EQ(tones, dtmf_->tones());
    EXPECT_EQ(duration, dtmf_->duration());
    EXPECT_EQ(inter_tone_gap, dtmf_->inter_tone_gap());
  }

  // Verify the provider got all the expected calls.
  void VerifyOnProvider(const std::string& tones, int duration,
                        int inter_tone_gap) {
    std::vector<FakeDtmfProvider::DtmfInfo> dtmf_queue_ref;
    GetDtmfInfoFromString(tones, duration, inter_tone_gap, &dtmf_queue_ref);
    VerifyOnProvider(dtmf_queue_ref);
  }

  void VerifyOnProvider(
      const std::vector<FakeDtmfProvider::DtmfInfo>& dtmf_queue_ref) {
    const std::vector<FakeDtmfProvider::DtmfInfo>& dtmf_queue =
        provider_->dtmf_info_queue();
    ASSERT_EQ(dtmf_queue_ref.size(), dtmf_queue.size());
    std::vector<FakeDtmfProvider::DtmfInfo>::const_iterator it_ref =
        dtmf_queue_ref.begin();
    std::vector<FakeDtmfProvider::DtmfInfo>::const_iterator it =
        dtmf_queue.begin();
    while (it_ref != dtmf_queue_ref.end() && it != dtmf_queue.end()) {
      EXPECT_EQ(it_ref->code, it->code);
      EXPECT_EQ(it_ref->duration, it->duration);
      // Allow ~100ms error.
      EXPECT_GE(it_ref->gap, it->gap - 100);
      EXPECT_LE(it_ref->gap, it->gap + 100);
      ++it_ref;
      ++it;
    }
  }

  // Verify the observer got all the expected callbacks.
  void VerifyOnObserver(const std::string& tones_ref) {
    const std::vector<std::string>& tones = observer_->tones();
    // The observer will get an empty string at the end.
    EXPECT_EQ(tones_ref.size() + 1, tones.size());
    EXPECT_TRUE(tones.back().empty());
    std::string::const_iterator it_ref = tones_ref.begin();
    std::vector<std::string>::const_iterator it = tones.begin();
    while (it_ref != tones_ref.end() && it != tones.end()) {
      EXPECT_EQ(*it_ref, it->at(0));
      ++it_ref;
      ++it;
    }
  }

  talk_base::scoped_refptr<AudioTrackInterface> track_;
  talk_base::scoped_ptr<FakeDtmfObserver> observer_;
  talk_base::scoped_ptr<FakeDtmfProvider> provider_;
  talk_base::scoped_refptr<DtmfSender> dtmf_;
};

TEST_F(DtmfSenderTest, CanInsertDtmf) {
  EXPECT_TRUE(dtmf_->CanInsertDtmf());
  provider_->RemoveCanInsertDtmfTrack(kTestAudioLabel);
  EXPECT_FALSE(dtmf_->CanInsertDtmf());
}

TEST_F(DtmfSenderTest, InsertDtmf) {
  std::string tones = "@1%a&*$";
  int duration = 100;
  int inter_tone_gap = 50;
  EXPECT_TRUE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
  EXPECT_TRUE_WAIT(observer_->completed(), kMaxWaitMs);

  // The unrecognized characters should be ignored.
  std::string known_tones = "1a*";
  VerifyOnProvider(known_tones, duration, inter_tone_gap);
  VerifyOnObserver(known_tones);
}

TEST_F(DtmfSenderTest, InsertDtmfTwice) {
  std::string tones1 = "12";
  std::string tones2 = "ab";
  int duration = 100;
  int inter_tone_gap = 50;
  EXPECT_TRUE(dtmf_->InsertDtmf(tones1, duration, inter_tone_gap));
  VerifyExpectedState(track_, tones1, duration, inter_tone_gap);
  // Wait until the first tone got sent.
  EXPECT_TRUE_WAIT(observer_->tones().size() == 1, kMaxWaitMs);
  VerifyExpectedState(track_, "2", duration, inter_tone_gap);
  // Insert with another tone buffer.
  EXPECT_TRUE(dtmf_->InsertDtmf(tones2, duration, inter_tone_gap));
  VerifyExpectedState(track_, tones2, duration, inter_tone_gap);
  // Wait until it's completed.
  EXPECT_TRUE_WAIT(observer_->completed(), kMaxWaitMs);

  std::vector<FakeDtmfProvider::DtmfInfo> dtmf_queue_ref;
  GetDtmfInfoFromString("1", duration, inter_tone_gap, &dtmf_queue_ref);
  GetDtmfInfoFromString("ab", duration, inter_tone_gap, &dtmf_queue_ref);
  VerifyOnProvider(dtmf_queue_ref);
  VerifyOnObserver("1ab");
}

TEST_F(DtmfSenderTest, InsertDtmfWhileProviderIsDeleted) {
  std::string tones = "@1%a&*$";
  int duration = 100;
  int inter_tone_gap = 50;
  EXPECT_TRUE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
  // Wait until the first tone got sent.
  EXPECT_TRUE_WAIT(observer_->tones().size() == 1, kMaxWaitMs);
  // Delete provider.
  provider_.reset();
  // The queue should be discontinued so no more tone callbacks.
  WAIT(false, 200);
  EXPECT_EQ(1U, observer_->tones().size());
}

TEST_F(DtmfSenderTest, InsertDtmfWhileSenderIsDeleted) {
  std::string tones = "@1%a&*$";
  int duration = 100;
  int inter_tone_gap = 50;
  EXPECT_TRUE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
  // Wait until the first tone got sent.
  EXPECT_TRUE_WAIT(observer_->tones().size() == 1, kMaxWaitMs);
  // Delete the sender.
  dtmf_ = NULL;
  // The queue should be discontinued so no more tone callbacks.
  WAIT(false, 200);
  EXPECT_EQ(1U, observer_->tones().size());
}

TEST_F(DtmfSenderTest, InsertEmptyTonesToCancelPreviousTask) {
  std::string tones1 = "12";
  std::string tones2 = "";
  int duration = 100;
  int inter_tone_gap = 50;
  EXPECT_TRUE(dtmf_->InsertDtmf(tones1, duration, inter_tone_gap));
  // Wait until the first tone got sent.
  EXPECT_TRUE_WAIT(observer_->tones().size() == 1, kMaxWaitMs);
  // Insert with another tone buffer.
  EXPECT_TRUE(dtmf_->InsertDtmf(tones2, duration, inter_tone_gap));
  // Wait until it's completed.
  EXPECT_TRUE_WAIT(observer_->completed(), kMaxWaitMs);

  std::vector<FakeDtmfProvider::DtmfInfo> dtmf_queue_ref;
  GetDtmfInfoFromString("1", duration, inter_tone_gap, &dtmf_queue_ref);
  VerifyOnProvider(dtmf_queue_ref);
  VerifyOnObserver("1");
}

TEST_F(DtmfSenderTest, InsertDtmfWithCommaAsDelay) {
  std::string tones = "3,4";
  int duration = 100;
  int inter_tone_gap = 50;
  EXPECT_TRUE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
  EXPECT_TRUE_WAIT(observer_->completed(), kMaxWaitMs);

  VerifyOnProvider(tones, duration, inter_tone_gap);
  VerifyOnObserver(tones);
}

TEST_F(DtmfSenderTest, TryInsertDtmfWhenItDoesNotWork) {
  std::string tones = "3,4";
  int duration = 100;
  int inter_tone_gap = 50;
  provider_->RemoveCanInsertDtmfTrack(kTestAudioLabel);
  EXPECT_FALSE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
}

TEST_F(DtmfSenderTest, InsertDtmfWithInvalidDurationOrGap) {
  std::string tones = "3,4";
  int duration = 100;
  int inter_tone_gap = 50;

  EXPECT_FALSE(dtmf_->InsertDtmf(tones, 6001, inter_tone_gap));
  EXPECT_FALSE(dtmf_->InsertDtmf(tones, 69, inter_tone_gap));
  EXPECT_FALSE(dtmf_->InsertDtmf(tones, duration, 49));

  EXPECT_TRUE(dtmf_->InsertDtmf(tones, duration, inter_tone_gap));
}
