/*
 * dot_product.c
 *
 * This file contains the function WebRtcSpl_DotProduct().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word32 WebRtcSpl_DotProduct(WebRtc_Word16 *vector1, WebRtc_Word16 *vector2, int length)
{
    WebRtc_Word32 sum;
    int i;

    sum = 0;
    for (i = 0; i < length; i++)
    {
        sum += WEBRTC_SPL_MUL_16_16(*vector1++, *vector2++);
    }
    return sum;
}

