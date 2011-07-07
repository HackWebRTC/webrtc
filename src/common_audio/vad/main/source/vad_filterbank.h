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
 * This header file includes the description of the internal VAD call
 * WebRtcVad_GaussianProbability.
 */

#ifndef WEBRTC_VAD_FILTERBANK_H_
#define WEBRTC_VAD_FILTERBANK_H_

#include "vad_core.h"

/****************************************************************************
 * WebRtcVad_HpOutput(...)
 *
 * This function removes DC from the lowest frequency band
 *
 * Input:
 *      - in_vector         : Samples in the frequency interval 0 - 250 Hz
 *      - in_vector_length  : Length of input and output vector
 *      - filter_state      : Current state of the filter
 *
 * Output:
 *      - out_vector        : Samples in the frequency interval 80 - 250 Hz
 *      - filter_state      : Updated state of the filter
 *
 */
void WebRtcVad_HpOutput(WebRtc_Word16* in_vector,
                        WebRtc_Word16  in_vector_length,
                        WebRtc_Word16* out_vector,
                        WebRtc_Word16* filter_state);

/****************************************************************************
 * WebRtcVad_Allpass(...)
 *
 * This function is used when before splitting a speech file into 
 * different frequency bands
 *
 * Note! Do NOT let the arrays in_vector and out_vector correspond to the same address.
 *
 * Input:
 *      - in_vector             : (Q0)
 *      - filter_coefficients   : (Q15)
 *      - vector_length         : Length of input and output vector
 *      - filter_state          : Current state of the filter (Q(-1))
 *
 * Output:
 *      - out_vector            : Output speech signal (Q(-1))
 *      - filter_state          : Updated state of the filter (Q(-1))
 *
 */
void WebRtcVad_Allpass(WebRtc_Word16* in_vector,
                       WebRtc_Word16* outw16,
                       WebRtc_Word16 filter_coefficients,
                       int vector_length,
                       WebRtc_Word16* filter_state);

/****************************************************************************
 * WebRtcVad_SplitFilter(...)
 *
 * This function is used when before splitting a speech file into 
 * different frequency bands
 *
 * Input:
 *      - in_vector         : Input signal to be split into two frequency bands.
 *      - upper_state       : Current state of the upper filter
 *      - lower_state       : Current state of the lower filter
 *      - in_vector_length  : Length of input vector
 *
 * Output:
 *      - out_vector_hp     : Upper half of the spectrum
 *      - out_vector_lp     : Lower half of the spectrum
 *      - upper_state       : Updated state of the upper filter
 *      - lower_state       : Updated state of the lower filter
 *
 */
void WebRtcVad_SplitFilter(WebRtc_Word16* in_vector,
                           WebRtc_Word16* out_vector_hp,
                           WebRtc_Word16* out_vector_lp,
                           WebRtc_Word16* upper_state,
                           WebRtc_Word16* lower_state,
                           int in_vector_length);

/****************************************************************************
 * WebRtcVad_get_features(...)
 *
 * This function is used to get the logarithm of the power of each of the 
 * 6 frequency bands used by the VAD:
 *        80 Hz - 250 Hz
 *        250 Hz - 500 Hz
 *        500 Hz - 1000 Hz
 *        1000 Hz - 2000 Hz
 *        2000 Hz - 3000 Hz
 *        3000 Hz - 4000 Hz 
 *
 * Input:
 *      - inst        : Pointer to VAD instance
 *      - in_vector   : Input speech signal
 *      - frame_size  : Frame size, in number of samples
 *
 * Output:
 *      - out_vector  : 10*log10(power in each freq. band), Q4
 *    
 * Return: total power in the signal (NOTE! This value is not exact since it
 *         is only used in a comparison.
 */
WebRtc_Word16 WebRtcVad_get_features(VadInstT* inst,
                                     WebRtc_Word16* in_vector,
                                     int frame_size,
                                     WebRtc_Word16* out_vector);

/****************************************************************************
 * WebRtcVad_LogOfEnergy(...)
 *
 * This function is used to get the logarithm of the power of one frequency band.
 *
 * Input:
 *      - vector            : Input speech samples for one frequency band
 *      - offset            : Offset value for the current frequency band
 *      - vector_length     : Length of input vector
 *
 * Output:
 *      - enerlogval        : 10*log10(energy);
 *      - power             : Update total power in speech frame. NOTE! This value
 *                            is not exact since it is only used in a comparison.
 *     
 */
void WebRtcVad_LogOfEnergy(WebRtc_Word16* vector,
                           WebRtc_Word16* enerlogval,
                           WebRtc_Word16* power,
                           WebRtc_Word16 offset,
                           int vector_length);

#endif // WEBRTC_VAD_FILTERBANK_H_
