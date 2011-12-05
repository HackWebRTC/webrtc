/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_metrics.h"

#include <algorithm> // min_element, max_element
#include <cmath>
#include <fstream>

// Calculates PSNR from MSE
static inline double CalcPsnr(double mse) {
  // Formula: PSNR = 10 log (255^2 / MSE) = 20 log(255) - 10 log(MSE)
  return 20.0 * std::log10(255.0) - 10.0 * std::log10(mse);
}

// Used for calculating min and max values
static bool LessForFrameResultValue (const FrameResult& s1,
                                     const FrameResult& s2) {
    return s1.value < s2.value;
}

int
PsnrFromFiles(const WebRtc_Word8 *refFileName, const WebRtc_Word8 *testFileName,
              WebRtc_Word32 width, WebRtc_Word32 height, QualityMetricsResult *result)
{
    FILE *refFp = fopen(refFileName, "rb");
    if (refFp == NULL )
    {
        // cannot open reference file
        fprintf(stderr, "Cannot open file %s\n", refFileName);
        return -1;
    }

    FILE *testFp = fopen(testFileName, "rb");
    if (testFp == NULL )
    {
        // cannot open test file
        fprintf(stderr, "Cannot open file %s\n", testFileName);
        return -2;
    }

    double mse = 0.0;
    double mseSum = 0.0;
    WebRtc_Word32 frames = 0;

    // Allocating size for one I420 frame.
    WebRtc_Word32 frameBytes = 3 * width * height >> 1;
    WebRtc_UWord8 *ref = new WebRtc_UWord8[frameBytes];
    WebRtc_UWord8 *test = new WebRtc_UWord8[frameBytes];

    WebRtc_Word32 refBytes = (WebRtc_Word32) fread(ref, 1, frameBytes, refFp);
    WebRtc_Word32 testBytes = (WebRtc_Word32) fread(test, 1, frameBytes, testFp);

    while (refBytes == frameBytes && testBytes == frameBytes)
    {
        mse = 0.0;

        WebRtc_Word32 sh = 8; //boundary offset
        for (WebRtc_Word32 k2 = sh; k2 < height - sh; k2++)
        for (WebRtc_Word32 k = sh; k < width - sh; k++)
        {
            WebRtc_Word32 kk = k2 * width + k;
            mse += (test[kk] - ref[kk]) * (test[kk] - ref[kk]);
        }

        // divide by number of pixels
        mse /= (double) (width * height);

        // Save statistics for this specific frame
        FrameResult frame_result;
        frame_result.value = CalcPsnr(mse);
        frame_result.frame_number = frames;
        result->frames.push_back(frame_result);

        // accumulate for total average
        mseSum += mse;
        frames++;

        refBytes = (WebRtc_Word32) fread(ref, 1, frameBytes, refFp);
        testBytes = (WebRtc_Word32) fread(test, 1, frameBytes, testFp);
    }

    if (mseSum == 0)
    {
        // The PSNR value is undefined in this case.
        // This value effectively means that the files are equal.
        result->average = std::numeric_limits<double>::max();
    }
    else
    {
        result->average = CalcPsnr(mseSum / frames);
    }

    if (result->frames.size() == 0)
    {
      fprintf(stderr, "Tried to measure SSIM from empty files (reference "
              "file: %s  test file: %s\n", refFileName, testFileName);
      return -3;
    }
    else
    {
        // Calculate min/max statistics
        std::vector<FrameResult>::iterator element;
        element = min_element(result->frames.begin(),
                              result->frames.end(), LessForFrameResultValue);
        result->min = element->value;
        result->min_frame_number = element->frame_number;
        element = max_element(result->frames.begin(),
                              result->frames.end(), LessForFrameResultValue);
        result->max = element->value;
        result->max_frame_number = element->frame_number;
    }
    delete [] ref;
    delete [] test;

    fclose(refFp);
    fclose(testFp);

    return 0;
}

static double
Similarity(WebRtc_UWord64 sum_s, WebRtc_UWord64 sum_r, WebRtc_UWord64 sum_sq_s,
           WebRtc_UWord64 sum_sq_r, WebRtc_UWord64 sum_sxr, WebRtc_Word32 count)
{
    WebRtc_Word64 ssim_n, ssim_d;
    WebRtc_Word64 c1, c2;
    const WebRtc_Word64 cc1 = 26634; // (64^2*(.01*255)^2
    const WebRtc_Word64 cc2 = 239708; // (64^2*(.03*255)^2

    // Scale the constants by number of pixels
    c1 = (cc1 * count * count) >> 12;
    c2 = (cc2 * count * count) >> 12;

    ssim_n = (2 * sum_s * sum_r + c1) * ((WebRtc_Word64) 2 * count * sum_sxr-
          (WebRtc_Word64) 2 * sum_s * sum_r + c2);

    ssim_d = (sum_s * sum_s + sum_r * sum_r + c1)*
        ((WebRtc_Word64)count * sum_sq_s - (WebRtc_Word64)sum_s * sum_s +
        (WebRtc_Word64)count * sum_sq_r - (WebRtc_Word64) sum_r * sum_r + c2);

    return ssim_n * 1.0 / ssim_d;
}

#if !defined(WEBRTC_USE_SSE2)
static double
Ssim8x8C(WebRtc_UWord8 *s, WebRtc_Word32 sp,
         WebRtc_UWord8 *r, WebRtc_Word32 rp)
{
    WebRtc_UWord64 sum_s    = 0;
    WebRtc_UWord64 sum_r    = 0;
    WebRtc_UWord64 sum_sq_s = 0;
    WebRtc_UWord64 sum_sq_r = 0;
    WebRtc_UWord64 sum_sxr  = 0;

    WebRtc_Word32 i, j;
    for (i = 0; i < 8; i++, s += sp,r += rp)
    {
        for (j = 0; j < 8; j++)
        {
            sum_s += s[j];
            sum_r += r[j];
            sum_sq_s += s[j] * s[j];
            sum_sq_r += r[j] * r[j];
            sum_sxr += s[j] * r[j];
        }
    }
    return Similarity(sum_s, sum_r, sum_sq_s, sum_sq_r, sum_sxr, 64);
}
#endif

#if defined(WEBRTC_USE_SSE2)
#include <emmintrin.h>
#include <xmmintrin.h>
static double
Ssim8x8Sse2(WebRtc_UWord8 *s, WebRtc_Word32 sp,
            WebRtc_UWord8 *r, WebRtc_Word32 rp)
{
    WebRtc_Word32 i;
    const __m128i z     = _mm_setzero_si128();
    __m128i sum_s_16    = _mm_setzero_si128();
    __m128i sum_r_16    = _mm_setzero_si128();
    __m128i sum_sq_s_32 = _mm_setzero_si128();
    __m128i sum_sq_r_32 = _mm_setzero_si128();
    __m128i sum_sxr_32  = _mm_setzero_si128();

    for (i = 0; i < 8; i++, s += sp,r += rp)
    {
        const __m128i s_8 = _mm_loadl_epi64((__m128i*)(s));
        const __m128i r_8 = _mm_loadl_epi64((__m128i*)(r));

        const __m128i s_16 = _mm_unpacklo_epi8(s_8,z);
        const __m128i r_16 = _mm_unpacklo_epi8(r_8,z);

        sum_s_16 = _mm_adds_epu16(sum_s_16, s_16);
        sum_r_16 = _mm_adds_epu16(sum_r_16, r_16);
        const __m128i sq_s_32 = _mm_madd_epi16(s_16, s_16);
        sum_sq_s_32 = _mm_add_epi32(sum_sq_s_32, sq_s_32);
        const __m128i sq_r_32 = _mm_madd_epi16(r_16, r_16);
        sum_sq_r_32 = _mm_add_epi32(sum_sq_r_32, sq_r_32);
        const __m128i sxr_32 = _mm_madd_epi16(s_16, r_16);
        sum_sxr_32 = _mm_add_epi32(sum_sxr_32, sxr_32);
    }

    const __m128i sum_s_32  = _mm_add_epi32(_mm_unpackhi_epi16(sum_s_16, z),
                                            _mm_unpacklo_epi16(sum_s_16, z));
    const __m128i sum_r_32  = _mm_add_epi32(_mm_unpackhi_epi16(sum_r_16, z),
                                            _mm_unpacklo_epi16(sum_r_16, z));

    __m128i sum_s_128;
    __m128i sum_r_128;
    __m128i sum_sq_s_128;
    __m128i sum_sq_r_128;
    __m128i sum_sxr_128;

    _mm_store_si128 (&sum_s_128,
                      _mm_add_epi64(_mm_unpackhi_epi32(sum_s_32, z),
                                    _mm_unpacklo_epi32(sum_s_32, z)));
    _mm_store_si128 (&sum_r_128,
                      _mm_add_epi64(_mm_unpackhi_epi32(sum_r_32, z),
                                    _mm_unpacklo_epi32(sum_r_32, z)));
    _mm_store_si128 (&sum_sq_s_128,
                      _mm_add_epi64(_mm_unpackhi_epi32(sum_sq_s_32, z),
                                    _mm_unpacklo_epi32(sum_sq_s_32, z)));
    _mm_store_si128 (&sum_sq_r_128,
                      _mm_add_epi64(_mm_unpackhi_epi32(sum_sq_r_32, z),
                                    _mm_unpacklo_epi32(sum_sq_r_32, z)));
    _mm_store_si128 (&sum_sxr_128,
                      _mm_add_epi64(_mm_unpackhi_epi32(sum_sxr_32, z),
                                    _mm_unpacklo_epi32(sum_sxr_32, z)));

    const WebRtc_UWord64 *sum_s_64 =
                          reinterpret_cast<WebRtc_UWord64*>(&sum_s_128);
    const WebRtc_UWord64 *sum_r_64 =
                          reinterpret_cast<WebRtc_UWord64*>(&sum_r_128);
    const WebRtc_UWord64 *sum_sq_s_64 =
                          reinterpret_cast<WebRtc_UWord64*>(&sum_sq_s_128);
    const WebRtc_UWord64 *sum_sq_r_64 =
                          reinterpret_cast<WebRtc_UWord64*>(&sum_sq_r_128);
    const WebRtc_UWord64 *sum_sxr_64 =
                          reinterpret_cast<WebRtc_UWord64*>(&sum_sxr_128);

    const WebRtc_UWord64 sum_s    = sum_s_64[0]    + sum_s_64[1];
    const WebRtc_UWord64 sum_r    = sum_r_64[0]    + sum_r_64[1];
    const WebRtc_UWord64 sum_sq_s = sum_sq_s_64[0] + sum_sq_s_64[1];
    const WebRtc_UWord64 sum_sq_r = sum_sq_r_64[0] + sum_sq_r_64[1];
    const WebRtc_UWord64 sum_sxr  = sum_sxr_64[0]  + sum_sxr_64[1];

    return Similarity(sum_s, sum_r, sum_sq_s, sum_sq_r, sum_sxr, 64);
}
#endif

double
SsimFrame(WebRtc_UWord8 *img1, WebRtc_UWord8 *img2, WebRtc_Word32 stride_img1,
          WebRtc_Word32 stride_img2, WebRtc_Word32 width, WebRtc_Word32 height)
{
    WebRtc_Word32 i,j;
    WebRtc_UWord32 samples = 0;
    double ssim_total = 0;
    double (*ssim_8x8)(WebRtc_UWord8*, WebRtc_Word32,
                       WebRtc_UWord8*, WebRtc_Word32 rp);

#if defined(WEBRTC_USE_SSE2)
    ssim_8x8 = Ssim8x8Sse2;
#else
    ssim_8x8 = Ssim8x8C;
#endif

    // Sample point start with each 4x4 location
    for (i = 0; i < height - 8; i += 4, img1 += stride_img1 * 4,
         img2 += stride_img2 * 4)
    {
        for (j = 0; j < width - 8; j += 4 )
        {
            double v = ssim_8x8(img1 + j, stride_img1, img2 + j, stride_img2);
            ssim_total += v;
            samples++;
        }
    }
    ssim_total /= samples;
    return ssim_total;
}

int
SsimFromFiles(const WebRtc_Word8 *refFileName, const WebRtc_Word8 *testFileName,
              WebRtc_Word32 width, WebRtc_Word32 height, QualityMetricsResult *result)
{
    FILE *refFp = fopen(refFileName, "rb");
    if (refFp == NULL)
    {
        // cannot open reference file
        fprintf(stderr, "Cannot open file %s\n", refFileName);
        return -1;
    }

    FILE *testFp = fopen(testFileName, "rb");
    if (testFp == NULL)
    {
        // cannot open test file
        fprintf(stderr, "Cannot open file %s\n", testFileName);
        return -2;
    }

    WebRtc_Word32 frames = 0;

    // Bytes in one frame I420
    const WebRtc_Word32 frameBytes = 3 * width  * height / 2;
    WebRtc_UWord8 *ref = new WebRtc_UWord8[frameBytes];
    WebRtc_UWord8 *test = new WebRtc_UWord8[frameBytes];

    WebRtc_Word32 refBytes = (WebRtc_Word32) fread(ref, 1, frameBytes, refFp);
    WebRtc_Word32 testBytes = (WebRtc_Word32) fread(test, 1, frameBytes, testFp);

    double ssimScene = 0.0; //average SSIM for sequence

    while (refBytes == frameBytes && testBytes == frameBytes )
    {
        double ssimFrameValue = SsimFrame(ref, test, width, width, width, height);
        // Save statistics for this specific frame
        FrameResult frame_result;
        frame_result.value = ssimFrameValue;
        frame_result.frame_number = frames;
        result->frames.push_back(frame_result);

        ssimScene += ssimFrameValue;
        frames++;

        refBytes = (WebRtc_Word32) fread(ref, 1, frameBytes, refFp);
        testBytes = (WebRtc_Word32) fread(test, 1, frameBytes, testFp);
    }

    if (result->frames.size() == 0)
    {
      fprintf(stderr, "Tried to measure SSIM from empty files (reference "
              "file: %s  test file: %s\n", refFileName, testFileName);
      return -3;
    }
    else
    {
        // SSIM: normalize/average for sequence
        ssimScene  = ssimScene / frames;
        result->average = ssimScene;

        // Calculate min/max statistics
        std::vector<FrameResult>::iterator element;
        element = min_element(result->frames.begin(),
                              result->frames.end(), LessForFrameResultValue);
        result->min = element->value;
        result->min_frame_number = element->frame_number;
        element = max_element(result->frames.begin(),
                              result->frames.end(), LessForFrameResultValue);
        result->max = element->value;
        result->max_frame_number = element->frame_number;
    }
    delete [] ref;
    delete [] test;

    fclose(refFp);
    fclose(testFp);

    return 0;
}
