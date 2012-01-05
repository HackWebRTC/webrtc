/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file includes the implementation of the internal filterbank associated functions.
 * For function description, see vad_filterbank.h.
 */

#include "vad_filterbank.h"

#include "signal_processing_library.h"
#include "typedefs.h"
#include "vad_defines.h"

// Constant 160*log10(2) in Q9
static const int16_t kLogConst = 24660;

// Coefficients used by WebRtcVad_HpOutput, Q14
static const int16_t kHpZeroCoefs[3] = { 6631, -13262, 6631 };
static const int16_t kHpPoleCoefs[3] = { 16384, -7756, 5620 };

// Allpass filter coefficients, upper and lower, in Q15
// Upper: 0.64, Lower: 0.17
static const int16_t kAllPassCoefsQ15[2] = { 20972, 5571 };

// Adjustment for division with two in WebRtcVad_SplitFilter
static const int16_t kOffsetVector[6] = { 368, 368, 272, 176, 176, 176 };

void WebRtcVad_HpOutput(int16_t* in_vector,
                        int in_vector_length,
                        int16_t* filter_state,
                        int16_t* out_vector) {
  int i;
  int16_t* in_ptr = in_vector;
  int16_t* out_ptr = out_vector;
  int32_t tmp32 = 0;


  // The sum of the absolute values of the impulse response:
  // The zero/pole-filter has a max amplification of a single sample of: 1.4546
  // Impulse response: 0.4047 -0.6179 -0.0266  0.1993  0.1035  -0.0194
  // The all-zero section has a max amplification of a single sample of: 1.6189
  // Impulse response: 0.4047 -0.8094  0.4047  0       0        0
  // The all-pole section has a max amplification of a single sample of: 1.9931
  // Impulse response: 1.0000  0.4734 -0.1189 -0.2187 -0.0627   0.04532

  for (i = 0; i < in_vector_length; i++) {
    // all-zero section (filter coefficients in Q14)
    tmp32 = (int32_t) WEBRTC_SPL_MUL_16_16(kHpZeroCoefs[0], (*in_ptr));
    tmp32 += (int32_t) WEBRTC_SPL_MUL_16_16(kHpZeroCoefs[1], filter_state[0]);
    tmp32 += (int32_t) WEBRTC_SPL_MUL_16_16(kHpZeroCoefs[2],
                                            filter_state[1]);  // Q14
    filter_state[1] = filter_state[0];
    filter_state[0] = *in_ptr++;

    // all-pole section
    tmp32 -= (int32_t) WEBRTC_SPL_MUL_16_16(kHpPoleCoefs[1],
                                            filter_state[2]);  // Q14
    tmp32 -= (int32_t) WEBRTC_SPL_MUL_16_16(kHpPoleCoefs[2], filter_state[3]);
    filter_state[3] = filter_state[2];
    filter_state[2] = (int16_t) WEBRTC_SPL_RSHIFT_W32 (tmp32, 14);
    *out_ptr++ = filter_state[2];
  }
}

void WebRtcVad_Allpass(int16_t* in_vector,
                       int16_t filter_coefficients,
                       int vector_length,
                       int16_t* filter_state,
                       int16_t* out_vector) {
  // The filter can only cause overflow (in the w16 output variable)
  // if more than 4 consecutive input numbers are of maximum value and
  // has the the same sign as the impulse responses first taps.
  // First 6 taps of the impulse response: 0.6399 0.5905 -0.3779
  // 0.2418 -0.1547 0.0990

  int i;
  int16_t tmp16 = 0;
  int32_t tmp32 = 0, in32 = 0;
  int32_t state32 = WEBRTC_SPL_LSHIFT_W32((int32_t) (*filter_state), 16); // Q31

  for (i = 0; i < vector_length; i++) {
    tmp32 = state32 + WEBRTC_SPL_MUL_16_16(filter_coefficients, (*in_vector));
    tmp16 = (int16_t) WEBRTC_SPL_RSHIFT_W32(tmp32, 16);
    *out_vector++ = tmp16;
    in32 = WEBRTC_SPL_LSHIFT_W32(((int32_t) (*in_vector)), 14);
    state32 = in32 - WEBRTC_SPL_MUL_16_16(filter_coefficients, tmp16);
    state32 = WEBRTC_SPL_LSHIFT_W32(state32, 1);
    in_vector += 2;
  }

  *filter_state = (int16_t) WEBRTC_SPL_RSHIFT_W32(state32, 16);
}

void WebRtcVad_SplitFilter(int16_t* in_vector,
                           int in_vector_length,
                           int16_t* upper_state,
                           int16_t* lower_state,
                           int16_t* out_vector_hp,
                           int16_t* out_vector_lp) {
  int16_t tmp_out;
  int i;
  int half_length = WEBRTC_SPL_RSHIFT_W16(in_vector_length, 1);

  // All-pass filtering upper branch
  WebRtcVad_Allpass(&in_vector[0], kAllPassCoefsQ15[0], half_length,
                    upper_state, out_vector_hp);

  // All-pass filtering lower branch
  WebRtcVad_Allpass(&in_vector[1], kAllPassCoefsQ15[1], half_length,
                    lower_state, out_vector_lp);

  // Make LP and HP signals
  for (i = 0; i < half_length; i++) {
    tmp_out = *out_vector_hp;
    *out_vector_hp++ -= *out_vector_lp;
    *out_vector_lp++ += tmp_out;
  }
}

int16_t WebRtcVad_get_features(VadInstT* inst,
                               int16_t* in_vector,
                               int frame_size,
                               int16_t* out_vector) {
  int16_t power = 0;
  // We expect |frame_size| to be 80, 160 or 240 samples, which corresponds to
  // 10, 20 or 30 ms in 8 kHz. Therefore, the intermediate downsampled data will
  // have at most 120 samples after the first split and at most 60 samples after
  // the second split.
  int16_t hp_120[120], lp_120[120];
  int16_t hp_60[60], lp_60[60];
  // Initialize variables for the first SplitFilter().
  int length = frame_size;
  int frequency_band = 0;
  int16_t* in_ptr = in_vector;
  int16_t* hp_out_ptr = hp_120;
  int16_t* lp_out_ptr = lp_120;

  // Split at 2000 Hz and downsample
  WebRtcVad_SplitFilter(in_ptr, length, &inst->upper_state[frequency_band],
                        &inst->lower_state[frequency_band], hp_out_ptr,
                        lp_out_ptr);

  // Split at 3000 Hz and downsample
  frequency_band = 1;
  in_ptr = hp_120;
  hp_out_ptr = hp_60;
  lp_out_ptr = lp_60;
  length = WEBRTC_SPL_RSHIFT_W16(frame_size, 1);

  WebRtcVad_SplitFilter(in_ptr, length, &inst->upper_state[frequency_band],
                        &inst->lower_state[frequency_band], hp_out_ptr,
                        lp_out_ptr);

  // Energy in 3000 Hz - 4000 Hz
  length = WEBRTC_SPL_RSHIFT_W16(length, 1);
  WebRtcVad_LogOfEnergy(hp_60, length, kOffsetVector[5], &power,
                        &out_vector[5]);

  // Energy in 2000 Hz - 3000 Hz
  WebRtcVad_LogOfEnergy(lp_60, length, kOffsetVector[4], &power,
                        &out_vector[4]);

  // Split at 1000 Hz and downsample
  frequency_band = 2;
  in_ptr = lp_120;
  hp_out_ptr = hp_60;
  lp_out_ptr = lp_60;
  length = WEBRTC_SPL_RSHIFT_W16(frame_size, 1);
  WebRtcVad_SplitFilter(in_ptr, length, &inst->upper_state[frequency_band],
                        &inst->lower_state[frequency_band], hp_out_ptr,
                        lp_out_ptr);

  // Energy in 1000 Hz - 2000 Hz
  length = WEBRTC_SPL_RSHIFT_W16(length, 1);
  WebRtcVad_LogOfEnergy(hp_60, length, kOffsetVector[3], &power,
                        &out_vector[3]);

  // Split at 500 Hz
  frequency_band = 3;
  in_ptr = lp_60;
  hp_out_ptr = hp_120;
  lp_out_ptr = lp_120;

  WebRtcVad_SplitFilter(in_ptr, length, &inst->upper_state[frequency_band],
                        &inst->lower_state[frequency_band], hp_out_ptr,
                        lp_out_ptr);

  // Energy in 500 Hz - 1000 Hz
  length = WEBRTC_SPL_RSHIFT_W16(length, 1);
  WebRtcVad_LogOfEnergy(hp_120, length, kOffsetVector[2], &power,
                        &out_vector[2]);

  // Split at 250 Hz
  frequency_band = 4;
  in_ptr = lp_120;
  hp_out_ptr = hp_60;
  lp_out_ptr = lp_60;

  WebRtcVad_SplitFilter(in_ptr, length, &inst->upper_state[frequency_band],
                        &inst->lower_state[frequency_band], hp_out_ptr,
                        lp_out_ptr);

  // Energy in 250 Hz - 500 Hz
  length = WEBRTC_SPL_RSHIFT_W16(length, 1);
  WebRtcVad_LogOfEnergy(hp_60, length, kOffsetVector[1], &power,
                        &out_vector[1]);

  // Remove DC and LFs
  WebRtcVad_HpOutput(lp_60, length, inst->hp_filter_state, hp_120);

  // Power in 80 Hz - 250 Hz
  WebRtcVad_LogOfEnergy(hp_120, length, kOffsetVector[0], &power,
                        &out_vector[0]);

  return power;
}

void WebRtcVad_LogOfEnergy(int16_t* vector,
                           int vector_length,
                           int16_t offset,
                           int16_t* power,
                           int16_t* log_energy) {
  int shfts = 0, shfts2 = 0;
  int16_t energy_s16 = 0;
  int16_t zeros = 0, frac = 0, log2 = 0;
  int32_t energy = WebRtcSpl_Energy(vector, vector_length, &shfts);

  if (energy > 0) {

    shfts2 = 16 - WebRtcSpl_NormW32(energy);
    shfts += shfts2;
    // "shfts" is the total number of right shifts that has been done to
    // energy_s16.
    energy_s16 = (int16_t) WEBRTC_SPL_SHIFT_W32(energy, -shfts2);

    // Find:
    // 160*log10(energy_s16*2^shfts) = 160*log10(2)*log2(energy_s16*2^shfts) =
    // 160*log10(2)*(log2(energy_s16) + log2(2^shfts)) =
    // 160*log10(2)*(log2(energy_s16) + shfts)

    zeros = WebRtcSpl_NormU32(energy_s16);
    frac = (int16_t) (((uint32_t) ((int32_t) (energy_s16) << zeros)
        & 0x7FFFFFFF) >> 21);
    log2 = (int16_t) (((31 - zeros) << 10) + frac);

    *log_energy = (int16_t) WEBRTC_SPL_MUL_16_16_RSFT(kLogConst, log2, 19)
        + (int16_t) WEBRTC_SPL_MUL_16_16_RSFT(shfts, kLogConst, 9);

    if (*log_energy < 0) {
      *log_energy = 0;
    }
  } else {
    *log_energy = 0;
    shfts = -15;
    energy_s16 = 0;
  }

  *log_energy += offset;

  // Total power in frame
  if (*power <= MIN_ENERGY) {
    if (shfts > 0) {
      *power += MIN_ENERGY + 1;
    } else if (WEBRTC_SPL_SHIFT_W16(energy_s16, shfts) > MIN_ENERGY) {
      *power += MIN_ENERGY + 1;
    } else {
      *power += WEBRTC_SPL_SHIFT_W16(energy_s16, shfts);
    }
  }
}
