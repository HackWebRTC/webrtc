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

#ifndef WEBRTC_COMMON_AUDIO_VAD_VAD_FILTERBANK_H_
#define WEBRTC_COMMON_AUDIO_VAD_VAD_FILTERBANK_H_

#include "typedefs.h"
#include "vad_core.h"

// TODO(bjornv): Rename to CalcFeatures() or similar. Update at the same time
// comments and parameter order.
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
int16_t WebRtcVad_get_features(VadInstT* inst,
                               const int16_t* in_vector,
                               int frame_size,
                               int16_t* out_vector);

#endif  // WEBRTC_COMMON_AUDIO_VAD_VAD_FILTERBANK_H_
