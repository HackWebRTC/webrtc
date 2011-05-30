/*
 * add_affine_vector_to_vector.c
 *
 * This file contains the function WebRtcSpl_AddAffineVectorToVector().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_AddAffineVectorToVector(WebRtc_Word16 *out, WebRtc_Word16 *in,
                                       WebRtc_Word16 gain, WebRtc_Word32 add_constant,
                                       WebRtc_Word16 right_shifts, int vector_length)
{
    WebRtc_Word16 *inPtr;
    WebRtc_Word16 *outPtr;
    int i;

    inPtr = in;
    outPtr = out;
    for (i = 0; i < vector_length; i++)
    {
        (*outPtr++) += (WebRtc_Word16)((WEBRTC_SPL_MUL_16_16((*inPtr++), gain)
                + (WebRtc_Word32)add_constant) >> right_shifts);
    }
}
