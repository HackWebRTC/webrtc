/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/codecs/ilbc/audio_decoder_ilbc.h"
#include "webrtc/modules/audio_coding/codecs/ilbc/audio_encoder_ilbc.h"

namespace webrtc {

TEST(IlbcTest, BadPacket) {
  // Get a good packet.
  AudioEncoderIlbc::Config config;
  config.frame_size_ms = 20;  // We need 20 ms rather than the default 30 ms;
                              // otherwise, all possible values of cb_index[2]
                              // are valid.
  AudioEncoderIlbc encoder(config);
  std::vector<int16_t> samples(encoder.SampleRateHz() / 100, 4711);
  rtc::Buffer packet;
  int num_10ms_chunks = 0;
  while (packet.size() == 0) {
    encoder.Encode(0, samples, &packet);
    num_10ms_chunks += 1;
  }

  // Break the packet by setting all bits of the unsigned 7-bit number
  // cb_index[2] to 1, giving it a value of 127. For a 20 ms packet, this is
  // too large.
  EXPECT_EQ(38u, packet.size());
  rtc::Buffer bad_packet(packet.data(), packet.size());
  bad_packet[29] |= 0x3f;  // Bits 1-6.
  bad_packet[30] |= 0x80;  // Bit 0.

  // Decode the bad packet. We expect the decoder to respond by returning -1.
  AudioDecoderIlbc decoder;
  std::vector<int16_t> decoded_samples(num_10ms_chunks * samples.size());
  AudioDecoder::SpeechType speech_type;
  EXPECT_EQ(-1, decoder.Decode(bad_packet.data(), bad_packet.size(),
                               encoder.SampleRateHz(),
                               sizeof(int16_t) * decoded_samples.size(),
                               decoded_samples.data(), &speech_type));

  // Decode the good packet. This should work, because the failed decoding
  // should not have left the decoder in a broken state.
  EXPECT_EQ(static_cast<int>(decoded_samples.size()),
            decoder.Decode(packet.data(), packet.size(), encoder.SampleRateHz(),
                           sizeof(int16_t) * decoded_samples.size(),
                           decoded_samples.data(), &speech_type));
}

}  // namespace webrtc
