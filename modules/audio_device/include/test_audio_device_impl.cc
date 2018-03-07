/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_device/include/test_audio_device_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "common_audio/wav_file.h"
#include "modules/audio_device/include/audio_device_default.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/random.h"
#include "rtc_base/refcountedobject.h"
#include "system_wrappers/include/event_wrapper.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class EventTimerWrapper;

// todo(titovartem): use anonymous namespace here after downstream projects
// won't use test/FakeAudioDevice
namespace webrtc_impl {

constexpr int kFrameLengthMs = 10;
constexpr int kFramesPerSecond = 1000 / kFrameLengthMs;

// TestAudioDeviceModule implements an AudioDevice module that can act both as a
// capturer and a renderer. It will use 10ms audio frames.
TestAudioDeviceModuleImpl::TestAudioDeviceModuleImpl(
    std::unique_ptr<Capturer> capturer,
    std::unique_ptr<Renderer> renderer,
    float speed)
    : capturer_(std::move(capturer)),
      renderer_(std::move(renderer)),
      speed_(speed),
      audio_callback_(nullptr),
      rendering_(false),
      capturing_(false),
      done_rendering_(true, true),
      done_capturing_(true, true),
      tick_(EventTimerWrapper::Create()),
      thread_(TestAudioDeviceModuleImpl::Run,
              this,
              "TestAudioDeviceModuleImpl") {
  auto good_sample_rate = [](int sr) {
    return sr == 8000 || sr == 16000 || sr == 32000 || sr == 44100 ||
           sr == 48000;
  };

  if (renderer_) {
    const int sample_rate = renderer_->SamplingFrequency();
    playout_buffer_.resize(SamplesPerFrame(sample_rate), 0);
    RTC_CHECK(good_sample_rate(sample_rate));
  }
  if (capturer_) {
    RTC_CHECK(good_sample_rate(capturer_->SamplingFrequency()));
  }
}

TestAudioDeviceModuleImpl::~TestAudioDeviceModuleImpl() {
  StopPlayout();
  StopRecording();
  thread_.Stop();
}

int32_t TestAudioDeviceModuleImpl::Init() {
  RTC_CHECK(tick_->StartTimer(true, kFrameLengthMs / speed_));
  thread_.Start();
  thread_.SetPriority(rtc::kHighPriority);
  return 0;
}

int32_t TestAudioDeviceModuleImpl::RegisterAudioCallback(
    AudioTransport* callback) {
  rtc::CritScope cs(&lock_);
  RTC_DCHECK(callback || audio_callback_);
  audio_callback_ = callback;
  return 0;
}

int32_t TestAudioDeviceModuleImpl::StartPlayout() {
  rtc::CritScope cs(&lock_);
  RTC_CHECK(renderer_);
  rendering_ = true;
  done_rendering_.Reset();
  return 0;
}

int32_t TestAudioDeviceModuleImpl::StopPlayout() {
  rtc::CritScope cs(&lock_);
  rendering_ = false;
  done_rendering_.Set();
  return 0;
}

int32_t TestAudioDeviceModuleImpl::StartRecording() {
  rtc::CritScope cs(&lock_);
  RTC_CHECK(capturer_);
  capturing_ = true;
  done_capturing_.Reset();
  return 0;
}

int32_t TestAudioDeviceModuleImpl::StopRecording() {
  rtc::CritScope cs(&lock_);
  capturing_ = false;
  done_capturing_.Set();
  return 0;
}

bool TestAudioDeviceModuleImpl::Playing() const {
  rtc::CritScope cs(&lock_);
  return rendering_;
}

bool TestAudioDeviceModuleImpl::Recording() const {
  rtc::CritScope cs(&lock_);
  return capturing_;
}

// Blocks until the Renderer refuses to receive data.
// Returns false if |timeout_ms| passes before that happens.
bool TestAudioDeviceModuleImpl::WaitForPlayoutEnd(int timeout_ms) {
  return done_rendering_.Wait(timeout_ms);
}
// Blocks until the Recorder stops producing data.
// Returns false if |timeout_ms| passes before that happens.
bool TestAudioDeviceModuleImpl::WaitForRecordingEnd(int timeout_ms) {
  return done_capturing_.Wait(timeout_ms);
}

void TestAudioDeviceModuleImpl::ProcessAudio() {
  {
    rtc::CritScope cs(&lock_);
    if (capturing_) {
      // Capture 10ms of audio. 2 bytes per sample.
      const bool keep_capturing = capturer_->Capture(&recording_buffer_);
      uint32_t new_mic_level;
      if (recording_buffer_.size() > 0) {
        audio_callback_->RecordedDataIsAvailable(
            recording_buffer_.data(), recording_buffer_.size(), 2, 1,
            capturer_->SamplingFrequency(), 0, 0, 0, false, new_mic_level);
      }
      if (!keep_capturing) {
        capturing_ = false;
        done_capturing_.Set();
      }
    }
    if (rendering_) {
      size_t samples_out;
      int64_t elapsed_time_ms;
      int64_t ntp_time_ms;
      const int sampling_frequency = renderer_->SamplingFrequency();
      audio_callback_->NeedMorePlayData(
          SamplesPerFrame(sampling_frequency), 2, 1, sampling_frequency,
          playout_buffer_.data(), samples_out, &elapsed_time_ms, &ntp_time_ms);
      const bool keep_rendering = renderer_->Render(
          rtc::ArrayView<const int16_t>(playout_buffer_.data(), samples_out));
      if (!keep_rendering) {
        rendering_ = false;
        done_rendering_.Set();
      }
    }
  }
  tick_->Wait(WEBRTC_EVENT_INFINITE);
}

bool TestAudioDeviceModuleImpl::Run(void* obj) {
  static_cast<TestAudioDeviceModuleImpl*>(obj)->ProcessAudio();
  return true;
}

}  // namespace webrtc_impl

namespace {
// A fake capturer that generates pulses with random samples between
// -max_amplitude and +max_amplitude.
class PulsedNoiseCapturerImpl final
    : public TestAudioDeviceModule::PulsedNoiseCapturer {
 public:
  // Assuming 10ms audio packets.
  PulsedNoiseCapturerImpl(int16_t max_amplitude, int sampling_frequency_in_hz)
      : sampling_frequency_in_hz_(sampling_frequency_in_hz),
        fill_with_zero_(false),
        random_generator_(1),
        max_amplitude_(max_amplitude) {
    RTC_DCHECK_GT(max_amplitude, 0);
  }

  int SamplingFrequency() const override { return sampling_frequency_in_hz_; }

  bool Capture(rtc::BufferT<int16_t>* buffer) override {
    fill_with_zero_ = !fill_with_zero_;
    int16_t max_amplitude;
    {
      rtc::CritScope cs(&lock_);
      max_amplitude = max_amplitude_;
    }
    buffer->SetData(
        TestAudioDeviceModule::SamplesPerFrame(sampling_frequency_in_hz_),
        [&](rtc::ArrayView<int16_t> data) {
          if (fill_with_zero_) {
            std::fill(data.begin(), data.end(), 0);
          } else {
            std::generate(data.begin(), data.end(), [&]() {
              return random_generator_.Rand(-max_amplitude, max_amplitude);
            });
          }
          return data.size();
        });
    return true;
  }

  void SetMaxAmplitude(int16_t amplitude) override {
    rtc::CritScope cs(&lock_);
    max_amplitude_ = amplitude;
  }

 private:
  int sampling_frequency_in_hz_;
  bool fill_with_zero_;
  Random random_generator_;
  rtc::CriticalSection lock_;
  int16_t max_amplitude_ RTC_GUARDED_BY(lock_);
};

class WavFileReader final : public TestAudioDeviceModule::Capturer {
 public:
  WavFileReader(std::string filename, int sampling_frequency_in_hz)
      : sampling_frequency_in_hz_(sampling_frequency_in_hz),
        wav_reader_(filename) {
    RTC_CHECK_EQ(wav_reader_.sample_rate(), sampling_frequency_in_hz);
    RTC_CHECK_EQ(wav_reader_.num_channels(), 1);
  }

  int SamplingFrequency() const override { return sampling_frequency_in_hz_; }

  bool Capture(rtc::BufferT<int16_t>* buffer) override {
    buffer->SetData(
        TestAudioDeviceModule::SamplesPerFrame(sampling_frequency_in_hz_),
        [&](rtc::ArrayView<int16_t> data) {
          return wav_reader_.ReadSamples(data.size(), data.data());
        });
    return buffer->size() > 0;
  }

 private:
  int sampling_frequency_in_hz_;
  WavReader wav_reader_;
};

class WavFileWriter final : public TestAudioDeviceModule::Renderer {
 public:
  WavFileWriter(std::string filename, int sampling_frequency_in_hz)
      : sampling_frequency_in_hz_(sampling_frequency_in_hz),
        wav_writer_(filename, sampling_frequency_in_hz, 1) {}

  int SamplingFrequency() const override { return sampling_frequency_in_hz_; }

  bool Render(rtc::ArrayView<const int16_t> data) override {
    wav_writer_.WriteSamples(data.data(), data.size());
    return true;
  }

 private:
  int sampling_frequency_in_hz_;
  WavWriter wav_writer_;
};

class BoundedWavFileWriter : public TestAudioDeviceModule::Renderer {
 public:
  BoundedWavFileWriter(std::string filename, int sampling_frequency_in_hz)
      : sampling_frequency_in_hz_(sampling_frequency_in_hz),
        wav_writer_(filename, sampling_frequency_in_hz, 1),
        silent_audio_(
            TestAudioDeviceModule::SamplesPerFrame(sampling_frequency_in_hz),
            0),
        started_writing_(false),
        trailing_zeros_(0) {}

  int SamplingFrequency() const override { return sampling_frequency_in_hz_; }

  bool Render(rtc::ArrayView<const int16_t> data) override {
    const int16_t kAmplitudeThreshold = 5;

    const int16_t* begin = data.begin();
    const int16_t* end = data.end();
    if (!started_writing_) {
      // Cut off silence at the beginning.
      while (begin < end) {
        if (std::abs(*begin) > kAmplitudeThreshold) {
          started_writing_ = true;
          break;
        }
        ++begin;
      }
    }
    if (started_writing_) {
      // Cut off silence at the end.
      while (begin < end) {
        if (*(end - 1) != 0) {
          break;
        }
        --end;
      }
      if (begin < end) {
        // If it turns out that the silence was not final, need to write all the
        // skipped zeros and continue writing audio.
        while (trailing_zeros_ > 0) {
          const size_t zeros_to_write =
              std::min(trailing_zeros_, silent_audio_.size());
          wav_writer_.WriteSamples(silent_audio_.data(), zeros_to_write);
          trailing_zeros_ -= zeros_to_write;
        }
        wav_writer_.WriteSamples(begin, end - begin);
      }
      // Save the number of zeros we skipped in case this needs to be restored.
      trailing_zeros_ += data.end() - end;
    }
    return true;
  }

 private:
  int sampling_frequency_in_hz_;
  WavWriter wav_writer_;
  std::vector<int16_t> silent_audio_;
  bool started_writing_;
  size_t trailing_zeros_;
};

class DiscardRenderer final : public TestAudioDeviceModule::Renderer {
 public:
  explicit DiscardRenderer(int sampling_frequency_in_hz)
      : sampling_frequency_in_hz_(sampling_frequency_in_hz) {}

  int SamplingFrequency() const override { return sampling_frequency_in_hz_; }

  bool Render(rtc::ArrayView<const int16_t> data) override { return true; }

 private:
  int sampling_frequency_in_hz_;
};

}  // namespace

size_t TestAudioDeviceModule::SamplesPerFrame(int sampling_frequency_in_hz) {
  return rtc::CheckedDivExact(sampling_frequency_in_hz,
                              webrtc_impl::kFramesPerSecond);
}

rtc::scoped_refptr<TestAudioDeviceModule>
TestAudioDeviceModule::CreateTestAudioDeviceModule(
    std::unique_ptr<Capturer> capturer,
    std::unique_ptr<Renderer> renderer,
    float speed) {
  return new rtc::RefCountedObject<webrtc_impl::TestAudioDeviceModuleImpl>(
      std::move(capturer), std::move(renderer), speed);
}

std::unique_ptr<TestAudioDeviceModule::PulsedNoiseCapturer>
TestAudioDeviceModule::CreatePulsedNoiseCapturer(int16_t max_amplitude,
                                                 int sampling_frequency_in_hz) {
  return std::unique_ptr<TestAudioDeviceModule::PulsedNoiseCapturer>(
      new PulsedNoiseCapturerImpl(max_amplitude, sampling_frequency_in_hz));
}

std::unique_ptr<TestAudioDeviceModule::Capturer>
TestAudioDeviceModule::CreateWavFileReader(std::string filename,
                                           int sampling_frequency_in_hz) {
  return std::unique_ptr<TestAudioDeviceModule::Capturer>(
      new WavFileReader(filename, sampling_frequency_in_hz));
}

std::unique_ptr<TestAudioDeviceModule::Capturer>
TestAudioDeviceModule::CreateWavFileReader(std::string filename) {
  int sampling_frequency_in_hz = WavReader(filename).sample_rate();
  return std::unique_ptr<TestAudioDeviceModule::Capturer>(
      new WavFileReader(filename, sampling_frequency_in_hz));
}

std::unique_ptr<TestAudioDeviceModule::Renderer>
TestAudioDeviceModule::CreateWavFileWriter(std::string filename,
                                           int sampling_frequency_in_hz) {
  return std::unique_ptr<TestAudioDeviceModule::Renderer>(
      new WavFileWriter(filename, sampling_frequency_in_hz));
}

std::unique_ptr<TestAudioDeviceModule::Renderer>
TestAudioDeviceModule::CreateBoundedWavFileWriter(
    std::string filename,
    int sampling_frequency_in_hz) {
  return std::unique_ptr<TestAudioDeviceModule::Renderer>(
      new BoundedWavFileWriter(filename, sampling_frequency_in_hz));
}

std::unique_ptr<TestAudioDeviceModule::Renderer>
TestAudioDeviceModule::CreateDiscardRenderer(int sampling_frequency_in_hz) {
  return std::unique_ptr<TestAudioDeviceModule::Renderer>(
      new DiscardRenderer(sampling_frequency_in_hz));
}

}  // namespace webrtc
