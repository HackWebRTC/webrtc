/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_MOCK_MOCK_AUDIO_ENCODER_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_MOCK_MOCK_AUDIO_ENCODER_H_

#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace webrtc {

class MockAudioEncoder : public AudioEncoder {
 public:
  virtual ~MockAudioEncoder() { Die(); }
  MOCK_METHOD0(Die, void());
  MOCK_CONST_METHOD0(SampleRateHz, int());
  MOCK_CONST_METHOD0(NumChannels, int());
  MOCK_CONST_METHOD0(MaxEncodedBytes, size_t());
  MOCK_CONST_METHOD0(Num10MsFramesInNextPacket, size_t());
  MOCK_CONST_METHOD0(Max10MsFramesInAPacket, size_t());
  MOCK_CONST_METHOD0(GetTargetBitrate, int());
  MOCK_METHOD1(SetTargetBitrate, void(int));
  MOCK_METHOD1(SetProjectedPacketLossRate, void(double));
  // Note, we explicitly chose not to create a mock for the Encode method.
  MOCK_METHOD4(EncodeInternal,
               EncodedInfo(uint32_t timestamp,
                           const int16_t* audio,
                           size_t max_encoded_bytes,
                           uint8_t* encoded));
};

class MockAudioEncoderMutable : public AudioEncoderMutable {
 public:
  MOCK_CONST_METHOD0(SampleRateHz, int());
  MOCK_CONST_METHOD0(NumChannels, int());
  MOCK_CONST_METHOD0(MaxEncodedBytes, size_t());
  MOCK_CONST_METHOD0(Num10MsFramesInNextPacket, size_t());
  MOCK_CONST_METHOD0(Max10MsFramesInAPacket, size_t());
  MOCK_CONST_METHOD0(GetTargetBitrate, int());
  MOCK_METHOD1(SetTargetBitrate, void(int));
  MOCK_METHOD1(SetProjectedPacketLossRate, void(double));
  // Note, we explicitly chose not to create a mock for the Encode method.
  MOCK_METHOD4(EncodeInternal,
               EncodedInfo(uint32_t timestamp,
                           const int16_t* audio,
                           size_t max_encoded_bytes,
                           uint8_t* encoded));

  MOCK_METHOD0(Reset, void());
  MOCK_METHOD1(SetFec, bool(bool enable));
  MOCK_METHOD1(SetDtx, bool(bool enable));
  MOCK_METHOD1(SetApplication, bool(Application application));
  MOCK_METHOD1(SetMaxPayloadSize, void(int max_payload_size_bytes));
  MOCK_METHOD1(SetMaxRate, void(int max_rate_bps));
  MOCK_METHOD1(SetMaxPlaybackRate, bool(int frequency_hz));
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_MOCK_MOCK_AUDIO_ENCODER_H_
