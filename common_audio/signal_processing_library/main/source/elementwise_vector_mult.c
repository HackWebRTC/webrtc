/*
 * elementwise_vector_mult.c
 *
 * This file contains the function WebRtcSpl_ElementwiseVectorMult().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_ElementwiseVectorMult(WebRtc_Word16 *out, G_CONST WebRtc_Word16 *in,
                                     G_CONST WebRtc_Word16 *win, WebRtc_Word16 vector_length,
                                     WebRtc_Word16 right_shifts)
{
    int i;
    WebRtc_Word16 *outptr = out;
    G_CONST WebRtc_Word16 *inptr = in;
    G_CONST WebRtc_Word16 *winptr = win;
    for (i = 0; i < vector_length; i++)
    {
        (*outptr++) = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(*inptr++,
                                                               *winptr++, right_shifts);
    }
}
