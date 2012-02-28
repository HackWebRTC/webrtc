/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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

namespace webrtc {
//
// PARAMETERS FOR RESOLUTION ADAPTATION
//

// Initial level of buffer in secs: should corresponds to wrapper settings.
const float kInitBufferLevel = 0.5f;

// Optimal level of buffer in secs: should corresponds to wrapper settings.
const float kOptBufferLevel = 0.6f;

// Threshold of (max) buffer size below which we consider too low (underflow).
const float kPercBufferThr = 0.10f;

// Threshold on the occurrences of low buffer levels.
const float kMaxBufferLow = 0.5f;

// Threshold on rate mismatch
const float kMaxRateMisMatch = 0.5f;

// Threshold on amount of under/over encoder shooting.
const float kRateOverShoot = 0.75f;
const float kRateUnderShoot = 0.75f;

// Factor for transitional rate for going back up in resolution.
const float kTransRateScaleUpSpatial = 1.25f;
const float kTransRateScaleUpTemp = 1.25f;
const float kTransRateScaleUpSpatialTemp = 1.25f;

// Threshold on packet loss rate, above which favor resolution reduction.
const float kPacketLossThr = 0.1f;

// Factor for reducing transitonal bitrate under packet loss.
const float kPacketLossRateFac = 1.0f;

// Maximum possible transitional rate for down-sampling:
// (units in kbps), for 30fps.
const uint16_t kMaxRateQm[7] = {
    100,   // QCIF
    250,   // CIF
    500,   // VGA
    800,   // 4CIF
    1000,  // 720 HD 4:3,
    1500,  // 720 HD 16:9
    2000   // 1080HD
};

// Frame rate scale for maximum transition rate.
const float kFrameRateFac[3] = {
    0.7f,  // L
    1.0f,  // H
    0.8f   // D
};

// Scale for transitional rate: based on content class
// motion=L/H/D,spatial==L/H/D: for low, high, middle levels
const float kScaleTransRateQm[18] = {
    // 4CIF and lower
    0.50f,       // L, L
    0.50f,       // L, H
    0.50f,       // L, D
    0.50f,       // H ,L
    0.25f,       // H, H
    0.25f,       // H, D
    0.50f,       // D, L
    0.50f,       // D, D
    0.25f,       // D, H

    // over 4CIF: WHD, HD
    0.50f,       // L, L
    0.50f,       // L, H
    0.50f,       // L, D
    0.50f,       // H ,L
    0.25f,       // H, H
    0.25f,       // H, D
    0.50f,       // D, L
    0.50f,       // D, D
    0.25f,       // D, H
};

// Action for down-sampling:
// motion=L/H/D,spatial==L/H/D: for low, high, middle levels
const uint8_t kSpatialAction[9] = {
    1,       // L, L
    1,       // L, H
    1,       // L, D
    4,       // H ,L
    1,       // H, H
    4,       // H, D
    4,       // D, L
    1,       // D, H
    1,       // D, D
};

const uint8_t kTemporalAction[9] = {
    1,       // L, L
    2,       // L, H
    2,       // L, D
    1,       // H ,L
    2,       // H, H
    1,       // H, D
    1,       // D, L
    2,       // D, H
    1,       // D, D
};

// Control the total amount of down-sampling allowed.
const int kMaxSpatialDown = 16;
const int kMaxTempDown = 4;
const int kMaxDownSample = 16;

// Minimum image size for a spatial down-sampling.
const int kMinImageSize= 176 * 144;

// Minimum frame rate for temporal down-sampling:
// no frame rate reduction if incomingFrameRate <= MIN_FRAME_RATE
const int kMinFrameRate = 8;

// Boundaries for the closest standard frame size
const uint32_t kFrameSizeTh[6] = {
    63360,    // between 176*144 and 352*288
    204288,   // between 352*288 and 640*480
    356352,   // between 640*480 and 704*576
    548352,   // between 704*576 and 960*720
    806400,   // between 960*720 and 1280*720
    1497600,  // between 1280*720 and 1920*1080
};

//
// PARAMETERS FOR FEC ADJUSTMENT: TODO (marpan)
//

//
// PARAMETETS FOR SETTING LOW/HIGH STATES OF CONTENT METRICS:
//

// Thresholds for frame rate:
const int kLowFrameRate = 10;
const int kHighFrameRate = 25;

// Thresholds for motion: motion level is from NFD
const float kHighMotionNfd = 0.075f;
const float kLowMotionNfd = 0.04f;

// Thresholds for spatial prediction error:
// this is applied on the min(2x2,1x2,2x1)
const float kHighTexture = 0.035f;
const float kLowTexture = 0.025f;

// Used to reduce thresholds for larger/HD scenes: correction factor since
// higher correlation in HD scenes means lower spatial prediction error.
const float kScaleTexture = 0.9f;

// percentage reduction in transitional bitrate for 2x2 selected over 1x2/2x1
const float kRateRedSpatial2X2 = 0.6f;

const float kSpatialErr2x2VsHoriz = 0.1f;   // percentage to favor 2x2 over H
const float kSpatialErr2X2VsVert = 0.1f;    // percentage to favor 2x2 over V
const float kSpatialErrVertVsHoriz = 0.1f;  // percentage to favor H over V

}  //  namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_SOURCE_QM_SELECT_DATA_H_

