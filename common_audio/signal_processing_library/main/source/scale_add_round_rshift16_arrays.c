/*
 * scale_and_add_vectors_r_shift16.c
 *
 * This file contains the function WebRtcSpl_ScaleAndAddVectorsRShift16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_ScaleAndAddVectorsRShift16(G_CONST WebRtc_Word16 *in1, WebRtc_Word16 gain1,
                                           G_CONST WebRtc_Word16 *in2, WebRtc_Word16 gain2,
                                           WebRtc_Word16 *out, int nrOfElements)
{
    /* Performs vector operation: out = (gain1*in1+2^15)>>16 + (gain2*in2+2^15)>>16  */
    int i;
    G_CONST WebRtc_Word16 *in1ptr;
    G_CONST WebRtc_Word16 *in2ptr;
    WebRtc_Word16 *outptr;

    in1ptr = in1;
    in2ptr = in2;
    outptr = out;

    for (i = 0; i < nrOfElements; i++)
    {
        ( *outptr++)
                = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((WEBRTC_SPL_MUL_16_16(gain1, *in1ptr++) + (WebRtc_Word32)32768), 16)
                        + (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((WEBRTC_SPL_MUL_16_16(gain2, *in2ptr++) + (WebRtc_Word32)32768), 16);
    }
}
