/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "scale_bilinear_yuv.h"
#include <string.h>

namespace webrtc
{
// 16.16 fixed point arithmetic
const WebRtc_UWord32 kFractionBits = 16;
const WebRtc_UWord32 kFractionMax = 1 << kFractionBits;
const WebRtc_UWord32 kFractionMask = ((1 << kFractionBits) - 1);

#if USE_MMX
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <mmintrin.h>
#endif
#endif

#if USE_SSE2
#include <emmintrin.h>
#endif

#if USE_SSE2
// FilterHorizontal combines two rows of the image using linear interpolation.
// SSE2 version does 16 pixels at a time

static void FilterHorizontal(WebRtc_UWord8* ybuf,
                             const WebRtc_UWord8* y0_ptr,
                             const WebRtc_UWord8* y1_ptr,
                             WebRtc_UWord32 source_width,
                             WebRtc_UWord32 source_y_fraction)
{
    __m128i zero = _mm_setzero_si128();
    __m128i y1_fraction = _mm_set1_epi16(source_y_fraction);
    __m128i y0_fraction = _mm_set1_epi16(256 - source_y_fraction);

    const __m128i* y0_ptr128 = reinterpret_cast<const __m128i*>(y0_ptr);
    const __m128i* y1_ptr128 = reinterpret_cast<const __m128i*>(y1_ptr);
    __m128i* dest128 = reinterpret_cast<__m128i*>(ybuf);
    __m128i* end128 = reinterpret_cast<__m128i*>(ybuf + source_width);

    do
    {
        __m128i y0 = _mm_loadu_si128(y0_ptr128);
        __m128i y1 = _mm_loadu_si128(y1_ptr128);
        __m128i y2 = _mm_unpackhi_epi8(y0, zero);
        __m128i y3 = _mm_unpackhi_epi8(y1, zero);
        y0 = _mm_unpacklo_epi8(y0, zero);
        y1 = _mm_unpacklo_epi8(y1, zero);
        y0 = _mm_mullo_epi16(y0, y0_fraction);
        y1 = _mm_mullo_epi16(y1, y1_fraction);
        y2 = _mm_mullo_epi16(y2, y0_fraction);
        y3 = _mm_mullo_epi16(y3, y1_fraction);
        y0 = _mm_add_epi16(y0, y1);
        y2 = _mm_add_epi16(y2, y3);
        y0 = _mm_srli_epi16(y0, 8);
        y2 = _mm_srli_epi16(y2, 8);
        y0 = _mm_packus_epi16(y0, y2);
        *dest128++ = y0;
        ++y0_ptr128;
        ++y1_ptr128;
    }
    while (dest128 < end128);
}
#elif USE_MMX
// MMX version does 8 pixels at a time
static void FilterHorizontal(WebRtc_UWord8* ybuf,
                             const WebRtc_UWord8* y0_ptr,
                             const WebRtc_UWord8* y1_ptr,
                             WebRtc_UWord32 source_width,
                             WebRtc_UWord32 source_y_fraction)
{
    __m64 zero = _mm_setzero_si64();
    __m64 y1_fraction = _mm_set1_pi16(source_y_fraction);
    __m64 y0_fraction = _mm_set1_pi16(256 - source_y_fraction);

    const __m64* y0_ptr64 = reinterpret_cast<const __m64*>(y0_ptr);
    const __m64* y1_ptr64 = reinterpret_cast<const __m64*>(y1_ptr);
    __m64* dest64 = reinterpret_cast<__m64*>(ybuf);
    __m64* end64 = reinterpret_cast<__m64*>(ybuf + source_width);

    do
    {
        __m64 y0 = *y0_ptr64++;
        __m64 y1 = *y1_ptr64++;
        __m64 y2 = _mm_unpackhi_pi8(y0, zero);
        __m64 y3 = _mm_unpackhi_pi8(y1, zero);
        y0 = _mm_unpacklo_pi8(y0, zero);
        y1 = _mm_unpacklo_pi8(y1, zero);
        y0 = _mm_mullo_pi16(y0, y0_fraction);
        y1 = _mm_mullo_pi16(y1, y1_fraction);
        y2 = _mm_mullo_pi16(y2, y0_fraction);
        y3 = _mm_mullo_pi16(y3, y1_fraction);
        y0 = _mm_add_pi16(y0, y1);
        y2 = _mm_add_pi16(y2, y3);
        y0 = _mm_srli_pi16(y0, 8);
        y2 = _mm_srli_pi16(y2, 8);
        y0 = _mm_packs_pu16(y0, y2);
        *dest64++ = y0;
    }
    while (dest64 < end64);
}
#else  // no MMX or SSE2
// C version does 8 at a time to mimic MMX code
static void FilterHorizontal(WebRtc_UWord8* ybuf,
                             const WebRtc_UWord8* y0_ptr,
                             const WebRtc_UWord8* y1_ptr,
                             WebRtc_UWord32 source_width,
                             WebRtc_UWord32 source_y_fraction)
{
    WebRtc_UWord32 y1_fraction = source_y_fraction;
    WebRtc_UWord32 y0_fraction = 256 - y1_fraction;
    WebRtc_UWord8* end = ybuf + source_width;
    do
    {
        ybuf[0] = (y0_ptr[0] * y0_fraction + y1_ptr[0] * y1_fraction) >> 8;
        ybuf[1] = (y0_ptr[1] * y0_fraction + y1_ptr[1] * y1_fraction) >> 8;
        ybuf[2] = (y0_ptr[2] * y0_fraction + y1_ptr[2] * y1_fraction) >> 8;
        ybuf[3] = (y0_ptr[3] * y0_fraction + y1_ptr[3] * y1_fraction) >> 8;
        ybuf[4] = (y0_ptr[4] * y0_fraction + y1_ptr[4] * y1_fraction) >> 8;
        ybuf[5] = (y0_ptr[5] * y0_fraction + y1_ptr[5] * y1_fraction) >> 8;
        ybuf[6] = (y0_ptr[6] * y0_fraction + y1_ptr[6] * y1_fraction) >> 8;
        ybuf[7] = (y0_ptr[7] * y0_fraction + y1_ptr[7] * y1_fraction) >> 8;
        y0_ptr += 8;
        y1_ptr += 8;
        ybuf += 8;
    }
    while (ybuf < end);
}
#endif

static void FilterVertical(WebRtc_UWord8* ybuf,
                           const WebRtc_UWord8* y0_ptr,
                           WebRtc_UWord32 width,
                           WebRtc_UWord32 source_dx)
{
    WebRtc_UWord32 x = 0;

    for (WebRtc_UWord32 i = 0; i < width; i ++)
    {
        WebRtc_UWord32 y0 = y0_ptr[x >> 16];
        WebRtc_UWord32 y1 = y0_ptr[(x >> 16) + 1];

        WebRtc_UWord32 y_frac = (x & 65535);
        ybuf[i] = (y_frac * y1 + (y_frac ^ 65535) * y0) >> 16;

        x += source_dx;
    }
}

WebRtc_Word32
ScaleBilinear(const WebRtc_UWord8* srcFrame, WebRtc_UWord8*& dstFrame,
              WebRtc_UWord32 srcWidth, WebRtc_UWord32 srcHeight,
              WebRtc_UWord32 dstWidth, WebRtc_UWord32 dstHeight,
              WebRtc_UWord32& dstSize)
{
    // Setting source
    const WebRtc_UWord8* src = srcFrame;
    WebRtc_UWord8* srcTmp = NULL;

    const WebRtc_UWord32 srcStride = (srcWidth  + 15) & ~15;
    const WebRtc_UWord32 srcUvStride = (((srcStride + 1 >> 1) + 15) & ~15);

    const WebRtc_UWord32 srcStrideArray[3] = {srcStride,
                                              srcUvStride,
                                              srcUvStride
                                             };
    const WebRtc_UWord32 srcWidthArray[3] = {srcWidth,
                                            (srcWidth + 1) >> 1,
                                            (srcWidth + 1) >> 1
                                            };

    // if srcFrame isn't aligned to nice boundaries then copy it over
    // in another buffer
    if ((srcStride > srcWidth) || (srcUvStride > ((srcWidth + 1) >> 1)))
    {
        // allocate buffer that can accommodate the stride
        srcTmp = new WebRtc_UWord8[srcStride * srcHeight * 3 >> 1];
        WebRtc_UWord8* tmpPlaneArray[3];
        tmpPlaneArray[0] = srcTmp;
        tmpPlaneArray[1] = tmpPlaneArray[0] + srcStride * srcHeight;
        tmpPlaneArray[2] = tmpPlaneArray[1] +
                           (srcStride >> 1) * (srcHeight >> 1);

        WebRtc_UWord8* tmpPtr = srcTmp;
        const WebRtc_UWord8* srcPtr = srcFrame;

        for (WebRtc_UWord32 p = 0; p < 3; p++)
        {
            WebRtc_UWord8* dstPtr = tmpPlaneArray[p];
            const WebRtc_UWord32 h = (p == 0) ? srcHeight : srcHeight >> 1;

            for (WebRtc_UWord32 i = 0; i < h; i++)
            {
                memcpy(dstPtr, srcPtr, srcWidthArray[p]);
                dstPtr += srcStrideArray[p];
                srcPtr += srcWidthArray[p];
            }
        }
        src = srcTmp;
    }

    const WebRtc_UWord8* srcPlaneArray[3];
    srcPlaneArray[0] = src;
    srcPlaneArray[1] = srcPlaneArray[0] + srcStride * srcHeight;
    srcPlaneArray[2] = srcPlaneArray[1] + (srcStride >> 1) * (srcHeight >> 1);

    // Setting destination
    const WebRtc_UWord32 dstStride = (dstWidth + 31) & ~31;
    const WebRtc_UWord32 dstUvStride = dstStride >> 1;

    WebRtc_UWord32 dstRequiredSize = dstStride * dstHeight +
                                     2 * (dstUvStride * ((dstHeight + 1) >> 1));
    WebRtc_UWord32 dstFinalRequiredSize = dstWidth * dstHeight * 3 >> 1;


    if (dstFrame && dstFinalRequiredSize > dstSize)
    {
        // allocated buffer is too small
        delete [] dstFrame;
        dstFrame = NULL;
    }
    if (dstFrame == NULL)
    {
        dstFrame = new WebRtc_UWord8[dstFinalRequiredSize];
        dstSize = dstFinalRequiredSize;
    }
    WebRtc_UWord8* dstPtr = dstFrame;
    WebRtc_UWord8* tmpDst = NULL;

    if (dstFinalRequiredSize < dstRequiredSize)
    {
        // allocate a tmp buffer for destination
        tmpDst = new WebRtc_UWord8[dstRequiredSize];
        dstPtr = tmpDst;
    }

    WebRtc_UWord8* dstPlaneArray[3] = {dstPtr,
                                       dstPlaneArray[0] + dstStride * dstHeight,
                                       dstPlaneArray[1] +
                                       (dstUvStride * ((dstHeight + 1) >> 1))
                                      };

    const WebRtc_UWord32 dstStrideArray[3] = {dstStride,
                                              dstUvStride,
                                              dstUvStride
                                             };
    const WebRtc_UWord32 dstWidthArray[3] = {dstWidth,
                                             dstWidth>>1,
                                             dstWidth>>1
                                            };

    for (WebRtc_UWord32 p = 0; p < 3; p++)
    {
        const WebRtc_UWord32 sh = (p == 0) ? srcHeight : srcHeight >> 1;
        const WebRtc_UWord32 dh = (p == 0) ? dstHeight : dstHeight >> 1;
        WebRtc_UWord8* filteredBuf = dstPlaneArray[p];
        WebRtc_UWord8* horizontalFilteredBuf;
        WebRtc_UWord8* intermediaryBuf = new WebRtc_UWord8[srcStrideArray[p]];

        const WebRtc_UWord32 hscale_fixed = (sh << kFractionBits) / dh;
        const WebRtc_UWord32 source_dx = srcWidthArray[p] * kFractionMax /
                                         dstWidthArray[p];


        for (WebRtc_UWord32 h = 0; h < dh; ++h)
        {
            horizontalFilteredBuf = filteredBuf;

            // If vertical filtering must be done then put the results
            // from the horizontal filtering in an intermediary buffer
            if (source_dx != kFractionMax)
            {
                horizontalFilteredBuf = intermediaryBuf;
            }

            // horizontal filter
            WebRtc_UWord32 source_h_subpixel = (h * hscale_fixed);
            if (hscale_fixed >= (kFractionMax * 2))
            {
                // For 1/2 or less, center filter.
                source_h_subpixel += kFractionMax / 2;
            }

            // Choose the two lines that are going to be bilinear filtered.
            WebRtc_UWord32 source_h = source_h_subpixel >> kFractionBits;
            const WebRtc_UWord8* ptr_0 = srcPlaneArray[p] +
                                         source_h * srcStrideArray[p];
            const WebRtc_UWord8* ptr_1 = ptr_0 + srcStrideArray[p];

            // scaler uses 16.8 fixed point
            WebRtc_UWord32 source_h_fraction =
                (source_h_subpixel & kFractionMask) >> 8;

            if (hscale_fixed != kFractionMax &&
                    source_h_fraction && ((source_h + 1) < sh))
            {
                FilterHorizontal(horizontalFilteredBuf, ptr_0, ptr_1,
                                 srcWidthArray[p], source_h_fraction);
            }
            else
            {
                memcpy(horizontalFilteredBuf, ptr_1, srcWidthArray[p]);
            }

            // vertical filter only if necessary
            if (source_dx != kFractionMax)
            {
                FilterVertical(filteredBuf, horizontalFilteredBuf,
                               dstWidthArray[p], source_dx);
            }

            filteredBuf += dstStrideArray[p];
        }

        if (intermediaryBuf != NULL)
        {
            delete [] intermediaryBuf;
        }
    }

    if (srcTmp != NULL)
    {
        delete [] srcTmp;
    }
    // Filtered image was placed in an aligned buffer.  If the
    // final output is not in an aligned buffer copy the image over.
    if (dstStride > dstWidth)
    {
        WebRtc_UWord8* dstFramePtr = dstFrame;

        for (WebRtc_UWord32 p = 0; p < 3; p++)
        {
            WebRtc_UWord8* dstPlanePtr = dstPlaneArray[p];
            const WebRtc_UWord32 h = (p == 0) ? dstHeight : dstHeight >> 1;

            for (WebRtc_UWord32 i = 0; i < h; i++)
            {
                memcpy(dstFramePtr, dstPlanePtr, dstWidthArray[p]);
                dstFramePtr += dstWidthArray[p];
                dstPlanePtr += dstStrideArray[p];
            }
        }
    }

    if (tmpDst != NULL)
    {
        delete [] tmpDst;
    }

    return dstHeight;
}

}  // namespace webrtc
