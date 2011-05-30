/*
 * max_value_w32.c
 *
 * This file contains the function WebRtcSpl_MaxValueW32().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifndef XSCALE_OPT

WebRtc_Word32 WebRtcSpl_MaxValueW32(G_CONST WebRtc_Word32* vector, // (i) Input vector
                                    WebRtc_Word16 length) // (i) Number of elements
{
    WebRtc_Word32 tempMax;
    WebRtc_Word16 i;
    G_CONST WebRtc_Word32 *tmpvector = vector;

    tempMax = *tmpvector++;
    for (i = 1; i < length; i++)
    {
        if (*tmpvector++ > tempMax)
            tempMax = vector[i];
    }
    return tempMax;
}

#else
#pragma message(">> max_value_w32.c is excluded from this build")
#endif
