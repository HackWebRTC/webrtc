/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdio>

#include <gtest/gtest.h>

#include "audio_processing.h"
#include "audio_processing_unittest.pb.h"
#include "event_wrapper.h"
#include "module_common_types.h"
#include "thread_wrapper.h"
#include "trace.h"
#include "signal_processing_library.h"

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
// When true, this will compare the output data with the results stored to
// file. This is the typical case. When the file should be updated, it can
// be set to false with the command-line switch --write_output_data.
bool global_read_output_data = true;

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

class ApmTest : public ::testing::Test {
 protected:
  ApmTest();
  virtual void SetUp();
  virtual void TearDown();

  webrtc::AudioProcessing* apm_;
  webrtc::AudioFrame* frame_;
  webrtc::AudioFrame* revframe_;
  FILE* far_file_;
  FILE* near_file_;
  bool update_output_data_;
};

ApmTest::ApmTest()
    : apm_(NULL),
      far_file_(NULL),
      near_file_(NULL),
      frame_(NULL),
      revframe_(NULL) {}

void ApmTest::SetUp() {
  apm_ = AudioProcessing::Create(0);
  ASSERT_TRUE(apm_ != NULL);

  frame_ = new AudioFrame();
  revframe_ = new AudioFrame();

  ASSERT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(32000));
  ASSERT_EQ(apm_->kNoError, apm_->set_num_channels(2, 2));
  ASSERT_EQ(apm_->kNoError, apm_->set_num_reverse_channels(2));

  frame_->_payloadDataLengthInSamples = 320;
  frame_->_audioChannel = 2;
  frame_->_frequencyInHz = 32000;
  revframe_->_payloadDataLengthInSamples = 320;
  revframe_->_audioChannel = 2;
  revframe_->_frequencyInHz = 32000;

  far_file_ = fopen("aec_far.pcm", "rb");
  ASSERT_TRUE(far_file_ != NULL) << "Could not open input file aec_far.pcm\n";
  near_file_ = fopen("aec_near.pcm", "rb");
  ASSERT_TRUE(near_file_ != NULL) << "Could not open input file aec_near.pcm\n";
}

void ApmTest::TearDown() {
  if (frame_) {
    delete frame_;
  }
  frame_ = NULL;

  if (revframe_) {
    delete revframe_;
  }
  revframe_ = NULL;

  if (far_file_) {
    ASSERT_EQ(0, fclose(far_file_));
  }
  far_file_ = NULL;

  if (near_file_) {
    ASSERT_EQ(0, fclose(near_file_));
  }
  near_file_ = NULL;

  if (apm_ != NULL) {
    AudioProcessing::Destroy(apm_);
  }
  apm_ = NULL;
}

void MixStereoToMono(WebRtc_Word16* stereo,
                     WebRtc_Word16* mono,
                     int numSamples) {
  for (int i = 0; i < numSamples; i++) {
    int int32 = (static_cast<int>(stereo[i * 2]) +
                 static_cast<int>(stereo[i * 2 + 1])) >> 1;
    mono[i] = static_cast<WebRtc_Word16>(int32);
  }
}

void WriteMessageLiteToFile(const char* filename,
                            const ::google::protobuf::MessageLite& message) {
  assert(filename != NULL);

  FILE* file = fopen(filename, "wb");
  ASSERT_TRUE(file != NULL) << "Could not open " << filename;
  int size = message.ByteSize();
  ASSERT_GT(size, 0);
  unsigned char* array = new unsigned char[size];
  ASSERT_TRUE(message.SerializeToArray(array, size));

  ASSERT_EQ(1, fwrite(&size, sizeof(int), 1, file));
  ASSERT_EQ(size, fwrite(array, sizeof(unsigned char), size, file));

  delete [] array;
  fclose(file);
}

void ReadMessageLiteFromFile(const char* filename,
                             ::google::protobuf::MessageLite* message) {
  assert(filename != NULL);
  assert(message != NULL);

  FILE* file = fopen(filename, "rb");
  ASSERT_TRUE(file != NULL) << "Could not open " << filename;
  int size = 0;
  ASSERT_EQ(1, fread(&size, sizeof(int), 1, file));
  ASSERT_GT(size, 0);
  unsigned char* array = new unsigned char[size];
  ASSERT_EQ(size, fread(array, sizeof(unsigned char), size, file));

  ASSERT_TRUE(message->ParseFromArray(array, size));

  delete [] array;
  fclose(file);
}

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
            apm_->AnalyzeReverseStream(revframe_));
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
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  audio_processing_unittest::OutputData output_data;

  if (global_read_output_data) {
    ReadMessageLiteFromFile("output_data.pb", &output_data);

  } else {
    // We don't have a file; add the required tests to the protobuf.
    int rev_ch[] = {1, 2};
    int ch[] = {1, 2};
    int fs[] = {8000, 16000, 32000};
    for (size_t i = 0; i < sizeof(rev_ch) / sizeof(*rev_ch); i++) {
      for (size_t j = 0; j < sizeof(ch) / sizeof(*ch); j++) {
        for (size_t k = 0; k < sizeof(fs) / sizeof(*fs); k++) {
          audio_processing_unittest::Test* test = output_data.add_test();
          test->set_numreversechannels(rev_ch[i]);
          test->set_numchannels(ch[j]);
          test->set_samplerate(fs[k]);
        }
      }
    }
  }

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

  //EXPECT_EQ(apm_->kNoError,
  //          apm_->level_estimator()->Enable(true));

  EXPECT_EQ(apm_->kNoError,
            apm_->noise_suppression()->Enable(true));

  EXPECT_EQ(apm_->kNoError,
            apm_->voice_detection()->Enable(true));

  for (int i = 0; i < output_data.test_size(); i++) {
    printf("Running test %d of %d...\n", i + 1, output_data.test_size());

    audio_processing_unittest::Test* test = output_data.mutable_test(i);
    const int num_samples = test->samplerate() / 100;
    revframe_->_payloadDataLengthInSamples = num_samples;
    revframe_->_audioChannel = test->numreversechannels();
    revframe_->_frequencyInHz = test->samplerate();
    frame_->_payloadDataLengthInSamples = num_samples;
    frame_->_audioChannel = test->numchannels();
    frame_->_frequencyInHz = test->samplerate();

    EXPECT_EQ(apm_->kNoError, apm_->Initialize());
    ASSERT_EQ(apm_->kNoError, apm_->set_sample_rate_hz(test->samplerate()));
    ASSERT_EQ(apm_->kNoError, apm_->set_num_channels(frame_->_audioChannel,
                                                     frame_->_audioChannel));
    ASSERT_EQ(apm_->kNoError,
        apm_->set_num_reverse_channels(revframe_->_audioChannel));


    int has_echo_count = 0;
    int has_voice_count = 0;
    int is_saturated_count = 0;

    while (1) {
      WebRtc_Word16 temp_data[640];
      int analog_level = 127;

      // Read far-end frame
      size_t read_count = fread(temp_data,
                                sizeof(WebRtc_Word16),
                                num_samples * 2,
                                far_file_);
      if (read_count != static_cast<size_t>(num_samples * 2)) {
        // Check that the file really ended.
        ASSERT_NE(0, feof(far_file_));
        break; // This is expected.
      }

      if (revframe_->_audioChannel == 1) {
        MixStereoToMono(temp_data, revframe_->_payloadData,
            revframe_->_payloadDataLengthInSamples);
      } else {
        memcpy(revframe_->_payloadData,
               &temp_data[0],
               sizeof(WebRtc_Word16) * read_count);
      }

      EXPECT_EQ(apm_->kNoError,
          apm_->AnalyzeReverseStream(revframe_));

      EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(0));
      EXPECT_EQ(apm_->kNoError,
          apm_->echo_cancellation()->set_stream_drift_samples(0));
      EXPECT_EQ(apm_->kNoError,
          apm_->gain_control()->set_stream_analog_level(analog_level));

      // Read near-end frame
      read_count = fread(temp_data,
                         sizeof(WebRtc_Word16),
                         num_samples * 2,
                         near_file_);
      if (read_count != static_cast<size_t>(num_samples * 2)) {
        // Check that the file really ended.
        ASSERT_NE(0, feof(near_file_));
        break; // This is expected.
      }

      if (frame_->_audioChannel == 1) {
        MixStereoToMono(temp_data, frame_->_payloadData, num_samples);
      } else {
        memcpy(frame_->_payloadData,
               &temp_data[0],
               sizeof(WebRtc_Word16) * read_count);
      }

      EXPECT_EQ(apm_->kNoError, apm_->ProcessStream(frame_));

      if (apm_->echo_cancellation()->stream_has_echo()) {
        has_echo_count++;
      }

      analog_level = apm_->gain_control()->stream_analog_level();
      if (apm_->gain_control()->stream_is_saturated()) {
        is_saturated_count++;
      }
      if (apm_->voice_detection()->stream_has_voice()) {
        has_voice_count++;
      }
    }

    //<-- Statistics -->
    //LevelEstimator::Metrics far_metrics;
    //LevelEstimator::Metrics near_metrics;
    //EchoCancellation::Metrics echo_metrics;
    //LevelEstimator::Metrics far_metrics_ref_;
    //LevelEstimator::Metrics near_metrics_ref_;
    //EchoCancellation::Metrics echo_metrics_ref_;
    //EXPECT_EQ(apm_->kNoError,
    //          apm_->echo_cancellation()->GetMetrics(&echo_metrics));
    //EXPECT_EQ(apm_->kNoError,
    //          apm_->level_estimator()->GetMetrics(&near_metrics,

    // TODO(ajm): check echo metrics and output audio.
    if (global_read_output_data) {
      EXPECT_EQ(has_echo_count,
                test->hasechocount());
      EXPECT_EQ(has_voice_count,
                test->hasvoicecount());
      EXPECT_EQ(is_saturated_count,
                test->issaturatedcount());
    } else {
      test->set_hasechocount(has_echo_count);
      test->set_hasvoicecount(has_voice_count);
      test->set_issaturatedcount(is_saturated_count);
    }

    rewind(far_file_);
    rewind(near_file_);
  }

  if (!global_read_output_data) {
    WriteMessageLiteToFile("output_data.pb", output_data);
  }

  google::protobuf::ShutdownProtobufLibrary();
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
}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ApmEnvironment* env = new ApmEnvironment; // GTest takes ownership.
  ::testing::AddGlobalTestEnvironment(env);

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--write_output_data") == 0) {
      global_read_output_data = false;
    }
  }

  return RUN_ALL_TESTS();
}
