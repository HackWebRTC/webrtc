/*
 * ones_array_w32.c
 *
 * This file contains the function WebRtcSpl_OnesArrayW32().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_OnesArrayW32(WebRtc_Word32 *vector, WebRtc_Word16 length)
{
    WebRtc_Word16 i;
    WebRtc_Word32 *tmpvec = vector;
    for (i = 0; i < length; i++)
    {
        *tmpvec++ = 1;
    }
    return length;
}
