/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "unit_test.h"

#include "event_wrapper.h"
#include "module_common_types.h"
#include "thread_wrapper.h"
#include "trace.h"
#include "signal_processing_library.h"
#include "audio_processing.h"

using webrtc::AudioProcessing;
using webrtc::AudioFrame;
using webrtc::GainControl;
using webrtc::NoiseSuppression;
using webrtc::EchoCancellation;
using webrtc::EventWrapper;
using webrtc::Trace;
using webrtc::LevelEstimator;
using webrtc::EchoCancellation;
using webrtc::EchoControlMobile;
using webrtc::VoiceDetection;

namespace {

// If false, this will write out a new statistics file.
// For usual testing we normally want to read the file.
const bool kReadStatFile = true;

struct ThreadData {
  ThreadData(int thread_num_, AudioProcessing* ap_)
      : thread_num(thread_num_),
        error(false),
        ap(ap_) {}
  int thread_num;
  bool error;
  AudioProcessing* ap;
};

// Don't use GTest here; non-thread-safe on Windows (as of 1.5.0).
bool DeadlockProc(void* thread_object) {
  ThreadData* thread_data = static_cast<ThreadData*>(thread_object);
  AudioProcessing* ap = thread_data->ap;
  int err = ap->kNoError;

  AudioFrame primary_frame;
  AudioFrame reverse_frame;
  primary_frame._payloadDataLengthInSamples = 320;
  primary_frame._audioChannel = 2;
  primary_frame._frequencyInHz = 32000;
  reverse_frame._payloadDataLengthInSamples = 320;
  reverse_frame._audioChannel = 2;
  reverse_frame._frequencyInHz = 32000;

  ap->echo_cancellation()->Enable(true);
  ap->gain_control()->Enable(true);
  ap->high_pass_filter()->Enable(true);
  ap->level_estimator()->Enable(true);
  ap->noise_suppression()->Enable(true);
  ap->voice_detection()->Enable(true);

  if (thread_data->thread_num % 2 == 0) {
    err = ap->AnalyzeReverseStream(&reverse_frame);
    if (err != ap->kNoError) {
      printf("Error in AnalyzeReverseStream(): %d\n", err);
      thread_data->error = true;
      return false;
    }
  }

  if (thread_data->thread_num % 2 == 1) {
    ap->set_stream_delay_ms(0);
    ap->echo_cancellation()->set_stream_drift_samples(0);
    ap->gain_control()->set_stream_analog_level(0);
    err = ap->ProcessStream(&primary_frame);
    if (err == ap->kStreamParameterNotSetError) {
      printf("Expected kStreamParameterNotSetError in ProcessStream(): %d\n",
          err);
    } else if (err != ap->kNoError) {
      printf("Error in ProcessStream(): %d\n", err);
      thread_data->error = true;
      return false;
    }
    ap->gain_control()->stream_analog_level();
  }

  EventWrapper* event = EventWrapper::Create();
  event->Wait(1);
  delete event;
  event = NULL;

  return true;
}
}  // namespace

class ApmEnvironment : public ::testing::Environment {
 public:
  virtual void SetUp() {
    Trace::CreateTrace();
    ASSERT_EQ(0, Trace::SetTraceFile("apm_trace.txt"));
  }

  virtual void TearDown() {
    Trace::ReturnTrace();
  }
};

ApmTest::ApmTest()
    : apm_(NULL),
      far_file_(NULL),
      near_file_(NULL),
      stat_file_(NULL),
      frame_(NULL),
      reverse_frame_(NULL) {}

void ApmTest::SetUp() {
  apm_ = AudioProcessing::Create(0);
  ASSERT_TRUE(apm_ != NULL);

  frame_ = new AudioFrame();
  reverse_frame_ = new AudioFrame();

  ASSERT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(32000));
  ASSERT_EQ(apm_->kNoError, apm_->set_num_channels(2, 2));
  ASSERT_EQ(apm_->kNoError, apm_->set_num_reverse_channels(2));

  frame_->_payloadDataLengthInSamples = 320;
  frame_->_audioChannel = 2;
  frame_->_frequencyInHz = 32000;
  reverse_frame_->_payloadDataLengthInSamples = 320;
  reverse_frame_->_audioChannel = 2;
  reverse_frame_->_frequencyInHz = 32000;

  far_file_ = fopen("aec_far.pcm", "rb");
  ASSERT_TRUE(far_file_ != NULL) << "Cannot read source file aec_far.pcm\n";
  near_file_ = fopen("aec_near.pcm", "rb");
  ASSERT_TRUE(near_file_ != NULL) << "Cannot read source file aec_near.pcm\n";

  if (kReadStatFile) {
    stat_file_  = fopen("stat_data.dat", "rb");
    ASSERT_TRUE(stat_file_ != NULL) <<
      "Cannot write to source file stat_data.dat\n";
  }
}

void ApmTest::TearDown() {
  if (frame_) {
    delete frame_;
  }
  frame_ = NULL;

  if (reverse_frame_) {
    delete reverse_frame_;
  }
  reverse_frame_ = NULL;

  if (far_file_) {
    ASSERT_EQ(0, fclose(far_file_));
  }
  far_file_ = NULL;

  if (near_file_) {
    ASSERT_EQ(0, fclose(near_file_));
  }
  near_file_ = NULL;

  if (stat_file_) {
    ASSERT_EQ(0, fclose(stat_file_));
  }
  stat_file_ = NULL;

  if (apm_ != NULL) {
    AudioProcessing::Destroy(apm_);
  }
  apm_ = NULL;
}

/*TEST_F(ApmTest, Deadlock) {
  const int num_threads = 16;
  std::vector<ThreadWrapper*> threads(num_threads);
  std::vector<ThreadData*> thread_data(num_threads);

  ASSERT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(32000));
  ASSERT_EQ(apm_->kNoError, apm_->set_num_channels(2, 2));
  ASSERT_EQ(apm_->kNoError, apm_->set_num_reverse_channels(2));

  for (int i = 0; i < num_threads; i++) {
    thread_data[i] = new ThreadData(i, apm_);
    threads[i] = ThreadWrapper::CreateThread(DeadlockProc,
                                             thread_data[i],
                                             kNormalPriority,
                                             0);
    ASSERT_TRUE(threads[i] != NULL);
    unsigned int thread_id = 0;
    threads[i]->Start(thread_id);
  }

  EventWrapper* event = EventWrapper::Create();
  ASSERT_EQ(kEventTimeout, event->Wait(5000));
  delete event;
  event = NULL;

  for (int i = 0; i < num_threads; i++) {
    // This will return false if the thread has deadlocked.
    ASSERT_TRUE(threads[i]->Stop());
    ASSERT_FALSE(thread_data[i]->error);
    delete threads[i];
    threads[i] = NULL;
    delete thread_data[i];
    thread_data[i] = NULL;
  }
}*/

TEST_F(ApmTest, StreamParameters) {
  // No errors when the components are disabled.
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(frame_));

  // Missing agc level
  EXPECT_EQ(apm_->kNoError, apm_->Initialize());
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(true));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));
  EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(100));
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->set_stream_drift_samples(0));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(false));

  // Missing delay
  EXPECT_EQ(apm_->kNoError, apm_->Initialize());
  EXPECT_EQ(apm_->kNoError, apm_->echo_cancellation()->Enable(true));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(true));
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->set_stream_drift_samples(0));
  EXPECT_EQ(apm_->kNoError,
            apm_->gain_control()->set_stream_analog_level(127));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(false));

  // Missing drift
  EXPECT_EQ(apm_->kNoError, apm_->Initialize());
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_drift_compensation(true));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(true));
  EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(100));
  EXPECT_EQ(apm_->kNoError,
            apm_->gain_control()->set_stream_analog_level(127));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));

  // No stream parameters
  EXPECT_EQ(apm_->kNoError, apm_->Initialize());
  EXPECT_EQ(apm_->kNoError,
            apm_->AnalyzeReverseStream(reverse_frame_));
  EXPECT_EQ(apm_->kStreamParameterNotSetError,
            apm_->ProcessStream(frame_));

  // All there
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(true));
  EXPECT_EQ(apm_->kNoError, apm_->Initialize());
  EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(100));
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->set_stream_drift_samples(0));
  EXPECT_EQ(apm_->kNoError,
            apm_->gain_control()->set_stream_analog_level(127));
  EXPECT_EQ(apm_->kNoError, apm_->ProcessStream(frame_));
}

TEST_F(ApmTest, Channels) {
  // Testing number of invalid channels
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_channels(0, 1));
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_channels(1, 0));
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_channels(3, 1));
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_channels(1, 3));
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_reverse_channels(0));
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_reverse_channels(3));
  // Testing number of valid channels
  for (int i = 1; i < 3; i++) {
    for (int j = 1; j < 3; j++) {
      if (j > i) {
        EXPECT_EQ(apm_->kBadParameterError, apm_->set_num_channels(i, j));
      } else {
        EXPECT_EQ(apm_->kNoError, apm_->set_num_channels(i, j));
        EXPECT_EQ(j, apm_->num_output_channels());
      }
    }
    EXPECT_EQ(i, apm_->num_input_channels());
    EXPECT_EQ(apm_->kNoError, apm_->set_num_reverse_channels(i));
    EXPECT_EQ(i, apm_->num_reverse_channels());
  }
}

TEST_F(ApmTest, SampleRates) {
  // Testing invalid sample rates
  EXPECT_EQ(apm_->kBadParameterError, apm_->set_sample_rate_hz(10000));
  // Testing valid sample rates
  int fs[] = {8000, 16000, 32000};
  for (size_t i = 0; i < sizeof(fs) / sizeof(*fs); i++) {
    EXPECT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(fs[i]));
    EXPECT_EQ(fs[i], apm_->sample_rate_hz());
  }
}

TEST_F(ApmTest, Process) {
  if (!kReadStatFile) {
    stat_file_  = fopen("statData.dat", "wb");
    ASSERT_TRUE(stat_file_ != NULL)
      << "Cannot write to source file statData.dat\n";
  }

  AudioFrame render_audio;
  AudioFrame capture_audio;

  render_audio._payloadDataLengthInSamples = 320;
  render_audio._audioChannel = 2;
  render_audio._frequencyInHz = 32000;
  capture_audio._payloadDataLengthInSamples = 320;
  capture_audio._audioChannel = 2;
  capture_audio._frequencyInHz = 32000;

  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_drift_compensation(true));
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_metrics(true));
  EXPECT_EQ(apm_->kNoError, apm_->echo_cancellation()->Enable(true));

  EXPECT_EQ(apm_->kNoError,
            apm_->gain_control()->set_mode(GainControl::kAdaptiveAnalog));
  EXPECT_EQ(apm_->kNoError,
            apm_->gain_control()->set_analog_level_limits(0, 255));
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(true));

  EXPECT_EQ(apm_->kNoError,
            apm_->high_pass_filter()->Enable(true));

  EXPECT_EQ(apm_->kUnsupportedComponentError,
            apm_->level_estimator()->Enable(true));

  EXPECT_EQ(apm_->kNoError,
            apm_->noise_suppression()->Enable(true));

  EXPECT_EQ(apm_->kNoError,
            apm_->voice_detection()->Enable(true));

  LevelEstimator::Metrics far_metrics;
  LevelEstimator::Metrics near_metrics;
  EchoCancellation::Metrics echo_metrics;
  for (int i = 0; i < 100; i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->AnalyzeReverseStream(&render_audio));

    EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(100));
    EXPECT_EQ(apm_->kNoError,
              apm_->echo_cancellation()->set_stream_drift_samples(0));

    EXPECT_EQ(apm_->kNoError,
              apm_->gain_control()->set_stream_analog_level(127));

    EXPECT_EQ(apm_->kNoError, apm_->ProcessStream(&capture_audio));

    apm_->echo_cancellation()->stream_has_echo();
    EXPECT_EQ(apm_->kNoError,
              apm_->echo_cancellation()->GetMetrics(&echo_metrics));

    apm_->gain_control()->stream_analog_level();
    apm_->gain_control()->stream_is_saturated();

    EXPECT_EQ(apm_->kUnsupportedComponentError,
              apm_->level_estimator()->GetMetrics(&near_metrics,
                                                  &far_metrics));

    apm_->voice_detection()->stream_has_voice();
  }

  // Test with real audio
  // Loop through all possible combinations
  // (# reverse channels, # channels, sample rates)
  int rev_ch[] = {1, 2};
  int ch[] = {1, 2};
  int fs[] = {8000, 16000, 32000};
  size_t rev_ch_size = sizeof(rev_ch) / sizeof(*rev_ch);
  size_t ch_size = sizeof(ch) / sizeof(*ch);
  size_t fs_size = sizeof(fs) / sizeof(*fs);
  if (kReadStatFile) {
    fread(&rev_ch_size, sizeof(rev_ch_size), 1, stat_file_);
    fread(rev_ch, sizeof(int), rev_ch_size, stat_file_);
    fread(&ch_size, sizeof(ch_size), 1, stat_file_);
    fread(ch, sizeof(int), ch_size, stat_file_);
    fread(&fs_size, sizeof(fs_size), 1, stat_file_);
    fread(fs, sizeof(int), fs_size, stat_file_);
  } else {
    fwrite(&rev_ch_size, sizeof(int), 1, stat_file_);
    fwrite(rev_ch, sizeof(int), rev_ch_size, stat_file_);
    fwrite(&ch_size, sizeof(int), 1, stat_file_);
    fwrite(ch, sizeof(int), ch_size, stat_file_);
    fwrite(&fs_size, sizeof(int), 1, stat_file_);
    fwrite(fs, sizeof(int), fs_size, stat_file_);
  }
  int test_count = 0;
  for (size_t i_rev_ch = 0; i_rev_ch < rev_ch_size; i_rev_ch++) {
    for (size_t i_ch = 0; i_ch < ch_size; i_ch++) {
      for (size_t i_fs = 0; i_fs < fs_size; i_fs++) {
        render_audio._payloadDataLengthInSamples = fs[i_fs] / 100;
        render_audio._audioChannel = rev_ch[i_rev_ch];
        render_audio._frequencyInHz = fs[i_fs];
        capture_audio._payloadDataLengthInSamples = fs[i_fs] / 100;
        capture_audio._audioChannel = ch[i_ch];
        capture_audio._frequencyInHz = fs[i_fs];

        EXPECT_EQ(apm_->kNoError, apm_->Initialize());
        ASSERT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(fs[i_fs]));
        ASSERT_EQ(apm_->kNoError,
            apm_->set_num_channels(capture_audio._audioChannel,
                                   capture_audio._audioChannel));
        ASSERT_EQ(apm_->kNoError,
                  apm_->set_num_reverse_channels(render_audio._audioChannel));
        EXPECT_EQ(apm_->kNoError,
                  apm_->echo_cancellation()->enable_drift_compensation(false));
        EXPECT_EQ(apm_->kNoError,
                  apm_->echo_cancellation()->enable_metrics(true));
        EXPECT_EQ(apm_->kNoError, apm_->echo_cancellation()->Enable(true));

        EXPECT_EQ(apm_->kNoError,
                  apm_->gain_control()->set_mode(GainControl::kAdaptiveAnalog));
        EXPECT_EQ(apm_->kNoError,
                  apm_->gain_control()->set_analog_level_limits(0, 255));
        EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(true));

        EXPECT_EQ(apm_->kNoError,
                  apm_->high_pass_filter()->Enable(true));

        //EXPECT_EQ(apm_->kNoError,
        //          apm_->level_estimator()->Enable(true));

        EXPECT_EQ(apm_->kNoError,
                  apm_->noise_suppression()->Enable(true));

        EXPECT_EQ(apm_->kNoError,
                  apm_->voice_detection()->Enable(true));
        bool runningFiles = true;
        int echo_count = 0;
        int vad_count = 0;
        int sat_count = 0;
        int analog_level = 127;
        int sat_gain = 2;
        size_t read_count = 0;
        WebRtc_Word16 tmpData[640];
        int tmp_int;

        int echo_count_ref_ = 0;
        int vad_count_ref_ = 0;
        int sat_count_ref_ = 0;
        //LevelEstimator::Metrics far_metrics_ref_;
        //LevelEstimator::Metrics near_metrics_ref_;
        EchoCancellation::Metrics echo_metrics_ref_;

        while (runningFiles) {
          // Read far end frame
          read_count = fread(tmpData,
                             sizeof(WebRtc_Word16),
                             render_audio._payloadDataLengthInSamples * 2,
                             far_file_);
          if (read_count !=
              static_cast<size_t>
              (render_audio._payloadDataLengthInSamples * 2)) {
            break; // This is expected.
          }
          if (render_audio._audioChannel == 1) {
            for (int i = 0; i < render_audio._payloadDataLengthInSamples;
                i++) {
              tmp_int = (static_cast<int>(tmpData[i * 2]) +
                  static_cast<int>(tmpData[i * 2 + 1])) >> 1;
              render_audio._payloadData[i] =
                  static_cast<WebRtc_Word16>(tmp_int);
            }
          } else {
            memcpy(render_audio._payloadData,
                   &tmpData[0],
                   sizeof(WebRtc_Word16) * read_count);
          }
          EXPECT_EQ(apm_->kNoError,
              apm_->AnalyzeReverseStream(&render_audio));

          EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(0));
          EXPECT_EQ(apm_->kNoError,
              apm_->gain_control()->set_stream_analog_level(analog_level));

          // Read near end frame
          read_count = fread(tmpData,
                             sizeof(WebRtc_Word16),
                             capture_audio._payloadDataLengthInSamples * 2,
                             near_file_);
          if (read_count !=
              static_cast<size_t>
              (capture_audio._payloadDataLengthInSamples * 2)) {
            break; // This is expected.
          }
          if (capture_audio._audioChannel == 1) {
            for (int i = 0;
                i < capture_audio._payloadDataLengthInSamples; i++) {
              tmp_int = (static_cast<int>(tmpData[i * 2]) +
                  static_cast<int>(tmpData[i * 2 + 1])) >> 1;
              capture_audio._payloadData[i] =
                  static_cast<WebRtc_Word16>(tmp_int);
            }
          } else {
            memcpy(capture_audio._payloadData,
                   &tmpData[0],
                   sizeof(WebRtc_Word16) * read_count);
          }
          WebRtc_Word32 tmpF = 0;
          for (size_t i = 0; i < read_count; i++) {
            tmpF = (WebRtc_Word32)capture_audio._payloadData[i] * sat_gain;
            if (tmpF > WEBRTC_SPL_WORD16_MAX) {
              capture_audio._payloadData[i] = WEBRTC_SPL_WORD16_MAX;
            } else if (tmpF < WEBRTC_SPL_WORD16_MIN) {
              capture_audio._payloadData[i] = WEBRTC_SPL_WORD16_MIN;
            } else {
              capture_audio._payloadData[i] = static_cast<WebRtc_Word16>(tmpF);
            }
          }
          EXPECT_EQ(apm_->kNoError, apm_->ProcessStream(&capture_audio));

          if (apm_->echo_cancellation()->stream_has_echo()) {
            echo_count++;
          }

          analog_level = apm_->gain_control()->stream_analog_level();
          if (apm_->gain_control()->stream_is_saturated()) {
            sat_count++;
            sat_gain = 1;
          }
          if (apm_->voice_detection()->stream_has_voice()) {
            vad_count++;
          }
        }
        //<-- Statistics -->
        EXPECT_EQ(apm_->kNoError,
                  apm_->echo_cancellation()->GetMetrics(&echo_metrics));
        //EXPECT_EQ(apm_->kNoError,
        //          apm_->level_estimator()->GetMetrics(&near_metrics,

        // TODO(ajm): Perhaps we don't have to check every value? The average
        //            could be sufficient. Or, how about hashing the output?
        if (kReadStatFile) {
          // Read from statData
          fread(&echo_count_ref_, 1, sizeof(echo_count), stat_file_);
          EXPECT_EQ(echo_count_ref_, echo_count);
          fread(&echo_metrics_ref_,
                1,
                sizeof(EchoCancellation::Metrics),
                stat_file_);
          EXPECT_EQ(echo_metrics_ref_.residual_echo_return_loss.instant,
                    echo_metrics.residual_echo_return_loss.instant);
          EXPECT_EQ(echo_metrics_ref_.residual_echo_return_loss.average,
                    echo_metrics.residual_echo_return_loss.average);
          EXPECT_EQ(echo_metrics_ref_.residual_echo_return_loss.maximum,
                    echo_metrics.residual_echo_return_loss.maximum);
          EXPECT_EQ(echo_metrics_ref_.residual_echo_return_loss.minimum,
                    echo_metrics.residual_echo_return_loss.minimum);
          EXPECT_EQ(echo_metrics_ref_.echo_return_loss.instant,
                    echo_metrics.echo_return_loss.instant);
          EXPECT_EQ(echo_metrics_ref_.echo_return_loss.average,
                    echo_metrics.echo_return_loss.average);
          EXPECT_EQ(echo_metrics_ref_.echo_return_loss.maximum,
                    echo_metrics.echo_return_loss.maximum);
          EXPECT_EQ(echo_metrics_ref_.echo_return_loss.minimum,
                    echo_metrics.echo_return_loss.minimum);
          EXPECT_EQ(echo_metrics_ref_.echo_return_loss_enhancement.instant,
                    echo_metrics.echo_return_loss_enhancement.instant);
          EXPECT_EQ(echo_metrics_ref_.echo_return_loss_enhancement.average,
                    echo_metrics.echo_return_loss_enhancement.average);
          EXPECT_EQ(echo_metrics_ref_.echo_return_loss_enhancement.maximum,
                    echo_metrics.echo_return_loss_enhancement.maximum);
          EXPECT_EQ(echo_metrics_ref_.echo_return_loss_enhancement.minimum,
                    echo_metrics.echo_return_loss_enhancement.minimum);
          EXPECT_EQ(echo_metrics_ref_.a_nlp.instant,
                    echo_metrics.a_nlp.instant);
          EXPECT_EQ(echo_metrics_ref_.a_nlp.average,
                    echo_metrics.a_nlp.average);
          EXPECT_EQ(echo_metrics_ref_.a_nlp.maximum,
                    echo_metrics.a_nlp.maximum);
          EXPECT_EQ(echo_metrics_ref_.a_nlp.minimum,
                    echo_metrics.a_nlp.minimum);

          fread(&vad_count_ref_, 1, sizeof(vad_count), stat_file_);
          EXPECT_EQ(vad_count_ref_, vad_count);
          fread(&sat_count_ref_, 1, sizeof(sat_count), stat_file_);
          EXPECT_EQ(sat_count_ref_, sat_count);

          /*fread(&far_metrics_ref_,
                1,
                sizeof(LevelEstimator::Metrics),
                stat_file_);
          EXPECT_EQ(far_metrics_ref_.signal.instant,
                    far_metrics.signal.instant);
          EXPECT_EQ(far_metrics_ref_.signal.average,
                    far_metrics.signal.average);
          EXPECT_EQ(far_metrics_ref_.signal.maximum,
                    far_metrics.signal.maximum);
          EXPECT_EQ(far_metrics_ref_.signal.minimum,
                    far_metrics.signal.minimum);

          EXPECT_EQ(far_metrics_ref_.speech.instant,
                    far_metrics.speech.instant);
          EXPECT_EQ(far_metrics_ref_.speech.average,
                    far_metrics.speech.average);
          EXPECT_EQ(far_metrics_ref_.speech.maximum,
                    far_metrics.speech.maximum);
          EXPECT_EQ(far_metrics_ref_.speech.minimum,
                    far_metrics.speech.minimum);

          EXPECT_EQ(far_metrics_ref_.noise.instant,
                    far_metrics.noise.instant);
          EXPECT_EQ(far_metrics_ref_.noise.average,
                    far_metrics.noise.average);
          EXPECT_EQ(far_metrics_ref_.noise.maximum,
                    far_metrics.noise.maximum);
          EXPECT_EQ(far_metrics_ref_.noise.minimum,
                    far_metrics.noise.minimum);

          fread(&near_metrics_ref_,
                1,
                sizeof(LevelEstimator::Metrics),
                stat_file_);
          EXPECT_EQ(near_metrics_ref_.signal.instant,
                    near_metrics.signal.instant);
          EXPECT_EQ(near_metrics_ref_.signal.average,
                    near_metrics.signal.average);
          EXPECT_EQ(near_metrics_ref_.signal.maximum,
                    near_metrics.signal.maximum);
          EXPECT_EQ(near_metrics_ref_.signal.minimum,
                    near_metrics.signal.minimum);

          EXPECT_EQ(near_metrics_ref_.speech.instant,
                    near_metrics.speech.instant);
          EXPECT_EQ(near_metrics_ref_.speech.average,
                    near_metrics.speech.average);
          EXPECT_EQ(near_metrics_ref_.speech.maximum,
                    near_metrics.speech.maximum);
          EXPECT_EQ(near_metrics_ref_.speech.minimum,
                    near_metrics.speech.minimum);

          EXPECT_EQ(near_metrics_ref_.noise.instant,
                    near_metrics.noise.instant);
          EXPECT_EQ(near_metrics_ref_.noise.average,
                    near_metrics.noise.average);
          EXPECT_EQ(near_metrics_ref_.noise.maximum,
                    near_metrics.noise.maximum);
          EXPECT_EQ(near_metrics_ref_.noise.minimum,
                    near_metrics.noise.minimum);*/
        } else {
          // Write to statData
          fwrite(&echo_count, 1, sizeof(echo_count), stat_file_);
          fwrite(&echo_metrics,
                 1,
                 sizeof(EchoCancellation::Metrics),
                 stat_file_);
          fwrite(&vad_count, 1, sizeof(vad_count), stat_file_);
          fwrite(&sat_count, 1, sizeof(sat_count), stat_file_);
          //fwrite(&far_metrics, 1, sizeof(LevelEstimator::Metrics), stat_file_);
          //fwrite(&near_metrics, 1, sizeof(LevelEstimator::Metrics), stat_file_);
        }

        rewind(far_file_);
        rewind(near_file_);
        test_count++;
        printf("Loop %d of %lu\n", test_count, rev_ch_size * ch_size * fs_size);
      }
    }
  }
  if (!kReadStatFile) {
    if (stat_file_ != NULL) {
      ASSERT_EQ(0, fclose(stat_file_));
    }
    stat_file_ = NULL;
  }
}

TEST_F(ApmTest, EchoCancellation) {
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_drift_compensation(true));
  EXPECT_TRUE(apm_->echo_cancellation()->is_drift_compensation_enabled());
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_drift_compensation(false));
  EXPECT_FALSE(apm_->echo_cancellation()->is_drift_compensation_enabled());

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->echo_cancellation()->set_device_sample_rate_hz(4000));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->echo_cancellation()->set_device_sample_rate_hz(100000));

  int rate[] = {16000, 44100, 48000};
  for (size_t i = 0; i < sizeof(rate)/sizeof(*rate); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->echo_cancellation()->set_device_sample_rate_hz(rate[i]));
    EXPECT_EQ(rate[i],
        apm_->echo_cancellation()->device_sample_rate_hz());
  }

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->echo_cancellation()->set_suppression_level(
          static_cast<EchoCancellation::SuppressionLevel>(-1)));

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->echo_cancellation()->set_suppression_level(
          static_cast<EchoCancellation::SuppressionLevel>(4)));

  EchoCancellation::SuppressionLevel level[] = {
    EchoCancellation::kLowSuppression,
    EchoCancellation::kModerateSuppression,
    EchoCancellation::kHighSuppression,
  };
  for (size_t i = 0; i < sizeof(level)/sizeof(*level); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->echo_cancellation()->set_suppression_level(level[i]));
    EXPECT_EQ(level[i],
        apm_->echo_cancellation()->suppression_level());
  }

  EchoCancellation::Metrics metrics;
  EXPECT_EQ(apm_->kNotEnabledError,
            apm_->echo_cancellation()->GetMetrics(&metrics));

  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_metrics(true));
  EXPECT_TRUE(apm_->echo_cancellation()->are_metrics_enabled());
  EXPECT_EQ(apm_->kNoError,
            apm_->echo_cancellation()->enable_metrics(false));
  EXPECT_FALSE(apm_->echo_cancellation()->are_metrics_enabled());

  EXPECT_EQ(apm_->kNoError, apm_->echo_cancellation()->Enable(true));
  EXPECT_TRUE(apm_->echo_cancellation()->is_enabled());
  EXPECT_EQ(apm_->kNoError, apm_->echo_cancellation()->Enable(false));
  EXPECT_FALSE(apm_->echo_cancellation()->is_enabled());
}

TEST_F(ApmTest, EchoControlMobile) {
  // AECM won't use super-wideband.
  EXPECT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(32000));
  EXPECT_EQ(apm_->kBadSampleRateError, apm_->echo_control_mobile()->Enable(true));
  EXPECT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(16000));
  // Turn AECM on (and AEC off)
  EXPECT_EQ(apm_->kNoError, apm_->echo_control_mobile()->Enable(true));
  EXPECT_TRUE(apm_->echo_control_mobile()->is_enabled());

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->echo_control_mobile()->set_routing_mode(
      static_cast<EchoControlMobile::RoutingMode>(-1)));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->echo_control_mobile()->set_routing_mode(
      static_cast<EchoControlMobile::RoutingMode>(5)));

  // Toggle routing modes
  EchoControlMobile::RoutingMode mode[] = {
      EchoControlMobile::kQuietEarpieceOrHeadset,
      EchoControlMobile::kEarpiece,
      EchoControlMobile::kLoudEarpiece,
      EchoControlMobile::kSpeakerphone,
      EchoControlMobile::kLoudSpeakerphone,
  };
  for (size_t i = 0; i < sizeof(mode)/sizeof(*mode); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->echo_control_mobile()->set_routing_mode(mode[i]));
    EXPECT_EQ(mode[i],
        apm_->echo_control_mobile()->routing_mode());
  }
  // Turn comfort noise off/on
  EXPECT_EQ(apm_->kNoError,
      apm_->echo_control_mobile()->enable_comfort_noise(false));
  EXPECT_FALSE(apm_->echo_control_mobile()->is_comfort_noise_enabled());
  EXPECT_EQ(apm_->kNoError,
      apm_->echo_control_mobile()->enable_comfort_noise(true));
  EXPECT_TRUE(apm_->echo_control_mobile()->is_comfort_noise_enabled());
  // Turn AECM off
  EXPECT_EQ(apm_->kNoError, apm_->echo_control_mobile()->Enable(false));
  EXPECT_FALSE(apm_->echo_control_mobile()->is_enabled());
}

TEST_F(ApmTest, GainControl) {
  // Testing gain modes
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_mode(static_cast<GainControl::Mode>(-1)));

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_mode(static_cast<GainControl::Mode>(3)));

  EXPECT_EQ(apm_->kNoError,
      apm_->gain_control()->set_mode(
      apm_->gain_control()->mode()));

  GainControl::Mode mode[] = {
    GainControl::kAdaptiveAnalog,
    GainControl::kAdaptiveDigital,
    GainControl::kFixedDigital
  };
  for (size_t i = 0; i < sizeof(mode)/sizeof(*mode); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->gain_control()->set_mode(mode[i]));
    EXPECT_EQ(mode[i], apm_->gain_control()->mode());
  }
  // Testing invalid target levels
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_target_level_dbfs(-3));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_target_level_dbfs(-40));
  // Testing valid target levels
  EXPECT_EQ(apm_->kNoError,
      apm_->gain_control()->set_target_level_dbfs(
      apm_->gain_control()->target_level_dbfs()));

  int level_dbfs[] = {0, 6, 31};
  for (size_t i = 0; i < sizeof(level_dbfs)/sizeof(*level_dbfs); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->gain_control()->set_target_level_dbfs(level_dbfs[i]));
    EXPECT_EQ(level_dbfs[i], apm_->gain_control()->target_level_dbfs());
  }

  // Testing invalid compression gains
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_compression_gain_db(-1));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_compression_gain_db(100));

  // Testing valid compression gains
  EXPECT_EQ(apm_->kNoError,
      apm_->gain_control()->set_compression_gain_db(
      apm_->gain_control()->compression_gain_db()));

  int gain_db[] = {0, 10, 90};
  for (size_t i = 0; i < sizeof(gain_db)/sizeof(*gain_db); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->gain_control()->set_compression_gain_db(gain_db[i]));
    EXPECT_EQ(gain_db[i], apm_->gain_control()->compression_gain_db());
  }

  // Testing limiter off/on
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->enable_limiter(false));
  EXPECT_FALSE(apm_->gain_control()->is_limiter_enabled());
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->enable_limiter(true));
  EXPECT_TRUE(apm_->gain_control()->is_limiter_enabled());

  // Testing invalid level limits
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_analog_level_limits(-1, 512));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_analog_level_limits(100000, 512));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_analog_level_limits(512, -1));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_analog_level_limits(512, 100000));
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->gain_control()->set_analog_level_limits(512, 255));

  // Testing valid level limits
  EXPECT_EQ(apm_->kNoError,
      apm_->gain_control()->set_analog_level_limits(
      apm_->gain_control()->analog_level_minimum(),
      apm_->gain_control()->analog_level_maximum()));

  int min_level[] = {0, 255, 1024};
  for (size_t i = 0; i < sizeof(min_level)/sizeof(*min_level); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->gain_control()->set_analog_level_limits(min_level[i], 1024));
    EXPECT_EQ(min_level[i], apm_->gain_control()->analog_level_minimum());
  }

  int max_level[] = {0, 1024, 65535};
  for (size_t i = 0; i < sizeof(min_level)/sizeof(*min_level); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->gain_control()->set_analog_level_limits(0, max_level[i]));
    EXPECT_EQ(max_level[i], apm_->gain_control()->analog_level_maximum());
  }

  // TODO(ajm): stream_is_saturated() and stream_analog_level()

  // Turn AGC off
  EXPECT_EQ(apm_->kNoError, apm_->gain_control()->Enable(false));
  EXPECT_FALSE(apm_->gain_control()->is_enabled());
}

TEST_F(ApmTest, NoiseSuppression) {
  // Tesing invalid suppression levels
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->noise_suppression()->set_level(
          static_cast<NoiseSuppression::Level>(-1)));

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->noise_suppression()->set_level(
          static_cast<NoiseSuppression::Level>(5)));

  // Tesing valid suppression levels
  NoiseSuppression::Level level[] = {
    NoiseSuppression::kLow,
    NoiseSuppression::kModerate,
    NoiseSuppression::kHigh,
    NoiseSuppression::kVeryHigh
  };
  for (size_t i = 0; i < sizeof(level)/sizeof(*level); i++) {
    EXPECT_EQ(apm_->kNoError,
        apm_->noise_suppression()->set_level(level[i]));
    EXPECT_EQ(level[i], apm_->noise_suppression()->level());
  }

  // Turing NS on/off
  EXPECT_EQ(apm_->kNoError, apm_->noise_suppression()->Enable(true));
  EXPECT_TRUE(apm_->noise_suppression()->is_enabled());
  EXPECT_EQ(apm_->kNoError, apm_->noise_suppression()->Enable(false));
  EXPECT_FALSE(apm_->noise_suppression()->is_enabled());
}

TEST_F(ApmTest, HighPassFilter) {
  // Turing HP filter on/off
  EXPECT_EQ(apm_->kNoError, apm_->high_pass_filter()->Enable(true));
  EXPECT_TRUE(apm_->high_pass_filter()->is_enabled());
  EXPECT_EQ(apm_->kNoError, apm_->high_pass_filter()->Enable(false));
  EXPECT_FALSE(apm_->high_pass_filter()->is_enabled());
}

TEST_F(ApmTest, LevelEstimator) {
  // Turing Level estimator on/off
  EXPECT_EQ(apm_->kUnsupportedComponentError,
            apm_->level_estimator()->Enable(true));
  EXPECT_FALSE(apm_->level_estimator()->is_enabled());
  EXPECT_EQ(apm_->kUnsupportedComponentError,
            apm_->level_estimator()->Enable(false));
  EXPECT_FALSE(apm_->level_estimator()->is_enabled());
}

TEST_F(ApmTest, VoiceDetection) {
  // Test external VAD
  EXPECT_EQ(apm_->kNoError,
            apm_->voice_detection()->set_stream_has_voice(true));
  EXPECT_TRUE(apm_->voice_detection()->stream_has_voice());
  EXPECT_EQ(apm_->kNoError,
            apm_->voice_detection()->set_stream_has_voice(false));
  EXPECT_FALSE(apm_->voice_detection()->stream_has_voice());

  // Tesing invalid likelihoods
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->voice_detection()->set_likelihood(
          static_cast<VoiceDetection::Likelihood>(-1)));

  EXPECT_EQ(apm_->kBadParameterError,
      apm_->voice_detection()->set_likelihood(
          static_cast<VoiceDetection::Likelihood>(5)));

  // Tesing valid likelihoods
  VoiceDetection::Likelihood likelihood[] = {
      VoiceDetection::kVeryLowLikelihood,
      VoiceDetection::kLowLikelihood,
      VoiceDetection::kModerateLikelihood,
      VoiceDetection::kHighLikelihood
  };
  for (size_t i = 0; i < sizeof(likelihood)/sizeof(*likelihood); i++) {
    EXPECT_EQ(apm_->kNoError,
              apm_->voice_detection()->set_likelihood(likelihood[i]));
    EXPECT_EQ(likelihood[i], apm_->voice_detection()->likelihood());
  }

  /* TODO(bjornv): Enable once VAD supports other frame lengths than 10 ms
  // Tesing invalid frame sizes
  EXPECT_EQ(apm_->kBadParameterError,
      apm_->voice_detection()->set_frame_size_ms(12));

  // Tesing valid frame sizes
  for (int i = 10; i <= 30; i += 10) {
    EXPECT_EQ(apm_->kNoError,
        apm_->voice_detection()->set_frame_size_ms(i));
    EXPECT_EQ(i, apm_->voice_detection()->frame_size_ms());
  }
  */

  // Turing VAD on/off
  EXPECT_EQ(apm_->kNoError, apm_->voice_detection()->Enable(true));
  EXPECT_TRUE(apm_->voice_detection()->is_enabled());
  EXPECT_EQ(apm_->kNoError, apm_->voice_detection()->Enable(false));
  EXPECT_FALSE(apm_->voice_detection()->is_enabled());

  // TODO(bjornv): Add tests for streamed voice; stream_has_voice()
}

// Below are some ideas for tests from VPM.

/*TEST_F(VideoProcessingModuleTest, GetVersionTest)
{
}

TEST_F(VideoProcessingModuleTest, HandleNullBuffer)
{
}

TEST_F(VideoProcessingModuleTest, HandleBadSize)
{
}

TEST_F(VideoProcessingModuleTest, IdenticalResultsAfterReset)
{
}
*/

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ApmEnvironment* env = new ApmEnvironment;
  ::testing::AddGlobalTestEnvironment(env);

  return RUN_ALL_TESTS();
}
