/*
 * k_to_lar_w32.c
 *
 * This file contains the function WebRtcSpl_KToLarW32().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_KToLarW32(WebRtc_Word32 *kLar, int use_order)
{
    // The LARs are computed from the reflection coefficients using
    // a linear approximation of the logarithm.
    WebRtc_Word32 tmp;
    int i;
    for (i = 0; i < use_order; i++, kLar++)
    {
        tmp = WEBRTC_SPL_ABS_W16(*kLar);
        if (tmp < (WebRtc_Word32)1300000000)
            tmp >>= 1;
        else if (tmp < (WebRtc_Word32)2000000000)
            tmp -= 650000000;
        else
        {
            tmp -= 1662500000;
            tmp <<= 2;
        }
        *kLar = *kLar < 0 ? -tmp : tmp;
    }
}
