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
 * The core AEC algorithm, which is presented with time-aligned signals.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "aec_core.h"
#include "ring_buffer.h"
#include "system_wrappers/interface/cpu_features_wrapper.h"

#define IP_LEN PART_LEN // this must be at least ceil(2 + sqrt(PART_LEN))
#define W_LEN PART_LEN

// Noise suppression
static const int converged = 250;

// Metrics
static const int subCountLen = 4;
static const int countLen = 50;

// Quantities to control H band scaling for SWB input
static const int flagHbandCn = 1; // flag for adding comfort noise in H band
static const float cnScaleHband = (float)0.4; // scale for comfort noise in H band
// Initial bin for averaging nlp gain in low band
static const int freqAvgIc = PART_LEN / 2;

/* Matlab code to produce table:
win = sqrt(hanning(63)); win = [0 ; win(1:32)];
fprintf(1, '\t%.14f, %.14f, %.14f,\n', win);
*/
/*
static const float sqrtHanning[33] = {
    0.00000000000000, 0.04906767432742, 0.09801714032956,
    0.14673047445536, 0.19509032201613, 0.24298017990326,
    0.29028467725446, 0.33688985339222, 0.38268343236509,
    0.42755509343028, 0.47139673682600, 0.51410274419322,
    0.55557023301960, 0.59569930449243, 0.63439328416365,
    0.67155895484702, 0.70710678118655, 0.74095112535496,
    0.77301045336274, 0.80320753148064, 0.83146961230255,
    0.85772861000027, 0.88192126434835, 0.90398929312344,
    0.92387953251129, 0.94154406518302, 0.95694033573221,
    0.97003125319454, 0.98078528040323, 0.98917650996478,
    0.99518472667220, 0.99879545620517, 1.00000000000000
};
*/

static const float sqrtHanning[65] = {
    0.00000000000000f, 0.02454122852291f, 0.04906767432742f,
    0.07356456359967f, 0.09801714032956f, 0.12241067519922f,
    0.14673047445536f, 0.17096188876030f, 0.19509032201613f,
    0.21910124015687f, 0.24298017990326f, 0.26671275747490f,
    0.29028467725446f, 0.31368174039889f, 0.33688985339222f,
    0.35989503653499f, 0.38268343236509f, 0.40524131400499f,
    0.42755509343028f, 0.44961132965461f, 0.47139673682600f,
    0.49289819222978f, 0.51410274419322f, 0.53499761988710f,
    0.55557023301960f, 0.57580819141785f, 0.59569930449243f,
    0.61523159058063f, 0.63439328416365f, 0.65317284295378f,
    0.67155895484702f, 0.68954054473707f, 0.70710678118655f,
    0.72424708295147f, 0.74095112535496f, 0.75720884650648f,
    0.77301045336274f, 0.78834642762661f, 0.80320753148064f,
    0.81758481315158f, 0.83146961230255f, 0.84485356524971f,
    0.85772861000027f, 0.87008699110871f, 0.88192126434835f,
    0.89322430119552f, 0.90398929312344f, 0.91420975570353f,
    0.92387953251129f, 0.93299279883474f, 0.94154406518302f,
    0.94952818059304f, 0.95694033573221f, 0.96377606579544f,
    0.97003125319454f, 0.97570213003853f, 0.98078528040323f,
    0.98527764238894f, 0.98917650996478f, 0.99247953459871f,
    0.99518472667220f, 0.99729045667869f, 0.99879545620517f,
    0.99969881869620f, 1.00000000000000f
};

/* Matlab code to produce table:
weightCurve = [0 ; 0.3 * sqrt(linspace(0,1,64))' + 0.1];
fprintf(1, '\t%.4f, %.4f, %.4f, %.4f, %.4f, %.4f,\n', weightCurve);
*/
static const float weightCurve[65] = {
    0.0000f, 0.1000f, 0.1378f, 0.1535f, 0.1655f, 0.1756f,
    0.1845f, 0.1926f, 0.2000f, 0.2069f, 0.2134f, 0.2195f,
    0.2254f, 0.2309f, 0.2363f, 0.2414f, 0.2464f, 0.2512f,
    0.2558f, 0.2604f, 0.2648f, 0.2690f, 0.2732f, 0.2773f,
    0.2813f, 0.2852f, 0.2890f, 0.2927f, 0.2964f, 0.3000f,
    0.3035f, 0.3070f, 0.3104f, 0.3138f, 0.3171f, 0.3204f,
    0.3236f, 0.3268f, 0.3299f, 0.3330f, 0.3360f, 0.3390f,
    0.3420f, 0.3449f, 0.3478f, 0.3507f, 0.3535f, 0.3563f,
    0.3591f, 0.3619f, 0.3646f, 0.3673f, 0.3699f, 0.3726f,
    0.3752f, 0.3777f, 0.3803f, 0.3828f, 0.3854f, 0.3878f,
    0.3903f, 0.3928f, 0.3952f, 0.3976f, 0.4000f
};

/* Matlab code to produce table:
overDriveCurve = [sqrt(linspace(0,1,65))' + 1];
fprintf(1, '\t%.4f, %.4f, %.4f, %.4f, %.4f, %.4f,\n', overDriveCurve);
*/
static const float overDriveCurve[65] = {
    1.0000f, 1.1250f, 1.1768f, 1.2165f, 1.2500f, 1.2795f,
    1.3062f, 1.3307f, 1.3536f, 1.3750f, 1.3953f, 1.4146f,
    1.4330f, 1.4507f, 1.4677f, 1.4841f, 1.5000f, 1.5154f,
    1.5303f, 1.5449f, 1.5590f, 1.5728f, 1.5863f, 1.5995f,
    1.6124f, 1.6250f, 1.6374f, 1.6495f, 1.6614f, 1.6731f,
    1.6847f, 1.6960f, 1.7071f, 1.7181f, 1.7289f, 1.7395f,
    1.7500f, 1.7603f, 1.7706f, 1.7806f, 1.7906f, 1.8004f,
    1.8101f, 1.8197f, 1.8292f, 1.8385f, 1.8478f, 1.8570f,
    1.8660f, 1.8750f, 1.8839f, 1.8927f, 1.9014f, 1.9100f,
    1.9186f, 1.9270f, 1.9354f, 1.9437f, 1.9520f, 1.9601f,
    1.9682f, 1.9763f, 1.9843f, 1.9922f, 2.0000f
};

// "Private" function prototypes.
static void ProcessBlock(aec_t *aec, const short *farend,
                              const short *nearend, const short *nearendH,
                              short *out, short *outH);

static void BufferFar(aec_t *aec, const short *farend, int farLen);
static void FetchFar(aec_t *aec, short *farend, int farLen, int knownDelay);

static void NonLinearProcessing(aec_t *aec, int *ip, float *wfft, short *output,
                         short *outputH);

static void GetHighbandGain(const float *lambda, float *nlpGainHband);

// Comfort_noise also computes noise for H band returned in comfortNoiseHband
static void ComfortNoise(aec_t *aec, complex_t *efw,
                                  complex_t *comfortNoiseHband,
                                  const float *noisePow, const float *lambda);

static void WebRtcAec_InitLevel(power_level_t *level);
static void WebRtcAec_InitStats(stats_t *stats);
static void UpdateLevel(power_level_t *level, const short *in);
static void UpdateMetrics(aec_t *aec);

__inline static float MulRe(float aRe, float aIm, float bRe, float bIm)
{
    return aRe * bRe - aIm * bIm;
}

__inline static float MulIm(float aRe, float aIm, float bRe, float bIm)
{
    return aRe * bIm + aIm * bRe;
}

static int CmpFloat(const void *a, const void *b)
{
    const float *da = (const float *)a;
    const float *db = (const float *)b;

    return (*da > *db) - (*da < *db);
}

int WebRtcAec_CreateAec(aec_t **aecInst)
{
    aec_t *aec = malloc(sizeof(aec_t));
    *aecInst = aec;
    if (aec == NULL) {
        return -1;
    }

    if (WebRtcApm_CreateBuffer(&aec->farFrBuf, FRAME_LEN + PART_LEN) == -1) {
        WebRtcAec_FreeAec(aec);
        aec = NULL;
        return -1;
    }

    if (WebRtcApm_CreateBuffer(&aec->nearFrBuf, FRAME_LEN + PART_LEN) == -1) {
        WebRtcAec_FreeAec(aec);
        aec = NULL;
        return -1;
    }

    if (WebRtcApm_CreateBuffer(&aec->outFrBuf, FRAME_LEN + PART_LEN) == -1) {
        WebRtcAec_FreeAec(aec);
        aec = NULL;
        return -1;
    }

    if (WebRtcApm_CreateBuffer(&aec->nearFrBufH, FRAME_LEN + PART_LEN) == -1) {
        WebRtcAec_FreeAec(aec);
        aec = NULL;
        return -1;
    }

    if (WebRtcApm_CreateBuffer(&aec->outFrBufH, FRAME_LEN + PART_LEN) == -1) {
        WebRtcAec_FreeAec(aec);
        aec = NULL;
        return -1;
    }

    return 0;
}

int WebRtcAec_FreeAec(aec_t *aec)
{
    if (aec == NULL) {
        return -1;
    }

    WebRtcApm_FreeBuffer(aec->farFrBuf);
    WebRtcApm_FreeBuffer(aec->nearFrBuf);
    WebRtcApm_FreeBuffer(aec->outFrBuf);

    WebRtcApm_FreeBuffer(aec->nearFrBufH);
    WebRtcApm_FreeBuffer(aec->outFrBufH);

    free(aec);
    return 0;
}

static void FilterFar(aec_t *aec, float yf[2][PART_LEN1])
{
  for (int i = 0; i < NR_PART; i++) {
    int xPos = (i + aec->xfBufBlockPos) * PART_LEN1;
    // Check for wrap
    if (i + aec->xfBufBlockPos >= NR_PART) {
      xPos -= NR_PART*(PART_LEN1);
    }

    int pos = i * PART_LEN1;
    for (int j = 0; j < PART_LEN1; j++) {
      yf[0][j] += MulRe(aec->xfBuf[0][xPos + j], aec->xfBuf[1][xPos + j],
                        aec->wfBuf[0][ pos + j], aec->wfBuf[1][ pos + j]);
      yf[1][j] += MulIm(aec->xfBuf[0][xPos + j], aec->xfBuf[1][xPos + j],
                        aec->wfBuf[0][ pos + j], aec->wfBuf[1][ pos + j]);
    }
  }
}

static void ScaleErrorSignal(aec_t *aec, float ef[2][PART_LEN1])
{
  for (int i = 0; i < (PART_LEN1); i++) {
    ef[0][i] /= (aec->xPow[i] + 1e-10f);
    ef[1][i] /= (aec->xPow[i] + 1e-10f);
    float absEf = sqrtf(ef[0][i] * ef[0][i] + ef[1][i] * ef[1][i]);

    if (absEf > aec->errThresh) {
      absEf = aec->errThresh / (absEf + 1e-10f);
      ef[0][i] *= absEf;
      ef[1][i] *= absEf;
    }

    // Stepsize factor
    ef[0][i] *= aec->mu;
    ef[1][i] *= aec->mu;
  }
}

WebRtcAec_FilterFar_t WebRtcAec_FilterFar;
WebRtcAec_ScaleErrorSignal_t WebRtcAec_ScaleErrorSignal;

extern void WebRtcAec_InitAec_SSE2(void);

int WebRtcAec_InitAec(aec_t *aec, int sampFreq)
{
    int i;

    aec->sampFreq = sampFreq;

    if (sampFreq == 8000) {
        aec->mu = 0.6f;
        aec->errThresh = 2e-6f;
    }
    else {
        aec->mu = 0.5f;
        aec->errThresh = 1.5e-6f;
    }

    if (WebRtcApm_InitBuffer(aec->farFrBuf) == -1) {
        return -1;
    }

    if (WebRtcApm_InitBuffer(aec->nearFrBuf) == -1) {
        return -1;
    }

    if (WebRtcApm_InitBuffer(aec->outFrBuf) == -1) {
        return -1;
    }

    if (WebRtcApm_InitBuffer(aec->nearFrBufH) == -1) {
        return -1;
    }

    if (WebRtcApm_InitBuffer(aec->outFrBufH) == -1) {
        return -1;
    }

    // Default target suppression level
    aec->targetSupp = -11.5;
    aec->minOverDrive = 2.0;

    // Sampling frequency multiplier
    // SWB is processed as 160 frame size
    if (aec->sampFreq == 32000) {
      aec->mult = (short)aec->sampFreq / 16000;
    }
    else {
        aec->mult = (short)aec->sampFreq / 8000;
    }

    aec->farBufWritePos = 0;
    aec->farBufReadPos = 0;

    aec->inSamples = 0;
    aec->outSamples = 0;
    aec->knownDelay = 0;

    // Initialize buffers
    memset(aec->farBuf, 0, sizeof(aec->farBuf));
    memset(aec->xBuf, 0, sizeof(aec->xBuf));
    memset(aec->dBuf, 0, sizeof(aec->dBuf));
    memset(aec->eBuf, 0, sizeof(aec->eBuf));
    // For H band
    memset(aec->dBufH, 0, sizeof(aec->dBufH));

    memset(aec->xPow, 0, sizeof(aec->xPow));
    memset(aec->dPow, 0, sizeof(aec->dPow));
    memset(aec->dInitMinPow, 0, sizeof(aec->dInitMinPow));
    aec->noisePow = aec->dInitMinPow;
    aec->noiseEstCtr = 0;

    // Initial comfort noise power
    for (i = 0; i < PART_LEN1; i++) {
        aec->dMinPow[i] = 1.0e6f;
    }

    // Holds the last block written to
    aec->xfBufBlockPos = 0;
    // TODO: Investigate need for these initializations. Deleting them doesn't
    //       change the output at all and yields 0.4% overall speedup.
    memset(aec->xfBuf, 0, sizeof(complex_t) * NR_PART * PART_LEN1);
    memset(aec->wfBuf, 0, sizeof(complex_t) * NR_PART * PART_LEN1);
    memset(aec->sde, 0, sizeof(complex_t) * PART_LEN1);
    memset(aec->sxd, 0, sizeof(complex_t) * PART_LEN1);
    memset(aec->xfwBuf, 0, sizeof(complex_t) * NR_PART * PART_LEN1);
    memset(aec->se, 0, sizeof(float) * PART_LEN1);

    // To prevent numerical instability in the first block.
    for (i = 0; i < PART_LEN1; i++) {
        aec->sd[i] = 1;
    }
    for (i = 0; i < PART_LEN1; i++) {
        aec->sx[i] = 1;
    }

    memset(aec->hNs, 0, sizeof(aec->hNs));
    memset(aec->outBuf, 0, sizeof(float) * PART_LEN);

    aec->hNlFbMin = 1;
    aec->hNlFbLocalMin = 1;
    aec->hNlXdAvgMin = 1;
    aec->hNlNewMin = 0;
    aec->hNlMinCtr = 0;
    aec->overDrive = 2;
    aec->overDriveSm = 2;
    aec->delayIdx = 0;
    aec->stNearState = 0;
    aec->echoState = 0;
    aec->divergeState = 0;

    aec->seed = 777;
    aec->delayEstCtr = 0;

    // Features on by default (G.167)
#ifdef G167
    aec->adaptToggle = 1;
    aec->nlpToggle = 1;
    aec->cnToggle = 1;
#endif

    // Metrics disabled by default
    aec->metricsMode = 0;
    WebRtcAec_InitMetrics(aec);

    // Assembly optimization
    WebRtcAec_FilterFar = FilterFar;
    WebRtcAec_ScaleErrorSignal = ScaleErrorSignal;
    if (WebRtc_GetCPUInfo(kSSE2)) {
#if defined(__SSE2__)
      WebRtcAec_InitAec_SSE2();
#endif
    }

    return 0;
}

void WebRtcAec_InitMetrics(aec_t *aec)
{
    aec->stateCounter = 0;
    WebRtcAec_InitLevel(&aec->farlevel);
    WebRtcAec_InitLevel(&aec->nearlevel);
    WebRtcAec_InitLevel(&aec->linoutlevel);
    WebRtcAec_InitLevel(&aec->nlpoutlevel);

    WebRtcAec_InitStats(&aec->erl);
    WebRtcAec_InitStats(&aec->erle);
    WebRtcAec_InitStats(&aec->aNlp);
    WebRtcAec_InitStats(&aec->rerl);
}


void WebRtcAec_ProcessFrame(aec_t *aec, const short *farend,
                       const short *nearend, const short *nearendH,
                       short *out, short *outH,
                       int knownDelay)
{
    short farBl[PART_LEN], nearBl[PART_LEN], outBl[PART_LEN];
    short farFr[FRAME_LEN];
    // For H band
    short nearBlH[PART_LEN], outBlH[PART_LEN];

    int size = 0;

    // initialize: only used for SWB
    memset(nearBlH, 0, sizeof(nearBlH));
    memset(outBlH, 0, sizeof(outBlH));

    // Buffer the current frame.
    // Fetch an older one corresponding to the delay.
    BufferFar(aec, farend, FRAME_LEN);
    FetchFar(aec, farFr, FRAME_LEN, knownDelay);

    // Buffer the synchronized far and near frames,
    // to pass the smaller blocks individually.
    WebRtcApm_WriteBuffer(aec->farFrBuf, farFr, FRAME_LEN);
    WebRtcApm_WriteBuffer(aec->nearFrBuf, nearend, FRAME_LEN);
    // For H band
    if (aec->sampFreq == 32000) {
        WebRtcApm_WriteBuffer(aec->nearFrBufH, nearendH, FRAME_LEN);
    }

    // Process as many blocks as possible.
    while (WebRtcApm_get_buffer_size(aec->farFrBuf) >= PART_LEN) {

        WebRtcApm_ReadBuffer(aec->farFrBuf, farBl, PART_LEN);
        WebRtcApm_ReadBuffer(aec->nearFrBuf, nearBl, PART_LEN);

        // For H band
        if (aec->sampFreq == 32000) {
            WebRtcApm_ReadBuffer(aec->nearFrBufH, nearBlH, PART_LEN);
        }

        ProcessBlock(aec, farBl, nearBl, nearBlH, outBl, outBlH);

        WebRtcApm_WriteBuffer(aec->outFrBuf, outBl, PART_LEN);
        // For H band
        if (aec->sampFreq == 32000) {
            WebRtcApm_WriteBuffer(aec->outFrBufH, outBlH, PART_LEN);
        }
    }

    // Stuff the out buffer if we have less than a frame to output.
    // This should only happen for the first frame.
    size = WebRtcApm_get_buffer_size(aec->outFrBuf);
    if (size < FRAME_LEN) {
        WebRtcApm_StuffBuffer(aec->outFrBuf, FRAME_LEN - size);
        if (aec->sampFreq == 32000) {
            WebRtcApm_StuffBuffer(aec->outFrBufH, FRAME_LEN - size);
        }
    }

    // Obtain an output frame.
    WebRtcApm_ReadBuffer(aec->outFrBuf, out, FRAME_LEN);
    // For H band
    if (aec->sampFreq == 32000) {
        WebRtcApm_ReadBuffer(aec->outFrBufH, outH, FRAME_LEN);
    }
}

static void ProcessBlock(aec_t *aec, const short *farend,
                              const short *nearend, const short *nearendH,
                              short *output, short *outputH)
{
    int i, j, pos;
    float d[PART_LEN], y[PART_LEN], e[PART_LEN], dH[PART_LEN];
    short eInt16[PART_LEN];
    float scale;
    int xPos;
    float absEf;

    float fft[PART_LEN2];
    float xf[2][PART_LEN1], yf[2][PART_LEN1], ef[2][PART_LEN1];
    complex_t df[PART_LEN1];
    int ip[IP_LEN];
    float wfft[W_LEN];

    const float gPow[2] = {0.9f, 0.1f};

    // Noise estimate constants.
    const int noiseInitBlocks = 500 * aec->mult;
    const float step = 0.1f;
    const float ramp = 1.0002f;
    const float gInitNoise[2] = {0.999f, 0.001f};

#ifdef AEC_DEBUG
    fwrite(farend, sizeof(short), PART_LEN, aec->farFile);
    fwrite(nearend, sizeof(short), PART_LEN, aec->nearFile);
#endif

    memset(dH, 0, sizeof(dH));

    // ---------- Ooura fft ----------
    // Concatenate old and new farend blocks.
    for (i = 0; i < PART_LEN; i++) {
        aec->xBuf[i + PART_LEN] = (float)farend[i];
        d[i] = (float)nearend[i];
    }

    if (aec->sampFreq == 32000) {
        for (i = 0; i < PART_LEN; i++) {
            dH[i] = (float)nearendH[i];
        }
    }


    memcpy(fft, aec->xBuf, sizeof(float) * PART_LEN2);
    memcpy(aec->dBuf + PART_LEN, d, sizeof(float) * PART_LEN);
    // For H band
    if (aec->sampFreq == 32000) {
        memcpy(aec->dBufH + PART_LEN, dH, sizeof(float) * PART_LEN);
    }

    // Setting this on the first call initializes work arrays.
    ip[0] = 0;
    rdft(PART_LEN2, 1, fft, ip, wfft);

    // Far fft
    xf[1][0] = 0;
    xf[1][PART_LEN] = 0;
    xf[0][0] = fft[0];
    xf[0][PART_LEN] = fft[1];

    for (i = 1; i < PART_LEN; i++) {
        xf[0][i] = fft[2 * i];
        xf[1][i] = fft[2 * i + 1];
    }

    // Near fft
    memcpy(fft, aec->dBuf, sizeof(float) * PART_LEN2);
    rdft(PART_LEN2, 1, fft, ip, wfft);
    df[0][1] = 0;
    df[PART_LEN][1] = 0;
    df[0][0] = fft[0];
    df[PART_LEN][0] = fft[1];

    for (i = 1; i < PART_LEN; i++) {
        df[i][0] = fft[2 * i];
        df[i][1] = fft[2 * i + 1];
    }

    // Power smoothing
    for (i = 0; i < PART_LEN1; i++) {
        aec->xPow[i] = gPow[0] * aec->xPow[i] + gPow[1] * NR_PART *
            (xf[0][i] * xf[0][i] + xf[1][i] * xf[1][i]);
        aec->dPow[i] = gPow[0] * aec->dPow[i] + gPow[1] *
            (df[i][0] * df[i][0] + df[i][1] * df[i][1]);
    }

    // Estimate noise power. Wait until dPow is more stable.
    if (aec->noiseEstCtr > 50) {
        for (i = 0; i < PART_LEN1; i++) {
            if (aec->dPow[i] < aec->dMinPow[i]) {
                aec->dMinPow[i] = (aec->dPow[i] + step * (aec->dMinPow[i] -
                    aec->dPow[i])) * ramp;
            }
            else {
                aec->dMinPow[i] *= ramp;
            }
        }
    }

    // Smooth increasing noise power from zero at the start,
    // to avoid a sudden burst of comfort noise.
    if (aec->noiseEstCtr < noiseInitBlocks) {
        aec->noiseEstCtr++;
        for (i = 0; i < PART_LEN1; i++) {
            if (aec->dMinPow[i] > aec->dInitMinPow[i]) {
                aec->dInitMinPow[i] = gInitNoise[0] * aec->dInitMinPow[i] +
                    gInitNoise[1] * aec->dMinPow[i];
            }
            else {
                aec->dInitMinPow[i] = aec->dMinPow[i];
            }
        }
        aec->noisePow = aec->dInitMinPow;
    }
    else {
        aec->noisePow = aec->dMinPow;
    }


    // Update the xfBuf block position.
    aec->xfBufBlockPos--;
    if (aec->xfBufBlockPos == -1) {
        aec->xfBufBlockPos = NR_PART - 1;
    }

    // Buffer xf
    memcpy(aec->xfBuf[0] + aec->xfBufBlockPos * PART_LEN1, xf[0],
           sizeof(float) * PART_LEN1);
    memcpy(aec->xfBuf[1] + aec->xfBufBlockPos * PART_LEN1, xf[1],
           sizeof(float) * PART_LEN1);

    memset(yf[0], 0, sizeof(float) * (PART_LEN1 * 2));

    // Filter far
    WebRtcAec_FilterFar(aec, yf);

    // Inverse fft to obtain echo estimate and error.
    fft[0] = yf[0][0];
    fft[1] = yf[0][PART_LEN];
    for (i = 1; i < PART_LEN; i++) {
        fft[2 * i] = yf[0][i];
        fft[2 * i + 1] = yf[1][i];
    }
    rdft(PART_LEN2, -1, fft, ip, wfft);

    scale = 2.0f / PART_LEN2;
    for (i = 0; i < PART_LEN; i++) {
        y[i] = fft[PART_LEN + i] * scale; // fft scaling
    }

    for (i = 0; i < PART_LEN; i++) {
        e[i] = d[i] - y[i];
    }

    // Error fft
    memcpy(aec->eBuf + PART_LEN, e, sizeof(float) * PART_LEN);
    memset(fft, 0, sizeof(float) * PART_LEN);
    memcpy(fft + PART_LEN, e, sizeof(float) * PART_LEN);
    rdft(PART_LEN2, 1, fft, ip, wfft);

    ef[1][0] = 0;
    ef[1][PART_LEN] = 0;
    ef[0][0] = fft[0];
    ef[0][PART_LEN] = fft[1];
    for (i = 1; i < PART_LEN; i++) {
        ef[0][i] = fft[2 * i];
        ef[1][i] = fft[2 * i + 1];
    }

    // Scale error signal inversely with far power.
    WebRtcAec_ScaleErrorSignal(aec, ef);
#ifdef G167
    if (aec->adaptToggle) {
#endif
        // Filter adaptation
        for (i = 0; i < NR_PART; i++) {
            xPos = (i + aec->xfBufBlockPos)*(PART_LEN1);
            // Check for wrap
            if (i + aec->xfBufBlockPos >= NR_PART) {
                xPos -= NR_PART*(PART_LEN1);
            }

            pos = i * PART_LEN1;

#ifdef UNCONSTR
            for (j = 0; j < PART_LEN1; j++) {
                aec->wfBuf[pos + j][0] += MulRe(aec->xfBuf[xPos + j][0],
                    -aec->xfBuf[xPos + j][1], ef[j][0], ef[j][1]);
                aec->wfBuf[pos + j][1] += MulIm(aec->xfBuf[xPos + j][0],
                    -aec->xfBuf[xPos + j][1], ef[j][0], ef[j][1]);
            }
#else
            fft[0] = MulRe(aec->xfBuf[0][xPos], -aec->xfBuf[1][xPos],
                           ef[0][0], ef[1][0]);
            fft[1] = MulRe(aec->xfBuf[0][xPos + PART_LEN],
                           -aec->xfBuf[1][xPos + PART_LEN],
                           ef[0][PART_LEN], ef[1][PART_LEN]);

            for (j = 1; j < PART_LEN; j++) {

                fft[2 * j] = MulRe(aec->xfBuf[0][xPos + j],
                                   -aec->xfBuf[1][xPos + j],
                                   ef[0][j], ef[1][j]);
                fft[2 * j + 1] = MulIm(aec->xfBuf[0][xPos + j],
                                       -aec->xfBuf[1][xPos + j],
                                       ef[0][j], ef[1][j]);
            }
            rdft(PART_LEN2, -1, fft, ip, wfft);
            memset(fft + PART_LEN, 0, sizeof(float)*PART_LEN);

            scale = 2.0f / PART_LEN2;
            for (j = 0; j < PART_LEN; j++) {
                fft[j] *= scale; // fft scaling
            }
            rdft(PART_LEN2, 1, fft, ip, wfft);

            aec->wfBuf[0][pos] += fft[0];
            aec->wfBuf[0][pos + PART_LEN] += fft[1];

            for (j = 1; j < PART_LEN; j++) {
                aec->wfBuf[0][pos + j] += fft[2 * j];
                aec->wfBuf[1][pos + j] += fft[2 * j + 1];
            }
#endif // UNCONSTR
        }
#ifdef G167
    }
#endif

    NonLinearProcessing(aec, ip, wfft, output, outputH);

#if defined(AEC_DEBUG) || defined(G167)
    for (i = 0; i < PART_LEN; i++) {
        eInt16[i] = (short)WEBRTC_SPL_SAT(WEBRTC_SPL_WORD16_MAX, e[i],
            WEBRTC_SPL_WORD16_MIN);
    }
#endif
#ifdef G167
    if (aec->nlpToggle == 0) {
        memcpy(output, eInt16, sizeof(eInt16));
    }
#endif

    if (aec->metricsMode == 1) {
        for (i = 0; i < PART_LEN; i++) {
            eInt16[i] = (short)WEBRTC_SPL_SAT(WEBRTC_SPL_WORD16_MAX, e[i],
                WEBRTC_SPL_WORD16_MIN);
        }

        // Update power levels and echo metrics
        UpdateLevel(&aec->farlevel, farend);
        UpdateLevel(&aec->nearlevel, nearend);
        UpdateLevel(&aec->linoutlevel, eInt16);
        UpdateLevel(&aec->nlpoutlevel, output);
        UpdateMetrics(aec);
    }

#ifdef AEC_DEBUG
    fwrite(eInt16, sizeof(short), PART_LEN, aec->outLpFile);
    fwrite(output, sizeof(short), PART_LEN, aec->outFile);
#endif
}

static void NonLinearProcessing(aec_t *aec, int *ip, float *wfft, short *output, short *outputH)
{
    complex_t dfw[PART_LEN1], efw[PART_LEN1], xfw[PART_LEN1];
    complex_t comfortNoiseHband[PART_LEN1];
    float fft[PART_LEN2];
    float scale, dtmp;
    float nlpGainHband;
    int i, j, pos;

    // Coherence and non-linear filter
    float cohde[PART_LEN1], cohxd[PART_LEN1];
    float hNlDeAvg, hNlXdAvg;
    float hNl[PART_LEN1];
    float hNlPref[PREF_BAND_SIZE];
    float hNlFb = 0, hNlFbLow = 0;
    const float prefBandQuant = 0.75f, prefBandQuantLow = 0.5f;
    const int prefBandSize = PREF_BAND_SIZE / aec->mult;
    const int minPrefBand = 4 / aec->mult;

    // Near and error power sums
    float sdSum = 0, seSum = 0;

    // Power estimate smoothing coefficients
    const float gCoh[2][2] = {{0.9f, 0.1f}, {0.93f, 0.07f}};
    const float *ptrGCoh = gCoh[aec->mult - 1];

    // Filter energey
    float wfEnMax = 0, wfEn = 0;
    const int delayEstInterval = 10 * aec->mult;

    aec->delayEstCtr++;
    if (aec->delayEstCtr == delayEstInterval) {
        aec->delayEstCtr = 0;
    }

    // initialize comfort noise for H band
    memset(comfortNoiseHband, 0, sizeof(comfortNoiseHband));
    nlpGainHband = (float)0.0;
    dtmp = (float)0.0;

    // Measure energy in each filter partition to determine delay.
    // TODO: Spread by computing one partition per block?
    if (aec->delayEstCtr == 0) {
        wfEnMax = 0;
        aec->delayIdx = 0;
        for (i = 0; i < NR_PART; i++) {
            pos = i * PART_LEN1;
            wfEn = 0;
            for (j = 0; j < PART_LEN1; j++) {
                wfEn += aec->wfBuf[0][pos + j] * aec->wfBuf[0][pos + j] +
                    aec->wfBuf[1][pos + j] * aec->wfBuf[1][pos + j];
            }

            if (wfEn > wfEnMax) {
                wfEnMax = wfEn;
                aec->delayIdx = i;
            }
        }
    }

    // NLP
    // Windowed far fft
    for (i = 0; i < PART_LEN; i++) {
        fft[i] = aec->xBuf[i] * sqrtHanning[i];
        fft[PART_LEN + i] = aec->xBuf[PART_LEN + i] * sqrtHanning[PART_LEN - i];
    }
    rdft(PART_LEN2, 1, fft, ip, wfft);

    xfw[0][1] = 0;
    xfw[PART_LEN][1] = 0;
    xfw[0][0] = fft[0];
    xfw[PART_LEN][0] = fft[1];
    for (i = 1; i < PART_LEN; i++) {
        xfw[i][0] = fft[2 * i];
        xfw[i][1] = fft[2 * i + 1];
    }

    // Buffer far.
    memcpy(aec->xfwBuf, xfw, sizeof(xfw));

    // Use delayed far.
    memcpy(xfw, aec->xfwBuf + aec->delayIdx * PART_LEN1, sizeof(xfw));

    // Windowed near fft
    for (i = 0; i < PART_LEN; i++) {
        fft[i] = aec->dBuf[i] * sqrtHanning[i];
        fft[PART_LEN + i] = aec->dBuf[PART_LEN + i] * sqrtHanning[PART_LEN - i];
    }
    rdft(PART_LEN2, 1, fft, ip, wfft);

    dfw[0][1] = 0;
    dfw[PART_LEN][1] = 0;
    dfw[0][0] = fft[0];
    dfw[PART_LEN][0] = fft[1];
    for (i = 1; i < PART_LEN; i++) {
        dfw[i][0] = fft[2 * i];
        dfw[i][1] = fft[2 * i + 1];
    }

    // Windowed error fft
    for (i = 0; i < PART_LEN; i++) {
        fft[i] = aec->eBuf[i] * sqrtHanning[i];
        fft[PART_LEN + i] = aec->eBuf[PART_LEN + i] * sqrtHanning[PART_LEN - i];
    }
    rdft(PART_LEN2, 1, fft, ip, wfft);
    efw[0][1] = 0;
    efw[PART_LEN][1] = 0;
    efw[0][0] = fft[0];
    efw[PART_LEN][0] = fft[1];
    for (i = 1; i < PART_LEN; i++) {
        efw[i][0] = fft[2 * i];
        efw[i][1] = fft[2 * i + 1];
    }

    // Smoothed PSD
    for (i = 0; i < PART_LEN1; i++) {
        aec->sd[i] = ptrGCoh[0] * aec->sd[i] + ptrGCoh[1] *
            (dfw[i][0] * dfw[i][0] + dfw[i][1] * dfw[i][1]);
        aec->se[i] = ptrGCoh[0] * aec->se[i] + ptrGCoh[1] *
            (efw[i][0] * efw[i][0] + efw[i][1] * efw[i][1]);
        // We threshold here to protect against the ill-effects of a zero farend.
        // The threshold is not arbitrarily chosen, but balances protection and
        // adverse interaction with the algorithm's tuning.
        // TODO: investigate further why this is so sensitive.
        aec->sx[i] = ptrGCoh[0] * aec->sx[i] + ptrGCoh[1] *
            WEBRTC_SPL_MAX(xfw[i][0] * xfw[i][0] + xfw[i][1] * xfw[i][1], 15);

        aec->sde[i][0] = ptrGCoh[0] * aec->sde[i][0] + ptrGCoh[1] *
            (dfw[i][0] * efw[i][0] + dfw[i][1] * efw[i][1]);
        aec->sde[i][1] = ptrGCoh[0] * aec->sde[i][1] + ptrGCoh[1] *
            (dfw[i][0] * efw[i][1] - dfw[i][1] * efw[i][0]);

        aec->sxd[i][0] = ptrGCoh[0] * aec->sxd[i][0] + ptrGCoh[1] *
            (dfw[i][0] * xfw[i][0] + dfw[i][1] * xfw[i][1]);
        aec->sxd[i][1] = ptrGCoh[0] * aec->sxd[i][1] + ptrGCoh[1] *
            (dfw[i][0] * xfw[i][1] - dfw[i][1] * xfw[i][0]);

        sdSum += aec->sd[i];
        seSum += aec->se[i];
    }

    // Divergent filter safeguard.
    if (aec->divergeState == 0) {
        if (seSum > sdSum) {
            aec->divergeState = 1;
        }
    }
    else {
        if (seSum * 1.05f < sdSum) {
            aec->divergeState = 0;
        }
    }

    if (aec->divergeState == 1) {
        memcpy(efw, dfw, sizeof(efw));
    }

    // Reset if error is significantly larger than nearend (13 dB).
    if (seSum > (19.95f * sdSum)) {
        memset(aec->wfBuf, 0, sizeof(aec->wfBuf));
    }

    // Subband coherence
    for (i = 0; i < PART_LEN1; i++) {
        cohde[i] = (aec->sde[i][0] * aec->sde[i][0] + aec->sde[i][1] * aec->sde[i][1]) /
            (aec->sd[i] * aec->se[i] + 1e-10f);
        cohxd[i] = (aec->sxd[i][0] * aec->sxd[i][0] + aec->sxd[i][1] * aec->sxd[i][1]) /
            (aec->sx[i] * aec->sd[i] + 1e-10f);
    }

    hNlXdAvg = 0;
    for (i = minPrefBand; i < prefBandSize + minPrefBand; i++) {
        hNlXdAvg += cohxd[i];
    }
    hNlXdAvg /= prefBandSize;
    hNlXdAvg = 1 - hNlXdAvg;

    hNlDeAvg = 0;
    for (i = minPrefBand; i < prefBandSize + minPrefBand; i++) {
        hNlDeAvg += cohde[i];
    }
    hNlDeAvg /= prefBandSize;

    if (hNlXdAvg < 0.75f && hNlXdAvg < aec->hNlXdAvgMin) {
        aec->hNlXdAvgMin = hNlXdAvg;
    }

    if (hNlDeAvg > 0.98f && hNlXdAvg > 0.9f) {
        aec->stNearState = 1;
    }
    else if (hNlDeAvg < 0.95f || hNlXdAvg < 0.8f) {
        aec->stNearState = 0;
    }

    if (aec->hNlXdAvgMin == 1) {
        aec->echoState = 0;
        aec->overDrive = aec->minOverDrive;

        if (aec->stNearState == 1) {
            memcpy(hNl, cohde, sizeof(hNl));
            hNlFb = hNlDeAvg;
            hNlFbLow = hNlDeAvg;
        }
        else {
            for (i = 0; i < PART_LEN1; i++) {
                hNl[i] = 1 - cohxd[i];
            }
            hNlFb = hNlXdAvg;
            hNlFbLow = hNlXdAvg;
        }
    }
    else {

        if (aec->stNearState == 1) {
            aec->echoState = 0;
            memcpy(hNl, cohde, sizeof(hNl));
            hNlFb = hNlDeAvg;
            hNlFbLow = hNlDeAvg;
        }
        else {
            aec->echoState = 1;
            for (i = 0; i < PART_LEN1; i++) {
                hNl[i] = WEBRTC_SPL_MIN(cohde[i], 1 - cohxd[i]);
            }

            // Select an order statistic from the preferred bands.
            // TODO: Using quicksort now, but a selection algorithm may be preferred.
            memcpy(hNlPref, &hNl[minPrefBand], sizeof(float) * prefBandSize);
            qsort(hNlPref, prefBandSize, sizeof(float), CmpFloat);
            hNlFb = hNlPref[(int)floor(prefBandQuant * (prefBandSize - 1))];
            hNlFbLow = hNlPref[(int)floor(prefBandQuantLow * (prefBandSize - 1))];
        }
    }

    // Track the local filter minimum to determine suppression overdrive.
    if (hNlFbLow < 0.6f && hNlFbLow < aec->hNlFbLocalMin) {
        aec->hNlFbLocalMin = hNlFbLow;
        aec->hNlFbMin = hNlFbLow;
        aec->hNlNewMin = 1;
        aec->hNlMinCtr = 0;
    }
    aec->hNlFbLocalMin = WEBRTC_SPL_MIN(aec->hNlFbLocalMin + 0.0008f / aec->mult, 1);
    aec->hNlXdAvgMin = WEBRTC_SPL_MIN(aec->hNlXdAvgMin + 0.0006f / aec->mult, 1);

    if (aec->hNlNewMin == 1) {
        aec->hNlMinCtr++;
    }
    if (aec->hNlMinCtr == 2) {
        aec->hNlNewMin = 0;
        aec->hNlMinCtr = 0;
        aec->overDrive = WEBRTC_SPL_MAX(aec->targetSupp /
            ((float)log(aec->hNlFbMin + 1e-10f) + 1e-10f), aec->minOverDrive);
    }

    // Smooth the overdrive.
    if (aec->overDrive < aec->overDriveSm) {
        aec->overDriveSm = 0.99f * aec->overDriveSm + 0.01f * aec->overDrive;
    }
    else {
        aec->overDriveSm = 0.9f * aec->overDriveSm + 0.1f * aec->overDrive;
    }

    for (i = 0; i < PART_LEN1; i++) {
        // Weight subbands
        if (hNl[i] > hNlFb) {
            hNl[i] = weightCurve[i] * hNlFb + (1 - weightCurve[i]) * hNl[i];
        }

        hNl[i] = (float)pow(hNl[i], aec->overDriveSm * overDriveCurve[i]);

        // Suppress error signal
        efw[i][0] *= hNl[i];
        efw[i][1] *= hNl[i];

        // Ooura fft returns incorrect sign on imaginary component.
        // It matters here because we are making an additive change with comfort noise.
        efw[i][1] *= -1;
    }


#ifdef G167
    if (aec->cnToggle) {
      ComfortNoise(aec, efw, comfortNoiseHband, aec->noisePow, hNl);
    }
#else
    // Add comfort noise.
    ComfortNoise(aec, efw, comfortNoiseHband, aec->noisePow, hNl);
#endif

    // Inverse error fft.
    fft[0] = efw[0][0];
    fft[1] = efw[PART_LEN][0];
    for (i = 1; i < PART_LEN; i++) {
        fft[2*i] = efw[i][0];
        // Sign change required by Ooura fft.
        fft[2*i + 1] = -efw[i][1];
    }
    rdft(PART_LEN2, -1, fft, ip, wfft);

    // Overlap and add to obtain output.
    scale = 2.0f / PART_LEN2;
    for (i = 0; i < PART_LEN; i++) {
        fft[i] *= scale; // fft scaling
        fft[i] = fft[i]*sqrtHanning[i] + aec->outBuf[i];

        // Saturation protection
        output[i] = (short)WEBRTC_SPL_SAT(WEBRTC_SPL_WORD16_MAX, fft[i],
            WEBRTC_SPL_WORD16_MIN);

        fft[PART_LEN + i] *= scale; // fft scaling
        aec->outBuf[i] = fft[PART_LEN + i] * sqrtHanning[PART_LEN - i];
    }

    // For H band
    if (aec->sampFreq == 32000) {

        // H band gain
        // average nlp over low band: average over second half of freq spectrum
        // (4->8khz)
        GetHighbandGain(hNl, &nlpGainHband);

        // Inverse comfort_noise
        if (flagHbandCn == 1) {
            fft[0] = comfortNoiseHband[0][0];
            fft[1] = comfortNoiseHband[PART_LEN][0];
            for (i = 1; i < PART_LEN; i++) {
                fft[2*i] = comfortNoiseHband[i][0];
                fft[2*i + 1] = comfortNoiseHband[i][1];
            }
            rdft(PART_LEN2, -1, fft, ip, wfft);
            scale = 2.0f / PART_LEN2;
        }

        // compute gain factor
        for (i = 0; i < PART_LEN; i++) {
            dtmp = (float)aec->dBufH[i];
            dtmp = (float)dtmp * nlpGainHband; // for variable gain

            // add some comfort noise where Hband is attenuated
            if (flagHbandCn == 1) {
                fft[i] *= scale; // fft scaling
                dtmp += cnScaleHband * fft[i];
            }

            // Saturation protection
            outputH[i] = (short)WEBRTC_SPL_SAT(WEBRTC_SPL_WORD16_MAX, dtmp,
                WEBRTC_SPL_WORD16_MIN);
         }
    }

    // Copy the current block to the old position.
    memcpy(aec->xBuf, aec->xBuf + PART_LEN, sizeof(float) * PART_LEN);
    memcpy(aec->dBuf, aec->dBuf + PART_LEN, sizeof(float) * PART_LEN);
    memcpy(aec->eBuf, aec->eBuf + PART_LEN, sizeof(float) * PART_LEN);

    // Copy the current block to the old position for H band
    if (aec->sampFreq == 32000) {
        memcpy(aec->dBufH, aec->dBufH + PART_LEN, sizeof(float) * PART_LEN);
    }

    memmove(aec->xfwBuf + PART_LEN1, aec->xfwBuf, sizeof(aec->xfwBuf) -
        sizeof(complex_t) * PART_LEN1);
}

static void GetHighbandGain(const float *lambda, float *nlpGainHband)
{
    int i;

    nlpGainHband[0] = (float)0.0;
    for (i = freqAvgIc; i < PART_LEN1 - 1; i++) {
        nlpGainHband[0] += lambda[i];
    }
    nlpGainHband[0] /= (float)(PART_LEN1 - 1 - freqAvgIc);
}

static void ComfortNoise(aec_t *aec, complex_t *efw,
    complex_t *comfortNoiseHband, const float *noisePow, const float *lambda)
{
    int i, num;
    float rand[PART_LEN];
    float noise, noiseAvg, tmp, tmpAvg;
    WebRtc_Word16 randW16[PART_LEN];
    complex_t u[PART_LEN1];

    const float pi2 = 6.28318530717959f;

    // Generate a uniform random array on [0 1]
    WebRtcSpl_RandUArray(randW16, PART_LEN, &aec->seed);
    for (i = 0; i < PART_LEN; i++) {
        rand[i] = ((float)randW16[i]) / 32768;
    }

    // Reject LF noise
    u[0][0] = 0;
    u[0][1] = 0;
    for (i = 1; i < PART_LEN1; i++) {
        tmp = pi2 * rand[i - 1];

        noise = sqrtf(noisePow[i]);
        u[i][0] = noise * (float)cos(tmp);
        u[i][1] = -noise * (float)sin(tmp);
    }
    u[PART_LEN][1] = 0;

    for (i = 0; i < PART_LEN1; i++) {
        // This is the proper weighting to match the background noise power
        tmp = sqrtf(WEBRTC_SPL_MAX(1 - lambda[i] * lambda[i], 0));
        //tmp = 1 - lambda[i];
        efw[i][0] += tmp * u[i][0];
        efw[i][1] += tmp * u[i][1];
    }

    // For H band comfort noise
    // TODO: don't compute noise and "tmp" twice. Use the previous results.
    noiseAvg = 0.0;
    tmpAvg = 0.0;
    num = 0;
    if (aec->sampFreq == 32000 && flagHbandCn == 1) {

        // average noise scale
        // average over second half of freq spectrum (i.e., 4->8khz)
        // TODO: we shouldn't need num. We know how many elements we're summing.
        for (i = PART_LEN1 >> 1; i < PART_LEN1; i++) {
            num++;
            noiseAvg += sqrtf(noisePow[i]);
        }
        noiseAvg /= (float)num;

        // average nlp scale
        // average over second half of freq spectrum (i.e., 4->8khz)
        // TODO: we shouldn't need num. We know how many elements we're summing.
        num = 0;
        for (i = PART_LEN1 >> 1; i < PART_LEN1; i++) {
            num++;
            tmpAvg += sqrtf(WEBRTC_SPL_MAX(1 - lambda[i] * lambda[i], 0));
        }
        tmpAvg /= (float)num;

        // Use average noise for H band
        // TODO: we should probably have a new random vector here.
        // Reject LF noise
        u[0][0] = 0;
        u[0][1] = 0;
        for (i = 1; i < PART_LEN1; i++) {
            tmp = pi2 * rand[i - 1];

            // Use average noise for H band
            u[i][0] = noiseAvg * (float)cos(tmp);
            u[i][1] = -noiseAvg * (float)sin(tmp);
        }
        u[PART_LEN][1] = 0;

        for (i = 0; i < PART_LEN1; i++) {
            // Use average NLP weight for H band
            comfortNoiseHband[i][0] = tmpAvg * u[i][0];
            comfortNoiseHband[i][1] = tmpAvg * u[i][1];
        }
    }
}

// Buffer the farend to account for knownDelay
static void BufferFar(aec_t *aec, const short *farend, int farLen)
{
    int writeLen = farLen, writePos = 0;

    // Check if the write position must be wrapped.
    while (aec->farBufWritePos + writeLen > FAR_BUF_LEN) {

        // Write to remaining buffer space before wrapping.
        writeLen = FAR_BUF_LEN - aec->farBufWritePos;
        memcpy(aec->farBuf + aec->farBufWritePos, farend + writePos,
            sizeof(short) * writeLen);
        aec->farBufWritePos = 0;
        writePos = writeLen;
        writeLen = farLen - writeLen;
    }

    memcpy(aec->farBuf + aec->farBufWritePos, farend + writePos,
        sizeof(short) * writeLen);
    aec->farBufWritePos +=  writeLen;
}

static void FetchFar(aec_t *aec, short *farend, int farLen, int knownDelay)
{
    int readLen = farLen, readPos = 0, delayChange = knownDelay - aec->knownDelay;

    aec->farBufReadPos -= delayChange;

    // Check if delay forces a read position wrap.
    while(aec->farBufReadPos < 0) {
        aec->farBufReadPos += FAR_BUF_LEN;
    }
    while(aec->farBufReadPos > FAR_BUF_LEN - 1) {
        aec->farBufReadPos -= FAR_BUF_LEN;
    }

    aec->knownDelay = knownDelay;

    // Check if read position must be wrapped.
    while (aec->farBufReadPos + readLen > FAR_BUF_LEN) {

        // Read from remaining buffer space before wrapping.
        readLen = FAR_BUF_LEN - aec->farBufReadPos;
        memcpy(farend + readPos, aec->farBuf + aec->farBufReadPos,
            sizeof(short) * readLen);
        aec->farBufReadPos = 0;
        readPos = readLen;
        readLen = farLen - readLen;
    }
    memcpy(farend + readPos, aec->farBuf + aec->farBufReadPos,
        sizeof(short) * readLen);
    aec->farBufReadPos += readLen;
}

static void WebRtcAec_InitLevel(power_level_t *level)
{
    const float bigFloat = 1E17f;

    level->averagelevel = 0;
    level->framelevel = 0;
    level->minlevel = bigFloat;
    level->frsum = 0;
    level->sfrsum = 0;
    level->frcounter = 0;
    level->sfrcounter = 0;
}

static void WebRtcAec_InitStats(stats_t *stats)
{
    stats->instant = offsetLevel;
    stats->average = offsetLevel;
    stats->max = offsetLevel;
    stats->min = offsetLevel * (-1);
    stats->sum = 0;
    stats->hisum = 0;
    stats->himean = offsetLevel;
    stats->counter = 0;
    stats->hicounter = 0;
}

static void UpdateLevel(power_level_t *level, const short *in)
{
    int k;

    for (k = 0; k < PART_LEN; k++) {
        level->sfrsum += in[k] * in[k];
    }
    level->sfrcounter++;

    if (level->sfrcounter > subCountLen) {
        level->framelevel = level->sfrsum / (subCountLen * PART_LEN);
        level->sfrsum = 0;
        level->sfrcounter = 0;

        if (level->framelevel > 0) {
            if (level->framelevel < level->minlevel) {
                level->minlevel = level->framelevel;     // New minimum
            } else {
                level->minlevel *= (1 + 0.001f);   // Small increase
            }
        }
        level->frcounter++;
        level->frsum += level->framelevel;

        if (level->frcounter > countLen) {
            level->averagelevel =  level->frsum / countLen;
            level->frsum = 0;
            level->frcounter = 0;
        }

    }
}

static void UpdateMetrics(aec_t *aec)
{
    float dtmp, dtmp2, dtmp3;

    const float actThresholdNoisy = 8.0f;
    const float actThresholdClean = 40.0f;
    const float safety = 0.99995f;
    const float noisyPower = 300000.0f;

    float actThreshold;
    float echo, suppressedEcho;

    if (aec->echoState) {   // Check if echo is likely present
        aec->stateCounter++;
    }

    if (aec->farlevel.frcounter == countLen) {

        if (aec->farlevel.minlevel < noisyPower) {
            actThreshold = actThresholdClean;
        }
        else {
            actThreshold = actThresholdNoisy;
        }

        if ((aec->stateCounter > (0.5f * countLen * subCountLen))
            && (aec->farlevel.sfrcounter == 0)

            // Estimate in active far-end segments only
            && (aec->farlevel.averagelevel > (actThreshold * aec->farlevel.minlevel))
            ) {

            // Subtract noise power
            echo = aec->nearlevel.averagelevel - safety * aec->nearlevel.minlevel;

            // ERL
            dtmp = 10 * (float)log10(aec->farlevel.averagelevel /
                aec->nearlevel.averagelevel + 1e-10f);
            dtmp2 = 10 * (float)log10(aec->farlevel.averagelevel / echo + 1e-10f);

            aec->erl.instant = dtmp;
            if (dtmp > aec->erl.max) {
                aec->erl.max = dtmp;
            }

            if (dtmp < aec->erl.min) {
                aec->erl.min = dtmp;
            }

            aec->erl.counter++;
            aec->erl.sum += dtmp;
            aec->erl.average = aec->erl.sum / aec->erl.counter;

            // Upper mean
            if (dtmp > aec->erl.average) {
                aec->erl.hicounter++;
                aec->erl.hisum += dtmp;
                aec->erl.himean = aec->erl.hisum / aec->erl.hicounter;
            }

            // A_NLP
            dtmp = 10 * (float)log10(aec->nearlevel.averagelevel /
                aec->linoutlevel.averagelevel + 1e-10f);

            // subtract noise power
            suppressedEcho = aec->linoutlevel.averagelevel - safety * aec->linoutlevel.minlevel;

            dtmp2 = 10 * (float)log10(echo / suppressedEcho + 1e-10f);
            dtmp3 = 10 * (float)log10(aec->nearlevel.averagelevel / suppressedEcho + 1e-10f);

            aec->aNlp.instant = dtmp2;
            if (dtmp > aec->aNlp.max) {
                aec->aNlp.max = dtmp;
            }

            if (dtmp < aec->aNlp.min) {
                aec->aNlp.min = dtmp;
            }

            aec->aNlp.counter++;
            aec->aNlp.sum += dtmp;
            aec->aNlp.average = aec->aNlp.sum / aec->aNlp.counter;

            // Upper mean
            if (dtmp > aec->aNlp.average) {
                aec->aNlp.hicounter++;
                aec->aNlp.hisum += dtmp;
                aec->aNlp.himean = aec->aNlp.hisum / aec->aNlp.hicounter;
            }

            // ERLE

            // subtract noise power
            suppressedEcho = aec->nlpoutlevel.averagelevel - safety * aec->nlpoutlevel.minlevel;

            dtmp = 10 * (float)log10(aec->nearlevel.averagelevel /
                aec->nlpoutlevel.averagelevel + 1e-10f);
            dtmp2 = 10 * (float)log10(echo / suppressedEcho + 1e-10f);

            dtmp = dtmp2;
            aec->erle.instant = dtmp;
            if (dtmp > aec->erle.max) {
                aec->erle.max = dtmp;
            }

            if (dtmp < aec->erle.min) {
                aec->erle.min = dtmp;
            }

            aec->erle.counter++;
            aec->erle.sum += dtmp;
            aec->erle.average = aec->erle.sum / aec->erle.counter;

            // Upper mean
            if (dtmp > aec->erle.average) {
                aec->erle.hicounter++;
                aec->erle.hisum += dtmp;
                aec->erle.himean = aec->erle.hisum / aec->erle.hicounter;
            }
        }

        aec->stateCounter = 0;
    }
}

