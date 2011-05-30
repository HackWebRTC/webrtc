/*
 * div_u32_u16.c
 *
 * This file contains the function WebRtcSpl_DivU32U16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_UWord32 WebRtcSpl_DivU32U16(WebRtc_UWord32 num, WebRtc_UWord16 den)
{
    // Guard against division with 0
    if (den != 0)
    {
        return ((WebRtc_UWord32)(num / den));
    } else
    {
        return ((WebRtc_UWord32)0xFFFFFFFF);
    }
}
