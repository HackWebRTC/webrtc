/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_device/android/ensure_initialized.h"
#include "webrtc/modules/audio_device/audio_device_impl.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_refptr.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/test/testsupport/fileutils.h"

using std::cout;
using std::endl;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Gt;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::TestWithParam;

// #define ENABLE_PRINTF
#ifdef ENABLE_PRINTF
#define PRINT(...) printf(__VA_ARGS__);
#else
#define PRINT(...) ((void)0)
#endif

namespace webrtc {

// Perform all tests for the different audio layers listed in this array.
// See the INSTANTIATE_TEST_CASE_P statement for details.
// TODO(henrika): the test framework supports both Java and OpenSL ES based
// audio backends but there are currently some issues (crashes) in the
// OpenSL ES implementation, hence it is not added to kAudioLayers yet.
static const AudioDeviceModule::AudioLayer kAudioLayers[] = {
    AudioDeviceModule::kAndroidJavaAudio
    /*, AudioDeviceModule::kAndroidOpenSLESAudio */};
// Number of callbacks (input or output) the tests waits for before we set
// an event indicating that the test was OK.
static const int kNumCallbacks = 10;
// Max amount of time we wait for an event to be set while counting callbacks.
static const int kTestTimeOutInMilliseconds = 10 * 1000;
// Average number of audio callbacks per second assuming 10ms packet size.
static const int kNumCallbacksPerSecond = 100;
// Play out a test file during this time (unit is in seconds).
static const int kFilePlayTimeInSec = 2;
// Fixed value for the recording delay using Java based audio backend.
// TODO(henrika): harmonize with OpenSL ES and look for possible improvements.
static const uint32_t kFixedRecordingDelay = 100;
static const int kBitsPerSample = 16;
static const int kBytesPerSample = kBitsPerSample / 8;

enum TransportType {
  kPlayout = 0x1,
  kRecording = 0x2,
};

// Simple helper struct for device specific audio parameters.
struct AudioParameters {
  int playout_frames_per_buffer() const {
    return playout_sample_rate / 100;  // WebRTC uses 10 ms as buffer size.
  }
  int recording_frames_per_buffer() const {
    return recording_sample_rate / 100;
  }
  int playout_sample_rate;
  int recording_sample_rate;
  int playout_channels;
  int recording_channels;
};

class MockAudioTransport : public AudioTransport {
 public:
  explicit MockAudioTransport(int type)
      : num_callbacks_(0),
        type_(type),
        play_count_(0),
        rec_count_(0),
        file_size_in_bytes_(0),
        sample_rate_(0),
        file_pos_(0) {}

  // Read file with name |file_name| into |file_| array to ensure that we
  // only read from memory during the test. Note that, we only support mono
  // files currently.
  bool LoadFile(const std::string& file_name, int sample_rate) {
    file_size_in_bytes_ = test::GetFileSize(file_name);
    sample_rate_ = sample_rate;
    EXPECT_NE(0, num_callbacks_)
        << "Test must call HandleCallbacks before LoadFile.";
    EXPECT_GE(file_size_in_callbacks(), num_callbacks_)
        << "Size of test file is not large enough to last during the test.";
    const int num_16bit_samples =
        test::GetFileSize(file_name) / kBytesPerSample;
    file_.reset(new int16_t[num_16bit_samples]);
    FILE* audio_file = fopen(file_name.c_str(), "rb");
    EXPECT_NE(audio_file, nullptr);
    int num_samples_read = fread(
        file_.get(), sizeof(int16_t), num_16bit_samples, audio_file);
    EXPECT_EQ(num_samples_read, num_16bit_samples);
    fclose(audio_file);
    return true;
  }

  MOCK_METHOD10(RecordedDataIsAvailable,
                int32_t(const void* audioSamples,
                        const uint32_t nSamples,
                        const uint8_t nBytesPerSample,
                        const uint8_t nChannels,
                        const uint32_t samplesPerSec,
                        const uint32_t totalDelayMS,
                        const int32_t clockDrift,
                        const uint32_t currentMicLevel,
                        const bool keyPressed,
                        uint32_t& newMicLevel));
  MOCK_METHOD8(NeedMorePlayData,
               int32_t(const uint32_t nSamples,
                       const uint8_t nBytesPerSample,
                       const uint8_t nChannels,
                       const uint32_t samplesPerSec,
                       void* audioSamples,
                       uint32_t& nSamplesOut,
                       int64_t* elapsed_time_ms,
                       int64_t* ntp_time_ms));

  void HandleCallbacks(EventWrapper* test_is_done, int num_callbacks) {
    test_is_done_ = test_is_done;
    num_callbacks_ = num_callbacks;
    if (play_mode()) {
      ON_CALL(*this, NeedMorePlayData(_, _, _, _, _, _, _, _))
          .WillByDefault(
              Invoke(this, &MockAudioTransport::RealNeedMorePlayData));
    }
    if (rec_mode()) {
      ON_CALL(*this, RecordedDataIsAvailable(_, _, _, _, _, _, _, _, _, _))
          .WillByDefault(
              Invoke(this, &MockAudioTransport::RealRecordedDataIsAvailable));
    }
  }

  int32_t RealRecordedDataIsAvailable(const void* audioSamples,
                                      const uint32_t nSamples,
                                      const uint8_t nBytesPerSample,
                                      const uint8_t nChannels,
                                      const uint32_t samplesPerSec,
                                      const uint32_t totalDelayMS,
                                      const int32_t clockDrift,
                                      const uint32_t currentMicLevel,
                                      const bool keyPressed,
                                      uint32_t& newMicLevel) {
    EXPECT_TRUE(rec_mode()) << "No test is expecting these callbacks.";
    rec_count_++;
    if (ReceivedEnoughCallbacks())
      test_is_done_->Set();
    return 0;
  }

  int32_t RealNeedMorePlayData(const uint32_t nSamples,
                               const uint8_t nBytesPerSample,
                               const uint8_t nChannels,
                               const uint32_t samplesPerSec,
                               void* audioSamples,
                               uint32_t& nSamplesOut,
                               int64_t* elapsed_time_ms,
                               int64_t* ntp_time_ms) {
    EXPECT_TRUE(play_mode()) << "No test is expecting these callbacks.";
    nSamplesOut = nSamples;
    if (file_mode()) {
      // Read samples from file stored in memory (at construction) and copy
      // |nSamples| (<=> 10ms) to the |audioSamples| byte buffer.
      memcpy(audioSamples,
             static_cast<int16_t*> (&file_[file_pos_]),
             nSamples * nBytesPerSample);
      file_pos_ += nSamples;
    }
    play_count_++;
    if (ReceivedEnoughCallbacks())
      test_is_done_->Set();
    return 0;
  }

  bool ReceivedEnoughCallbacks() {
    bool recording_done = false;
    if (rec_mode())
      recording_done = rec_count_ >= num_callbacks_;
    else
      recording_done = true;

    bool playout_done = false;
    if (play_mode())
      playout_done = play_count_ >= num_callbacks_;
    else
      playout_done = true;

    return recording_done && playout_done;
  }

  bool play_mode() const { return type_ & kPlayout; }
  bool rec_mode() const { return type_ & kRecording; }
  bool file_mode() const { return file_.get() != nullptr; }
  int file_size_in_seconds() const {
    return (file_size_in_bytes_ / (kBytesPerSample * sample_rate_));
  }
  int file_size_in_callbacks() const {
    return file_size_in_seconds() * kNumCallbacksPerSecond;
  }

 private:
  EventWrapper* test_is_done_;
  int num_callbacks_;
  int type_;
  int play_count_;
  int rec_count_;
  int file_size_in_bytes_;
  int sample_rate_;
  rtc::scoped_ptr<int16_t[]> file_;
  int file_pos_;
};

// AudioDeviceTest is a value-parameterized test.
class AudioDeviceTest
    : public testing::TestWithParam<AudioDeviceModule::AudioLayer> {
 protected:
  AudioDeviceTest()
      : test_is_done_(EventWrapper::Create()) {
    // One-time initialization of JVM and application context. Ensures that we
    // can do calls between C++ and Java. Initializes both Java and OpenSL ES
    // implementations.
    webrtc::audiodevicemodule::EnsureInitialized();
    // Creates an audio device based on the test parameter. See
    // INSTANTIATE_TEST_CASE_P() for details.
    audio_device_ = CreateAudioDevice();
    EXPECT_NE(audio_device_.get(), nullptr);
    EXPECT_EQ(0, audio_device_->Init());
    CacheAudioParameters();
  }
  virtual ~AudioDeviceTest() {
    EXPECT_EQ(0, audio_device_->Terminate());
  }

  int playout_sample_rate() const {
    return parameters_.playout_sample_rate;
  }
  int recording_sample_rate() const {
    return parameters_.recording_sample_rate;
  }
  int playout_channels() const {
    return parameters_.playout_channels;
  }
  int recording_channels() const {
    return parameters_.playout_channels;
  }
  int playout_frames_per_buffer() const {
    return parameters_.playout_frames_per_buffer();
  }
  int recording_frames_per_buffer() const {
    return parameters_.recording_frames_per_buffer();
  }

  scoped_refptr<AudioDeviceModule> audio_device() const {
    return audio_device_;
  }

  scoped_refptr<AudioDeviceModule> CreateAudioDevice() {
    scoped_refptr<AudioDeviceModule> module(
        AudioDeviceModuleImpl::Create(0, GetParam()));
    return module;
  }

  void CacheAudioParameters() {
    AudioDeviceBuffer* audio_buffer =
        static_cast<AudioDeviceModuleImpl*> (
            audio_device_.get())->GetAudioDeviceBuffer();
    parameters_.playout_sample_rate = audio_buffer->PlayoutSampleRate();
    parameters_.recording_sample_rate = audio_buffer->RecordingSampleRate();
    parameters_.playout_channels = audio_buffer->PlayoutChannels();
    parameters_.recording_channels = audio_buffer->RecordingChannels();
  }

  // Retuerns file name relative to the resource root given a sample rate.
  std::string GetFileName(int sample_rate) {
    EXPECT_TRUE(sample_rate == 48000 || sample_rate == 44100);
    char fname[64];
    snprintf(fname,
             sizeof(fname),
             "audio_device/audio_short%d",
             sample_rate / 1000);
    std::string file_name(webrtc::test::ResourcePath(fname, "pcm"));
    EXPECT_TRUE(test::FileExists(file_name));
#ifdef ENABLE_PRINTF
    PRINT("file name: %s\n", file_name.c_str());
    const int bytes = test::GetFileSize(file_name);
    PRINT("file size: %d [bytes]\n", bytes);
    PRINT("file size: %d [samples]\n", bytes / kBytesPerSample);
    const int seconds = bytes / (sample_rate * kBytesPerSample);
    PRINT("file size: %d [secs]\n", seconds);
    PRINT("file size: %d [callbacks]\n", seconds * kNumCallbacksPerSecond);
#endif
    return file_name;
  }

  void StartPlayout() {
    EXPECT_FALSE(audio_device()->PlayoutIsInitialized());
    EXPECT_FALSE(audio_device()->Playing());
    EXPECT_EQ(0, audio_device()->InitPlayout());
    EXPECT_TRUE(audio_device()->PlayoutIsInitialized());
    EXPECT_EQ(0, audio_device()->StartPlayout());
    EXPECT_TRUE(audio_device()->Playing());
  }

  void StopPlayout() {
    EXPECT_EQ(0, audio_device()->StopPlayout());
    EXPECT_FALSE(audio_device()->Playing());
  }

  void StartRecording() {
    EXPECT_FALSE(audio_device()->RecordingIsInitialized());
    EXPECT_FALSE(audio_device()->Recording());
    EXPECT_EQ(0, audio_device()->InitRecording());
    EXPECT_TRUE(audio_device()->RecordingIsInitialized());
    EXPECT_EQ(0, audio_device()->StartRecording());
    EXPECT_TRUE(audio_device()->Recording());
  }

  void StopRecording() {
    EXPECT_EQ(0, audio_device()->StopRecording());
    EXPECT_FALSE(audio_device()->Recording());
  }

  rtc::scoped_ptr<EventWrapper> test_is_done_;
  scoped_refptr<AudioDeviceModule> audio_device_;
  AudioParameters parameters_;
};

TEST_P(AudioDeviceTest, ConstructDestruct) {
  // Using the test fixture to create and destruct the audio device module.
}

// Create an audio device instance and print out the native audio parameters.
TEST_P(AudioDeviceTest, AudioParameters) {
  EXPECT_NE(0, playout_sample_rate());
  PRINT("playout_sample_rate: %d\n", playout_sample_rate());
  EXPECT_NE(0, recording_sample_rate());
  PRINT("playout_sample_rate: %d\n", recording_sample_rate());
  EXPECT_NE(0, playout_channels());
  PRINT("playout_channels: %d\n", playout_channels());
  EXPECT_NE(0, recording_channels());
  PRINT("recording_channels: %d\n", recording_channels());
}

TEST_P(AudioDeviceTest, InitTerminate) {
  // Initialization is part of the test fixture.
  EXPECT_TRUE(audio_device()->Initialized());
  EXPECT_EQ(0, audio_device()->Terminate());
  EXPECT_FALSE(audio_device()->Initialized());
}

TEST_P(AudioDeviceTest, Devices) {
  // Device enumeration is not supported. Verify fixed values only.
  EXPECT_EQ(1, audio_device()->PlayoutDevices());
  EXPECT_EQ(1, audio_device()->RecordingDevices());
}

// Tests that playout can be initiated, started and stopped.
TEST_P(AudioDeviceTest, StartStopPlayout) {
  StartPlayout();
  StopPlayout();
}

// Tests that recording can be initiated, started and stopped.
TEST_P(AudioDeviceTest, StartStopRecording) {
  StartRecording();
  StopRecording();
}

// Start playout and verify that the native audio layer starts asking for real
// audio samples to play out using the NeedMorePlayData callback.
TEST_P(AudioDeviceTest, StartPlayoutVerifyCallbacks) {
  MockAudioTransport mock(kPlayout);
  mock.HandleCallbacks(test_is_done_.get(), kNumCallbacks);
  EXPECT_CALL(mock, NeedMorePlayData(playout_frames_per_buffer(),
                                     kBytesPerSample,
                                     playout_channels(),
                                     playout_sample_rate(),
                                     NotNull(),
                                     _, _, _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartPlayout();
  test_is_done_->Wait(kTestTimeOutInMilliseconds);
  StopPlayout();
}

// Start recording and verify that the native audio layer starts feeding real
// audio samples via the RecordedDataIsAvailable callback.
TEST_P(AudioDeviceTest, StartRecordingVerifyCallbacks) {
  MockAudioTransport mock(kRecording);
  mock.HandleCallbacks(test_is_done_.get(), kNumCallbacks);
  EXPECT_CALL(mock, RecordedDataIsAvailable(NotNull(),
                                            recording_frames_per_buffer(),
                                            kBytesPerSample,
                                            recording_channels(),
                                            recording_sample_rate(),
                                            kFixedRecordingDelay,
                                            0,
                                            0,
                                            false,
                                            _))
      .Times(AtLeast(kNumCallbacks));

  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartRecording();
  test_is_done_->Wait(kTestTimeOutInMilliseconds);
  StopRecording();
}


// Start playout and recording (full-duplex audio) and verify that audio is
// active in both directions.
TEST_P(AudioDeviceTest, StartPlayoutAndRecordingVerifyCallbacks) {
  MockAudioTransport mock(kPlayout | kRecording);
  mock.HandleCallbacks(test_is_done_.get(), kNumCallbacks);
  EXPECT_CALL(mock, NeedMorePlayData(playout_frames_per_buffer(),
                                     kBytesPerSample,
                                     playout_channels(),
                                     playout_sample_rate(),
                                     NotNull(),
                                     _, _, _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_CALL(mock, RecordedDataIsAvailable(NotNull(),
                                            recording_frames_per_buffer(),
                                            kBytesPerSample,
                                            recording_channels(),
                                            recording_sample_rate(),
                                            Gt(kFixedRecordingDelay),
                                            0,
                                            0,
                                            false,
                                            _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartPlayout();
  StartRecording();
  test_is_done_->Wait(kTestTimeOutInMilliseconds);
  StopRecording();
  StopPlayout();
}

// Start playout and read audio from an external PCM file when the audio layer
// asks for data to play out. Real audio is played out in this test but it does
// not contain any explicit verification that the audio quality is perfect.
TEST_P(AudioDeviceTest, RunPlayoutWithFileAsSource) {
  // TODO(henrika): extend test when mono output is supported.
  EXPECT_EQ(1, playout_channels());
  NiceMock<MockAudioTransport> mock(kPlayout);
  mock.HandleCallbacks(test_is_done_.get(),
                       kFilePlayTimeInSec * kNumCallbacksPerSecond);
  std::string file_name = GetFileName(playout_sample_rate());
  mock.LoadFile(file_name, playout_sample_rate());
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartPlayout();
  test_is_done_->Wait(kTestTimeOutInMilliseconds);
  StopPlayout();
}

INSTANTIATE_TEST_CASE_P(AudioDeviceTest, AudioDeviceTest,
  ::testing::ValuesIn(kAudioLayers));

}  // namespace webrtc
