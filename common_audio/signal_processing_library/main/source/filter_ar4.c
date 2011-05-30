/*
 * filter_ar4.c
 *
 * This file contains the function WebRtcSpl_FilterAR4().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

int WebRtcSpl_FilterAR4(G_CONST WebRtc_Word16 *a, int a_length, G_CONST WebRtc_Word16 *x,
                        int x_length, WebRtc_Word16 *state, int state_length,
                        WebRtc_Word16 *state_low, int state_low_length,
                        WebRtc_Word16 *filtered, int max_length, WebRtc_Word16 *filtered_low,
                        int filtered_low_length)
{
    WebRtc_Word32 o;
    WebRtc_Word32 oLOW;
    int i;
    int j;
    int stop;
    G_CONST WebRtc_Word16 *a_ptr;
    WebRtc_Word16 *filtered_ptr;
    WebRtc_Word16 *filtered_low_ptr;
    WebRtc_Word16 *state_ptr;
    WebRtc_Word16 *state_low_ptr;
    G_CONST WebRtc_Word16 *x_ptr = &x[0];
    WebRtc_Word16 *filteredFINAL_ptr = filtered;
    WebRtc_Word16 *filteredFINAL_LOW_ptr = filtered_low;

#ifdef _DEBUG
    if (max_length < x_length)
    {
        printf(" FilterAR4 : out vector is shorter than in vector\n");
        exit(0);
    }
    if (state_length != a_length - 1)
    {
        printf(" FilterAR4 : state vector does not have the correct length\n");
        exit(0);
    }
#endif

    /* Unused input variable */
    max_length = max_length;
    state_low_length = state_low_length;
    filtered_low_length = filtered_low_length;

    for (i = 0; i < 4; i++)
    {
        a_ptr = &a[1];
        filtered_ptr = &filtered[i - 1];
        filtered_low_ptr = &filtered_low[i - 1];
        state_ptr = &state[state_length - 1];
        state_low_ptr = &state_low[state_length - 1];

        o = (WebRtc_Word32)(*x_ptr++) << 12; // Q12 operations
        oLOW = (WebRtc_Word32)0;

        stop = (i < a_length) ? i + 1 : a_length;
        for (j = 1; j < stop; j++)
        {
            o -= WEBRTC_SPL_MUL_16_16(*a_ptr,*filtered_ptr--);
            oLOW -= WEBRTC_SPL_MUL_16_16(*a_ptr++,*filtered_low_ptr--);
        }
        for (j = i + 1; j < a_length; j++)
        {
            o -= WEBRTC_SPL_MUL_16_16(*a_ptr,*state_ptr--);
            oLOW -= WEBRTC_SPL_MUL_16_16(*a_ptr++,*state_low_ptr--);
        }

        o += (oLOW >> 12); // Q12 operations
        *filteredFINAL_ptr = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12);// Q12 operations
        *filteredFINAL_LOW_ptr++ = (WebRtc_Word16)(o - ((WebRtc_Word32)(*filteredFINAL_ptr++)
                << 12));
    }

    for (i = 4; i < x_length; i++)
    {
        /* Calculate filtered[0] */
        a_ptr = &a[1];
        filtered_ptr = &filtered[i - 1];
        filtered_low_ptr = &filtered_low[i - 1];

        o = (WebRtc_Word32)(*x_ptr++) << 12; // Q12 operations
        oLOW = 0;

        o -= WEBRTC_SPL_MUL_16_16(*a_ptr, *filtered_ptr--);
        oLOW -= WEBRTC_SPL_MUL_16_16(*a_ptr++, *filtered_low_ptr--);

        o -= WEBRTC_SPL_MUL_16_16(*a_ptr, *filtered_ptr--);
        oLOW -= WEBRTC_SPL_MUL_16_16(*a_ptr++, *filtered_low_ptr--);

        o -= WEBRTC_SPL_MUL_16_16(*a_ptr, *filtered_ptr--);
        oLOW -= WEBRTC_SPL_MUL_16_16(*a_ptr++, *filtered_low_ptr--);

        o -= WEBRTC_SPL_MUL_16_16(*a_ptr, *filtered_ptr--);
        oLOW -= WEBRTC_SPL_MUL_16_16(*a_ptr++, *filtered_low_ptr--);

        o += (oLOW >> 12); // Q12 operations
        *filteredFINAL_ptr = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12);// Q12 operations
        *filteredFINAL_LOW_ptr++ = (WebRtc_Word16)(o - ((WebRtc_Word32)(*filteredFINAL_ptr++)
                << 12));
    }

    if (x_length >= state_length)
    {
        WebRtcSpl_CopyFromEndW16(filtered, x_length, a_length - 1, state, state_length);
        WebRtcSpl_CopyFromEndW16(filtered_low, x_length, a_length - 1, state_low, state_length);
    } else
    {
        for (i = 0; i < state_length - x_length; i++)
        {
            state[i] = state[i + x_length];
            state_low[i] = state_low[i + x_length];
        }
        for (i = 0; i < x_length; i++)
        {
            state[state_length - x_length + i] = filtered[i];
            state[state_length - x_length + i] = filtered_low[i];
        }
    }

    return x_length;
}
