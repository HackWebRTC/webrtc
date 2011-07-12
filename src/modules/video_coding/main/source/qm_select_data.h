/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_SOURCE_QM_SELECT_DATA_H_
#define WEBRTC_MODULES_VIDEO_CODING_SOURCE_QM_SELECT_DATA_H_

/***************************************************************
*QMSelectData.h
* This file includes parameters for content-aware media optimization
****************************************************************/

#include "typedefs.h"

namespace webrtc
{

//
// PARAMETERS FOR RESOLUTION ADAPTATION
//

// Initial level of buffer in secs: should corresponds to wrapper settings
#define INIT_BUFFER_LEVEL 0.5

// Optimal level of buffer in secs: should corresponds to wrapper settings
#define OPT_BUFFER_LEVEL 0.6

// Threshold of (max) buffer size below which we consider too low (underflow)
#define PERC_BUFFER_THR  0.10

// Threshold on rate mismatch
#define MAX_RATE_MM  0.5

// Avoid outliers in seq-rate MM
#define THRESH_SUM_MM 1000

// Threshold on the occurrences of low buffer levels
#define MAX_BUFFER_LOW 0.5

// Factor for transitional rate for going back up in resolution
#define TRANS_RATE_SCALE_UP_SPATIAL     1.25
#define TRANS_RATE_SCALE_UP_TEMP        1.25

// Threshold on packet loss rate, above which favor resolution reduction
#define LOSS_THR 0.1

// Factor for reducing transitonal bitrate under packet loss
#define LOSS_RATE_FAC 1.0

// Maximum possible transitional rate for down-sampling:
// (units in kbps), for 30fps
const WebRtc_UWord16 kMaxRateQm[7] = {
    100,  //QCIF
    500,  //CIF
    800,  //VGA
    1500, //4CIF
    2000, //720 HD 4:3,
    2500, //720 HD 16:9
    3000  //1080HD
};

// Scale for transitional rate: based on content class
// motion=L/H/D,spatial==L/H/D: for low, high, middle levels
const float kScaleTransRateQm[18] = {
    //4CIF and lower
    0.25f,       // L, L
    0.75f,       // L, H
    0.75f,       // L, D
    0.75f,       // H ,L
    0.50f,       // H, H
    0.50f,       // H, D
    0.50f,       // D, L
    0.63f,       // D, D
    0.25f,       // D, H

    //over 4CIF: WHD, HD
    0.25f,       // L, L
    0.75f,       // L, H
    0.75f,       // L, D
    0.75f,       // H ,L
    0.50f,       // H, H
    0.50f,       // H, D
    0.50f,       // D, L
    0.63f,       // D, D
    0.25f        // D, H
};

// Action for down-sampling:
// motion=L/H/D,spatial==L/H/D: for low, high, middle levels
const WebRtc_UWord8 kSpatialAction[9] = {
      1,       // L, L
      1,       // L, H
      1,       // L, D
      4,       // H ,L
      1,       // H, H
      4,       // H, D
      4,       // D, L
      1,       // D, D
      1,       // D, H
};

const WebRtc_UWord8 kTemporalAction[9] = {
      1,       // L, L
      2,       // L, H
      2,       // L, D
      1,       // H ,L
      2,       // H, H
      1,       // H, D
      1,       // D, L
      2,       // D, D
      1,       // D, H
};

// Control the total amount of down-sampling allowed
#define MAX_SPATIAL_DOWN_FACT       4
#define MAX_TEMP_DOWN_FACT          4
#define MAX_SPATIAL_TEMP_DOWN_FACT  8

// Minimum image size for a spatial down-sampling:
// no spatial down-sampling if input size <= MIN_IMAGE_SIZE
#define MIN_IMAGE_SIZE  25344 //176*144

// Minimum frame rate for temporal down-sampling:
// no frame rate reduction if incomingFrameRate <= MIN_FRAME_RATE
#define MIN_FRAME_RATE_QM  8

// Boundaries for the closest standard frame size
const WebRtc_UWord32 kFrameSizeTh[6] = {
    63360,    //between 176*144 and 352*288
    204288,   //between 352*288 and 640*480
    356352,   //between 640*480 and 704*576
    548352,   //between 704*576 and 960*720
    806400,   //between 960*720 and 1280*720
    1497600,  // between 1280*720 and 1920*1080
};


//
// PARAMETERS FOR FEC ADJUSTMENT: TODO (marpan)
//


//
// PARAMETETS FOR SETTING LOW/HIGH STATES OF CONTENT METRICS:
//

// Threshold to determine if high amount of zero_motion
#define HIGH_ZERO_MOTION_SIZE 0.95

// Thresholds for motion:
// motion level is derived from motion vectors: motion = size_nz*magn_nz
#define HIGH_MOTION 0.7
#define LOW_MOTION  0.4

// Thresholds for motion: motion level is from NFD
#define HIGH_MOTION_NFD 0.075
#define LOW_MOTION_NFD  0.04

// Thresholds for spatial prediction error:
// this is appLied on the min(2x2,1x2,2x1)
#define HIGH_TEXTURE 0.035
#define LOW_TEXTURE  0.025

// Used to reduce thresholds for HD scenes: correction factor since higher
// correlation in HD scenes means lower spatial prediction error
#define SCALE_TEXTURE_HD 0.9;

// Thresholds for distortion and horizontalness:
// applied on product: horiz_nz/dist_nz
#define COHERENCE_THR   1.0
#define COH_MAX 10

// percentage reduction in transitional bitrate for 2x2 selected over 1x2/2x1
#define RATE_RED_SPATIAL_2X2    0.6

#define SPATIAL_ERR_2X2_VS_H    0.1  //percentage to favor 2x2
#define SPATIAL_ERR_2X2_VS_V    0.1  //percentage to favor 2x2 over V
#define SPATIAL_ERR_V_VS_H      0.1  //percentage to favor H over V

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_SOURCE_QM_SELECT_DATA_H_
