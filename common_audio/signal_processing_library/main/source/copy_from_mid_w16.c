/*
 */
#include <string.h>
#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_CopyFromMidW16(G_CONST WebRtc_Word16 *vector_in, WebRtc_Word16 length,
                                       WebRtc_Word16 startpos, WebRtc_Word16 samples,
                                       WebRtc_Word16 *vector_out, WebRtc_Word16 max_length)
{
#ifdef _DEBUG
    if (length < samples + startpos)
    {
        printf("w16mid : invector copy out of bounds\n");
        exit(0);
    }
    if (max_length < samples)
    {
        printf("w16mid : outvector shorter than requested length\n");
        exit(0);
    }
#endif
    /* Unused input variable */
    length = length;
    max_length = max_length;

    /* Copy the <samples> from pos <start> of the input vector to vector_out */
    /* A WebRtc_Word16 is 2 bytes long */
    WEBRTC_SPL_MEMCPY_W16(vector_out, &vector_in[startpos], samples);

    return (samples);
}
