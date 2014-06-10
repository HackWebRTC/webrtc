/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/isac/fix/source/filterbank_internal.h"

// WebRtcIsacfix_AllpassFilter2FixDec16 function optimized for MIPSDSP platform
// Bit-exact with WebRtcIsacfix_AllpassFilter2FixDec16C from filterbanks.c
void WebRtcIsacfix_AllpassFilter2FixDec16MIPS(
    int16_t *data_ch1,  // Input and output in channel 1, in Q0
    int16_t *data_ch2,  // Input and output in channel 2, in Q0
    const int16_t *factor_ch1,  // Scaling factor for channel 1, in Q15
    const int16_t *factor_ch2,  // Scaling factor for channel 2, in Q15
    const int length,  // Length of the data buffers
    int32_t *filter_state_ch1,  // Filter state for channel 1, in Q16
    int32_t *filter_state_ch2) {  // Filter state for channel 2, in Q16

  int32_t st0_ch1, st1_ch1; // channel1 state variables
  int32_t st0_ch2, st1_ch2; // channel2 state variables
  int32_t f_ch10, f_ch11, f_ch20, f_ch21; // factor variables
  int32_t r0, r1, r2, r3, r4, r5; // temporary ragister variables

  __asm __volatile (
    ".set           push                                                  \n\t"
    ".set           noreorder                                             \n\t"
    // Load all the state and factor variables
    "lh             %[f_ch10],      0(%[factor_ch1])                      \n\t"
    "lh             %[f_ch20],      0(%[factor_ch2])                      \n\t"
    "lh             %[f_ch11],      2(%[factor_ch1])                      \n\t"
    "lh             %[f_ch21],      2(%[factor_ch2])                      \n\t"
    "lw             %[st0_ch1],     0(%[filter_state_ch1])                \n\t"
    "lw             %[st1_ch1],     4(%[filter_state_ch1])                \n\t"
    "lw             %[st0_ch2],     0(%[filter_state_ch2])                \n\t"
    "lw             %[st1_ch2],     4(%[filter_state_ch2])                \n\t"
    // Allpass filtering loop
   "1:                                                                    \n\t"
    "lh             %[r0],          0(%[data_ch1])                        \n\t"
    "lh             %[r1],          0(%[data_ch2])                        \n\t"
    "addiu          %[length],      %[length],              -1            \n\t"
    "mul            %[r2],          %[r0],                  %[f_ch10]     \n\t"
    "mul            %[r3],          %[r1],                  %[f_ch20]     \n\t"
    "sll            %[r0],          %[r0],                  16            \n\t"
    "sll            %[r1],          %[r1],                  16            \n\t"
    "sll            %[r2],          %[r2],                  1             \n\t"
    "addq_s.w       %[r2],          %[r2],                  %[st0_ch1]    \n\t"
    "sll            %[r3],          %[r3],                  1             \n\t"
    "addq_s.w       %[r3],          %[r3],                  %[st0_ch2]    \n\t"
    "sra            %[r2],          %[r2],                  16            \n\t"
    "mul            %[st0_ch1],     %[f_ch10],              %[r2]         \n\t"
    "sra            %[r3],          %[r3],                  16            \n\t"
    "mul            %[st0_ch2],     %[f_ch20],              %[r3]         \n\t"
    "mul            %[r4],          %[r2],                  %[f_ch11]     \n\t"
    "mul            %[r5],          %[r3],                  %[f_ch21]     \n\t"
    "sll            %[st0_ch1],     %[st0_ch1],             1             \n\t"
    "subq_s.w       %[st0_ch1],     %[r0],                  %[st0_ch1]    \n\t"
    "sll            %[st0_ch2],     %[st0_ch2],             1             \n\t"
    "subq_s.w       %[st0_ch2],     %[r1],                  %[st0_ch2]    \n\t"
    "sll            %[r4],          %[r4],                  1             \n\t"
    "addq_s.w       %[r4],          %[r4],                  %[st1_ch1]    \n\t"
    "sll            %[r5],          %[r5],                  1             \n\t"
    "addq_s.w       %[r5],          %[r5],                  %[st1_ch2]    \n\t"
    "sra            %[r4],          %[r4],                  16            \n\t"
    "mul            %[r0],          %[r4],                  %[f_ch11]     \n\t"
    "sra            %[r5],          %[r5],                  16            \n\t"
    "mul            %[r1],          %[r5],                  %[f_ch21]     \n\t"
    "sh             %[r4],          0(%[data_ch1])                        \n\t"
    "sh             %[r5],          0(%[data_ch2])                        \n\t"
    "addiu          %[data_ch1],    %[data_ch1],            2             \n\t"
    "sll            %[r2],          %[r2],                  16            \n\t"
    "sll            %[r0],          %[r0],                  1             \n\t"
    "subq_s.w       %[st1_ch1],     %[r2],                  %[r0]         \n\t"
    "sll            %[r3],          %[r3],                  16            \n\t"
    "sll            %[r1],          %[r1],                  1             \n\t"
    "subq_s.w       %[st1_ch2],     %[r3],                  %[r1]         \n\t"
    "bgtz           %[length],      1b                                    \n\t"
    " addiu         %[data_ch2],    %[data_ch2],            2             \n\t"
    // Store channel states
    "sw             %[st0_ch1],     0(%[filter_state_ch1])                \n\t"
    "sw             %[st1_ch1],     4(%[filter_state_ch1])                \n\t"
    "sw             %[st0_ch2],     0(%[filter_state_ch2])                \n\t"
    "sw             %[st1_ch2],     4(%[filter_state_ch2])                \n\t"
    ".set           pop                                                   \n\t"
    : [f_ch10] "=&r" (f_ch10), [f_ch20] "=&r" (f_ch20),
      [f_ch11] "=&r" (f_ch11), [f_ch21] "=&r" (f_ch21),
      [st0_ch1] "=&r" (st0_ch1), [st1_ch1] "=&r" (st1_ch1),
      [st0_ch2] "=&r" (st0_ch2), [st1_ch2] "=&r" (st1_ch2),
      [r0] "=&r" (r0), [r1] "=&r" (r1), [r2] "=&r" (r2),
      [r3] "=&r" (r3), [r4] "=&r" (r4), [r5] "=&r" (r5)
    : [factor_ch1] "r" (factor_ch1), [factor_ch2] "r" (factor_ch2),
      [filter_state_ch1] "r" (filter_state_ch1),
      [filter_state_ch2] "r" (filter_state_ch2),
      [data_ch1] "r" (data_ch1), [data_ch2] "r" (data_ch2),
      [length] "r" (length)
    : "memory", "hi", "lo"
  );
}
