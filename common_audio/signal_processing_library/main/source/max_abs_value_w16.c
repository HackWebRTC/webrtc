/*
 * max_abs_value_w16.c
 *
 * This file contains the function WebRtcSpl_MaxAbsValueW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_MaxAbsValueW16(G_CONST WebRtc_Word16 *vector, WebRtc_Word16 length)
{
    WebRtc_Word32 tempMax = 0;
    WebRtc_Word32 absVal;
    WebRtc_Word16 totMax;
    int i;
    G_CONST WebRtc_Word16 *tmpvector = vector;

#ifdef _ARM_OPT_
#pragma message("NOTE: _ARM_OPT_ optimizations are used")
    WebRtc_Word16 len4 = (length >> 2) << 2;
#endif

#ifndef _ARM_OPT_
    for (i = 0; i < length; i++)
    {
        absVal = WEBRTC_SPL_ABS_W32((*tmpvector));
        if (absVal > tempMax)
        {
            tempMax = absVal;
        }
        tmpvector++;
    }
#else
    for (i = 0; i < len4; i = i + 4)
    {
        absVal = WEBRTC_SPL_ABS_W32((*tmpvector));
        if (absVal > tempMax)
        {
            tempMax = absVal;
        }
        tmpvector++;
        absVal = WEBRTC_SPL_ABS_W32((*tmpvector));
        if (absVal > tempMax)
        {
            tempMax = absVal;
        }
        tmpvector++;
        absVal = WEBRTC_SPL_ABS_W32((*tmpvector));
        if (absVal > tempMax)
        {
            tempMax = absVal;
        }
        tmpvector++;
        absVal = WEBRTC_SPL_ABS_W32((*tmpvector));
        if (absVal > tempMax)
        {
            tempMax = absVal;
        }
        tmpvector++;
    }

    for (i = len4; i < len; i++)
    {
        absVal = WEBRTC_SPL_ABS_W32((*tmpvector));
        if (absVal > tempMax)
        {
            tempMax = absVal;
        }
        tmpvector++;
    }
#endif
    totMax = (WebRtc_Word16)WEBRTC_SPL_MIN(tempMax, WEBRTC_SPL_WORD16_MAX);
    return totMax;
}
