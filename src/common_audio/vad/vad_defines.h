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
 * This header file includes the macros used in VAD.
 */

#ifndef WEBRTC_VAD_DEFINES_H_
#define WEBRTC_VAD_DEFINES_H_

#define NUM_CHANNELS        6   // Eight frequency bands
#define NUM_MODELS          2   // Number of Gaussian models
#define NUM_TABLE_VALUES    NUM_CHANNELS * NUM_MODELS

#define MIN_ENERGY          10
#define ALPHA1              6553    // 0.2 in Q15
#define ALPHA2              32439   // 0.99 in Q15
#define NSP_MAX             6       // Maximum number of VAD=1 frames in a row counted
#define MIN_STD             384     // Minimum standard deviation
// Mode 0, Quality thresholds - Different thresholds for the different frame lengths
#define INDIVIDUAL_10MS_Q   24
#define INDIVIDUAL_20MS_Q   21      // (log10(2)*66)<<2 ~=16
#define INDIVIDUAL_30MS_Q   24

#define TOTAL_10MS_Q        57
#define TOTAL_20MS_Q        48
#define TOTAL_30MS_Q        57

#define OHMAX1_10MS_Q       8  // Max Overhang 1
#define OHMAX2_10MS_Q       14 // Max Overhang 2
#define OHMAX1_20MS_Q       4  // Max Overhang 1
#define OHMAX2_20MS_Q       7  // Max Overhang 2
#define OHMAX1_30MS_Q       3
#define OHMAX2_30MS_Q       5

// Mode 1, Low bitrate thresholds - Different thresholds for the different frame lengths
#define INDIVIDUAL_10MS_LBR 37
#define INDIVIDUAL_20MS_LBR 32
#define INDIVIDUAL_30MS_LBR 37

#define TOTAL_10MS_LBR      100
#define TOTAL_20MS_LBR      80
#define TOTAL_30MS_LBR      100

#define OHMAX1_10MS_LBR     8  // Max Overhang 1
#define OHMAX2_10MS_LBR     14 // Max Overhang 2
#define OHMAX1_20MS_LBR     4
#define OHMAX2_20MS_LBR     7

#define OHMAX1_30MS_LBR     3
#define OHMAX2_30MS_LBR     5

// Mode 2, Very aggressive thresholds - Different thresholds for the different frame lengths
#define INDIVIDUAL_10MS_AGG 82
#define INDIVIDUAL_20MS_AGG 78
#define INDIVIDUAL_30MS_AGG 82

#define TOTAL_10MS_AGG      285 //580
#define TOTAL_20MS_AGG      260
#define TOTAL_30MS_AGG      285

#define OHMAX1_10MS_AGG     6  // Max Overhang 1
#define OHMAX2_10MS_AGG     9  // Max Overhang 2
#define OHMAX1_20MS_AGG     3
#define OHMAX2_20MS_AGG     5

#define OHMAX1_30MS_AGG     2
#define OHMAX2_30MS_AGG     3

// Mode 3, Super aggressive thresholds - Different thresholds for the different frame lengths
#define INDIVIDUAL_10MS_VAG 94
#define INDIVIDUAL_20MS_VAG 94
#define INDIVIDUAL_30MS_VAG 94

#define TOTAL_10MS_VAG      1100 //1700
#define TOTAL_20MS_VAG      1050
#define TOTAL_30MS_VAG      1100

#define OHMAX1_10MS_VAG     6  // Max Overhang 1
#define OHMAX2_10MS_VAG     9  // Max Overhang 2
#define OHMAX1_20MS_VAG     3
#define OHMAX2_20MS_VAG     5

#define OHMAX1_30MS_VAG     2
#define OHMAX2_30MS_VAG     3

#endif // WEBRTC_VAD_DEFINES_H_
