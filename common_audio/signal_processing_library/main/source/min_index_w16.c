/*
 * min_index_w16.c
 *
 * This file contains the function WebRtcSpl_MinIndexW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifndef XSCALE_OPT

WebRtc_Word16 WebRtcSpl_MinIndexW16(G_CONST WebRtc_Word16* vector, WebRtc_Word16 vector_length)
{
    WebRtc_Word16 tempMin;
    WebRtc_Word16 tempMinIndex, i;
    G_CONST WebRtc_Word16 *tmpvector = vector;

    // Find index of smallest value
    tempMinIndex = 0;
    tempMin = *tmpvector++;
    for (i = 1; i < vector_length; i++)
    {
        if (*tmpvector++ < tempMin)
        {
            tempMin = vector[i];
            tempMinIndex = i;
        }
    }
    return tempMinIndex;
}

#else
#pragma message(">> min_index_w16.c is excluded from this build")
#endif
