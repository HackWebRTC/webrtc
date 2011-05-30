/*
 * zeros_array_w16.c
 *
 * This file contains the function WebRtcSpl_ZerosArrayW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_ZerosArrayW16(WebRtc_Word16 *vector, WebRtc_Word16 length)
{
    WebRtcSpl_MemSetW16(vector, 0, length);
    return length;
}
