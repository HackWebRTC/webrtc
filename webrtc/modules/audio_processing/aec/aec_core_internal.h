/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_CORE_INTERNAL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_CORE_INTERNAL_H_

#include <memory>

extern "C" {
#include "webrtc/common_audio/ring_buffer.h"
}
#include "webrtc/base/constructormagic.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/modules/audio_processing/aec/aec_common.h"
#include "webrtc/modules/audio_processing/aec/aec_core.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"
#include "webrtc/modules/audio_processing/utility/block_mean_calculator.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Number of partitions for the extended filter mode. The first one is an enum
// to be used in array declarations, as it represents the maximum filter length.
enum { kExtendedNumPartitions = 32 };
static const int kNormalNumPartitions = 12;

// Delay estimator constants, used for logging and delay compensation if
// if reported delays are disabled.
enum { kLookaheadBlocks = 15 };
enum {
  // 500 ms for 16 kHz which is equivalent with the limit of reported delays.
  kHistorySizeBlocks = 125
};

typedef struct PowerLevel {
  PowerLevel();

  BlockMeanCalculator framelevel;
  BlockMeanCalculator averagelevel;
  float minlevel;
} PowerLevel;

class DivergentFilterFraction {
 public:
  DivergentFilterFraction();

  // Reset.
  void Reset();

  void AddObservation(const PowerLevel& nearlevel,
                      const PowerLevel& linoutlevel,
                      const PowerLevel& nlpoutlevel);

  // Return the latest fraction.
  float GetLatestFraction() const;

 private:
  // Clear all values added.
  void Clear();

  size_t count_;
  size_t occurrence_;
  float fraction_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DivergentFilterFraction);
};

typedef struct CoherenceState {
  complex_t sde[PART_LEN1];  // cross-psd of nearend and error
  complex_t sxd[PART_LEN1];  // cross-psd of farend and nearend
  float sx[PART_LEN1], sd[PART_LEN1], se[PART_LEN1];  // far, near, error psd
} CoherenceState;

struct AecCore {
  explicit AecCore(int instance_index);
  ~AecCore();

  std::unique_ptr<ApmDataDumper> data_dumper;

  CoherenceState coherence_state;

  int farBufWritePos, farBufReadPos;

  int knownDelay;
  int inSamples, outSamples;
  int delayEstCtr;

  RingBuffer* nearFrBuf;
  RingBuffer* outFrBuf;

  RingBuffer* nearFrBufH[NUM_HIGH_BANDS_MAX];
  RingBuffer* outFrBufH[NUM_HIGH_BANDS_MAX];

  float dBuf[PART_LEN2];  // nearend
  float eBuf[PART_LEN2];  // error

  float dBufH[NUM_HIGH_BANDS_MAX][PART_LEN2];  // nearend

  float xPow[PART_LEN1];
  float dPow[PART_LEN1];
  float dMinPow[PART_LEN1];
  float dInitMinPow[PART_LEN1];
  float* noisePow;

  float xfBuf[2][kExtendedNumPartitions * PART_LEN1];  // farend fft buffer
  float wfBuf[2][kExtendedNumPartitions * PART_LEN1];  // filter fft
  // Farend windowed fft buffer.
  complex_t xfwBuf[kExtendedNumPartitions * PART_LEN1];

  float hNs[PART_LEN1];
  float hNlFbMin, hNlFbLocalMin;
  float hNlXdAvgMin;
  int hNlNewMin, hNlMinCtr;
  float overDrive;
  float overdrive_scaling;
  int nlp_mode;
  float outBuf[PART_LEN];
  int delayIdx;

  short stNearState, echoState;
  short divergeState;

  int xfBufBlockPos;

  RingBuffer* far_time_buf;

  int system_delay;  // Current system delay buffered in AEC.

  int mult;  // sampling frequency multiple
  int sampFreq = 16000;
  size_t num_bands;
  uint32_t seed;

  float filter_step_size;  // stepsize
  float error_threshold;   // error threshold

  int noiseEstCtr;

  PowerLevel farlevel;
  PowerLevel nearlevel;
  PowerLevel linoutlevel;
  PowerLevel nlpoutlevel;

  int metricsMode;
  int stateCounter;
  Stats erl;
  Stats erle;
  Stats aNlp;
  Stats rerl;
  DivergentFilterFraction divergent_filter_fraction;

  // Quantities to control H band scaling for SWB input
  int freq_avg_ic;       // initial bin for averaging nlp gain
  int flag_Hband_cn;     // for comfort noise
  float cn_scale_Hband;  // scale for comfort noise in H band

  int delay_metrics_delivered;
  int delay_histogram[kHistorySizeBlocks];
  int num_delay_values;
  int delay_median;
  int delay_std;
  float fraction_poor_delays;
  int delay_logging_enabled;
  void* delay_estimator_farend;
  void* delay_estimator;
  // Variables associated with delay correction through signal based delay
  // estimation feedback.
  int signal_delay_correction;
  int previous_delay;
  int delay_correction_count;
  int shift_offset;
  float delay_quality_threshold;
  int frame_count;

  // 0 = delay agnostic mode (signal based delay correction) disabled.
  // Otherwise enabled.
  int delay_agnostic_enabled;
  // 1 = extended filter mode enabled, 0 = disabled.
  int extended_filter_enabled;
  // 1 = next generation aec mode enabled, 0 = disabled.
  int aec3_enabled;
  bool refined_adaptive_filter_enabled;

  // Runtime selection of number of filter partitions.
  int num_partitions;

  // Flag that extreme filter divergence has been detected by the Echo
  // Suppressor.
  int extreme_filter_divergence;
};

typedef void (*WebRtcAecFilterFar)(
    int num_partitions,
    int x_fft_buf_block_pos,
    float x_fft_buf[2][kExtendedNumPartitions * PART_LEN1],
    float h_fft_buf[2][kExtendedNumPartitions * PART_LEN1],
    float y_fft[2][PART_LEN1]);
extern WebRtcAecFilterFar WebRtcAec_FilterFar;
typedef void (*WebRtcAecScaleErrorSignal)(float mu,
                                          float error_threshold,
                                          float x_pow[PART_LEN1],
                                          float ef[2][PART_LEN1]);
extern WebRtcAecScaleErrorSignal WebRtcAec_ScaleErrorSignal;
typedef void (*WebRtcAecFilterAdaptation)(
    int num_partitions,
    int x_fft_buf_block_pos,
    float x_fft_buf[2][kExtendedNumPartitions * PART_LEN1],
    float e_fft[2][PART_LEN1],
    float h_fft_buf[2][kExtendedNumPartitions * PART_LEN1]);
extern WebRtcAecFilterAdaptation WebRtcAec_FilterAdaptation;

typedef void (*WebRtcAecOverdrive)(float overdrive_scaling,
                                   const float hNlFb,
                                   float hNl[PART_LEN1]);
extern WebRtcAecOverdrive WebRtcAec_Overdrive;

typedef void (*WebRtcAecSuppress)(const float hNl[PART_LEN1],
                                  float efw[2][PART_LEN1]);
extern WebRtcAecSuppress WebRtcAec_Suppress;

typedef void (*WebRtcAecComputeCoherence)(const CoherenceState* coherence_state,
                                          float* cohde,
                                          float* cohxd);
extern WebRtcAecComputeCoherence WebRtcAec_ComputeCoherence;

typedef void (*WebRtcAecUpdateCoherenceSpectra)(int mult,
                                                bool extended_filter_enabled,
                                                float efw[2][PART_LEN1],
                                                float dfw[2][PART_LEN1],
                                                float xfw[2][PART_LEN1],
                                                CoherenceState* coherence_state,
                                                short* filter_divergence_state,
                                                int* extreme_filter_divergence);
extern WebRtcAecUpdateCoherenceSpectra WebRtcAec_UpdateCoherenceSpectra;

typedef int (*WebRtcAecPartitionDelay)(
    int num_partitions,
    float h_fft_buf[2][kExtendedNumPartitions * PART_LEN1]);
extern WebRtcAecPartitionDelay WebRtcAec_PartitionDelay;

typedef void (*WebRtcAecStoreAsComplex)(const float* data,
                                        float data_complex[2][PART_LEN1]);
extern WebRtcAecStoreAsComplex WebRtcAec_StoreAsComplex;

typedef void (*WebRtcAecWindowData)(float* x_windowed, const float* x);
extern WebRtcAecWindowData WebRtcAec_WindowData;

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC_AEC_CORE_INTERNAL_H_
