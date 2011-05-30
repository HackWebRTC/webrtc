/*
 * update_energy_from_value.c
 *
 * This file contains the function WebRtcSpl_UpdateEnergyFromValue().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_UpdateEnergyFromValue(WebRtc_Word32 *energy, WebRtc_Word16 weight1,
                                     WebRtc_Word32 new_data, WebRtc_Word16 weight2)
{
    int sh1, sh2;
    WebRtc_Word32 tmp32a, tmp32b;
    WebRtc_Word16 tmp16a, tmp16b;

    tmp32a = *energy; /* Let tmp32a	*/
    tmp32b = new_data; /* Let tmp32b	*/

    /* Make tmp32a to a WebRtc_Word16 in Q(sh1-16) domain */
    sh1 = WebRtcSpl_NormW32(tmp32a);
    tmp16a = (WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(tmp32a, sh1 - 16);

    /* Make tmp32b to a WebRtc_Word16 in Q(sh2-16) domain */
    sh2 = WebRtcSpl_NormW32(tmp32b);
    tmp16b = (WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(tmp32b, sh2 - 16);

    /* Determine weight1*tmp16a + weight2*tmp16b */
    tmp32a = WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(tmp16a, weight1, sh1 - 1);
    tmp32b = WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(tmp16b, weight2, sh2 - 1);

    *energy = tmp32a + tmp32b;
}
