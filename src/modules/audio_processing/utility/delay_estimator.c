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

int WebRtc_FreeBinaryDelayEstimator(BinaryDelayEstimator_t* handle) {
  assert(handle != NULL);

  if (handle->mean_bit_counts != NULL) {
    free(handle->mean_bit_counts);
    handle->mean_bit_counts = NULL;
  }
  if (handle->bit_counts != NULL) {
    free(handle->bit_counts);
    handle->bit_counts = NULL;
  }
  if (handle->binary_far_history != NULL) {
    free(handle->binary_far_history);
    handle->binary_far_history = NULL;
  }
  if (handle->binary_near_history != NULL) {
    free(handle->binary_near_history);
    handle->binary_near_history = NULL;
  }
  if (handle->delay_histogram != NULL) {
    free(handle->delay_histogram);
    handle->delay_histogram = NULL;
  }

  free(handle);

  return 0;
}

int WebRtc_CreateBinaryDelayEstimator(BinaryDelayEstimator_t** handle,
                                      int max_delay,
                                      int lookahead) {
  BinaryDelayEstimator_t* self = NULL;
  int history_size = max_delay + lookahead;

  if (handle == NULL) {
    return -1;
  }
  if (max_delay < 0) {
    return -1;
  }
  if (lookahead < 0) {
    return -1;
  }
  if (history_size < 2) {
    // Must be this large for buffer shifting.
    return -1;
  }

  self = malloc(sizeof(BinaryDelayEstimator_t));
  *handle = self;
  if (self == NULL) {
    return -1;
  }

  self->mean_bit_counts = NULL;
  self->bit_counts = NULL;
  self->binary_far_history = NULL;
  self->delay_histogram = NULL;

  self->history_size = history_size;
  self->near_history_size = lookahead + 1;

  // Allocate memory for spectrum buffers
  self->mean_bit_counts = malloc(history_size * sizeof(int32_t));
  if (self->mean_bit_counts == NULL) {
    WebRtc_FreeBinaryDelayEstimator(self);
    self = NULL;
    return -1;
  }
  self->bit_counts = malloc(history_size * sizeof(int32_t));
  if (self->bit_counts == NULL) {
    WebRtc_FreeBinaryDelayEstimator(self);
    self = NULL;
    return -1;
  }
  // Allocate memory for history buffers
  self->binary_far_history = malloc(history_size * sizeof(uint32_t));
  if (self->binary_far_history == NULL) {
    WebRtc_FreeBinaryDelayEstimator(self);
    self = NULL;
    return -1;
  }
  self->binary_near_history = malloc(self->near_history_size *
      sizeof(uint32_t));
  if (self->binary_near_history == NULL) {
    WebRtc_FreeBinaryDelayEstimator(self);
    self = NULL;
    return -1;
  }
  self->delay_histogram = malloc(history_size * sizeof(int));
  if (self->delay_histogram == NULL) {
    WebRtc_FreeBinaryDelayEstimator(self);
    self = NULL;
    return -1;
  }

  return 0;
}

int WebRtc_InitBinaryDelayEstimator(BinaryDelayEstimator_t* handle) {
  assert(handle != NULL);

  memset(handle->mean_bit_counts, 0, sizeof(int32_t) * handle->history_size);
  memset(handle->bit_counts, 0, sizeof(int32_t) * handle->history_size);
  memset(handle->binary_far_history, 0,
         sizeof(uint32_t) * handle->history_size);
  memset(handle->binary_near_history, 0,
         sizeof(uint32_t) * handle->near_history_size);
  memset(handle->delay_histogram, 0, sizeof(int) * handle->history_size);

  handle->vad_counter = 0;

  // Default value to return if we're unable to estimate. -1 is used for
  // errors.
  handle->last_delay = -2;

  return 0;
}

int WebRtc_ProcessBinarySpectrum(BinaryDelayEstimator_t* handle,
                                 uint32_t binary_far_spectrum,
                                 uint32_t binary_near_spectrum,
                                 int vad_value) {
  const int kVadCountThreshold = 25;
  const int kMaxHistogram = 600;

  int histogram_bin = 0;
  int i = 0;
  int max_histogram_level = 0;
  int min_position = -1;

  int32_t bit_counts_tmp = 0;

  assert(handle != NULL);
  // Shift binary spectrum history
  memmove(&(handle->binary_far_history[1]), &(handle->binary_far_history[0]),
          (handle->history_size - 1) * sizeof(uint32_t));
  // Insert new binary spectrum
  handle->binary_far_history[0] = binary_far_spectrum;

  if (handle->near_history_size > 1) {
    memmove(&(handle->binary_near_history[1]),
            &(handle->binary_near_history[0]),
            (handle->near_history_size - 1) * sizeof(uint32_t));
    handle->binary_near_history[0] = binary_near_spectrum;
    binary_near_spectrum =
        handle->binary_near_history[handle->near_history_size - 1];
  }

  // Compare with delayed spectra
  BitCountComparison(binary_near_spectrum,
                     handle->binary_far_history,
                     handle->history_size,
                     handle->bit_counts);

  // Smooth bit count curve
  for (i = 0; i < handle->history_size; i++) {
    // Update sum
    // |bit_counts| is constrained to [0, 32], meaning we can smooth with a
    // factor up to 2^26. We use Q9.
    bit_counts_tmp = WEBRTC_SPL_LSHIFT_W32(handle->bit_counts[i], 9); // Q9
    WebRtc_MeanEstimatorFix(bit_counts_tmp, 9, &(handle->mean_bit_counts[i]));
  }

  // Find minimum position of bit count curve
  min_position = (int) WebRtcSpl_MinIndexW32(handle->mean_bit_counts,
                                             (int16_t) handle->history_size);

  // If the far end has been active sufficiently long, begin accumulating a
  // histogram of the minimum positions. Search for the maximum bin to
  // determine the delay.
  if (vad_value == 1) {
    if (handle->vad_counter >= kVadCountThreshold) {
      // Increment the histogram at the current minimum position.
      if (handle->delay_histogram[min_position] < kMaxHistogram) {
        handle->delay_histogram[min_position] += 3;
      }

      for (i = 0; i < handle->history_size; i++) {
        histogram_bin = handle->delay_histogram[i];

        // Decrement the histogram bin.
        if (histogram_bin > 0) {
          histogram_bin--;
          handle->delay_histogram[i] = histogram_bin;
          // Select the histogram index corresponding to the maximum bin as the
          // delay.
          if (histogram_bin > max_histogram_level) {
            max_histogram_level = histogram_bin;
            handle->last_delay = i;
          }
        }
      }
    } else {
      handle->vad_counter++;
    }
  } else {
    handle->vad_counter = 0;
  }

  return handle->last_delay;
}

int WebRtc_binary_last_delay(BinaryDelayEstimator_t* handle) {
  assert(handle != NULL);
  return handle->last_delay;
}

int WebRtc_history_size(BinaryDelayEstimator_t* handle) {
  assert(handle != NULL);
  return handle->history_size;
}

void WebRtc_MeanEstimatorFix(int32_t new_value,
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
