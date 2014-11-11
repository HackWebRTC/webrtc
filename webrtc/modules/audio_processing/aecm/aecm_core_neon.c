/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aecm/aecm_core.h"

#include <arm_neon.h>
#include <assert.h>

#include "webrtc/common_audio/signal_processing/include/real_fft.h"

// TODO(kma): Re-write the corresponding assembly file, the offset
// generating script and makefile, to replace these C functions.

// Square root of Hanning window in Q14.
const ALIGN8_BEG int16_t WebRtcAecm_kSqrtHanning[] ALIGN8_END = {
  0,
  399, 798, 1196, 1594, 1990, 2386, 2780, 3172,
  3562, 3951, 4337, 4720, 5101, 5478, 5853, 6224,
  6591, 6954, 7313, 7668, 8019, 8364, 8705, 9040,
  9370, 9695, 10013, 10326, 10633, 10933, 11227, 11514,
  11795, 12068, 12335, 12594, 12845, 13089, 13325, 13553,
  13773, 13985, 14189, 14384, 14571, 14749, 14918, 15079,
  15231, 15373, 15506, 15631, 15746, 15851, 15947, 16034,
  16111, 16179, 16237, 16286, 16325, 16354, 16373, 16384
};

// Square root of Hanning window in Q14, in reversed order.
static const ALIGN8_BEG int16_t kSqrtHanningReversed[] ALIGN8_END = {
  16384, 16373, 16354, 16325, 16286, 16237, 16179, 16111,
  16034, 15947, 15851, 15746, 15631, 15506, 15373, 15231,
  15079, 14918, 14749, 14571, 14384, 14189, 13985, 13773,
  13553, 13325, 13089, 12845, 12594, 12335, 12068, 11795,
  11514, 11227, 10933, 10633, 10326, 10013, 9695,  9370,
  9040,  8705,  8364,  8019, 7668,  7313,  6954,  6591,
  6224,  5853,  5478,  5101, 4720,  4337,  3951,  3562,
  3172,  2780,  2386,  1990, 1594,  1196,  798,   399
};

void WebRtcAecm_WindowAndFFTNeon(AecmCore_t* aecm,
                                 int16_t* fft,
                                 const int16_t* time_signal,
                                 complex16_t* freq_signal,
                                 int time_signal_scaling) {
  int i = 0;
  const int16_t* p_time_signal = time_signal;
  const int16_t* p_time_signal_offset = &time_signal[PART_LEN];
  const int16_t* p_hanning = WebRtcAecm_kSqrtHanning;
  const int16_t* p_hanning_reversed = kSqrtHanningReversed;
  int16_t* p_fft = fft;
  int16_t* p_fft_offset = &fft[PART_LEN2];

  assert((uintptr_t)p_time_signal % 8 == 0);
  assert((uintptr_t)freq_signal % 32 == 0);
  assert((uintptr_t)p_hanning % 8 == 0);
  assert((uintptr_t)p_fft % 16 == 0);

  __asm __volatile(
    "vdup.16 d16, %0\n\t"
    "vmov.i16 d21, #0\n\t"
    "vmov.i16 d27, #0\n\t"
    :
    :"r"(time_signal_scaling)
    : "d16", "d21", "d27"
  );

  for (i = 0; i < PART_LEN; i += 4) {
    __asm __volatile(
      "vld1.16 d0, [%[p_time_signal], :64]!\n\t"
      "vld1.16 d22, [%[p_time_signal_offset], :64]!\n\t"
      "vld1.16 d17, [%[p_hanning], :64]!\n\t"
      "vld1.16 d23, [%[p_hanning_reversed], :64]!\n\t"
      "vshl.s16  d18, d0, d16\n\t"
      "vshl.s16  d22, d22, d16\n\t"
      "vmull.s16 q9, d18, d17\n\t"
      "vmull.s16 q12, d22, d23\n\t"
      "vshrn.i32 d20, q9, #14\n\t"
      "vshrn.i32 d26, q12, #14\n\t"
      "vst2.16 {d20, d21}, [%[p_fft], :128]!\n\t"
      "vst2.16 {d26, d27}, [%[p_fft_offset], :128]!\n\t"
      :[p_time_signal]"+r"(p_time_signal),
       [p_time_signal_offset]"+r"(p_time_signal_offset),
       [p_hanning]"+r"(p_hanning),
       [p_hanning_reversed]"+r"(p_hanning_reversed),
       [p_fft]"+r"(p_fft),
       [p_fft_offset]"+r"(p_fft_offset)
      :
      :"d0", "d16", "d17", "d18", "d19", "d20", "d21",
       "d22", "d23", "d24", "d25", "d26", "d27"
    );
  }

  // Do forward FFT, then take only the first PART_LEN complex samples,
  // and change signs of the imaginary parts.
  WebRtcSpl_RealForwardFFT(aecm->real_fft, (int16_t*)fft,
                           (int16_t*)freq_signal);

  for (i = 0; i < PART_LEN; i += 8) {
    __asm __volatile(
      "vld2.16 {d20, d21, d22, d23}, [%[freq_signal], :256]\n\t"
      "vneg.s16 d22, d22\n\t"
      "vneg.s16 d23, d23\n\t"
      "vst2.16 {d20, d21, d22, d23}, [%[freq_signal], :256]!\n\t"
      :[freq_signal]"+r"(freq_signal)
      :
      : "d20", "d21", "d22", "d23"
    );
  }
}

void WebRtcAecm_InverseFFTAndWindowNeon(AecmCore_t* aecm,
                                        int16_t* fft,
                                        complex16_t* efw,
                                        int16_t* output,
                                        const int16_t* nearendClean) {
  int i, j, outCFFT;

  assert((uintptr_t)efw % 32 == 0);
  assert((uintptr_t)fft % 16 == 0);
  assert((uintptr_t)output% 8 == 0);
  assert((uintptr_t)WebRtcAecm_kSqrtHanning % 8 == 0);
  assert((uintptr_t)kSqrtHanningReversed % 8 == 0);
  assert((uintptr_t)(aecm->outBuf) % 8 == 0);
  assert((uintptr_t)(aecm->xBuf) % 32 == 0);
  assert((uintptr_t)(aecm->dBufNoisy) % 32 == 0);
  assert((uintptr_t)(aecm->dBufClean) % 32 == 0);

  // Synthesis
  complex16_t* p_efw = efw;
  int16_t* p_fft = fft;
  int16_t* p_fft_offset = &fft[PART_LEN4 - 6];

  for (i = 0, j = 0; i < PART_LEN; i += 4, j += 8) {
    // We overwrite two more elements in fft[], but it's ok.
    __asm __volatile(
      "vld2.16 {q10}, [%[p_efw], :128]!\n\t"
      "vmov q11, q10\n\t"
      "vneg.s16 d23, d23\n\t"
      "vst2.16 {d22, d23}, [%[p_fft], :128]!\n\t"
      "vrev64.16 q10, q10\n\t"
      "vst2.16 {q10}, [%[p_fft_offset]], %[offset]\n\t"
      :[p_efw]"+r"(p_efw),
       [p_fft]"+r"(p_fft),
       [p_fft_offset]"+r"(p_fft_offset)
      :[offset]"r"(-16)
      :"d20", "d21", "d22", "d23"
    );
  }

  fft[PART_LEN2] = efw[PART_LEN].real;
  fft[PART_LEN2 + 1] = -efw[PART_LEN].imag;

  // Inverse FFT. Then take only the real values, and keep outCFFT
  // to scale the samples.
  outCFFT = WebRtcSpl_RealInverseFFT(aecm->real_fft, fft, (int16_t*)efw);

  int32x4_t tmp32x4_2;
  __asm __volatile("vdup.32 %q0, %1" : "=w"(tmp32x4_2) : "r"((int32_t)
      (outCFFT - aecm->dfaCleanQDomain)));
  for (i = 0; i < PART_LEN; i += 4) {
    int16x4_t tmp16x4_0;
    int16x4_t tmp16x4_1;
    int32x4_t tmp32x4_0;
    int32x4_t tmp32x4_1;

    //efw[i].real = (int16_t)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(
    //              efw[i].real, WebRtcAecm_kSqrtHanning[i], 14);
    __asm __volatile("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_0) : "r"(&efw[i].real));
    __asm __volatile("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_1) : "r"(&WebRtcAecm_kSqrtHanning[i]));
    __asm __volatile("vmull.s16 %q0, %P1, %P2" : "=w"(tmp32x4_0) : "w"(tmp16x4_0), "w"(tmp16x4_1));
    __asm __volatile("vrshr.s32 %q0, %q1, #14" : "=w"(tmp32x4_0) : "0"(tmp32x4_0));

    //tmp32no1 = WEBRTC_SPL_SHIFT_W32((int32_t)efw[i].real,
    //        outCFFT - aecm->dfaCleanQDomain);
    __asm __volatile("vshl.s32 %q0, %q1, %q2" : "=w"(tmp32x4_0) : "0"(tmp32x4_0), "w"(tmp32x4_2));

    //efw[i].real = (int16_t)WEBRTC_SPL_SAT(WEBRTC_SPL_WORD16_MAX,
    //        tmp32no1 + aecm->outBuf[i], WEBRTC_SPL_WORD16_MIN);
    // output[i] = efw[i].real;
    __asm __volatile("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_0) : "r"(&aecm->outBuf[i]));
    __asm __volatile("vmovl.s16 %q0, %P1" : "=w"(tmp32x4_1) : "w"(tmp16x4_0));
    __asm __volatile("vadd.i32 %q0, %q1" : : "w"(tmp32x4_0), "w"(tmp32x4_1));
    __asm __volatile("vqmovn.s32 %P0, %q1" : "=w"(tmp16x4_0) : "w"(tmp32x4_0));
    __asm __volatile("vst1.16 %P0, [%1, :64]" : : "w"(tmp16x4_0), "r"(&efw[i].real));
    __asm __volatile("vst1.16 %P0, [%1, :64]" : : "w"(tmp16x4_0), "r"(&output[i]));

    // tmp32no1 = WEBRTC_SPL_MUL_16_16_RSFT(
    //        efw[PART_LEN + i].real, WebRtcAecm_kSqrtHanning[PART_LEN - i], 14);
    __asm __volatile("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_0) : "r"(&efw[PART_LEN + i].real));
    __asm __volatile("vld1.16 %P0, [%1, :64]" : "=w"(tmp16x4_1) : "r"(&kSqrtHanningReversed[i]));
    __asm __volatile("vmull.s16 %q0, %P1, %P2" : "=w"(tmp32x4_0) : "w"(tmp16x4_0), "w"(tmp16x4_1));
    __asm __volatile("vshr.s32 %q0, %q1, #14" : "=w"(tmp32x4_0) : "0"(tmp32x4_0));

    // tmp32no1 = WEBRTC_SPL_SHIFT_W32(tmp32no1, outCFFT - aecm->dfaCleanQDomain);
    __asm __volatile("vshl.s32 %q0, %q1, %q2" : "=w"(tmp32x4_0) : "0"(tmp32x4_0), "w"(tmp32x4_2));
    // aecm->outBuf[i] = (int16_t)WEBRTC_SPL_SAT(
    //    WEBRTC_SPL_WORD16_MAX, tmp32no1, WEBRTC_SPL_WORD16_MIN);
    __asm __volatile("vqmovn.s32 %P0, %q1" : "=w"(tmp16x4_0) : "w"(tmp32x4_0));
    __asm __volatile("vst1.16 %P0, [%1, :64]" : : "w"(tmp16x4_0), "r"(&aecm->outBuf[i]));
  }

  // Copy the current block to the old position (outBuf is shifted elsewhere).
  for (i = 0; i < PART_LEN; i += 16) {
    __asm __volatile("vld1.16 {d20, d21, d22, d23}, [%0, :256]" : :
            "r"(&aecm->xBuf[i + PART_LEN]) : "q10");
    __asm __volatile("vst1.16 {d20, d21, d22, d23}, [%0, :256]" : : "r"(&aecm->xBuf[i]): "q10");
  }
  for (i = 0; i < PART_LEN; i += 16) {
    __asm __volatile("vld1.16 {d20, d21, d22, d23}, [%0, :256]" : :
            "r"(&aecm->dBufNoisy[i + PART_LEN]) : "q10");
    __asm __volatile("vst1.16 {d20, d21, d22, d23}, [%0, :256]" : :
            "r"(&aecm->dBufNoisy[i]): "q10");
  }
  if (nearendClean != NULL) {
    for (i = 0; i < PART_LEN; i += 16) {
      __asm __volatile("vld1.16 {d20, d21, d22, d23}, [%0, :256]" : :
              "r"(&aecm->dBufClean[i + PART_LEN]) : "q10");
      __asm __volatile("vst1.16 {d20, d21, d22, d23}, [%0, :256]" : :
              "r"(&aecm->dBufClean[i]): "q10");
    }
  }
}

static inline void AddLanes(uint32_t* ptr, uint32x4_t v) {
#if defined(__aarch64__)
  *(ptr) = vaddvq_u32(v);
#else
  uint32x2_t tmp_v;
  tmp_v = vadd_u32(vget_low_u32(v), vget_high_u32(v));
  tmp_v = vpadd_u32(tmp_v, tmp_v);
  *(ptr) = vget_lane_u32(tmp_v, 0);
#endif
}

void WebRtcAecm_CalcLinearEnergiesNeon(AecmCore_t* aecm,
                                       const uint16_t* far_spectrum,
                                       int32_t* echo_est,
                                       uint32_t* far_energy,
                                       uint32_t* echo_energy_adapt,
                                       uint32_t* echo_energy_stored) {
  int16_t* start_stored_p = aecm->channelStored;
  int16_t* start_adapt_p = aecm->channelAdapt16;
  int32_t* echo_est_p = echo_est;
  const int16_t* end_stored_p = aecm->channelStored + PART_LEN;
  const uint16_t* far_spectrum_p = far_spectrum;
  int16x8_t store_v, adapt_v, spectrum_v;
  uint32x4_t echo_est_v_low, echo_est_v_high;
  uint32x4_t far_energy_v, echo_energy_stored_v, echo_energy_adapt_v;

  far_energy_v = vdupq_n_u32(0);
  echo_energy_adapt_v = vdupq_n_u32(0);
  echo_energy_stored_v = vdupq_n_u32(0);

  // Get energy for the delayed far end signal and estimated
  // echo using both stored and adapted channels.
  // The C code:
  //  for (i = 0; i < PART_LEN1; i++) {
  //      echo_est[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
  //                                         far_spectrum[i]);
  //      (*far_energy) += (uint32_t)(far_spectrum[i]);
  //      *echo_energy_adapt += aecm->channelAdapt16[i] * far_spectrum[i];
  //      (*echo_energy_stored) += (uint32_t)echo_est[i];
  //  }
  while (start_stored_p < end_stored_p) {
    spectrum_v = vld1q_u16(far_spectrum_p);
    adapt_v = vld1q_s16(start_adapt_p);
    store_v = vld1q_s16(start_stored_p);

    far_energy_v = vaddw_u16(far_energy_v, vget_low_s16(spectrum_v));
    far_energy_v = vaddw_u16(far_energy_v, vget_high_s16(spectrum_v));

    echo_est_v_low = vmull_u16(vget_low_s16(store_v), vget_low_s16(spectrum_v));
    echo_est_v_high = vmull_u16(vget_high_s16(store_v),
                                vget_high_s16(spectrum_v));
    vst1q_s32(echo_est_p, echo_est_v_low);
    vst1q_s32(echo_est_p + 4, echo_est_v_high);

    echo_energy_stored_v = vaddq_s32(echo_est_v_low, echo_energy_stored_v);
    echo_energy_stored_v = vaddq_s32(echo_est_v_high, echo_energy_stored_v);

    echo_energy_adapt_v = vmlal_u16(echo_energy_adapt_v,
                                    vget_low_s16(adapt_v),
                                    vget_low_s16(spectrum_v));
    echo_energy_adapt_v = vmlal_u16(echo_energy_adapt_v,
                                    vget_high_s16(adapt_v),
                                    vget_high_s16(spectrum_v));

    start_stored_p += 8;
    start_adapt_p += 8;
    far_spectrum_p += 8;
    echo_est_p += 8;
  }

  AddLanes(far_energy, far_energy_v);
  AddLanes(echo_energy_stored, echo_energy_stored_v);
  AddLanes(echo_energy_adapt, echo_energy_adapt_v);

  echo_est[PART_LEN] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[PART_LEN],
                                             far_spectrum[PART_LEN]);
  *echo_energy_stored += (uint32_t)echo_est[PART_LEN];
  *far_energy += (uint32_t)far_spectrum[PART_LEN];
  *echo_energy_adapt += aecm->channelAdapt16[PART_LEN] * far_spectrum[PART_LEN];
}

void WebRtcAecm_StoreAdaptiveChannelNeon(AecmCore_t* aecm,
                                         const uint16_t* far_spectrum,
                                         int32_t* echo_est) {
  assert((uintptr_t)echo_est % 32 == 0);
  assert((uintptr_t)(aecm->channelStored) % 16 == 0);
  assert((uintptr_t)(aecm->channelAdapt16) % 16 == 0);

  // This is C code of following optimized code.
  // During startup we store the channel every block.
  //  memcpy(aecm->channelStored,
  //         aecm->channelAdapt16,
  //         sizeof(int16_t) * PART_LEN1);
  // Recalculate echo estimate
  //  for (i = 0; i < PART_LEN; i += 4) {
  //    echo_est[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
  //                                        far_spectrum[i]);
  //    echo_est[i + 1] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i + 1],
  //                                            far_spectrum[i + 1]);
  //    echo_est[i + 2] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i + 2],
  //                                            far_spectrum[i + 2]);
  //    echo_est[i + 3] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i + 3],
  //                                            far_spectrum[i + 3]);
  //  }
  //  echo_est[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
  //                                     far_spectrum[i]);
  const uint16_t* far_spectrum_p = far_spectrum;
  int16_t* start_adapt_p = aecm->channelAdapt16;
  int16_t* start_stored_p = aecm->channelStored;
  const int16_t* end_stored_p = aecm->channelStored + PART_LEN;
  int32_t* echo_est_p = echo_est;

  int16x8_t far_spectrum_v, adapt_v;
  int32x4_t echo_est_v_low, echo_est_v_high;

  while (start_stored_p < end_stored_p) {
    far_spectrum_v = vld1q_u16(far_spectrum_p);
    adapt_v = vld1q_s16(start_adapt_p);

    vst1q_s16(start_stored_p, adapt_v);

    echo_est_v_low = vmull_u16(vget_low_u16(far_spectrum_v),
                               vget_low_u16(adapt_v));
    echo_est_v_high = vmull_u16(vget_high_u16(far_spectrum_v),
                                vget_high_u16(adapt_v));

    vst1q_s32(echo_est_p, echo_est_v_low);
    vst1q_s32(echo_est_p + 4, echo_est_v_high);

    far_spectrum_p += 8;
    start_adapt_p += 8;
    start_stored_p += 8;
    echo_est_p += 8;
  }
  aecm->channelStored[PART_LEN] = aecm->channelAdapt16[PART_LEN];
  echo_est[PART_LEN] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[PART_LEN],
                                             far_spectrum[PART_LEN]);
}

void WebRtcAecm_ResetAdaptiveChannelNeon(AecmCore_t* aecm) {
  assert((uintptr_t)(aecm->channelStored) % 16 == 0);
  assert((uintptr_t)(aecm->channelAdapt16) % 16 == 0);
  assert((uintptr_t)(aecm->channelAdapt32) % 32 == 0);

  // The C code of following optimized code.
  // for (i = 0; i < PART_LEN1; i++) {
  //   aecm->channelAdapt16[i] = aecm->channelStored[i];
  //   aecm->channelAdapt32[i] = WEBRTC_SPL_LSHIFT_W32(
  //              (int32_t)aecm->channelStored[i], 16);
  // }

  int16_t* start_stored_p = aecm->channelStored;
  int16_t* start_adapt16_p = aecm->channelAdapt16;
  int32_t* start_adapt32_p = aecm->channelAdapt32;
  const int16_t* end_stored_p = start_stored_p + PART_LEN;

  int16x8_t stored_v;
  int32x4_t adapt32_v_low, adapt32_v_high;

  while (start_stored_p < end_stored_p) {
    stored_v = vld1q_s16(start_stored_p);
    vst1q_s16(start_adapt16_p, stored_v);

    adapt32_v_low = vshll_n_s16(vget_low_s16(stored_v), 16);
    adapt32_v_high = vshll_n_s16(vget_high_s16(stored_v), 16);

    vst1q_s32(start_adapt32_p, adapt32_v_low);
    vst1q_s32(start_adapt32_p + 4, adapt32_v_high);

    start_stored_p += 8;
    start_adapt16_p += 8;
    start_adapt32_p += 8;
  }
  aecm->channelAdapt16[PART_LEN] = aecm->channelStored[PART_LEN];
  aecm->channelAdapt32[PART_LEN] = (int32_t)aecm->channelStored[PART_LEN] << 16;
}
