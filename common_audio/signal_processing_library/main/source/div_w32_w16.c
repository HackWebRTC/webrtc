/*
 * div_w32_w16.c
 *
 * This file contains the function WebRtcSpl_DivW32W16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word32 WebRtcSpl_DivW32W16(WebRtc_Word32 num, WebRtc_Word16 den)
{
    // Guard against division with 0
    if (den != 0)
    {
        return ((WebRtc_Word32)(num / den));
    } else
    {
        return ((WebRtc_Word32)0x7FFFFFFF);
    }
}
