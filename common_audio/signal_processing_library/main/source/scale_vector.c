/*
 * scale_vector.c
 *
 * This file contains the function WebRtcSpl_ScaleVector().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_ScaleVector(G_CONST WebRtc_Word16 *in_vector, WebRtc_Word16 *out_vector,
                           WebRtc_Word16 gain, WebRtc_Word16 in_vector_length,
                           WebRtc_Word16 right_shifts)
{
    // Performs vector operation: out_vector = (gain*in_vector)>>right_shifts
    int i;
    G_CONST WebRtc_Word16 *inptr;
    WebRtc_Word16 *outptr;

    inptr = in_vector;
    outptr = out_vector;

    for (i = 0; i < in_vector_length; i++)
    {
        (*outptr++) = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(*inptr++, gain, right_shifts);
    }
}
