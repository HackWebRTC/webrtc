/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "delay_estimator.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "signal_processing_library.h"

typedef struct {
  // Pointers to mean values of spectrum and bit counts
  int32_t* mean_far_spectrum;
  int32_t* mean_near_spectrum;
  int32_t* mean_bit_counts;

  // Arrays only used locally in DelayEstimatorProcess() but whose size
  // is determined at run-time.
  int32_t* bit_counts;
  int32_t* far_spectrum_32;
  int32_t* near_spectrum_32;

  // Binary history variables
  uint32_t* binary_far_history;

  // Far end history variables
  uint16_t* far_history;
  int far_history_pos;
  int* far_q_domains;

  // Delay histogram variables
  int* delay_histogram;
  int vad_counter;

  // Delay memory
  int last_delay;

  // Used to enable far end alignment. If it is disabled, only delay values are
  // produced
  int alignment_enabled;

  // Buffer size parameters
  int history_size;
  int spectrum_size;

} DelayEstimator_t;

// Only bit |kBandFirst| through bit |kBandLast| are processed
// |kBandFirst| - |kBandLast| must be < 32
static const int kBandFirst = 12;
static const int kBandLast = 43;

static __inline uint32_t SetBit(uint32_t in, int32_t pos) {
  uint32_t mask = WEBRTC_SPL_LSHIFT_W32(1, pos);
  uint32_t out = (in | mask);

  return out;
}

// Compares the |binary_vector| with all rows of the |binary_matrix| and counts
// per row the number of times they have the same value.
//
// Inputs:
//      - binary_vector     : binary "vector" stored in a long
//      - binary_matrix     : binary "matrix" stored as a vector of long
//      - matrix_size       : size of binary "matrix"
//
// Output:
//      - bit_counts        : "Vector" stored as a long, containing for each
//                            row the number of times the matrix row and the
//                            input vector have the same value
//
static void BitCountComparison(uint32_t binary_vector,
                               const uint32_t* binary_matrix,
                               int matrix_size,
                               int32_t* bit_counts) {
  int n = 0;
  uint32_t a = binary_vector;
  register uint32_t tmp;

  // compare |binary_vector| with all rows of the |binary_matrix|
  for (; n < matrix_size; n++) {
    a = (binary_vector ^ binary_matrix[n]);
    // Returns bit counts in tmp
    tmp = a - ((a >> 1) & 033333333333) - ((a >> 2) & 011111111111);
    tmp = ((tmp + (tmp >> 3)) & 030707070707);
    tmp = (tmp + (tmp >> 6));
    tmp = (tmp + (tmp >> 12) + (tmp >> 24)) & 077;

    bit_counts[n] = (int32_t) tmp;
  }
}

// Computes the binary spectrum by comparing the input |spectrum| with a
// |threshold_spectrum|.
//
// Inputs:
//      - spectrum            : Spectrum of which the binary spectrum should be
//                              calculated.
//      - threshold_spectrum  : Threshold spectrum with which the input
//                              spectrum is compared.
// Return:
//      - out                 : Binary spectrum
//
static uint32_t BinarySpectrum(int32_t* spectrum, int32_t* threshold_spectrum) {
  int k = kBandFirst;
  uint32_t out = 0;

  for (; k <= kBandLast; k++) {
    if (spectrum[k] > threshold_spectrum[k]) {
      out = SetBit(out, k - kBandFirst);
    }
  }

  return out;
}

//   Calculates the mean recursively.
//
//   Inputs:
//      - new_value     : new additional value
//      - factor        : factor for smoothing
//
//   Input/Output:
//      - mean_value    : pointer to the mean value that should be updated
//
static void MeanEstimator(const int32_t new_value,
                          int factor,
                          int32_t* mean_value) {
  int32_t mean_new = *mean_value;
  int32_t diff = new_value - mean_new;

  // mean_new = mean_value + ((new_value - mean_value) >> factor);
  if (diff < 0) {
    diff = -WEBRTC_SPL_RSHIFT_W32(-diff, factor);
  } else {
    diff = WEBRTC_SPL_RSHIFT_W32(diff, factor);
  }
  mean_new += diff;

  *mean_value = mean_new;
}

// Moves the pointer to the next entry and inserts |far_spectrum| and
// corresponding Q-domain in its buffer.
//
// Inputs:
//      - self          : Pointer to the delay estimation instance
//      - far_spectrum  : Pointer to the far end spectrum
//      - far_q         : Q-domain of far end spectrum
//
static void UpdateFarHistory(DelayEstimator_t* self,
                             uint16_t* far_spectrum,
                             int far_q) {
  // Get new buffer position
  self->far_history_pos++;
  if (self->far_history_pos >= self->history_size) {
    self->far_history_pos = 0;
  }
  // Update Q-domain buffer
  self->far_q_domains[self->far_history_pos] = far_q;
  // Update far end spectrum buffer
  memcpy(&(self->far_history[self->far_history_pos * self->spectrum_size]),
         far_spectrum,
         sizeof(uint16_t) * self->spectrum_size);
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
  if (self->mean_bit_counts != NULL) {
    free(self->mean_bit_counts);
    self->mean_bit_counts = NULL;
  }
  if (self->bit_counts != NULL) {
    free(self->bit_counts);
    self->bit_counts = NULL;
  }
  if (self->far_spectrum_32 != NULL) {
    free(self->far_spectrum_32);
    self->far_spectrum_32 = NULL;
  }
  if (self->near_spectrum_32 != NULL) {
    free(self->near_spectrum_32);
    self->near_spectrum_32 = NULL;
  }
  if (self->binary_far_history != NULL) {
    free(self->binary_far_history);
    self->binary_far_history = NULL;
  }
  if (self->far_history != NULL) {
    free(self->far_history);
    self->far_history = NULL;
  }
  if (self->far_q_domains != NULL) {
    free(self->far_q_domains);
    self->far_q_domains = NULL;
  }
  if (self->delay_histogram != NULL) {
    free(self->delay_histogram);
    self->delay_histogram = NULL;
  }

  free(self);

  return 0;
}

int WebRtc_CreateDelayEstimator(void** handle,
                                int spectrum_size,
                                int history_size,
                                int enable_alignment) {
  DelayEstimator_t *self = NULL;

  // Check if the sub band used in the delay estimation is small enough to
  // fit the binary spectra in a uint32.
  assert(kBandLast - kBandFirst < 32);

  if (spectrum_size < kBandLast) {
    return -1;
  }
  if (history_size < 0) {
    return -1;
  }
  if ((enable_alignment != 0) && (enable_alignment != 1)) {
    return -1;
  }

  self = malloc(sizeof(DelayEstimator_t));
  *handle = self;
  if (self == NULL) {
    return -1;
  }

  self->mean_far_spectrum = NULL;
  self->mean_near_spectrum = NULL;
  self->mean_bit_counts = NULL;
  self->bit_counts = NULL;
  self->far_spectrum_32 = NULL;
  self->near_spectrum_32 = NULL;
  self->binary_far_history = NULL;
  self->far_history = NULL;
  self->far_q_domains = NULL;
  self->delay_histogram = NULL;

  // Allocate memory for spectrum buffers
  self->mean_far_spectrum = malloc(spectrum_size * sizeof(int32_t));
  if (self->mean_far_spectrum == NULL) {
    WebRtc_FreeDelayEstimator(self);
    self = NULL;
    return -1;
  }
  self->mean_near_spectrum = malloc(spectrum_size * sizeof(int32_t));
  if (self->mean_near_spectrum == NULL) {
    WebRtc_FreeDelayEstimator(self);
    self = NULL;
    return -1;
  }
  self->mean_bit_counts = malloc(history_size * sizeof(int32_t));
  if (self->mean_bit_counts == NULL) {
    WebRtc_FreeDelayEstimator(self);
    self = NULL;
    return -1;
  }
  self->bit_counts = malloc(history_size * sizeof(int32_t));
  if (self->bit_counts == NULL) {
    WebRtc_FreeDelayEstimator(self);
    self = NULL;
    return -1;
  }
  self->far_spectrum_32 = malloc(spectrum_size * sizeof(int32_t));
  if (self->far_spectrum_32 == NULL) {
    WebRtc_FreeDelayEstimator(self);
    self = NULL;
    return -1;
  }
  self->near_spectrum_32 = malloc(spectrum_size * sizeof(int32_t));
  if (self->near_spectrum_32 == NULL) {
    WebRtc_FreeDelayEstimator(self);
    self = NULL;
    return -1;
  }
  // Allocate memory for history buffers
  self->binary_far_history = malloc(history_size * sizeof(uint32_t));
  if (self->binary_far_history == NULL) {
    WebRtc_FreeDelayEstimator(self);
    self = NULL;
    return -1;
  }
  if (enable_alignment) {
    self->far_history = malloc(spectrum_size * history_size * sizeof(uint16_t));
    if (self->far_history == NULL) {
      WebRtc_FreeDelayEstimator(self);
      self = NULL;
      return -1;
    }
    self->far_q_domains = malloc(history_size * sizeof(int));
    if (self->far_q_domains == NULL) {
      WebRtc_FreeDelayEstimator(self);
      self = NULL;
      return -1;
    }
  }
  self->delay_histogram = malloc(history_size * sizeof(int));
  if (self->delay_histogram == NULL) {
    WebRtc_FreeDelayEstimator(self);
    self = NULL;
    return -1;
  }

  self->spectrum_size = spectrum_size;
  self->history_size = history_size;
  self->alignment_enabled = enable_alignment;

  return 0;
}

int WebRtc_InitDelayEstimator(void* handle) {
  DelayEstimator_t* self = (DelayEstimator_t*) handle;

  if (self == NULL) {
    return -1;
  }
  // Set averaged far and near end spectra to zero
  memset(self->mean_far_spectrum, 0, sizeof(int32_t) * self->spectrum_size);
  memset(self->mean_near_spectrum, 0, sizeof(int32_t) * self->spectrum_size);
  // Set averaged bit counts to zero
  memset(self->mean_bit_counts, 0, sizeof(int32_t) * self->history_size);
  memset(self->bit_counts, 0, sizeof(int32_t) * self->history_size);
  memset(self->far_spectrum_32, 0, sizeof(int32_t) * self->spectrum_size);
  memset(self->near_spectrum_32, 0, sizeof(int32_t) * self->spectrum_size);
  // Set far end histories to zero
  memset(self->binary_far_history, 0, sizeof(uint32_t) * self->history_size);
  if (self->alignment_enabled) {
    memset(self->far_history,
           0,
           sizeof(uint16_t) * self->spectrum_size * self->history_size);
    memset(self->far_q_domains, 0, sizeof(int) * self->history_size);
    self->far_history_pos = self->history_size;
  }
  // Set delay histogram to zero
  memset(self->delay_histogram, 0, sizeof(int) * self->history_size);
  // Set VAD counter to zero
  self->vad_counter = 0;
  // Set delay memory to zero
  self->last_delay = 0;

  return 0;
}

int WebRtc_DelayEstimatorProcess(void* handle,
                                 uint16_t* far_spectrum,
                                 uint16_t* near_spectrum,
                                 int spectrum_size,
                                 int far_q,
                                 int vad_value) {
  DelayEstimator_t* self = (DelayEstimator_t*) handle;

  const int kVadCountThreshold = 25;
  const int kMaxHistogram = 600;

  int histogram_bin = 0;
  int i = 0;
  int max_histogram_level = 0;
  int min_position = -1;

  uint32_t binary_far_spectrum = 0;
  uint32_t binary_near_spectrum = 0;

  int32_t bit_counts_tmp = 0;

  if (self == NULL) {
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

  if (self->alignment_enabled) {
    // Update far end history
    UpdateFarHistory(self, far_spectrum, far_q);
  } // Update the far and near end means
  for (i = 0; i < self->spectrum_size; i++) {
    self->far_spectrum_32[i] = (int32_t) far_spectrum[i];
    MeanEstimator(self->far_spectrum_32[i], 6, &(self->mean_far_spectrum[i]));

    self->near_spectrum_32[i] = (int32_t) near_spectrum[i];
    MeanEstimator(self->near_spectrum_32[i], 6, &(self->mean_near_spectrum[i]));
  }

  // Shift binary spectrum history
  memmove(&(self->binary_far_history[1]), &(self->binary_far_history[0]),
          (self->history_size - 1) * sizeof(uint32_t));

  // Get binary spectra
  binary_far_spectrum = BinarySpectrum(self->far_spectrum_32,
                                       self->mean_far_spectrum);
  binary_near_spectrum = BinarySpectrum(self->near_spectrum_32,
                                        self->mean_near_spectrum);
  // Insert new binary spectrum
  self->binary_far_history[0] = binary_far_spectrum;

  // Compare with delayed spectra
  BitCountComparison(binary_near_spectrum,
                     self->binary_far_history,
                     self->history_size,
                     self->bit_counts);

  // Smooth bit count curve
  for (i = 0; i < self->history_size; i++) {
    // Update sum
    // |bit_counts| is constrained to [0, 32], meaning we can smooth with a
    // factor up to 2^26. We use Q9.
    bit_counts_tmp = WEBRTC_SPL_LSHIFT_W32(self->bit_counts[i], 9); // Q9
    MeanEstimator(bit_counts_tmp, 9, &(self->mean_bit_counts[i]));
  }

  // Find minimum position of bit count curve
  min_position = (int) WebRtcSpl_MinIndexW32(self->mean_bit_counts,
                                             (int16_t) self->history_size);

  // If the far end has been active sufficiently long, begin accumulating a
  // histogram of the minimum positions. Search for the maximum bin to
  // determine the delay.
  if (vad_value == 1) {
    if (self->vad_counter >= kVadCountThreshold) {
      // Increment the histogram at the current minimum position.
      if (self->delay_histogram[min_position] < kMaxHistogram) {
        self->delay_histogram[min_position] += 3;
      }

      self->last_delay = 0;
      for (i = 0; i < self->history_size; i++) {
        histogram_bin = self->delay_histogram[i];

        // Decrement the histogram bin.
        if (histogram_bin > 0) {
          histogram_bin--;
          self->delay_histogram[i] = histogram_bin;
          // Select the histogram index corresponding to the maximum bin as the
          // delay.
          if (histogram_bin > max_histogram_level) {
            max_histogram_level = histogram_bin;
            self->last_delay = i;
          }
        }
      }
    } else {
      self->vad_counter++;
    }
  } else {
    self->vad_counter = 0;
  }

  return self->last_delay;
}

const uint16_t* WebRtc_AlignedFarend(void* handle,
                                     int far_spectrum_size,
                                     int* far_q) {
  DelayEstimator_t* self = (DelayEstimator_t*) handle;
  int buffer_position = 0;

  if (self == NULL) {
    return NULL;
  }
  if (far_spectrum_size != self->spectrum_size) {
    return NULL;
  }
  if (self->alignment_enabled == 0) {
    return NULL;
  }

  // Get buffer position
  buffer_position = self->far_history_pos - self->last_delay;
  if (buffer_position < 0) {
    buffer_position += self->history_size;
  }
  // Get Q-domain
  *far_q = self->far_q_domains[buffer_position];
  // Return far end spectrum
  return (self->far_history + (buffer_position * far_spectrum_size));

}

int WebRtc_last_delay(void* handle) {
  DelayEstimator_t* self = (DelayEstimator_t*) handle;

  if (self == NULL) {
    return -1;
  }

  return self->last_delay;
}

int WebRtc_history_size(void* handle) {
  DelayEstimator_t* self = (DelayEstimator_t*) handle;

  if (self == NULL) {
    return -1;
  }

  return self->history_size;
}

int WebRtc_spectrum_size(void* handle) {
  DelayEstimator_t* self = (DelayEstimator_t*) handle;

  if (self == NULL) {
    return -1;
  }

  return self->spectrum_size;
}

int WebRtc_is_alignment_enabled(void* handle) {
  DelayEstimator_t* self = (DelayEstimator_t*) handle;

  if (self == NULL) {
    return -1;
  }

  return self->alignment_enabled;
}
