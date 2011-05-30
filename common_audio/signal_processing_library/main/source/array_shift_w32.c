/*
 * array_shift_w32.c
 *
 * This file contains the function WebRtcSpl_ArrayShiftW32().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_ArrayShiftW32(WebRtc_Word32 *out_vector, // (o) Output vector
                             WebRtc_Word16 vector_length, // (i) Number of samples
                             G_CONST WebRtc_Word32 *in_vector, // (i) Input vector
                             WebRtc_Word16 right_shifts) // (i) Number of right shifts
{
    int i;

    if (right_shifts > 0)
    {
        for (i = vector_length; i > 0; i--)
        {
            (*out_vector++) = ((*in_vector++) >> right_shifts);
        }
    } else
    {
        for (i = vector_length; i > 0; i--)
        {
            (*out_vector++) = ((*in_vector++) << (-right_shifts));
        }
    }
}
