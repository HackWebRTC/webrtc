/*
 * rand_n_array.c
 *
 * This file contains the function WebRtcSpl_RandNArray().
 * The description header can be found in signal_processing_library.h
 *
 */

#include <string.h>

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_RandNArray(WebRtc_Word16* vector,
                                   WebRtc_Word16 vector_length,
                                   WebRtc_UWord32* seed)
{
    WebRtc_Word16 startpos;
    WebRtc_Word16 endpos;
    WebRtc_Word16* vecptr;

    startpos = (WebRtc_Word16)((*seed) & 0x1FF); // Value between 0 and 511
    *seed = *seed + vector_length;
    endpos = (WebRtc_Word16)((*seed) & 0x1FF); // Value between 0 and 511

    if (vector_length < 512)
    {
        if (endpos > startpos)
        {
            WEBRTC_SPL_MEMCPY_W16(vector, &WebRtcSpl_kRandNTable[startpos], vector_length);
        } else
        {
            WEBRTC_SPL_MEMCPY_W16(vector, &WebRtcSpl_kRandNTable[startpos], (512 - startpos));
            WEBRTC_SPL_MEMCPY_W16(&vector[512-startpos], WebRtcSpl_kRandNTable,
                                  (vector_length - (512 - startpos)));
        }
    } else
    {
        WebRtc_Word16 lensave = vector_length;

        WEBRTC_SPL_MEMCPY_W16(vector, &WebRtcSpl_kRandNTable[startpos], (512-startpos));
        vecptr = &vector[512 - startpos];
        vector_length = vector_length - (512 - startpos);
        while (vector_length > 512)
        {
            WEBRTC_SPL_MEMCPY_W16(vecptr, WebRtcSpl_kRandNTable, 512);
            vecptr += 512;
            vector_length -= 512;
        }
        WEBRTC_SPL_MEMCPY_W16(vecptr, WebRtcSpl_kRandNTable, vector_length);
        vector_length = lensave;
    }
    return vector_length;
}
