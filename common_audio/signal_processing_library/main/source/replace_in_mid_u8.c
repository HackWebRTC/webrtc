/*
 */

#include <string.h>

#include "signal_processing_library.h"

#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

WebRtc_Word16 WebRtcSpl_ReplaceInMidU8(unsigned char *in_vector, WebRtc_Word16 in_length,
                                       WebRtc_Word16 pos, unsigned char *insert_vector,
                                       WebRtc_Word16 insert_length)
{
#ifdef _DEBUG
    if (in_length < insert_length + pos)
    {
        printf("chreplacemid : vector currently shorter than the length required to insert the samples\n");
        exit(0);
    }
#endif

    /* A unsigned char is 1 bytes long */
    WEBRTC_SPL_MEMCPY_W8(&in_vector[pos], insert_vector, insert_length);

    return (in_length);
}
