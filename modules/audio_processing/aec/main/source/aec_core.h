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
 * Specifies the interface for the AEC core.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC_MAIN_SOURCE_AEC_CORE_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC_MAIN_SOURCE_AEC_CORE_H_

#include <stdio.h>
#include "fft4g.h"
#include "typedefs.h"
#include "signal_processing_library.h"

//#define G167 // for running G167 tests
//#define UNCONSTR // time-unconstrained filter
//#define AEC_DEBUG // for recording files

#define FRAME_LEN 80
#define PART_LEN 64 // Length of partition
#define PART_LEN1 (PART_LEN + 1) // Unique fft coefficients
#define PART_LEN2 (PART_LEN * 2) // Length of partition * 2
#define NR_PART 12 // Number of partitions
#define FILT_LEN (PART_LEN * NR_PART) // Filter length
#define FILT_LEN2 (FILT_LEN * 2) // Double filter length
#define FAR_BUF_LEN (FILT_LEN2 * 2)
#define PREF_BAND_SIZE 24

#define BLOCKL_MAX FRAME_LEN

typedef float complex_t[2];

// Metrics
enum {offsetLevel = -100};

typedef struct {
    float sfrsum;
    int sfrcounter;
    float framelevel;
    float frsum;
    int frcounter;
    float minlevel;
    float averagelevel;
} power_level_t;

typedef struct {
    float instant;
    float average;
    float min;
    float max;
    float sum;
    float hisum;
    float himean;
    int counter;
    int hicounter;
} stats_t;

typedef struct {
    int farBufWritePos, farBufReadPos;

    int knownDelay;
    int inSamples, outSamples;
    int delayEstCtr;

    void *farFrBuf, *nearFrBuf, *outFrBuf;

    void *nearFrBufH;
    void *outFrBufH;

    float xBuf[PART_LEN2]; // farend
    float dBuf[PART_LEN2]; // nearend
    float eBuf[PART_LEN2]; // error

    float dBufH[PART_LEN2]; // nearend

    float xPow[PART_LEN1];
    float dPow[PART_LEN1];
    float dMinPow[PART_LEN1];
    float dInitMinPow[PART_LEN1];
    float *noisePow;
#ifdef FFTW
    float fftR[PART_LEN2];
    fftw_complex fftC[PART_LEN2];
    fftw_plan fftPlan, ifftPlan;

    fftw_complex xfBuf[NR_PART * PART_LEN1];
    fftw_complex wfBuf[NR_PART * PART_LEN1];
    fftw_complex sde[PART_LEN1];
#else
    complex_t xfBuf[NR_PART * PART_LEN1]; // farend fft buffer
    complex_t wfBuf[NR_PART * PART_LEN1]; // filter fft
    complex_t sde[PART_LEN1]; // cross-psd of nearend and error
    complex_t sxd[PART_LEN1]; // cross-psd of farend and nearend
    complex_t xfwBuf[NR_PART * PART_LEN1]; // farend windowed fft buffer
#endif
    float sx[PART_LEN1], sd[PART_LEN1], se[PART_LEN1]; // far, near and error psd
    float hNs[PART_LEN1];
    float hNlFbMin, hNlFbLocalMin;
    float hNlXdAvgMin;
    int hNlNewMin, hNlMinCtr;
    float overDrive, overDriveSm;
    float targetSupp, minOverDrive;
    float outBuf[PART_LEN];
    int delayIdx;

    short stNearState, echoState;
    short divergeState;

    int xfBufBlockPos;

    short farBuf[FILT_LEN2 * 2];

    short mult; // sampling frequency multiple
    int sampFreq;
    WebRtc_UWord32 seed;

    float mu; // stepsize
    float errThresh; // error threshold

    int noiseEstCtr;

    // Toggles for G.167 testing
#ifdef G167
    short adaptToggle;  // Filter adaptation
    short nlpToggle;    // Nonlinear processing
    short cnToggle;     // Comfort noise
#endif

    power_level_t farlevel;
    power_level_t nearlevel;
    power_level_t linoutlevel;
    power_level_t nlpoutlevel;

    int metricsMode;
    int stateCounter;
    stats_t erl;
    stats_t erle;
    stats_t aNlp;
    stats_t rerl;

    // Quantities to control H band scaling for SWB input
    int freq_avg_ic;         //initial bin for averaging nlp gain
    int flag_Hband_cn;      //for comfort noise
    float cn_scale_Hband;   //scale for comfort noise in H band

#ifdef AEC_DEBUG
    FILE *farFile;
    FILE *nearFile;
    FILE *outFile;
    FILE *outLpFile;
#endif
} aec_t;

int WebRtcAec_CreateAec(aec_t **aec);
int WebRtcAec_FreeAec(aec_t *aec);
int WebRtcAec_InitAec(aec_t *aec, int sampFreq);

void WebRtcAec_InitMetrics(aec_t *aec);
void WebRtcAec_ProcessFrame(aec_t *aec, const short *farend,
                       const short *nearend, const short *nearendH,
                       short *out, short *outH,
                       int knownDelay);

#endif // WEBRTC_MODULES_AUDIO_PROCESSING_AEC_MAIN_SOURCE_AEC_CORE_H_

