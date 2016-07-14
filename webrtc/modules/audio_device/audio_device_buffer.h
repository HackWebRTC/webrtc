/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_AUDIO_DEVICE_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_AUDIO_DEVICE_BUFFER_H_

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/system_wrappers/include/file_wrapper.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;

const uint32_t kPulsePeriodMs = 1000;
const size_t kMaxBufferSizeBytes = 3840;  // 10ms in stereo @ 96kHz

class AudioDeviceObserver;

class AudioDeviceBuffer {
 public:
  AudioDeviceBuffer();
  virtual ~AudioDeviceBuffer();

  void SetId(uint32_t id) {};
  int32_t RegisterAudioCallback(AudioTransport* audioCallback);

  int32_t InitPlayout();
  int32_t InitRecording();

  virtual int32_t SetRecordingSampleRate(uint32_t fsHz);
  virtual int32_t SetPlayoutSampleRate(uint32_t fsHz);
  int32_t RecordingSampleRate() const;
  int32_t PlayoutSampleRate() const;

  virtual int32_t SetRecordingChannels(size_t channels);
  virtual int32_t SetPlayoutChannels(size_t channels);
  size_t RecordingChannels() const;
  size_t PlayoutChannels() const;
  int32_t SetRecordingChannel(const AudioDeviceModule::ChannelType channel);
  int32_t RecordingChannel(AudioDeviceModule::ChannelType& channel) const;

  virtual int32_t SetRecordedBuffer(const void* audioBuffer, size_t nSamples);
  int32_t SetCurrentMicLevel(uint32_t level);
  virtual void SetVQEData(int playDelayMS, int recDelayMS, int clockDrift);
  virtual int32_t DeliverRecordedData();
  uint32_t NewMicLevel() const;

  virtual int32_t RequestPlayoutData(size_t nSamples);
  virtual int32_t GetPlayoutData(void* audioBuffer);

  int32_t StartInputFileRecording(const char fileName[kAdmMaxFileNameSize]);
  int32_t StopInputFileRecording();
  int32_t StartOutputFileRecording(const char fileName[kAdmMaxFileNameSize]);
  int32_t StopOutputFileRecording();

  int32_t SetTypingStatus(bool typingStatus);

 private:
  // Posts the first delayed task in the task queue and starts the periodic
  // timer.
  void StartTimer();

  // Called periodically on the internal thread created by the TaskQueue.
  void LogStats();

  // Updates counters in each play/record callback but does it on the task
  // queue to ensure that they can be read by LogStats() without any locks since
  // each task is serialized by the task queue.
  void UpdateRecStats(size_t num_samples);
  void UpdatePlayStats(size_t num_samples);

  // Ensures that methods are called on the same thread as the thread that
  // creates this object.
  rtc::ThreadChecker thread_checker_;

  rtc::CriticalSection _critSect;
  rtc::CriticalSection _critSectCb;

  AudioTransport* _ptrCbAudioTransport;

  // Task queue used to invoke LogStats() periodically. Tasks are executed on a
  // worker thread but it does not necessarily have to be the same thread for
  // each task.
  rtc::TaskQueue task_queue_;

  // Ensures that the timer is only started once.
  bool timer_has_started_;

  uint32_t _recSampleRate;
  uint32_t _playSampleRate;

  size_t _recChannels;
  size_t _playChannels;

  // selected recording channel (left/right/both)
  AudioDeviceModule::ChannelType _recChannel;

  // 2 or 4 depending on mono or stereo
  size_t _recBytesPerSample;
  size_t _playBytesPerSample;

  // 10ms in stereo @ 96kHz
  int8_t _recBuffer[kMaxBufferSizeBytes];

  // one sample <=> 2 or 4 bytes
  size_t _recSamples;
  size_t _recSize;  // in bytes

  // 10ms in stereo @ 96kHz
  int8_t _playBuffer[kMaxBufferSizeBytes];

  // one sample <=> 2 or 4 bytes
  size_t _playSamples;
  size_t _playSize;  // in bytes

  FileWrapper& _recFile;
  FileWrapper& _playFile;

  uint32_t _currentMicLevel;
  uint32_t _newMicLevel;

  bool _typingStatus;

  int _playDelayMS;
  int _recDelayMS;
  int _clockDrift;
  int high_delay_counter_;

  // Counts number of times LogStats() has been called.
  size_t num_stat_reports_;

  // Total number of recording callbacks where the source provides 10ms audio
  // data each time.
  uint64_t rec_callbacks_;

  // Total number of recording callbacks stored at the last timer task.
  uint64_t last_rec_callbacks_;

  // Total number of playback callbacks where the sink asks for 10ms audio
  // data each time.
  uint64_t play_callbacks_;

  // Total number of playout callbacks stored at the last timer task.
  uint64_t last_play_callbacks_;

  // Total number of recorded audio samples.
  uint64_t rec_samples_;

  // Total number of recorded samples stored at the previous timer task.
  uint64_t last_rec_samples_;

  // Total number of played audio samples.
  uint64_t play_samples_;

  // Total number of played samples stored at the previous timer task.
  uint64_t last_play_samples_;

  // Time stamp of last stat report.
  uint64_t last_log_stat_time_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_AUDIO_DEVICE_BUFFER_H_
