/*
 * downsample.c
 *
 * This file contains the function WebRtcSpl_Downsample().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

int WebRtcSpl_Downsample(G_CONST WebRtc_Word16 *b, int b_length,
                         G_CONST WebRtc_Word16 *signal_in, int signal_length,
                         WebRtc_Word16 *state, int state_length,
                         WebRtc_Word16 *signal_downsampled, int max_length, int factor,
                         int delay)
{
    WebRtc_Word32 o;
    int i, j, stop;

    WebRtc_Word16 *signal_downsampled_ptr = signal_downsampled;
    G_CONST WebRtc_Word16 *b_ptr;
    G_CONST WebRtc_Word16 *signal_in_ptr;
    WebRtc_Word16 *state_ptr;
    WebRtc_Word16 inc = 1 << factor;

    // Unused input variable
    max_length = max_length;

    for (i = delay; i < signal_length; i += inc)
    {
        b_ptr = &b[0];
        signal_in_ptr = &signal_in[i];
        state_ptr = &state[state_length - 1];

        o = (WebRtc_Word32)0;
        stop = (i < b_length) ? i + 1 : b_length;

        for (j = 0; j < stop; j++)
        {
            o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *signal_in_ptr--);
        }
        for (j = i + 1; j < b_length; j++)
        {
            o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *state_ptr--);
        }

        // If output is higher than 32768, saturate it. Same with negative side
        // 2^27 = 134217728, which corresponds to 32768
        if (o < (WebRtc_Word32)-134217728)
            o = (WebRtc_Word32)-134217728;

        if (o > (WebRtc_Word32)(134217727 - 2048))
            o = (WebRtc_Word32)(134217727 - 2048);

        *signal_downsampled_ptr++ = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12); //Q12 ops
    }

    // Get the last delay part.
    for (i = ((signal_length >> factor) << factor) + inc; i < signal_length + delay; i += inc)
    {
        o = 0;
        if (i < signal_length)
        {
            b_ptr = &b[0];
            signal_in_ptr = &signal_in[i];
            for (j = 0; j < b_length; j++)
            {
                o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *signal_in_ptr--);
            }
        } else
        {
            b_ptr = &b[i - signal_length];
            signal_in_ptr = &signal_in[signal_length - 1];
            for (j = 0; j < b_length - (i - signal_length); j++)
            {
                o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *signal_in_ptr--);
            }
        }

        /* If output is higher than 32768, saturate it. Same with negative side
         2^27 = 134217728, which corresponds to 32768
         */
        if (o < (WebRtc_Word32)-134217728)
            o = (WebRtc_Word32)-134217728;

        if (o > (WebRtc_Word32)(134217727 - 2048))
            o = (WebRtc_Word32)(134217727 - 2048);

        *signal_downsampled_ptr++ = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12); //Q12 ops
    }

    return (signal_length >> factor);
}
