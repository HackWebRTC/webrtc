/*
 * zeros_array_w32.c
 *
 * This file contains the function WebRtcSpl_ZerosArrayW32().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_ZerosArrayW32(WebRtc_Word32 *vector, WebRtc_Word16 length)
{
    WebRtcSpl_MemSetW32(vector, 0, length);
    return length;
}
