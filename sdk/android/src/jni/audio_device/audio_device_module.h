/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AUDIO_DEVICE_MODULE_H_
#define SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AUDIO_DEVICE_MODULE_H_

#include <memory>

#include "api/optional.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"
#include "sdk/android/src/jni/audio_device/audio_manager.h"

namespace webrtc {

namespace android_adm {

class AudioManager;

class AudioInput {
 public:
  virtual ~AudioInput() {}

  virtual int32_t Init() = 0;
  virtual int32_t Terminate() = 0;

  virtual int32_t InitRecording() = 0;
  virtual bool RecordingIsInitialized() const = 0;

  virtual int32_t StartRecording() = 0;
  virtual int32_t StopRecording() = 0;
  virtual bool Recording() const = 0;

  virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) = 0;

  virtual int32_t EnableBuiltInAEC(bool enable) = 0;
  virtual int32_t EnableBuiltInAGC(bool enable) = 0;
  virtual int32_t EnableBuiltInNS(bool enable) = 0;
};

class AudioOutput {
 public:
  virtual ~AudioOutput() {}

  virtual int32_t Init() = 0;
  virtual int32_t Terminate() = 0;
  virtual int32_t InitPlayout() = 0;
  virtual bool PlayoutIsInitialized() const = 0;
  virtual int32_t StartPlayout() = 0;
  virtual int32_t StopPlayout() = 0;
  virtual bool Playing() const = 0;
  virtual bool SpeakerVolumeIsAvailable() = 0;
  virtual int SetSpeakerVolume(uint32_t volume) = 0;
  virtual rtc::Optional<uint32_t> SpeakerVolume() const = 0;
  virtual rtc::Optional<uint32_t> MaxSpeakerVolume() const = 0;
  virtual rtc::Optional<uint32_t> MinSpeakerVolume() const = 0;
  virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) = 0;
};

rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceModuleFromInputAndOutput(
    AudioDeviceModule::AudioLayer audio_layer,
    std::unique_ptr<AudioManager> audio_manager,
    std::unique_ptr<AudioInput> audio_input,
    std::unique_ptr<AudioOutput> audio_output);

}  // namespace android_adm

}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AUDIO_DEVICE_MODULE_H_
