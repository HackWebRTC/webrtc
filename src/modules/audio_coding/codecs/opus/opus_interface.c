/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/opus/interface/opus_interface.h"

#include <stdlib.h>
#include <string.h>

#include "opus.h"

#include "common_audio/signal_processing/resample_by_2_internal.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"

enum {
  /* We always produce 20ms frames. */
  kWebRtcOpusMaxEncodeFrameSizeMs = 20,

  /* The format allows up to 120ms frames. Since we
   * don't control the other side, we must allow
   * for packets that large. NetEq is currently
   * limited to 60 ms on the receive side.
   */
  kWebRtcOpusMaxDecodeFrameSizeMs = 120,

  /* Sample count is 48 kHz * samples per frame. */
  kWebRtcOpusMaxFrameSize = 48 * kWebRtcOpusMaxDecodeFrameSizeMs,
};

struct WebRtcOpusEncInst {
  OpusEncoder* encoder;
};

int16_t WebRtcOpus_EncoderCreate(OpusEncInst** inst, int32_t channels) {
  OpusEncInst* state;
  state = (OpusEncInst*) calloc(1, sizeof(OpusEncInst));
  if (state) {
    int error;
    state->encoder = opus_encoder_create(48000, channels, OPUS_APPLICATION_VOIP,
                                         &error);
    if (error == OPUS_OK || state->encoder != NULL ) {
      *inst = state;
      return 0;
    }
    free(state);
  }
  return -1;
}

int16_t WebRtcOpus_EncoderFree(OpusEncInst* inst) {
  opus_encoder_destroy(inst->encoder);
  return 0;
}

int16_t WebRtcOpus_Encode(OpusEncInst* inst, int16_t* audio_in, int16_t samples,
                          int16_t length_encoded_buffer, uint8_t* encoded) {
  opus_int16* audio = (opus_int16*) audio_in;
  unsigned char* coded = encoded;
  int res;

  if (samples > 48 * kWebRtcOpusMaxEncodeFrameSizeMs) {
    return -1;
  }

  res = opus_encode(inst->encoder, audio, samples, coded,
                    length_encoded_buffer);

  if (res > 0) {
    return res;
  }
  return -1;
}

int16_t WebRtcOpus_SetBitRate(OpusEncInst* inst, int32_t rate) {
  return opus_encoder_ctl(inst->encoder, OPUS_SET_BITRATE(rate));
}

struct WebRtcOpusDecInst {
  int16_t state_48_32[8];
  OpusDecoder* decoder;
};

int16_t WebRtcOpus_DecoderCreate(OpusDecInst** inst, int channels) {
  OpusDecInst* state;
  state = (OpusDecInst*) calloc(1, sizeof(OpusDecInst));
  if (state) {
    int error;
    // Always create a 48000 Hz Opus decoder.
    state->decoder = opus_decoder_create(48000, channels, &error);
    if (error == OPUS_OK && state->decoder != NULL ) {
      *inst = state;
      return 0;
    }
    free(state);
    state = NULL;
  }
  return -1;
}

int16_t WebRtcOpus_DecoderFree(OpusDecInst* inst) {
  opus_decoder_destroy(inst->decoder);
  free(inst);
  return 0;
}

int16_t WebRtcOpus_DecoderInit(OpusDecInst* inst) {
  int error = opus_decoder_ctl(inst->decoder, OPUS_RESET_STATE);
  if (error == OPUS_OK) {
    memset(inst->state_48_32, 0, sizeof(inst->state_48_32));
    return 0;
  }
  return -1;
}

static int DecodeNative(OpusDecInst* inst, int16_t* encoded,
                        int16_t encoded_bytes, int16_t* decoded,
                        int16_t* audio_type) {
  unsigned char* coded = (unsigned char*) encoded;
  opus_int16* audio = (opus_int16*) decoded;

  int res = opus_decode(inst->decoder, coded, encoded_bytes, audio,
                        kWebRtcOpusMaxFrameSize, 0);
  /* TODO(tlegrand): set to DTX for zero-length packets? */
  *audio_type = 0;

  if (res > 0) {
    return res;
  }
  return -1;
}

int16_t WebRtcOpus_Decode(OpusDecInst* inst, int16_t* encoded,
                          int16_t encoded_bytes, int16_t* decoded,
                          int16_t* audio_type) {
  /* Enough for 120 ms (the largest Opus packet size) of mono audio at 48 kHz
   * and resampler overlap. This will need to be enlarged for stereo decoding.
   */
  int16_t buffer16[kWebRtcOpusMaxFrameSize];
  int32_t buffer32[kWebRtcOpusMaxFrameSize + 7];
  int decoded_samples;
  int blocks;
  int16_t output_samples;
  int i;

  /* Decode to a temporary buffer. */
  decoded_samples = DecodeNative(inst, encoded, encoded_bytes, buffer16,
                                 audio_type);
  if (decoded_samples < 0) {
    return -1;
  }
  /* Resample from 48 kHz to 32 kHz. */
  for (i = 0; i < 7; i++) {
    buffer32[i] = inst->state_48_32[i];
    inst->state_48_32[i] = buffer16[decoded_samples -7 + i];
  }
  for (i = 0; i < decoded_samples; i++) {
    buffer32[7 + i] = buffer16[i];
  }
  /* Resampling 3 samples to 2. Function divides the input in |blocks| number
   * of 3-sample groups, and output is |blocks| number of 2-sample groups. */
  blocks = decoded_samples / 3;
  WebRtcSpl_Resample48khzTo32khz(buffer32, buffer32, blocks);
  output_samples = (int16_t) (blocks * 2);
  WebRtcSpl_VectorBitShiftW32ToW16(decoded, output_samples, buffer32, 15);

  return output_samples;
}

int16_t WebRtcOpus_DecodePlc(OpusDecInst* inst, int16_t* decoded,
                             int16_t number_of_lost_frames) {
  /* TODO(tlegrand): We can pass NULL to opus_decode to activate packet
   * loss concealment, but I don't know how many samples
   * number_of_lost_frames corresponds to. */
  return -1;
}
