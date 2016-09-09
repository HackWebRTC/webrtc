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

// Delta times between two successive playout callbacks are limited to this
// value before added to an internal array.
const size_t kMaxDeltaTimeInMs = 500;
// TODO(henrika): remove when no longer used by external client.
const size_t kMaxBufferSizeBytes = 3840;  // 10ms in stereo @ 96kHz

class AudioDeviceObserver;

class AudioDeviceBuffer {
 public:
  AudioDeviceBuffer();
  virtual ~AudioDeviceBuffer();

  void SetId(uint32_t id) {};
  int32_t RegisterAudioCallback(AudioTransport* audio_callback);

  int32_t InitPlayout();
  int32_t InitRecording();

  int32_t SetRecordingSampleRate(uint32_t fsHz);
  int32_t SetPlayoutSampleRate(uint32_t fsHz);
  int32_t RecordingSampleRate() const;
  int32_t PlayoutSampleRate() const;

  int32_t SetRecordingChannels(size_t channels);
  int32_t SetPlayoutChannels(size_t channels);
  size_t RecordingChannels() const;
  size_t PlayoutChannels() const;
  int32_t SetRecordingChannel(const AudioDeviceModule::ChannelType channel);
  int32_t RecordingChannel(AudioDeviceModule::ChannelType& channel) const;

  virtual int32_t SetRecordedBuffer(const void* audio_buffer,
                                    size_t num_samples);
  int32_t SetCurrentMicLevel(uint32_t level);
  virtual void SetVQEData(int play_delay_ms, int rec_delay_ms, int clock_drift);
  virtual int32_t DeliverRecordedData();
  uint32_t NewMicLevel() const;

  virtual int32_t RequestPlayoutData(size_t num_samples);
  virtual int32_t GetPlayoutData(void* audio_buffer);

  // TODO(henrika): these methods should not be used and does not contain any
  // valid implementation. Investigate the possibility to either remove them
  // or add a proper implementation if needed.
  int32_t StartInputFileRecording(const char fileName[kAdmMaxFileNameSize]);
  int32_t StopInputFileRecording();
  int32_t StartOutputFileRecording(const char fileName[kAdmMaxFileNameSize]);
  int32_t StopOutputFileRecording();

  int32_t SetTypingStatus(bool typing_status);

 private:
  // Playout and recording parameters can change on the fly. e.g. at device
  // switch. These methods ensures that the callback methods always use the
  // latest parameters.
  void UpdatePlayoutParameters();
  void UpdateRecordingParameters();

  // Posts the first delayed task in the task queue and starts the periodic
  // timer.
  void StartTimer();

  // Called periodically on the internal thread created by the TaskQueue.
  void LogStats();

  // Clears all members tracking stats for recording and playout.
  void ResetRecStats();
  void ResetPlayStats();

  // Updates counters in each play/record callback but does it on the task
  // queue to ensure that they can be read by LogStats() without any locks since
  // each task is serialized by the task queue.
  void UpdateRecStats(const void* audio_buffer, size_t num_samples);
  void UpdatePlayStats(const void* audio_buffer, size_t num_samples);

  // Ensures that methods are called on the same thread as the thread that
  // creates this object.
  rtc::ThreadChecker thread_checker_;

  // Raw pointer to AudioTransport instance. Supplied to RegisterAudioCallback()
  // and it must outlive this object.
  AudioTransport* audio_transport_cb_;

  // TODO(henrika): given usage of thread checker, it should be possible to
  // remove all locks in this class.
  rtc::CriticalSection _critSect;
  rtc::CriticalSection _critSectCb;

  // Task queue used to invoke LogStats() periodically. Tasks are executed on a
  // worker thread but it does not necessarily have to be the same thread for
  // each task.
  rtc::TaskQueue task_queue_;

  // Ensures that the timer is only started once.
  bool timer_has_started_;

  // Sample rate in Hertz.
  uint32_t rec_sample_rate_;
  uint32_t play_sample_rate_;

  // Number of audio channels.
  size_t rec_channels_;
  size_t play_channels_;

  // selected recording channel (left/right/both)
  AudioDeviceModule::ChannelType rec_channel_;

  // Number of bytes per audio sample (2 or 4).
  size_t rec_bytes_per_sample_;
  size_t play_bytes_per_sample_;

  // Number of audio samples/bytes per 10ms.
  size_t rec_samples_per_10ms_;
  size_t rec_bytes_per_10ms_;
  size_t play_samples_per_10ms_;
  size_t play_bytes_per_10ms_;

  // Buffer used for recorded audio samples. Size is currently fixed
  // but it should be changed to be dynamic and correspond to
  // |play_bytes_per_10ms_|. TODO(henrika): avoid using fixed (max) size.
  std::unique_ptr<int8_t[]> rec_buffer_;

  // Buffer used for audio samples to be played out. Size is currently fixed
  // but it should be changed to be dynamic and correspond to
  // |play_bytes_per_10ms_|. TODO(henrika): avoid using fixed (max) size.
  std::unique_ptr<int8_t[]> play_buffer_;

  // AGC parameters.
  uint32_t current_mic_level_;
  uint32_t new_mic_level_;

  // Contains true of a key-press has been detected.
  bool typing_status_;

  // Delay values used by the AEC.
  int play_delay_ms_;
  int rec_delay_ms_;

  // Contains a clock-drift measurement.
  int clock_drift_;

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

  // Time stamp of last playout callback.
  uint64_t last_playout_time_;

  // An array where the position corresponds to time differences (in
  // milliseconds) between two successive playout callbacks, and the stored
  // value is the number of times a given time difference was found.
  // Writing to the array is done without a lock since it is only read once at
  // destruction when no audio is running.
  uint32_t playout_diff_times_[kMaxDeltaTimeInMs + 1] = {0};

  // Contains max level (max(abs(x))) of recorded audio packets over the last
  // 10 seconds where a new measurement is done twice per second. The level
  // is reset to zero at each call to LogStats(). Only modified on the task
  // queue thread.
  int16_t max_rec_level_;

  // Contains max level of recorded audio packets over the last 10 seconds
  // where a new measurement is done twice per second.
  int16_t max_play_level_;

  // Counts number of times we detect "no audio" corresponding to a case where
  // all level measurements since the last log has been exactly zero.
  // In other words: this counter is incremented only if 20 measurements
  // (two per second) in a row equals zero. The member is only incremented on
  // the task queue and max once every 10th second.
  size_t num_rec_level_is_zero_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_AUDIO_DEVICE_BUFFER_H_
