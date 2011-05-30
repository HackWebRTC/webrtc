;// Optimised ARM assembler multi-radix FFT
        INCLUDE fft_main_inverse.h

                
        MACRO
        GENERATE_IFFT_FUNCTION $flags
        ; first work out a readable function name
        ; based on the flags
        FFT_OPTIONS_STRING $flags, name

        ; Entry:
        ;   r0 = input array
        ;   r1 = output array
        ;   r2 = number of points in FFT
        ;   r3 = pre-scale shift
        ;
        ; Exit:
        ;   r0 = 0 if successful
        ;      = 1 if table too small
        ;


        EXPORT FFT_$name
FFT_4OIQ14
        STMFD   sp!, {r4-r11, r14}
        IF "$radix"="4O"
tablename SETS "_8"
tablename SETS "$qname$coeforder$tablename"
        ELSE
tablename SETS "_4"
tablename SETS "$qname$coeforder$tablename"
        ENDIF
        IMPORT  s_$tablename
        LDR     lr, =s_$tablename
        LDR     lr,[lr]

        CMP     N, lr
        MOVGT   r0, #1
        LDMGTFD sp!, {r4-r11, pc}
        GENERATE_FFT $flags
        MOV     r0, #0
        LDMFD   sp!, {r4-r11, pc}
        LTORG
        MEND

        AREA FFTCODE, CODE, READONLY
        

        GENERATE_IFFT_FUNCTION FFT_16bit +FFT_RADIX4_8F +FFT_INVERSE +FFT_NONORM ; +FFT_REVERSED

        END
