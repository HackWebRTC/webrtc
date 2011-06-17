@
@  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
@
@  Use of this source code is governed by a BSD-style license
@  that can be found in the LICENSE file in the root of the source
@  tree. An additional intellectual property rights grant can be found
@  in the file PATENTS.  All contributing project authors may
@  be found in the AUTHORS file in the root of the source tree.

@ sqrt() routine. 3 cycles/bit, total 51 cycles.
@ IN :  r0 32 bit unsigned integer
@ OUT:  r0 = INT (SQRT (r0)), precision is 16 bits
@ TMP:  r1, r2

.global WebRtcSpl_Sqrt

.align  2
.section .text.WebRtcSpl_Sqrt:
WebRtcSpl_Sqrt:
.fnstart

    MOV    r1, #3 << 30
    MOV    r2, #1 << 30

    @ unroll for i = 0 .. 15

    CMP    r0, r2, ROR #2 * 0
    SUBHS  r0, r0, r2, ROR #2 * 0
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 1
    SUBHS  r0, r0, r2, ROR #2 * 1
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 2
    SUBHS  r0, r0, r2, ROR #2 * 2
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 3
    SUBHS  r0, r0, r2, ROR #2 * 3
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 4
    SUBHS  r0, r0, r2, ROR #2 * 4
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 5
    SUBHS  r0, r0, r2, ROR #2 * 5
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 6
    SUBHS  r0, r0, r2, ROR #2 * 6
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 7
    SUBHS  r0, r0, r2, ROR #2 * 7
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 8
    SUBHS  r0, r0, r2, ROR #2 * 8
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 9
    SUBHS  r0, r0, r2, ROR #2 * 9
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 10
    SUBHS  r0, r0, r2, ROR #2 * 10
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 11
    SUBHS  r0, r0, r2, ROR #2 * 11
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 12
    SUBHS  r0, r0, r2, ROR #2 * 12
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 13
    SUBHS  r0, r0, r2, ROR #2 * 13
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 14
    SUBHS  r0, r0, r2, ROR #2 * 14
    ADC    r2, r1, r2, LSL #1

    CMP    r0, r2, ROR #2 * 15
    SUBHS  r0, r0, r2, ROR #2 * 15
    ADC    r2, r1, r2, LSL #1

    BIC    r0, r2, #3 << 30  @ for rounding add: CMP r0, r2  ADC r2, #1

.fnend
