/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_TESTVADDTX_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_TESTVADDTX_H_


#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/main/test/ACMTest.h"
#include "webrtc/modules/audio_coding/main/test/Channel.h"

namespace webrtc {

class ActivityMonitor : public ACMVADCallback {
 public:
  static const int kPacketTypes = 6;

  ActivityMonitor();
  int32_t InFrameType(int16_t frame_type);
  void PrintStatistics();
  void ResetStatistics();
  void GetStatistics(uint32_t* stats);
 private:
  // Counting according to
  //   counter_[0] - kNoEncoding,
  //   counter_[1] - kActiveNormalEncoded,
  //   counter_[2] - kPassiveNormalEncoded,
  //   counter_[3] - kPassiveDTXNB,
  //   counter_[4] - kPassiveDTXWB,
  //   counter_[5] - kPassiveDTXSWB
  uint32_t counter_[kPacketTypes];
};


// TestVadDtx is to verify that VAD/DTX perform as they should. It runs through
// an audio file and check if the occurrence of various packet types follows
// expectation. TestVadDtx needs its derived class to implement the Perform()
// to put the test together.
class TestVadDtx : public ACMTest {
 public:
  static const int kOutputFreqHz = 16000;
  static const int kPacketTypes = 6;

  TestVadDtx();

  virtual void Perform() = 0;

 protected:
  void RegisterCodec(CodecInst codec_param);

  // Encoding a file and see if the numbers that various packets occur follow
  // the expectation. Saves result to a file.
  // expects[x] means
  // -1 : do not care,
  // 0  : there have been no packets of type |x|,
  // 1  : there have been packets of type |x|,
  // with |x| indicates the following packet types
  // 0 - kNoEncoding
  // 1 - kActiveNormalEncoded
  // 2 - kPassiveNormalEncoded
  // 3 - kPassiveDTXNB
  // 4 - kPassiveDTXWB
  // 5 - kPassiveDTXSWB
  void Run(std::string in_filename, int frequency, int channels,
           std::string out_filename, bool append, const int* expects);

  rtc::scoped_ptr<AudioCodingModule> acm_send_;
  rtc::scoped_ptr<AudioCodingModule> acm_receive_;
  rtc::scoped_ptr<Channel> channel_;
  rtc::scoped_ptr<ActivityMonitor> monitor_;
};

// TestWebRtcVadDtx is to verify that the WebRTC VAD/DTX perform as they should.
class TestWebRtcVadDtx final : public TestVadDtx {
 public:
  TestWebRtcVadDtx();

  void Perform() override;

 private:
  void RunTestCases();
  void Test(bool new_outfile);
  void SetVAD(bool enable_dtx, bool enable_vad, ACMVADMode vad_mode);

  bool vad_enabled_;
  bool dtx_enabled_;
  bool use_webrtc_dtx_;
  int output_file_num_;
};

// TestOpusDtx is to verify that the Opus DTX performs as it should.
class TestOpusDtx final : public TestVadDtx {
 public:
  void Perform() override;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_TESTVADDTX_H_
