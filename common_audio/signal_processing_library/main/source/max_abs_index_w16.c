/*
 * max_abs_index_w16.c
 *
 * This file contains the function WebRtcSpl_MaxAbsIndexW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_MaxAbsIndexW16(G_CONST WebRtc_Word16* vector,
                                       WebRtc_Word16 vector_length)
{
    WebRtc_Word16 tempMax;
    WebRtc_Word16 absTemp;
    WebRtc_Word16 tempMaxIndex, i;
    G_CONST WebRtc_Word16 *tmpvector = vector;

    tempMaxIndex = 0;
    tempMax = WEBRTC_SPL_ABS_W16(*tmpvector);
    tmpvector++;
    for (i = 1; i < vector_length; i++)
    {
        absTemp = WEBRTC_SPL_ABS_W16(*tmpvector);
        tmpvector++;
        if (absTemp > tempMax)
        {
            tempMax = absTemp;
            tempMaxIndex = i;
        }
    }
    return tempMaxIndex;
}
