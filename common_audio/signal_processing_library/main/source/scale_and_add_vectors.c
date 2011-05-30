/*
 * scale_and_add_vectors.c
 *
 * This file contains the function WebRtcSpl_ScaleAndAddVectors().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_ScaleAndAddVectors(G_CONST WebRtc_Word16 *in1, WebRtc_Word16 gain1, int shift1,
                                  G_CONST WebRtc_Word16 *in2, WebRtc_Word16 gain2, int shift2,
                                  WebRtc_Word16 *out, int vector_length)
{
    // Performs vector operation: out = (gain1*in1)>>shift1 + (gain2*in2)>>shift2
    int i;
    G_CONST WebRtc_Word16 *in1ptr;
    G_CONST WebRtc_Word16 *in2ptr;
    WebRtc_Word16 *outptr;

    in1ptr = in1;
    in2ptr = in2;
    outptr = out;

    for (i = 0; i < vector_length; i++)
    {
        (*outptr++) = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(gain1, *in1ptr++, shift1)
                + (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(gain2, *in2ptr++, shift2);
    }
}
