/*
 * update_energy_from_array.c
 *
 * This file contains the function WebRtcSpl_UpdateEnergyFromArray().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_UpdateEnergyFromArray(WebRtc_Word32 *E, WebRtc_Word16 *vector,
                                     WebRtc_Word16 vector_length, WebRtc_Word16 alpha,
                                     WebRtc_Word16 round_factor)
{
    int loop;
    WebRtc_Word32 tmp32a;

    for (loop = 0; loop < vector_length; loop++)
    {
        tmp32a = WEBRTC_SPL_MUL_16_16(vector[loop], vector[loop]);
        tmp32a = (WebRtc_Word32)(tmp32a - *E + round_factor); // rounding factor
        tmp32a = tmp32a >> alpha;
        *E += tmp32a;
    }
}
