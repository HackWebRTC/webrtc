/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file contains the function WebRtcSpl_ComplexFFT().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#define CFFTSFT 14
#define CFFTRND 1
#define CFFTRND2 16384

#if (defined ARM9E_GCC) || (defined ARM_WINM) || (defined ANDROID_AECOPT)
extern "C" int FFT_4OFQ14(void *src, void *dest, int NC, int shift);

// For detailed description of the fft functions, check the readme files in fft_ARM9E folder.
int WebRtcSpl_ComplexFFT2(WebRtc_Word16 frfi[], WebRtc_Word16 frfiOut[], int stages, int mode)
{
    return FFT_4OFQ14(frfi, frfiOut, 1 << stages, 0);
}
#endif

int WebRtcSpl_ComplexFFT(WebRtc_Word16 frfi[], int stages, int mode)
{
    int i, j, l, k, istep, n, m;
    WebRtc_Word16 wr, wi;
    WebRtc_Word32 tr32, ti32, qr32, qi32;

    /* The 1024-value is a constant given from the size of WebRtcSpl_kSinTable1024[],
     * and should not be changed depending on the input parameter 'stages'
     */
    n = 1 << stages;
    if (n > 1024)
        return -1;

    l = 1;
    k = 10 - 1; /* Constant for given WebRtcSpl_kSinTable1024[]. Do not change
         depending on the input parameter 'stages' */

    if (mode == 0)
    {
        // mode==0: Low-complexity and Low-accuracy mode
        while (l < n)
        {
            istep = l << 1;

            for (m = 0; m < l; ++m)
            {
                j = m << k;

                /* The 256-value is a constant given as 1/4 of the size of
                 * WebRtcSpl_kSinTable1024[], and should not be changed depending on the input
                 * parameter 'stages'. It will result in 0 <= j < N_SINE_WAVE/2
                 */
                wr = WebRtcSpl_kSinTable1024[j + 256];
                wi = -WebRtcSpl_kSinTable1024[j];

                for (i = m; i < n; i += istep)
                {
                    j = i + l;

                    tr32 = WEBRTC_SPL_RSHIFT_W32((WEBRTC_SPL_MUL_16_16(wr, frfi[2 * j])
                            - WEBRTC_SPL_MUL_16_16(wi, frfi[2 * j + 1])), 15);

                    ti32 = WEBRTC_SPL_RSHIFT_W32((WEBRTC_SPL_MUL_16_16(wr, frfi[2 * j + 1])
                            + WEBRTC_SPL_MUL_16_16(wi, frfi[2 * j])), 15);

                    qr32 = (WebRtc_Word32)frfi[2 * i];
                    qi32 = (WebRtc_Word32)frfi[2 * i + 1];
                    frfi[2 * j] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(qr32 - tr32, 1);
                    frfi[2 * j + 1] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(qi32 - ti32, 1);
                    frfi[2 * i] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(qr32 + tr32, 1);
                    frfi[2 * i + 1] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(qi32 + ti32, 1);
                }
            }

            --k;
            l = istep;

        }

    } else
    {
        // mode==1: High-complexity and High-accuracy mode
        while (l < n)
        {
            istep = l << 1;

            for (m = 0; m < l; ++m)
            {
                j = m << k;

                /* The 256-value is a constant given as 1/4 of the size of
                 * WebRtcSpl_kSinTable1024[], and should not be changed depending on the input
                 * parameter 'stages'. It will result in 0 <= j < N_SINE_WAVE/2
                 */
                wr = WebRtcSpl_kSinTable1024[j + 256];
                wi = -WebRtcSpl_kSinTable1024[j];

                for (i = m; i < n; i += istep)
                {
                    j = i + l;

                    tr32 = WEBRTC_SPL_RSHIFT_W32((WEBRTC_SPL_MUL_16_16(wr, frfi[2 * j])
                            - WEBRTC_SPL_MUL_16_16(wi, frfi[2 * j + 1]) + CFFTRND),
                            15 - CFFTSFT);

                    ti32 = WEBRTC_SPL_RSHIFT_W32((WEBRTC_SPL_MUL_16_16(wr, frfi[2 * j + 1])
                            + WEBRTC_SPL_MUL_16_16(wi, frfi[2 * j]) + CFFTRND), 15 - CFFTSFT);

                    qr32 = ((WebRtc_Word32)frfi[2 * i]) << CFFTSFT;
                    qi32 = ((WebRtc_Word32)frfi[2 * i + 1]) << CFFTSFT;
                    frfi[2 * j] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(
                            (qr32 - tr32 + CFFTRND2), 1 + CFFTSFT);
                    frfi[2 * j + 1] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(
                            (qi32 - ti32 + CFFTRND2), 1 + CFFTSFT);
                    frfi[2 * i] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(
                            (qr32 + tr32 + CFFTRND2), 1 + CFFTSFT);
                    frfi[2 * i + 1] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(
                            (qi32 + ti32 + CFFTRND2), 1 + CFFTSFT);
                }
            }

            --k;
            l = istep;
        }
    }
    return 0;
}
