/*
 * filter_ma.c
 *
 * This file contains the function WebRtcSpl_FilterMA().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

int WebRtcSpl_FilterMA(G_CONST WebRtc_Word16 *b, int b_length, G_CONST WebRtc_Word16 *x,
                       int x_length, WebRtc_Word16 *state, int state_length,
                       WebRtc_Word16 *filtered, int max_length)
{
    WebRtc_Word32 o;
    int i, j, stop;
    WebRtc_Word16 *filtered_ptr = filtered;

    /* Unused input variable */
    max_length = max_length;

    for (i = 0; i < x_length; i++)
    {
        G_CONST WebRtc_Word16 *b_ptr = &b[0];
        G_CONST WebRtc_Word16 *x_ptr = &x[i];
        WebRtc_Word16 *state_ptr = &state[state_length - 1];

        o = (WebRtc_Word32)0;
        stop = (i < b_length) ? i + 1 : b_length;

        for (j = 0; j < stop; j++)
        {
            o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
        }
        for (j = i + 1; j < b_length; j++)
        {
            o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *state_ptr--);
        }

        /* If output is higher than 32768, saturate it. Same with negative side
         2^27 = 134217728, which corresponds to 32768 in Q12 */
        if (o < (WebRtc_Word32)-134217728)
            o = (WebRtc_Word32)-134217728;

        if (o > (WebRtc_Word32)(134217727 - 2048))
            o = (WebRtc_Word32)(134217727 - 2048);

        *filtered_ptr++ = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12);
    }

    /* Save filter state */
    if (x_length >= state_length)
    {
        WebRtcSpl_CopyFromEndW16(x, x_length, b_length - 1, state, state_length);
    } else
    {
        for (i = 0; i < state_length - x_length; i++)
        {
            state[i] = state[i + x_length];
        }
        for (i = 0; i < x_length; i++)
        {
            state[state_length - x_length + i] = x[i];
        }
    }

    return x_length;
}
