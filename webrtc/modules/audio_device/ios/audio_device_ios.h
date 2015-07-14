/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_IOS_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_IOS_H

#include <AudioUnit/AudioUnit.h>

#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"

namespace webrtc {
const uint32_t N_REC_SAMPLES_PER_SEC = 44100;
const uint32_t N_PLAY_SAMPLES_PER_SEC = 44100;

const uint32_t ENGINE_REC_BUF_SIZE_IN_SAMPLES = (N_REC_SAMPLES_PER_SEC / 100);
const uint32_t ENGINE_PLAY_BUF_SIZE_IN_SAMPLES = (N_PLAY_SAMPLES_PER_SEC / 100);

// Number of 10 ms recording blocks in recording buffer
const uint16_t N_REC_BUFFERS = 20;

class AudioDeviceIOS : public AudioDeviceGeneric {
 public:
  AudioDeviceIOS();
  ~AudioDeviceIOS();

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override;

  int32_t Init() override;
  int32_t Terminate() override;
  bool Initialized() const override { return _initialized; }

  int32_t InitPlayout() override;
  bool PlayoutIsInitialized() const override { return _playIsInitialized; }

  int32_t InitRecording() override;
  bool RecordingIsInitialized() const override { return _recIsInitialized; }

  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override { return _playing; }

  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Recording() const override { return _recording; }

  int32_t SetLoudspeakerStatus(bool enable) override;
  int32_t GetLoudspeakerStatus(bool& enabled) const override;

  // TODO(henrika): investigate if we can reduce the complexity here.
  // Do we even need delay estimates?
  int32_t PlayoutDelay(uint16_t& delayMS) const override;
  int32_t RecordingDelay(uint16_t& delayMS) const override;

  int32_t PlayoutBuffer(AudioDeviceModule::BufferType& type,
                        uint16_t& sizeMS) const override;

  // These methods are unique for the iOS implementation.

  // Native audio parameters stored during construction.
  int GetPlayoutAudioParameters(AudioParameters* params) const override;
  int GetRecordAudioParameters(AudioParameters* params) const override;

  // These methods are currently not implemented on iOS.
  // See audio_device_not_implemented_ios.mm for dummy implementations.

  int32_t ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const;
  int32_t ResetAudioDevice() override;
  int32_t PlayoutIsAvailable(bool& available) override;
  int32_t RecordingIsAvailable(bool& available) override;
  int32_t SetAGC(bool enable) override;
  bool AGC() const override;
  int16_t PlayoutDevices() override;
  int16_t RecordingDevices() override;
  int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]) override;
  int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) override;
  int32_t SetPlayoutDevice(uint16_t index) override;
  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) override;
  int32_t SetRecordingDevice(uint16_t index) override;
  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) override;
  int32_t SetWaveOutVolume(uint16_t volumeLeft, uint16_t volumeRight) override;
  int32_t WaveOutVolume(uint16_t& volumeLeft,
                        uint16_t& volumeRight) const override;
  int32_t InitSpeaker() override;
  bool SpeakerIsInitialized() const override;
  int32_t InitMicrophone() override;
  bool MicrophoneIsInitialized() const override;
  int32_t SpeakerVolumeIsAvailable(bool& available) override;
  int32_t SetSpeakerVolume(uint32_t volume) override;
  int32_t SpeakerVolume(uint32_t& volume) const override;
  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const override;
  int32_t MinSpeakerVolume(uint32_t& minVolume) const override;
  int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const override;
  int32_t MicrophoneVolumeIsAvailable(bool& available) override;
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t& volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const override;
  int32_t MinMicrophoneVolume(uint32_t& minVolume) const override;
  int32_t MicrophoneVolumeStepSize(uint16_t& stepSize) const override;
  int32_t MicrophoneMuteIsAvailable(bool& available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool& enabled) const override;
  int32_t SpeakerMuteIsAvailable(bool& available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool& enabled) const override;
  int32_t MicrophoneBoostIsAvailable(bool& available) override;
  int32_t SetMicrophoneBoost(bool enable) override;
  int32_t MicrophoneBoost(bool& enabled) const override;
  int32_t StereoPlayoutIsAvailable(bool& available) override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoPlayout(bool& enabled) const override;
  int32_t StereoRecordingIsAvailable(bool& available) override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool& enabled) const override;
  int32_t SetPlayoutBuffer(const AudioDeviceModule::BufferType type,
                           uint16_t sizeMS) override;
  int32_t CPULoad(uint16_t& load) const override;
  bool PlayoutWarning() const override;
  bool PlayoutError() const override;
  bool RecordingWarning() const override;
  bool RecordingError() const override;
  void ClearPlayoutWarning() override{};
  void ClearPlayoutError() override{};
  void ClearRecordingWarning() override{};
  void ClearRecordingError() override{};

 private:
  // TODO(henrika): try to remove these.
  void Lock() {
    _critSect.Enter();
  }

  void UnLock() {
    _critSect.Leave();
  }

  // Init and shutdown
  int32_t InitPlayOrRecord();
  int32_t ShutdownPlayOrRecord();

  void UpdateRecordingDelay();
  void UpdatePlayoutDelay();

  static OSStatus RecordProcess(void *inRefCon,
                                AudioUnitRenderActionFlags *ioActionFlags,
                                const AudioTimeStamp *timeStamp,
                                UInt32 inBusNumber,
                                UInt32 inNumberFrames,
                                AudioBufferList *ioData);

  static OSStatus PlayoutProcess(void *inRefCon,
                                 AudioUnitRenderActionFlags *ioActionFlags,
                                 const AudioTimeStamp *timeStamp,
                                 UInt32 inBusNumber,
                                 UInt32 inNumberFrames,
                                 AudioBufferList *ioData);

  OSStatus RecordProcessImpl(AudioUnitRenderActionFlags *ioActionFlags,
                             const AudioTimeStamp *timeStamp,
                             uint32_t inBusNumber,
                             uint32_t inNumberFrames);

  OSStatus PlayoutProcessImpl(uint32_t inNumberFrames,
                              AudioBufferList *ioData);

  static bool RunCapture(void* ptrThis);
  bool CaptureWorkerThread();

 private:
  rtc::ThreadChecker thread_checker_;

  // Raw pointer handle provided to us in AttachAudioBuffer(). Owned by the
  // AudioDeviceModuleImpl class and called by AudioDeviceModuleImpl::Create().
  // The AudioDeviceBuffer is a member of the AudioDeviceModuleImpl instance
  // and therefore outlives this object.
  AudioDeviceBuffer* audio_device_buffer_;

  CriticalSectionWrapper& _critSect;

  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;

  rtc::scoped_ptr<ThreadWrapper> _captureWorkerThread;

  AudioUnit _auVoiceProcessing;
  void* _audioInterruptionObserver;

  bool _initialized;
  bool _isShutDown;
  bool _recording;
  bool _playing;
  bool _recIsInitialized;
  bool _playIsInitialized;

  // The sampling rate to use with Audio Device Buffer
  int _adbSampFreq;

  // Delay calculation
  uint32_t _recordingDelay;
  uint32_t _playoutDelay;
  uint32_t _playoutDelayMeasurementCounter;
  uint32_t _recordingDelayHWAndOS;
  uint32_t _recordingDelayMeasurementCounter;

  // Playout buffer, needed for 44.0 / 44.1 kHz mismatch
  int16_t _playoutBuffer[ENGINE_PLAY_BUF_SIZE_IN_SAMPLES];
  uint32_t  _playoutBufferUsed;  // How much is filled

  // Recording buffers
  int16_t _recordingBuffer[N_REC_BUFFERS][ENGINE_REC_BUF_SIZE_IN_SAMPLES];
  uint32_t _recordingLength[N_REC_BUFFERS];
  uint32_t _recordingSeqNumber[N_REC_BUFFERS];
  uint32_t _recordingCurrentSeq;

  // Current total size all data in buffers, used for delay estimate
  uint32_t _recordingBufferTotalSize;
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_IOS_H
