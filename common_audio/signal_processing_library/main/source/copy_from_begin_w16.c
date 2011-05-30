/*
 * copy_from_begin_w16.c
 *
 * This file contains the function WebRtcSpl_CopyFromBeginW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include <string.h>
#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_CopyFromBeginW16(G_CONST WebRtc_Word16 *vector_in,
                                         WebRtc_Word16 length,
                                         WebRtc_Word16 samples,
                                         WebRtc_Word16 *vector_out,
                                         WebRtc_Word16 max_length)
{
#ifdef _DEBUG
    if (length < samples)
    {
        printf(" CopyFromBeginW16 : vector_in shorter than requested length\n");
        exit(0);
    }
    if (max_length < samples)
    {
        printf(" CopyFromBeginW16 : vector_out shorter than requested length\n");
        exit(0);
    }
#endif
    // Unused input variable
    length = length;
    max_length = max_length;

    // Copy the first <samples> of the input vector to vector_out
    // A WebRtc_Word16 is 2 bytes long
    WEBRTC_SPL_MEMCPY_W16(vector_out, vector_in, samples);

    return samples;
}
