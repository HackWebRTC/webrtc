/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_QM_SELECT_H_
#define WEBRTC_MODULES_VIDEO_CODING_QM_SELECT_H_

#include "typedefs.h"
#include "common_types.h"
/************************/
/* Quality Modes       */
/**********************/

namespace webrtc
{

struct VideoContentMetrics;

struct VCMQualityMode
{
    VCMQualityMode():spatialWidthFact(1), spatialHeightFact(1), temporalFact(1){}
    void Reset()
    {
        spatialWidthFact = 1;
        spatialHeightFact = 1;
        temporalFact = 1;
    }

    WebRtc_UWord16  spatialWidthFact;
    WebRtc_UWord16  spatialHeightFact;
    WebRtc_UWord16  temporalFact;
};

enum VCMMagValues
{
    kLow,
    kHigh,
    kDefault //default do nothing mode
};

struct VCMContFeature
{
    VCMContFeature(): value(0.0f), level(kDefault){}

    void Reset()
    {
        value = 0.0f;
        level = kDefault;
    }

    float value;
    VCMMagValues level;
};

class VCMQmSelect
{
public:
    VCMQmSelect();
    ~VCMQmSelect();

    // Initialize:
    WebRtc_Word32 Initialize(float bitRate, float userFrameRate, WebRtc_UWord32 width, WebRtc_UWord32 height);

    // Allow the user to set preferences: favor frame rate/resolution
    WebRtc_Word32 SetPreferences(WebRtc_Word8 resolPref);

    // Extract ST QM behavior and make decision
    // Inputs: Content Metrics per frame (averaged over time)
    //         qm: Reference to the quality modes pointer
    WebRtc_Word32 SelectQuality(const VideoContentMetrics* contentMetrics, VCMQualityMode** qm);

    // Update QMselect with actual bit rate (size of the latest encoded frame) and frame type
    // -> update buffer level and frame-mismatch
    void UpdateEncodedSize(WebRtc_Word64 encodedSize, FrameType encodedFrameType);

    // Update QM with new rates from SetTargetRates
    void UpdateRates(float targetBitRate, float avgSentRate, float incomingFrameRate);

    // Select 1x2,2x2,2x2 spatial sampling mode
    WebRtc_Word32 SelectSpatialDirectionMode(float transRate);

    // Reset values prior to QMSelect
    void ResetQM();

    // Reset rate quantities and counter values after every QMSelect call
    void ResetRates();

    // Reset all
    void Reset();
private:

    // Compute spatial texture magnitude and level
    void Spatial();

    // Compute motion magnitude and level
    void Motion();

    // Compute motion magnitude and level for NFD metric
    void MotionNFD();

    // Compute coherence magnitude and level
    void Coherence();

    // Set the max rate for QM selection
    WebRtc_Word32 SetMaxRateForQM(WebRtc_UWord32 width, WebRtc_UWord32 height);

    // Content Data
    const VideoContentMetrics*    _contentMetrics;

    // Encoder stats/rate-control metrics
    float                        _targetBitRate;
    float                        _userFrameRate;
    float                        _incomingFrameRate;
    float                        _perFrameBandwidth;
    float                        _bufferLevel;
    float                        _sumTargetRate;
    float                        _sumIncomingFrameRate;
    float                        _sumSeqRateMM;
    float                        _sumFrameRateMM;
    WebRtc_Word64                _sumEncodedBytes;

    //Encoder and native frame sizes
    WebRtc_UWord32               _width;
    WebRtc_UWord32               _height;
    WebRtc_UWord32               _nativeWidth;
    WebRtc_UWord32               _nativeHeight;
    WebRtc_UWord8                _stateDecFactorSpatial;

    WebRtc_UWord32               _nativeFrameRate;
    WebRtc_UWord8                _stateDecFactorTemp;

    //Counters
    WebRtc_UWord32               _frameCnt;
    WebRtc_UWord32               _frameCntDelta;
    WebRtc_UWord32               _updateRateCnt;
    WebRtc_UWord32               _lowBufferCnt;

    //Content L/M/H values
    VCMContFeature               _motion;
    VCMContFeature               _spatial;
    VCMContFeature               _coherence;
    bool                         _stationaryMotion;

    //aspect ratio
    float                        _aspectRatio;

    //Max rate to saturate the transitionalRate
    WebRtc_UWord32               _maxRateQM;
    WebRtc_UWord8                _imageType;

    //User preference for resolution or qmax change
    WebRtc_UWord8                _userResolutionPref;
    bool                         _init;
    VCMQualityMode*              _qm;

};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_QM_SELECT_H_
