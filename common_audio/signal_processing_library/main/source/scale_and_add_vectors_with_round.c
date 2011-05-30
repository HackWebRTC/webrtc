/*
 * scale_and_add_vectors_with_round.c
 *
 * This file contains the function WebRtcSpl_ScaleAndAddVectorsWithRound().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_ScaleAndAddVectorsWithRound(WebRtc_Word16 *vec1, WebRtc_Word16 scale1,
                                           WebRtc_Word16 *vec2, WebRtc_Word16 scale2,
                                           WebRtc_Word16 rshifts, WebRtc_Word16 *out,
                                           WebRtc_Word16 length)
{
    int i;
    WebRtc_Word16 roundVal;
    roundVal = 1 << rshifts;
    roundVal = roundVal >> 1;
    for (i = 0; i < length; i++)
    {
        out[i] = (WebRtc_Word16)((WEBRTC_SPL_MUL_16_16(vec1[i],scale1)
                + WEBRTC_SPL_MUL_16_16(vec2[i],scale2) + roundVal) >> rshifts);
    }
}
