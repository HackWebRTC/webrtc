/*
 * max_value_w16.c
 *
 * This file contains the function WebRtcSpl_MaxValueW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifndef XSCALE_OPT

WebRtc_Word16 WebRtcSpl_MaxValueW16(G_CONST WebRtc_Word16* vector, WebRtc_Word16 length)
{
    WebRtc_Word16 tempMax;
    WebRtc_Word16 i;
    G_CONST WebRtc_Word16 *tmpvector = vector;

    tempMax = *tmpvector++;
    for (i = 1; i < length; i++)
    {
        if (*tmpvector++ > tempMax)
            tempMax = vector[i];
    }
    return tempMax;
}

#else
#pragma message(">> max_value_w16.c is excluded from this build")
#endif
