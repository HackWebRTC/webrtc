/*
 * copy_from_mid_u8.c
 *
 * This file contains the function WebRtcSpl_CopyFromMidU8().
 * The description header can be found in signal_processing_library.h
 *
 */

#include <string.h>
#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_CopyFromMidU8(unsigned char *vector_in, WebRtc_Word16 length,
                                      WebRtc_Word16 startpos, WebRtc_Word16 samples,
                                      unsigned char *vector_out, WebRtc_Word16 max_length)
{
#ifdef _DEBUG
    if (length < samples + startpos)
    {
        printf("chmid : invector copy out of bounds\n");
        exit(0);
    }
    if (max_length < samples)
    {
        printf("chmid : outvector shorter than requested length\n");
        exit(0);
    }
#endif
    /* Unused input variable */
    max_length = max_length;
    length = length;

    /* Copy the <samples> from pos <start> of the input vector to vector_out */
    /* A unsigned char is 1 bytes long */
    WEBRTC_SPL_MEMCPY_W8(vector_out,&vector_in[startpos],samples);

    return (samples);
}
