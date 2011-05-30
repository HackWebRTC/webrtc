/*
 */
#include <string.h>
#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_CatArraysU8(G_CONST unsigned char *vector1, WebRtc_Word16 len1,
                                    G_CONST unsigned char *vector2, WebRtc_Word16 len2,
                                    unsigned char *outvector, WebRtc_Word16 maxlen)
{
#ifdef _DEBUG
    if (maxlen < len1 + len2)
    {
        printf("chcatarr : out vector is too short\n");
        exit(0);
    }
    if ((len1 != len2) || (len2 < 0))
    {
        printf("chcatarr : input vectors are not of equal length\n");
        exit(0);
    }
#endif
    /* Unused input variable */
    maxlen = maxlen;

    /* Concat the two vectors */
    /* A unsigned char is bytes long */
    WEBRTC_SPL_MEMCPY_W8(outvector, vector1, len1);
    WEBRTC_SPL_MEMCPY_W8(&outvector[len1], vector2, len2);

    return (len1 + len2);
}
