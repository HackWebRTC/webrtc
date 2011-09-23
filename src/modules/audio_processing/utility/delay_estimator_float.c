/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "delay_estimator_float.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "delay_estimator.h"
#include "signal_processing_library.h"

typedef struct {
  // Fixed point spectra
  uint16_t* far_spectrum_u16;
  uint16_t* near_spectrum_u16;

  // Far end history variables
  float* far_history;
  int far_history_pos;

  // Fixed point delay estimator
  void* fixed_handle;

} DelayEstimatorFloat_t;

// Moves the pointer to the next buffer entry and inserts new far end spectrum.
// Only used when alignment is enabled.
//
// Inputs:
//      - self          : Pointer to the delay estimation instance
//      - far_spectrum  : Pointer to the far end spectrum
//
static void UpdateFarHistory(DelayEstimatorFloat_t* self, float* far_spectrum) {
  int spectrum_size = WebRtc_spectrum_size(self->fixed_handle);
  // Get new buffer position
  self->far_history_pos++;
  if (self->far_history_pos >= WebRtc_history_size(self->fixed_handle)) {
    self->far_history_pos = 0;
  }
  // Update far end spectrum buffer
  memcpy(&(self->far_history[self->far_history_pos * spectrum_size]),
         far_spectrum,
         sizeof(float) * spectrum_size);
}

int WebRtc_FreeDelayEstimatorFloat(void* handle) {
  DelayEstimatorFloat_t* self = (DelayEstimatorFloat_t*) handle;

  if (self == NULL) {
    return -1;
  }

  if (self->far_history != NULL) {
    free(self->far_history);
    self->far_history = NULL;
  }
  if (self->far_spectrum_u16 != NULL) {
    free(self->far_spectrum_u16);
    self->far_spectrum_u16 = NULL;
  }
  if (self->near_spectrum_u16 != NULL) {
    free(self->near_spectrum_u16);
    self->near_spectrum_u16 = NULL;
  }

  WebRtc_FreeDelayEstimator(self->fixed_handle);
  free(self);

  return 0;
}

int WebRtc_CreateDelayEstimatorFloat(void** handle,
                                     int spectrum_size,
                                     int history_size,
                                     int enable_alignment) {
  DelayEstimatorFloat_t *self = NULL;
  if ((enable_alignment != 0) && (enable_alignment != 1)) {
    return -1;
  }

  self = malloc(sizeof(DelayEstimatorFloat_t));
  *handle = self;
  if (self == NULL) {
    return -1;
  }

  self->far_history = NULL;
  self->far_spectrum_u16 = NULL;
  self->near_spectrum_u16 = NULL;

  // Create fixed point core delay estimator
  if (WebRtc_CreateDelayEstimator(&self->fixed_handle,
                                  spectrum_size,
                                  history_size,
                                  enable_alignment) != 0) {
    WebRtc_FreeDelayEstimatorFloat(self);
    self = NULL;
    return -1;
  }

  // Allocate memory for far history buffer
  if (enable_alignment) {
    self->far_history = malloc(spectrum_size * history_size * sizeof(float));
    if (self->far_history == NULL) {
      WebRtc_FreeDelayEstimatorFloat(self);
      self = NULL;
      return -1;
    }
  }
  // Allocate memory for fixed point spectra
  self->far_spectrum_u16 = malloc(spectrum_size * sizeof(uint16_t));
  if (self->far_spectrum_u16 == NULL) {
    WebRtc_FreeDelayEstimatorFloat(self);
    self = NULL;
    return -1;
  }
  self->near_spectrum_u16 = malloc(spectrum_size * sizeof(uint16_t));
  if (self->near_spectrum_u16 == NULL) {
    WebRtc_FreeDelayEstimatorFloat(self);
    self = NULL;
    return -1;
  }

  return 0;
}

int WebRtc_InitDelayEstimatorFloat(void* handle) {
  DelayEstimatorFloat_t* self = (DelayEstimatorFloat_t*) handle;

  if (self == NULL) {
    return -1;
  }

  if (WebRtc_InitDelayEstimator(self->fixed_handle) != 0) {
    return -1;
  }

  {
    int history_size = WebRtc_history_size(self->fixed_handle);
    int spectrum_size = WebRtc_spectrum_size(self->fixed_handle);
    if (WebRtc_is_alignment_enabled(self->fixed_handle) == 1) {
      // Set far end histories to zero
      memset(self->far_history,
             0,
             sizeof(float) * spectrum_size * history_size);
      self->far_history_pos = history_size;
    }
    // Set fixed point spectra to zero
    memset(self->far_spectrum_u16, 0, sizeof(uint16_t) * spectrum_size);
    memset(self->near_spectrum_u16, 0, sizeof(uint16_t) * spectrum_size);
  }

  return 0;
}

int WebRtc_DelayEstimatorProcessFloat(void* handle,
                                      float* far_spectrum,
                                      float* near_spectrum,
                                      int spectrum_size,
                                      int vad_value) {
  DelayEstimatorFloat_t* self = (DelayEstimatorFloat_t*) handle;

  const float kFftSize = (float) (2 * (spectrum_size - 1));
  const float kLogOf2Inverse = 1.4426950f;
  float max_value = 0.0f;
  float scaling = 0;

  int far_q = 0;
  int scaling_log = 0;
  int i = 0;

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
  if (spectrum_size != WebRtc_spectrum_size(self->fixed_handle)) {
    // Data sizes don't match
    return -1;
  }

  // Convert floating point spectrum to fixed point
  // 1) Find largest value
  // 2) Scale largest value to fit in Word16
  for (i = 0; i < spectrum_size; ++i) {
    if (near_spectrum[i] > max_value) {
      max_value = near_spectrum[i];
    }
  }
  // Find the largest possible scaling that is a multiple of two.
  // With largest we mean to fit in a Word16.
  // TODO(bjornv): I've taken the size of FFT into account, since there is a
  // different scaling in float vs fixed point FFTs. I'm not completely sure
  // this is necessary.
  scaling_log = 14 - (int) (log(max_value / kFftSize + 1) * kLogOf2Inverse);
  scaling = (float) (1 << scaling_log) / kFftSize;
  for (i = 0; i < spectrum_size; ++i) {
    self->near_spectrum_u16[i] = (uint16_t) (near_spectrum[i] * scaling);
  }

  // Same for far end
  max_value = 0.0f;
  for (i = 0; i < spectrum_size; ++i) {
    if (far_spectrum[i] > max_value) {
      max_value = far_spectrum[i];
    }
  }
  // Find the largest possible scaling that is a multiple of two.
  // With largest we mean to fit in a Word16.
  scaling_log = 14 - (int) (log(max_value / kFftSize + 1) * kLogOf2Inverse);
  scaling = (float) (1 << scaling_log) / kFftSize;
  for (i = 0; i < spectrum_size; ++i) {
    self->far_spectrum_u16[i] = (uint16_t) (far_spectrum[i] * scaling);
  }
  far_q = (int) scaling_log;
  assert(far_q < 16); // Catch too large scaling, which should never be able to
                      // occur.

  if (WebRtc_is_alignment_enabled(self->fixed_handle) == 1) {
    // Update far end history
    UpdateFarHistory(self, far_spectrum);
  }

  return WebRtc_DelayEstimatorProcess(self->fixed_handle,
                                      self->far_spectrum_u16,
                                      self->near_spectrum_u16,
                                      spectrum_size,
                                      far_q,
                                      vad_value);
}

const float* WebRtc_AlignedFarendFloat(void* handle, int far_spectrum_size) {
  DelayEstimatorFloat_t* self = (DelayEstimatorFloat_t*) handle;
  int buffer_pos = 0;

  if (self == NULL) {
    return NULL;
  }
  if (far_spectrum_size != WebRtc_spectrum_size(self->fixed_handle)) {
    return NULL;
  }
  if (WebRtc_is_alignment_enabled(self->fixed_handle) != 1) {
    return NULL;
  }

  // Get buffer position
  buffer_pos = self->far_history_pos - WebRtc_last_delay(self->fixed_handle);
  if (buffer_pos < 0) {
    buffer_pos += WebRtc_history_size(self->fixed_handle);
  }
  // Return pointer to far end spectrum
  return (self->far_history + (buffer_pos * far_spectrum_size));
}

int WebRtc_last_delay_float(void* handle) {
  DelayEstimatorFloat_t* self = (DelayEstimatorFloat_t*) handle;

  if (self == NULL) {
    return -1;
  }

  return WebRtc_last_delay(self->fixed_handle);
}

int WebRtc_is_alignment_enabled_float(void* handle) {
  DelayEstimatorFloat_t* self = (DelayEstimatorFloat_t*) handle;

  if (self == NULL) {
    return -1;
  }

  return WebRtc_is_alignment_enabled(self->fixed_handle);
}
