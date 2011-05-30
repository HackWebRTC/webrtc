/*
 * array_shift_w32_to_w16.c
 *
 * This file contains the function WebRtcSpl_ArrayShiftW32ToW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_ArrayShiftW32ToW16(WebRtc_Word16 *res, // (o) Output vector
                                  WebRtc_Word16 length, // (i) Number of samples
                                  G_CONST WebRtc_Word32 *in, // (i) Input vector
                                  WebRtc_Word16 right_shifts) // (i) Number of right shifts
{
    int i;

    if (right_shifts >= 0)
    {
        for (i = length; i > 0; i--)
        {
            (*res++) = (WebRtc_Word16)((*in++) >> right_shifts);
        }
    } else
    {
        WebRtc_Word16 left_shifts = -right_shifts;
        for (i = length; i > 0; i--)
        {
            (*res++) = (WebRtc_Word16)((*in++) << left_shifts);
        }
    }
}
