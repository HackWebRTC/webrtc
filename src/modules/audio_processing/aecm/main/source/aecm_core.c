/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>

#include "aecm_core.h"
#include "ring_buffer.h"
#include "echo_control_mobile.h"
#include "typedefs.h"

// TODO(bjornv): Will be removed in final version.
//#include <stdio.h>

#ifdef ARM_WINM_LOG
#include <stdio.h>
#include <windows.h>
#endif

// BANDLAST - BANDFIRST must be < 32
#define BANDFIRST                   12   // Only bit BANDFIRST through bit BANDLAST are processed
#define BANDLAST                    43

#ifdef ARM_WINM
#define WebRtcSpl_AddSatW32(a,b)  _AddSatInt(a,b)
#define WebRtcSpl_SubSatW32(a,b)  _SubSatInt(a,b)
#endif
// 16 instructions on most risc machines for 32-bit bitcount !

#ifdef AEC_DEBUG
FILE *dfile;
FILE *testfile;
#endif

#ifdef AECM_SHORT

// Square root of Hanning window in Q14
static const WebRtc_Word16 kSqrtHanning[] =
{
    0, 804, 1606, 2404, 3196, 3981, 4756, 5520,
    6270, 7005, 7723, 8423, 9102, 9760, 10394, 11003,
    11585, 12140, 12665, 13160, 13623, 14053, 14449, 14811,
    15137, 15426, 15679, 15893, 16069, 16207, 16305, 16364,
    16384
};

#else

// Square root of Hanning window in Q14
static const WebRtc_Word16 kSqrtHanning[] = {0, 399, 798, 1196, 1594, 1990, 2386, 2780, 3172,
        3562, 3951, 4337, 4720, 5101, 5478, 5853, 6224, 6591, 6954, 7313, 7668, 8019, 8364,
        8705, 9040, 9370, 9695, 10013, 10326, 10633, 10933, 11227, 11514, 11795, 12068, 12335,
        12594, 12845, 13089, 13325, 13553, 13773, 13985, 14189, 14384, 14571, 14749, 14918,
        15079, 15231, 15373, 15506, 15631, 15746, 15851, 15947, 16034, 16111, 16179, 16237,
        16286, 16325, 16354, 16373, 16384};

#endif

//Q15 alpha = 0.99439986968132  const Factor for magnitude approximation
static const WebRtc_UWord16 kAlpha1 = 32584;
//Q15 beta = 0.12967166976970   const Factor for magnitude approximation
static const WebRtc_UWord16 kBeta1 = 4249;
//Q15 alpha = 0.94234827210087  const Factor for magnitude approximation
static const WebRtc_UWord16 kAlpha2 = 30879;
//Q15 beta = 0.33787806009150   const Factor for magnitude approximation
static const WebRtc_UWord16 kBeta2 = 11072;
//Q15 alpha = 0.82247698684306  const Factor for magnitude approximation
static const WebRtc_UWord16 kAlpha3 = 26951;
//Q15 beta = 0.57762063060713   const Factor for magnitude approximation
static const WebRtc_UWord16 kBeta3 = 18927;

// Initialization table for echo channel in 8 kHz
static const WebRtc_Word16 kChannelStored8kHz[PART_LEN1] = {
    2040,   1815,   1590,   1498,   1405,   1395,   1385,   1418,
    1451,   1506,   1562,   1644,   1726,   1804,   1882,   1918,
    1953,   1982,   2010,   2025,   2040,   2034,   2027,   2021,
    2014,   1997,   1980,   1925,   1869,   1800,   1732,   1683,
    1635,   1604,   1572,   1545,   1517,   1481,   1444,   1405,
    1367,   1331,   1294,   1270,   1245,   1239,   1233,   1247,
    1260,   1282,   1303,   1338,   1373,   1407,   1441,   1470,
    1499,   1524,   1549,   1565,   1582,   1601,   1621,   1649,
    1676
};

// Initialization table for echo channel in 16 kHz
static const WebRtc_Word16 kChannelStored16kHz[PART_LEN1] = {
    2040,   1590,   1405,   1385,   1451,   1562,   1726,   1882,
    1953,   2010,   2040,   2027,   2014,   1980,   1869,   1732,
    1635,   1572,   1517,   1444,   1367,   1294,   1245,   1233,
    1260,   1303,   1373,   1441,   1499,   1549,   1582,   1621,
    1676,   1741,   1802,   1861,   1921,   1983,   2040,   2102,
    2170,   2265,   2375,   2515,   2651,   2781,   2922,   3075,
    3253,   3471,   3738,   3976,   4151,   4258,   4308,   4288,
    4270,   4253,   4237,   4179,   4086,   3947,   3757,   3484,
    3153
};

#ifdef ARM_WINM_LOG
HANDLE logFile = NULL;
#endif

static void WebRtcAecm_ComfortNoise(AecmCore_t* const aecm, const WebRtc_UWord16 * const dfa,
                                    WebRtc_Word16 * const outReal,
                                    WebRtc_Word16 * const outImag,
                                    const WebRtc_Word16 * const lambda);

static __inline WebRtc_UWord32 WebRtcAecm_SetBit(WebRtc_UWord32 in, WebRtc_Word32 pos)
{
    WebRtc_UWord32 mask, out;

    mask = WEBRTC_SPL_SHIFT_W32(1, pos);
    out = (in | mask);

    return out;
}

// WebRtcAecm_Hisser(...)
//
// This function compares the binary vector specvec with all rows of the binary matrix specmat
// and counts per row the number of times they have the same value.
// Input:
//       - specvec   : binary "vector"  that is stored in a long
//       - specmat   : binary "matrix"  that is stored as a vector of long
// Output:
//       - bcount    : "Vector" stored as a long, containing for each row the number of times
//                      the matrix row and the input vector have the same value
//
//
void WebRtcAecm_Hisser(const WebRtc_UWord32 specvec, const WebRtc_UWord32 * const specmat,
                       WebRtc_UWord32 * const bcount)
{
    int n;
    WebRtc_UWord32 a, b;
    register WebRtc_UWord32 tmp;

    a = specvec;
    // compare binary vector specvec with all rows of the binary matrix specmat
    for (n = 0; n < MAX_DELAY; n++)
    {
        b = specmat[n];
        a = (specvec ^ b);
        // Returns bit counts in tmp
        tmp = a - ((a >> 1) & 033333333333) - ((a >> 2) & 011111111111);
        tmp = ((tmp + (tmp >> 3)) & 030707070707);
        tmp = (tmp + (tmp >> 6));
        tmp = (tmp + (tmp >> 12) + (tmp >> 24)) & 077;

        bcount[n] = tmp;
    }
}

// WebRtcAecm_BSpectrum(...)
//
// Computes the binary spectrum by comparing the input spectrum with a threshold spectrum.
//
// Input:
//       - spectrum  : Spectrum of which the binary spectrum should be calculated.
//       - thresvec  : Threshold spectrum with which the input spectrum is compared.
// Return:
//       - out       : Binary spectrum
//
WebRtc_UWord32 WebRtcAecm_BSpectrum(const WebRtc_UWord16 * const spectrum,
                                    const WebRtc_UWord16 * const thresvec)
{
    int k;
    WebRtc_UWord32 out;

    out = 0;
    for (k = BANDFIRST; k <= BANDLAST; k++)
    {
        if (spectrum[k] > thresvec[k])
        {
            out = WebRtcAecm_SetBit(out, k - BANDFIRST);
        }
    }

    return out;
}

//   WebRtcAecm_MedianEstimator(...)
//
//   Calculates the median recursively.
//
//   Input:
//           - newVal            :   new additional value
//           - medianVec         :   vector with current medians
//           - factor            :   factor for smoothing
//
//   Output:
//           - medianVec         :   vector with updated median
//
int WebRtcAecm_MedianEstimator(const WebRtc_UWord16 newVal, WebRtc_UWord16 * const medianVec,
                               const int factor)
{
    WebRtc_Word32 median;
    WebRtc_Word32 diff;

    median = (WebRtc_Word32)medianVec[0];

    //median = median + ((newVal-median)>>factor);
    diff = (WebRtc_Word32)newVal - median;
    diff = WEBRTC_SPL_SHIFT_W32(diff, -factor);
    median = median + diff;

    medianVec[0] = (WebRtc_UWord16)median;

    return 0;
}

int WebRtcAecm_CreateCore(AecmCore_t **aecmInst)
{
    AecmCore_t *aecm = malloc(sizeof(AecmCore_t));
    *aecmInst = aecm;
    if (aecm == NULL)
    {
        return -1;
    }

    if (WebRtcApm_CreateBuffer(&aecm->farFrameBuf, FRAME_LEN + PART_LEN) == -1)
    {
        WebRtcAecm_FreeCore(aecm);
        aecm = NULL;
        return -1;
    }

    if (WebRtcApm_CreateBuffer(&aecm->nearNoisyFrameBuf, FRAME_LEN + PART_LEN) == -1)
    {
        WebRtcAecm_FreeCore(aecm);
        aecm = NULL;
        return -1;
    }

    if (WebRtcApm_CreateBuffer(&aecm->nearCleanFrameBuf, FRAME_LEN + PART_LEN) == -1)
    {
        WebRtcAecm_FreeCore(aecm);
        aecm = NULL;
        return -1;
    }

    if (WebRtcApm_CreateBuffer(&aecm->outFrameBuf, FRAME_LEN + PART_LEN) == -1)
    {
        WebRtcAecm_FreeCore(aecm);
        aecm = NULL;
        return -1;
    }

    return 0;
}

// WebRtcAecm_InitCore(...)
//
// This function initializes the AECM instant created with WebRtcAecm_CreateCore(...)
// Input:
//      - aecm            : Pointer to the Echo Suppression instance
//      - samplingFreq   : Sampling Frequency
//
// Output:
//      - aecm            : Initialized instance
//
// Return value         :  0 - Ok
//                        -1 - Error
//
int WebRtcAecm_InitCore(AecmCore_t * const aecm, int samplingFreq)
{
    int retVal = 0;
    WebRtc_Word16 i;
    WebRtc_Word16 tmp16;

    if (samplingFreq != 8000 && samplingFreq != 16000)
    {
        samplingFreq = 8000;
        retVal = -1;
    }
    // sanity check of sampling frequency
    aecm->mult = (WebRtc_Word16)samplingFreq / 8000;

    aecm->farBufWritePos = 0;
    aecm->farBufReadPos = 0;
    aecm->knownDelay = 0;
    aecm->lastKnownDelay = 0;

    WebRtcApm_InitBuffer(aecm->farFrameBuf);
    WebRtcApm_InitBuffer(aecm->nearNoisyFrameBuf);
    WebRtcApm_InitBuffer(aecm->nearCleanFrameBuf);
    WebRtcApm_InitBuffer(aecm->outFrameBuf);

    memset(aecm->xBuf, 0, sizeof(aecm->xBuf));
    memset(aecm->dBufClean, 0, sizeof(aecm->dBufClean));
    memset(aecm->dBufNoisy, 0, sizeof(aecm->dBufNoisy));
    memset(aecm->outBuf, 0, sizeof(WebRtc_Word16) * PART_LEN);

    aecm->seed = 666;
    aecm->totCount = 0;

    memset(aecm->xfaHistory, 0, sizeof(WebRtc_UWord16) * (PART_LEN1) * MAX_DELAY);

    aecm->delHistoryPos = MAX_DELAY;

    memset(aecm->medianYlogspec, 0, sizeof(WebRtc_UWord16) * PART_LEN1);
    memset(aecm->medianXlogspec, 0, sizeof(WebRtc_UWord16) * PART_LEN1);
    memset(aecm->medianBCount, 0, sizeof(WebRtc_UWord16) * MAX_DELAY);
    memset(aecm->bxHistory, 0, sizeof(aecm->bxHistory));

    // Initialize to reasonable values
    aecm->currentDelay = 8;
    aecm->previousDelay = 8;
    aecm->delayAdjust = 0;

    aecm->nlpFlag = 1;
    aecm->fixedDelay = -1;

    memset(aecm->xfaQDomainBuf, 0, sizeof(WebRtc_Word16) * MAX_DELAY);
    aecm->dfaCleanQDomain = 0;
    aecm->dfaCleanQDomainOld = 0;
    aecm->dfaNoisyQDomain = 0;
    aecm->dfaNoisyQDomainOld = 0;

    memset(aecm->nearLogEnergy, 0, sizeof(WebRtc_Word16) * MAX_BUF_LEN);
    memset(aecm->farLogEnergy, 0, sizeof(WebRtc_Word16) * MAX_BUF_LEN);
    memset(aecm->echoAdaptLogEnergy, 0, sizeof(WebRtc_Word16) * MAX_BUF_LEN);
    memset(aecm->echoStoredLogEnergy, 0, sizeof(WebRtc_Word16) * MAX_BUF_LEN);

    // Initialize the echo channels with a stored shape.
    if (samplingFreq == 8000)
    {
        memcpy(aecm->channelAdapt16, kChannelStored8kHz, sizeof(WebRtc_Word16) * PART_LEN1);
    }
    else
    {
        memcpy(aecm->channelAdapt16, kChannelStored16kHz, sizeof(WebRtc_Word16) * PART_LEN1);
    }
    memcpy(aecm->channelStored, aecm->channelAdapt16, sizeof(WebRtc_Word16) * PART_LEN1);
    for (i = 0; i < PART_LEN1; i++)
    {
        aecm->channelAdapt32[i] = WEBRTC_SPL_LSHIFT_W32(
            (WebRtc_Word32)(aecm->channelAdapt16[i]), 16);
    }

    memset(aecm->echoFilt, 0, sizeof(WebRtc_Word32) * PART_LEN1);
    memset(aecm->nearFilt, 0, sizeof(WebRtc_Word16) * PART_LEN1);
    aecm->noiseEstCtr = 0;

    aecm->cngMode = AecmTrue;

    // Increase the noise Q domain with increasing frequency, to correspond to the
    // expected energy levels.
    // Also shape the initial noise level with this consideration.
#if (!defined ARM_WINM) && (!defined ARM9E_GCC) && (!defined ANDROID_AECOPT)
    for (i = 0; i < PART_LEN1; i++)
    {
        if (i < PART_LEN1 >> 2)
        {
            aecm->noiseEstQDomain[i] = 10;
            tmp16 = PART_LEN1 - i;
            aecm->noiseEst[i] = (tmp16 * tmp16) << 4;
        } else if (i < PART_LEN1 >> 1)
        {
            aecm->noiseEstQDomain[i] = 11;
            tmp16 = PART_LEN1 - i;
            aecm->noiseEst[i] = ((tmp16 * tmp16) << 4) << 1;
        } else
        {
            aecm->noiseEstQDomain[i] = 12;
            aecm->noiseEst[i] = aecm->noiseEst[(PART_LEN1 >> 1) - 1] << 1;
        }
    }
#else
    for (i = 0; i < PART_LEN1 >> 2; i++)
    {
        aecm->noiseEstQDomain[i] = 10;
        tmp16 = PART_LEN1 - i;
        aecm->noiseEst[i] = (tmp16 * tmp16) << 4;
    }
    for (; i < PART_LEN1 >> 1; i++)
    {
        aecm->noiseEstQDomain[i] = 11;
        tmp16 = PART_LEN1 - i;
        aecm->noiseEst[i] = ((tmp16 * tmp16) << 4) << 1;
    }
    for (; i < PART_LEN1; i++)
    {
        aecm->noiseEstQDomain[i] = 12;
        aecm->noiseEst[i] = aecm->noiseEst[(PART_LEN1 >> 1) - 1] << 1;
    }
#endif

    aecm->mseAdaptOld = 1000;
    aecm->mseStoredOld = 1000;
    aecm->mseThreshold = WEBRTC_SPL_WORD32_MAX;

    aecm->farEnergyMin = WEBRTC_SPL_WORD16_MAX;
    aecm->farEnergyMax = WEBRTC_SPL_WORD16_MIN;
    aecm->farEnergyMaxMin = 0;
    aecm->farEnergyVAD = FAR_ENERGY_MIN; // This prevents false speech detection at the
                                         // beginning.
    aecm->farEnergyMSE = 0;
    aecm->currentVADValue = 0;
    aecm->vadUpdateCount = 0;
    aecm->firstVAD = 1;

    aecm->delayCount = 0;
    aecm->newDelayCorrData = 0;
    aecm->lastDelayUpdateCount = 0;
    memset(aecm->delayCorrelation, 0, sizeof(WebRtc_Word16) * ((CORR_MAX << 1) + 1));

    aecm->startupState = 0;
    aecm->mseChannelCount = 0;
    aecm->supGain = SUPGAIN_DEFAULT;
    aecm->supGainOld = SUPGAIN_DEFAULT;
    aecm->delayOffsetFlag = 0;

    memset(aecm->delayHistogram, 0, sizeof(aecm->delayHistogram));
    aecm->delayVadCount = 0;
    aecm->maxDelayHistIdx = 0;
    aecm->lastMinPos = 0;

    aecm->supGainErrParamA = SUPGAIN_ERROR_PARAM_A;
    aecm->supGainErrParamD = SUPGAIN_ERROR_PARAM_D;
    aecm->supGainErrParamDiffAB = SUPGAIN_ERROR_PARAM_A - SUPGAIN_ERROR_PARAM_B;
    aecm->supGainErrParamDiffBD = SUPGAIN_ERROR_PARAM_B - SUPGAIN_ERROR_PARAM_D;

    return 0;
}

int WebRtcAecm_Control(AecmCore_t *aecm, int delay, int nlpFlag, int delayOffsetFlag)
{
    aecm->nlpFlag = nlpFlag;
    aecm->fixedDelay = delay;
    aecm->delayOffsetFlag = delayOffsetFlag;

    return 0;
}

// WebRtcAecm_GetNewDelPos(...)
//
// Moves the pointer to the next entry. Returns to zero if max position reached.
//
// Input:
//       - aecm     : Pointer to the AECM instance
// Return:
//       - pos      : New position in the history.
//
//
WebRtc_Word16 WebRtcAecm_GetNewDelPos(AecmCore_t * const aecm)
{
    WebRtc_Word16 pos;

    pos = aecm->delHistoryPos;
    pos++;
    if (pos >= MAX_DELAY)
    {
        pos = 0;
    }
    aecm->delHistoryPos = pos;

    return pos;
}

// WebRtcAecm_EstimateDelay(...)
//
// Estimate the delay of the echo signal.
//
// Inputs:
//      - aecm          : Pointer to the AECM instance
//      - farSpec       : Delayed farend magnitude spectrum
//      - nearSpec      : Nearend magnitude spectrum
//      - stages        : Q-domain of xxFIX and yyFIX (without dynamic Q-domain)
//      - xfaQ          : normalization factor, i.e., Q-domain before FFT
// Return:
//      - delay         : Estimated delay
//
WebRtc_Word16 WebRtcAecm_EstimateDelay(AecmCore_t * const aecm,
                                       const WebRtc_UWord16 * const farSpec,
                                       const WebRtc_UWord16 * const nearSpec,
                                       const WebRtc_Word16 xfaQ)
{
    WebRtc_UWord32 bxspectrum, byspectrum;
    WebRtc_UWord32 bcount[MAX_DELAY];

    int i, res;

    WebRtc_UWord16 xmean[PART_LEN1], ymean[PART_LEN1];
    WebRtc_UWord16 dtmp1;
    WebRtc_Word16 fcount[MAX_DELAY];

    //WebRtc_Word16 res;
    WebRtc_Word16 histpos;
    WebRtc_Word16 maxHistLvl;
    WebRtc_UWord16 *state;
    WebRtc_Word16 minpos = -1;

    enum
    {
        kVadCountThreshold = 25
    };
    enum
    {
        kMaxHistogram = 600
    };

    histpos = WebRtcAecm_GetNewDelPos(aecm);

    for (i = 0; i < PART_LEN1; i++)
    {
        aecm->xfaHistory[i][histpos] = farSpec[i];

        state = &(aecm->medianXlogspec[i]);
        res = WebRtcAecm_MedianEstimator(farSpec[i], state, 6);

        state = &(aecm->medianYlogspec[i]);
        res = WebRtcAecm_MedianEstimator(nearSpec[i], state, 6);

        //  Mean:
        //  FLOAT:
        //  ymean = dtmp2/MAX_DELAY
        //
        //  FIX:
        //  input: dtmp2FIX in Q0
        //  output: ymeanFIX in Q8
        //  20 = 1/MAX_DELAY in Q13 = 1/MAX_DELAY * 2^13
        xmean[i] = (aecm->medianXlogspec[i]);
        ymean[i] = (aecm->medianYlogspec[i]);

    }
    // Update Q-domain buffer
    aecm->xfaQDomainBuf[histpos] = xfaQ;

    // Get binary spectra
    //  FLOAT:
    //  bxspectrum = bspectrum(xlogspec, xmean);
    //
    //  FIX:
    //  input:  xlogspecFIX,ylogspecFIX in Q8
    //          xmeanFIX, ymeanFIX in Q8
    //  output: unsigned long bxspectrum, byspectrum in Q0
    bxspectrum = WebRtcAecm_BSpectrum(farSpec, xmean);
    byspectrum = WebRtcAecm_BSpectrum(nearSpec, ymean);

    // Shift binary spectrum history
    memmove(&(aecm->bxHistory[1]), &(aecm->bxHistory[0]),
            (MAX_DELAY - 1) * sizeof(WebRtc_UWord32));

    aecm->bxHistory[0] = bxspectrum;

    // Compare with delayed spectra
    WebRtcAecm_Hisser(byspectrum, aecm->bxHistory, bcount);

    for (i = 0; i < MAX_DELAY; i++)
    {
        // Update sum
        // bcount is constrained to [0, 32], meaning we can smooth with a factor up to 2^11.
        dtmp1 = (WebRtc_UWord16)bcount[i];
        dtmp1 = WEBRTC_SPL_LSHIFT_W16(dtmp1, 9);
        state = &(aecm->medianBCount[i]);
        res = WebRtcAecm_MedianEstimator(dtmp1, state, 9);
        fcount[i] = (aecm->medianBCount[i]);
    }

    // Find minimum
    minpos = WebRtcSpl_MinIndexW16(fcount, MAX_DELAY);

    // If the farend has been active sufficiently long, begin accumulating a histogram
    // of the minimum positions. Search for the maximum bin to determine the delay.
    if (aecm->currentVADValue == 1)
    {
        if (aecm->delayVadCount >= kVadCountThreshold)
        {
            // Increment the histogram at the current minimum position.
            if (aecm->delayHistogram[minpos] < kMaxHistogram)
            {
                aecm->delayHistogram[minpos] += 3;
            }

#if (!defined ARM_WINM) && (!defined ARM9E_GCC) && (!defined ANDROID_AECOPT)
            // Decrement the entire histogram.
            for (i = 0; i < MAX_DELAY; i++)
            {
                if (aecm->delayHistogram[i] > 0)
                {
                    aecm->delayHistogram[i]--;
                }
            }

            // Select the histogram index corresponding to the maximum bin as the delay.
            maxHistLvl = 0;
            aecm->maxDelayHistIdx = 0;
            for (i = 0; i < MAX_DELAY; i++)
            {
                if (aecm->delayHistogram[i] > maxHistLvl)
                {
                    maxHistLvl = aecm->delayHistogram[i];
                    aecm->maxDelayHistIdx = i;
                }
            }
#else
            maxHistLvl = 0;
            aecm->maxDelayHistIdx = 0;

            for (i = 0; i < MAX_DELAY; i++)
            {
                WebRtc_Word16 tempVar = aecm->delayHistogram[i];

                // Decrement the entire histogram.
                if (tempVar > 0)
                {
                    tempVar--;
                    aecm->delayHistogram[i] = tempVar;

                    // Select the histogram index corresponding to the maximum bin as the delay.
                    if (tempVar > maxHistLvl)
                    {
                        maxHistLvl = tempVar;
                        aecm->maxDelayHistIdx = i;
                    }
                }
            }
#endif
        } else
        {
            aecm->delayVadCount++;
        }
    } else
    {
        aecm->delayVadCount = 0;
    }

    return aecm->maxDelayHistIdx;
}

int WebRtcAecm_FreeCore(AecmCore_t *aecm)
{
    if (aecm == NULL)
    {
        return -1;
    }

    WebRtcApm_FreeBuffer(aecm->farFrameBuf);
    WebRtcApm_FreeBuffer(aecm->nearNoisyFrameBuf);
    WebRtcApm_FreeBuffer(aecm->nearCleanFrameBuf);
    WebRtcApm_FreeBuffer(aecm->outFrameBuf);

    free(aecm);

    return 0;
}

void WebRtcAecm_ProcessFrame(AecmCore_t * const aecm, const WebRtc_Word16 * const farend,
                             const WebRtc_Word16 * const nearendNoisy,
                             const WebRtc_Word16 * const nearendClean,
                             WebRtc_Word16 * const out)
{
    WebRtc_Word16 farBlock[PART_LEN];
    WebRtc_Word16 nearNoisyBlock[PART_LEN];
    WebRtc_Word16 nearCleanBlock[PART_LEN];
    WebRtc_Word16 outBlock[PART_LEN];
    WebRtc_Word16 farFrame[FRAME_LEN];
    int size = 0;

    // Buffer the current frame.
    // Fetch an older one corresponding to the delay.
    WebRtcAecm_BufferFarFrame(aecm, farend, FRAME_LEN);
    WebRtcAecm_FetchFarFrame(aecm, farFrame, FRAME_LEN, aecm->knownDelay);

    // Buffer the synchronized far and near frames,
    // to pass the smaller blocks individually.
    WebRtcApm_WriteBuffer(aecm->farFrameBuf, farFrame, FRAME_LEN);
    WebRtcApm_WriteBuffer(aecm->nearNoisyFrameBuf, nearendNoisy, FRAME_LEN);
    if (nearendClean != NULL)
    {
        WebRtcApm_WriteBuffer(aecm->nearCleanFrameBuf, nearendClean, FRAME_LEN);
    }

    // Process as many blocks as possible.
    while (WebRtcApm_get_buffer_size(aecm->farFrameBuf) >= PART_LEN)
    {
        WebRtcApm_ReadBuffer(aecm->farFrameBuf, farBlock, PART_LEN);
        WebRtcApm_ReadBuffer(aecm->nearNoisyFrameBuf, nearNoisyBlock, PART_LEN);
        if (nearendClean != NULL)
        {
            WebRtcApm_ReadBuffer(aecm->nearCleanFrameBuf, nearCleanBlock, PART_LEN);
            WebRtcAecm_ProcessBlock(aecm, farBlock, nearNoisyBlock, nearCleanBlock, outBlock);
        } else
        {
            WebRtcAecm_ProcessBlock(aecm, farBlock, nearNoisyBlock, NULL, outBlock);
        }

        WebRtcApm_WriteBuffer(aecm->outFrameBuf, outBlock, PART_LEN);
    }

    // Stuff the out buffer if we have less than a frame to output.
    // This should only happen for the first frame.
    size = WebRtcApm_get_buffer_size(aecm->outFrameBuf);
    if (size < FRAME_LEN)
    {
        WebRtcApm_StuffBuffer(aecm->outFrameBuf, FRAME_LEN - size);
    }

    // Obtain an output frame.
    WebRtcApm_ReadBuffer(aecm->outFrameBuf, out, FRAME_LEN);
}

// WebRtcAecm_AsymFilt(...)
//
// Performs asymmetric filtering.
//
// Inputs:
//      - filtOld       : Previous filtered value.
//      - inVal         : New input value.
//      - stepSizePos   : Step size when we have a positive contribution.
//      - stepSizeNeg   : Step size when we have a negative contribution.
//
// Output:
//
// Return: - Filtered value.
//
WebRtc_Word16 WebRtcAecm_AsymFilt(const WebRtc_Word16 filtOld, const WebRtc_Word16 inVal,
                                  const WebRtc_Word16 stepSizePos,
                                  const WebRtc_Word16 stepSizeNeg)
{
    WebRtc_Word16 retVal;

    if ((filtOld == WEBRTC_SPL_WORD16_MAX) | (filtOld == WEBRTC_SPL_WORD16_MIN))
    {
        return inVal;
    }
    retVal = filtOld;
    if (filtOld > inVal)
    {
        retVal -= WEBRTC_SPL_RSHIFT_W16(filtOld - inVal, stepSizeNeg);
    } else
    {
        retVal += WEBRTC_SPL_RSHIFT_W16(inVal - filtOld, stepSizePos);
    }

    return retVal;
}

// WebRtcAecm_CalcEnergies(...)
//
// This function calculates the log of energies for nearend, farend and estimated
// echoes. There is also an update of energy decision levels, i.e. internl VAD.
//
//
// @param  aecm         [i/o]   Handle of the AECM instance.
// @param  delayDiff    [in]    Delay position in farend buffer.
// @param  nearEner     [in]    Near end energy for current block (Q[aecm->dfaQDomain]).
// @param  echoEst      [i/o]   Estimated echo
//                              (Q[aecm->xfaQDomain[delayDiff]+RESOLUTION_CHANNEL16]).
//
void WebRtcAecm_CalcEnergies(AecmCore_t * const aecm, const WebRtc_Word16 delayDiff,
                             const WebRtc_UWord32 nearEner, WebRtc_Word32 * const echoEst)
{
    // Local variables
    WebRtc_UWord32 tmpAdapt, tmpStored, tmpFar;

    int i;

    WebRtc_Word16 zeros, frac;
    WebRtc_Word16 tmp16;
    WebRtc_Word16 increase_max_shifts = 4;
    WebRtc_Word16 decrease_max_shifts = 11;
    WebRtc_Word16 increase_min_shifts = 11;
    WebRtc_Word16 decrease_min_shifts = 3;

    // Get log of near end energy and store in buffer

    // Shift buffer
    memmove(aecm->nearLogEnergy + 1, aecm->nearLogEnergy,
            sizeof(WebRtc_Word16) * (MAX_BUF_LEN - 1));

    // Logarithm of integrated magnitude spectrum (nearEner)
    if (nearEner)
    {
        zeros = WebRtcSpl_NormU32(nearEner);
        frac = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_U32(
                              (WEBRTC_SPL_LSHIFT_U32(nearEner, zeros) & 0x7FFFFFFF),
                              23);
        // log2 in Q8
        aecm->nearLogEnergy[0] = WEBRTC_SPL_LSHIFT_W16((31 - zeros), 8) + frac;
        aecm->nearLogEnergy[0] -= WEBRTC_SPL_LSHIFT_W16(aecm->dfaNoisyQDomain, 8);
    } else
    {
        aecm->nearLogEnergy[0] = 0;
    }
    aecm->nearLogEnergy[0] += WEBRTC_SPL_LSHIFT_W16(PART_LEN_SHIFT, 7);
    // END: Get log of near end energy

    // Get energy for the delayed far end signal and estimated
    // echo using both stored and adapted channels.
    tmpAdapt = 0;
    tmpStored = 0;
    tmpFar = 0;

    for (i = 0; i < PART_LEN1; i++)
    {
        // Get estimated echo energies for adaptive channel and stored channel
        echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
                aecm->xfaHistory[i][delayDiff]);
        tmpFar += (WebRtc_UWord32)(aecm->xfaHistory[i][delayDiff]);
        tmpAdapt += WEBRTC_SPL_UMUL_16_16(aecm->channelAdapt16[i],
                aecm->xfaHistory[i][delayDiff]);
        tmpStored += (WebRtc_UWord32)echoEst[i];
    }
    // Shift buffers
    memmove(aecm->farLogEnergy + 1, aecm->farLogEnergy,
            sizeof(WebRtc_Word16) * (MAX_BUF_LEN - 1));
    memmove(aecm->echoAdaptLogEnergy + 1, aecm->echoAdaptLogEnergy,
            sizeof(WebRtc_Word16) * (MAX_BUF_LEN - 1));
    memmove(aecm->echoStoredLogEnergy + 1, aecm->echoStoredLogEnergy,
            sizeof(WebRtc_Word16) * (MAX_BUF_LEN - 1));

    // Logarithm of delayed far end energy
    if (tmpFar)
    {
        zeros = WebRtcSpl_NormU32(tmpFar);
        frac = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_U32((WEBRTC_SPL_LSHIFT_U32(tmpFar, zeros)
                        & 0x7FFFFFFF), 23);
        // log2 in Q8
        aecm->farLogEnergy[0] = WEBRTC_SPL_LSHIFT_W16((31 - zeros), 8) + frac;
        aecm->farLogEnergy[0] -= WEBRTC_SPL_LSHIFT_W16(aecm->xfaQDomainBuf[delayDiff], 8);
    } else
    {
        aecm->farLogEnergy[0] = 0;
    }
    aecm->farLogEnergy[0] += WEBRTC_SPL_LSHIFT_W16(PART_LEN_SHIFT, 7);

    // Logarithm of estimated echo energy through adapted channel
    if (tmpAdapt)
    {
        zeros = WebRtcSpl_NormU32(tmpAdapt);
        frac = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_U32((WEBRTC_SPL_LSHIFT_U32(tmpAdapt, zeros)
                        & 0x7FFFFFFF), 23);
        //log2 in Q8
        aecm->echoAdaptLogEnergy[0] = WEBRTC_SPL_LSHIFT_W16((31 - zeros), 8) + frac;
        aecm->echoAdaptLogEnergy[0]
                -= WEBRTC_SPL_LSHIFT_W16(RESOLUTION_CHANNEL16 + aecm->xfaQDomainBuf[delayDiff], 8);
    } else
    {
        aecm->echoAdaptLogEnergy[0] = 0;
    }
    aecm->echoAdaptLogEnergy[0] += WEBRTC_SPL_LSHIFT_W16(PART_LEN_SHIFT, 7);

    // Logarithm of estimated echo energy through stored channel
    if (tmpStored)
    {
        zeros = WebRtcSpl_NormU32(tmpStored);
        frac = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_U32((WEBRTC_SPL_LSHIFT_U32(tmpStored, zeros)
                        & 0x7FFFFFFF), 23);
        //log2 in Q8
        aecm->echoStoredLogEnergy[0] = WEBRTC_SPL_LSHIFT_W16((31 - zeros), 8) + frac;
        aecm->echoStoredLogEnergy[0]
                -= WEBRTC_SPL_LSHIFT_W16(RESOLUTION_CHANNEL16 + aecm->xfaQDomainBuf[delayDiff], 8);
    } else
    {
        aecm->echoStoredLogEnergy[0] = 0;
    }
    aecm->echoStoredLogEnergy[0] += WEBRTC_SPL_LSHIFT_W16(PART_LEN_SHIFT, 7);

    // Update farend energy levels (min, max, vad, mse)
    if (aecm->farLogEnergy[0] > FAR_ENERGY_MIN)
    {
        if (aecm->startupState == 0)
        {
            increase_max_shifts = 2;
            decrease_min_shifts = 2;
            increase_min_shifts = 8;
        }

        aecm->farEnergyMin = WebRtcAecm_AsymFilt(aecm->farEnergyMin, aecm->farLogEnergy[0],
                                                 increase_min_shifts, decrease_min_shifts);
        aecm->farEnergyMax = WebRtcAecm_AsymFilt(aecm->farEnergyMax, aecm->farLogEnergy[0],
                                                 increase_max_shifts, decrease_max_shifts);
        aecm->farEnergyMaxMin = (aecm->farEnergyMax - aecm->farEnergyMin);

        // Dynamic VAD region size
        tmp16 = 2560 - aecm->farEnergyMin;
        if (tmp16 > 0)
        {
            tmp16 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmp16, FAR_ENERGY_VAD_REGION, 9);
        } else
        {
            tmp16 = 0;
        }
        tmp16 += FAR_ENERGY_VAD_REGION;

        if ((aecm->startupState == 0) | (aecm->vadUpdateCount > 1024))
        {
            // In startup phase or VAD update halted
            aecm->farEnergyVAD = aecm->farEnergyMin + tmp16;
        } else
        {
            if (aecm->farEnergyVAD > aecm->farLogEnergy[0])
            {
                aecm->farEnergyVAD += WEBRTC_SPL_RSHIFT_W16(aecm->farLogEnergy[0] + tmp16
                        - aecm->farEnergyVAD, 6);
                aecm->vadUpdateCount = 0;
            } else
            {
                aecm->vadUpdateCount++;
            }
        }
        // Put MSE threshold higher than VAD
        aecm->farEnergyMSE = aecm->farEnergyVAD + (1 << 8);
    }

    // Update VAD variables
    if (aecm->farLogEnergy[0] > aecm->farEnergyVAD)
    {
        if ((aecm->startupState == 0) | (aecm->farEnergyMaxMin > FAR_ENERGY_DIFF))
        {
            // We are in startup or have significant dynamics in input speech level
            aecm->currentVADValue = 1;
        }
    } else
    {
        aecm->currentVADValue = 0;
    }
    if ((aecm->currentVADValue) && (aecm->firstVAD))
    {
        aecm->firstVAD = 0;
        if (aecm->echoAdaptLogEnergy[0] > aecm->nearLogEnergy[0])
        {
            // The estimated echo has higher energy than the near end signal. This means that
            // the initialization was too aggressive. Scale down by a factor 8
            for (i = 0; i < PART_LEN1; i++)
            {
                aecm->channelAdapt16[i] >>= 3;
            }
            // Compensate the adapted echo energy level accordingly.
            aecm->echoAdaptLogEnergy[0] -= (3 << 8);
            aecm->firstVAD = 1;
        }
    }
    // END: Energies of delayed far, echo estimates
    // TODO(bjornv): Will be removed in final version.
#ifdef VAD_DATA
    fwrite(&(aecm->currentVADValue), sizeof(WebRtc_Word16), 1, aecm->vad_file);
    fwrite(&(aecm->currentDelay), sizeof(WebRtc_Word16), 1, aecm->delay_file);
    fwrite(&(aecm->farLogEnergy[0]), sizeof(WebRtc_Word16), 1, aecm->far_cur_file);
    fwrite(&(aecm->farEnergyMin), sizeof(WebRtc_Word16), 1, aecm->far_min_file);
    fwrite(&(aecm->farEnergyMax), sizeof(WebRtc_Word16), 1, aecm->far_max_file);
    fwrite(&(aecm->farEnergyVAD), sizeof(WebRtc_Word16), 1, aecm->far_vad_file);
#endif
}

// WebRtcAecm_CalcStepSize(...)
//
// This function calculates the step size used in channel estimation
//
//
// @param  aecm  [in]    Handle of the AECM instance.
// @param  mu   [out]   (Return value) Stepsize in log2(), i.e. number of shifts.
//
//
WebRtc_Word16 WebRtcAecm_CalcStepSize(AecmCore_t * const aecm)
{

    WebRtc_Word32 tmp32;
    WebRtc_Word16 tmp16;
    WebRtc_Word16 mu;

    // Here we calculate the step size mu used in the
    // following NLMS based Channel estimation algorithm
    mu = MU_MAX;
    if (!aecm->currentVADValue)
    {
        // Far end energy level too low, no channel update
        mu = 0;
    } else if (aecm->startupState > 0)
    {
        if (aecm->farEnergyMin >= aecm->farEnergyMax)
        {
            mu = MU_MIN;
        } else
        {
            tmp16 = (aecm->farLogEnergy[0] - aecm->farEnergyMin);
            tmp32 = WEBRTC_SPL_MUL_16_16(tmp16, MU_DIFF);
            tmp32 = WebRtcSpl_DivW32W16(tmp32, aecm->farEnergyMaxMin);
            mu = MU_MIN - 1 - (WebRtc_Word16)(tmp32);
            // The -1 is an alternative to rounding. This way we get a larger
            // stepsize, so we in some sense compensate for truncation in NLMS
        }
        if (mu < MU_MAX)
        {
            mu = MU_MAX; // Equivalent with maximum step size of 2^-MU_MAX
        }
    }
    // END: Update step size

    return mu;
}

// WebRtcAecm_UpdateChannel(...)
//
// This function performs channel estimation. NLMS and decision on channel storage.
//
//
// @param  aecm         [i/o]   Handle of the AECM instance.
// @param  dfa          [in]    Absolute value of the nearend signal (Q[aecm->dfaQDomain])
// @param  delayDiff    [in]    Delay position in farend buffer.
// @param  mu           [in]    NLMS step size.
// @param  echoEst      [i/o]   Estimated echo
//                              (Q[aecm->xfaQDomain[delayDiff]+RESOLUTION_CHANNEL16]).
//
void WebRtcAecm_UpdateChannel(AecmCore_t * const aecm, const WebRtc_UWord16 * const dfa,
                              const WebRtc_Word16 delayDiff, const WebRtc_Word16 mu,
                              WebRtc_Word32 * const echoEst)
{

    WebRtc_UWord32 tmpU32no1, tmpU32no2;
    WebRtc_Word32 tmp32no1, tmp32no2;
    WebRtc_Word32 mseStored;
    WebRtc_Word32 mseAdapt;

    int i;

    WebRtc_Word16 zerosFar, zerosNum, zerosCh, zerosDfa;
    WebRtc_Word16 shiftChFar, shiftNum, shift2ResChan;
    WebRtc_Word16 tmp16no1;
    WebRtc_Word16 xfaQ, dfaQ;

    // This is the channel estimation algorithm. It is base on NLMS but has a variable step
    // length, which was calculated above.
    if (mu)
    {
        for (i = 0; i < PART_LEN1; i++)
        {
            // Determine norm of channel and farend to make sure we don't get overflow in
            // multiplication
            zerosCh = WebRtcSpl_NormU32(aecm->channelAdapt32[i]);
            zerosFar = WebRtcSpl_NormU32((WebRtc_UWord32)aecm->xfaHistory[i][delayDiff]);
            if (zerosCh + zerosFar > 31)
            {
                // Multiplication is safe
                tmpU32no1 = WEBRTC_SPL_UMUL_32_16(aecm->channelAdapt32[i],
                        aecm->xfaHistory[i][delayDiff]);
                shiftChFar = 0;
            } else
            {
                // We need to shift down before multiplication
                shiftChFar = 32 - zerosCh - zerosFar;
                tmpU32no1
                        = WEBRTC_SPL_UMUL_32_16(WEBRTC_SPL_RSHIFT_W32(aecm->channelAdapt32[i],
                                        shiftChFar),
                                aecm->xfaHistory[i][delayDiff]);
            }
            // Determine Q-domain of numerator
            zerosNum = WebRtcSpl_NormU32(tmpU32no1);
            if (dfa[i])
            {
                zerosDfa = WebRtcSpl_NormU32((WebRtc_UWord32)dfa[i]);
            } else
            {
                zerosDfa = 32;
            }
            tmp16no1 = zerosDfa - 2 + aecm->dfaNoisyQDomain - RESOLUTION_CHANNEL32
                    - aecm->xfaQDomainBuf[delayDiff] + shiftChFar;
            if (zerosNum > tmp16no1 + 1)
            {
                xfaQ = tmp16no1;
                dfaQ = zerosDfa - 2;
            } else
            {
                xfaQ = zerosNum - 2;
                dfaQ = RESOLUTION_CHANNEL32 + aecm->xfaQDomainBuf[delayDiff]
                        - aecm->dfaNoisyQDomain - shiftChFar + xfaQ;
            }
            // Add in the same Q-domain
            tmpU32no1 = WEBRTC_SPL_SHIFT_W32(tmpU32no1, xfaQ);
            tmpU32no2 = WEBRTC_SPL_SHIFT_W32((WebRtc_UWord32)dfa[i], dfaQ);
            tmp32no1 = (WebRtc_Word32)tmpU32no2 - (WebRtc_Word32)tmpU32no1;
            zerosNum = WebRtcSpl_NormW32(tmp32no1);
            if ((tmp32no1) && (aecm->xfaHistory[i][delayDiff] > (CHANNEL_VAD
                    << aecm->xfaQDomainBuf[delayDiff])))
            {
                //
                // Update is needed
                //
                // This is what we would like to compute
                //
                // tmp32no1 = dfa[i] - (aecm->channelAdapt[i] * aecm->xfaHistory[i][delayDiff])
                // tmp32norm = (i + 1)
                // aecm->channelAdapt[i] += (2^mu) * tmp32no1
                //                        / (tmp32norm * aecm->xfaHistory[i][delayDiff])
                //

                // Make sure we don't get overflow in multiplication.
                if (zerosNum + zerosFar > 31)
                {
                    if (tmp32no1 > 0)
                    {
                        tmp32no2 = (WebRtc_Word32)WEBRTC_SPL_UMUL_32_16(tmp32no1,
                                aecm->xfaHistory[i][delayDiff]);
                    } else
                    {
                        tmp32no2 = -(WebRtc_Word32)WEBRTC_SPL_UMUL_32_16(-tmp32no1,
                                aecm->xfaHistory[i][delayDiff]);
                    }
                    shiftNum = 0;
                } else
                {
                    shiftNum = 32 - (zerosNum + zerosFar);
                    if (tmp32no1 > 0)
                    {
                        tmp32no2 = (WebRtc_Word32)WEBRTC_SPL_UMUL_32_16(
                                WEBRTC_SPL_RSHIFT_W32(tmp32no1, shiftNum),
                                aecm->xfaHistory[i][delayDiff]);
                    } else
                    {
                        tmp32no2 = -(WebRtc_Word32)WEBRTC_SPL_UMUL_32_16(
                                WEBRTC_SPL_RSHIFT_W32(-tmp32no1, shiftNum),
                                aecm->xfaHistory[i][delayDiff]);
                    }
                }
                // Normalize with respect to frequency bin
                tmp32no2 = WebRtcSpl_DivW32W16(tmp32no2, i + 1);
                // Make sure we are in the right Q-domain
                shift2ResChan = shiftNum + shiftChFar - xfaQ - mu - ((30 - zerosFar) << 1);
                if (WebRtcSpl_NormW32(tmp32no2) < shift2ResChan)
                {
                    tmp32no2 = WEBRTC_SPL_WORD32_MAX;
                } else
                {
                    tmp32no2 = WEBRTC_SPL_SHIFT_W32(tmp32no2, shift2ResChan);
                }
                aecm->channelAdapt32[i] = WEBRTC_SPL_ADD_SAT_W32(aecm->channelAdapt32[i],
                        tmp32no2);
                if (aecm->channelAdapt32[i] < 0)
                {
                    // We can never have negative channel gain
                    aecm->channelAdapt32[i] = 0;
                }
                aecm->channelAdapt16[i]
                        = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(aecm->channelAdapt32[i], 16);
            }
        }
    }
    // END: Adaptive channel update

    // Determine if we should store or restore the channel
    if ((aecm->startupState == 0) & (aecm->currentVADValue))
    {
        // During startup we store the channel every block.
        memcpy(aecm->channelStored, aecm->channelAdapt16, sizeof(WebRtc_Word16) * PART_LEN1);
        // TODO(bjornv): Will be removed in final version.
#ifdef STORE_CHANNEL_DATA
        fwrite(aecm->channelStored, sizeof(WebRtc_Word16), PART_LEN1, aecm->channel_file_init);
#endif
        // Recalculate echo estimate
#if (!defined ARM_WINM) && (!defined ARM9E_GCC) && (!defined ANDROID_AECOPT)
        for (i = 0; i < PART_LEN1; i++)
        {
            echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
                    aecm->xfaHistory[i][delayDiff]);
        }
#else
        for (i = 0; i < PART_LEN; ) //assume PART_LEN is 4's multiples

        {
            echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
                    aecm->xfaHistory[i][delayDiff]);
            i++;
            echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
                    aecm->xfaHistory[i][delayDiff]);
            i++;
            echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
                    aecm->xfaHistory[i][delayDiff]);
            i++;
            echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
                    aecm->xfaHistory[i][delayDiff]);
            i++;
        }
        echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i],
                aecm->xfaHistory[i][delayDiff]);
#endif
    } else
    {
        if (aecm->farLogEnergy[0] < aecm->farEnergyMSE)
        {
            aecm->mseChannelCount = 0;
            aecm->delayCount = 0;
        } else
        {
            aecm->mseChannelCount++;
            aecm->delayCount++;
        }
        // Enough data for validation. Store channel if we can.
        if (aecm->mseChannelCount >= (MIN_MSE_COUNT + 10))
        {
            // We have enough data.
            // Calculate MSE of "Adapt" and "Stored" versions.
            // It is actually not MSE, but average absolute error.
            mseStored = 0;
            mseAdapt = 0;
            for (i = 0; i < MIN_MSE_COUNT; i++)
            {
                tmp32no1 = ((WebRtc_Word32)aecm->echoStoredLogEnergy[i]
                        - (WebRtc_Word32)aecm->nearLogEnergy[i]);
                tmp32no2 = WEBRTC_SPL_ABS_W32(tmp32no1);
                mseStored += tmp32no2;

                tmp32no1 = ((WebRtc_Word32)aecm->echoAdaptLogEnergy[i]
                        - (WebRtc_Word32)aecm->nearLogEnergy[i]);
                tmp32no2 = WEBRTC_SPL_ABS_W32(tmp32no1);
                mseAdapt += tmp32no2;
            }
            if (((mseStored << MSE_RESOLUTION) < (MIN_MSE_DIFF * mseAdapt))
                    & ((aecm->mseStoredOld << MSE_RESOLUTION) < (MIN_MSE_DIFF
                            * aecm->mseAdaptOld)))
            {
                // The stored channel has a significantly lower MSE than the adaptive one for
                // two consecutive calculations. Reset the adaptive channel.
                memcpy(aecm->channelAdapt16, aecm->channelStored,
                       sizeof(WebRtc_Word16) * PART_LEN1);
                // Restore the W32 channel
#if (!defined ARM_WINM) && (!defined ARM9E_GCC) && (!defined ANDROID_AECOPT)
                for (i = 0; i < PART_LEN1; i++)
                {
                    aecm->channelAdapt32[i]
                            = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)aecm->channelStored[i], 16);
                }
#else
                for (i = 0; i < PART_LEN; ) //assume PART_LEN is 4's multiples

                {
                    aecm->channelAdapt32[i] = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)aecm->channelStored[i], 16);
                    i++;
                    aecm->channelAdapt32[i] = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)aecm->channelStored[i], 16);
                    i++;
                    aecm->channelAdapt32[i] = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)aecm->channelStored[i], 16);
                    i++;
                    aecm->channelAdapt32[i] = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)aecm->channelStored[i], 16);
                    i++;
                }
                aecm->channelAdapt32[i] = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)aecm->channelStored[i], 16);
#endif

            } else if (((MIN_MSE_DIFF * mseStored) > (mseAdapt << MSE_RESOLUTION)) & (mseAdapt
                    < aecm->mseThreshold) & (aecm->mseAdaptOld < aecm->mseThreshold))
            {
                // The adaptive channel has a significantly lower MSE than the stored one.
                // The MSE for the adaptive channel has also been low for two consecutive
                // calculations. Store the adaptive channel.
                memcpy(aecm->channelStored, aecm->channelAdapt16,
                       sizeof(WebRtc_Word16) * PART_LEN1);
                // TODO(bjornv): Will be removed in final version.
#ifdef STORE_CHANNEL_DATA
                fwrite(aecm->channelStored, sizeof(WebRtc_Word16), PART_LEN1,
                       aecm->channel_file);
#endif
// Recalculate echo estimate
#if (!defined ARM_WINM) && (!defined ARM9E_GCC) && (!defined ANDROID_AECOPT)
                for (i = 0; i < PART_LEN1; i++)
                {
                    echoEst[i]
                            = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i], aecm->xfaHistory[i][delayDiff]);
                }
#else
                for (i = 0; i < PART_LEN; ) //assume PART_LEN is 4's multiples

                {
                    echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i], aecm->xfaHistory[i][delayDiff]);
                    i++;
                    echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i], aecm->xfaHistory[i][delayDiff]);
                    i++;
                    echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i], aecm->xfaHistory[i][delayDiff]);
                    i++;
                    echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i], aecm->xfaHistory[i][delayDiff]);
                    i++;
                }
                echoEst[i] = WEBRTC_SPL_MUL_16_U16(aecm->channelStored[i], aecm->xfaHistory[i][delayDiff]);
#endif
                // Update threshold
                if (aecm->mseThreshold == WEBRTC_SPL_WORD32_MAX)
                {
                    aecm->mseThreshold = (mseAdapt + aecm->mseAdaptOld);
                } else
                {
                    aecm->mseThreshold += WEBRTC_SPL_MUL_16_16_RSFT(mseAdapt
                            - WEBRTC_SPL_MUL_16_16_RSFT(aecm->mseThreshold, 5, 3), 205, 8);
                }

            }

            // Reset counter
            aecm->mseChannelCount = 0;

            // Store the MSE values.
            aecm->mseStoredOld = mseStored;
            aecm->mseAdaptOld = mseAdapt;
        }
    }
    // END: Determine if we should store or reset channel estimate.
}

// WebRtcAecm_CalcSuppressionGain(...)
//
// This function calculates the suppression gain that is used in the Wiener filter.
//
//
// @param  aecm     [i/n]   Handle of the AECM instance.
// @param  supGain  [out]   (Return value) Suppression gain with which to scale the noise
//                          level (Q14).
//
//
WebRtc_Word16 WebRtcAecm_CalcSuppressionGain(AecmCore_t * const aecm)
{
    WebRtc_Word32 tmp32no1;

    WebRtc_Word16 supGain;
    WebRtc_Word16 tmp16no1;
    WebRtc_Word16 dE = 0;

    // Determine suppression gain used in the Wiener filter. The gain is based on a mix of far
    // end energy and echo estimation error.
    supGain = SUPGAIN_DEFAULT;
    // Adjust for the far end signal level. A low signal level indicates no far end signal,
    // hence we set the suppression gain to 0
    if (!aecm->currentVADValue)
    {
        supGain = 0;
    } else
    {
        // Adjust for possible double talk. If we have large variations in estimation error we
        // likely have double talk (or poor channel).
        tmp16no1 = (aecm->nearLogEnergy[0] - aecm->echoStoredLogEnergy[0] - ENERGY_DEV_OFFSET);
        dE = WEBRTC_SPL_ABS_W16(tmp16no1);

        if (dE < ENERGY_DEV_TOL)
        {
            // Likely no double talk. The better estimation, the more we can suppress signal.
            // Update counters
            if (dE < SUPGAIN_EPC_DT)
            {
                tmp32no1 = WEBRTC_SPL_MUL_16_16(aecm->supGainErrParamDiffAB, dE);
                tmp32no1 += (SUPGAIN_EPC_DT >> 1);
                tmp16no1 = (WebRtc_Word16)WebRtcSpl_DivW32W16(tmp32no1, SUPGAIN_EPC_DT);
                supGain = aecm->supGainErrParamA - tmp16no1;
            } else
            {
                tmp32no1 = WEBRTC_SPL_MUL_16_16(aecm->supGainErrParamDiffBD,
                                                (ENERGY_DEV_TOL - dE));
                tmp32no1 += ((ENERGY_DEV_TOL - SUPGAIN_EPC_DT) >> 1);
                tmp16no1 = (WebRtc_Word16)WebRtcSpl_DivW32W16(tmp32no1, (ENERGY_DEV_TOL
                        - SUPGAIN_EPC_DT));
                supGain = aecm->supGainErrParamD + tmp16no1;
            }
        } else
        {
            // Likely in double talk. Use default value
            supGain = aecm->supGainErrParamD;
        }
    }

    if (supGain > aecm->supGainOld)
    {
        tmp16no1 = supGain;
    } else
    {
        tmp16no1 = aecm->supGainOld;
    }
    aecm->supGainOld = supGain;
    if (tmp16no1 < aecm->supGain)
    {
        aecm->supGain += (WebRtc_Word16)((tmp16no1 - aecm->supGain) >> 4);
    } else
    {
        aecm->supGain += (WebRtc_Word16)((tmp16no1 - aecm->supGain) >> 4);
    }

    // END: Update suppression gain

    return aecm->supGain;
}

// WebRtcAecm_DelayCompensation(...)
//
// Secondary delay estimation that can be used as a backup or for validation. This function is
// still under construction and not activated in current version.
//
//
// @param  aecm  [i/o]   Handle of the AECM instance.
//
//
void WebRtcAecm_DelayCompensation(AecmCore_t * const aecm)
{
    int i, j;
    WebRtc_Word32 delayMeanEcho[CORR_BUF_LEN];
    WebRtc_Word32 delayMeanNear[CORR_BUF_LEN];
    WebRtc_Word16 sumBitPattern, bitPatternEcho, bitPatternNear, maxPos, maxValue,
            maxValueLeft, maxValueRight;

    // Check delay (calculate the delay offset (if we can)).
    if ((aecm->startupState > 0) & (aecm->delayCount >= CORR_MAX_BUF) & aecm->delayOffsetFlag)
    {
        // Calculate mean values
        for (i = 0; i < CORR_BUF_LEN; i++)
        {
            delayMeanEcho[i] = 0;
            delayMeanNear[i] = 0;
#if (!defined ARM_WINM) && (!defined ARM9E_GCC) && (!defined ANDROID_AECOPT)
            for (j = 0; j < CORR_WIDTH; j++)
            {
                delayMeanEcho[i] += (WebRtc_Word32)aecm->echoStoredLogEnergy[i + j];
                delayMeanNear[i] += (WebRtc_Word32)aecm->nearLogEnergy[i + j];
            }
#else
            for (j = 0; j < CORR_WIDTH -1; )
            {
                delayMeanEcho[i] += (WebRtc_Word32)aecm->echoStoredLogEnergy[i + j];
                delayMeanNear[i] += (WebRtc_Word32)aecm->nearLogEnergy[i + j];
                j++;
                delayMeanEcho[i] += (WebRtc_Word32)aecm->echoStoredLogEnergy[i + j];
                delayMeanNear[i] += (WebRtc_Word32)aecm->nearLogEnergy[i + j];
                j++;
            }
            delayMeanEcho[i] += (WebRtc_Word32)aecm->echoStoredLogEnergy[i + j];
            delayMeanNear[i] += (WebRtc_Word32)aecm->nearLogEnergy[i + j];
#endif
        }
        // Calculate correlation values
        for (i = 0; i < CORR_BUF_LEN; i++)
        {
            sumBitPattern = 0;
#if (!defined ARM_WINM) && (!defined ARM9E_GCC) && (!defined ANDROID_AECOPT)
            for (j = 0; j < CORR_WIDTH; j++)
            {
                bitPatternEcho = (WebRtc_Word16)((WebRtc_Word32)aecm->echoStoredLogEnergy[i
                        + j] * CORR_WIDTH > delayMeanEcho[i]);
                bitPatternNear = (WebRtc_Word16)((WebRtc_Word32)aecm->nearLogEnergy[CORR_MAX
                        + j] * CORR_WIDTH > delayMeanNear[CORR_MAX]);
                sumBitPattern += !(bitPatternEcho ^ bitPatternNear);
            }
#else
            for (j = 0; j < CORR_WIDTH -1; )
            {
                bitPatternEcho = (WebRtc_Word16)((WebRtc_Word32)aecm->echoStoredLogEnergy[i
                    + j] * CORR_WIDTH > delayMeanEcho[i]);
                bitPatternNear = (WebRtc_Word16)((WebRtc_Word32)aecm->nearLogEnergy[CORR_MAX
                    + j] * CORR_WIDTH > delayMeanNear[CORR_MAX]);
                sumBitPattern += !(bitPatternEcho ^ bitPatternNear);
                j++;
                bitPatternEcho = (WebRtc_Word16)((WebRtc_Word32)aecm->echoStoredLogEnergy[i
                    + j] * CORR_WIDTH > delayMeanEcho[i]);
                bitPatternNear = (WebRtc_Word16)((WebRtc_Word32)aecm->nearLogEnergy[CORR_MAX
                    + j] * CORR_WIDTH > delayMeanNear[CORR_MAX]);
                sumBitPattern += !(bitPatternEcho ^ bitPatternNear);
                j++;
            }
            bitPatternEcho = (WebRtc_Word16)((WebRtc_Word32)aecm->echoStoredLogEnergy[i + j]
                    * CORR_WIDTH > delayMeanEcho[i]);
            bitPatternNear = (WebRtc_Word16)((WebRtc_Word32)aecm->nearLogEnergy[CORR_MAX + j]
                    * CORR_WIDTH > delayMeanNear[CORR_MAX]);
            sumBitPattern += !(bitPatternEcho ^ bitPatternNear);
#endif
            aecm->delayCorrelation[i] = sumBitPattern;
        }
        aecm->newDelayCorrData = 1; // Indicate we have new correlation data to evaluate
    }
    if ((aecm->startupState == 2) & (aecm->lastDelayUpdateCount > (CORR_WIDTH << 1))
            & aecm->newDelayCorrData)
    {
        // Find maximum value and maximum position as well as values on the sides.
        maxPos = 0;
        maxValue = aecm->delayCorrelation[0];
        maxValueLeft = maxValue;
        maxValueRight = aecm->delayCorrelation[CORR_DEV];
        for (i = 1; i < CORR_BUF_LEN; i++)
        {
            if (aecm->delayCorrelation[i] > maxValue)
            {
                maxValue = aecm->delayCorrelation[i];
                maxPos = i;
                if (maxPos < CORR_DEV)
                {
                    maxValueLeft = aecm->delayCorrelation[0];
                    maxValueRight = aecm->delayCorrelation[i + CORR_DEV];
                } else if (maxPos > (CORR_MAX << 1) - CORR_DEV)
                {
                    maxValueLeft = aecm->delayCorrelation[i - CORR_DEV];
                    maxValueRight = aecm->delayCorrelation[(CORR_MAX << 1)];
                } else
                {
                    maxValueLeft = aecm->delayCorrelation[i - CORR_DEV];
                    maxValueRight = aecm->delayCorrelation[i + CORR_DEV];
                }
            }
        }
        if ((maxPos > 0) & (maxPos < (CORR_MAX << 1)))
        {
            // Avoid maximum at boundaries. The maximum peak has to be higher than
            // CORR_MAX_LEVEL. It also has to be sharp, i.e. the value CORR_DEV bins off should
            // be CORR_MAX_LOW lower than the maximum.
            if ((maxValue > CORR_MAX_LEVEL) & (maxValueLeft < maxValue - CORR_MAX_LOW)
                    & (maxValueRight < maxValue - CORR_MAX_LOW))
            {
                aecm->delayAdjust += CORR_MAX - maxPos;
                aecm->newDelayCorrData = 0;
                aecm->lastDelayUpdateCount = 0;
            }
        }
    }
    // END: "Check delay"
}

void WebRtcAecm_ProcessBlock(AecmCore_t * const aecm, const WebRtc_Word16 * const farend,
                             const WebRtc_Word16 * const nearendNoisy,
                             const WebRtc_Word16 * const nearendClean,
                             WebRtc_Word16 * const output)
{
    int i, j;

    WebRtc_UWord32 xfaSum;
    WebRtc_UWord32 dfaNoisySum;
    WebRtc_UWord32 echoEst32Gained;
    WebRtc_UWord32 tmpU32;

    WebRtc_Word32 tmp32no1;
    WebRtc_Word32 tmp32no2;
    WebRtc_Word32 echoEst32[PART_LEN1];

    WebRtc_UWord16 xfa[PART_LEN1];
    WebRtc_UWord16 dfaNoisy[PART_LEN1];
    WebRtc_UWord16 dfaClean[PART_LEN1];
    WebRtc_UWord16* ptrDfaClean = dfaClean;

    int outCFFT;

    WebRtc_Word16 fft[PART_LEN4];
#if (defined ARM_WINM) || (defined ARM9E_GCC) || (defined ANDROID_AECOPT)
    WebRtc_Word16 postFft[PART_LEN4];
#else
    WebRtc_Word16 postFft[PART_LEN2];
#endif
    WebRtc_Word16 dfwReal[PART_LEN1];
    WebRtc_Word16 dfwImag[PART_LEN1];
    WebRtc_Word16 xfwReal[PART_LEN1];
    WebRtc_Word16 xfwImag[PART_LEN1];
    WebRtc_Word16 efwReal[PART_LEN1];
    WebRtc_Word16 efwImag[PART_LEN1];
    WebRtc_Word16 hnl[PART_LEN1];
    WebRtc_Word16 numPosCoef;
    WebRtc_Word16 nlpGain;
    WebRtc_Word16 delay, diff, diffMinusOne;
    WebRtc_Word16 tmp16no1;
    WebRtc_Word16 tmp16no2;
#ifdef AECM_WITH_ABS_APPROX
    WebRtc_Word16 maxValue;
    WebRtc_Word16 minValue;
#endif
    WebRtc_Word16 mu;
    WebRtc_Word16 supGain;
    WebRtc_Word16 zeros32, zeros16;
    WebRtc_Word16 zerosDBufNoisy, zerosDBufClean, zerosXBuf;
    WebRtc_Word16 resolutionDiff, qDomainDiff;

#ifdef ARM_WINM_LOG_
    DWORD temp;
    static int flag0 = 0;
    __int64 freq, start, end, diff__;
    unsigned int milliseconds;
#endif

#ifdef AECM_WITH_ABS_APPROX
    WebRtc_UWord16 alpha, beta;
#endif

    // Determine startup state. There are three states:
    // (0) the first CONV_LEN blocks
    // (1) another CONV_LEN blocks
    // (2) the rest

    if (aecm->startupState < 2)
    {
        aecm->startupState = (aecm->totCount >= CONV_LEN) + (aecm->totCount >= CONV_LEN2);
    }
    // END: Determine startup state

    // Buffer near and far end signals
    memcpy(aecm->xBuf + PART_LEN, farend, sizeof(WebRtc_Word16) * PART_LEN);
    memcpy(aecm->dBufNoisy + PART_LEN, nearendNoisy, sizeof(WebRtc_Word16) * PART_LEN);
    if (nearendClean != NULL)
    {
        memcpy(aecm->dBufClean + PART_LEN, nearendClean, sizeof(WebRtc_Word16) * PART_LEN);
    }
    // TODO(bjornv): Will be removed in final version.
#ifdef VAD_DATA
    fwrite(aecm->xBuf, sizeof(WebRtc_Word16), PART_LEN, aecm->far_file);
#endif

#ifdef AECM_DYNAMIC_Q
    tmp16no1 = WebRtcSpl_MaxAbsValueW16(aecm->dBufNoisy, PART_LEN2);
    tmp16no2 = WebRtcSpl_MaxAbsValueW16(aecm->xBuf, PART_LEN2);
    zerosDBufNoisy = WebRtcSpl_NormW16(tmp16no1);
    zerosXBuf = WebRtcSpl_NormW16(tmp16no2);
#else
    zerosDBufNoisy = 0;
    zerosXBuf = 0;
#endif
    aecm->dfaNoisyQDomainOld = aecm->dfaNoisyQDomain;
    aecm->dfaNoisyQDomain = zerosDBufNoisy;

    if (nearendClean != NULL)
    {
#ifdef AECM_DYNAMIC_Q
        tmp16no1 = WebRtcSpl_MaxAbsValueW16(aecm->dBufClean, PART_LEN2);
        zerosDBufClean = WebRtcSpl_NormW16(tmp16no1);
#else
        zerosDBufClean = 0;
#endif
        aecm->dfaCleanQDomainOld = aecm->dfaCleanQDomain;
        aecm->dfaCleanQDomain = zerosDBufClean;
    } else
    {
        zerosDBufClean = zerosDBufNoisy;
        aecm->dfaCleanQDomainOld = aecm->dfaNoisyQDomainOld;
        aecm->dfaCleanQDomain = aecm->dfaNoisyQDomain;
    }

#ifdef ARM_WINM_LOG_
    // measure tick start
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    QueryPerformanceCounter((LARGE_INTEGER*)&start);
#endif

    // FFT of noisy near end signal
    for (i = 0; i < PART_LEN; i++)
    {
        j = WEBRTC_SPL_LSHIFT_W32(i, 1);
        // Window near end
        fft[j] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT((aecm->dBufNoisy[i]
                        << zerosDBufNoisy), kSqrtHanning[i], 14);
        fft[PART_LEN2 + j] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(
                (aecm->dBufNoisy[PART_LEN + i] << zerosDBufNoisy),
                kSqrtHanning[PART_LEN - i], 14);
        // Inserting zeros in imaginary parts
        fft[j + 1] = 0;
        fft[PART_LEN2 + j + 1] = 0;
    }

    // Fourier transformation of near end signal.
    // The result is scaled with 1/PART_LEN2, that is, the result is in Q(-6) for PART_LEN = 32

#if (defined ARM_WINM) || (defined ARM9E_GCC) || (defined ANDROID_AECOPT)
    outCFFT = WebRtcSpl_ComplexFFT2(fft, postFft, PART_LEN_SHIFT, 1);

    // The imaginary part has to switch sign
    for(i = 1; i < PART_LEN2-1;)
    {
        postFft[i] = -postFft[i];
        i += 2;
        postFft[i] = -postFft[i];
        i += 2;
    }
#else
    WebRtcSpl_ComplexBitReverse(fft, PART_LEN_SHIFT);
    outCFFT = WebRtcSpl_ComplexFFT(fft, PART_LEN_SHIFT, 1);

    // Take only the first PART_LEN2 samples
    for (i = 0; i < PART_LEN2; i++)
    {
        postFft[i] = fft[i];
    }
    // The imaginary part has to switch sign
    for (i = 1; i < PART_LEN2;)
    {
        postFft[i] = -postFft[i];
        i += 2;
    }
#endif

    // Extract imaginary and real part, calculate the magnitude for all frequency bins
    dfwImag[0] = 0;
    dfwImag[PART_LEN] = 0;
    dfwReal[0] = postFft[0];
#if (defined ARM_WINM) || (defined ARM9E_GCC) || (defined ANDROID_AECOPT)
    dfwReal[PART_LEN] = postFft[PART_LEN2];
#else
    dfwReal[PART_LEN] = fft[PART_LEN2];
#endif
    dfaNoisy[0] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(dfwReal[0]);
    dfaNoisy[PART_LEN] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(dfwReal[PART_LEN]);
    dfaNoisySum = (WebRtc_UWord32)(dfaNoisy[0]);
    dfaNoisySum += (WebRtc_UWord32)(dfaNoisy[PART_LEN]);

    for (i = 1; i < PART_LEN; i++)
    {
        j = WEBRTC_SPL_LSHIFT_W32(i, 1);
        dfwReal[i] = postFft[j];
        dfwImag[i] = postFft[j + 1];

        if (dfwReal[i] == 0 || dfwImag[i] == 0)
        {
            dfaNoisy[i] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(dfwReal[i] + dfwImag[i]);
        } else
        {
            // Approximation for magnitude of complex fft output
            // magn = sqrt(real^2 + imag^2)
            // magn ~= alpha * max(|imag|,|real|) + beta * min(|imag|,|real|)
            //
            // The parameters alpha and beta are stored in Q15

            tmp16no1 = WEBRTC_SPL_ABS_W16(postFft[j]);
            tmp16no2 = WEBRTC_SPL_ABS_W16(postFft[j + 1]);

#ifdef AECM_WITH_ABS_APPROX
            if(tmp16no1 > tmp16no2)
            {
                maxValue = tmp16no1;
                minValue = tmp16no2;
            } else
            {
                maxValue = tmp16no2;
                minValue = tmp16no1;
            }

            // Magnitude in Q-6
            if ((maxValue >> 2) > minValue)
            {
                alpha = kAlpha1;
                beta = kBeta1;
            } else if ((maxValue >> 1) > minValue)
            {
                alpha = kAlpha2;
                beta = kBeta2;
            } else
            {
                alpha = kAlpha3;
                beta = kBeta3;
            }
            tmp16no1 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(maxValue, alpha, 15);
            tmp16no2 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(minValue, beta, 15);
            dfaNoisy[i] = (WebRtc_UWord16)tmp16no1 + (WebRtc_UWord16)tmp16no2;
#else
            tmp32no1 = WEBRTC_SPL_MUL_16_16(tmp16no1, tmp16no1);
            tmp32no2 = WEBRTC_SPL_MUL_16_16(tmp16no2, tmp16no2);
            tmp32no2 = WEBRTC_SPL_ADD_SAT_W32(tmp32no1, tmp32no2);
            tmp32no1 = WebRtcSpl_Sqrt(tmp32no2);
            dfaNoisy[i] = (WebRtc_UWord16)tmp32no1;
#endif
        }
        dfaNoisySum += (WebRtc_UWord32)dfaNoisy[i];
    }
    // END: FFT of noisy near end signal

    if (nearendClean == NULL)
    {
        ptrDfaClean = dfaNoisy;
    } else
    {
        // FFT of clean near end signal
        for (i = 0; i < PART_LEN; i++)
        {
            j = WEBRTC_SPL_LSHIFT_W32(i, 1);
            // Window near end
            fft[j]
                    = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT((aecm->dBufClean[i] << zerosDBufClean), kSqrtHanning[i], 14);
            fft[PART_LEN2 + j]
                    = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT((aecm->dBufClean[PART_LEN + i] << zerosDBufClean), kSqrtHanning[PART_LEN - i], 14);
            // Inserting zeros in imaginary parts
            fft[j + 1] = 0;
            fft[PART_LEN2 + j + 1] = 0;
        }

        // Fourier transformation of near end signal.
        // The result is scaled with 1/PART_LEN2, that is, in Q(-6) for PART_LEN = 32

#if (defined ARM_WINM) || (defined ARM9E_GCC) || (defined ANDROID_AECOPT)
        outCFFT = WebRtcSpl_ComplexFFT2(fft, postFft, PART_LEN_SHIFT, 1);

        // The imaginary part has to switch sign
        for(i = 1; i < PART_LEN2-1;)
        {
            postFft[i] = -postFft[i];
            i += 2;
            postFft[i] = -postFft[i];
            i += 2;
        }
#else
        WebRtcSpl_ComplexBitReverse(fft, PART_LEN_SHIFT);
        outCFFT = WebRtcSpl_ComplexFFT(fft, PART_LEN_SHIFT, 1);

        // Take only the first PART_LEN2 samples
        for (i = 0; i < PART_LEN2; i++)
        {
            postFft[i] = fft[i];
        }
        // The imaginary part has to switch sign
        for (i = 1; i < PART_LEN2;)
        {
            postFft[i] = -postFft[i];
            i += 2;
        }
#endif

        // Extract imaginary and real part, calculate the magnitude for all frequency bins
        dfwImag[0] = 0;
        dfwImag[PART_LEN] = 0;
        dfwReal[0] = postFft[0];
#if (defined ARM_WINM) || (defined ARM9E_GCC) || (defined ANDROID_AECOPT)
        dfwReal[PART_LEN] = postFft[PART_LEN2];
#else
        dfwReal[PART_LEN] = fft[PART_LEN2];
#endif
        dfaClean[0] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(dfwReal[0]);
        dfaClean[PART_LEN] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(dfwReal[PART_LEN]);

        for (i = 1; i < PART_LEN; i++)
        {
            j = WEBRTC_SPL_LSHIFT_W32(i, 1);
            dfwReal[i] = postFft[j];
            dfwImag[i] = postFft[j + 1];

            if (dfwReal[i] == 0 || dfwImag[i] == 0)
            {
                dfaClean[i] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(dfwReal[i] + dfwImag[i]);
            } else
            {
                // Approximation for magnitude of complex fft output
                // magn = sqrt(real^2 + imag^2)
                // magn ~= alpha * max(|imag|,|real|) + beta * min(|imag|,|real|)
                //
                // The parameters alpha and beta are stored in Q15

                tmp16no1 = WEBRTC_SPL_ABS_W16(postFft[j]);
                tmp16no2 = WEBRTC_SPL_ABS_W16(postFft[j + 1]);

#ifdef AECM_WITH_ABS_APPROX
                if(tmp16no1 > tmp16no2)
                {
                    maxValue = tmp16no1;
                    minValue = tmp16no2;
                } else
                {
                    maxValue = tmp16no2;
                    minValue = tmp16no1;
                }

                // Magnitude in Q-6
                if ((maxValue >> 2) > minValue)
                {
                    alpha = kAlpha1;
                    beta = kBeta1;
                } else if ((maxValue >> 1) > minValue)
                {
                    alpha = kAlpha2;
                    beta = kBeta2;
                } else
                {
                    alpha = kAlpha3;
                    beta = kBeta3;
                }
                tmp16no1 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(maxValue, alpha, 15);
                tmp16no2 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(minValue, beta, 15);
                dfaClean[i] = (WebRtc_UWord16)tmp16no1 + (WebRtc_UWord16)tmp16no2;
#else
                tmp32no1 = WEBRTC_SPL_MUL_16_16(tmp16no1, tmp16no1);
                tmp32no2 = WEBRTC_SPL_MUL_16_16(tmp16no2, tmp16no2);
                tmp32no2 = WEBRTC_SPL_ADD_SAT_W32(tmp32no1, tmp32no2);
                tmp32no1 = WebRtcSpl_Sqrt(tmp32no2);
                dfaClean[i] = (WebRtc_UWord16)tmp32no1;
#endif
            }
        }
    }
    // END: FFT of clean near end signal

    // FFT of far end signal
    for (i = 0; i < PART_LEN; i++)
    {
        j = WEBRTC_SPL_LSHIFT_W32(i, 1);
        // Window farend
        fft[j]
                = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT((aecm->xBuf[i] << zerosXBuf), kSqrtHanning[i], 14);
        fft[PART_LEN2 + j]
                = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT((aecm->xBuf[PART_LEN + i] << zerosXBuf), kSqrtHanning[PART_LEN - i], 14);
        // Inserting zeros in imaginary parts
        fft[j + 1] = 0;
        fft[PART_LEN2 + j + 1] = 0;
    }
    // Fourier transformation of far end signal.
    // The result is scaled with 1/PART_LEN2, that is the result is in Q(-6) for PART_LEN = 32
#if (defined ARM_WINM) || (defined ARM9E_GCC) || (defined ANDROID_AECOPT)
    outCFFT = WebRtcSpl_ComplexFFT2(fft, postFft, PART_LEN_SHIFT, 1);

    // The imaginary part has to switch sign
    for(i = 1; i < PART_LEN2-1;)
    {
        postFft[i] = -postFft[i];
        i += 2;
        postFft[i] = -postFft[i];
        i += 2;
    }
#else
    WebRtcSpl_ComplexBitReverse(fft, PART_LEN_SHIFT);
    outCFFT = WebRtcSpl_ComplexFFT(fft, PART_LEN_SHIFT, 1);

    // Take only the first PART_LEN2 samples
    for (i = 0; i < PART_LEN2; i++)
    {
        postFft[i] = fft[i];
    }
    // The imaginary part has to switch sign
    for (i = 1; i < PART_LEN2;)
    {
        postFft[i] = -postFft[i];
        i += 2;
    }
#endif

    // Extract imaginary and real part, calculate the magnitude for all frequency bins
    xfwImag[0] = 0;
    xfwImag[PART_LEN] = 0;
    xfwReal[0] = postFft[0];
#if (defined ARM_WINM) || (defined ARM9E_GCC) || (defined ANDROID_AECOPT)
    xfwReal[PART_LEN] = postFft[PART_LEN2];
#else
    xfwReal[PART_LEN] = fft[PART_LEN2];
#endif
    xfa[0] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(xfwReal[0]);
    xfa[PART_LEN] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(xfwReal[PART_LEN]);
    xfaSum = (WebRtc_UWord32)(xfa[0]) + (WebRtc_UWord32)(xfa[PART_LEN]);

    for (i = 1; i < PART_LEN; i++)
    {
        j = WEBRTC_SPL_LSHIFT_W32(i,1);
        xfwReal[i] = postFft[j];
        xfwImag[i] = postFft[j + 1];

        if (xfwReal[i] == 0 || xfwImag[i] == 0)
        {
            xfa[i] = (WebRtc_UWord16)WEBRTC_SPL_ABS_W16(xfwReal[i] + xfwImag[i]);
        } else
        {
            // Approximation for magnitude of complex fft output
            // magn = sqrt(real^2 + imag^2)
            // magn ~= alpha * max(|imag|,|real|) + beta * min(|imag|,|real|)
            //
            // The parameters alpha and beta are stored in Q15

            tmp16no1 = WEBRTC_SPL_ABS_W16(postFft[j]);
            tmp16no2 = WEBRTC_SPL_ABS_W16(postFft[j + 1]);

#ifdef AECM_WITH_ABS_APPROX
            if(tmp16no1 > xfwImag[i])
            {
                maxValue = tmp16no1;
                minValue = tmp16no2;
            } else
            {
                maxValue = tmp16no2;
                minValue = tmp16no1;
            }
            // Magnitude in Q-6
            if ((maxValue >> 2) > minValue)
            {
                alpha = kAlpha1;
                beta = kBeta1;
            } else if ((maxValue >> 1) > minValue)
            {
                alpha = kAlpha2;
                beta = kBeta2;
            } else
            {
                alpha = kAlpha3;
                beta = kBeta3;
            }
            tmp16no1 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(maxValue, alpha, 15);
            tmp16no2 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(minValue, beta, 15);
            xfa[i] = (WebRtc_UWord16)tmp16no1 + (WebRtc_UWord16)tmp16no2;
#else
            tmp32no1 = WEBRTC_SPL_MUL_16_16(tmp16no1, tmp16no1);
            tmp32no2 = WEBRTC_SPL_MUL_16_16(tmp16no2, tmp16no2);
            tmp32no2 = WEBRTC_SPL_ADD_SAT_W32(tmp32no1, tmp32no2);
            tmp32no1 = WebRtcSpl_Sqrt(tmp32no2);
            xfa[i] = (WebRtc_UWord16)tmp32no1;
#endif
        }
        xfaSum += (WebRtc_UWord32)xfa[i];
    }

#ifdef ARM_WINM_LOG_
    // measure tick end
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    diff__ = ((end - start) * 1000) / (freq/1000);
    milliseconds = (unsigned int)(diff__ & 0xffffffff);
    WriteFile (logFile, &milliseconds, sizeof(unsigned int), &temp, NULL);
#endif
    // END: FFT of far end signal

    // Get the delay

    // Fixed delay estimation
    // input: dfaFIX, xfaFIX in Q-stages
    // output: delay in Q0
    //
    // comment on the fixed point accuracy of estimate_delayFIX
    // -> due to rounding the fixed point variables xfa and dfa contain a lot more zeros
    // than the corresponding floating point variables this results in big differences
    // between the floating point and the fixed point logarithmic spectra for small values
#ifdef ARM_WINM_LOG_
    // measure tick start
    QueryPerformanceCounter((LARGE_INTEGER*)&start);
#endif

    // Save far-end history and estimate delay
    delay = WebRtcAecm_EstimateDelay(aecm, xfa, dfaNoisy, zerosXBuf);

    if (aecm->fixedDelay >= 0)
    {
        // Use fixed delay
        delay = aecm->fixedDelay;
    }

    aecm->currentDelay = delay;

    if ((aecm->delayOffsetFlag) & (aecm->startupState > 0)) // If delay compensation is on
    {
        // If the delay estimate changed from previous block, update the offset
        if ((aecm->currentDelay != aecm->previousDelay) & !aecm->currentDelay
                & !aecm->previousDelay)
        {
            aecm->delayAdjust += (aecm->currentDelay - aecm->previousDelay);
        }
        // Compensate with the offset estimate
        aecm->currentDelay -= aecm->delayAdjust;
        aecm->previousDelay = delay;
    }

    diff = aecm->delHistoryPos - aecm->currentDelay;
    if (diff < 0)
    {
        diff = diff + MAX_DELAY;
    }

#ifdef ARM_WINM_LOG_
    // measure tick end
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    diff__ = ((end - start) * 1000) / (freq/1000);
    milliseconds = (unsigned int)(diff__ & 0xffffffff);
    WriteFile (logFile, &milliseconds, sizeof(unsigned int), &temp, NULL);
#endif

    // END: Get the delay

#ifdef ARM_WINM_LOG_
    // measure tick start
    QueryPerformanceCounter((LARGE_INTEGER*)&start);
#endif
    // Calculate log(energy) and update energy threshold levels
    WebRtcAecm_CalcEnergies(aecm, diff, dfaNoisySum, echoEst32);

    // Calculate stepsize
    mu = WebRtcAecm_CalcStepSize(aecm);

    // Update counters
    aecm->totCount++;
    aecm->lastDelayUpdateCount++;

    // This is the channel estimation algorithm.
    // It is base on NLMS but has a variable step length, which was calculated above.
    WebRtcAecm_UpdateChannel(aecm, dfaNoisy, diff, mu, echoEst32);
    WebRtcAecm_DelayCompensation(aecm);
    supGain = WebRtcAecm_CalcSuppressionGain(aecm);

#ifdef ARM_WINM_LOG_
    // measure tick end
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    diff__ = ((end - start) * 1000) / (freq/1000);
    milliseconds = (unsigned int)(diff__ & 0xffffffff);
    WriteFile (logFile, &milliseconds, sizeof(unsigned int), &temp, NULL);
#endif

#ifdef ARM_WINM_LOG_
    // measure tick start
    QueryPerformanceCounter((LARGE_INTEGER*)&start);
#endif

    // Calculate Wiener filter hnl[]
    numPosCoef = 0;
    diffMinusOne = diff - 1;
    if (diff == 0)
    {
        diffMinusOne = MAX_DELAY;
    }
    for (i = 0; i < PART_LEN1; i++)
    {
        // Far end signal through channel estimate in Q8
        // How much can we shift right to preserve resolution
        tmp32no1 = echoEst32[i] - aecm->echoFilt[i];
        aecm->echoFilt[i] += WEBRTC_SPL_RSHIFT_W32(WEBRTC_SPL_MUL_32_16(tmp32no1, 50), 8);

        zeros32 = WebRtcSpl_NormW32(aecm->echoFilt[i]) + 1;
        zeros16 = WebRtcSpl_NormW16(supGain) + 1;
        if (zeros32 + zeros16 > 16)
        {
            // Multiplication is safe
            // Result in Q(RESOLUTION_CHANNEL+RESOLUTION_SUPGAIN+aecm->xfaQDomainBuf[diff])
            echoEst32Gained = WEBRTC_SPL_UMUL_32_16((WebRtc_UWord32)aecm->echoFilt[i],
                                                    (WebRtc_UWord16)supGain);
            resolutionDiff = 14 - RESOLUTION_CHANNEL16 - RESOLUTION_SUPGAIN;
            resolutionDiff += (aecm->dfaCleanQDomain - aecm->xfaQDomainBuf[diff]);
        } else
        {
            tmp16no1 = 17 - zeros32 - zeros16;
            resolutionDiff = 14 + tmp16no1 - RESOLUTION_CHANNEL16 - RESOLUTION_SUPGAIN;
            resolutionDiff += (aecm->dfaCleanQDomain - aecm->xfaQDomainBuf[diff]);
            if (zeros32 > tmp16no1)
            {
                echoEst32Gained = WEBRTC_SPL_UMUL_32_16((WebRtc_UWord32)aecm->echoFilt[i],
                        (WebRtc_UWord16)WEBRTC_SPL_RSHIFT_W16(supGain,
                                tmp16no1)); // Q-(RESOLUTION_CHANNEL+RESOLUTION_SUPGAIN-16)
            } else
            {
                // Result in Q-(RESOLUTION_CHANNEL+RESOLUTION_SUPGAIN-16)
                echoEst32Gained = WEBRTC_SPL_UMUL_32_16(
                        (WebRtc_UWord32)WEBRTC_SPL_RSHIFT_W32(aecm->echoFilt[i], tmp16no1),
                        (WebRtc_UWord16)supGain);
            }
        }

        zeros16 = WebRtcSpl_NormW16(aecm->nearFilt[i]);
        if ((zeros16 < (aecm->dfaCleanQDomain - aecm->dfaCleanQDomainOld))
                & (aecm->nearFilt[i]))
        {
            tmp16no1 = WEBRTC_SPL_SHIFT_W16(aecm->nearFilt[i], zeros16);
            qDomainDiff = zeros16 - aecm->dfaCleanQDomain + aecm->dfaCleanQDomainOld;
        } else
        {
            tmp16no1 = WEBRTC_SPL_SHIFT_W16(aecm->nearFilt[i], aecm->dfaCleanQDomain
                                            - aecm->dfaCleanQDomainOld);
            qDomainDiff = 0;
        }
        tmp16no2 = WEBRTC_SPL_SHIFT_W16(ptrDfaClean[i], qDomainDiff);
        tmp16no2 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmp16no2 - tmp16no1, 1, 4);
        tmp16no2 += tmp16no1;
        zeros16 = WebRtcSpl_NormW16(tmp16no2);
        if ((tmp16no2) & (-qDomainDiff > zeros16))
        {
            aecm->nearFilt[i] = WEBRTC_SPL_WORD16_MAX;
        } else
        {
            aecm->nearFilt[i] = WEBRTC_SPL_SHIFT_W16(tmp16no2, -qDomainDiff);
        }

        // Wiener filter coefficients, resulting hnl in Q14
        if (echoEst32Gained == 0)
        {
            hnl[i] = ONE_Q14;
        } else if (aecm->nearFilt[i] == 0)
        {
            hnl[i] = 0;
        } else
        {
            // Multiply the suppression gain
            // Rounding
            echoEst32Gained += (WebRtc_UWord32)(aecm->nearFilt[i] >> 1);
            tmpU32 = WebRtcSpl_DivU32U16(echoEst32Gained, (WebRtc_UWord16)aecm->nearFilt[i]);

            // Current resolution is
            // Q-(RESOLUTION_CHANNEL + RESOLUTION_SUPGAIN - max(0, 17 - zeros16 - zeros32))
            // Make sure we are in Q14
            tmp32no1 = (WebRtc_Word32)WEBRTC_SPL_SHIFT_W32(tmpU32, resolutionDiff);
            if (tmp32no1 > ONE_Q14)
            {
                hnl[i] = 0;
            } else if (tmp32no1 < 0)
            {
                hnl[i] = ONE_Q14;
            } else
            {
                // 1-echoEst/dfa
#if (!defined ARM_WINM) && (!defined ARM9E_GCC) && (!defined ANDROID_AECOPT)
                hnl[i] = ONE_Q14 - (WebRtc_Word16)tmp32no1;
                if (hnl[i] < 0)
                {
                    hnl[i] = 0;
                }
#else
                hnl[i] = ((ONE_Q14 - (WebRtc_Word16)tmp32no1) > 0) ? (ONE_Q14 - (WebRtc_Word16)tmp32no1) : 0;
#endif
            }
        }
        if (hnl[i])
        {
            numPosCoef++;
        }
    }

#ifdef ARM_WINM_LOG_
    // measure tick end
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    diff__ = ((end - start) * 1000) / (freq/1000);
    milliseconds = (unsigned int)(diff__ & 0xffffffff);
    WriteFile (logFile, &milliseconds, sizeof(unsigned int), &temp, NULL);
#endif

#ifdef ARM_WINM_LOG_
    // measure tick start
    QueryPerformanceCounter((LARGE_INTEGER*)&start);
#endif

    // Calculate NLP gain, result is in Q14
    for (i = 0; i < PART_LEN1; i++)
    {
        if (aecm->nlpFlag)
        {
            // Truncate values close to zero and one.
            if (hnl[i] > NLP_COMP_HIGH)
            {
                hnl[i] = ONE_Q14;
            } else if (hnl[i] < NLP_COMP_LOW)
            {
                hnl[i] = 0;
            }

            // Remove outliers
            if (numPosCoef < 3)
            {
                nlpGain = 0;
            } else
            {
                nlpGain = ONE_Q14;
            }
            // NLP
            if ((hnl[i] == ONE_Q14) && (nlpGain == ONE_Q14))
            {
                hnl[i] = ONE_Q14;
            } else
            {
                hnl[i] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(hnl[i], nlpGain, 14);
            }
        }

        // multiply with Wiener coefficients
        efwReal[i] = (WebRtc_Word16)(WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(dfwReal[i], hnl[i],
                                                                          14));
        efwImag[i] = (WebRtc_Word16)(WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(dfwImag[i], hnl[i],
                                                                          14));
    }

    if (aecm->cngMode == AecmTrue)
    {
        WebRtcAecm_ComfortNoise(aecm, ptrDfaClean, efwReal, efwImag, hnl);
    }

#ifdef ARM_WINM_LOG_
    // measure tick end
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    diff__ = ((end - start) * 1000) / (freq/1000);
    milliseconds = (unsigned int)(diff__ & 0xffffffff);
    WriteFile (logFile, &milliseconds, sizeof(unsigned int), &temp, NULL);
#endif

#ifdef ARM_WINM_LOG_
    // measure tick start
    QueryPerformanceCounter((LARGE_INTEGER*)&start);
#endif

    // Synthesis
    for (i = 1; i < PART_LEN; i++)
    {
        j = WEBRTC_SPL_LSHIFT_W32(i, 1);
        fft[j] = efwReal[i];

        // mirrored data, even
        fft[PART_LEN4 - j] = efwReal[i];
        fft[j + 1] = -efwImag[i];

        //mirrored data, odd
        fft[PART_LEN4 - (j - 1)] = efwImag[i];
    }
    fft[0] = efwReal[0];
    fft[1] = -efwImag[0];

    fft[PART_LEN2] = efwReal[PART_LEN];
    fft[PART_LEN2 + 1] = -efwImag[PART_LEN];

#if (!defined ARM_WINM) && (!defined ARM9E_GCC) && (!defined ANDROID_AECOPT)
    // inverse FFT, result should be scaled with outCFFT
    WebRtcSpl_ComplexBitReverse(fft, PART_LEN_SHIFT);
    outCFFT = WebRtcSpl_ComplexIFFT(fft, PART_LEN_SHIFT, 1);

    //take only the real values and scale with outCFFT
    for (i = 0; i < PART_LEN2; i++)
    {
        j = WEBRTC_SPL_LSHIFT_W32(i, 1);
        fft[i] = fft[j];
    }
#else
    outCFFT = WebRtcSpl_ComplexIFFT2(fft, postFft, PART_LEN_SHIFT, 1);

    //take only the real values and scale with outCFFT
    for(i = 0, j = 0; i < PART_LEN2;)
    {
        fft[i] = postFft[j];
        i += 1;
        j += 2;
        fft[i] = postFft[j];
        i += 1;
        j += 2;
    }
#endif

    for (i = 0; i < PART_LEN; i++)
    {
        fft[i] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(
                fft[i],
                kSqrtHanning[i],
                14);
        tmp32no1 = WEBRTC_SPL_SHIFT_W32((WebRtc_Word32)fft[i],
                outCFFT - aecm->dfaCleanQDomain);
        fft[i] = (WebRtc_Word16)WEBRTC_SPL_SAT(WEBRTC_SPL_WORD16_MAX,
                tmp32no1 + aecm->outBuf[i],
                WEBRTC_SPL_WORD16_MIN);
        output[i] = fft[i];

        tmp32no1 = WEBRTC_SPL_MUL_16_16_RSFT(
                fft[PART_LEN + i],
                kSqrtHanning[PART_LEN - i],
                14);
        tmp32no1 = WEBRTC_SPL_SHIFT_W32(tmp32no1,
                outCFFT - aecm->dfaCleanQDomain);
        aecm->outBuf[i] = (WebRtc_Word16)WEBRTC_SPL_SAT(
                WEBRTC_SPL_WORD16_MAX,
                tmp32no1,
                WEBRTC_SPL_WORD16_MIN);
    }

#ifdef ARM_WINM_LOG_
    // measure tick end
    QueryPerformanceCounter((LARGE_INTEGER*)&end);
    diff__ = ((end - start) * 1000) / (freq/1000);
    milliseconds = (unsigned int)(diff__ & 0xffffffff);
    WriteFile (logFile, &milliseconds, sizeof(unsigned int), &temp, NULL);
#endif
    // Copy the current block to the old position (outBuf is shifted elsewhere)
    memcpy(aecm->xBuf, aecm->xBuf + PART_LEN, sizeof(WebRtc_Word16) * PART_LEN);
    memcpy(aecm->dBufNoisy, aecm->dBufNoisy + PART_LEN, sizeof(WebRtc_Word16) * PART_LEN);
    if (nearendClean != NULL)
    {
        memcpy(aecm->dBufClean, aecm->dBufClean + PART_LEN, sizeof(WebRtc_Word16) * PART_LEN);
    }
}

// Generate comfort noise and add to output signal.
//
// \param[in]     aecm     Handle of the AECM instance.
// \param[in]     dfa     Absolute value of the nearend signal (Q[aecm->dfaQDomain]).
// \param[in,out] outReal Real part of the output signal (Q[aecm->dfaQDomain]).
// \param[in,out] outImag Imaginary part of the output signal (Q[aecm->dfaQDomain]).
// \param[in]     lambda  Suppression gain with which to scale the noise level (Q14).
//
static void WebRtcAecm_ComfortNoise(AecmCore_t * const aecm, const WebRtc_UWord16 * const dfa,
                                    WebRtc_Word16 * const outReal,
                                    WebRtc_Word16 * const outImag,
                                    const WebRtc_Word16 * const lambda)
{
    WebRtc_Word16 i;
    WebRtc_Word16 tmp16;
    WebRtc_Word32 tmp32;

    WebRtc_Word16 randW16[PART_LEN];
    WebRtc_Word16 uReal[PART_LEN1];
    WebRtc_Word16 uImag[PART_LEN1];
    WebRtc_Word32 outLShift32[PART_LEN1];
    WebRtc_Word16 noiseRShift16[PART_LEN1];

    WebRtc_Word16 shiftFromNearToNoise[PART_LEN1];
    WebRtc_Word16 minTrackShift;
    WebRtc_Word32 upper32;
    WebRtc_Word32 lower32;

    if (aecm->noiseEstCtr < 100)
    {
        // Track the minimum more quickly initially.
        aecm->noiseEstCtr++;
        minTrackShift = 7;
    } else
    {
        minTrackShift = 9;
    }

    // Estimate noise power.
    for (i = 0; i < PART_LEN1; i++)
    {
        shiftFromNearToNoise[i] = aecm->noiseEstQDomain[i] - aecm->dfaCleanQDomain;

        // Shift to the noise domain.
        tmp32 = (WebRtc_Word32)dfa[i];
        outLShift32[i] = WEBRTC_SPL_SHIFT_W32(tmp32, shiftFromNearToNoise[i]);

        if (outLShift32[i] < aecm->noiseEst[i])
        {
            // Track the minimum.
            aecm->noiseEst[i] += ((outLShift32[i] - aecm->noiseEst[i]) >> minTrackShift);
        } else
        {
            // Ramp slowly upwards until we hit the minimum again.

            // Avoid overflow.
            if (aecm->noiseEst[i] < 2146435583)
            {
                // Store the fractional portion.
                upper32 = (aecm->noiseEst[i] & 0xffff0000) >> 16;
                lower32 = aecm->noiseEst[i] & 0x0000ffff;
                upper32 = ((upper32 * 2049) >> 11);
                lower32 = ((lower32 * 2049) >> 11);
                aecm->noiseEst[i] = WEBRTC_SPL_ADD_SAT_W32(upper32 << 16, lower32);
            }
        }
    }

    for (i = 0; i < PART_LEN1; i++)
    {
        tmp32 = WEBRTC_SPL_SHIFT_W32(aecm->noiseEst[i], -shiftFromNearToNoise[i]);
        if (tmp32 > 32767)
        {
            tmp32 = 32767;
            aecm->noiseEst[i] = WEBRTC_SPL_SHIFT_W32(tmp32, shiftFromNearToNoise[i]);
        }
        noiseRShift16[i] = (WebRtc_Word16)tmp32;

        tmp16 = ONE_Q14 - lambda[i];
        noiseRShift16[i]
                = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(tmp16, noiseRShift16[i], 14);
    }

    // Generate a uniform random array on [0 2^15-1].
    WebRtcSpl_RandUArray(randW16, PART_LEN, &aecm->seed);

    // Generate noise according to estimated energy.
    uReal[0] = 0; // Reject LF noise.
    uImag[0] = 0;
    for (i = 1; i < PART_LEN1; i++)
    {
        // Get a random index for the cos and sin tables over [0 359].
        tmp16 = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(359, randW16[i - 1], 15);

        // Tables are in Q13.
        uReal[i] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(noiseRShift16[i],
                WebRtcSpl_kCosTable[tmp16], 13);
        uImag[i] = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(-noiseRShift16[i],
                WebRtcSpl_kSinTable[tmp16], 13);
    }
    uImag[PART_LEN] = 0;

#if (!defined ARM_WINM) && (!defined ARM9E_GCC) && (!defined ANDROID_AECOPT)
    for (i = 0; i < PART_LEN1; i++)
    {
        outReal[i] = WEBRTC_SPL_ADD_SAT_W16(outReal[i], uReal[i]);
        outImag[i] = WEBRTC_SPL_ADD_SAT_W16(outImag[i], uImag[i]);
    }
#else
    for (i = 0; i < PART_LEN1 -1; )
    {
        outReal[i] = WEBRTC_SPL_ADD_SAT_W16(outReal[i], uReal[i]);
        outImag[i] = WEBRTC_SPL_ADD_SAT_W16(outImag[i], uImag[i]);
        i++;

        outReal[i] = WEBRTC_SPL_ADD_SAT_W16(outReal[i], uReal[i]);
        outImag[i] = WEBRTC_SPL_ADD_SAT_W16(outImag[i], uImag[i]);
        i++;
    }
    outReal[i] = WEBRTC_SPL_ADD_SAT_W16(outReal[i], uReal[i]);
    outImag[i] = WEBRTC_SPL_ADD_SAT_W16(outImag[i], uImag[i]);
#endif
}

void WebRtcAecm_BufferFarFrame(AecmCore_t * const aecm, const WebRtc_Word16 * const farend,
                               const int farLen)
{
    int writeLen = farLen, writePos = 0;

    // Check if the write position must be wrapped
    while (aecm->farBufWritePos + writeLen > FAR_BUF_LEN)
    {
        // Write to remaining buffer space before wrapping
        writeLen = FAR_BUF_LEN - aecm->farBufWritePos;
        memcpy(aecm->farBuf + aecm->farBufWritePos, farend + writePos,
               sizeof(WebRtc_Word16) * writeLen);
        aecm->farBufWritePos = 0;
        writePos = writeLen;
        writeLen = farLen - writeLen;
    }

    memcpy(aecm->farBuf + aecm->farBufWritePos, farend + writePos,
           sizeof(WebRtc_Word16) * writeLen);
    aecm->farBufWritePos += writeLen;
}

void WebRtcAecm_FetchFarFrame(AecmCore_t * const aecm, WebRtc_Word16 * const farend,
                              const int farLen, const int knownDelay)
{
    int readLen = farLen;
    int readPos = 0;
    int delayChange = knownDelay - aecm->lastKnownDelay;

    aecm->farBufReadPos -= delayChange;

    // Check if delay forces a read position wrap
    while (aecm->farBufReadPos < 0)
    {
        aecm->farBufReadPos += FAR_BUF_LEN;
    }
    while (aecm->farBufReadPos > FAR_BUF_LEN - 1)
    {
        aecm->farBufReadPos -= FAR_BUF_LEN;
    }

    aecm->lastKnownDelay = knownDelay;

    // Check if read position must be wrapped
    while (aecm->farBufReadPos + readLen > FAR_BUF_LEN)
    {

        // Read from remaining buffer space before wrapping
        readLen = FAR_BUF_LEN - aecm->farBufReadPos;
        memcpy(farend + readPos, aecm->farBuf + aecm->farBufReadPos,
               sizeof(WebRtc_Word16) * readLen);
        aecm->farBufReadPos = 0;
        readPos = readLen;
        readLen = farLen - readLen;
    }
    memcpy(farend + readPos, aecm->farBuf + aecm->farBufReadPos,
           sizeof(WebRtc_Word16) * readLen);
    aecm->farBufReadPos += readLen;
}
