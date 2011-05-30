/*
 * max_abs_value_w32.c
 *
 * This file contains the function WebRtcSpl_MaxAbsValueW32().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word32 WebRtcSpl_MaxAbsValueW32(G_CONST WebRtc_Word32 *vector, // (i) Input vector
                                       WebRtc_Word16 length) // (i) Number of elements
{
    WebRtc_UWord32 tempMax = 0;
    WebRtc_UWord32 absVal;
    WebRtc_Word32 retval;
    int i;
    G_CONST WebRtc_Word32 *tmpvector = vector;

    for (i = 0; i < length; i++)
    {
        absVal = WEBRTC_SPL_ABS_W32((*tmpvector));
        if (absVal > tempMax)
        {
            tempMax = absVal;
        }
        tmpvector++;
    }
    retval = (WebRtc_Word32)(WEBRTC_SPL_MIN(tempMax, WEBRTC_SPL_WORD32_MAX));
    return retval;
}
