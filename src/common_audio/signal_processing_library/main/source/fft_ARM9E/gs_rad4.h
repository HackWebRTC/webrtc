;
; $Copyright:
; ----------------------------------------------------------------
; This confidential and proprietary software may be used only as
; authorised by a licensing agreement from ARM Limited
;   (C) COPYRIGHT 2000,2002 ARM Limited
;       ALL RIGHTS RESERVED
; The entire notice above must be reproduced on all authorised
; copies and copies may only be made to the extent permitted
; by a licensing agreement from ARM Limited.
; ----------------------------------------------------------------
; File:     gs_rad4.h,v
; Revision: 1.8
; ----------------------------------------------------------------
; $
;
; Optimised ARM assembler multi-radix FFT
; Please read the readme.txt before this file
;
; This file contains the general stage, radix 4 macro

        MACRO
        GS_RAD4
        SETSHIFT postldshift, 2*norm
        SETSHIFT postmulshift, 2*norm+qshift
        ; dinc contains the number of bytes between the values to read
        ; for the radix 4 bufferfly
        ; Thus:
        ;  dinc*4 = number of bytes between the blocks at this level
        ;  dinc>>datalog = number of elements in each block at this level
        MOV     count, count, LSR #2     ; a quarter the blocks per stage
        STMFD   sp!, {dptr, count}
        ADD     t0, dinc, dinc, LSL #1   ; 3*dinc
        ADD     dptr, dptr, t0          ; move to last of 4 butterflys
        SUB     count, count, #1<<16    ; prepare top half of counter
12      ; block loop
        ; set top half of counter to (elements/block - 1)
        ADD     count, count, dinc, LSL #(16-$datalog)
15      ; butterfly loop
        IF (architecture>=5):LAND:(qshift<16)
          ; E extensions available (21 cycles)
          ; But needs a different table format
          LDMIA     cptr!, {x0i, x1i, x2i}
          LDR       x2r, [dptr], -dinc
          LDR       x1r, [dptr], -dinc
          LDR       x0r, [dptr], -dinc
          TWIDDLE_E x3r, x3i, x2i, t0, x2r
          TWIDDLE_E x2r, x2i, x1i, t0, x1r
          TWIDDLE_E x1r, x1i, x0i, t0, x0r
        ELSE
          ; load next three twiddle factors (66 @ 4 cycles/mul)
          LOADCOEFS cptr, x1r, x1i, x2r, x2i, x3r, x3i
          ; load data in reversed order & perform twiddles
          LOADDATA  dptr, -dinc, x0r, x0i
          TWIDDLE   x0r, x0i, x3r, x3i, t0, t1
          LOADDATA  dptr, -dinc, x0r, x0i
          TWIDDLE   x0r, x0i, x2r, x2i, t0, t1
          LOADDATA  dptr, -dinc, x0r, x0i
          TWIDDLE   x0r, x0i, x1r, x1i, t0, t1
        ENDIF
        LOADDATAZ  dptr, x0r, x0i
        SHIFTDATA x0r, x0i
        ; now calculate the h's
        ; h[0,k] = g[0,k] + g[2,k]
        ; h[1,k] = g[0,k] - g[2,k]
        ; h[2,k] = g[1,k] + g[3,k]
        ; h[3,k] = g[1,k] - g[3,k]
        SETREGS h,t0,t1,x0r,x0i,x1r,x1i,x2r,x2i
        ADD     $h0r, x0r, x1r $postmulshift
        ADD     $h0i, x0i, x1i $postmulshift
        SUB     $h1r, x0r, x1r $postmulshift
        SUB     $h1i, x0i, x1i $postmulshift
        ADD     $h2r, x2r, x3r
        ADD     $h2i, x2i, x3i
        SUB     $h3r, x2r, x3r
        SUB     $h3i, x2i, x3i
        ; now calculate the y's and store results
        ; y[0*N/4+k] = h[0,k] +   h[2,k]
        ; y[1*N/4+k] = h[1,k] + j*h[3,k]
        ; y[2*N/4+k] = h[0,k] -   h[2,k]
        ; y[3*N/4+k] = h[1,k] - j*h[3,k]
        SETREG  y0,x3r,x3i
        ADD     $y0r, $h0r, $h2r $postmulshift
        ADD     $y0i, $h0i, $h2i $postmulshift
        STORE   dptr, dinc, $y0r, $y0i
        SUBi    $y0r, $h1r, $h3i $postmulshift
        ADDi    $y0i, $h1i, $h3r $postmulshift
        STORE   dptr, dinc, $y0r, $y0i
        SUB     $y0r, $h0r, $h2r $postmulshift
        SUB     $y0i, $h0i, $h2i $postmulshift
        STORE   dptr, dinc, $y0r, $y0i
        ADDi    $y0r, $h1r, $h3i $postmulshift
        SUBi    $y0i, $h1i, $h3r $postmulshift
        STOREP  dptr, $y0r, $y0i
        ; continue butterfly loop
        SUBS    count, count, #1<<16
        BGE     %BT15
		; decrement counts for block loop
        ADD     t0, dinc, dinc, LSL #1   ; dinc * 3
        ADD     dptr, dptr, t0          ; move onto next block
        SUB     cptr, cptr, t0 $cdshift ; move back to coeficients start
        SUB     count, count, #1        ; done one more block
        MOVS    t1, count, LSL #16
        BNE     %BT12                   ; still more blocks to do
        ; finished stage
        ADD     cptr, cptr, t0 $cdshift ; move onto next stage coeficients
        LDMFD   sp!, {dptr, count}
        MOV     dinc, dinc, LSL #2       ; four times the entries per block
        MEND

        END
