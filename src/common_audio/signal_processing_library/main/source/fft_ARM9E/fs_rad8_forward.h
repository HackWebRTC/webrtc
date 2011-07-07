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
; File:     fs_rad8.h,v
; Revision: 1.5
; ----------------------------------------------------------------
; $
;
; Optimised ARM assembler multi-radix FFT
; Please read the readme.txt before this file
;
; This file contains first stage, radix-8 code
; It bit reverses (assuming a power of 2 FFT) and performs the first stage
;

        MACRO
        FS_RAD8
        SETSHIFT postldshift, 3*norm
        SETSHIFT postmulshift, 3*norm+qshift
        SETSHIFT postldshift1, 3*norm-1
        SETSHIFT postmulshift1, 3*norm+qshift-1
        IF "$prescale"<>""
          STMFD sp!, {dptr, N, r3}
        ELSE
          STMFD sp!, {dptr, N}
        ENDIF
        MOV     bitrev, #0
        MOV     dinc, N, LSL #($datalog-2)
12      ; first (radix 8) stage loop
        ; do first two (radix 2) stages
        FIRST_STAGE_RADIX8_ODD dinc, "dinc, LSR #1", bitrev
        FIRST_STAGE_RADIX8_EVEN dinc, bitrev
        ; third (radix 2) stage
        LDMFD   sp!, {x0r, x0i}
        ADD     $h0r, $h0r, x0r $postldshift  ; standard add
        ADD     $h0i, $h0i, x0i $postldshift
        SUB     x0r, $h0r, x0r $postldshift1
        SUB     x0i, $h0i, x0i $postldshift1
        STORE   dptr, #1<<$datalog, $h0r, $h0i
        LDMFD   sp!, {x1r, x1i}
        ADD     $h1r, $h1r, x1r $postmulshift
        ADD     $h1i, $h1i, x1i $postmulshift
        SUB     x1r, $h1r, x1r $postmulshift1
        SUB     x1i, $h1i, x1i $postmulshift1
        STORE   dptr, #1<<$datalog, $h1r, $h1i
        LDMFD   sp!, {x2r, x2i}
        SUBi    $h2r, $h2r, x2r $postldshift  ; note that x2r & x2i were
        ADDi    $h2i, $h2i, x2i $postldshift  ; swapped above
        ADDi    x2r, $h2r, x2r $postldshift1
        SUBi    x2i, $h2i, x2i $postldshift1
        STORE   dptr, #1<<$datalog, $h2r, $h2i
        LDMFD   sp!, {x3r, x3i}
        ADD     $h3r, $h3r, x3r $postmulshift
        ADD     $h3i, $h3i, x3i $postmulshift
        SUB     x3r, $h3r, x3r $postmulshift1
        SUB     x3i, $h3i, x3i $postmulshift1
        STORE   dptr, #1<<$datalog, $h3r, $h3i
        STORE   dptr, #1<<$datalog, x0r, x0i
        STORE   dptr, #1<<$datalog, x1r, x1i
        STORE   dptr, #1<<$datalog, x2r, x2i
        STORE   dptr, #1<<$datalog, x3r, x3i

        IF reversed
          SUBS  dinc, dinc, #2<<$datalog
          BGT   %BT12
        ELSE
          ; increment the count in a bit reverse manner
          EOR   bitrev, bitrev, dinc, LSR #($datalog-2+4) ; t0 = (N/8)>>1
          TST   bitrev, dinc, LSR #($datalog-2+4)
          BNE   %BT12
          ; get here for 1/2 the loops - carry to next bit
          EOR   bitrev, bitrev, dinc, LSR #($datalog-2+5)
          TST   bitrev, dinc, LSR #($datalog-2+5)
          BNE   %BT12
          ; get here for 1/4 of the loops - stop unrolling
          MOV   t0, dinc, LSR #($datalog-2+6)
15        ; bit reverse increment loop
          EOR   bitrev, bitrev, t0
          TST   bitrev, t0
          BNE   %BT12
          ; get here for 1/8 of the loops (or when finished)
          MOVS  t0, t0, LSR #1   ; move down to next bit
          BNE   %BT15           ; carry on if we haven't run off the bottom
        ENDIF

        IF "$prescale"<>""
          LDMFD sp!, {dptr, N, r3}
        ELSE
          LDMFD sp!, {dptr, N}
        ENDIF
        MOV     count, N, LSR #3         ; start with N/8 blocks 8 each
        MOV     dinc, #8<<$datalog      ; initial skip is 8 elements
        MEND



        MACRO
        FIRST_STAGE_RADIX8_ODD $dinc, $dinc_lsr1, $bitrev

        IF reversed
          ; load non bit reversed
          ADD   t0, inptr, #4<<$datalog
          LOADDATAI t0, #1<<$datalog, x0r, x0i
          LOADDATAI t0, #1<<$datalog, x1r, x1i
          LOADDATAI t0, #1<<$datalog, x2r, x2i
          LOADDATAI t0, #1<<$datalog, x3r, x3i
        ELSE
          ; load data elements 1,3,5,7 into register order 1,5,3,7
          ADD   t0, inptr, $bitrev, LSL #$datalog
          ADD   t0, t0, $dinc_lsr1      ; load in odd terms first
          LOADDATAI t0, $dinc, x0r, x0i
          LOADDATAI t0, $dinc, x2r, x2i
          LOADDATAI t0, $dinc, x1r, x1i
          LOADDATAI t0, $dinc, x3r, x3i
        ENDIF

        IF "$prescale"="P"
          LDR   t0, [sp, #8]
          MOV   x0r, x0r, LSL t0
          MOV   x0i, x0i, LSL t0
          MOV   x1r, x1r, LSL t0
          MOV   x1i, x1i, LSL t0
          MOV   x2r, x2r, LSL t0
          MOV   x2i, x2i, LSL t0
          MOV   x3r, x3r, LSL t0
          MOV   x3i, x3i, LSL t0
        ENDIF

        SETREG  h2, x3r, x3i
        SETREG  h3, t0, t1
        ; first stage (radix 2) butterflies
        ADD     x0r, x0r, x1r
        ADD     x0i, x0i, x1i
        SUB     x1r, x0r, x1r, LSL #1
        SUB     x1i, x0i, x1i, LSL #1
        SUB     $h3r, x2r, x3r
        SUB     $h3i, x2i, x3i
        ADD     $h2r, x2r, x3r
        ADD     $h2i, x2i, x3i
        ; second stage (radix 2) butterflies
        SUB     x2i, x0r, $h2r  ; swap real and imag here
        SUB     x2r, x0i, $h2i  ; for use later
        ADD     x0r, x0r, $h2r
        ADD     x0i, x0i, $h2i
        ADDi    x3r, x1r, $h3i
        SUBi    x3i, x1i, $h3r
        SUBi    x1r, x1r, $h3i
        ADDi    x1i, x1i, $h3r
        ; do the 1/sqrt(2) (+/-1 +/- i) twiddles for third stage
		LCLS tempname
tempname SETS "R_rad8"
        IMPORT  t_$qname$tempname
        LDR     t1, =t_$qname$tempname
;		IMPORT  t_$qname.R_rad8
;		LDR     t1, =t_$qname.R_rad8
        LOADCOEFR t1, t1

        STMFD   sp!, {dinc}     ;;; FIXME!!!

          ADD   t0, x1r, x1i            ; real part when * (1-i)
          SCALE x1r, t0, t1, dinc       ; scale by 1/sqrt(2)
          RSB   t0, t0, x1i, LSL #1      ; imag part when * (1-i)
          SCALE x1i, t0, t1, dinc       ; scale by 1/sqrt(2)
          SUB   t0, x3i, x3r            ; real part when * (-1-i)
          SCALE x3r, t0, t1, dinc       ; scale by 1/sqrt(2)
          SUB   t0, t0, x3i, LSL #1      ; imag part when * (-1-i)
          SCALE x3i, t0, t1, dinc       ; scale by 1/sqrt(2)

        LDMFD   sp!, {dinc}     ;;; FIXME!!!
        STMFD   sp!, {x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i}
        MEND

        MACRO
        FIRST_STAGE_RADIX8_EVEN $dinc, $bitrev
        ; load elements 0,2,4,6 into register order 0,4,2,6
        SETREGS h, x1r, x1i, x2r, x2i, x3r, x3i, t0, t1
        SETREG  g3, x0r, x0i

        IF reversed
          ; load normally
          LOADDATAI inptr, #1<<$datalog, $h0r, $h0i
          LOADDATAI inptr, #1<<$datalog, $h1r, $h1i
          LOADDATAI inptr, #1<<$datalog, $h2r, $h2i
          LOADDATAI inptr, #1<<$datalog, $h3r, $h3i
          ADD   inptr, inptr, #4<<$datalog
        ELSE
          ; load bit reversed
          ADD   x0r, inptr, $bitrev, LSL #$datalog
          LOADDATAI x0r, $dinc, $h0r, $h0i
          LOADDATAI x0r, $dinc, $h2r, $h2i
          LOADDATAI x0r, $dinc, $h1r, $h1i
          LOADDATAI x0r, $dinc, $h3r, $h3i
        ENDIF

        IF "$prescale"="P"
          LDR   x0r, [sp, #8+32]        ; NB we've stacked 8 extra regs!
          MOV   $h0r, $h0r, LSL x0r
          MOV   $h0i, $h0i, LSL x0r
          MOV   $h1r, $h1r, LSL x0r
          MOV   $h1i, $h1i, LSL x0r
          MOV   $h2r, $h2r, LSL x0r
          MOV   $h2i, $h2i, LSL x0r
          MOV   $h3r, $h3r, LSL x0r
          MOV   $h3i, $h3i, LSL x0r
        ENDIF

        SHIFTDATA $h0r, $h0i
        ; first stage (radix 2) butterflies
        ADD     $h0r, $h0r, $h1r $postldshift
        ADD     $h0i, $h0i, $h1i $postldshift
        SUB     $h1r, $h0r, $h1r $postldshift1
        SUB     $h1i, $h0i, $h1i $postldshift1
        SUB     $g3r, $h2r, $h3r
        SUB     $g3i, $h2i, $h3i
        ADD     $h2r, $h2r, $h3r
        ADD     $h2i, $h2i, $h3i
        ; second stage (radix 2) butterflies
        ADD     $h0r, $h0r, $h2r $postldshift
        ADD     $h0i, $h0i, $h2i $postldshift
        SUB     $h2r, $h0r, $h2r $postldshift1
        SUB     $h2i, $h0i, $h2i $postldshift1
        ADDi    $h3r, $h1r, $g3i $postldshift
        SUBi    $h3i, $h1i, $g3r $postldshift
        SUBi    $h1r, $h1r, $g3i $postldshift
        ADDi    $h1i, $h1i, $g3r $postldshift
        MEND

        END
