/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_CORE_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_CORE_H_

#ifdef WEBRTC_AEC_DEBUG_DUMP
#include <stdio.h>
#endif

#include "webrtc/typedefs.h"

#define FRAME_LEN 80
#define PART_LEN 64  // Length of partition
#define PART_LEN1 (PART_LEN + 1)  // Unique fft coefficients
#define PART_LEN2 (PART_LEN * 2)  // Length of partition * 2
#define NR_PART 12  // Number of partitions in filter.
#define PREF_BAND_SIZE 24

// Delay estimator constants, used for logging.
enum { kMaxDelayBlocks = 60 };
enum { kLookaheadBlocks = 15 };
enum { kHistorySizeBlocks = kMaxDelayBlocks + kLookaheadBlocks };

typedef float complex_t[2];
// For performance reasons, some arrays of complex numbers are replaced by twice
// as long arrays of float, all the real parts followed by all the imaginary
// ones (complex_t[SIZE] -> float[2][SIZE]). This allows SIMD optimizations and
// is better than two arrays (one for the real parts and one for the imaginary
// parts) as this other way would require two pointers instead of one and cause
// extra register spilling. This also allows the offsets to be calculated at
// compile time.

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

typedef struct Stats {
  float instant;
  float average;
  float min;
  float max;
  float sum;
  float hisum;
  float himean;
  int counter;
  int hicounter;
} Stats;

typedef struct {
    int farBufWritePos, farBufReadPos;

    int knownDelay;
    int inSamples, outSamples;
    int delayEstCtr;

    void *nearFrBuf, *outFrBuf;

    void *nearFrBufH;
    void *outFrBufH;

    float dBuf[PART_LEN2];  // nearend
    float eBuf[PART_LEN2];  // error

    float dBufH[PART_LEN2];  // nearend

    float xPow[PART_LEN1];
    float dPow[PART_LEN1];
    float dMinPow[PART_LEN1];
    float dInitMinPow[PART_LEN1];
    float *noisePow;

    float xfBuf[2][NR_PART * PART_LEN1];  // farend fft buffer
    float wfBuf[2][NR_PART * PART_LEN1];  // filter fft
    complex_t sde[PART_LEN1];  // cross-psd of nearend and error
    complex_t sxd[PART_LEN1];  // cross-psd of farend and nearend
    complex_t xfwBuf[NR_PART * PART_LEN1];  // farend windowed fft buffer

    float sx[PART_LEN1], sd[PART_LEN1], se[PART_LEN1];  // far, near, error psd
    float hNs[PART_LEN1];
    float hNlFbMin, hNlFbLocalMin;
    float hNlXdAvgMin;
    int hNlNewMin, hNlMinCtr;
    float overDrive, overDriveSm;
    int nlp_mode;
    float outBuf[PART_LEN];
    int delayIdx;

    short stNearState, echoState;
    short divergeState;

    int xfBufBlockPos;

    void* far_buf;
    void* far_buf_windowed;
    int system_delay;  // Current system delay buffered in AEC.

    int mult;  // sampling frequency multiple
    int sampFreq;
    WebRtc_UWord32 seed;

    float mu;  // stepsize
    float errThresh;  // error threshold

    int noiseEstCtr;

    power_level_t farlevel;
    power_level_t nearlevel;
    power_level_t linoutlevel;
    power_level_t nlpoutlevel;

    int metricsMode;
    int stateCounter;
    Stats erl;
    Stats erle;
    Stats aNlp;
    Stats rerl;

    // Quantities to control H band scaling for SWB input
    int freq_avg_ic;  // initial bin for averaging nlp gain
    int flag_Hband_cn;  // for comfort noise
    float cn_scale_Hband;  // scale for comfort noise in H band

    int delay_histogram[kHistorySizeBlocks];
    int delay_logging_enabled;
    void* delay_estimator_farend;
    void* delay_estimator;

#ifdef WEBRTC_AEC_DEBUG_DUMP
    void* far_time_buf;
    FILE *farFile;
    FILE *nearFile;
    FILE *outFile;
    FILE *outLinearFile;
#endif
} aec_t;

typedef void (*WebRtcAec_FilterFar_t)(aec_t *aec, float yf[2][PART_LEN1]);
extern WebRtcAec_FilterFar_t WebRtcAec_FilterFar;
typedef void (*WebRtcAec_ScaleErrorSignal_t)(aec_t *aec, float ef[2][PART_LEN1]);
extern WebRtcAec_ScaleErrorSignal_t WebRtcAec_ScaleErrorSignal;
typedef void (*WebRtcAec_FilterAdaptation_t)
  (aec_t *aec, float *fft, float ef[2][PART_LEN1]);
extern WebRtcAec_FilterAdaptation_t WebRtcAec_FilterAdaptation;
typedef void (*WebRtcAec_OverdriveAndSuppress_t)
  (aec_t *aec, float hNl[PART_LEN1], const float hNlFb, float efw[2][PART_LEN1]);
extern WebRtcAec_OverdriveAndSuppress_t WebRtcAec_OverdriveAndSuppress;

int WebRtcAec_CreateAec(aec_t **aec);
int WebRtcAec_FreeAec(aec_t *aec);
int WebRtcAec_InitAec(aec_t *aec, int sampFreq);
void WebRtcAec_InitAec_SSE2(void);

void WebRtcAec_BufferFarendPartition(aec_t *aec, const float* farend);
void WebRtcAec_ProcessFrame(aec_t* aec,
                            const short* nearend,
                            const short* nearendH,
                            int knownDelay,
                            int16_t* out,
                            int16_t* outH);

// A helper function to call WebRtc_MoveReadPtr() for all far-end buffers.
// Returns the number of elements moved, and adjusts |system_delay| by the
// corresponding amount in ms.
int WebRtcAec_MoveFarReadPtr(aec_t* aec, int elements);
// Calculates the median and standard deviation among the delay estimates
// collected since the last call to this function.
int WebRtcAec_GetDelayMetricsCore(aec_t* self, int* median, int* std);
// Returns the echo state (1: echo, 0: no echo).
int WebRtcAec_echo_state(aec_t* self);
// Gets statistics of the echo metrics ERL, ERLE, A_NLP.
void WebRtcAec_GetEchoStats(aec_t* self, Stats* erl, Stats* erle, Stats* a_nlp);
#ifdef WEBRTC_AEC_DEBUG_DUMP
void* WebRtcAec_far_time_buf(aec_t* self);
#endif
// Sets local configuration modes.
void WebRtcAec_SetConfigCore(aec_t* self, int nlp_mode, int metrics_mode,
                             int delay_logging);
// Returns the current |system_delay|, i.e., the buffered difference between
// far-end and near-end.
int WebRtcAec_system_delay(aec_t* self);
// Sets the |system_delay| to |value|.  Note that if the value is changed
// improperly, there can be a performance regression.  So it should be used with
// care.
void WebRtcAec_SetSystemDelay(aec_t* self, int delay);

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_CORE_H_
