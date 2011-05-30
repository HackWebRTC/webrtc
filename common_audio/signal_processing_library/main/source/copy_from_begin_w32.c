/*
 * copy_from_begin_w32.c
 *
 * This file contains the function WebRtcSpl_CopyFromBeginW32().
 * The description header can be found in signal_processing_library.h
 *
 */

#include <string.h>
#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_CopyFromBeginW32(G_CONST WebRtc_Word32 *vector_in,
                                         WebRtc_Word16 length,
                                         WebRtc_Word16 samples,
                                         WebRtc_Word32 *vector_out,
                                         WebRtc_Word16 max_length)
{
#ifdef _DEBUG
    if (length < samples)
    {
        printf(" CopyFromBeginW32 : invector shorter than requested length\n");
        exit(0);
    }
    if (max_length < samples)
    {
        printf(" CopyFromBeginW32 : outvector shorter than requested length\n");
        exit(0);
    }
#endif

    // Unused input variable
    max_length = max_length;
    length = length;

    // Copy the first <samples> of the input vector to vector_out
    // A WebRtc_Word32 is 4 bytes long
    WEBRTC_SPL_MEMCPY_W32(vector_out, vector_in, samples);

    return samples;
}
