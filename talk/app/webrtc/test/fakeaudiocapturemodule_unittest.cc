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

#include "talk/app/webrtc/test/fakeaudiocapturemodule.h"

#include <algorithm>

#include "talk/base/gunit.h"
#include "talk/base/scoped_ref_ptr.h"
#include "talk/base/thread.h"

using std::min;

class FakeAdmTest : public testing::Test,
                    public webrtc::AudioTransport {
 protected:
  static const int kMsInSecond = 1000;

  FakeAdmTest()
      : push_iterations_(0),
        pull_iterations_(0),
        rec_buffer_bytes_(0) {
    memset(rec_buffer_, 0, sizeof(rec_buffer_));
  }

  virtual void SetUp() {
    fake_audio_capture_module_ = FakeAudioCaptureModule::Create(
        talk_base::Thread::Current());
    EXPECT_TRUE(fake_audio_capture_module_.get() != NULL);
  }

  // Callbacks inherited from webrtc::AudioTransport.
  // ADM is pushing data.
  virtual int32_t RecordedDataIsAvailable(const void* audioSamples,
                                          const uint32_t nSamples,
                                          const uint8_t nBytesPerSample,
                                          const uint8_t nChannels,
                                          const uint32_t samplesPerSec,
                                          const uint32_t totalDelayMS,
                                          const int32_t clockDrift,
                                          const uint32_t currentMicLevel,
                                          const bool keyPressed,
                                          uint32_t& newMicLevel) {
    rec_buffer_bytes_ = nSamples * nBytesPerSample;
    if ((rec_buffer_bytes_ <= 0) ||
        (rec_buffer_bytes_ > FakeAudioCaptureModule::kNumberSamples *
         FakeAudioCaptureModule::kNumberBytesPerSample)) {
      ADD_FAILURE();
      return -1;
    }
    memcpy(rec_buffer_, audioSamples, rec_buffer_bytes_);
    ++push_iterations_;
    newMicLevel = currentMicLevel;
    return 0;
  }

  // ADM is pulling data.
  virtual int32_t NeedMorePlayData(const uint32_t nSamples,
                                   const uint8_t nBytesPerSample,
                                   const uint8_t nChannels,
                                   const uint32_t samplesPerSec,
                                   void* audioSamples,
                                   uint32_t& nSamplesOut) {
    ++pull_iterations_;
    const uint32_t audio_buffer_size = nSamples * nBytesPerSample;
    const uint32_t bytes_out = RecordedDataReceived() ?
        CopyFromRecBuffer(audioSamples, audio_buffer_size):
        GenerateZeroBuffer(audioSamples, audio_buffer_size);
    nSamplesOut = bytes_out / nBytesPerSample;
    return 0;
  }

  int push_iterations() const { return push_iterations_; }
  int pull_iterations() const { return pull_iterations_; }

  talk_base::scoped_refptr<FakeAudioCaptureModule> fake_audio_capture_module_;

 private:
  bool RecordedDataReceived() const {
    return rec_buffer_bytes_ != 0;
  }
  int32_t GenerateZeroBuffer(void* audio_buffer, uint32_t audio_buffer_size) {
    memset(audio_buffer, 0, audio_buffer_size);
    return audio_buffer_size;
  }
  int32_t CopyFromRecBuffer(void* audio_buffer, uint32_t audio_buffer_size) {
    EXPECT_EQ(audio_buffer_size, rec_buffer_bytes_);
    const uint32_t min_buffer_size = min(audio_buffer_size, rec_buffer_bytes_);
    memcpy(audio_buffer, rec_buffer_, min_buffer_size);
    return min_buffer_size;
  }

  int push_iterations_;
  int pull_iterations_;

  char rec_buffer_[FakeAudioCaptureModule::kNumberSamples *
                   FakeAudioCaptureModule::kNumberBytesPerSample];
  uint32_t rec_buffer_bytes_;
};

TEST_F(FakeAdmTest, TestProccess) {
  // Next process call must be some time in the future (or now).
  EXPECT_LE(0, fake_audio_capture_module_->TimeUntilNextProcess());
  // Process call updates TimeUntilNextProcess() but there are no guarantees on
  // timing so just check that Process can ba called successfully.
  EXPECT_LE(0, fake_audio_capture_module_->Process());
}

TEST_F(FakeAdmTest, PlayoutTest) {
  EXPECT_EQ(0, fake_audio_capture_module_->RegisterAudioCallback(this));

  bool speaker_available = false;
  EXPECT_EQ(0, fake_audio_capture_module_->SpeakerIsAvailable(
      &speaker_available));
  EXPECT_TRUE(speaker_available);

  bool stereo_available = false;
  EXPECT_EQ(0,
            fake_audio_capture_module_->StereoPlayoutIsAvailable(
                &stereo_available));
  EXPECT_TRUE(stereo_available);

  EXPECT_NE(0, fake_audio_capture_module_->StartPlayout());
  EXPECT_FALSE(fake_audio_capture_module_->PlayoutIsInitialized());
  EXPECT_FALSE(fake_audio_capture_module_->Playing());
  EXPECT_EQ(0, fake_audio_capture_module_->StopPlayout());

  EXPECT_EQ(0, fake_audio_capture_module_->InitPlayout());
  EXPECT_TRUE(fake_audio_capture_module_->PlayoutIsInitialized());
  EXPECT_FALSE(fake_audio_capture_module_->Playing());

  EXPECT_EQ(0, fake_audio_capture_module_->StartPlayout());
  EXPECT_TRUE(fake_audio_capture_module_->Playing());

  uint16_t delay_ms = 10;
  EXPECT_EQ(0, fake_audio_capture_module_->PlayoutDelay(&delay_ms));
  EXPECT_EQ(0, delay_ms);

  EXPECT_TRUE_WAIT(pull_iterations() > 0, kMsInSecond);
  EXPECT_GE(0, push_iterations());

  EXPECT_EQ(0, fake_audio_capture_module_->StopPlayout());
  EXPECT_FALSE(fake_audio_capture_module_->Playing());
}

TEST_F(FakeAdmTest, RecordTest) {
  EXPECT_EQ(0, fake_audio_capture_module_->RegisterAudioCallback(this));

  bool microphone_available = false;
  EXPECT_EQ(0, fake_audio_capture_module_->MicrophoneIsAvailable(
      &microphone_available));
  EXPECT_TRUE(microphone_available);

  bool stereo_available = false;
  EXPECT_EQ(0, fake_audio_capture_module_->StereoRecordingIsAvailable(
      &stereo_available));
  EXPECT_FALSE(stereo_available);

  EXPECT_NE(0, fake_audio_capture_module_->StartRecording());
  EXPECT_FALSE(fake_audio_capture_module_->Recording());
  EXPECT_EQ(0, fake_audio_capture_module_->StopRecording());

  EXPECT_EQ(0, fake_audio_capture_module_->InitRecording());
  EXPECT_EQ(0, fake_audio_capture_module_->StartRecording());
  EXPECT_TRUE(fake_audio_capture_module_->Recording());

  EXPECT_TRUE_WAIT(push_iterations() > 0, kMsInSecond);
  EXPECT_GE(0, pull_iterations());

  EXPECT_EQ(0, fake_audio_capture_module_->StopRecording());
  EXPECT_FALSE(fake_audio_capture_module_->Recording());
}

TEST_F(FakeAdmTest, DuplexTest) {
  EXPECT_EQ(0, fake_audio_capture_module_->RegisterAudioCallback(this));

  EXPECT_EQ(0, fake_audio_capture_module_->InitPlayout());
  EXPECT_EQ(0, fake_audio_capture_module_->StartPlayout());

  EXPECT_EQ(0, fake_audio_capture_module_->InitRecording());
  EXPECT_EQ(0, fake_audio_capture_module_->StartRecording());

  EXPECT_TRUE_WAIT(push_iterations() > 0, kMsInSecond);
  EXPECT_TRUE_WAIT(pull_iterations() > 0, kMsInSecond);

  EXPECT_EQ(0, fake_audio_capture_module_->StopPlayout());
  EXPECT_EQ(0, fake_audio_capture_module_->StopRecording());
}
