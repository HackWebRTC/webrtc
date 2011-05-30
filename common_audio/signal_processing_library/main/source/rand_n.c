/*
 * rand_n.c
 *
 * This file contains the function WebRtcSpl_RandN().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

WebRtc_Word16 WebRtcSpl_RandN(WebRtc_UWord32 *seed)
{
    return (WebRtcSpl_kRandNTable[WebRtcSpl_IncreaseSeed(seed) >> 23]);
}
