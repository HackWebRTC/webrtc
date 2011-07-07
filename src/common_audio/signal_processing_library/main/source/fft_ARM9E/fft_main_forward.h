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
; File:     fft_main.h,v
; Revision: 1.10
; ----------------------------------------------------------------
; $
;
; Optimised ARM assembler multi-radix FFT
; Please read the readme.txt before this file
;

        INCLUDE fft_mac_forward.h       ; general macros
        INCLUDE fs_rad8_forward.h       ; first stage, radix 8 macros
        INCLUDE gs_rad4.h       ; general stage, radix 4 macros

; The macro in this file generates a whole FFT by glueing together
; FFT stage macros. It is designed to handle a range of power-of-2
; FFT's, the power of 2 set at run time.

; The following should be set up:
;
; $flags   = a 32-bit integer indicating what FFT code to generate
;            formed by a bitmask of the above FFT_* flag definitions
;            (see fft_mac.h)
;
; r0 = inptr = address of the input buffer
; r1 = dptr  = address of the output buffer
; r2 = N     = the number of points in the FFT
; r3 =       = optional pre-left shift to apply to the input data
;
; The contents of the input buffer are preserved (provided that the
; input and output buffer are different, which must be the case unless
; no bitreversal is required and the input is provided pre-reversed).

        MACRO
        GENERATE_FFT $flags
        ; decode the options word
        FFT_OPTIONS_STRING $flags, name

        IF "$outpos"<>""
          ; stack the input buffer address for later on
          STMFD sp!, {inptr}
        ENDIF

        ; Do first stage - radix 4 or radix 8 depending on parity
        IF "$radix"="4O"
          FS_RAD8
tablename SETS "_8"
tablename SETS "$qname$coeforder$tablename"
		ELSE
          FS_RAD4
tablename SETS "_4"
tablename SETS "$qname$coeforder$tablename"
        ENDIF
        IMPORT  t_$tablename
        LDR     cptr, =t_$tablename     ; coefficient table
        CMP     count, #1
        BEQ     %FT10                   ; exit for small case

12      ; General stage loop
		GS_RAD4
        CMP     count, #2
        BGT     %BT12

        IF "$radix"="4B"
          ; support odd parity as well
          ;BLT  %FT10           ; less than 2 left (ie, finished)
          ;LS_RAD2              ; finish off with a radix 2 stage
        ENDIF

10      ; we've finished the complex FFT
        IF ($flags:AND:FFT_INPUTTYPE)=FFT_REAL
          ; convert to a real FFT
          IF "$outpos"="I"
            LDMFD sp!, {dout}
          ELSE
            MOV   dout, dptr
          ENDIF
          ; dinc = (N/2) >> datalog where N is the number of real points
          IMPORT s_$tablename
          LDR   t0, = s_$tablename
          LDR   t0, [t0]                        ; max N handled by the table
          MOV   t1, dinc, LSR #($datalog-1)      ; real N we want to handle
          CMP   t0, t1
          MOV   cinc, #3<<$coeflog              ; radix 4 table stage
          MOVEQ cinc, #1<<$coeflog              ; radix 4 table stage
          LS_ZTOR
        ENDIF

        MEND

        END
