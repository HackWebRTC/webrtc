/*
 * array_shift_w16.c
 *
 * This file contains the function WebRtcSpl_ArrayShiftW16().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_ArrayShiftW16(WebRtc_Word16 *res,
                             WebRtc_Word16 length,
                             G_CONST WebRtc_Word16 *in,
                             WebRtc_Word16 right_shifts)
{
    int i;

    if (right_shifts > 0)
    {
        for (i = length; i > 0; i--)
        {
            (*res++) = ((*in++) >> right_shifts);
        }
    } else
    {
        for (i = length; i > 0; i--)
        {
            (*res++) = ((*in++) << (-right_shifts));
        }
    }
}
