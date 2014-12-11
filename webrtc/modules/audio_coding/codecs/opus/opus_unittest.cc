/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/codecs/opus/interface/opus_interface.h"
#include "webrtc/modules/audio_coding/codecs/opus/opus_inst.h"
#include "webrtc/modules/audio_coding/neteq/tools/audio_loop.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

using test::AudioLoop;

// Maximum number of bytes in output bitstream.
const size_t kMaxBytes = 1000;
// Sample rate of Opus.
const int kOpusRateKhz = 48;
// Number of samples-per-channel in a 20 ms frame, sampled at 48 kHz.
const int kOpus20msFrameSamples = kOpusRateKhz * 20;
// Number of samples-per-channel in a 10 ms frame, sampled at 48 kHz.
const int kOpus10msFrameSamples = kOpusRateKhz * 10;

class OpusTest : public ::testing::Test {
 protected:
  OpusTest();

  void TestSetMaxPlaybackRate(opus_int32 expect, int32_t set);
  void TestDtxEffect(bool dtx);

  // Prepare |speech_data_| for encoding, read from a hard-coded file.
  // After preparation, |speech_data_.GetNextBlock()| returns a pointer to a
  // block of |block_length_ms| milliseconds. The data is looped every
  // |loop_length_ms| milliseconds.
  void PrepareSpeechData(int channel, int block_length_ms, int loop_length_ms);

  int EncodeDecode(WebRtcOpusEncInst* encoder,
                   const int16_t* input_audio,
                   const int input_samples,
                   WebRtcOpusDecInst* decoder,
                   int16_t* output_audio,
                   int16_t* audio_type);

  WebRtcOpusEncInst* opus_mono_encoder_;
  WebRtcOpusEncInst* opus_stereo_encoder_;
  WebRtcOpusDecInst* opus_mono_decoder_;
  WebRtcOpusDecInst* opus_stereo_decoder_;

  AudioLoop speech_data_;
  uint8_t bitstream_[kMaxBytes];
  int encoded_bytes_;
};

OpusTest::OpusTest()
    : opus_mono_encoder_(NULL),
      opus_stereo_encoder_(NULL),
      opus_mono_decoder_(NULL),
      opus_stereo_decoder_(NULL) {
}

void OpusTest::PrepareSpeechData(int channel, int block_length_ms,
                                 int loop_length_ms) {
  const std::string file_name =
        webrtc::test::ResourcePath("audio_coding/speech_mono_32_48kHz", "pcm");
  if (loop_length_ms < block_length_ms) {
    loop_length_ms = block_length_ms;
  }
  EXPECT_TRUE(speech_data_.Init(file_name,
                                loop_length_ms * kOpusRateKhz * channel,
                                block_length_ms * kOpusRateKhz * channel));
}

void OpusTest::TestSetMaxPlaybackRate(opus_int32 expect, int32_t set) {
  opus_int32 bandwidth;
  // Test mono encoder.
  EXPECT_EQ(0, WebRtcOpus_SetMaxPlaybackRate(opus_mono_encoder_, set));
  opus_encoder_ctl(opus_mono_encoder_->encoder,
                   OPUS_GET_MAX_BANDWIDTH(&bandwidth));
  EXPECT_EQ(expect, bandwidth);
  // Test stereo encoder.
  EXPECT_EQ(0, WebRtcOpus_SetMaxPlaybackRate(opus_stereo_encoder_, set));
  opus_encoder_ctl(opus_stereo_encoder_->encoder,
                   OPUS_GET_MAX_BANDWIDTH(&bandwidth));
  EXPECT_EQ(expect, bandwidth);
}

int OpusTest::EncodeDecode(WebRtcOpusEncInst* encoder,
                           const int16_t* input_audio,
                           const int input_samples,
                           WebRtcOpusDecInst* decoder,
                           int16_t* output_audio,
                           int16_t* audio_type) {
  encoded_bytes_ = WebRtcOpus_Encode(encoder,
                                    input_audio,
                                    input_samples, kMaxBytes,
                                    bitstream_);
  return WebRtcOpus_Decode(decoder, bitstream_,
                           encoded_bytes_, output_audio,
                           audio_type);
}

// Test if encoder/decoder can enter DTX mode properly and do not enter DTX when
// they should not. This test is signal dependent.
void OpusTest::TestDtxEffect(bool dtx) {
  PrepareSpeechData(1, 20, 2000);

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_mono_encoder_, 1));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_mono_decoder_, 1));

  // Set bitrate.
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_mono_encoder_, 32000));

  // Set input audio as silence.
  int16_t silence[kOpus20msFrameSamples] = {0};

  // Setting DTX.
  EXPECT_EQ(0, dtx ? WebRtcOpus_EnableDtx(opus_mono_encoder_) :
      WebRtcOpus_DisableDtx(opus_mono_encoder_));

  int16_t audio_type;
  int16_t output_data_decode[kOpus20msFrameSamples];

  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(kOpus20msFrameSamples,
              EncodeDecode(opus_mono_encoder_, speech_data_.GetNextBlock(),
                           kOpus20msFrameSamples, opus_mono_decoder_,
                           output_data_decode, &audio_type));
    // If not DTX, it should never enter DTX mode. If DTX, we do not care since
    // whether it enters DTX depends on the signal type.
    if (!dtx) {
      EXPECT_GT(encoded_bytes_, 1);
      EXPECT_EQ(0, opus_mono_encoder_->in_dtx_mode);
      EXPECT_EQ(0, opus_mono_decoder_->in_dtx_mode);
      EXPECT_EQ(0, audio_type);  // Speech.
    }
  }

  // We input some silent segments. In DTX mode, the encoder will stop sending.
  // However, DTX may happen after a while.
  for (int i = 0; i < 22; ++i) {
    EXPECT_EQ(kOpus20msFrameSamples,
              EncodeDecode(opus_mono_encoder_, silence,
                           kOpus20msFrameSamples, opus_mono_decoder_,
                           output_data_decode, &audio_type));
    if (!dtx) {
      EXPECT_GT(encoded_bytes_, 1);
      EXPECT_EQ(0, opus_mono_encoder_->in_dtx_mode);
      EXPECT_EQ(0, opus_mono_decoder_->in_dtx_mode);
      EXPECT_EQ(0, audio_type);  // Speech.
    } else if (1 == encoded_bytes_) {
      EXPECT_EQ(1, opus_mono_encoder_->in_dtx_mode);
      EXPECT_EQ(1, opus_mono_decoder_->in_dtx_mode);
      EXPECT_EQ(2, audio_type);  // Comfort noise.
      break;
    }
  }

  // DTX mode is maintained 400 ms.
  for (int i = 0; i < 20; ++i) {
    EXPECT_EQ(kOpus20msFrameSamples,
              EncodeDecode(opus_mono_encoder_, silence,
                           kOpus20msFrameSamples, opus_mono_decoder_,
                           output_data_decode, &audio_type));
    if (dtx) {
      EXPECT_EQ(0, encoded_bytes_)  // Send 0 byte.
          << "Opus should have entered DTX mode.";
      EXPECT_EQ(1, opus_mono_encoder_->in_dtx_mode);
      EXPECT_EQ(1, opus_mono_decoder_->in_dtx_mode);
      EXPECT_EQ(2, audio_type);  // Comfort noise.
    } else {
      EXPECT_GT(encoded_bytes_, 1);
      EXPECT_EQ(0, opus_mono_encoder_->in_dtx_mode);
      EXPECT_EQ(0, opus_mono_decoder_->in_dtx_mode);
      EXPECT_EQ(0, audio_type);  // Speech.
    }
  }

  // Quit DTX after 400 ms
  EXPECT_EQ(kOpus20msFrameSamples,
            EncodeDecode(opus_mono_encoder_, silence,
                         kOpus20msFrameSamples, opus_mono_decoder_,
                         output_data_decode, &audio_type));

  EXPECT_GT(encoded_bytes_, 1);
  EXPECT_EQ(0, opus_mono_encoder_->in_dtx_mode);
  EXPECT_EQ(0, opus_mono_decoder_->in_dtx_mode);
  EXPECT_EQ(0, audio_type);  // Speech.

  // Enters DTX again immediately.
  EXPECT_EQ(kOpus20msFrameSamples,
            EncodeDecode(opus_mono_encoder_, silence,
                         kOpus20msFrameSamples, opus_mono_decoder_,
                         output_data_decode, &audio_type));
  if (dtx) {
    EXPECT_EQ(1, encoded_bytes_);  // Send 1 byte.
    EXPECT_EQ(1, opus_mono_encoder_->in_dtx_mode);
    EXPECT_EQ(1, opus_mono_decoder_->in_dtx_mode);
    EXPECT_EQ(2, audio_type);  // Comfort noise.
  } else {
    EXPECT_GT(encoded_bytes_, 1);
    EXPECT_EQ(0, opus_mono_encoder_->in_dtx_mode);
    EXPECT_EQ(0, opus_mono_decoder_->in_dtx_mode);
    EXPECT_EQ(0, audio_type);  // Speech.
  }

  silence[0] = 10000;
  if (dtx) {
    // Verify that encoder/decoder can jump out from DTX mode.
    EXPECT_EQ(kOpus20msFrameSamples,
              EncodeDecode(opus_mono_encoder_, silence,
                           kOpus20msFrameSamples, opus_mono_decoder_,
                           output_data_decode, &audio_type));
    EXPECT_GT(encoded_bytes_, 1);
    EXPECT_EQ(0, opus_mono_encoder_->in_dtx_mode);
    EXPECT_EQ(0, opus_mono_decoder_->in_dtx_mode);
    EXPECT_EQ(0, audio_type);  // Speech.
  }

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_mono_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_mono_decoder_));
}

// Test failing Create.
TEST_F(OpusTest, OpusCreateFail) {
  // Test to see that an invalid pointer is caught.
  EXPECT_EQ(-1, WebRtcOpus_EncoderCreate(NULL, 1));
  EXPECT_EQ(-1, WebRtcOpus_EncoderCreate(&opus_mono_encoder_, 3));
  EXPECT_EQ(-1, WebRtcOpus_DecoderCreate(NULL, 1));
  EXPECT_EQ(-1, WebRtcOpus_DecoderCreate(&opus_mono_decoder_, 3));
}

// Test failing Free.
TEST_F(OpusTest, OpusFreeFail) {
  // Test to see that an invalid pointer is caught.
  EXPECT_EQ(-1, WebRtcOpus_EncoderFree(NULL));
  EXPECT_EQ(-1, WebRtcOpus_DecoderFree(NULL));
}

// Test normal Create and Free.
TEST_F(OpusTest, OpusCreateFree) {
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_mono_encoder_, 1));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_mono_decoder_, 1));
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_stereo_encoder_, 2));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_stereo_decoder_, 2));
  EXPECT_TRUE(opus_mono_encoder_ != NULL);
  EXPECT_TRUE(opus_mono_decoder_ != NULL);
  EXPECT_TRUE(opus_stereo_encoder_ != NULL);
  EXPECT_TRUE(opus_stereo_decoder_ != NULL);
  // Free encoder and decoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_mono_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_mono_decoder_));
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_stereo_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_stereo_decoder_));
}

TEST_F(OpusTest, OpusEncodeDecodeMono) {
  PrepareSpeechData(1, 20, 20);

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_mono_encoder_, 1));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_mono_decoder_, 1));

  // Set bitrate.
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_mono_encoder_, 32000));

  // Check number of channels for decoder.
  EXPECT_EQ(1, WebRtcOpus_DecoderChannels(opus_mono_decoder_));

  // Encode & decode.
  int16_t audio_type;
  int16_t output_data_decode[kOpus20msFrameSamples];
  EXPECT_EQ(kOpus20msFrameSamples,
            EncodeDecode(opus_mono_encoder_, speech_data_.GetNextBlock(),
                         kOpus20msFrameSamples, opus_mono_decoder_,
                         output_data_decode, &audio_type));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_mono_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_mono_decoder_));
}

TEST_F(OpusTest, OpusEncodeDecodeStereo) {
  PrepareSpeechData(2, 20, 20);

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_stereo_encoder_, 2));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_stereo_decoder_, 2));

  // Set bitrate.
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_stereo_encoder_, 64000));

  // Check number of channels for decoder.
  EXPECT_EQ(2, WebRtcOpus_DecoderChannels(opus_stereo_decoder_));

  // Encode & decode.
  int16_t audio_type;
  int16_t output_data_decode[kOpus20msFrameSamples * 2];
  EXPECT_EQ(kOpus20msFrameSamples,
            EncodeDecode(opus_stereo_encoder_, speech_data_.GetNextBlock(),
                         kOpus20msFrameSamples, opus_stereo_decoder_,
                         output_data_decode, &audio_type));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_stereo_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_stereo_decoder_));
}

TEST_F(OpusTest, OpusSetBitRate) {
  // Test without creating encoder memory.
  EXPECT_EQ(-1, WebRtcOpus_SetBitRate(opus_mono_encoder_, 60000));
  EXPECT_EQ(-1, WebRtcOpus_SetBitRate(opus_stereo_encoder_, 60000));

  // Create encoder memory, try with different bitrates.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_mono_encoder_, 1));
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_stereo_encoder_, 2));
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_mono_encoder_, 30000));
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_stereo_encoder_, 60000));
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_mono_encoder_, 300000));
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_stereo_encoder_, 600000));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_mono_encoder_));
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_stereo_encoder_));
}

TEST_F(OpusTest, OpusSetComplexity) {
  // Test without creating encoder memory.
  EXPECT_EQ(-1, WebRtcOpus_SetComplexity(opus_mono_encoder_, 9));
  EXPECT_EQ(-1, WebRtcOpus_SetComplexity(opus_stereo_encoder_, 9));

  // Create encoder memory, try with different complexities.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_mono_encoder_, 1));
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_stereo_encoder_, 2));

  EXPECT_EQ(0, WebRtcOpus_SetComplexity(opus_mono_encoder_, 0));
  EXPECT_EQ(0, WebRtcOpus_SetComplexity(opus_stereo_encoder_, 0));
  EXPECT_EQ(0, WebRtcOpus_SetComplexity(opus_mono_encoder_, 10));
  EXPECT_EQ(0, WebRtcOpus_SetComplexity(opus_stereo_encoder_, 10));
  EXPECT_EQ(-1, WebRtcOpus_SetComplexity(opus_mono_encoder_, 11));
  EXPECT_EQ(-1, WebRtcOpus_SetComplexity(opus_stereo_encoder_, 11));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_mono_encoder_));
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_stereo_encoder_));
}

// Encode and decode one frame (stereo), initialize the decoder and
// decode once more.
TEST_F(OpusTest, OpusDecodeInit) {
  PrepareSpeechData(2, 20, 20);

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_stereo_encoder_, 2));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_stereo_decoder_, 2));

  // Encode & decode.
  int16_t audio_type;
  int16_t output_data_decode[kOpus20msFrameSamples * 2];
  EXPECT_EQ(kOpus20msFrameSamples,
            EncodeDecode(opus_stereo_encoder_, speech_data_.GetNextBlock(),
                         kOpus20msFrameSamples, opus_stereo_decoder_,
                         output_data_decode, &audio_type));

  EXPECT_EQ(0, WebRtcOpus_DecoderInit(opus_stereo_decoder_));

  EXPECT_EQ(kOpus20msFrameSamples,
            WebRtcOpus_Decode(opus_stereo_decoder_, bitstream_,
                              encoded_bytes_, output_data_decode,
                              &audio_type));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_stereo_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_stereo_decoder_));
}

TEST_F(OpusTest, OpusEnableDisableFec) {
  // Test without creating encoder memory.
  EXPECT_EQ(-1, WebRtcOpus_EnableFec(opus_mono_encoder_));
  EXPECT_EQ(-1, WebRtcOpus_DisableFec(opus_stereo_encoder_));

  // Create encoder memory, try with different bitrates.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_mono_encoder_, 1));
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_stereo_encoder_, 2));

  EXPECT_EQ(0, WebRtcOpus_EnableFec(opus_mono_encoder_));
  EXPECT_EQ(0, WebRtcOpus_EnableFec(opus_stereo_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DisableFec(opus_mono_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DisableFec(opus_stereo_encoder_));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_mono_encoder_));
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_stereo_encoder_));
}

TEST_F(OpusTest, OpusEnableDisableDtx) {
  // Test without creating encoder memory.
  EXPECT_EQ(-1, WebRtcOpus_EnableDtx(opus_mono_encoder_));
  EXPECT_EQ(-1, WebRtcOpus_DisableDtx(opus_stereo_encoder_));

  // Create encoder memory, try with different bitrates.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_mono_encoder_, 1));
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_stereo_encoder_, 2));

  opus_int32 dtx;

  // DTX is off by default.
  opus_encoder_ctl(opus_mono_encoder_->encoder,
                   OPUS_GET_DTX(&dtx));
  EXPECT_EQ(0, dtx);

  opus_encoder_ctl(opus_stereo_encoder_->encoder,
                   OPUS_GET_DTX(&dtx));
  EXPECT_EQ(0, dtx);

  // Test to enable DTX.
  EXPECT_EQ(0, WebRtcOpus_EnableDtx(opus_mono_encoder_));
  opus_encoder_ctl(opus_mono_encoder_->encoder,
                   OPUS_GET_DTX(&dtx));
  EXPECT_EQ(1, dtx);

  EXPECT_EQ(0, WebRtcOpus_EnableDtx(opus_stereo_encoder_));
  opus_encoder_ctl(opus_stereo_encoder_->encoder,
                   OPUS_GET_DTX(&dtx));
  EXPECT_EQ(1, dtx);

  // Test to disable DTX.
  EXPECT_EQ(0, WebRtcOpus_DisableDtx(opus_mono_encoder_));
  opus_encoder_ctl(opus_mono_encoder_->encoder,
                   OPUS_GET_DTX(&dtx));
  EXPECT_EQ(0, dtx);

  EXPECT_EQ(0, WebRtcOpus_DisableDtx(opus_stereo_encoder_));
  opus_encoder_ctl(opus_stereo_encoder_->encoder,
                   OPUS_GET_DTX(&dtx));
  EXPECT_EQ(0, dtx);

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_mono_encoder_));
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_stereo_encoder_));
}

TEST_F(OpusTest, OpusDtxOff) {
  TestDtxEffect(false);
}

TEST_F(OpusTest, OpusDtxOn) {
  TestDtxEffect(true);
}

TEST_F(OpusTest, OpusSetPacketLossRate) {
  // Test without creating encoder memory.
  EXPECT_EQ(-1, WebRtcOpus_SetPacketLossRate(opus_mono_encoder_, 50));
  EXPECT_EQ(-1, WebRtcOpus_SetPacketLossRate(opus_stereo_encoder_, 50));

  // Create encoder memory, try with different bitrates.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_mono_encoder_, 1));
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_stereo_encoder_, 2));

  EXPECT_EQ(0, WebRtcOpus_SetPacketLossRate(opus_mono_encoder_, 50));
  EXPECT_EQ(0, WebRtcOpus_SetPacketLossRate(opus_stereo_encoder_, 50));
  EXPECT_EQ(-1, WebRtcOpus_SetPacketLossRate(opus_mono_encoder_, -1));
  EXPECT_EQ(-1, WebRtcOpus_SetPacketLossRate(opus_stereo_encoder_, -1));
  EXPECT_EQ(-1, WebRtcOpus_SetPacketLossRate(opus_mono_encoder_, 101));
  EXPECT_EQ(-1, WebRtcOpus_SetPacketLossRate(opus_stereo_encoder_, 101));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_mono_encoder_));
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_stereo_encoder_));
}

TEST_F(OpusTest, OpusSetMaxPlaybackRate) {
  // Test without creating encoder memory.
  EXPECT_EQ(-1, WebRtcOpus_SetMaxPlaybackRate(opus_mono_encoder_, 20000));
  EXPECT_EQ(-1, WebRtcOpus_SetMaxPlaybackRate(opus_stereo_encoder_, 20000));

  // Create encoder memory, try with different bitrates.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_mono_encoder_, 1));
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_stereo_encoder_, 2));

  TestSetMaxPlaybackRate(OPUS_BANDWIDTH_FULLBAND, 48000);
  TestSetMaxPlaybackRate(OPUS_BANDWIDTH_FULLBAND, 24001);
  TestSetMaxPlaybackRate(OPUS_BANDWIDTH_SUPERWIDEBAND, 24000);
  TestSetMaxPlaybackRate(OPUS_BANDWIDTH_SUPERWIDEBAND, 16001);
  TestSetMaxPlaybackRate(OPUS_BANDWIDTH_WIDEBAND, 16000);
  TestSetMaxPlaybackRate(OPUS_BANDWIDTH_WIDEBAND, 12001);
  TestSetMaxPlaybackRate(OPUS_BANDWIDTH_MEDIUMBAND, 12000);
  TestSetMaxPlaybackRate(OPUS_BANDWIDTH_MEDIUMBAND, 8001);
  TestSetMaxPlaybackRate(OPUS_BANDWIDTH_NARROWBAND, 8000);
  TestSetMaxPlaybackRate(OPUS_BANDWIDTH_NARROWBAND, 4000);

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_mono_encoder_));
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_stereo_encoder_));
}

// PLC in mono mode.
TEST_F(OpusTest, OpusDecodePlcMono) {
  PrepareSpeechData(1, 20, 20);

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_mono_encoder_, 1));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_mono_decoder_, 1));

  // Set bitrate.
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_mono_encoder_, 32000));

  // Check number of channels for decoder.
  EXPECT_EQ(1, WebRtcOpus_DecoderChannels(opus_mono_decoder_));

  // Encode & decode.
  int16_t audio_type;
  int16_t output_data_decode[kOpus20msFrameSamples];
  EXPECT_EQ(kOpus20msFrameSamples,
            EncodeDecode(opus_mono_encoder_, speech_data_.GetNextBlock(),
                         kOpus20msFrameSamples, opus_mono_decoder_,
                         output_data_decode, &audio_type));

  // Call decoder PLC.
  int16_t plc_buffer[kOpus20msFrameSamples];
  EXPECT_EQ(kOpus20msFrameSamples,
            WebRtcOpus_DecodePlc(opus_mono_decoder_, plc_buffer, 1));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_mono_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_mono_decoder_));
}

// PLC in stereo mode.
TEST_F(OpusTest, OpusDecodePlcStereo) {
  PrepareSpeechData(2, 20, 20);

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_stereo_encoder_, 2));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_stereo_decoder_, 2));

  // Set bitrate.
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_stereo_encoder_, 64000));

  // Check number of channels for decoder.
  EXPECT_EQ(2, WebRtcOpus_DecoderChannels(opus_stereo_decoder_));

  // Encode & decode.
  int16_t audio_type;
  int16_t output_data_decode[kOpus20msFrameSamples * 2];
  EXPECT_EQ(kOpus20msFrameSamples,
            EncodeDecode(opus_stereo_encoder_, speech_data_.GetNextBlock(),
                         kOpus20msFrameSamples, opus_stereo_decoder_,
                         output_data_decode, &audio_type));

  // Call decoder PLC.
  int16_t plc_buffer[kOpus20msFrameSamples * 2];
  EXPECT_EQ(kOpus20msFrameSamples,
            WebRtcOpus_DecodePlc(opus_stereo_decoder_, plc_buffer, 1));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_stereo_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_stereo_decoder_));
}

// Duration estimation.
TEST_F(OpusTest, OpusDurationEstimation) {
  PrepareSpeechData(2, 20, 20);

  // Create.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_stereo_encoder_, 2));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_stereo_decoder_, 2));

  // 10 ms. We use only first 10 ms of a 20 ms block.
  encoded_bytes_ = WebRtcOpus_Encode(opus_stereo_encoder_,
                                     speech_data_.GetNextBlock(),
                                     kOpus10msFrameSamples, kMaxBytes,
                                     bitstream_);
  EXPECT_EQ(kOpus10msFrameSamples,
            WebRtcOpus_DurationEst(opus_stereo_decoder_, bitstream_,
                                   encoded_bytes_));

  // 20 ms
  encoded_bytes_ = WebRtcOpus_Encode(opus_stereo_encoder_,
                                     speech_data_.GetNextBlock(),
                                     kOpus20msFrameSamples, kMaxBytes,
                                     bitstream_);
  EXPECT_EQ(kOpus20msFrameSamples,
            WebRtcOpus_DurationEst(opus_stereo_decoder_, bitstream_,
                                   encoded_bytes_));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_stereo_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_stereo_decoder_));
}

}  // namespace webrtc
