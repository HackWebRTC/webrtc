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
; File:     fft_mac.h,v
; Revision: 1.14
; ----------------------------------------------------------------
; $
;
; Optimised ARM assembler multi-radix FFT
; Please read the readme.txt before this file
;
; Shared macros and interface definition file.

; NB: All the algorithms in this code are Decimation in Time. ARM
; is much better at Decimation in Time (as opposed to Decimation
; in Frequency) due to the position of the barrel shifter. Decimation
; in time has the twiddeling at the start of the butterfly, where as
; decimation in frequency has it at the end of the butterfly. The
; post multiply shifts can be hidden for Decimation in Time.

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;  FIRST STAGE INTERFACE
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; The FIRST STAGE macros "FS_RAD<R>" have the following interface:
;
; ON ENTRY:
;   REGISTERS:
;     r0 = inptr  => points to the input buffer consisting of N complex
;                    numbers of size (1<<datainlog) bytes each
;     r1 = dptr   => points to the output buffer consisting of N complex
;                    numbers of size (1<<datalog) bytes each
;     r2 = N      => is the number of points in the transform
;     r3 = pscale => shift to prescale input by (if applicable)
;   ASSEMBLER VARIABLES:
;     reversed    => logical variable, true if input data is already bit reversed
;                    The data needs to be bit reversed otherwise
;
; ACTION:
;     The routine should
;      (1) Bit reverse the data as required for the whole FFT (unless
;          the reversed flag is set)
;      (2) Prescale the input data by
;      (3) Perform a radix R first stage on the data
;      (4) Place the processed data in the output array pointed to be dptr
;
; ON EXIT:
;     r1 = dptr  => preserved and pointing to the output data
;     r2 = dinc  => number of bytes per "block" or "Group" in this stage
;                   this is: R<<datalog
;     r3 = count => number of radix-R blocks or groups processed in this stage
;                   this is: N/R
;     r0,r4-r12,r14 corrupted

inptr   RN 0    ; input buffer
dptr    RN 1    ; output/scratch buffer
N       RN 2    ; size of the FFT

dptr    RN 1    ; data pointer - points to end (load in reverse order)
dinc    RN 2    ; bytes between data elements at this level of FFT
count   RN 3    ; (elements per block<<16) | (blocks per stage)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;  GENERAL STAGE INTERFACE
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; The GENERAL STAGE macros "GS_RAD<R>" have the following interface.
;
; To describe the arguments, suppose this routine is called as stage j
; in a k-stage FFT with N=R1*R2*...*Rk. This stage is radix R=Rj.
;
; ON ENTRY:
;   REGISTERS:
;     r0 = cptr   => Pointer to twiddle coefficients for this stage consisting
;                    of complex numbers of size (1<<coeflog) bytes each in some
;                    stage dependent format.
;                    The format currently used in described in full in the
;                    ReadMe file in the tables subdirectory.
;     r1 = dptr   => points to the working buffer consisting of N complex
;                    numbers of size (1<<datalog) bytes each
;     r2 = dinc   => number of bytes per "block" or "Group" in the last stage:
;                      dinc  = (R1*R2*...*R(j-1))<<datalog
;     r3 = count  => number of blocks or Groups in the last stage:
;                      count = Rj*R(j+1)*...*Rk
;                    NB dinc*count = N<<datalog
;
; ACTION:
;     The routine should
;      (1) Twiddle the input data
;      (2) Perform a radix R stage on the data
;      (3) Perform the actions in place, result written to the dptr buffer
;
; ON EXIT:
;     r0 = cptr  => Updated to the end of the coefficients for the stage
;                   (the coefficients for the next stage will usually follow)
;     r1 = dptr  => preserved and pointing to the output data
;     r2 = dinc  => number of bytes per "block" or "Group" in this stage:
;                     dinc  = (R1*R2*..*Rj)<<datalog = (input dinc)*R
;     r3 = count => number of radix-R blocks or groups processed in this stage
;                     count = R(j+1)*...*Rk = (input count)/R
;     r0,r4-r12,r14 corrupted

cptr    RN 0    ; pointer to twiddle coefficients
dptr    RN 1    ; pointer to FFT data working buffer
dinc    RN 2    ; bytes per block/group at this stage
count   RN 3    ; number of blocks/groups at this stage

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;  LAST STAGE INTERFACE
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; The LAST STAGE macros "LS_RAD<R>" have the following interface.
;
; ON ENTRY:
;   REGISTERS:
;     r0 = cptr   => Pointer to twiddle coefficients for this stage consisting
;                    of complex numbers of size (1<<coeflog) bytes each in some
;                    stage dependent format.
;                    The format currently used in described in full in the
;                    ReadMe file in the tables subdirectory.
;                    There is a possible stride between the coefficients
;                    specified by cinc
;     r1 = dptr   => points to the working buffer consisting of N complex
;                    numbers of size (1<<datalog) bytes each
;     r2 = dinc   => number of bytes per "block" or "Group" in the last stage:
;                      dinc  = (N/R)<<datalog
;     r3 = cinc   => Bytes between twiddle values in the array pointed to by cptr
;
; ACTION:
;     The routine should
;      (1) Twiddle the input data
;      (2) Perform a (last stage optimised) radix R stage on the data
;      (3) Perform the actions in place, result written to the dptr buffer
;
; ON EXIT:
;     r0 = cptr  => Updated to point to real-to-complex conversion coefficients
;     r1 = dptr  => preserved and pointing to the output data
;     r2 = dinc  => number of bytes per "block" or "Group" in this stage:
;                     dinc  = N<<datalog = (input dinc)*R
;     r0,r4-r12,r14 corrupted

cptr    RN 0    ; pointer to twiddle coefficients
dptr    RN 1    ; pointer to FFT data working buffer
dinc    RN 2    ; bytes per block/group at this stage
cinc    RN 3    ; stride between twiddle coefficients in bytes

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
;  COMPLEX TO REAL CONVERSION INTERFACE
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; The COMPLEX TO REAL macros "LS_ZTOR" have the following interface.
;
; Suppose that 'w' is the N'th root of unity being used for the real FFT
; (usually exp(-2*pi*i/N) for forward transforms and exp(+2*pi*i/N) for
;  the inverse transform).
;
; ON ENTRY:
;   REGISTERS:
;     r0 = cptr   => Pointer to twiddle coefficients for this stage
;                    This consists of (1,w,w^2,w^3,...,w^(N/4-1)).
;                    There is a stride between each coeficient specified by cinc
;     r1 = dptr   => points to the working buffer consisting of N/2 complex
;                    numbers of size (1<<datalog) bytes each
;     r2 = dinc   => (N/2)<<datalog, the size of the complex buffer in bytes
;     r3 = cinc   => Bytes between twiddle value in array pointed to by cptr
;     r4 = dout   => Output buffer (usually the same as dptr)
;
; ACTION:
;     The routine should take the output of an N/2 point complex FFT and convert
;     it to the output of an N point real FFT, assuming that the real input
;     inputs were packed up into the real,imag,real,imag,... buffers of the complex
;     input. The output is N/2 complex numbers of the form:
;      y[0]+i*y[N/2], y[1], y[2], ..., y[N/2-1]
;     where y[0],...,y[N-1] is the output from a complex transform of the N
;     real inputs.
;
; ON EXIT:
;     r0-r12,r14 corrupted

cptr    RN 0    ; pointer to twiddle coefficients
dptr    RN 1    ; pointer to FFT data working buffer
dinc    RN 2    ; (N/2)<<datalog, the size of the data in bytes
cinc    RN 3    ; bytes between twiddle values in the coefficient buffer
dout    RN 4    ; address to write the output (normally the same as dptr)

;;;;;;;;;;;;;;;;;;;;;; END OF INTERFACES ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; first stage/outer loop level
;inptr  RN 0
;dptr   RN 1
;N      RN 2    ; size of FFT
;dinc   RN 2    ; bytes between block size when bit reversed (scaling of N)
bitrev  RN 3

; inner loop level
;cptr   RN 0    ; coefficient pointer for this level
;dptr   RN 1    ; data pointer - points to end (load in reverse order)
;dinc   RN 2    ; bytes between data elements at this level of FFT
;count  RN 3    ; (elements per block<<16) | (blocks per stage)

; data registers
x0r     RN 4
x0i     RN 5
x1r     RN 6
x1i     RN 7
x2r     RN 8
x2i     RN 9
x3r     RN 10
x3i     RN 11

t0      RN 12   ; these MUST be in correct order (t0<t1) for STM's
t1      RN 14

        MACRO
        SETREG  $prefix,$v0,$v1
        GBLS    $prefix.r
        GBLS    $prefix.i
$prefix.r SETS  "$v0"
$prefix.i SETS  "$v1"
        MEND

        MACRO
        SETREGS $prefix,$v0,$v1,$v2,$v3,$v4,$v5,$v6,$v7
        SETREG  $prefix.0,$v0,$v1
        SETREG  $prefix.1,$v2,$v3
        SETREG  $prefix.2,$v4,$v5
        SETREG  $prefix.3,$v6,$v7
        MEND

        MACRO
        SET2REGS $prefix,$v0,$v1,$v2,$v3
        SETREG  $prefix.0,$v0,$v1
        SETREG  $prefix.1,$v2,$v3
        MEND

        ; Macro to load twiddle coeficients
        ; Customise according to coeficient format
        ; Load next 3 complex coeficients into thr given registers
        ; Update the coeficient pointer
        MACRO
        LOADCOEFS $cp, $c0r, $c0i, $c1r, $c1i, $c2r, $c2i
        IF "$coefformat"="W"
          ; one word per scalar
          LDMIA $cp!, {$c0r, $c0i, $c1r, $c1i, $c2r, $c2i}
          MEXIT
        ENDIF
        IF "$coefformat"="H"
          ; one half word per scalar
          LDRSH $c0r, [$cp], #2
          LDRSH $c0i, [$cp], #2
          LDRSH $c1r, [$cp], #2
          LDRSH $c1i, [$cp], #2
          LDRSH $c2r, [$cp], #2
          LDRSH $c2i, [$cp], #2
          MEXIT
        ENDIF
        ERROR "Unsupported coeficient format: $coefformat"
        MEND

        ; Macro to load one twiddle coeficient
        ; $cp = address to load complex data
        ; $ci = post index to make to address after load
        MACRO
        LOADCOEF $cp, $ci, $re, $im
        IF "$coefformat"="W"
          LDR   $im, [$cp, #4]
          LDR   $re, [$cp], $ci
          MEXIT
        ENDIF
        IF "$coefformat"="H"
          LDRSH $im, [$cp, #2]
          LDRSH $re, [$cp], $ci
          MEXIT
        ENDIF
        ERROR "Unsupported coeficient format: $coefformat"
        MEND

        ; Macro to load one component of one twiddle coeficient
        ; $cp = address to load complex data
        ; $ci = post index to make to address after load
        MACRO
        LOADCOEFR $cp, $re
        IF "$coefformat"="W"
          LDR   $re, [$cp]
          MEXIT
        ENDIF
        IF "$coefformat"="H"
          LDRSH $re, [$cp]
          MEXIT
        ENDIF
        ERROR "Unsupported coeficient format: $coefformat"
        MEND

        ; Macro to load data elements in the given format
        ; $dp = address to load complex data
        ; $di = post index to make to address after load
        MACRO
        LOADDATAF $dp, $di, $re, $im, $format
        IF "$format"="W"
          LDR   $im, [$dp, #4]
          LDR   $re, [$dp], $di
          MEXIT
        ENDIF
        IF "$format"="H"
          LDRSH $im, [$dp, #2]
          LDRSH $re, [$dp], $di
          MEXIT
        ENDIF
        ERROR "Unsupported load format: $format"
        MEND

        MACRO
        LOADDATAZ $dp, $re, $im
        IF "$datainformat"="W"
          LDMIA $dp, {$re,$im}
          MEXIT
        ENDIF
        IF "$datainformat"="H"
          LDRSH $im, [$dp, #2]
          LDRSH $re, [$dp]
          MEXIT
        ENDIF
        ERROR "Unsupported load format: $format"
        MEND

        ; Load a complex data element from the working array
        MACRO
        LOADDATA $dp, $di, $re, $im
        LOADDATAF $dp, $di, $re, $im, $dataformat
        MEND

        ; Load a complex data element from the input array
        MACRO
        LOADDATAI $dp, $di, $re, $im
        LOADDATAF $dp, $di, $re, $im, $datainformat
        MEND

        MACRO
        LOADDATA4 $dp, $re0,$im0, $re1,$im1, $re2,$im2, $re3,$im3
        IF "$datainformat"="W"
         LDMIA  $dp!, {$re0,$im0, $re1,$im1, $re2,$im2, $re3,$im3}
        ELSE
         LOADDATAI $dp, #1<<$datalog, $re0,$im0
         LOADDATAI $dp, #1<<$datalog, $re1,$im1
         LOADDATAI $dp, #1<<$datalog, $re2,$im2
         LOADDATAI $dp, #1<<$datalog, $re3,$im3
        ENDIF
        MEND

        ; Shift data after load
        MACRO
        SHIFTDATA $dr, $di
        IF "$postldshift"<>""
          IF "$di"<>""
            MOV $di, $di $postldshift
          ENDIF
          MOV   $dr, $dr $postldshift
        ENDIF
        MEND

        ; Store a complex data item in the output data buffer
        MACRO
        STORE   $dp, $di, $re, $im
        IF "$dataformat"="W"
          STR   $im, [$dp, #4]
          STR   $re, [$dp], $di
          MEXIT
        ENDIF
        IF "$dataformat"="H"
          STRH  $im, [$dp, #2]
          STRH  $re, [$dp], $di
          MEXIT
        ENDIF
        ERROR "Unsupported save format: $dataformat"
        MEND

        ; Store a complex data item in the output data buffer
        MACRO
        STOREP  $dp, $re, $im
        IF "$dataformat"="W"
          STMIA $dp!, {$re,$im}
          MEXIT
        ENDIF
        IF "$dataformat"="H"
          STRH  $im, [$dp, #2]
          STRH  $re, [$dp], #4
          MEXIT
        ENDIF
        ERROR "Unsupported save format: $dataformat"
        MEND

        MACRO
        STORE3P $dp, $re0, $im0, $re1, $im1, $re2, $im2
        IF "$dataformat"="W"
          STMIA $dp!, {$re0,$im0, $re1,$im1, $re2,$im2}
          MEXIT
        ENDIF
        IF "$dataformat"="H"
          STRH  $im0, [$dp, #2]
          STRH  $re0, [$dp], #4
          STRH  $im1, [$dp, #2]
          STRH  $re1, [$dp], #4
          STRH  $im2, [$dp, #2]
          STRH  $re2, [$dp], #4
          MEXIT
        ENDIF
        ERROR "Unsupported save format: $dataformat"
        MEND

        ; do different command depending on forward/inverse FFT
        MACRO
        DOi     $for, $bac, $d, $s1, $s2, $shift
          IF "$shift"=""
            $for $d, $s1, $s2
          ELSE
            $for $d, $s1, $s2, $shift
          ENDIF
        MEND

        ; d = s1 + s2 if w=exp(+2*pi*i/N) j=+i - inverse transform
        ; d = s1 - s2 if w=exp(-2*pi*i/N) j=-i - forward transform
        MACRO
        ADDi    $d, $s1, $s2, $shift
        DOi     SUB, ADD, $d, $s1, $s2, $shift
        MEND

        ; d = s1 - s2 if w=exp(+2*pi*i/N) j=+i - inverse transform
        ; d = s1 + s2 if w=exp(-2*pi*i/N) j=-i - forward transform
        MACRO
        SUBi    $d, $s1, $s2, $shift
        DOi     ADD, SUB, $d, $s1, $s2, $shift
        MEND

        ; check that $val is in the range -$max to +$max-1
        ; set carry flag (sicky) if not (2 cycles)
        ; has the advantage of not needing a separate register
        ; to store the overflow state
        MACRO
        CHECKOV $val, $tmp, $max
        EOR     $tmp, $val, $val, ASR#31
        CMPCC   $tmp, $max
        MEND

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; Macro's to perform the twiddle stage (complex multiply by coefficient)
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; The coefficients are stored in different formats according to the
; precision and processor architecture. The coefficients required
; will be of the form:
;
;   c(k) = cos( + k*2*pi*i/N ),  s(k) = sin( + k*2*pi*i/N )
;
;               c(k) + i*s(k) = exp(+2*pi*k*i/N)
;
; for some k's. The storage formats are:
;
; Format        Data
; Q14S          (c-s, s) in Q14 format, 16-bits per real
; Q14R          (c, s)   in Q14 format, 16-bits per real
; Q30S          (c-s, s) in Q30 format, 32-bits per real
;
; The operation to be performed is one of:
;
;     a+i*b = (x+i*y)*(c-i*s)   => forward transform
; OR  a+i*b = (x+i*y)*(c+i*s)   => inverse transform
;
; For the R format the operation is quite simple - requiring 4 muls
; and 2 adds:
;
;   Forward:  a = x*c+y*s, b = y*c-x*s
;   Inverse:  a = x*c-y*s, b = y*c+x*s
;
; For the S format the operations is more complex but only requires
; three multiplies, and is simpler to schedule:
;
;   Forward:  a = (y-x)*s + x*(c+s) = x*(c-s) + (x+y)*s
;             b = (y-x)*s + y*(c-s) = y*(c+s) - (x+y)*s
;
;   Inverse:  a = (x-y)*s + x*(c-s)
;             b = (x-y)*s + y*(c+s)
;
; S advantage 16bit: 1ADD, 1SUB, 1MUL, 2MLA instead of 1SUB, 3MUL, 1MLA
; S advantage 32bit: 2ADD, 1SUB, 2SMULL, 1SMLAL instead of 1RSB, 2SMULL, 2SMLAL
; So S wins except for a very fast multiplier (eg 9E)
;
; NB The coefficients must always be the second operand on processor that
; take a variable number of cycles per multiply - so the FFT time remains constant

        ; This twiddle takes unpacked real and imaginary values
        ; Expects (cr,ci) = (c-s,s) on input
        ; Sets    (cr,ci) = (a,b) on output
        MACRO
        TWIDDLE $xr, $xi, $cr, $ci, $t0, $t1
        IF qshift>=0 :LAND: qshift<32
            SUB $t1, $xi, $xr           ; y-x
            MUL $t0, $t1, $ci           ; (y-x)*s
            ADD $t1, $cr, $ci, LSL #1    ; t1 = c+s allow mul to finish on SA
            MLA $ci, $xi, $cr, $t0      ; b
            MLA $cr, $xr, $t1, $t0      ; a
        ELSE
            ADD   $t1, $cr, $ci, LSL #1  ; t1 = c+s
            SMULL $cr, $t0, $xi, $cr    ; t0 = y*(c-s)
            SUB   $xi, $xi, $xr         ; xr = y-x + allow mul to finish on SA
            SMULL $ci, $cr, $xi, $ci    ; cr = (y-x)*s
            ADD   $ci, $cr, $t0         ; b + allow mul to finish on SA
            SMLAL $t0, $cr, $xr, $t1    ; a
        ENDIF
        MEND

        ; The following twiddle variant is similar to the above
        ; except that it is for an "E" processor varient. A standard
        ; 4 multiply twiddle is used as it requires the same number
        ; of cycles and needs less intermediate precision
        ;
        ; $co = coeficent real and imaginary (c,s) (packed)
        ; $xx = input data real and imaginary part (packed)
        ;
        ; $xr = destination register for real part of product
        ; $xi = destination register for imaginary part of product
        ;
        ; All registers should be distinct
        ;
        MACRO
        TWIDDLE_E $xr, $xi, $c0, $t0, $xx, $xxi
          SMULBT  $t0, $xx, $c0
          SMULBB  $xr, $xx, $c0
          IF "$xxi"=""
            SMULTB  $xi, $xx, $c0
            SMLATT  $xr, $xx, $c0, $xr
          ELSE
            SMULBB  $xi, $xxi, $c0
            SMLABT  $xr, $xxi, $c0, $xr
          ENDIF
          SUB     $xi, $xi, $t0
        MEND

        ; Scale data value in by the coefficient, writing result to out
        ; The coeficient must be the second multiplicand
        ; The post mul shift need not be done so in most cases this
        ; is just a multiply (unless you need higher precision)
        ; coef must be preserved
        MACRO
        SCALE   $out, $in, $coef, $tmp
        IF qshift>=0 :LAND: qshift<32
          MUL   $out, $in, $coef
        ELSE
          SMULL $tmp, $out, $in, $coef
        ENDIF
        MEND

        MACRO
        DECODEFORMAT    $out, $format
        GBLS    $out.log
        GBLS    $out.format
$out.format SETS "$format"
        IF "$format"="B"
$out.log  SETS "1"
          MEXIT
        ENDIF
        IF "$format"="H"
$out.log  SETS "2"
          MEXIT
        ENDIF
        IF "$format"="W"
$out.log SETS "3"
         MEXIT
        ENDIF
        ERROR "Unrecognised format for $out: $format"
        MEND

        ; generate a string in $var of the correct right shift
        ; amount - negative values = left shift
        MACRO
        SETSHIFT $var, $value
        LCLA svalue
svalue  SETA $value
$var    SETS ""
        IF svalue>0 :LAND: svalue<32
$var      SETS ",ASR #0x$svalue"
        ENDIF
svalue  SETA -svalue
        IF svalue>0 :LAND: svalue<32
$var      SETS ",LSL #0x$svalue"
        ENDIF
        MEND


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;                                                                ;
;  CODE to decipher the FFT options                              ;
;                                                                ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


        ; The $flags variable specifies the FFT options
        ; The global string $name is set to a textual version
        ; The global string $table is set the table name
        MACRO
        FFT_OPTIONS_STRING $flags, $name
        GBLS    $name
        GBLS    qname           ; name of the precision (eg Q14, Q30)
        GBLS    direction       ; name of the direction (eg I, F)
        GBLS    radix           ; name of the radix (2, 4E, 4B, 4O etc)
        GBLS    intype          ; name of input data type (if real)
        GBLS    prescale        ; flag to indicate prescale
        GBLS    outpos          ; position for the output data
        GBLS    datainformat    ; bytes per input data item
        GBLS    dataformat      ; bytes per working item
        GBLS    coefformat      ; bytes per coefficient working item
        GBLS    coeforder       ; R=(c,s) S=(c-s,s) storage format
        GBLA    datainlog       ; shift to bytes per input complex
        GBLA    datalog         ; shift to bytes per working complex
        GBLA    coeflog         ; shift to bytes per coefficient complex
        GBLA    qshift          ; right shift after multiply
        GBLA    norm
        GBLA    architecture    ; 4=Arch4(7TDMI,SA), 5=Arch5TE(ARM9E)
        GBLS    cdshift
        GBLS    postmulshift
        GBLS    postldshift
        GBLS    postmulshift1
        GBLS    postldshift1
        GBLL    reversed        ; flag to indicate input is already bit reversed
        GBLS    tablename


        ; find what sort of processor we are building the FFT for
architecture SETA 4             ; Architecture 4 (7TDMI, StrongARM etc)
;qname SETS {CPU}
;    P $qname
        IF ((({ARCHITECTURE}:CC:"aaaa"):LEFT:3="5TE") :LOR: (({ARCHITECTURE}:CC:"aa"):LEFT:1="6"))
architecture SETA 5             ; Architecture 5 (ARM9E, E extensions)
;    P arch E
        ENDIF

reversed SETL {FALSE}
        ; decode input order
        IF ($flags:AND:FFT_INPUTORDER)=FFT_REVERSED
reversed SETL {TRUE}
        ENDIF

        ; decode radix type to $radix
        IF ($flags:AND:FFT_RADIX)=FFT_RADIX4
radix     SETS "4E"
        ENDIF
        IF ($flags:AND:FFT_RADIX)=FFT_RADIX4_8F
radix     SETS "4O"
        ENDIF
        IF ($flags:AND:FFT_RADIX)=FFT_RADIX4_2L
radix     SETS "4B"
        ENDIF

        ; decode direction to $direction
direction SETS "F"

        ; decode data size to $qname, and *log's
        IF ($flags:AND:FFT_DATA_SIZES)=FFT_32bit
qname     SETS "Q30"
datainlog SETA 3        ; 8 bytes per complex
datalog   SETA 3
coeflog   SETA 3
datainformat SETS "W"
dataformat   SETS "W"
coefformat   SETS "W"
qshift    SETA -2       ; shift left top word of 32 bit result
        ENDIF
        IF ($flags:AND:FFT_DATA_SIZES)=FFT_16bit
qname     SETS "Q14"
datainlog SETA 2
datalog   SETA 2
coeflog   SETA 2
datainformat SETS "H"
dataformat   SETS "H"
coefformat   SETS "H"
qshift    SETA 14
        ENDIF

        ; find the coefficient ordering
coeforder SETS "S"
        IF (architecture>=5):LAND:(qshift<16)
coeforder SETS "R"
        ENDIF

        ; decode real vs complex input data type
intype  SETS ""
        IF ($flags:AND:FFT_INPUTTYPE)=FFT_REAL
intype    SETS "R"
        ENDIF

        ; decode on outpos
outpos  SETS ""
        IF ($flags:AND:FFT_OUTPUTPOS)=FFT_OUT_INBUF
outpos  SETS "I"
        ENDIF

        ; decode on prescale
prescale SETS ""
        IF ($flags:AND:FFT_INPUTSCALE)=FFT_PRESCALE
prescale SETS "P"
        ENDIF

        ; decode on output scale
norm    SETA 1
        IF ($flags:AND:FFT_OUTPUTSCALE)=FFT_NONORM
norm      SETA 0
        ENDIF

        ; calculate shift to convert data offsets to coefficient offsets
        SETSHIFT cdshift, ($datalog)-($coeflog)

$name   SETS    "$radix$direction$qname$intype$outpos$prescale"
		MEND

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;                                                                ;
;  FFT GENERATOR                                                 ;
;                                                                ;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; FFT options bitfield

FFT_DIRECTION   EQU     0x00000001      ; direction select bit
FFT_FORWARD     EQU     0x00000000      ; forward exp(-ijkw) coefficient FFT
FFT_INVERSE     EQU     0x00000001      ; inverse exp(+ijkw) coefficient FFT

FFT_INPUTORDER  EQU     0x00000002      ; input order select field
FFT_BITREV      EQU     0x00000000      ; input data is in normal order (bit reverse)
FFT_REVERSED    EQU     0x00000002      ; assume input data is already bit revesed

FFT_INPUTSCALE  EQU     0x00000004      ; select scale on input data
FFT_NOPRESCALE  EQU     0x00000000      ; do not scale input data
FFT_PRESCALE    EQU     0x00000004      ; scale input data up by a register amount

FFT_INPUTTYPE   EQU     0x00000010      ; selector for real/complex input data
FFT_COMPLEX     EQU     0x00000000      ; do complex FFT of N points
FFT_REAL        EQU     0x00000010      ; do a 2*N point real FFT

FFT_OUTPUTPOS   EQU     0x00000020      ; where is the output placed?
FFT_OUT_OUTBUF  EQU     0x00000000      ; default - in the output buffer
FFT_OUT_INBUF   EQU     0x00000020      ; copy it back to the input buffer

FFT_RADIX       EQU     0x00000F00      ; radix select
FFT_RADIX4      EQU     0x00000000      ; radix 4 (log_2 N must be even)
FFT_RADIX4_8F   EQU     0x00000100      ; radix 4 with radix 8 first stage
FFT_RADIX4_2L   EQU     0x00000200      ; radix 4 with optional radix 2 last stage

FFT_OUTPUTSCALE EQU     0x00001000      ; select output scale value
FFT_NORMALISE   EQU     0x00000000      ; default - divide by N during algorithm
FFT_NONORM      EQU     0x00001000      ; calculate the raw sum (no scale)

FFT_DATA_SIZES  EQU     0x000F0000
FFT_16bit       EQU     0x00000000      ; 16-bit data and Q14 coefs
FFT_32bit       EQU     0x00010000      ; 32-bit data and Q30 coefs

        END
