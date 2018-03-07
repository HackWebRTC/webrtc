/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_AUDIO_DEVICE_INCLUDE_TEST_AUDIO_DEVICE_IMPL_H_
#define MODULES_AUDIO_DEVICE_INCLUDE_TEST_AUDIO_DEVICE_IMPL_H_

#include <memory>
#include <utility>
#include <vector>

#include "modules/audio_device/include/audio_device_default.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "rtc_base/buffer.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"

namespace webrtc {

class EventTimerWrapper;

namespace webrtc_impl {

// TestAudioDeviceModule implements an AudioDevice module that can act both as a
// capturer and a renderer. It will use 10ms audio frames.
// todo(titovartem): hide implementation after downstream projects won't use
// test/FakeAudioDevice
class TestAudioDeviceModuleImpl
    : public AudioDeviceModuleDefault<TestAudioDeviceModule> {
 public:
  // Creates a new TestAudioDeviceModule. When capturing or playing, 10 ms audio
  // frames will be processed every 10ms / |speed|.
  // |capturer| is an object that produces audio data. Can be nullptr if this
  // device is never used for recording.
  // |renderer| is an object that receives audio data that would have been
  // played out. Can be nullptr if this device is never used for playing.
  // Use one of the Create... functions to get these instances.
  TestAudioDeviceModuleImpl(std::unique_ptr<Capturer> capturer,
                            std::unique_ptr<Renderer> renderer,
                            float speed = 1);

  ~TestAudioDeviceModuleImpl() override;

  int32_t Init() override;
  int32_t RegisterAudioCallback(AudioTransport* callback) override;
  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Playing() const override;
  bool Recording() const override;

  // Blocks until the Renderer refuses to receive data.
  // Returns false if |timeout_ms| passes before that happens.
  bool WaitForPlayoutEnd(int timeout_ms = rtc::Event::kForever) override;
  // Blocks until the Recorder stops producing data.
  // Returns false if |timeout_ms| passes before that happens.
  bool WaitForRecordingEnd(int timeout_ms = rtc::Event::kForever) override;

 private:
  void ProcessAudio();
  static bool Run(void* obj);

  const std::unique_ptr<Capturer> capturer_ RTC_GUARDED_BY(lock_);
  const std::unique_ptr<Renderer> renderer_ RTC_GUARDED_BY(lock_);
  const float speed_;

  rtc::CriticalSection lock_;
  AudioTransport* audio_callback_ RTC_GUARDED_BY(lock_);
  bool rendering_ RTC_GUARDED_BY(lock_);
  bool capturing_ RTC_GUARDED_BY(lock_);
  rtc::Event done_rendering_;
  rtc::Event done_capturing_;

  std::vector<int16_t> playout_buffer_ RTC_GUARDED_BY(lock_);
  rtc::BufferT<int16_t> recording_buffer_ RTC_GUARDED_BY(lock_);

  std::unique_ptr<EventTimerWrapper> tick_;
  rtc::PlatformThread thread_;
};

}  // namespace webrtc_impl
}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_INCLUDE_TEST_AUDIO_DEVICE_IMPL_H_
