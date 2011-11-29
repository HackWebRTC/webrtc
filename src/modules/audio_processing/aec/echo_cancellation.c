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
 * Contains the API functions for the AEC.
 */
#include "echo_cancellation.h"

#include <math.h>
#ifdef WEBRTC_AEC_DEBUG_DUMP
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "aec_core.h"
#include "aec_resampler.h"
#include "ring_buffer.h"

#define BUF_SIZE_FRAMES 50 // buffer size (frames)
// Maximum length of resampled signal. Must be an integer multiple of frames
// (ceil(1/(1 + MIN_SKEW)*2) + 1)*FRAME_LEN
// The factor of 2 handles wb, and the + 1 is as a safety margin
#define MAX_RESAMP_LEN (5 * FRAME_LEN)

static const int bufSizeSamp = BUF_SIZE_FRAMES * FRAME_LEN; // buffer size (samples)
static const int sampMsNb = 8; // samples per ms in nb
// Target suppression levels for nlp modes
// log{0.001, 0.00001, 0.00000001}
static const float targetSupp[3] = {-6.9f, -11.5f, -18.4f};
static const float minOverDrive[3] = {1.0f, 2.0f, 5.0f};
static const int initCheck = 42;

#ifdef WEBRTC_AEC_DEBUG_DUMP
static int instance_count = 0;
#endif

typedef struct {
    int delayCtr;
    int sampFreq;
    int splitSampFreq;
    int scSampFreq;
    float sampFactor; // scSampRate / sampFreq
    short nlpMode;
    short autoOnOff;
    short activity;
    short skewMode;
    short bufSizeStart;
    //short bufResetCtr;  // counts number of noncausal frames
    int knownDelay;

    // Stores the last frame added to the farend buffer
    short farendOld[2][FRAME_LEN];
    short initFlag; // indicates if AEC has been initialized

    // Variables used for averaging far end buffer size
    short counter;
    short sum;
    short firstVal;
    short checkBufSizeCtr;

    // Variables used for delay shifts
    short msInSndCardBuf;
    short filtDelay;
    int timeForDelayChange;
    int ECstartup;
    int checkBuffSize;
    int delayChange;
    short lastDelayDiff;

#ifdef WEBRTC_AEC_DEBUG_DUMP
    FILE *bufFile;
    FILE *delayFile;
    FILE *skewFile;
#endif

    // Structures
    void *farendBuf;
    void *resampler;

    int skewFrCtr;
    int resample; // if the skew is small enough we don't resample
    int highSkewCtr;
    float skew;

    int lastError;

    aec_t *aec;
} aecpc_t;

// Estimates delay to set the position of the farend buffer read pointer
// (controlled by knownDelay)
static int EstBufDelay(aecpc_t *aecInst, short msInSndCardBuf);

// Stuffs the farend buffer if the estimated delay is too large
static int DelayComp(aecpc_t *aecInst);

WebRtc_Word32 WebRtcAec_Create(void **aecInst)
{
    aecpc_t *aecpc;
    if (aecInst == NULL) {
        return -1;
    }

    aecpc = malloc(sizeof(aecpc_t));
    *aecInst = aecpc;
    if (aecpc == NULL) {
        return -1;
    }

    if (WebRtcAec_CreateAec(&aecpc->aec) == -1) {
        WebRtcAec_Free(aecpc);
        aecpc = NULL;
        return -1;
    }

    if (WebRtcApm_CreateBuffer(&aecpc->farendBuf, bufSizeSamp) == -1) {
        WebRtcAec_Free(aecpc);
        aecpc = NULL;
        return -1;
    }

    if (WebRtcAec_CreateResampler(&aecpc->resampler) == -1) {
        WebRtcAec_Free(aecpc);
        aecpc = NULL;
        return -1;
    }

    aecpc->initFlag = 0;
    aecpc->lastError = 0;

#ifdef WEBRTC_AEC_DEBUG_DUMP
    {
      char filename[64];
      sprintf(filename, "aec_far%d.pcm", instance_count);
      aecpc->aec->farFile = fopen(filename, "wb");
      sprintf(filename, "aec_near%d.pcm", instance_count);
      aecpc->aec->nearFile = fopen(filename, "wb");
      sprintf(filename, "aec_out%d.pcm", instance_count);
      aecpc->aec->outFile = fopen(filename, "wb");
      sprintf(filename, "aec_out_linear%d.pcm", instance_count);
      aecpc->aec->outLinearFile = fopen(filename, "wb");
      sprintf(filename, "aec_buf%d.dat", instance_count);
      aecpc->bufFile = fopen(filename, "wb");
      sprintf(filename, "aec_skew%d.dat", instance_count);
      aecpc->skewFile = fopen(filename, "wb");
      sprintf(filename, "aec_delay%d.dat", instance_count);
      aecpc->delayFile = fopen(filename, "wb");
      instance_count++;
    }
#endif

    return 0;
}

WebRtc_Word32 WebRtcAec_Free(void *aecInst)
{
    aecpc_t *aecpc = aecInst;

    if (aecpc == NULL) {
        return -1;
    }

#ifdef WEBRTC_AEC_DEBUG_DUMP
    fclose(aecpc->aec->farFile);
    fclose(aecpc->aec->nearFile);
    fclose(aecpc->aec->outFile);
    fclose(aecpc->aec->outLinearFile);
    fclose(aecpc->bufFile);
    fclose(aecpc->skewFile);
    fclose(aecpc->delayFile);
#endif

    WebRtcAec_FreeAec(aecpc->aec);
    WebRtcApm_FreeBuffer(aecpc->farendBuf);
    WebRtcAec_FreeResampler(aecpc->resampler);
    free(aecpc);

    return 0;
}

WebRtc_Word32 WebRtcAec_Init(void *aecInst, WebRtc_Word32 sampFreq, WebRtc_Word32 scSampFreq)
{
    aecpc_t *aecpc = aecInst;
    AecConfig aecConfig;

    if (aecpc == NULL) {
        return -1;
    }

    if (sampFreq != 8000 && sampFreq != 16000  && sampFreq != 32000) {
        aecpc->lastError = AEC_BAD_PARAMETER_ERROR;
        return -1;
    }
    aecpc->sampFreq = sampFreq;

    if (scSampFreq < 1 || scSampFreq > 96000) {
        aecpc->lastError = AEC_BAD_PARAMETER_ERROR;
        return -1;
    }
    aecpc->scSampFreq = scSampFreq;

    // Initialize echo canceller core
    if (WebRtcAec_InitAec(aecpc->aec, aecpc->sampFreq) == -1) {
        aecpc->lastError = AEC_UNSPECIFIED_ERROR;
        return -1;
    }

    // Initialize farend buffer
    if (WebRtcApm_InitBuffer(aecpc->farendBuf) == -1) {
        aecpc->lastError = AEC_UNSPECIFIED_ERROR;
        return -1;
    }

    if (WebRtcAec_InitResampler(aecpc->resampler, aecpc->scSampFreq) == -1) {
        aecpc->lastError = AEC_UNSPECIFIED_ERROR;
        return -1;
    }

    aecpc->initFlag = initCheck;  // indicates that initialization has been done

    if (aecpc->sampFreq == 32000) {
        aecpc->splitSampFreq = 16000;
    }
    else {
        aecpc->splitSampFreq = sampFreq;
    }

    aecpc->skewFrCtr = 0;
    aecpc->activity = 0;

    aecpc->delayChange = 1;
    aecpc->delayCtr = 0;

    aecpc->sum = 0;
    aecpc->counter = 0;
    aecpc->checkBuffSize = 1;
    aecpc->firstVal = 0;

    aecpc->ECstartup = 1;
    aecpc->bufSizeStart = 0;
    aecpc->checkBufSizeCtr = 0;
    aecpc->filtDelay = 0;
    aecpc->timeForDelayChange =0;
    aecpc->knownDelay = 0;
    aecpc->lastDelayDiff = 0;

    aecpc->skew = 0;
    aecpc->resample = kAecFalse;
    aecpc->highSkewCtr = 0;
    aecpc->sampFactor = (aecpc->scSampFreq * 1.0f) / aecpc->splitSampFreq;

    memset(&aecpc->farendOld[0][0], 0, 160);

    // Default settings.
    aecConfig.nlpMode = kAecNlpModerate;
    aecConfig.skewMode = kAecFalse;
    aecConfig.metricsMode = kAecFalse;
    aecConfig.delay_logging = kAecFalse;

    if (WebRtcAec_set_config(aecpc, aecConfig) == -1) {
        aecpc->lastError = AEC_UNSPECIFIED_ERROR;
        return -1;
    }

    return 0;
}

// only buffer L band for farend
WebRtc_Word32 WebRtcAec_BufferFarend(void *aecInst, const WebRtc_Word16 *farend,
    WebRtc_Word16 nrOfSamples)
{
    aecpc_t *aecpc = aecInst;
    WebRtc_Word32 retVal = 0;
    short newNrOfSamples;
    short newFarend[MAX_RESAMP_LEN];
    float skew;

    if (aecpc == NULL) {
        return -1;
    }

    if (farend == NULL) {
        aecpc->lastError = AEC_NULL_POINTER_ERROR;
        return -1;
    }

    if (aecpc->initFlag != initCheck) {
        aecpc->lastError = AEC_UNINITIALIZED_ERROR;
        return -1;
    }

    // number of samples == 160 for SWB input
    if (nrOfSamples != 80 && nrOfSamples != 160) {
        aecpc->lastError = AEC_BAD_PARAMETER_ERROR;
        return -1;
    }

    skew = aecpc->skew;

    // TODO: Is this really a good idea?
    if (!aecpc->ECstartup) {
        DelayComp(aecpc);
    }

    if (aecpc->skewMode == kAecTrue && aecpc->resample == kAecTrue) {
        // Resample and get a new number of samples
        newNrOfSamples = WebRtcAec_ResampleLinear(aecpc->resampler,
                                                  farend,
                                                  nrOfSamples,
                                                  skew,
                                                  newFarend);
        WebRtcApm_WriteBuffer(aecpc->farendBuf, newFarend, newNrOfSamples);
    }
    else {
        WebRtcApm_WriteBuffer(aecpc->farendBuf, farend, nrOfSamples);
    }

    return retVal;
}

WebRtc_Word32 WebRtcAec_Process(void *aecInst, const WebRtc_Word16 *nearend,
    const WebRtc_Word16 *nearendH, WebRtc_Word16 *out, WebRtc_Word16 *outH,
    WebRtc_Word16 nrOfSamples, WebRtc_Word16 msInSndCardBuf, WebRtc_Word32 skew)
{
    aecpc_t *aecpc = aecInst;
    WebRtc_Word32 retVal = 0;
    short i;
    short farend[FRAME_LEN];
    short nmbrOfFilledBuffers;
    short nBlocks10ms;
    short nFrames;
    // Limit resampling to doubling/halving of signal
    const float minSkewEst = -0.5f;
    const float maxSkewEst = 1.0f;

    if (aecpc == NULL) {
        return -1;
    }

    if (nearend == NULL) {
        aecpc->lastError = AEC_NULL_POINTER_ERROR;
        return -1;
    }

    if (out == NULL) {
        aecpc->lastError = AEC_NULL_POINTER_ERROR;
        return -1;
    }

    if (aecpc->initFlag != initCheck) {
        aecpc->lastError = AEC_UNINITIALIZED_ERROR;
        return -1;
    }

    // number of samples == 160 for SWB input
    if (nrOfSamples != 80 && nrOfSamples != 160) {
        aecpc->lastError = AEC_BAD_PARAMETER_ERROR;
        return -1;
    }

    // Check for valid pointers based on sampling rate
    if (aecpc->sampFreq == 32000 && nearendH == NULL) {
       aecpc->lastError = AEC_NULL_POINTER_ERROR;
       return -1;
    }

    if (msInSndCardBuf < 0) {
        msInSndCardBuf = 0;
        aecpc->lastError = AEC_BAD_PARAMETER_WARNING;
        retVal = -1;
    }
    else if (msInSndCardBuf > 500) {
        msInSndCardBuf = 500;
        aecpc->lastError = AEC_BAD_PARAMETER_WARNING;
        retVal = -1;
    }
    // TODO(andrew): we need to investigate if this +10 is really wanted.
    msInSndCardBuf += 10;
    aecpc->msInSndCardBuf = msInSndCardBuf;

    if (aecpc->skewMode == kAecTrue) {
        if (aecpc->skewFrCtr < 25) {
            aecpc->skewFrCtr++;
        }
        else {
            retVal = WebRtcAec_GetSkew(aecpc->resampler, skew, &aecpc->skew);
            if (retVal == -1) {
                aecpc->skew = 0;
                aecpc->lastError = AEC_BAD_PARAMETER_WARNING;
            }

            aecpc->skew /= aecpc->sampFactor*nrOfSamples;

            if (aecpc->skew < 1.0e-3 && aecpc->skew > -1.0e-3) {
                aecpc->resample = kAecFalse;
            }
            else {
                aecpc->resample = kAecTrue;
            }

            if (aecpc->skew < minSkewEst) {
                aecpc->skew = minSkewEst;
            }
            else if (aecpc->skew > maxSkewEst) {
                aecpc->skew = maxSkewEst;
            }

#ifdef WEBRTC_AEC_DEBUG_DUMP
            fwrite(&aecpc->skew, sizeof(aecpc->skew), 1, aecpc->skewFile);
#endif
        }
    }

    nFrames = nrOfSamples / FRAME_LEN;
    nBlocks10ms = nFrames / aecpc->aec->mult;

    if (aecpc->ECstartup) {
        if (nearend != out) {
            // Only needed if they don't already point to the same place.
            memcpy(out, nearend, sizeof(short) * nrOfSamples);
        }
        nmbrOfFilledBuffers = WebRtcApm_get_buffer_size(aecpc->farendBuf) / FRAME_LEN;

        // The AEC is in the start up mode
        // AEC is disabled until the soundcard buffer and farend buffers are OK

        // Mechanism to ensure that the soundcard buffer is reasonably stable.
        if (aecpc->checkBuffSize) {

            aecpc->checkBufSizeCtr++;
            // Before we fill up the far end buffer we require the amount of data on the
            // sound card to be stable (+/-8 ms) compared to the first value. This
            // comparison is made during the following 4 consecutive frames. If it seems
            // to be stable then we start to fill up the far end buffer.

            if (aecpc->counter == 0) {
                aecpc->firstVal = aecpc->msInSndCardBuf;
                aecpc->sum = 0;
            }

            if (abs(aecpc->firstVal - aecpc->msInSndCardBuf) <
                WEBRTC_SPL_MAX(0.2 * aecpc->msInSndCardBuf, sampMsNb)) {
                aecpc->sum += aecpc->msInSndCardBuf;
                aecpc->counter++;
            }
            else {
                aecpc->counter = 0;
            }

            if (aecpc->counter*nBlocks10ms >= 6) {
                // The farend buffer size is determined in blocks of 80 samples
                // Use 75% of the average value of the soundcard buffer
                aecpc->bufSizeStart = WEBRTC_SPL_MIN((int) (0.75 * (aecpc->sum *
                    aecpc->aec->mult) / (aecpc->counter * 10)), BUF_SIZE_FRAMES);
                // buffersize has now been determined
                aecpc->checkBuffSize = 0;
            }

            if (aecpc->checkBufSizeCtr * nBlocks10ms > 50) {
                // for really bad sound cards, don't disable echocanceller for more than 0.5 sec
                aecpc->bufSizeStart = WEBRTC_SPL_MIN((int) (0.75 * (aecpc->msInSndCardBuf *
                    aecpc->aec->mult) / 10), BUF_SIZE_FRAMES);
                aecpc->checkBuffSize = 0;
            }
        }

        // if checkBuffSize changed in the if-statement above
        if (!aecpc->checkBuffSize) {
            // soundcard buffer is now reasonably stable
            // When the far end buffer is filled with approximately the same amount of
            // data as the amount on the sound card we end the start up phase and start
            // to cancel echoes.

            if (nmbrOfFilledBuffers == aecpc->bufSizeStart) {
                aecpc->ECstartup = 0;  // Enable the AEC
            }
            else if (nmbrOfFilledBuffers > aecpc->bufSizeStart) {
                WebRtcApm_FlushBuffer(aecpc->farendBuf, WebRtcApm_get_buffer_size(aecpc->farendBuf) -
                    aecpc->bufSizeStart * FRAME_LEN);
                aecpc->ECstartup = 0;
            }
        }

    }
    else {
        // AEC is enabled

        // Note only 1 block supported for nb and 2 blocks for wb
        for (i = 0; i < nFrames; i++) {
            nmbrOfFilledBuffers = WebRtcApm_get_buffer_size(aecpc->farendBuf) / FRAME_LEN;

            // Check that there is data in the far end buffer
            if (nmbrOfFilledBuffers > 0) {
                // Get the next 80 samples from the farend buffer
                WebRtcApm_ReadBuffer(aecpc->farendBuf, farend, FRAME_LEN);

                // Always store the last frame for use when we run out of data
                memcpy(&(aecpc->farendOld[i][0]), farend, FRAME_LEN * sizeof(short));
            }
            else {
                // We have no data so we use the last played frame
                memcpy(farend, &(aecpc->farendOld[i][0]), FRAME_LEN * sizeof(short));
            }

            // Call buffer delay estimator when all data is extracted,
            // i.e. i = 0 for NB and i = 1 for WB or SWB
            if ((i == 0 && aecpc->splitSampFreq == 8000) ||
                    (i == 1 && (aecpc->splitSampFreq == 16000))) {
                EstBufDelay(aecpc, aecpc->msInSndCardBuf);
            }

            // Call the AEC
           WebRtcAec_ProcessFrame(aecpc->aec, farend, &nearend[FRAME_LEN * i], &nearendH[FRAME_LEN * i],
               &out[FRAME_LEN * i], &outH[FRAME_LEN * i], aecpc->knownDelay);
        }
    }

#ifdef WEBRTC_AEC_DEBUG_DUMP
    {
        short msInAECBuf = WebRtcApm_get_buffer_size(aecpc->farendBuf) /
            (sampMsNb*aecpc->aec->mult);
        fwrite(&msInAECBuf, 2, 1, aecpc->bufFile);
        fwrite(&(aecpc->knownDelay), sizeof(aecpc->knownDelay), 1, aecpc->delayFile);
    }
#endif

    return retVal;
}

WebRtc_Word32 WebRtcAec_set_config(void *aecInst, AecConfig config)
{
    aecpc_t *aecpc = aecInst;

    if (aecpc == NULL) {
        return -1;
    }

    if (aecpc->initFlag != initCheck) {
        aecpc->lastError = AEC_UNINITIALIZED_ERROR;
        return -1;
    }

    if (config.skewMode != kAecFalse && config.skewMode != kAecTrue) {
        aecpc->lastError = AEC_BAD_PARAMETER_ERROR;
        return -1;
    }
    aecpc->skewMode = config.skewMode;

    if (config.nlpMode != kAecNlpConservative && config.nlpMode !=
            kAecNlpModerate && config.nlpMode != kAecNlpAggressive) {
        aecpc->lastError = AEC_BAD_PARAMETER_ERROR;
        return -1;
    }
    aecpc->nlpMode = config.nlpMode;
    aecpc->aec->targetSupp = targetSupp[aecpc->nlpMode];
    aecpc->aec->minOverDrive = minOverDrive[aecpc->nlpMode];

    if (config.metricsMode != kAecFalse && config.metricsMode != kAecTrue) {
        aecpc->lastError = AEC_BAD_PARAMETER_ERROR;
        return -1;
    }
    aecpc->aec->metricsMode = config.metricsMode;
    if (aecpc->aec->metricsMode == kAecTrue) {
        WebRtcAec_InitMetrics(aecpc->aec);
    }

  if (config.delay_logging != kAecFalse && config.delay_logging != kAecTrue) {
    aecpc->lastError = AEC_BAD_PARAMETER_ERROR;
    return -1;
  }
  aecpc->aec->delay_logging_enabled = config.delay_logging;
  if (aecpc->aec->delay_logging_enabled == kAecTrue) {
    memset(aecpc->aec->delay_histogram, 0, sizeof(aecpc->aec->delay_histogram));
  }

    return 0;
}

WebRtc_Word32 WebRtcAec_get_config(void *aecInst, AecConfig *config)
{
    aecpc_t *aecpc = aecInst;

    if (aecpc == NULL) {
        return -1;
    }

    if (config == NULL) {
        aecpc->lastError = AEC_NULL_POINTER_ERROR;
        return -1;
    }

    if (aecpc->initFlag != initCheck) {
        aecpc->lastError = AEC_UNINITIALIZED_ERROR;
        return -1;
    }

    config->nlpMode = aecpc->nlpMode;
    config->skewMode = aecpc->skewMode;
    config->metricsMode = aecpc->aec->metricsMode;
    config->delay_logging = aecpc->aec->delay_logging_enabled;

    return 0;
}

WebRtc_Word32 WebRtcAec_get_echo_status(void *aecInst, WebRtc_Word16 *status)
{
    aecpc_t *aecpc = aecInst;

    if (aecpc == NULL) {
        return -1;
    }

    if (status == NULL) {
        aecpc->lastError = AEC_NULL_POINTER_ERROR;
        return -1;
    }

    if (aecpc->initFlag != initCheck) {
        aecpc->lastError = AEC_UNINITIALIZED_ERROR;
        return -1;
    }

    *status = aecpc->aec->echoState;

    return 0;
}

WebRtc_Word32 WebRtcAec_GetMetrics(void *aecInst, AecMetrics *metrics)
{
    const float upweight = 0.7f;
    float dtmp;
    short stmp;
    aecpc_t *aecpc = aecInst;

    if (aecpc == NULL) {
        return -1;
    }

    if (metrics == NULL) {
        aecpc->lastError = AEC_NULL_POINTER_ERROR;
        return -1;
    }

    if (aecpc->initFlag != initCheck) {
        aecpc->lastError = AEC_UNINITIALIZED_ERROR;
        return -1;
    }

    // ERL
    metrics->erl.instant = (short) aecpc->aec->erl.instant;

    if ((aecpc->aec->erl.himean > offsetLevel) && (aecpc->aec->erl.average > offsetLevel)) {
    // Use a mix between regular average and upper part average
        dtmp = upweight * aecpc->aec->erl.himean + (1 - upweight) * aecpc->aec->erl.average;
        metrics->erl.average = (short) dtmp;
    }
    else {
        metrics->erl.average = offsetLevel;
    }

    metrics->erl.max = (short) aecpc->aec->erl.max;

    if (aecpc->aec->erl.min < (offsetLevel * (-1))) {
        metrics->erl.min = (short) aecpc->aec->erl.min;
    }
    else {
        metrics->erl.min = offsetLevel;
    }

    // ERLE
    metrics->erle.instant = (short) aecpc->aec->erle.instant;

    if ((aecpc->aec->erle.himean > offsetLevel) && (aecpc->aec->erle.average > offsetLevel)) {
        // Use a mix between regular average and upper part average
        dtmp =  upweight * aecpc->aec->erle.himean + (1 - upweight) * aecpc->aec->erle.average;
        metrics->erle.average = (short) dtmp;
    }
    else {
        metrics->erle.average = offsetLevel;
    }

    metrics->erle.max = (short) aecpc->aec->erle.max;

    if (aecpc->aec->erle.min < (offsetLevel * (-1))) {
        metrics->erle.min = (short) aecpc->aec->erle.min;
    } else {
        metrics->erle.min = offsetLevel;
    }

    // RERL
    if ((metrics->erl.average > offsetLevel) && (metrics->erle.average > offsetLevel)) {
        stmp = metrics->erl.average + metrics->erle.average;
    }
    else {
        stmp = offsetLevel;
    }
    metrics->rerl.average = stmp;

    // No other statistics needed, but returned for completeness
    metrics->rerl.instant = stmp;
    metrics->rerl.max = stmp;
    metrics->rerl.min = stmp;

    // A_NLP
    metrics->aNlp.instant = (short) aecpc->aec->aNlp.instant;

    if ((aecpc->aec->aNlp.himean > offsetLevel) && (aecpc->aec->aNlp.average > offsetLevel)) {
        // Use a mix between regular average and upper part average
        dtmp =  upweight * aecpc->aec->aNlp.himean + (1 - upweight) * aecpc->aec->aNlp.average;
        metrics->aNlp.average = (short) dtmp;
    }
    else {
        metrics->aNlp.average = offsetLevel;
    }

    metrics->aNlp.max = (short) aecpc->aec->aNlp.max;

    if (aecpc->aec->aNlp.min < (offsetLevel * (-1))) {
        metrics->aNlp.min = (short) aecpc->aec->aNlp.min;
    }
    else {
        metrics->aNlp.min = offsetLevel;
    }

    return 0;
}

int WebRtcAec_GetDelayMetrics(void* handle, int* median, int* std) {
  aecpc_t* self = handle;
  int i = 0;
  int delay_values = 0;
  int num_delay_values = 0;
  int my_median = 0;
  const int kMsPerBlock = (PART_LEN * 1000) / self->splitSampFreq;
  float l1_norm = 0;

  if (self == NULL) {
    return -1;
  }
  if (median == NULL) {
    self->lastError = AEC_NULL_POINTER_ERROR;
    return -1;
  }
  if (std == NULL) {
    self->lastError = AEC_NULL_POINTER_ERROR;
    return -1;
  }
  if (self->initFlag != initCheck) {
    self->lastError = AEC_UNINITIALIZED_ERROR;
    return -1;
  }
  if (self->aec->delay_logging_enabled == 0) {
    // Logging disabled
    self->lastError = AEC_UNSUPPORTED_FUNCTION_ERROR;
    return -1;
  }

  // Get number of delay values since last update
  for (i = 0; i < kHistorySizeBlocks; i++) {
    num_delay_values += self->aec->delay_histogram[i];
  }
  if (num_delay_values == 0) {
    // We have no new delay value data. Even though -1 is a valid estimate, it
    // will practically never be used since multiples of |kMsPerBlock| will
    // always be returned.
    *median = -1;
    *std = -1;
    return 0;
  }

  delay_values = num_delay_values >> 1; // Start value for median count down
  // Get median of delay values since last update
  for (i = 0; i < kHistorySizeBlocks; i++) {
    delay_values -= self->aec->delay_histogram[i];
    if (delay_values < 0) {
      my_median = i;
      break;
    }
  }
  // Account for lookahead.
  *median = (my_median - kLookaheadBlocks) * kMsPerBlock;

  // Calculate the L1 norm, with median value as central moment
  for (i = 0; i < kHistorySizeBlocks; i++) {
    l1_norm += (float) (fabs(i - my_median) * self->aec->delay_histogram[i]);
  }
  *std = (int) (l1_norm / (float) num_delay_values + 0.5f) * kMsPerBlock;

  // Reset histogram
  memset(self->aec->delay_histogram, 0, sizeof(self->aec->delay_histogram));

  return 0;
}

WebRtc_Word32 WebRtcAec_get_version(WebRtc_Word8 *versionStr, WebRtc_Word16 len)
{
    const char version[] = "AEC 2.5.0";
    const short versionLen = (short)strlen(version) + 1; // +1 for null-termination

    if (versionStr == NULL) {
        return -1;
    }

    if (versionLen > len) {
        return -1;
    }

    strncpy(versionStr, version, versionLen);
    return 0;
}

WebRtc_Word32 WebRtcAec_get_error_code(void *aecInst)
{
    aecpc_t *aecpc = aecInst;

    if (aecpc == NULL) {
        return -1;
    }

    return aecpc->lastError;
}

static int EstBufDelay(aecpc_t *aecpc, short msInSndCardBuf)
{
    short delayNew, nSampFar, nSampSndCard;
    short diff;

    nSampFar = WebRtcApm_get_buffer_size(aecpc->farendBuf);
    nSampSndCard = msInSndCardBuf * sampMsNb * aecpc->aec->mult;

    delayNew = nSampSndCard - nSampFar;

    // Account for resampling frame delay
    if (aecpc->skewMode == kAecTrue && aecpc->resample == kAecTrue) {
        delayNew -= kResamplingDelay;
    }

    if (delayNew < FRAME_LEN) {
        WebRtcApm_FlushBuffer(aecpc->farendBuf, FRAME_LEN);
        delayNew += FRAME_LEN;
    }

    aecpc->filtDelay = WEBRTC_SPL_MAX(0, (short)(0.8*aecpc->filtDelay + 0.2*delayNew));

    diff = aecpc->filtDelay - aecpc->knownDelay;
    if (diff > 224) {
        if (aecpc->lastDelayDiff < 96) {
            aecpc->timeForDelayChange = 0;
        }
        else {
            aecpc->timeForDelayChange++;
        }
    }
    else if (diff < 96 && aecpc->knownDelay > 0) {
        if (aecpc->lastDelayDiff > 224) {
            aecpc->timeForDelayChange = 0;
        }
        else {
            aecpc->timeForDelayChange++;
        }
    }
    else {
        aecpc->timeForDelayChange = 0;
    }
    aecpc->lastDelayDiff = diff;

    if (aecpc->timeForDelayChange > 25) {
        aecpc->knownDelay = WEBRTC_SPL_MAX((int)aecpc->filtDelay - 160, 0);
    }
    return 0;
}

static int DelayComp(aecpc_t *aecpc)
{
    int nSampFar, nSampSndCard, delayNew, nSampAdd;
    const int maxStuffSamp = 10 * FRAME_LEN;

    nSampFar = WebRtcApm_get_buffer_size(aecpc->farendBuf);
    nSampSndCard = aecpc->msInSndCardBuf * sampMsNb * aecpc->aec->mult;
    delayNew = nSampSndCard - nSampFar;

    // Account for resampling frame delay
    if (aecpc->skewMode == kAecTrue && aecpc->resample == kAecTrue) {
        delayNew -= kResamplingDelay;
    }

    if (delayNew > FAR_BUF_LEN - FRAME_LEN*aecpc->aec->mult) {
        // The difference of the buffersizes is larger than the maximum
        // allowed known delay. Compensate by stuffing the buffer.
        nSampAdd = (int)(WEBRTC_SPL_MAX((int)(0.5 * nSampSndCard - nSampFar),
                    FRAME_LEN));
        nSampAdd = WEBRTC_SPL_MIN(nSampAdd, maxStuffSamp);

        WebRtcApm_StuffBuffer(aecpc->farendBuf, nSampAdd);
        aecpc->delayChange = 1; // the delay needs to be updated
    }

    return 0;
}
