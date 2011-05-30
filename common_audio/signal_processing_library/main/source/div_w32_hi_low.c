/*
 * div_w32_hi_low.c
 *
 * This file contains the function WebRtcSpl_DivW32HiLow().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word32 WebRtcSpl_DivW32HiLow(WebRtc_Word32 num, WebRtc_Word16 den_hi,
                                    WebRtc_Word16 den_low)
{
    WebRtc_Word16 approx, tmp_hi, tmp_low, num_hi, num_low;
    WebRtc_Word32 tmpW32;

    approx = (WebRtc_Word16)WebRtcSpl_DivW32W16((WebRtc_Word32)0x1FFFFFFF, den_hi);
    // result in Q14 (Note: 3FFFFFFF = 0.5 in Q30)

    // tmpW32 = 1/den = approx * (2.0 - den * approx) (in Q30)
    tmpW32 = (WEBRTC_SPL_MUL_16_16(den_hi, approx) << 1)
            + ((WEBRTC_SPL_MUL_16_16(den_low, approx) >> 15) << 1);
    // tmpW32 = den * approx

    tmpW32 = (WebRtc_Word32)0x7fffffffL - tmpW32; // result in Q30 (tmpW32 = 2.0-(den*approx))

    // Store tmpW32 in hi and low format
    tmp_hi = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmpW32, 16);
    tmp_low = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((tmpW32
            - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)tmp_hi, 16)), 1);

    // tmpW32 = 1/den in Q29
    tmpW32 = ((WEBRTC_SPL_MUL_16_16(tmp_hi, approx) + (WEBRTC_SPL_MUL_16_16(tmp_low, approx)
            >> 15)) << 1);

    // 1/den in hi and low format
    tmp_hi = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmpW32, 16);
    tmp_low = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((tmpW32
            - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)tmp_hi, 16)), 1);

    // Store num in hi and low format
    num_hi = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(num, 16);
    num_low = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((num
            - WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)num_hi, 16)), 1);

    // num * (1/den) by 32 bit multiplication (result in Q28)

    tmpW32 = (WEBRTC_SPL_MUL_16_16(num_hi, tmp_hi) + (WEBRTC_SPL_MUL_16_16(num_hi, tmp_low)
            >> 15) + (WEBRTC_SPL_MUL_16_16(num_low, tmp_hi) >> 15));

    // Put result in Q31 (convert from Q28)
    tmpW32 = WEBRTC_SPL_LSHIFT_W32(tmpW32, 3);

    return tmpW32;
}
