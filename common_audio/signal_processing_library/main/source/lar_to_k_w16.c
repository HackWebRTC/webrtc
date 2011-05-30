/*
 * lar_to_refl_coef_w16.c
 *
 * This file contains the function WebRtcSpl_LarToReflCoefW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_LarToReflCoefW16(WebRtc_Word16 *kLAR, int use_order)
{
    int i;
    WebRtc_Word16 temp;
    for (i = 0; i < use_order; i++, kLAR++)
    {
        if ( *kLAR < 0)
        {
            temp = *kLAR == WEBRTC_SPL_WORD16_MIN ? WEBRTC_SPL_WORD16_MAX : -( *kLAR);
            *kLAR = -((temp < 11059) ? temp << 1 : ((temp < 20070) ? temp + 11059
                    : WEBRTC_SPL_ADD_SAT_W16( temp >> 2, 26112 )));
        } else
        {
            temp = *kLAR;
            *kLAR = (temp < 11059) ? temp << 1 : ((temp < 20070) ? temp + 11059
                    : WEBRTC_SPL_ADD_SAT_W16( temp >> 2, 26112 ));
        }
    }
}
