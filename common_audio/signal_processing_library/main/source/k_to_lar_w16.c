/*
 * k_to_lar_w16.c
 *
 * This file contains the function WebRtcSpl_KToLarW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_KToLarW16(WebRtc_Word16 *kLar, int use_order)
{
    // The LARs are computed from the reflection coefficients using
    // a linear approximation of the logarithm.
    WebRtc_Word16 tmp16;
    int i;
    for (i = 0; i < use_order; i++, kLar++)
    {
        tmp16 = WEBRTC_SPL_ABS_W16( *kLar );
        if (tmp16 < 22118)
            tmp16 >>= 1;
        else if (tmp16 < 31130)
            tmp16 -= 11059;
        else
        {
            tmp16 -= 26112;
            tmp16 <<= 2;
        }
        *kLar = *kLar < 0 ? -tmp16 : tmp16;
    }
}
