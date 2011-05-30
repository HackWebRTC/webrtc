/*
 * filter_ma4.c
 *
 * This file contains the function WebRtcSpl_FilterMA4().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

int WebRtcSpl_FilterMA4(G_CONST WebRtc_Word16 *b, int b_length, G_CONST WebRtc_Word16 *x,
                        int x_length, WebRtc_Word16 *state, int state_length,
                        WebRtc_Word16 *filtered, int max_length)
{
    WebRtc_Word32 o;
    int i;

    WebRtc_Word16 *filtered_ptr = filtered;
    /* Calculate filtered[0] */G_CONST WebRtc_Word16 *b_ptr = &b[0];
    G_CONST WebRtc_Word16 *x_ptr = &x[0];
    WebRtc_Word16 *state_ptr = &state[state_length - 1];

    /* Unused input variable */
    max_length = max_length;

    o = (WebRtc_Word32)0;
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *state_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *state_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *state_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *state_ptr--);
    *filtered_ptr++ = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12); // Q12 operations

#ifdef _DEBUG
    if (max_length < x_length)
    {
        printf("FilterMA4: out vector is shorter than in vector\n");
        exit(0);
    }
    if ((state_length != 4) || (b_length != 5))
    {
        printf("FilterMA4: state or coefficient vector does not have the correct length\n");
        exit(0);
    }
#endif

    /* Calculate filtered[1] */
    b_ptr = &b[0];
    x_ptr = &x[1];
    state_ptr = &state[state_length - 1];
    o = (WebRtc_Word32)0;
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *state_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *state_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *state_ptr--);
    *filtered_ptr++ = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12); // Q12 operations

    /* Calculate filtered[2] */
    b_ptr = &b[0];
    x_ptr = &x[2];
    state_ptr = &state[state_length - 1];
    o = (WebRtc_Word32)0;
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *state_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *state_ptr--);
    *filtered_ptr++ = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12); // Q12 operations

    /* Calculate filtered[3] */
    b_ptr = &b[0];
    x_ptr = &x[3];
    state_ptr = &state[state_length - 1];
    o = (WebRtc_Word32)0;
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
    o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *state_ptr--);
    *filtered_ptr++ = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12); // Q12 operations

    for (i = 4; i < x_length; i++)
    {
        o = (WebRtc_Word32)0;

        b_ptr = &b[0];
        x_ptr = &x[i];

        o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
        o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
        o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
        o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
        o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);

        *filtered_ptr++ = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12); // Q12 operations
    }

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
