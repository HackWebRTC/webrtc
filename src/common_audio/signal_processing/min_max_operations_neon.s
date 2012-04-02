@
@ Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
@
@ Use of this source code is governed by a BSD-style license
@ that can be found in the LICENSE file in the root of the source
@ tree. An additional intellectual property rights grant can be found
@ in the file PATENTS.  All contributing project authors may
@ be found in the AUTHORS file in the root of the source tree.
@

@ This file contains the function WebRtcSpl_MaxAbsValueW16(), optimized for
@ ARM Neon platform. The description header can be found in
@ signal_processing_library.h
@
@ The reference C code is in file min_max_operations.c. Code here is basically
@ a loop unrolling by 8 with Neon instructions. Bit-exact.

.arch armv7-a
.fpu neon
.global WebRtcSpl_MaxAbsValueW16
.align  2

WebRtcSpl_MaxAbsValueW16:
.fnstart

  vmov.i16 q12, #0
  mov r2, #-1                 @ Return value for the maximum.
  cmp r1, #0                  @ length
  ble END                     @ Return -1 if length <= 0.
  cmp r1, #7
  ble LOOP_NO_UNROLLING

  lsr r3, r1, #3
  lsl r3, #3                  @ Counter for LOOP_UNROLLED_BY_8: length / 8 * 8.
  sub r1, r3                  @ Counter for LOOP_NO_UNROLLING: length % 8.

LOOP_UNROLLED_BY_8:
  vld1.16 {d26, d27}, [r0]!
  subs r3, #8
  vabs.s16 q13, q13           @ Note vabs doesn't change the value of -32768.
  vmax.u16 q12, q13           @ Use u16 so we don't lose the value -32768.
  bne LOOP_UNROLLED_BY_8

  @ Find the maximum value in the Neon registers and move it to r2.
  vmax.u16 d24, d25
  vpmax.u16 d24, d24
  vpmax.u16 d24, d24
  cmp r1, #0
  vmov.u16 r2, d24[0]
  ble END

LOOP_NO_UNROLLING:
  ldrsh r3, [r0], #2
  eor r12, r3, r3, asr #31    @ eor and then sub, to get absolute value.
  sub r12, r12, r3, asr #31
  cmp r2, r12
  movlt r2, r12
  subs r1, #1
  bne LOOP_NO_UNROLLING

END:
  cmp r2, #0x8000             @ Guard against the case for -32768.
  subeq r2, #1
  mov r0, r2
  bx  lr

.fnend
