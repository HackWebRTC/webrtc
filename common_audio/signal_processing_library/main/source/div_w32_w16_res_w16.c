/*
 * div_w32_w16_res_w16.c
 *
 * This file contains the function WebRtcSpl_DivW32W16ResW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_DivW32W16ResW16(WebRtc_Word32 num, WebRtc_Word16 den)
{
    // Guard against division with 0
    if (den != 0)
    {
        return (WebRtc_Word16)(num / den);
    } else
    {
        return (WebRtc_Word16)0x7FFF;
    }
}
