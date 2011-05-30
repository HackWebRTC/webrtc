/*
 * affine_transform_vector.c
 *
 * This file contains the function WebRtcSpl_AffineTransformVector().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_AffineTransformVector(WebRtc_Word16 *out, WebRtc_Word16 *in,
                                     WebRtc_Word16 gain, WebRtc_Word32 constAdd,
                                     WebRtc_Word16 Rshifts, int length)
{
    WebRtc_Word16 *inPtr;
    WebRtc_Word16 *outPtr;
    int i;

    inPtr = in;
    outPtr = out;
    for (i = 0; i < length; i++)
    {
        (*outPtr++) = (WebRtc_Word16)((WEBRTC_SPL_MUL_16_16((*inPtr++), gain)
                + (WebRtc_Word32)constAdd) >> Rshifts);
    }
}
