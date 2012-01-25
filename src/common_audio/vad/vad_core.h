/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This header file includes the descriptions of the core VAD calls.
 */

#ifndef WEBRTC_COMMON_AUDIO_VAD_VAD_CORE_H_
#define WEBRTC_COMMON_AUDIO_VAD_VAD_CORE_H_

#include "typedefs.h"
#include "vad_defines.h"

typedef struct VadInstT_
{

    WebRtc_Word16 vad;
    WebRtc_Word32 downsampling_filter_states[4];
    WebRtc_Word16 noise_means[NUM_TABLE_VALUES];
    WebRtc_Word16 speech_means[NUM_TABLE_VALUES];
    WebRtc_Word16 noise_stds[NUM_TABLE_VALUES];
    WebRtc_Word16 speech_stds[NUM_TABLE_VALUES];
    // TODO(bjornv): Change to |frame_count|.
    WebRtc_Word32 frame_counter;
    WebRtc_Word16 over_hang; // Over Hang
    WebRtc_Word16 num_of_speech;
    // TODO(bjornv): Change to |age_vector|.
    WebRtc_Word16 index_vector[16 * NUM_CHANNELS];
    WebRtc_Word16 low_value_vector[16 * NUM_CHANNELS];
    // TODO(bjornv): Change to |median|.
    WebRtc_Word16 mean_value[NUM_CHANNELS];
    WebRtc_Word16 upper_state[5];
    WebRtc_Word16 lower_state[5];
    WebRtc_Word16 hp_filter_state[4];
    WebRtc_Word16 over_hang_max_1[3];
    WebRtc_Word16 over_hang_max_2[3];
    WebRtc_Word16 individual[3];
    WebRtc_Word16 total[3];

    int init_flag;

} VadInstT;

// Initializes the core VAD component. The default aggressiveness mode is
// controlled by |kDefaultMode| in vad_core.c.
//
// - self [i/o] : Instance that should be initialized
//
// returns      : 0 (OK), -1 (NULL pointer in or if the default mode can't be
//                set)
int WebRtcVad_InitCore(VadInstT* self);

/****************************************************************************
 * WebRtcVad_set_mode_core(...)
 *
 * This function changes the VAD settings
 *
 * Input:
 *      - inst      : VAD instance
 *      - mode      : Aggressiveness degree
 *                    0 (High quality) - 3 (Highly aggressive)
 *
 * Output:
 *      - inst      : Changed  instance
 *
 * Return value     :  0 - Ok
 *                    -1 - Error
 */

int WebRtcVad_set_mode_core(VadInstT* inst, int mode);

/****************************************************************************
 * WebRtcVad_CalcVad32khz(...) 
 * WebRtcVad_CalcVad16khz(...) 
 * WebRtcVad_CalcVad8khz(...) 
 *
 * Calculate probability for active speech and make VAD decision.
 *
 * Input:
 *      - inst          : Instance that should be initialized
 *      - speech_frame  : Input speech frame
 *      - frame_length  : Number of input samples
 *
 * Output:
 *      - inst          : Updated filter states etc.
 *
 * Return value         : VAD decision
 *                        0 - No active speech
 *                        1-6 - Active speech
 */
WebRtc_Word16 WebRtcVad_CalcVad32khz(VadInstT* inst, WebRtc_Word16* speech_frame,
                                     int frame_length);
WebRtc_Word16 WebRtcVad_CalcVad16khz(VadInstT* inst, WebRtc_Word16* speech_frame,
                                     int frame_length);
WebRtc_Word16 WebRtcVad_CalcVad8khz(VadInstT* inst, WebRtc_Word16* speech_frame,
                                    int frame_length);

#endif  // WEBRTC_COMMON_AUDIO_VAD_VAD_CORE_H_
