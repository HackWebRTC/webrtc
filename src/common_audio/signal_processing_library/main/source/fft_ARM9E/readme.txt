# $Copyright: 
# ----------------------------------------------------------------
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
#   (C) COPYRIGHT 2000,2002 ARM Limited
#       ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
# ----------------------------------------------------------------
# File:     readme.txt,v
# Revision: 1.4
# ----------------------------------------------------------------
# $



!!! To fully understand the FFT/ARM9E/WIN_MOB implementation in SPLIB,
!!! you have to refer to the full set of files in RVDS' package:
!!! C:\Program Files\ARM\RVDS\Examples\3.0\79\windows\fft_v5te.



  ARM Assembler FFT implementation
  ================================
  
  Overview
  ========
  
This implementation has been restructured to allow FFT's of varying radix
rather than the fixed radix-2 or radix-4 versions allowed earlier. The
implementation of an optimised assembler FFT of a given size (N points)
consists of chaining together a sequence of stages 1,2,3,...,k such that the
j'th stage has radix Rj and:

  N = R1*R2*R3*...*Rk
  
For the ARM implementations we keep the size of the Rj's decreasing with
increasing j, EXCEPT that if there are any non power of 2 factors (ie, odd
prime factors) then these come before all the power of 2 factors.

For example:

  N=64 would be implemented as stages:
     radix 4, radix 4, radix 4
     
  N=128 would be implemented as stages:
     radix 8, radix 4, radix 4
    OR
     radix 4, radix 4, radix 4, radix 2
     
  N=192 would be implemented as stages:
     radix 3, radix 4, radix 4, radix 4

The bitreversal is usally combined with the first stage where possible.


  Structure
  =========
  
The actual FFT routine is built out of a hierarchy of macros. All stage
macros and filenames are one of:

  fs_rad<n>    => the macro implements a radix <n> First Stage (usually
                  including the bit reversal)
                    
  gs_rad<n>    => the macro implements a radix <n> General Stage (any
                  stage except the first - includes the twiddle operations)
                  
  ls_rad<n>    => the macro implements a radix <n> Last Stage (this macro
                  is like the gs_rad<n> version but is optimised for
                  efficiency in the last stage)
                  
  ls_ztor      => this macro converts the output of a complex FFT to
                  be the first half of the output of a real FFT of
                  double the number of input points.
                  
Other files are:

  fft_mac.h    => Macro's and register definitions shared by all radix
                  implementations
                  
  fft_main.h   => Main FFT macros drawing together the stage macros
                  to produce a complete FFT
                  
                  
  Interfaces
  ==========
  
The register interfaces for the different type of stage macros are described
at the start of fft_mac.h
