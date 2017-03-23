/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "webrtc/audio/test/low_bandwidth_audio_test.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/run_test.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace {
// Wait half a second between stopping sending and stopping receiving audio.
constexpr int kExtraRecordTimeMs = 500;

// Large bitrate by default.
const webrtc::CodecInst kDefaultCodec{120, "OPUS", 48000, 960, 2, 64000};

// The best that can be done with PESQ.
constexpr int kAudioFileBitRate = 16000;
}

namespace webrtc {
namespace test {

AudioQualityTest::AudioQualityTest()
    : EndToEndTest(CallTest::kDefaultTimeoutMs) {}

size_t AudioQualityTest::GetNumVideoStreams() const {
  return 0;
}
size_t AudioQualityTest::GetNumAudioStreams() const {
  return 1;
}
size_t AudioQualityTest::GetNumFlexfecStreams() const {
  return 0;
}

std::string AudioQualityTest::AudioInputFile() {
  return test::ResourcePath("voice_engine/audio_tiny16", "wav");
}

std::string AudioQualityTest::AudioOutputFile() {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  return webrtc::test::OutputPath() +
      "LowBandwidth_" + test_info->name() + ".wav";
}

std::unique_ptr<test::FakeAudioDevice::Capturer>
    AudioQualityTest::CreateCapturer() {
  return test::FakeAudioDevice::CreateWavFileReader(AudioInputFile());
}

std::unique_ptr<test::FakeAudioDevice::Renderer>
    AudioQualityTest::CreateRenderer() {
  return test::FakeAudioDevice::CreateBoundedWavFileWriter(
      AudioOutputFile(), kAudioFileBitRate);
}

void AudioQualityTest::OnFakeAudioDevicesCreated(
    test::FakeAudioDevice* send_audio_device,
    test::FakeAudioDevice* recv_audio_device) {
  send_audio_device_ = send_audio_device;
}

FakeNetworkPipe::Config AudioQualityTest::GetNetworkPipeConfig() {
  return FakeNetworkPipe::Config();
}

test::PacketTransport* AudioQualityTest::CreateSendTransport(
    Call* sender_call) {
  return new test::PacketTransport(
      sender_call, this, test::PacketTransport::kSender,
      GetNetworkPipeConfig());
}

test::PacketTransport* AudioQualityTest::CreateReceiveTransport() {
  return new test::PacketTransport(nullptr, this,
      test::PacketTransport::kReceiver, GetNetworkPipeConfig());
}

void AudioQualityTest::ModifyAudioConfigs(
  AudioSendStream::Config* send_config,
  std::vector<AudioReceiveStream::Config>* receive_configs) {
  send_config->send_codec_spec.codec_inst = kDefaultCodec;
}

void AudioQualityTest::PerformTest() {
  // Wait until the input audio file is done...
  send_audio_device_->WaitForRecordingEnd();
  // and some extra time to account for network delay.
  SleepMs(GetNetworkPipeConfig().queue_delay_ms + kExtraRecordTimeMs);
}

void AudioQualityTest::OnTestFinished() {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();

  // Output information about the input and output audio files so that further
  // processing can be done by an external process.
  printf("TEST %s %s:%s\n", test_info->name(),
         AudioInputFile().c_str(), AudioOutputFile().c_str());
}


using LowBandwidthAudioTest = CallTest;

TEST_F(LowBandwidthAudioTest, GoodNetworkHighBitrate) {
  AudioQualityTest test;
  RunBaseTest(&test);
}


class Mobile2GNetworkTest : public AudioQualityTest {
  void ModifyAudioConfigs(AudioSendStream::Config* send_config,
      std::vector<AudioReceiveStream::Config>* receive_configs) override {
    send_config->send_codec_spec.codec_inst = CodecInst{
        120,     // pltype
        "OPUS",  // plname
        48000,   // plfreq
        2880,    // pacsize
        1,       // channels
        6000     // rate bits/sec
    };
  }

  FakeNetworkPipe::Config GetNetworkPipeConfig() override {
    FakeNetworkPipe::Config pipe_config;
    pipe_config.link_capacity_kbps = 12;
    pipe_config.queue_length_packets = 1500;
    pipe_config.queue_delay_ms = 400;
    return pipe_config;
  }
};

TEST_F(LowBandwidthAudioTest, Mobile2GNetwork) {
  Mobile2GNetworkTest test;
  RunBaseTest(&test);
}

}  // namespace test
}  // namespace webrtc
