/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_processing/ns/include/noise_suppression.h"
#include "webrtc/modules/audio_processing/ns/ns_core.h"
#include "webrtc/modules/audio_processing/ns/windows_private.h"
#include "webrtc/modules/audio_processing/utility/fft4g.h"

// Set Feature Extraction Parameters
void WebRtcNs_set_feature_extraction_parameters(NSinst_t* inst) {
  // bin size of histogram
  inst->featureExtractionParams.binSizeLrt = 0.1f;
  inst->featureExtractionParams.binSizeSpecFlat = 0.05f;
  inst->featureExtractionParams.binSizeSpecDiff = 0.1f;

  // range of histogram over which lrt threshold is computed
  inst->featureExtractionParams.rangeAvgHistLrt = 1.f;

  // scale parameters: multiply dominant peaks of the histograms by scale factor
  // to obtain thresholds for prior model
  inst->featureExtractionParams.factor1ModelPars =
      1.2f;  // for lrt and spectral diff
  inst->featureExtractionParams.factor2ModelPars =
      0.9f;  // for spectral_flatness:
  // used when noise is flatter than speech

  // peak limit for spectral flatness (varies between 0 and 1)
  inst->featureExtractionParams.thresPosSpecFlat = 0.6f;

  // limit on spacing of two highest peaks in histogram: spacing determined by
  // bin size
  inst->featureExtractionParams.limitPeakSpacingSpecFlat =
      2 * inst->featureExtractionParams.binSizeSpecFlat;
  inst->featureExtractionParams.limitPeakSpacingSpecDiff =
      2 * inst->featureExtractionParams.binSizeSpecDiff;

  // limit on relevance of second peak:
  inst->featureExtractionParams.limitPeakWeightsSpecFlat = 0.5f;
  inst->featureExtractionParams.limitPeakWeightsSpecDiff = 0.5f;

  // fluctuation limit of lrt feature
  inst->featureExtractionParams.thresFluctLrt = 0.05f;

  // limit on the max and min values for the feature thresholds
  inst->featureExtractionParams.maxLrt = 1.f;
  inst->featureExtractionParams.minLrt = 0.2f;

  inst->featureExtractionParams.maxSpecFlat = 0.95f;
  inst->featureExtractionParams.minSpecFlat = 0.1f;

  inst->featureExtractionParams.maxSpecDiff = 1.f;
  inst->featureExtractionParams.minSpecDiff = 0.16f;

  // criteria of weight of histogram peak  to accept/reject feature
  inst->featureExtractionParams.thresWeightSpecFlat =
      (int)(0.3 * (inst->modelUpdatePars[1]));  // for spectral flatness
  inst->featureExtractionParams.thresWeightSpecDiff =
      (int)(0.3 * (inst->modelUpdatePars[1]));  // for spectral difference
}

// Initialize state
int WebRtcNs_InitCore(NSinst_t* inst, uint32_t fs) {
  int i;
  // We only support 10ms frames

  // check for valid pointer
  if (inst == NULL) {
    return -1;
  }

  // Initialization of struct
  if (fs == 8000 || fs == 16000 || fs == 32000) {
    inst->fs = fs;
  } else {
    return -1;
  }
  inst->windShift = 0;
  if (fs == 8000) {
    // We only support 10ms frames
    inst->blockLen = 80;
    inst->anaLen = 128;
    inst->window = kBlocks80w128;
  } else if (fs == 16000) {
    // We only support 10ms frames
    inst->blockLen = 160;
    inst->anaLen = 256;
    inst->window = kBlocks160w256;
  } else if (fs == 32000) {
    // We only support 10ms frames
    inst->blockLen = 160;
    inst->anaLen = 256;
    inst->window = kBlocks160w256;
  }
  inst->magnLen = inst->anaLen / 2 + 1;  // Number of frequency bins

  // Initialize fft work arrays.
  inst->ip[0] = 0;  // Setting this triggers initialization.
  memset(inst->dataBuf, 0, sizeof(float) * ANAL_BLOCKL_MAX);
  WebRtc_rdft(inst->anaLen, 1, inst->dataBuf, inst->ip, inst->wfft);

  memset(inst->analyzeBuf, 0, sizeof(float) * ANAL_BLOCKL_MAX);
  memset(inst->dataBuf, 0, sizeof(float) * ANAL_BLOCKL_MAX);
  memset(inst->syntBuf, 0, sizeof(float) * ANAL_BLOCKL_MAX);

  // for HB processing
  memset(inst->dataBufHB, 0, sizeof(float) * ANAL_BLOCKL_MAX);

  // for quantile noise estimation
  memset(inst->quantile, 0, sizeof(float) * HALF_ANAL_BLOCKL);
  for (i = 0; i < SIMULT * HALF_ANAL_BLOCKL; i++) {
    inst->lquantile[i] = 8.f;
    inst->density[i] = 0.3f;
  }

  for (i = 0; i < SIMULT; i++) {
    inst->counter[i] =
        (int)floor((float)(END_STARTUP_LONG * (i + 1)) / (float)SIMULT);
  }

  inst->updates = 0;

  // Wiener filter initialization
  for (i = 0; i < HALF_ANAL_BLOCKL; i++) {
    inst->smooth[i] = 1.f;
  }

  // Set the aggressiveness: default
  inst->aggrMode = 0;

  // initialize variables for new method
  inst->priorSpeechProb = 0.5f;  // prior prob for speech/noise
  // previous analyze mag spectrum
  memset(inst->magnPrevAnalyze, 0, sizeof(float) * HALF_ANAL_BLOCKL);
  // previous process mag spectrum
  memset(inst->magnPrevProcess, 0, sizeof(float) * HALF_ANAL_BLOCKL);
  // current noise-spectrum
  memset(inst->noise, 0, sizeof(float) * HALF_ANAL_BLOCKL);
  // previous noise-spectrum
  memset(inst->noisePrev, 0, sizeof(float) * HALF_ANAL_BLOCKL);
  // conservative noise spectrum estimate
  memset(inst->magnAvgPause, 0, sizeof(float) * HALF_ANAL_BLOCKL);
  // for estimation of HB in second pass
  memset(inst->speechProb, 0, sizeof(float) * HALF_ANAL_BLOCKL);
  // initial average mag spectrum
  memset(inst->initMagnEst, 0, sizeof(float) * HALF_ANAL_BLOCKL);
  for (i = 0; i < HALF_ANAL_BLOCKL; i++) {
    inst->logLrtTimeAvg[i] =
        LRT_FEATURE_THR;                 // smooth LR ratio (same as threshold)
  }

  // feature quantities
  inst->featureData[0] =
      SF_FEATURE_THR;  // spectral flatness (start on threshold)
  inst->featureData[1] = 0.f;  // spectral entropy: not used in this version
  inst->featureData[2] = 0.f;  // spectral variance: not used in this version
  inst->featureData[3] =
      LRT_FEATURE_THR;  // average lrt factor (start on threshold)
  inst->featureData[4] =
      SF_FEATURE_THR;  // spectral template diff (start on threshold)
  inst->featureData[5] = 0.f;  // normalization for spectral-diff
  inst->featureData[6] =
      0.f;  // window time-average of input magnitude spectrum

  // histogram quantities: used to estimate/update thresholds for features
  memset(inst->histLrt, 0, sizeof(int) * HIST_PAR_EST);
  memset(inst->histSpecFlat, 0, sizeof(int) * HIST_PAR_EST);
  memset(inst->histSpecDiff, 0, sizeof(int) * HIST_PAR_EST);


  inst->blockInd = -1;  // frame counter
  inst->priorModelPars[0] =
      LRT_FEATURE_THR;             // default threshold for lrt feature
  inst->priorModelPars[1] = 0.5f;  // threshold for spectral flatness:
  // determined on-line
  inst->priorModelPars[2] = 1.f;  // sgn_map par for spectral measure:
  // 1 for flatness measure
  inst->priorModelPars[3] = 0.5f;  // threshold for template-difference feature:
  // determined on-line
  inst->priorModelPars[4] = 1.f;  // default weighting parameter for lrt feature
  inst->priorModelPars[5] = 0.f;  // default weighting parameter for
  // spectral flatness feature
  inst->priorModelPars[6] = 0.f;  // default weighting parameter for
  // spectral difference feature

  inst->modelUpdatePars[0] = 2;  // update flag for parameters:
  // 0 no update, 1=update once, 2=update every window
  inst->modelUpdatePars[1] = 500;  // window for update
  inst->modelUpdatePars[2] =
      0;  // counter for update of conservative noise spectrum
  // counter if the feature thresholds are updated during the sequence
  inst->modelUpdatePars[3] = inst->modelUpdatePars[1];

  inst->signalEnergy = 0.0;
  inst->sumMagn = 0.0;
  inst->whiteNoiseLevel = 0.0;
  inst->pinkNoiseNumerator = 0.0;
  inst->pinkNoiseExp = 0.0;

  WebRtcNs_set_feature_extraction_parameters(inst);

  // default mode
  WebRtcNs_set_policy_core(inst, 0);

  inst->initFlag = 1;
  return 0;
}

int WebRtcNs_set_policy_core(NSinst_t* inst, int mode) {
  // allow for modes:0,1,2,3
  if (mode < 0 || mode > 3) {
    return (-1);
  }

  inst->aggrMode = mode;
  if (mode == 0) {
    inst->overdrive = 1.f;
    inst->denoiseBound = 0.5f;
    inst->gainmap = 0;
  } else if (mode == 1) {
    // inst->overdrive = 1.25f;
    inst->overdrive = 1.f;
    inst->denoiseBound = 0.25f;
    inst->gainmap = 1;
  } else if (mode == 2) {
    // inst->overdrive = 1.25f;
    inst->overdrive = 1.1f;
    inst->denoiseBound = 0.125f;
    inst->gainmap = 1;
  } else if (mode == 3) {
    // inst->overdrive = 1.3f;
    inst->overdrive = 1.25f;
    inst->denoiseBound = 0.09f;
    inst->gainmap = 1;
  }
  return 0;
}

// Estimate noise
void WebRtcNs_NoiseEstimation(NSinst_t* inst, float* magn, float* noise) {
  int i, s, offset;
  float lmagn[HALF_ANAL_BLOCKL], delta;

  if (inst->updates < END_STARTUP_LONG) {
    inst->updates++;
  }

  for (i = 0; i < inst->magnLen; i++) {
    lmagn[i] = (float)log(magn[i]);
  }

  // loop over simultaneous estimates
  for (s = 0; s < SIMULT; s++) {
    offset = s * inst->magnLen;

    // newquantest(...)
    for (i = 0; i < inst->magnLen; i++) {
      // compute delta
      if (inst->density[offset + i] > 1.0) {
        delta = FACTOR * 1.f / inst->density[offset + i];
      } else {
        delta = FACTOR;
      }

      // update log quantile estimate
      if (lmagn[i] > inst->lquantile[offset + i]) {
        inst->lquantile[offset + i] +=
            QUANTILE * delta / (float)(inst->counter[s] + 1);
      } else {
        inst->lquantile[offset + i] -=
            (1.f - QUANTILE) * delta / (float)(inst->counter[s] + 1);
      }

      // update density estimate
      if (fabs(lmagn[i] - inst->lquantile[offset + i]) < WIDTH) {
        inst->density[offset + i] =
            ((float)inst->counter[s] * inst->density[offset + i] +
             1.f / (2.f * WIDTH)) /
            (float)(inst->counter[s] + 1);
      }
    }  // end loop over magnitude spectrum

    if (inst->counter[s] >= END_STARTUP_LONG) {
      inst->counter[s] = 0;
      if (inst->updates >= END_STARTUP_LONG) {
        for (i = 0; i < inst->magnLen; i++) {
          inst->quantile[i] = (float)exp(inst->lquantile[offset + i]);
        }
      }
    }

    inst->counter[s]++;
  }  // end loop over simultaneous estimates

  // Sequentially update the noise during startup
  if (inst->updates < END_STARTUP_LONG) {
    // Use the last "s" to get noise during startup that differ from zero.
    for (i = 0; i < inst->magnLen; i++) {
      inst->quantile[i] = (float)exp(inst->lquantile[offset + i]);
    }
  }

  for (i = 0; i < inst->magnLen; i++) {
    noise[i] = inst->quantile[i];
  }
}

// Extract thresholds for feature parameters
// histograms are computed over some window_size (given by
// inst->modelUpdatePars[1])
// thresholds and weights are extracted every window
// flag 0 means update histogram only, flag 1 means compute the
// thresholds/weights
// threshold and weights are returned in: inst->priorModelPars
static void FeatureParameterExtraction(NSinst_t* const self, int flag) {
  int i, useFeatureSpecFlat, useFeatureSpecDiff, numHistLrt;
  int maxPeak1, maxPeak2;
  int weightPeak1SpecFlat, weightPeak2SpecFlat, weightPeak1SpecDiff,
      weightPeak2SpecDiff;

  float binMid, featureSum;
  float posPeak1SpecFlat, posPeak2SpecFlat, posPeak1SpecDiff, posPeak2SpecDiff;
  float fluctLrt, avgHistLrt, avgSquareHistLrt, avgHistLrtCompl;

  // 3 features: lrt, flatness, difference
  // lrt_feature = self->featureData[3];
  // flat_feature = self->featureData[0];
  // diff_feature = self->featureData[4];

  // update histograms
  if (flag == 0) {
    // LRT
    if ((self->featureData[3] <
         HIST_PAR_EST * self->featureExtractionParams.binSizeLrt) &&
        (self->featureData[3] >= 0.0)) {
      i = (int)(self->featureData[3] /
                self->featureExtractionParams.binSizeLrt);
      self->histLrt[i]++;
    }
    // Spectral flatness
    if ((self->featureData[0] <
         HIST_PAR_EST * self->featureExtractionParams.binSizeSpecFlat) &&
        (self->featureData[0] >= 0.0)) {
      i = (int)(self->featureData[0] /
                self->featureExtractionParams.binSizeSpecFlat);
      self->histSpecFlat[i]++;
    }
    // Spectral difference
    if ((self->featureData[4] <
         HIST_PAR_EST * self->featureExtractionParams.binSizeSpecDiff) &&
        (self->featureData[4] >= 0.0)) {
      i = (int)(self->featureData[4] /
                self->featureExtractionParams.binSizeSpecDiff);
      self->histSpecDiff[i]++;
    }
  }

  // extract parameters for speech/noise probability
  if (flag == 1) {
    // lrt feature: compute the average over
    // self->featureExtractionParams.rangeAvgHistLrt
    avgHistLrt = 0.0;
    avgHistLrtCompl = 0.0;
    avgSquareHistLrt = 0.0;
    numHistLrt = 0;
    for (i = 0; i < HIST_PAR_EST; i++) {
      binMid = ((float)i + 0.5f) * self->featureExtractionParams.binSizeLrt;
      if (binMid <= self->featureExtractionParams.rangeAvgHistLrt) {
        avgHistLrt += self->histLrt[i] * binMid;
        numHistLrt += self->histLrt[i];
      }
      avgSquareHistLrt += self->histLrt[i] * binMid * binMid;
      avgHistLrtCompl += self->histLrt[i] * binMid;
    }
    if (numHistLrt > 0) {
      avgHistLrt = avgHistLrt / ((float)numHistLrt);
    }
    avgHistLrtCompl = avgHistLrtCompl / ((float)self->modelUpdatePars[1]);
    avgSquareHistLrt = avgSquareHistLrt / ((float)self->modelUpdatePars[1]);
    fluctLrt = avgSquareHistLrt - avgHistLrt * avgHistLrtCompl;
    // get threshold for lrt feature:
    if (fluctLrt < self->featureExtractionParams.thresFluctLrt) {
      // very low fluct, so likely noise
      self->priorModelPars[0] = self->featureExtractionParams.maxLrt;
    } else {
      self->priorModelPars[0] =
          self->featureExtractionParams.factor1ModelPars * avgHistLrt;
      // check if value is within min/max range
      if (self->priorModelPars[0] < self->featureExtractionParams.minLrt) {
        self->priorModelPars[0] = self->featureExtractionParams.minLrt;
      }
      if (self->priorModelPars[0] > self->featureExtractionParams.maxLrt) {
        self->priorModelPars[0] = self->featureExtractionParams.maxLrt;
      }
    }
    // done with lrt feature

    // for spectral flatness and spectral difference: compute the main peaks of
    // histogram
    maxPeak1 = 0;
    maxPeak2 = 0;
    posPeak1SpecFlat = 0.0;
    posPeak2SpecFlat = 0.0;
    weightPeak1SpecFlat = 0;
    weightPeak2SpecFlat = 0;

    // peaks for flatness
    for (i = 0; i < HIST_PAR_EST; i++) {
      binMid =
          (i + 0.5f) * self->featureExtractionParams.binSizeSpecFlat;
      if (self->histSpecFlat[i] > maxPeak1) {
        // Found new "first" peak
        maxPeak2 = maxPeak1;
        weightPeak2SpecFlat = weightPeak1SpecFlat;
        posPeak2SpecFlat = posPeak1SpecFlat;

        maxPeak1 = self->histSpecFlat[i];
        weightPeak1SpecFlat = self->histSpecFlat[i];
        posPeak1SpecFlat = binMid;
      } else if (self->histSpecFlat[i] > maxPeak2) {
        // Found new "second" peak
        maxPeak2 = self->histSpecFlat[i];
        weightPeak2SpecFlat = self->histSpecFlat[i];
        posPeak2SpecFlat = binMid;
      }
    }

    // compute two peaks for spectral difference
    maxPeak1 = 0;
    maxPeak2 = 0;
    posPeak1SpecDiff = 0.0;
    posPeak2SpecDiff = 0.0;
    weightPeak1SpecDiff = 0;
    weightPeak2SpecDiff = 0;
    // peaks for spectral difference
    for (i = 0; i < HIST_PAR_EST; i++) {
      binMid =
          ((float)i + 0.5f) * self->featureExtractionParams.binSizeSpecDiff;
      if (self->histSpecDiff[i] > maxPeak1) {
        // Found new "first" peak
        maxPeak2 = maxPeak1;
        weightPeak2SpecDiff = weightPeak1SpecDiff;
        posPeak2SpecDiff = posPeak1SpecDiff;

        maxPeak1 = self->histSpecDiff[i];
        weightPeak1SpecDiff = self->histSpecDiff[i];
        posPeak1SpecDiff = binMid;
      } else if (self->histSpecDiff[i] > maxPeak2) {
        // Found new "second" peak
        maxPeak2 = self->histSpecDiff[i];
        weightPeak2SpecDiff = self->histSpecDiff[i];
        posPeak2SpecDiff = binMid;
      }
    }

    // for spectrum flatness feature
    useFeatureSpecFlat = 1;
    // merge the two peaks if they are close
    if ((fabs(posPeak2SpecFlat - posPeak1SpecFlat) <
         self->featureExtractionParams.limitPeakSpacingSpecFlat) &&
        (weightPeak2SpecFlat >
         self->featureExtractionParams.limitPeakWeightsSpecFlat *
             weightPeak1SpecFlat)) {
      weightPeak1SpecFlat += weightPeak2SpecFlat;
      posPeak1SpecFlat = 0.5f * (posPeak1SpecFlat + posPeak2SpecFlat);
    }
    // reject if weight of peaks is not large enough, or peak value too small
    if (weightPeak1SpecFlat <
            self->featureExtractionParams.thresWeightSpecFlat ||
        posPeak1SpecFlat < self->featureExtractionParams.thresPosSpecFlat) {
      useFeatureSpecFlat = 0;
    }
    // if selected, get the threshold
    if (useFeatureSpecFlat == 1) {
      // compute the threshold
      self->priorModelPars[1] =
          self->featureExtractionParams.factor2ModelPars * posPeak1SpecFlat;
      // check if value is within min/max range
      if (self->priorModelPars[1] < self->featureExtractionParams.minSpecFlat) {
        self->priorModelPars[1] = self->featureExtractionParams.minSpecFlat;
      }
      if (self->priorModelPars[1] > self->featureExtractionParams.maxSpecFlat) {
        self->priorModelPars[1] = self->featureExtractionParams.maxSpecFlat;
      }
    }
    // done with flatness feature

    // for template feature
    useFeatureSpecDiff = 1;
    // merge the two peaks if they are close
    if ((fabs(posPeak2SpecDiff - posPeak1SpecDiff) <
         self->featureExtractionParams.limitPeakSpacingSpecDiff) &&
        (weightPeak2SpecDiff >
         self->featureExtractionParams.limitPeakWeightsSpecDiff *
             weightPeak1SpecDiff)) {
      weightPeak1SpecDiff += weightPeak2SpecDiff;
      posPeak1SpecDiff = 0.5f * (posPeak1SpecDiff + posPeak2SpecDiff);
    }
    // get the threshold value
    self->priorModelPars[3] =
        self->featureExtractionParams.factor1ModelPars * posPeak1SpecDiff;
    // reject if weight of peaks is not large enough
    if (weightPeak1SpecDiff <
        self->featureExtractionParams.thresWeightSpecDiff) {
      useFeatureSpecDiff = 0;
    }
    // check if value is within min/max range
    if (self->priorModelPars[3] < self->featureExtractionParams.minSpecDiff) {
      self->priorModelPars[3] = self->featureExtractionParams.minSpecDiff;
    }
    if (self->priorModelPars[3] > self->featureExtractionParams.maxSpecDiff) {
      self->priorModelPars[3] = self->featureExtractionParams.maxSpecDiff;
    }
    // done with spectral difference feature

    // don't use template feature if fluctuation of lrt feature is very low:
    //  most likely just noise state
    if (fluctLrt < self->featureExtractionParams.thresFluctLrt) {
      useFeatureSpecDiff = 0;
    }

    // select the weights between the features
    // self->priorModelPars[4] is weight for lrt: always selected
    // self->priorModelPars[5] is weight for spectral flatness
    // self->priorModelPars[6] is weight for spectral difference
    featureSum = (float)(1 + useFeatureSpecFlat + useFeatureSpecDiff);
    self->priorModelPars[4] = 1.f / featureSum;
    self->priorModelPars[5] = ((float)useFeatureSpecFlat) / featureSum;
    self->priorModelPars[6] = ((float)useFeatureSpecDiff) / featureSum;

    // set hists to zero for next update
    if (self->modelUpdatePars[0] >= 1) {
      for (i = 0; i < HIST_PAR_EST; i++) {
        self->histLrt[i] = 0;
        self->histSpecFlat[i] = 0;
        self->histSpecDiff[i] = 0;
      }
    }
  }  // end of flag == 1
}

// Compute spectral flatness on input spectrum
// magnIn is the magnitude spectrum
// spectral flatness is returned in inst->featureData[0]
static void ComputeSpectralFlatness(NSinst_t* const self, const float* magnIn) {
  int i;
  int shiftLP = 1;  // option to remove first bin(s) from spectral measures
  float avgSpectralFlatnessNum, avgSpectralFlatnessDen, spectralTmp;

  // comute spectral measures
  // for flatness
  avgSpectralFlatnessNum = 0.0;
  avgSpectralFlatnessDen = self->sumMagn;
  for (i = 0; i < shiftLP; i++) {
    avgSpectralFlatnessDen -= magnIn[i];
  }
  // compute log of ratio of the geometric to arithmetic mean: check for log(0)
  // case
  for (i = shiftLP; i < self->magnLen; i++) {
    if (magnIn[i] > 0.0) {
      avgSpectralFlatnessNum += (float)log(magnIn[i]);
    } else {
      self->featureData[0] -= SPECT_FL_TAVG * self->featureData[0];
      return;
    }
  }
  // normalize
  avgSpectralFlatnessDen = avgSpectralFlatnessDen / self->magnLen;
  avgSpectralFlatnessNum = avgSpectralFlatnessNum / self->magnLen;

  // ratio and inverse log: check for case of log(0)
  spectralTmp = (float)exp(avgSpectralFlatnessNum) / avgSpectralFlatnessDen;

  // time-avg update of spectral flatness feature
  self->featureData[0] += SPECT_FL_TAVG * (spectralTmp - self->featureData[0]);
  // done with flatness feature
}

// Compute prior and post snr based on quantile noise estimation.
// Compute DD estimate of prior SNR.
// Inputs:
//   * |magn| is the signal magnitude spectrum estimate.
//   * |noise| is the magnitude noise spectrum estimate.
// Outputs:
//   * |snrLocPrior| is the computed prior SNR.
//   * |snrLocPost| is the computed post SNR.
static void ComputeSnr(const NSinst_t* const self,
                       const float* magn,
                       const float* noise,
                       float* snrLocPrior,
                       float* snrLocPost) {
  int i;

  for (i = 0; i < self->magnLen; i++) {
    // Previous post SNR.
    // Previous estimate: based on previous frame with gain filter.
    float previousEstimateStsa = self->magnPrevAnalyze[i] /
        (self->noisePrev[i] + 0.0001f) * self->smooth[i];
    // Post SNR.
    snrLocPost[i] = 0.f;
    if (magn[i] > noise[i]) {
      snrLocPost[i] = magn[i] / (noise[i] + 0.0001f) - 1.f;
    }
    // DD estimate is sum of two terms: current estimate and previous estimate
    // directed decision update of snrPrior.
    snrLocPrior[i] =
        DD_PR_SNR * previousEstimateStsa + (1.f - DD_PR_SNR) * snrLocPost[i];
  }  // End of loop over frequencies.
}

// Compute the difference measure between input spectrum and a template/learned
// noise spectrum
// magnIn is the input spectrum
// the reference/template spectrum is inst->magnAvgPause[i]
// returns (normalized) spectral difference in inst->featureData[4]
static void ComputeSpectralDifference(NSinst_t* const self,
                                      const float* magnIn) {
  // avgDiffNormMagn = var(magnIn) - cov(magnIn, magnAvgPause)^2 /
  // var(magnAvgPause)
  int i;
  float avgPause, avgMagn, covMagnPause, varPause, varMagn, avgDiffNormMagn;

  avgPause = 0.0;
  avgMagn = self->sumMagn;
  // compute average quantities
  for (i = 0; i < self->magnLen; i++) {
    // conservative smooth noise spectrum from pause frames
    avgPause += self->magnAvgPause[i];
  }
  avgPause = avgPause / ((float)self->magnLen);
  avgMagn = avgMagn / ((float)self->magnLen);

  covMagnPause = 0.0;
  varPause = 0.0;
  varMagn = 0.0;
  // compute variance and covariance quantities
  for (i = 0; i < self->magnLen; i++) {
    covMagnPause += (magnIn[i] - avgMagn) * (self->magnAvgPause[i] - avgPause);
    varPause +=
        (self->magnAvgPause[i] - avgPause) * (self->magnAvgPause[i] - avgPause);
    varMagn += (magnIn[i] - avgMagn) * (magnIn[i] - avgMagn);
  }
  covMagnPause = covMagnPause / ((float)self->magnLen);
  varPause = varPause / ((float)self->magnLen);
  varMagn = varMagn / ((float)self->magnLen);
  // update of average magnitude spectrum
  self->featureData[6] += self->signalEnergy;

  avgDiffNormMagn =
      varMagn - (covMagnPause * covMagnPause) / (varPause + 0.0001f);
  // normalize and compute time-avg update of difference feature
  avgDiffNormMagn = (float)(avgDiffNormMagn / (self->featureData[5] + 0.0001f));
  self->featureData[4] +=
      SPECT_DIFF_TAVG * (avgDiffNormMagn - self->featureData[4]);
}

// Compute speech/noise probability
// speech/noise probability is returned in: probSpeechFinal
// magn is the input magnitude spectrum
// noise is the noise spectrum
// snrLocPrior is the prior snr for each freq.
// snr loc_post is the post snr for each freq.
static void SpeechNoiseProb(NSinst_t* const self,
                            float* probSpeechFinal,
                            const float* snrLocPrior,
                            const float* snrLocPost) {
  int i, sgnMap;
  float invLrt, gainPrior, indPrior;
  float logLrtTimeAvgKsum, besselTmp;
  float indicator0, indicator1, indicator2;
  float tmpFloat1, tmpFloat2;
  float weightIndPrior0, weightIndPrior1, weightIndPrior2;
  float threshPrior0, threshPrior1, threshPrior2;
  float widthPrior, widthPrior0, widthPrior1, widthPrior2;

  widthPrior0 = WIDTH_PR_MAP;
  widthPrior1 = 2.f * WIDTH_PR_MAP;  // width for pause region:
  // lower range, so increase width in tanh map
  widthPrior2 = 2.f * WIDTH_PR_MAP;  // for spectral-difference measure

  // threshold parameters for features
  threshPrior0 = self->priorModelPars[0];
  threshPrior1 = self->priorModelPars[1];
  threshPrior2 = self->priorModelPars[3];

  // sign for flatness feature
  sgnMap = (int)(self->priorModelPars[2]);

  // weight parameters for features
  weightIndPrior0 = self->priorModelPars[4];
  weightIndPrior1 = self->priorModelPars[5];
  weightIndPrior2 = self->priorModelPars[6];

  // compute feature based on average LR factor
  // this is the average over all frequencies of the smooth log lrt
  logLrtTimeAvgKsum = 0.0;
  for (i = 0; i < self->magnLen; i++) {
    tmpFloat1 = 1.f + 2.f * snrLocPrior[i];
    tmpFloat2 = 2.f * snrLocPrior[i] / (tmpFloat1 + 0.0001f);
    besselTmp = (snrLocPost[i] + 1.f) * tmpFloat2;
    self->logLrtTimeAvg[i] +=
        LRT_TAVG * (besselTmp - (float)log(tmpFloat1) - self->logLrtTimeAvg[i]);
    logLrtTimeAvgKsum += self->logLrtTimeAvg[i];
  }
  logLrtTimeAvgKsum = (float)logLrtTimeAvgKsum / (self->magnLen);
  self->featureData[3] = logLrtTimeAvgKsum;
  // done with computation of LR factor

  //
  // compute the indicator functions
  //

  // average lrt feature
  widthPrior = widthPrior0;
  // use larger width in tanh map for pause regions
  if (logLrtTimeAvgKsum < threshPrior0) {
    widthPrior = widthPrior1;
  }
  // compute indicator function: sigmoid map
  indicator0 =
      0.5f *
      ((float)tanh(widthPrior * (logLrtTimeAvgKsum - threshPrior0)) + 1.f);

  // spectral flatness feature
  tmpFloat1 = self->featureData[0];
  widthPrior = widthPrior0;
  // use larger width in tanh map for pause regions
  if (sgnMap == 1 && (tmpFloat1 > threshPrior1)) {
    widthPrior = widthPrior1;
  }
  if (sgnMap == -1 && (tmpFloat1 < threshPrior1)) {
    widthPrior = widthPrior1;
  }
  // compute indicator function: sigmoid map
  indicator1 =
      0.5f *
      ((float)tanh((float)sgnMap * widthPrior * (threshPrior1 - tmpFloat1)) +
       1.f);

  // for template spectrum-difference
  tmpFloat1 = self->featureData[4];
  widthPrior = widthPrior0;
  // use larger width in tanh map for pause regions
  if (tmpFloat1 < threshPrior2) {
    widthPrior = widthPrior2;
  }
  // compute indicator function: sigmoid map
  indicator2 =
      0.5f * ((float)tanh(widthPrior * (tmpFloat1 - threshPrior2)) + 1.f);

  // combine the indicator function with the feature weights
  indPrior = weightIndPrior0 * indicator0 + weightIndPrior1 * indicator1 +
             weightIndPrior2 * indicator2;
  // done with computing indicator function

  // compute the prior probability
  self->priorSpeechProb += PRIOR_UPDATE * (indPrior - self->priorSpeechProb);
  // make sure probabilities are within range: keep floor to 0.01
  if (self->priorSpeechProb > 1.f) {
    self->priorSpeechProb = 1.f;
  }
  if (self->priorSpeechProb < 0.01f) {
    self->priorSpeechProb = 0.01f;
  }

  // final speech probability: combine prior model with LR factor:
  gainPrior = (1.f - self->priorSpeechProb) / (self->priorSpeechProb + 0.0001f);
  for (i = 0; i < self->magnLen; i++) {
    invLrt = (float)exp(-self->logLrtTimeAvg[i]);
    invLrt = (float)gainPrior * invLrt;
    probSpeechFinal[i] = 1.f / (1.f + invLrt);
  }
}

// Update the noise features.
// Inputs:
//   * |magn| is the signal magnitude spectrum estimate.
//   * |updateParsFlag| is an update flag for parameters.
static void FeatureUpdate(NSinst_t* const self,
                          const float* magn,
                          int updateParsFlag) {
  // Compute spectral flatness on input spectrum.
  ComputeSpectralFlatness(self, magn);
  // Compute difference of input spectrum with learned/estimated noise spectrum.
  ComputeSpectralDifference(self, magn);
  // Compute histograms for parameter decisions (thresholds and weights for
  // features).
  // Parameters are extracted once every window time.
  // (=self->modelUpdatePars[1])
  if (updateParsFlag >= 1) {
    // Counter update.
    self->modelUpdatePars[3]--;
    // Update histogram.
    if (self->modelUpdatePars[3] > 0) {
      FeatureParameterExtraction(self, 0);
    }
    // Compute model parameters.
    if (self->modelUpdatePars[3] == 0) {
      FeatureParameterExtraction(self, 1);
      self->modelUpdatePars[3] = self->modelUpdatePars[1];
      // If wish to update only once, set flag to zero.
      if (updateParsFlag == 1) {
        self->modelUpdatePars[0] = 0;
      } else {
        // Update every window:
        // Get normalization for spectral difference for next window estimate.
        self->featureData[6] =
            self->featureData[6] / ((float)self->modelUpdatePars[1]);
        self->featureData[5] =
            0.5f * (self->featureData[6] + self->featureData[5]);
        self->featureData[6] = 0.f;
      }
    }
  }
}

// Update the noise estimate.
// Inputs:
//   * |magn| is the signal magnitude spectrum estimate.
//   * |snrLocPrior| is the prior SNR.
//   * |snrLocPost| is the post SNR.
// Output:
//   * |noise| is the updated noise magnitude spectrum estimate.
static void UpdateNoiseEstimate(NSinst_t* const self,
                                const float* magn,
                                const float* snrLocPrior,
                                const float* snrLocPost,
                                float* noise) {
  int i;
  float probSpeech, probNonSpeech;
  // Time-avg parameter for noise update.
  float gammaNoiseTmp = NOISE_UPDATE;
  float gammaNoiseOld;
  float noiseUpdateTmp;

  for (i = 0; i < self->magnLen; i++) {
    probSpeech = self->speechProb[i];
    probNonSpeech = 1.f - probSpeech;
    // Temporary noise update:
    // Use it for speech frames if update value is less than previous.
    noiseUpdateTmp = gammaNoiseTmp * self->noisePrev[i] +
                     (1.f - gammaNoiseTmp) * (probNonSpeech * magn[i] +
                                              probSpeech * self->noisePrev[i]);
    //
    // Time-constant based on speech/noise state.
    gammaNoiseOld = gammaNoiseTmp;
    gammaNoiseTmp = NOISE_UPDATE;
    // Increase gamma (i.e., less noise update) for frame likely to be speech.
    if (probSpeech > PROB_RANGE) {
      gammaNoiseTmp = SPEECH_UPDATE;
    }
    // Conservative noise update.
    if (probSpeech < PROB_RANGE) {
      self->magnAvgPause[i] += GAMMA_PAUSE * (magn[i] - self->magnAvgPause[i]);
    }
    // Noise update.
    if (gammaNoiseTmp == gammaNoiseOld) {
      noise[i] = noiseUpdateTmp;
    } else {
      noise[i] = gammaNoiseTmp * self->noisePrev[i] +
                 (1.f - gammaNoiseTmp) * (probNonSpeech * magn[i] +
                                          probSpeech * self->noisePrev[i]);
      // Allow for noise update downwards:
      // If noise update decreases the noise, it is safe, so allow it to
      // happen.
      if (noiseUpdateTmp < noise[i]) {
        noise[i] = noiseUpdateTmp;
      }
    }
  }  // End of freq loop.
}

// Updates |buffer| with a new |frame|.
// Inputs:
//   * |frame| is a new speech frame or NULL for setting to zero.
//   * |frame_length| is the length of the new frame.
//   * |buffer_length| is the length of the buffer.
// Output:
//   * |buffer| is the updated buffer.
static void UpdateBuffer(const float* frame,
                         int frame_length,
                         int buffer_length,
                         float* buffer) {
  assert(buffer_length < 2 * frame_length);

  memcpy(buffer,
         buffer + frame_length,
         sizeof(*buffer) * (buffer_length - frame_length));
  if (frame) {
    memcpy(buffer + buffer_length - frame_length,
           frame,
           sizeof(*buffer) * frame_length);
  } else {
    memset(buffer + buffer_length - frame_length,
           0,
           sizeof(*buffer) * frame_length);
  }
}

// Transforms the signal from time to frequency domain.
// Inputs:
//   * |time_data| is the signal in the time domain.
//   * |time_data_length| is the length of the analysis buffer.
//   * |magnitude_length| is the length of the spectrum magnitude, which equals
//     the length of both |real| and |imag| (time_data_length / 2 + 1).
// Outputs:
//   * |time_data| is the signal in the frequency domain.
//   * |real| is the real part of the frequency domain.
//   * |imag| is the imaginary part of the frequency domain.
//   * |magn| is the calculated signal magnitude in the frequency domain.
static void FFT(NSinst_t* const self,
                float* time_data,
                int time_data_length,
                int magnitude_length,
                float* real,
                float* imag,
                float* magn) {
  int i;

  assert(magnitude_length == time_data_length / 2 + 1);

  WebRtc_rdft(time_data_length, 1, time_data, self->ip, self->wfft);

  imag[0] = 0;
  real[0] = time_data[0];
  magn[0] = fabs(real[0]) + 1.f;
  imag[magnitude_length - 1] = 0;
  real[magnitude_length - 1] = time_data[1];
  magn[magnitude_length - 1] = fabs(real[magnitude_length - 1]) + 1.f;
  for (i = 1; i < magnitude_length - 1; ++i) {
    real[i] = time_data[2 * i];
    imag[i] = time_data[2 * i + 1];
    // Magnitude spectrum.
    magn[i] = sqrtf(real[i] * real[i] + imag[i] * imag[i]) + 1.f;
  }
}

// Transforms the signal from frequency to time domain.
// Inputs:
//   * |real| is the real part of the frequency domain.
//   * |imag| is the imaginary part of the frequency domain.
//   * |magnitude_length| is the length of the spectrum magnitude, which equals
//     the length of both |real| and |imag|.
//   * |time_data_length| is the length of the analysis buffer
//     (2 * (magnitude_length - 1)).
// Output:
//   * |time_data| is the signal in the time domain.
static void IFFT(NSinst_t* const self,
                 const float* real,
                 const float* imag,
                 int magnitude_length,
                 int time_data_length,
                 float* time_data) {
  int i;

  assert(time_data_length == 2 * (magnitude_length - 1));

  time_data[0] = real[0];
  time_data[1] = real[magnitude_length - 1];
  for (i = 1; i < magnitude_length - 1; ++i) {
    time_data[2 * i] = real[i];
    time_data[2 * i + 1] = imag[i];
  }
  WebRtc_rdft(time_data_length, -1, time_data, self->ip, self->wfft);

  for (i = 0; i < time_data_length; ++i) {
    time_data[i] *= 2.f / time_data_length;  // FFT scaling.
  }
}

// Calculates the energy of a buffer.
// Inputs:
//   * |buffer| is the buffer over which the energy is calculated.
//   * |length| is the length of the buffer.
// Returns the calculated energy.
static float Energy(const float* buffer, int length) {
  int i;
  float energy = 0.f;

  for (i = 0; i < length; ++i) {
    energy += buffer[i] * buffer[i];
  }

  return energy;
}

// Windows a buffer.
// Inputs:
//   * |window| is the window by which to multiply.
//   * |data| is the data without windowing.
//   * |length| is the length of the window and data.
// Output:
//   * |data_windowed| is the windowed data.
static void Windowing(const float* window,
                      const float* data,
                      int length,
                      float* data_windowed) {
  int i;

  for (i = 0; i < length; ++i) {
    data_windowed[i] = window[i] * data[i];
  }
}

// Estimate prior SNR decision-directed and compute DD based Wiener Filter.
// Input:
//   * |magn| is the signal magnitude spectrum estimate.
// Output:
//   * |theFilter| is the frequency response of the computed Wiener filter.
static void ComputeDdBasedWienerFilter(const NSinst_t* const self,
                                       const float* magn,
                                       float* theFilter) {
  int i;
  float snrPrior, previousEstimateStsa, currentEstimateStsa;

  for (i = 0; i < self->magnLen; i++) {
    // Previous estimate: based on previous frame with gain filter.
    previousEstimateStsa = self->magnPrevProcess[i] /
                           (self->noisePrev[i] + 0.0001f) * self->smooth[i];
    // Post and prior SNR.
    currentEstimateStsa = 0.f;
    if (magn[i] > self->noise[i]) {
      currentEstimateStsa = magn[i] / (self->noise[i] + 0.0001f) - 1.f;
    }
    // DD estimate is sum of two terms: current estimate and previous estimate.
    // Directed decision update of |snrPrior|.
    snrPrior = DD_PR_SNR * previousEstimateStsa +
               (1.f - DD_PR_SNR) * currentEstimateStsa;
    // Gain filter.
    theFilter[i] = snrPrior / (self->overdrive + snrPrior);
  }  // End of loop over frequencies.
}

int WebRtcNs_AnalyzeCore(NSinst_t* inst, float* speechFrame) {
  int i;
  const int kStartBand = 5;  // Skip first frequency bins during estimation.
  int updateParsFlag;
  float energy;
  float signalEnergy = 0.f;
  float sumMagn = 0.f;
  float tmpFloat1, tmpFloat2, tmpFloat3;
  float winData[ANAL_BLOCKL_MAX];
  float magn[HALF_ANAL_BLOCKL], noise[HALF_ANAL_BLOCKL];
  float snrLocPost[HALF_ANAL_BLOCKL], snrLocPrior[HALF_ANAL_BLOCKL];
  float real[ANAL_BLOCKL_MAX], imag[HALF_ANAL_BLOCKL];
  // Variables during startup
  float sum_log_i = 0.0;
  float sum_log_i_square = 0.0;
  float sum_log_magn = 0.0;
  float sum_log_i_log_magn = 0.0;
  float parametric_exp = 0.0;
  float parametric_num = 0.0;

  // Check that initiation has been done
  if (inst->initFlag != 1) {
    return (-1);
  }
  //
  updateParsFlag = inst->modelUpdatePars[0];
  //

  // update analysis buffer for L band
  UpdateBuffer(speechFrame, inst->blockLen, inst->anaLen, inst->analyzeBuf);

  // windowing
  Windowing(inst->window, inst->analyzeBuf, inst->anaLen, winData);
  energy = Energy(winData, inst->anaLen);
  if (energy == 0.0) {
    // we want to avoid updating statistics in this case:
    // Updating feature statistics when we have zeros only will cause
    // thresholds to move towards zero signal situations. This in turn has the
    // effect that once the signal is "turned on" (non-zero values) everything
    // will be treated as speech and there is no noise suppression effect.
    // Depending on the duration of the inactive signal it takes a
    // considerable amount of time for the system to learn what is noise and
    // what is speech.
    return 0;
  }

  //
  inst->blockInd++;  // Update the block index only when we process a block.
  // FFT
  FFT(inst, winData, inst->anaLen, inst->magnLen, real, imag, magn);

  for (i = 0; i < inst->magnLen; i++) {
    signalEnergy += real[i] * real[i] + imag[i] * imag[i];
    sumMagn += magn[i];
    if (inst->blockInd < END_STARTUP_SHORT) {
      if (i >= kStartBand) {
        tmpFloat2 = log((float)i);
        sum_log_i += tmpFloat2;
        sum_log_i_square += tmpFloat2 * tmpFloat2;
        tmpFloat1 = log(magn[i]);
        sum_log_magn += tmpFloat1;
        sum_log_i_log_magn += tmpFloat2 * tmpFloat1;
      }
    }
  }
  signalEnergy = signalEnergy / ((float)inst->magnLen);
  inst->signalEnergy = signalEnergy;
  inst->sumMagn = sumMagn;

  // quantile noise estimate
  WebRtcNs_NoiseEstimation(inst, magn, noise);
  // compute simplified noise model during startup
  if (inst->blockInd < END_STARTUP_SHORT) {
    // Estimate White noise
    inst->whiteNoiseLevel += sumMagn / ((float)inst->magnLen) * inst->overdrive;
    // Estimate Pink noise parameters
    tmpFloat1 = sum_log_i_square * ((float)(inst->magnLen - kStartBand));
    tmpFloat1 -= (sum_log_i * sum_log_i);
    tmpFloat2 =
        (sum_log_i_square * sum_log_magn - sum_log_i * sum_log_i_log_magn);
    tmpFloat3 = tmpFloat2 / tmpFloat1;
    // Constrain the estimated spectrum to be positive
    if (tmpFloat3 < 0.f) {
      tmpFloat3 = 0.f;
    }
    inst->pinkNoiseNumerator += tmpFloat3;
    tmpFloat2 = (sum_log_i * sum_log_magn);
    tmpFloat2 -= ((float)(inst->magnLen - kStartBand)) * sum_log_i_log_magn;
    tmpFloat3 = tmpFloat2 / tmpFloat1;
    // Constrain the pink noise power to be in the interval [0, 1];
    if (tmpFloat3 < 0.f) {
      tmpFloat3 = 0.f;
    }
    if (tmpFloat3 > 1.f) {
      tmpFloat3 = 1.f;
    }
    inst->pinkNoiseExp += tmpFloat3;

    // Calculate frequency independent parts of parametric noise estimate.
    if (inst->pinkNoiseExp > 0.f) {
      // Use pink noise estimate
      parametric_num =
          exp(inst->pinkNoiseNumerator / (float)(inst->blockInd + 1));
      parametric_num *= (float)(inst->blockInd + 1);
      parametric_exp = inst->pinkNoiseExp / (float)(inst->blockInd + 1);
    }
    for (i = 0; i < inst->magnLen; i++) {
      // Estimate the background noise using the white and pink noise
      // parameters
      if (inst->pinkNoiseExp == 0.f) {
        // Use white noise estimate
        inst->parametricNoise[i] = inst->whiteNoiseLevel;
      } else {
        // Use pink noise estimate
        float use_band = (float)(i < kStartBand ? kStartBand : i);
        inst->parametricNoise[i] =
            parametric_num / pow(use_band, parametric_exp);
      }
      // Weight quantile noise with modeled noise
      noise[i] *= (inst->blockInd);
      tmpFloat2 =
          inst->parametricNoise[i] * (END_STARTUP_SHORT - inst->blockInd);
      noise[i] += (tmpFloat2 / (float)(inst->blockInd + 1));
      noise[i] /= END_STARTUP_SHORT;
    }
  }
  // compute average signal during END_STARTUP_LONG time:
  // used to normalize spectral difference measure
  if (inst->blockInd < END_STARTUP_LONG) {
    inst->featureData[5] *= inst->blockInd;
    inst->featureData[5] += signalEnergy;
    inst->featureData[5] /= (inst->blockInd + 1);
  }

  // Post and prior SNR needed for WebRtcNs_SpeechNoiseProb.
  ComputeSnr(inst, magn, noise, snrLocPrior, snrLocPost);

  FeatureUpdate(inst, magn, updateParsFlag);
  SpeechNoiseProb(inst, inst->speechProb, snrLocPrior, snrLocPost);
  UpdateNoiseEstimate(inst, magn, snrLocPrior, snrLocPost, noise);

  // keep track of noise spectrum for next frame
  memcpy(inst->noise, noise, sizeof(*noise) * inst->magnLen);
  memcpy(inst->magnPrevAnalyze, magn, sizeof(*magn) * inst->magnLen);

  return 0;
}

int WebRtcNs_ProcessCore(NSinst_t* inst,
                         float* speechFrame,
                         float* speechFrameHB,
                         float* outFrame,
                         float* outFrameHB) {
  // main routine for noise reduction
  int flagHB = 0;
  int i;

  float energy1, energy2, gain, factor, factor1, factor2;
  float fout[BLOCKL_MAX];
  float winData[ANAL_BLOCKL_MAX];
  float magn[HALF_ANAL_BLOCKL];
  float theFilter[HALF_ANAL_BLOCKL], theFilterTmp[HALF_ANAL_BLOCKL];
  float real[ANAL_BLOCKL_MAX], imag[HALF_ANAL_BLOCKL];

  // SWB variables
  int deltaBweHB = 1;
  int deltaGainHB = 1;
  float decayBweHB = 1.0;
  float gainMapParHB = 1.0;
  float gainTimeDomainHB = 1.0;
  float avgProbSpeechHB, avgProbSpeechHBTmp, avgFilterGainHB, gainModHB;
  float sumMagnAnalyze, sumMagnProcess;

  // Check that initiation has been done
  if (inst->initFlag != 1) {
    return (-1);
  }
  // Check for valid pointers based on sampling rate
  if (inst->fs == 32000) {
    if (speechFrameHB == NULL) {
      return -1;
    }
    flagHB = 1;
    // range for averaging low band quantities for H band gain
    deltaBweHB = (int)inst->magnLen / 4;
    deltaGainHB = deltaBweHB;
  }

  // update analysis buffer for L band
  UpdateBuffer(speechFrame, inst->blockLen, inst->anaLen, inst->dataBuf);

  if (flagHB == 1) {
    // update analysis buffer for H band
    UpdateBuffer(speechFrameHB, inst->blockLen, inst->anaLen, inst->dataBufHB);
  }

  // windowing
  Windowing(inst->window, inst->dataBuf, inst->anaLen, winData);
  energy1 = Energy(winData, inst->anaLen);
  if (energy1 == 0.0) {
    // synthesize the special case of zero input
    // read out fully processed segment
    for (i = inst->windShift; i < inst->blockLen + inst->windShift; i++) {
      fout[i - inst->windShift] = inst->syntBuf[i];
    }
    // update synthesis buffer
    UpdateBuffer(NULL, inst->blockLen, inst->anaLen, inst->syntBuf);

    for (i = 0; i < inst->blockLen; ++i)
      outFrame[i] =
          WEBRTC_SPL_SAT(WEBRTC_SPL_WORD16_MAX, fout[i], WEBRTC_SPL_WORD16_MIN);

    // for time-domain gain of HB
    if (flagHB == 1)
      for (i = 0; i < inst->blockLen; ++i)
        outFrameHB[i] = WEBRTC_SPL_SAT(
            WEBRTC_SPL_WORD16_MAX, inst->dataBufHB[i], WEBRTC_SPL_WORD16_MIN);

    return 0;
  }
  // FFT
  FFT(inst, winData, inst->anaLen, inst->magnLen, real, imag, magn);

  if (inst->blockInd < END_STARTUP_SHORT) {
    for (i = 0; i < inst->magnLen; i++) {
      inst->initMagnEst[i] += magn[i];
    }
  }

  ComputeDdBasedWienerFilter(inst, magn, theFilter);

  for (i = 0; i < inst->magnLen; i++) {
    // flooring bottom
    if (theFilter[i] < inst->denoiseBound) {
      theFilter[i] = inst->denoiseBound;
    }
    // flooring top
    if (theFilter[i] > 1.f) {
      theFilter[i] = 1.f;
    }
    if (inst->blockInd < END_STARTUP_SHORT) {
      theFilterTmp[i] =
          (inst->initMagnEst[i] - inst->overdrive * inst->parametricNoise[i]);
      theFilterTmp[i] /= (inst->initMagnEst[i] + 0.0001f);
      // flooring bottom
      if (theFilterTmp[i] < inst->denoiseBound) {
        theFilterTmp[i] = inst->denoiseBound;
      }
      // flooring top
      if (theFilterTmp[i] > 1.f) {
        theFilterTmp[i] = 1.f;
      }
      // Weight the two suppression filters
      theFilter[i] *= (inst->blockInd);
      theFilterTmp[i] *= (END_STARTUP_SHORT - inst->blockInd);
      theFilter[i] += theFilterTmp[i];
      theFilter[i] /= (END_STARTUP_SHORT);
    }
    // smoothing
    inst->smooth[i] = theFilter[i];
    real[i] *= inst->smooth[i];
    imag[i] *= inst->smooth[i];
  }
  // keep track of magn spectrum for next frame
  memcpy(inst->magnPrevProcess, magn, sizeof(*magn) * inst->magnLen);
  memcpy(inst->noisePrev, inst->noise, sizeof(inst->noise[0]) * inst->magnLen);
  // back to time domain
  IFFT(inst, real, imag, inst->magnLen, inst->anaLen, winData);

  // scale factor: only do it after END_STARTUP_LONG time
  factor = 1.f;
  if (inst->gainmap == 1 && inst->blockInd > END_STARTUP_LONG) {
    factor1 = 1.f;
    factor2 = 1.f;

    energy2 = Energy(winData, inst->anaLen);
    gain = (float)sqrt(energy2 / (energy1 + 1.f));

    // scaling for new version
    if (gain > B_LIM) {
      factor1 = 1.f + 1.3f * (gain - B_LIM);
      if (gain * factor1 > 1.f) {
        factor1 = 1.f / gain;
      }
    }
    if (gain < B_LIM) {
      // don't reduce scale too much for pause regions:
      // attenuation here should be controlled by flooring
      if (gain <= inst->denoiseBound) {
        gain = inst->denoiseBound;
      }
      factor2 = 1.f - 0.3f * (B_LIM - gain);
    }
    // combine both scales with speech/noise prob:
    // note prior (priorSpeechProb) is not frequency dependent
    factor = inst->priorSpeechProb * factor1 +
             (1.f - inst->priorSpeechProb) * factor2;
  }  // out of inst->gainmap==1

  Windowing(inst->window, winData, inst->anaLen, winData);

  // synthesis
  for (i = 0; i < inst->anaLen; i++) {
    inst->syntBuf[i] += factor * winData[i];
  }
  // read out fully processed segment
  for (i = inst->windShift; i < inst->blockLen + inst->windShift; i++) {
    fout[i - inst->windShift] = inst->syntBuf[i];
  }
  // update synthesis buffer
  UpdateBuffer(NULL, inst->blockLen, inst->anaLen, inst->syntBuf);

  for (i = 0; i < inst->blockLen; ++i)
    outFrame[i] =
        WEBRTC_SPL_SAT(WEBRTC_SPL_WORD16_MAX, fout[i], WEBRTC_SPL_WORD16_MIN);

  // for time-domain gain of HB
  if (flagHB == 1) {
    // average speech prob from low band
    // avg over second half (i.e., 4->8kHz) of freq. spectrum
    avgProbSpeechHB = 0.0;
    for (i = inst->magnLen - deltaBweHB - 1; i < inst->magnLen - 1; i++) {
      avgProbSpeechHB += inst->speechProb[i];
    }
    avgProbSpeechHB = avgProbSpeechHB / ((float)deltaBweHB);
    // If the speech was suppressed by a component between Analyze and
    // Process, for example the AEC, then it should not be considered speech
    // for high band suppression purposes.
    sumMagnAnalyze = 0;
    sumMagnProcess = 0;
    for (i = 0; i < inst->magnLen; ++i) {
      sumMagnAnalyze += inst->magnPrevAnalyze[i];
      sumMagnProcess += inst->magnPrevProcess[i];
    }
    avgProbSpeechHB *= sumMagnProcess / sumMagnAnalyze;
    // average filter gain from low band
    // average over second half (i.e., 4->8kHz) of freq. spectrum
    avgFilterGainHB = 0.0;
    for (i = inst->magnLen - deltaGainHB - 1; i < inst->magnLen - 1; i++) {
      avgFilterGainHB += inst->smooth[i];
    }
    avgFilterGainHB = avgFilterGainHB / ((float)(deltaGainHB));
    avgProbSpeechHBTmp = 2.f * avgProbSpeechHB - 1.f;
    // gain based on speech prob:
    gainModHB = 0.5f * (1.f + (float)tanh(gainMapParHB * avgProbSpeechHBTmp));
    // combine gain with low band gain
    gainTimeDomainHB = 0.5f * gainModHB + 0.5f * avgFilterGainHB;
    if (avgProbSpeechHB >= 0.5f) {
      gainTimeDomainHB = 0.25f * gainModHB + 0.75f * avgFilterGainHB;
    }
    gainTimeDomainHB = gainTimeDomainHB * decayBweHB;
    // make sure gain is within flooring range
    // flooring bottom
    if (gainTimeDomainHB < inst->denoiseBound) {
      gainTimeDomainHB = inst->denoiseBound;
    }
    // flooring top
    if (gainTimeDomainHB > 1.f) {
      gainTimeDomainHB = 1.f;
    }
    // apply gain
    for (i = 0; i < inst->blockLen; i++) {
      float o = gainTimeDomainHB * inst->dataBufHB[i];
      outFrameHB[i] =
          WEBRTC_SPL_SAT(WEBRTC_SPL_WORD16_MAX, o, WEBRTC_SPL_WORD16_MIN);
    }
  }  // end of H band gain computation
  //

  return 0;
}
