/*
 * rand_u.c
 *
 * This file contains the function WebRtcSpl_RandU().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_RandU(WebRtc_UWord32 *seed)
{
    return ((WebRtc_Word16)(WebRtcSpl_IncreaseSeed(seed) >> 16));
}
