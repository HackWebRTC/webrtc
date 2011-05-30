/*
 * max_index_w16.c
 *
 * This file contains the function WebRtcSpl_MaxIndexW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_MaxIndexW16(G_CONST WebRtc_Word16 *vector, WebRtc_Word16 length)
{
    WebRtc_Word16 tempMax;
    WebRtc_Word16 tempMaxIndex, i;
    G_CONST WebRtc_Word16 *tmpvector = vector;

    tempMaxIndex = 0;
    tempMax = *tmpvector++;
    for (i = 1; i < length; i++)
    {
        if ( *tmpvector++ > tempMax)
        {
            tempMax = vector[i];
            tempMaxIndex = i;
        }
    }
    return tempMaxIndex;
}
