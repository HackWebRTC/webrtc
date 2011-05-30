/*
 * lar_to_refl_coef_w32.c
 *
 * This file contains the function WebRtcSpl_LarToReflCoefW32().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_LarToReflCoefW32(WebRtc_Word32 *kLAR, int use_order)
{
    int i;
    WebRtc_Word32 temp;
    for (i = 0; i < use_order; i++, kLAR++)
    {
        if (*kLAR < 0)
        {
            temp = (*kLAR == WEBRTC_SPL_WORD32_MIN) ? WEBRTC_SPL_WORD32_MAX : -(*kLAR);
            *kLAR = -((temp < (WebRtc_Word32)650000000) ? temp << 1 : ((temp
                    < (WebRtc_Word32)1350000000) ? temp + 650000000
                    : WEBRTC_SPL_ADD_SAT_W32( temp >> 2, 1662500000 )));
        } else
        {
            temp = *kLAR;
            *kLAR = (temp < (WebRtc_Word32)650000000) ? temp << 1 : ((temp
                    < (WebRtc_Word32)1350000000) ? temp + 650000000
                    : WEBRTC_SPL_ADD_SAT_W32( temp >> 2, 1662500000 ));
        }

    }
}
