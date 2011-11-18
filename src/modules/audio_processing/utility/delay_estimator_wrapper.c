/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "delay_estimator_wrapper.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "delay_estimator.h"
#include "signal_processing_library.h"

typedef union {
  float float_;
  int32_t int32_;
} SpectrumType_t;

typedef struct {
  // Pointers to mean values of spectrum
  SpectrumType_t* mean_far_spectrum;
  SpectrumType_t* mean_near_spectrum;

  // Spectrum size
  int spectrum_size;

  // Binary spectrum based delay estimator
  BinaryDelayEstimator_t* binary_handle;
} DelayEstimator_t;

// Only bit |kBandFirst| through bit |kBandLast| are processed
// |kBandFirst| - |kBandLast| must be < 32
static const int kBandFirst = 12;
static const int kBandLast = 43;

static __inline uint32_t SetBit(uint32_t in, int pos) {
  uint32_t mask = WEBRTC_SPL_LSHIFT_W32(1, pos);
  uint32_t out = (in | mask);

  return out;
}

// Calculates the mean recursively. Same version as WebRtc_MeanEstimatorFix(),
// but for float.
//
// Inputs:
//    - new_value             : new additional value.
//    - scale                 : scale for smoothing (should be less than 1.0).
//
// Input/Output:
//    - mean_value            : pointer to the mean value for updating.
//
static void MeanEstimatorFloat(float new_value,
                               float scale,
                               float* mean_value) {
  assert(scale < 1.0f);
  // mean_new = mean_value + ((new_value - mean_value) * scale);
  float diff = (new_value - *mean_value) * scale;
  float mean_new = *mean_value + diff;

  *mean_value = mean_new;
}

// Computes the binary spectrum by comparing the input |spectrum| with a
// |threshold_spectrum|. Float and fixed point versions.
//
// Inputs:
//      - spectrum            : Spectrum of which the binary spectrum should be
//                              calculated.
//      - threshold_spectrum  : Threshold spectrum with which the input
//                              spectrum is compared.
// Return:
//      - out                 : Binary spectrum
//
static uint32_t BinarySpectrumFix(uint16_t* spectrum,
                                  SpectrumType_t* threshold_spectrum) {
  int k = kBandFirst;
  uint32_t out = 0;

  for (; k <= kBandLast; k++) {
    WebRtc_MeanEstimatorFix((int32_t) spectrum[k],
                            6,
                            &(threshold_spectrum[k].int32_));
    if (spectrum[k] > threshold_spectrum[k].int32_) {
      out = SetBit(out, k - kBandFirst);
    }
  }

  return out;
}

static uint32_t BinarySpectrumFloat(float* spectrum,
                                    SpectrumType_t* threshold_spectrum) {
  int k = kBandFirst;
  uint32_t out = 0;
  float scale = 1 / 64.0;

  for (; k <= kBandLast; k++) {
    MeanEstimatorFloat(spectrum[k], scale, &(threshold_spectrum[k].float_));
    if (spectrum[k] > threshold_spectrum[k].float_) {
      out = SetBit(out, k - kBandFirst);
    }
  }

  return out;
}

int WebRtc_FreeDelayEstimator(void* handle) {
  DelayEstimator_t* self = (DelayEstimator_t*) handle;

  if (self == NULL) {
    return -1;
  }

  if (self->mean_far_spectrum != NULL) {
    free(self->mean_far_spectrum);
    self->mean_far_spectrum = NULL;
  }
  if (self->mean_near_spectrum != NULL) {
    free(self->mean_near_spectrum);
    self->mean_near_spectrum = NULL;
  }

  WebRtc_FreeBinaryDelayEstimator(self->binary_handle);

  free(self);

  return 0;
}

int WebRtc_CreateDelayEstimator(void** handle,
                                int spectrum_size,
                                int history_size) {
  DelayEstimator_t *self = NULL;

  // Check if the sub band used in the delay estimation is small enough to
  // fit the binary spectra in a uint32.
  assert(kBandLast - kBandFirst < 32);

  if (handle == NULL) {
    return -1;
  }
  if (spectrum_size < kBandLast) {
    return -1;
  }

  self = malloc(sizeof(DelayEstimator_t));
  *handle = self;
  if (self == NULL) {
    return -1;
  }

  self->mean_far_spectrum = NULL;
  self->mean_near_spectrum = NULL;

  // Create binary delay estimator.
  if (WebRtc_CreateBinaryDelayEstimator(&self->binary_handle,
                                        history_size) != 0) {
    WebRtc_FreeDelayEstimator(self);
    self = NULL;
    return -1;
  }
  // Allocate memory for spectrum buffers
  self->mean_far_spectrum = malloc(spectrum_size * sizeof(SpectrumType_t));
  if (self->mean_far_spectrum == NULL) {
    WebRtc_FreeDelayEstimator(self);
    self = NULL;
    return -1;
  }
  self->mean_near_spectrum = malloc(spectrum_size * sizeof(SpectrumType_t));
  if (self->mean_near_spectrum == NULL) {
    WebRtc_FreeDelayEstimator(self);
    self = NULL;
    return -1;
  }

  self->spectrum_size = spectrum_size;

  return 0;
}

int WebRtc_InitDelayEstimator(void* handle) {
  DelayEstimator_t* self = (DelayEstimator_t*) handle;

  if (self == NULL) {
    return -1;
  }

  // Initialize binary delay estimator
  if (WebRtc_InitBinaryDelayEstimator(self->binary_handle) != 0) {
    return -1;
  }
  // Set averaged far and near end spectra to zero
  memset(self->mean_far_spectrum,
         0,
         sizeof(SpectrumType_t) * self->spectrum_size);
  memset(self->mean_near_spectrum,
         0,
         sizeof(SpectrumType_t) * self->spectrum_size);

  return 0;
}

int WebRtc_DelayEstimatorProcessFix(void* handle,
                                    uint16_t* far_spectrum,
                                    uint16_t* near_spectrum,
                                    int spectrum_size,
                                    int far_q,
                                    int vad_value) {
  DelayEstimator_t* self = (DelayEstimator_t*) handle;
  uint32_t binary_far_spectrum = 0;
  uint32_t binary_near_spectrum = 0;

  if (self == NULL) {
    return -1;
  }
  if (far_spectrum == NULL) {
    // Empty far end spectrum
    return -1;
  }
  if (near_spectrum == NULL) {
    // Empty near end spectrum
    return -1;
  }
  if (spectrum_size != self->spectrum_size) {
    // Data sizes don't match
    return -1;
  }
  if (far_q > 15) {
    // If |far_q| is larger than 15 we cannot guarantee no wrap around
    return -1;
  }

  // Get binary spectra
  binary_far_spectrum = BinarySpectrumFix(far_spectrum,
                                          self->mean_far_spectrum);
  binary_near_spectrum = BinarySpectrumFix(near_spectrum,
                                           self->mean_near_spectrum);

  return WebRtc_ProcessBinarySpectrum(self->binary_handle,
                                      binary_far_spectrum,
                                      binary_near_spectrum,
                                      vad_value);
}

int WebRtc_DelayEstimatorProcessFloat(void* handle,
                                      float* far_spectrum,
                                      float* near_spectrum,
                                      int spectrum_size,
                                      int vad_value) {
  DelayEstimator_t* self = (DelayEstimator_t*) handle;
  uint32_t binary_far_spectrum = 0;
  uint32_t binary_near_spectrum = 0;

  if (self == NULL) {
    return -1;
  }
  if (far_spectrum == NULL) {
    // Empty far end spectrum
    return -1;
  }
  if (near_spectrum == NULL) {
    // Empty near end spectrum
    return -1;
  }
  if (spectrum_size != self->spectrum_size) {
    // Data sizes don't match
    return -1;
  }

  // Get binary spectra
  binary_far_spectrum = BinarySpectrumFloat(far_spectrum,
                                            self->mean_far_spectrum);
  binary_near_spectrum = BinarySpectrumFloat(near_spectrum,
                                             self->mean_near_spectrum);

  return WebRtc_ProcessBinarySpectrum(self->binary_handle,
                                      binary_far_spectrum,
                                      binary_near_spectrum,
                                      vad_value);
}

int WebRtc_last_delay(void* handle) {
  DelayEstimator_t* self = (DelayEstimator_t*) handle;

  if (self == NULL) {
    return -1;
  }

  return WebRtc_binary_last_delay(self->binary_handle);
}
