/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Performs delay estimation on a block by block basis
// The return value is  0 - OK and -1 - Error, unless otherwise stated.

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AECM_MAIN_SOURCE_AECM_DELAY_ESTIMATOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AECM_MAIN_SOURCE_AECM_DELAY_ESTIMATOR_H_

#include "typedefs.h"

// Releases the memory allocated by WebRtcAecm_CreateDelayEstimator(...)
// Input:
//      - handle        : Pointer to the delay estimation instance
//
int WebRtcAecm_FreeDelayEstimator(void* handle);

// Allocates the memory needed by the delay estimation. The memory needs to be
// initialized separately using the WebRtcAecm_InitDelayEstimator(...) function.
//
// Input:
//      - handle        : Instance that should be created
//      - spectrum_size : Size of the spectrum used both in far end and near
//                        end. Used to allocate memory for spectrum specific
//                        buffers.
//      - history_size  : Size of the far end history used to estimate the
//                        delay from. Used to allocate memory for history
//                        specific buffers.
//
// Output:
//      - handle        : Created instance
//
int WebRtcAecm_CreateDelayEstimator(void** handle,
                                    int spectrum_size,
                                    int history_size);

// Initializes the delay estimation instance created with
// WebRtcAecm_CreateDelayEstimator(...)
// Input:
//      - handle        : Pointer to the delay estimation instance
//
// Output:
//      - handle        : Initialized instance
//
int WebRtcAecm_InitDelayEstimator(void* handle);

// Estimates and returns the delay between the far end and near end blocks.
// Input:
//      - handle        : Pointer to the delay estimation instance
//      - far_spectrum  : Pointer to the far end spectrum data
//      - near_spectrum : Pointer to the near end spectrum data of the current
//                        block
//      - spectrum_size : The size of the data arrays (same for both far and
//                        near end)
//      - far_q         : The Q-domain of the far end data
//      - vad_value     : The VAD decision of the current block
//
// Output:
//      - handle        : Updated instance
//
// Return value:
//      - delay         :  >= 0 - Calculated delay value
//                        -1    - Error
//
int WebRtcAecm_DelayEstimatorProcess(void* handle,
                                     WebRtc_UWord16* far_spectrum,
                                     WebRtc_UWord16* near_spectrum,
                                     int spectrum_size,
                                     WebRtc_Word16 far_q,
                                     WebRtc_Word16 vad_value);

// Returns a pointer to the far end spectrum aligned to current near end
// spectrum. The function WebRtcAecm_DelayEstimatorProcess(...) should
// have been called before WebRtcAecm_GetAlignedFarend(...). Otherwise, you get
// the pointer to the previous frame. The memory is only valid until the next
// call of WebRtcAecm_DelayEstimatorProcess(...).
//
// Inputs:
//      - handle            : Pointer to the delay estimation instance
//
// Output:
//      - far_q             : The Q-domain of the aligned far end spectrum
//
// Return value:
//      - far_spectrum      : Pointer to the aligned far end spectrum
//                            NULL - Error
//
const WebRtc_UWord16* WebRtcAecm_GetAlignedFarend(void* handle,
                                                  WebRtc_Word16* far_q);

// Returns the last calculated delay updated by the function
// WebRtcAecm_DelayEstimatorProcess(...)
//
// Inputs:
//      - handle        : Pointer to the delay estimation instance
//
// Return value:
//      - delay         :  >= 0 - Last calculated delay value
//                        -1    - Error
//
int WebRtcAecm_GetLastDelay(void* handle);

#endif // WEBRTC_MODULES_AUDIO_PROCESSING_AECM_MAIN_SOURCE_AECM_DELAY_ESTIMATOR_H_
