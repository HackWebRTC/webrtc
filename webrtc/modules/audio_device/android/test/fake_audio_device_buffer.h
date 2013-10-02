/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_FAKE_AUDIO_DEVICE_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_FAKE_AUDIO_DEVICE_BUFFER_H_

#include "webrtc/modules/audio_device/android/audio_manager_jni.h"
#include "webrtc/modules/audio_device/android/single_rw_fifo.h"
#include "webrtc/modules/audio_device/audio_device_buffer.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

// Fake AudioDeviceBuffer implementation that returns audio data that is pushed
// to it. It implements all APIs used by the OpenSL implementation.
class FakeAudioDeviceBuffer : public AudioDeviceBuffer {
 public:
  FakeAudioDeviceBuffer();
  virtual ~FakeAudioDeviceBuffer() {}

  virtual int32_t SetRecordingSampleRate(uint32_t fsHz);
  virtual int32_t SetPlayoutSampleRate(uint32_t fsHz);
  virtual int32_t SetRecordingChannels(uint8_t channels);
  virtual int32_t SetPlayoutChannels(uint8_t channels);
  virtual int32_t SetRecordedBuffer(const void* audioBuffer,
                                    uint32_t nSamples);
  virtual void SetVQEData(int playDelayMS,
                          int recDelayMS,
                          int clockDrift) {}
  virtual int32_t DeliverRecordedData() { return 0; }
  virtual int32_t RequestPlayoutData(uint32_t nSamples);
  virtual int32_t GetPlayoutData(void* audioBuffer);

  void ClearBuffer();

 private:
  enum {
    // Each buffer contains 10 ms of data since that is what OpenSlesInput
    // delivers. Keep 7 buffers which would cover 70 ms of data. These buffers
    // are needed because of jitter between OpenSl recording and playing.
    kNumBuffers = 7,
  };
  int sample_rate() const;
  int buffer_size_samples() const;
  int buffer_size_bytes() const;

  // Java API handle
  AudioManagerJni audio_manager_;

  SingleRwFifo fifo_;
  scoped_array<scoped_array<int8_t> > buf_;
  int next_available_buffer_;

  uint8_t record_channels_;
  uint8_t play_channels_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_FAKE_AUDIO_DEVICE_BUFFER_H_
