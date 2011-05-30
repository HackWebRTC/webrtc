/*
 */

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_MinValueW16(G_CONST WebRtc_Word16 *vector, WebRtc_Word16 vector_length)
{
    WebRtc_Word16 tempMin;
    WebRtc_Word16 i;
    G_CONST WebRtc_Word16 *tmpvector = vector;

    /* Find the minimum value */
    tempMin = *tmpvector++;
    for (i = 1; i < vector_length; i++)
    {
        if ( *tmpvector++ < tempMin)
            tempMin = (vector[i]);
    }
    return tempMin;
}
