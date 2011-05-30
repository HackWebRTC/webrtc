/*
 * max_index_w32.c
 *
 * This file contains the function WebRtcSpl_MaxIndexW32().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_MaxIndexW32(G_CONST WebRtc_Word32* vector, // (i) Input vector
                                    WebRtc_Word16 length) // (i) Number of elements
{
    WebRtc_Word32 tempMax;
    WebRtc_Word16 tempMaxIndex, i;
    G_CONST WebRtc_Word32 *tmpvector = vector;

    tempMaxIndex = 0;
    tempMax = *tmpvector++;
    for (i = 1; i < length; i++)
    {
        if (*tmpvector++ > tempMax)
        {
            tempMax = vector[i];
            tempMaxIndex = i;
        }
    }
    return tempMaxIndex;
}
