/*
 */

#include <string.h>
#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_CatArraysW32(G_CONST WebRtc_Word32 *vector1, WebRtc_Word16 len1,
                                     G_CONST WebRtc_Word32 *vector2, WebRtc_Word16 len2,
                                     WebRtc_Word32 *outvector, WebRtc_Word16 maxlen)
{
#ifdef _DEBUG
    if (maxlen < len1 + len2)
    {
        printf("w32catarr : out vector is too short\n");
        exit(0);
    }
    if ((len1 != len2) || (len2 < 0))
    {
        printf("w32catarr : input vectors are not of equal length\n");
        exit(0);
    }
#endif

    /* Unused input variable */
    maxlen = maxlen;

    /* Concat the two vectors */
    /* A word32 is 4 bytes long */
    WEBRTC_SPL_MEMCPY_W32(outvector, vector1, len1);
    WEBRTC_SPL_MEMCPY_W32(&outvector[len1], vector2, len2);

    return (len1 + len2);
}
