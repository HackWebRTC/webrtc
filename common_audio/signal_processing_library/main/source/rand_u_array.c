/*
 * rand_u_array.c
 *
 * This file contains the function WebRtcSpl_RandUArray().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

/*
 * create an array of uniformly distributed variables
 */
WebRtc_Word16 WebRtcSpl_RandUArray(WebRtc_Word16* vector,
                                   WebRtc_Word16 vector_length,
                                   WebRtc_UWord32* seed)
{
    int i;
    for (i = 0; i < vector_length; i++)
    {
        vector[i] = WebRtcSpl_RandU(seed);
    }
    return vector_length;
}
