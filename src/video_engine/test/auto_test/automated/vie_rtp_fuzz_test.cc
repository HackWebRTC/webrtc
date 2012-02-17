/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib>
#include <cmath>
#include <ctime>

#include "gtest/gtest.h"
#include "gflags/gflags.h"
#include "video_engine/include/vie_encryption.h"
#include "video_engine/test/auto_test/automated/two_windows_fixture.h"
#include "video_engine/test/auto_test/helpers/vie_window_creator.h"
#include "video_engine/test/auto_test/interface/tb_capture_device.h"
#include "video_engine/test/auto_test/interface/tb_interfaces.h"
#include "video_engine/test/auto_test/interface/tb_video_channel.h"
#include "video_engine/test/auto_test/interface/vie_autotest_window_manager_interface.h"
#include "video_engine/test/auto_test/primitives/general_primitives.h"
#include "video_engine/vie_defines.h"

namespace {

DEFINE_int32(rtp_fuzz_test_rand_seed, 0, "The rand seed to use for "
             "the RTP fuzz test. Defaults to time(). 0 cannot be specified.");

class ViERtpFuzzTest : public TwoWindowsFixture {
 protected:
  unsigned int FetchRandSeed() {
    if (FLAGS_rtp_fuzz_test_rand_seed != 0) {
      return FLAGS_rtp_fuzz_test_rand_seed;
    }
    return std::time(NULL);
  }
};

static int Saturate(int value, int min, int max) {
  return std::min(std::max(value, min), max);
}

// These algorithms attempt to create an uncrackable encryption
// scheme by completely disregarding the input data.
class RandomEncryption : public webrtc::Encryption {
 public:
  RandomEncryption(unsigned int rand_seed) {
    srand(rand_seed);
  }

  virtual void encrypt(int channel_no, unsigned char* in_data,
                       unsigned char* out_data, int bytes_in, int* bytes_out) {
    GenerateRandomData(out_data, bytes_in, bytes_out);
  }

  virtual void decrypt(int channel_no, unsigned char* in_data,
                       unsigned char* out_data, int bytes_in, int* bytes_out) {
    GenerateRandomData(out_data, bytes_in, bytes_out);
  }

  virtual void encrypt_rtcp(int channel_no, unsigned char* in_data,
                            unsigned char* out_data, int bytes_in,
                            int* bytes_out) {
    GenerateRandomData(out_data, bytes_in, bytes_out);
  }

  virtual void decrypt_rtcp(int channel_no, unsigned char* in_data,
                            unsigned char* out_data, int bytes_in,
                            int* bytes_out) {
    GenerateRandomData(out_data, bytes_in, bytes_out);
  }

 private:
  // Generates some completely random data with roughly the right length.
  void GenerateRandomData(unsigned char* out_data, int bytes_in,
                            int* bytes_out) {
    int out_length = MakeUpSimilarLength(bytes_in);
    for (int i = 0; i < out_length; i++) {
      // The modulo will skew the random distribution a bit, but I think it
      // will be random enough.
      out_data[i] = static_cast<unsigned char>(rand() % 256);
    }
    *bytes_out = out_length;
  }

  // Makes up a length within +- 50 of the original length, without
  // overstepping the contract for encrypt / decrypt.
  int MakeUpSimilarLength(int original_length) {
    int sign = rand() - RAND_MAX / 2;
    int length = original_length + sign * rand() % 50;

    return Saturate(length, 0, static_cast<int>(webrtc::kViEMaxMtu));
  }
};

TEST_F(ViERtpFuzzTest, VideoEngineRecoversAfterSomeCompletelyRandomPackets) {
  unsigned int rand_seed = FetchRandSeed();
  ViETest::Log("Running test with rand seed %d.", rand_seed);

  TbInterfaces video_engine("ViERtpTryInjectingRandomPacketsIntoRtpStream");
  TbVideoChannel video_channel(video_engine, webrtc::kVideoCodecVP8);
  TbCaptureDevice capture_device(video_engine);

  capture_device.ConnectTo(video_channel.videoChannel);

  // Enable PLI RTCP, which will allow the video engine to recover better.
  video_engine.rtp_rtcp->SetKeyFrameRequestMethod(
      video_channel.videoChannel, webrtc::kViEKeyFrameRequestPliRtcp);

  video_channel.StartReceive();
  video_channel.StartSend();

  RenderInWindow(
      video_engine.render, capture_device.captureId, window_1_, 0);
  RenderInWindow(
      video_engine.render, video_channel.videoChannel, window_2_, 1);

  ViETest::Log("Running as usual. You should see video output.");
  AutoTestSleep(2000);

  ViETest::Log("Injecting completely random packets...");
  RandomEncryption random_encryption(rand_seed);
  video_engine.encryption->RegisterExternalEncryption(
      video_channel.videoChannel, random_encryption);

  AutoTestSleep(5000);

  ViETest::Log("Back to normal.");
  video_engine.encryption->DeregisterExternalEncryption(
      video_channel.videoChannel);

  AutoTestSleep(5000);
}

}
