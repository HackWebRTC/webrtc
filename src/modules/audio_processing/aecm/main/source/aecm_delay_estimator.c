/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "aecm_delay_estimator.h"

#include <assert.h>
#include <stdlib.h>

#include "signal_processing_library.h"
#include "typedefs.h"

typedef struct
{
    // Pointers to mean values of spectrum and bit counts
    WebRtc_Word32* mean_far_spectrum;
    WebRtc_Word32* mean_near_spectrum;
    WebRtc_Word32* mean_bit_counts;

    // Binary history variables
    WebRtc_UWord32* binary_far_history;

    // Far end history variables
    WebRtc_UWord16* far_history;
    int far_history_position;
    WebRtc_Word16* far_q_domains;

    // Delay histogram variables
    WebRtc_Word16* delay_histogram;
    WebRtc_Word16 vad_counter;

    // Delay memory
    int last_delay;

    // Buffer size parameters
    int history_size;
    int spectrum_size;

} DelayEstimator_t;

// Only bit |kBandFirst| through bit |kBandLast| are processed
// |kBandFirst| - |kBandLast| must be < 32
static const int kBandFirst = 12;
static const int kBandLast = 43;

static __inline WebRtc_UWord32 SetBit(WebRtc_UWord32 in,
                                      WebRtc_Word32 pos)
{
    WebRtc_UWord32 mask = WEBRTC_SPL_LSHIFT_W32(1, pos);
    WebRtc_UWord32 out = (in | mask);

    return out;
}

// Compares the binary vector |binary_vector| with all rows of the binary
// matrix |binary_matrix| and counts per row the number of times they have the
// same value.
// Input:
//      - binary_vector     : binary "vector" stored in a long
//      - binary_matrix     : binary "matrix" stored as a vector of long
//      - matrix_size       : size of binary "matrix"
// Output:
//      - bit_counts        : "Vector" stored as a long, containing for each
//                            row the number of times the matrix row and the
//                            input vector have the same value
//
static void BitCountComparison(const WebRtc_UWord32 binary_vector,
                               const WebRtc_UWord32* binary_matrix,
                               int matrix_size,
                               WebRtc_Word32* bit_counts)
{
    int n = 0;
    WebRtc_UWord32 a = binary_vector;
    register WebRtc_UWord32 tmp;

    // compare binary vector |binary_vector| with all rows of the binary matrix
    // |binary_matrix|
    for (; n < matrix_size; n++)
    {
        a = (binary_vector ^ binary_matrix[n]);
        // Returns bit counts in tmp
        tmp = a - ((a >> 1) & 033333333333) - ((a >> 2) & 011111111111);
        tmp = ((tmp + (tmp >> 3)) & 030707070707);
        tmp = (tmp + (tmp >> 6));
        tmp = (tmp + (tmp >> 12) + (tmp >> 24)) & 077;

        bit_counts[n] = (WebRtc_Word32)tmp;
    }
}

// Computes the binary spectrum by comparing the input |spectrum| with a
// |threshold_spectrum|.
//
// Input:
//      - spectrum              : Spectrum of which the binary spectrum should
//                                be calculated.
//      - threshold_spectrum    : Threshold spectrum with which the input
//                                spectrum is compared.
// Return:
//      - out                   : Binary spectrum
//
static WebRtc_UWord32 GetBinarySpectrum(WebRtc_Word32* spectrum,
                                        WebRtc_Word32* threshold_spectrum)
{
    int k = kBandFirst;
    WebRtc_UWord32 out = 0;

    for (; k <= kBandLast; k++)
    {
        if (spectrum[k] > threshold_spectrum[k])
        {
            out = SetBit(out, k - kBandFirst);
        }
    }

    return out;
}

//   Calculates the mean recursively.
//
//   Input:
//      - new_value     : new additional value
//      - factor        : factor for smoothing
//
//   Input/Output:
//      - mean_value    : pointer to the mean value that should be updated
//
static void MeanEstimator(const WebRtc_Word32 new_value,
                          int factor,
                          WebRtc_Word32* mean_value)
{
    WebRtc_Word32 mean_new = *mean_value;
    WebRtc_Word32 diff = new_value - mean_new;

    // mean_new = mean_value + ((new_value - mean_value) >> factor);
    if (diff < 0)
    {
        diff = -WEBRTC_SPL_RSHIFT_W32(-diff, factor);
    }
    else
    {
        diff = WEBRTC_SPL_RSHIFT_W32(diff, factor);
    }
    mean_new += diff;

    *mean_value = mean_new;
}

// Moves the pointer to the next entry and inserts new far end spectrum and
// corresponding Q-domain in its buffer.
//
// Input:
//      - handle        : Pointer to the delay estimation instance
//      - far_spectrum  : Pointer to the far end spectrum
//      - far_q         : Q-domain of far end spectrum
//
static void UpdateFarHistory(DelayEstimator_t* self,
                             WebRtc_UWord16* far_spectrum,
                             WebRtc_Word16 far_q)
{
    // Get new buffer position
    self->far_history_position++;
    if (self->far_history_position >= self->history_size)
    {
        self->far_history_position = 0;
    }
    // Update Q-domain buffer
    self->far_q_domains[self->far_history_position] = far_q;
    // Update far end spectrum buffer
    memcpy(&(self->far_history[self->far_history_position * self->spectrum_size]),
           far_spectrum,
           sizeof(WebRtc_UWord16) * self->spectrum_size);
}

int WebRtcAecm_FreeDelayEstimator(void* handle)
{
    DelayEstimator_t* self = (DelayEstimator_t*)handle;

    if (self == NULL)
    {
        return -1;
    }

    if (self->mean_far_spectrum != NULL)
    {
        free(self->mean_far_spectrum);
        self->mean_far_spectrum = NULL;
    }
    if (self->mean_near_spectrum != NULL)
    {
        free(self->mean_near_spectrum);
        self->mean_near_spectrum = NULL;
    }
    if (self->far_history != NULL)
    {
        free(self->far_history);
        self->far_history = NULL;
    }
    if (self->mean_bit_counts != NULL)
    {
        free(self->mean_bit_counts);
        self->mean_bit_counts = NULL;
    }
    if (self->binary_far_history != NULL)
    {
        free(self->binary_far_history);
        self->binary_far_history = NULL;
    }
    if (self->far_q_domains != NULL)
    {
        free(self->far_q_domains);
        self->far_q_domains = NULL;
    }
    if (self->delay_histogram != NULL)
    {
        free(self->delay_histogram);
        self->delay_histogram = NULL;
    }

    free(self);

    return 0;
}

int WebRtcAecm_CreateDelayEstimator(void** handle,
                                    int spectrum_size,
                                    int history_size)
{
    // Check if the sub band used in the delay estimation is small enough to
    // fit in a Word32.
    assert(kBandLast - kBandFirst < 32);

    DelayEstimator_t *self = NULL;
    if (spectrum_size < kBandLast)
    {
        return -1;
    }
    if (history_size < 0)
    {
        return -1;
    }

    self = malloc(sizeof(DelayEstimator_t));
    *handle = self;
    if (self == NULL)
    {
        return -1;
    }

    self->mean_far_spectrum = NULL;
    self->mean_near_spectrum = NULL;
    self->far_history = NULL;
    self->mean_bit_counts = NULL;
    self->binary_far_history = NULL;
    self->far_q_domains = NULL;
    self->delay_histogram = NULL;

    // Allocate memory for spectrum buffers
    self->mean_far_spectrum = malloc(spectrum_size * sizeof(WebRtc_Word32));
    if (self->mean_far_spectrum == NULL)
    {
        WebRtcAecm_FreeDelayEstimator(self);
        self = NULL;
        return -1;
    }
    self->mean_near_spectrum = malloc(spectrum_size * sizeof(WebRtc_Word32));
    if (self->mean_near_spectrum == NULL)
    {
        WebRtcAecm_FreeDelayEstimator(self);
        self = NULL;
        return -1;
    }
    // Allocate memory for history buffers
    self->far_history = malloc(spectrum_size * history_size *
                               sizeof(WebRtc_UWord16));
    if (self->far_history == NULL)
    {
        WebRtcAecm_FreeDelayEstimator(self);
        self = NULL;
        return -1;
    }
    self->mean_bit_counts = malloc(history_size * sizeof(WebRtc_Word32));
    if (self->mean_bit_counts == NULL)
    {
        WebRtcAecm_FreeDelayEstimator(self);
        self = NULL;
        return -1;
    }
    self->binary_far_history = malloc(history_size * sizeof(WebRtc_UWord32));
    if (self->binary_far_history == NULL)
    {
        WebRtcAecm_FreeDelayEstimator(self);
        self = NULL;
        return -1;
    }
    self->far_q_domains = malloc(history_size * sizeof(WebRtc_Word16));
    if (self->far_q_domains == NULL)
    {
        WebRtcAecm_FreeDelayEstimator(self);
        self = NULL;
        return -1;
    }
    self->delay_histogram = malloc(history_size * sizeof(WebRtc_Word16));
    if (self->delay_histogram == NULL)
    {
        WebRtcAecm_FreeDelayEstimator(self);
        self = NULL;
        return -1;
    }

    self->spectrum_size = spectrum_size;
    self->history_size = history_size;

    return 0;
}

int WebRtcAecm_InitDelayEstimator(void* handle)
{
    DelayEstimator_t* self = (DelayEstimator_t*)handle;

    if (self == NULL)
    {
        return -1;
    }
    // Set averaged far and near end spectra to zero
    memset(self->mean_far_spectrum,
           0,
           sizeof(WebRtc_Word32) * self->spectrum_size);
    memset(self->mean_near_spectrum,
           0,
           sizeof(WebRtc_Word32) * self->spectrum_size);
    // Set averaged bit counts to zero
    memset(self->mean_bit_counts,
           0,
           sizeof(WebRtc_Word32) * self->history_size);
    // Set far end histories to zero
    memset(self->binary_far_history,
           0,
           sizeof(WebRtc_UWord32) * self->history_size);
    memset(self->far_history,
           0,
           sizeof(WebRtc_UWord16) * self->spectrum_size *
           self->history_size);
    memset(self->far_q_domains,
           0,
           sizeof(WebRtc_Word16) * self->history_size);

    self->far_history_position = self->history_size;
    // Set delay histogram to zero
    memset(self->delay_histogram,
           0,
           sizeof(WebRtc_Word16) * self->history_size);
    // Set VAD counter to zero
    self->vad_counter = 0;
    // Set delay memory to zero
    self->last_delay = 0;

    return 0;
}

int WebRtcAecm_DelayEstimatorProcess(void* handle,
                                     WebRtc_UWord16* far_spectrum,
                                     WebRtc_UWord16* near_spectrum,
                                     int spectrum_size,
                                     WebRtc_Word16 far_q,
                                     WebRtc_Word16 vad_value)
{
    DelayEstimator_t* self = (DelayEstimator_t*)handle;

    WebRtc_UWord32 bxspectrum, byspectrum;

    int i;

    WebRtc_Word32 dtmp1;

    WebRtc_Word16 maxHistLvl = 0;
    WebRtc_Word16 minpos = -1;

    const int kVadCountThreshold = 25;
    const int kMaxHistogram = 600;

    if (self == NULL)
    {
        return -1;
    }

    WebRtc_Word32 bit_counts[self->history_size];
    WebRtc_Word32 far_spectrum_32[self->spectrum_size];
    WebRtc_Word32 near_spectrum_32[self->spectrum_size];

    if (spectrum_size != self->spectrum_size)
    {
        // Data sizes don't match
        return -1;
    }
    if (far_q > 15)
    {
        // If far_Q is larger than 15 we can not guarantee no wrap around
        return -1;
    }

    // Update far end history
    UpdateFarHistory(self, far_spectrum, far_q);
    // Update the far and near end means
    for (i = 0; i < self->spectrum_size; i++)
    {
        far_spectrum_32[i] = (WebRtc_Word32)far_spectrum[i];
        MeanEstimator(far_spectrum_32[i], 6, &(self->mean_far_spectrum[i]));

        near_spectrum_32[i] = (WebRtc_Word32)near_spectrum[i];
        MeanEstimator(near_spectrum_32[i], 6, &(self->mean_near_spectrum[i]));
    }

    // Shift binary spectrum history
    memmove(&(self->binary_far_history[1]),
            &(self->binary_far_history[0]),
            (self->history_size - 1) * sizeof(WebRtc_UWord32));

    // Get binary spectra
    bxspectrum = GetBinarySpectrum(far_spectrum_32, self->mean_far_spectrum);
    byspectrum = GetBinarySpectrum(near_spectrum_32, self->mean_near_spectrum);
    // Insert new binary spectrum
    self->binary_far_history[0] = bxspectrum;

    // Compare with delayed spectra
    BitCountComparison(byspectrum,
                      self->binary_far_history,
                      self->history_size,
                      bit_counts);

    // Smooth bit count curve
    for (i = 0; i < self->history_size; i++)
    {
        // Update sum
        // |bit_counts| is constrained to [0, 32], meaning we can smooth with a
        // factor up to 2^26. We use Q9.
        dtmp1 = WEBRTC_SPL_LSHIFT_W32(bit_counts[i], 9); // Q9
        MeanEstimator(dtmp1, 9, &(self->mean_bit_counts[i]));
    }

    // Find minimum position of bit count curve
    minpos = WebRtcSpl_MinIndexW32(self->mean_bit_counts, self->history_size);

    // If the farend has been active sufficiently long, begin accumulating a
    // histogram of the minimum positions. Search for the maximum bin to
    // determine the delay.
    if (vad_value == 1)
    {
        if (self->vad_counter >= kVadCountThreshold)
        {
            // Increment the histogram at the current minimum position.
            if (self->delay_histogram[minpos] < kMaxHistogram)
            {
                self->delay_histogram[minpos] += 3;
            }

#if (!defined ARM_WINM) && (!defined ARM9E_GCC) && (!defined ANDROID_AECOPT)
            // Decrement the entire histogram.
            // Select the histogram index corresponding to the maximum bin as
            // the delay.
            self->last_delay = 0;
            for (i = 0; i < self->history_size; i++)
            {
                if (self->delay_histogram[i] > 0)
                {
                    self->delay_histogram[i]--;
                }
                if (self->delay_histogram[i] > maxHistLvl)
                {
                    maxHistLvl = self->delay_histogram[i];
                    self->last_delay = i;
                }
            }
#else
            self->last_delay = 0;

            for (i = 0; i < self->history_size; i++)
            {
                WebRtc_Word16 tempVar = self->delay_histogram[i];

                // Decrement the entire histogram.
                if (tempVar > 0)
                {
                    tempVar--;
                    self->delay_histogram[i] = tempVar;

                    // Select the histogram index corresponding to the maximum
                    // bin as the delay.
                    if (tempVar > maxHistLvl)
                    {
                        maxHistLvl = tempVar;
                        self->last_delay = i;
                    }
                }
            }
#endif
        } else
        {
            self->vad_counter++;
        }
    } else
    {
        self->vad_counter = 0;
    }

    return self->last_delay;
}

const WebRtc_UWord16* WebRtcAecm_GetAlignedFarend(void* handle,
                                                  WebRtc_Word16* far_q)
{
    DelayEstimator_t* self = (DelayEstimator_t*)handle;
    int buffer_position = 0;

    if (self == NULL)
    {
        return NULL;
    }

    // Get buffer position
    buffer_position = self->far_history_position - self->last_delay;
    if (buffer_position < 0)
    {
        buffer_position += self->history_size;
    }
    // Get Q-domain
    *far_q = self->far_q_domains[buffer_position];
    // Return far end spectrum
    return (self->far_history + (buffer_position * self->spectrum_size));

}

int WebRtcAecm_GetLastDelay(void* handle)
{
    DelayEstimator_t* self = (DelayEstimator_t*)handle;

    if (self == NULL)
    {
        return -1;
    }

    // Return last calculated delay
    return self->last_delay;
}
