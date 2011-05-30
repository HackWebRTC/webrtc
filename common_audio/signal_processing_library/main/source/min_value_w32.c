/*
 */

#include "signal_processing_library.h"

/* (o) Minimum value of input vector */
WebRtc_Word32 WebRtcSpl_MinValueW32(G_CONST WebRtc_Word32 *vector, /* (i) Input vector */
                                    WebRtc_Word16 vector_length) /* (i) Number of elements */
{
    WebRtc_Word32 tempMin;
    WebRtc_Word16 i;
    G_CONST WebRtc_Word32 *tmpvector = vector;

    /* Find the minimum value */
    tempMin = *tmpvector++;
    for (i = 1; i < vector_length; i++)
    {
        if ( *tmpvector++ < tempMin)
            tempMin = (vector[i]);
    }
    return tempMin;
}
