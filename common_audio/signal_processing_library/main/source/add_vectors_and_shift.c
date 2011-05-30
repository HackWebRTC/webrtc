/*
 * add_vectors_and_shift.c
 *
 * This file contains the function WebRtcSpl_AddVectorsAndShift().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_AddVectorsAndShift(WebRtc_Word16 *out, G_CONST WebRtc_Word16 *in1,
                                  G_CONST WebRtc_Word16 *in2, WebRtc_Word16 vector_length,
                                  WebRtc_Word16 right_shifts)
{
    int i;
    WebRtc_Word16 *outptr = out;
    G_CONST WebRtc_Word16 *in1ptr = in1;
    G_CONST WebRtc_Word16 *in2ptr = in2;
    for (i = vector_length; i > 0; i--)
    {
        (*outptr++) = (WebRtc_Word16)(((*in1ptr++) + (*in2ptr++)) >> right_shifts);
    }
}
