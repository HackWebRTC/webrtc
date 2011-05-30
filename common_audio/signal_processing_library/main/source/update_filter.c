/*
 * update_filter.c
 *
 * This file contains the function WebRtcSpl_UpdateFilter().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_UpdateFilter(WebRtc_Word16 gain, int vector_length, WebRtc_Word16* phi,
                            WebRtc_Word16* H)
{
    int i;

    for (i = 0; i < vector_length; i++)
    {
        *H += (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(gain, *phi++, 16);
        H++;
    }
}
