/*
 */

#include <string.h>
#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_ReplaceInMidW32(WebRtc_Word32 *in_vector, WebRtc_Word16 in_length,
                                        WebRtc_Word16 pos, WebRtc_Word32 *insert_vector,
                                        WebRtc_Word16 length)
{
#ifdef _DEBUG
    if (in_length < length + pos)
    {
        printf("w32replacemid : vector currently shorter than the length required to insert the samples\n");
        exit(0);
    }
#endif

    /* A WebRtc_Word32 is 4 bytes long */
    WEBRTC_SPL_MEMCPY_W32(&in_vector[pos], insert_vector, length);

    return (in_length);
}
