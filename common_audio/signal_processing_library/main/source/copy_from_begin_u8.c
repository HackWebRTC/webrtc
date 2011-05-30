/*
 * copy_from_begin_u8.c
 *
 * This file contains the function WebRtcSpl_CopyFromBeginU8().
 * The description header can be found in signal_processing_library.h
 *
 */

#include <string.h>
#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_CopyFromBeginU8(G_CONST unsigned char *vector_in,
                                        WebRtc_Word16 length,
                                        WebRtc_Word16 samples,
                                        unsigned char *vector_out,
                                        WebRtc_Word16 max_length)
{
#ifdef _DEBUG
    if (length < samples)
    {
        printf("CopyFromBeginU8 : vector_in shorter than requested length\n");
        exit(0);
    }
    if (max_length < samples)
    {
        printf("CopyFromBeginU8 : vector_out shorter than requested length\n");
        exit(0);
    }
#endif

    // Unused input variable
    max_length = max_length;
    length = length;

    // Copy the first <samples> of the input vector to vector_out
    // A unsigned char is 1 bytes long
    WEBRTC_SPL_MEMCPY_W8(vector_out, vector_in, samples);

    return samples;
}
