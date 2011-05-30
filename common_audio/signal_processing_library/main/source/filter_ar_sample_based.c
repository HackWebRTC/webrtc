/*
 * filter_ar_sample_based.c
 *
 * This file contains the function WebRtcSpl_FilterARSampleBased().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_FilterARSampleBased(WebRtc_Word16 *InOut, WebRtc_Word16 *InOutLOW,
                                   WebRtc_Word16 *Coef, WebRtc_Word16 orderCoef)
{
    int k;
    WebRtc_Word32 temp, tempLOW;
    WebRtc_Word16 *ptrIn, *ptrInLOW, *ptrCoef;

    temp = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)*InOut, 12);
    tempLOW = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)*InOutLOW, 12);

    // Filter integer part
    ptrIn = InOut - 1;
    ptrCoef = Coef + 1;
    for (k = 0; k < orderCoef; k++)
    {
        temp -= WEBRTC_SPL_MUL_16_16((*ptrCoef), (*ptrIn));
        ptrCoef++;
        ptrIn--;
    }

    // Filter lower part (Q12)
    ptrInLOW = InOutLOW - 1;
    ptrCoef = Coef + 1;
    for (k = 0; k < orderCoef; k++)
    {
        tempLOW -= WEBRTC_SPL_MUL_16_16((*ptrCoef), (*ptrInLOW));
        ptrCoef++;
        ptrInLOW--;
    }

    temp += WEBRTC_SPL_RSHIFT_W32(tempLOW, 12); // build WebRtc_Word32 result in Q12

    // 2048 == (0.5 << 12) for rounding, InOut is in (Q0)
    *InOut = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((temp+2048), 12);

    // InOutLOW is in Q12
    *InOutLOW = (WebRtc_Word16)(temp - (WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)(*InOut), 12)));
}
