/*
 */
#include <string.h>
#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_ReplaceInMidW16(WebRtc_Word16 *in_vector, WebRtc_Word16 in_length,
                                        WebRtc_Word16 pos, WebRtc_Word16 *insert_vector,
                                        WebRtc_Word16 insert_length)
{
#ifdef _DEBUG
    if (in_length < insert_length + pos)
    {
        printf("w16replacemid : vector currently shorter than the length required to insert the samples\n");
        exit(0);
    }
#endif

    /* A WebRtc_Word16 is 2 bytes long */
    WEBRTC_SPL_MEMCPY_W16(&in_vector[pos], insert_vector, insert_length);

    return (in_length);
}
