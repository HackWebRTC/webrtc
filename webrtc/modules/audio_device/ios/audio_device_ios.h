/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_IOS_AUDIO_DEVICE_IOS_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_IOS_AUDIO_DEVICE_IOS_H_

#include <AudioUnit/AudioUnit.h>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_device/audio_device_generic.h"

namespace webrtc {

class FineAudioBuffer;

// Implements full duplex 16-bit mono PCM audio support for iOS using a
// Voice-Processing (VP) I/O audio unit in Core Audio. The VP I/O audio unit
// supports audio echo cancellation. It also adds automatic gain control,
// adjustment of voice-processing quality and muting.
//
// An instance must be created and destroyed on one and the same thread.
// All supported public methods must also be called on the same thread.
// A thread checker will DCHECK if any supported method is called on an invalid
// thread.
//
// Recorded audio will be delivered on a real-time internal I/O thread in the
// audio unit. The audio unit will also ask for audio data to play out on this
// same thread.
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

  // These methods returns hard-coded delay values and not dynamic delay
  // estimates. The reason is that iOS supports a built-in AEC and the WebRTC
  // AEC will always be disabled in the Libjingle layer to avoid running two
  // AEC implementations at the same time. And, it saves resources to avoid
  // updating these delay values continuously.
  // TODO(henrika): it would be possible to mark these two methods as not
  // implemented since they are only called for A/V-sync purposes today and
  // A/V-sync is not supported on iOS. However, we avoid adding error messages
  // the log by using these dummy implementations instead.
  int32_t PlayoutDelay(uint16_t& delayMS) const override;
  int32_t RecordingDelay(uint16_t& delayMS) const override;

  // Native audio parameters stored during construction.
  // These methods are unique for the iOS implementation.
  int GetPlayoutAudioParameters(AudioParameters* params) const override;
  int GetRecordAudioParameters(AudioParameters* params) const override;

  // These methods are currently not fully implemented on iOS:

  // See audio_device_not_implemented.cc for trivial implementations.
  int32_t PlayoutBuffer(AudioDeviceModule::BufferType& type,
                        uint16_t& sizeMS) const override;
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
  // Uses current |_playoutParameters| and |_recordParameters| to inform the
  // audio device buffer (ADB) about our internal audio parameters.
  void UpdateAudioDeviceBuffer();

  // Since the preferred audio parameters are only hints to the OS, the actual
  // values may be different once the AVAudioSession has been activated.
  // This method asks for the current hardware parameters and takes actions
  // if they should differ from what we have asked for initially. It also
  // defines |_playoutParameters| and |_recordParameters|.
  void SetupAudioBuffersForActiveAudioSession();

  // Creates a Voice-Processing I/O unit and configures it for full-duplex
  // audio. The selected stream format is selected to avoid internal resampling
  // and to match the 10ms callback rate for WebRTC as well as possible.
  // This method also initializes the created audio unit.
  bool SetupAndInitializeVoiceProcessingAudioUnit();

  // Activates our audio session, creates and initilizes the voice-processing
  // audio unit and verifies that we got the preferred native audio parameters.
  bool InitPlayOrRecord();

  // Closes and deletes the voice-processing I/O unit.
  bool ShutdownPlayOrRecord();

  // Callback function called on a real-time priority I/O thread from the audio
  // unit. This method is used to signal that recorded audio is available.
  static OSStatus RecordedDataIsAvailable(
      void* inRefCon,
      AudioUnitRenderActionFlags* ioActionFlags,
      const AudioTimeStamp* timeStamp,
      UInt32 inBusNumber,
      UInt32 inNumberFrames,
      AudioBufferList* ioData);
  OSStatus OnRecordedDataIsAvailable(AudioUnitRenderActionFlags* ioActionFlags,
                                     const AudioTimeStamp* timeStamp,
                                     UInt32 inBusNumber,
                                     UInt32 inNumberFrames);

  // Callback function called on a real-time priority I/O thread from the audio
  // unit. This method is used to provide audio samples to the audio unit.
  static OSStatus GetPlayoutData(void* inRefCon,
                                 AudioUnitRenderActionFlags* ioActionFlags,
                                 const AudioTimeStamp* timeStamp,
                                 UInt32 inBusNumber,
                                 UInt32 inNumberFrames,
                                 AudioBufferList* ioData);
  OSStatus OnGetPlayoutData(AudioUnitRenderActionFlags* ioActionFlags,
                            UInt32 inNumberFrames,
                            AudioBufferList* ioData);

 private:
  // Ensures that methods are called from the same thread as this object is
  // created on.
  rtc::ThreadChecker _threadChecker;

  // Raw pointer handle provided to us in AttachAudioBuffer(). Owned by the
  // AudioDeviceModuleImpl class and called by AudioDeviceModuleImpl::Create().
  // The AudioDeviceBuffer is a member of the AudioDeviceModuleImpl instance
  // and therefore outlives this object.
  AudioDeviceBuffer* _audioDeviceBuffer;

  // Contains audio parameters (sample rate, #channels, buffer size etc.) for
  // the playout and recording sides. These structure is set in two steps:
  // first, native sample rate and #channels are defined in Init(). Next, the
  // audio session is activated and we verify that the preferred parameters
  // were granted by the OS. At this stage it is also possible to add a third
  // component to the parameters; the native I/O buffer duration.
  // A CHECK will be hit if we for some reason fail to open an audio session
  // using the specified parameters.
  AudioParameters _playoutParameters;
  AudioParameters _recordParameters;

  // The Voice-Processing I/O unit has the same characteristics as the
  // Remote I/O unit (supports full duplex low-latency audio input and output)
  // and adds AEC for for two-way duplex communication. It also adds AGC,
  // adjustment of voice-processing quality, and muting. Hence, ideal for
  // VoIP applications.
  AudioUnit _vpioUnit;

  // FineAudioBuffer takes an AudioDeviceBuffer which delivers audio data
  // in chunks of 10ms. It then allows for this data to be pulled in
  // a finer or coarser granularity. I.e. interacting with this class instead
  // of directly with the AudioDeviceBuffer one can ask for any number of
  // audio data samples. Is also supports a similar scheme for the recording
  // side.
  // Example: native buffer size can be 128 audio frames at 16kHz sample rate.
  // WebRTC will provide 480 audio frames per 10ms but iOS asks for 128
  // in each callback (one every 8ms). This class can then ask for 128 and the
  // FineAudioBuffer will ask WebRTC for new data only when needed and also
  // cache non-utilized audio between callbacks. On the recording side, iOS
  // can provide audio data frames of size 128 and these are accumulated until
  // enough data to supply one 10ms call exists. This 10ms chunk is then sent
  // to WebRTC and the remaining part is stored.
  rtc::scoped_ptr<FineAudioBuffer> _fineAudioBuffer;

  // Extra audio buffer to be used by the playout side for rendering audio.
  // The buffer size is given by FineAudioBuffer::RequiredBufferSizeBytes().
  rtc::scoped_ptr<SInt8[]> _playoutAudioBuffer;

  // Provides a mechanism for encapsulating one or more buffers of audio data.
  // Only used on the recording side.
  AudioBufferList _audioRecordBufferList;

  // Temporary storage for recorded data. AudioUnitRender() renders into this
  // array as soon as a frame of the desired buffer size has been recorded.
  rtc::scoped_ptr<SInt8[]> _recordAudioBuffer;

  // Set to 1 when recording is active and 0 otherwise.
  volatile int _recording;

  // Set to 1 when playout is active and 0 otherwise.
  volatile int _playing;

  // Set to true after successful call to Init(), false otherwise.
  bool _initialized;

  // Set to true after successful call to InitRecording(), false otherwise.
  bool _recIsInitialized;

  // Set to true after successful call to InitPlayout(), false otherwise.
  bool _playIsInitialized;

  // Audio interruption observer instance.
  void* _audioInterruptionObserver;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_IOS_AUDIO_DEVICE_IOS_H_
