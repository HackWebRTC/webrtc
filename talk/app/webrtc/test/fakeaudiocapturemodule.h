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

// This class implements an AudioCaptureModule that can be used to detect if
// audio is being received properly if it is fed by another AudioCaptureModule
// in some arbitrary audio pipeline where they are connected. It does not play
// out or record any audio so it does not need access to any hardware and can
// therefore be used in the gtest testing framework.

// Note P postfix of a function indicates that it should only be called by the
// processing thread.

#ifndef TALK_APP_WEBRTC_TEST_FAKEAUDIOCAPTUREMODULE_H_
#define TALK_APP_WEBRTC_TEST_FAKEAUDIOCAPTUREMODULE_H_

#include "talk/base/basictypes.h"
#include "talk/base/criticalsection.h"
#include "talk/base/messagehandler.h"
#include "talk/base/scoped_ref_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_device/include/audio_device.h"

namespace talk_base {

class Thread;

}  // namespace talk_base

class FakeAudioCaptureModule
    : public webrtc::AudioDeviceModule,
      public talk_base::MessageHandler {
 public:
  typedef uint16 Sample;

  // The value for the following constants have been derived by running VoE
  // using a real ADM. The constants correspond to 10ms of mono audio at 44kHz.
  enum{kNumberSamples = 440};
  enum{kNumberBytesPerSample = sizeof(Sample)};

  // Creates a FakeAudioCaptureModule or returns NULL on failure.
  // |process_thread| is used to push and pull audio frames to and from the
  // returned instance. Note: ownership of |process_thread| is not handed over.
  static talk_base::scoped_refptr<FakeAudioCaptureModule> Create(
      talk_base::Thread* process_thread);

  // Returns the number of frames that have been successfully pulled by the
  // instance. Note that correctly detecting success can only be done if the
  // pulled frame was generated/pushed from a FakeAudioCaptureModule.
  int frames_received() const;

  // Following functions are inherited from webrtc::AudioDeviceModule.
  // Only functions called by PeerConnection are implemented, the rest do
  // nothing and return success. If a function is not expected to be called by
  // PeerConnection an assertion is triggered if it is in fact called.
  virtual int32_t Version(char* version,
                          uint32_t& remaining_buffer_in_bytes,
                          uint32_t& position) const;
  virtual int32_t TimeUntilNextProcess();
  virtual int32_t Process();
  virtual int32_t ChangeUniqueId(const int32_t id);

  virtual int32_t ActiveAudioLayer(AudioLayer* audio_layer) const;

  virtual ErrorCode LastError() const;
  virtual int32_t RegisterEventObserver(
      webrtc::AudioDeviceObserver* event_callback);

  // Note: Calling this method from a callback may result in deadlock.
  virtual int32_t RegisterAudioCallback(webrtc::AudioTransport* audio_callback);

  virtual int32_t Init();
  virtual int32_t Terminate();
  virtual bool Initialized() const;

  virtual int16_t PlayoutDevices();
  virtual int16_t RecordingDevices();
  virtual int32_t PlayoutDeviceName(uint16_t index,
                                    char name[webrtc::kAdmMaxDeviceNameSize],
                                    char guid[webrtc::kAdmMaxGuidSize]);
  virtual int32_t RecordingDeviceName(uint16_t index,
                                      char name[webrtc::kAdmMaxDeviceNameSize],
                                      char guid[webrtc::kAdmMaxGuidSize]);

  virtual int32_t SetPlayoutDevice(uint16_t index);
  virtual int32_t SetPlayoutDevice(WindowsDeviceType device);
  virtual int32_t SetRecordingDevice(uint16_t index);
  virtual int32_t SetRecordingDevice(WindowsDeviceType device);

  virtual int32_t PlayoutIsAvailable(bool* available);
  virtual int32_t InitPlayout();
  virtual bool PlayoutIsInitialized() const;
  virtual int32_t RecordingIsAvailable(bool* available);
  virtual int32_t InitRecording();
  virtual bool RecordingIsInitialized() const;

  virtual int32_t StartPlayout();
  virtual int32_t StopPlayout();
  virtual bool Playing() const;
  virtual int32_t StartRecording();
  virtual int32_t StopRecording();
  virtual bool Recording() const;

  virtual int32_t SetAGC(bool enable);
  virtual bool AGC() const;

  virtual int32_t SetWaveOutVolume(uint16_t volume_left,
                                   uint16_t volume_right);
  virtual int32_t WaveOutVolume(uint16_t* volume_left,
                                uint16_t* volume_right) const;

  virtual int32_t SpeakerIsAvailable(bool* available);
  virtual int32_t InitSpeaker();
  virtual bool SpeakerIsInitialized() const;
  virtual int32_t MicrophoneIsAvailable(bool* available);
  virtual int32_t InitMicrophone();
  virtual bool MicrophoneIsInitialized() const;

  virtual int32_t SpeakerVolumeIsAvailable(bool* available);
  virtual int32_t SetSpeakerVolume(uint32_t volume);
  virtual int32_t SpeakerVolume(uint32_t* volume) const;
  virtual int32_t MaxSpeakerVolume(uint32_t* max_volume) const;
  virtual int32_t MinSpeakerVolume(uint32_t* min_volume) const;
  virtual int32_t SpeakerVolumeStepSize(uint16_t* step_size) const;

  virtual int32_t MicrophoneVolumeIsAvailable(bool* available);
  virtual int32_t SetMicrophoneVolume(uint32_t volume);
  virtual int32_t MicrophoneVolume(uint32_t* volume) const;
  virtual int32_t MaxMicrophoneVolume(uint32_t* max_volume) const;

  virtual int32_t MinMicrophoneVolume(uint32_t* min_volume) const;
  virtual int32_t MicrophoneVolumeStepSize(uint16_t* step_size) const;

  virtual int32_t SpeakerMuteIsAvailable(bool* available);
  virtual int32_t SetSpeakerMute(bool enable);
  virtual int32_t SpeakerMute(bool* enabled) const;

  virtual int32_t MicrophoneMuteIsAvailable(bool* available);
  virtual int32_t SetMicrophoneMute(bool enable);
  virtual int32_t MicrophoneMute(bool* enabled) const;

  virtual int32_t MicrophoneBoostIsAvailable(bool* available);
  virtual int32_t SetMicrophoneBoost(bool enable);
  virtual int32_t MicrophoneBoost(bool* enabled) const;

  virtual int32_t StereoPlayoutIsAvailable(bool* available) const;
  virtual int32_t SetStereoPlayout(bool enable);
  virtual int32_t StereoPlayout(bool* enabled) const;
  virtual int32_t StereoRecordingIsAvailable(bool* available) const;
  virtual int32_t SetStereoRecording(bool enable);
  virtual int32_t StereoRecording(bool* enabled) const;
  virtual int32_t SetRecordingChannel(const ChannelType channel);
  virtual int32_t RecordingChannel(ChannelType* channel) const;

  virtual int32_t SetPlayoutBuffer(const BufferType type,
                                   uint16_t size_ms = 0);
  virtual int32_t PlayoutBuffer(BufferType* type,
                                uint16_t* size_ms) const;
  virtual int32_t PlayoutDelay(uint16_t* delay_ms) const;
  virtual int32_t RecordingDelay(uint16_t* delay_ms) const;

  virtual int32_t CPULoad(uint16_t* load) const;

  virtual int32_t StartRawOutputFileRecording(
      const char pcm_file_name_utf8[webrtc::kAdmMaxFileNameSize]);
  virtual int32_t StopRawOutputFileRecording();
  virtual int32_t StartRawInputFileRecording(
      const char pcm_file_name_utf8[webrtc::kAdmMaxFileNameSize]);
  virtual int32_t StopRawInputFileRecording();

  virtual int32_t SetRecordingSampleRate(const uint32_t samples_per_sec);
  virtual int32_t RecordingSampleRate(uint32_t* samples_per_sec) const;
  virtual int32_t SetPlayoutSampleRate(const uint32_t samples_per_sec);
  virtual int32_t PlayoutSampleRate(uint32_t* samples_per_sec) const;

  virtual int32_t ResetAudioDevice();
  virtual int32_t SetLoudspeakerStatus(bool enable);
  virtual int32_t GetLoudspeakerStatus(bool* enabled) const;
  // End of functions inherited from webrtc::AudioDeviceModule.

  // The following function is inherited from talk_base::MessageHandler.
  virtual void OnMessage(talk_base::Message* msg);

 protected:
  // The constructor is protected because the class needs to be created as a
  // reference counted object (for memory managment reasons). It could be
  // exposed in which case the burden of proper instantiation would be put on
  // the creator of a FakeAudioCaptureModule instance. To create an instance of
  // this class use the Create(..) API.
  explicit FakeAudioCaptureModule(talk_base::Thread* process_thread);
  // The destructor is protected because it is reference counted and should not
  // be deleted directly.
  virtual ~FakeAudioCaptureModule();

 private:
  // Initializes the state of the FakeAudioCaptureModule. This API is called on
  // creation by the Create() API.
  bool Initialize();
  // SetBuffer() sets all samples in send_buffer_ to |value|.
  void SetSendBuffer(int value);
  // Resets rec_buffer_. I.e., sets all rec_buffer_ samples to 0.
  void ResetRecBuffer();
  // Returns true if rec_buffer_ contains one or more sample greater than or
  // equal to |value|.
  bool CheckRecBuffer(int value);

  // Returns true/false depending on if recording or playback has been
  // enabled/started.
  bool ShouldStartProcessing();

  // Starts or stops the pushing and pulling of audio frames.
  void UpdateProcessing(bool start);

  // Starts the periodic calling of ProcessFrame() in a thread safe way.
  void StartProcessP();
  // Periodcally called function that ensures that frames are pulled and pushed
  // periodically if enabled/started.
  void ProcessFrameP();
  // Pulls frames from the registered webrtc::AudioTransport.
  void ReceiveFrameP();
  // Pushes frames to the registered webrtc::AudioTransport.
  void SendFrameP();
  // Stops the periodic calling of ProcessFrame() in a thread safe way.
  void StopProcessP();

  // The time in milliseconds when Process() was last called or 0 if no call
  // has been made.
  uint32 last_process_time_ms_;

  // Callback for playout and recording.
  webrtc::AudioTransport* audio_callback_;

  bool recording_; // True when audio is being pushed from the instance.
  bool playing_; // True when audio is being pulled by the instance.

  bool play_is_initialized_; // True when the instance is ready to pull audio.
  bool rec_is_initialized_; // True when the instance is ready to push audio.

  // Input to and output from RecordedDataIsAvailable(..) makes it possible to
  // modify the current mic level. The implementation does not care about the
  // mic level so it just feeds back what it receives.
  uint32_t current_mic_level_;

  // next_frame_time_ is updated in a non-drifting manner to indicate the next
  // wall clock time the next frame should be generated and received. started_
  // ensures that next_frame_time_ can be initialized properly on first call.
  bool started_;
  uint32 next_frame_time_;

  // User provided thread context.
  talk_base::Thread* process_thread_;

  // Buffer for storing samples received from the webrtc::AudioTransport.
  char rec_buffer_[kNumberSamples * kNumberBytesPerSample];
  // Buffer for samples to send to the webrtc::AudioTransport.
  char send_buffer_[kNumberSamples * kNumberBytesPerSample];

  // Counter of frames received that have samples of high enough amplitude to
  // indicate that the frames are not faked somewhere in the audio pipeline
  // (e.g. by a jitter buffer).
  int frames_received_;

  // Protects variables that are accessed from process_thread_ and
  // the main thread.
  mutable talk_base::CriticalSection crit_;
  // Protects |audio_callback_| that is accessed from process_thread_ and
  // the main thread.
  talk_base::CriticalSection crit_callback_;
};

#endif  // TALK_APP_WEBRTC_TEST_FAKEAUDIOCAPTUREMODULE_H_
