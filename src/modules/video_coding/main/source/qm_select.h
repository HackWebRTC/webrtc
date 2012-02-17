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
/******************************************************/
/* Quality Modes: Resolution and Robustness settings  */
/******************************************************/

namespace webrtc
{

struct VideoContentMetrics;

struct VCMResolutionScale
{
    VCMResolutionScale(): spatialWidthFact(1), spatialHeightFact(1),
        temporalFact(1){}

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

// QmMethod class: main class for resolution and robustness settings

class VCMQmMethod
{
public:
    VCMQmMethod();
    virtual ~VCMQmMethod();

    // Reset values
    void ResetQM();
    virtual void Reset() = 0;

    // Update with the content metrics
    void UpdateContent(const VideoContentMetrics* contentMetrics);

    // Compute spatial texture magnitude and level
    void Spatial();

    // Compute motion magnitude and level
    void Motion();

    // Compute motion magnitude and level for NFD metric
    void MotionNFD();

    // Compute coherence magnitude and level
    void Coherence();

    // Get the imageType (CIF, VGA, HD, etc) for the system width/height
    WebRtc_Word8 GetImageType(WebRtc_UWord32 width, WebRtc_UWord32 height);

    // Content Data
     const VideoContentMetrics*    _contentMetrics;

    // Encoder and native frame sizes, frame rate, aspect ratio, imageType
    WebRtc_UWord32               _width;
    WebRtc_UWord32               _height;
    WebRtc_UWord32               _nativeWidth;
    WebRtc_UWord32               _nativeHeight;
    WebRtc_UWord32               _nativeFrameRate;
    float                        _aspectRatio;
    // Image type for the current encoder system size.
    WebRtc_UWord8                _imageType;

    // Content L/M/H values. stationary flag
    VCMContFeature               _motion;
    VCMContFeature               _spatial;
    VCMContFeature               _coherence;
    bool                         _stationaryMotion;
    bool                         _init;

};

// Resolution settings class

class VCMQmResolution : public VCMQmMethod
{
public:
    VCMQmResolution();
    ~VCMQmResolution();

    // Reset all quantities
    virtual void Reset();

    // Reset rate quantities and counter values after every Select Quality call
    void ResetRates();

    // Initialize rate control quantities after re-init of encoder.
    WebRtc_Word32 Initialize(float bitRate, float userFrameRate,
                             WebRtc_UWord32 width, WebRtc_UWord32 height);

    // Update QM with actual bit rate (size of the latest encoded frame)
    // and frame type, after every encoded frame.
    void UpdateEncodedSize(WebRtc_Word64 encodedSize,
                           FrameType encodedFrameType);

    // Update QM with new bit/frame/loss rates every ~1 sec from SetTargetRates
    void UpdateRates(float targetBitRate, float avgSentRate,
                     float incomingFrameRate, WebRtc_UWord8 packetLoss);

    // Extract ST (spatio-temporal) QM behavior and make decision
    // Inputs: qm: Reference to the quality modes pointer
    // Output: the spatial and/or temporal scale change
    WebRtc_Word32 SelectResolution(VCMResolutionScale** qm);

    // Select 1x2,2x2,2x2 spatial sampling mode
    WebRtc_Word32 SelectSpatialDirectionMode(float transRate);

private:
    // Encoder rate control parameter
    float                        _targetBitRate;
    float                        _userFrameRate;
    float                        _incomingFrameRate;
    float                        _perFrameBandwidth;
    float                        _bufferLevel;

    // Data accumulated every ~1sec from MediaOpt
    float                        _sumTargetRate;
    float                        _sumIncomingFrameRate;
    float                        _sumSeqRateMM;
    float                        _sumFrameRateMM;
    float                        _sumPacketLoss;
    WebRtc_Word64                _sumEncodedBytes;

    // Resolution state parameters
    WebRtc_UWord8                _stateDecFactorSpatial;
    WebRtc_UWord8                _stateDecFactorTemp;

    // Counters
    WebRtc_UWord32               _frameCnt;
    WebRtc_UWord32               _frameCntDelta;
    WebRtc_UWord32               _updateRateCnt;
    WebRtc_UWord32               _lowBufferCnt;

    VCMResolutionScale*          _qm;
};

// Robustness settings class

class VCMQmRobustness : public VCMQmMethod
{
public:
    VCMQmRobustness();
   ~VCMQmRobustness();

   virtual void Reset();

   // Adjust FEC rate based on content: every ~1 sec from SetTargetRates.
   // Returns an adjustment factor.
   float AdjustFecFactor(WebRtc_UWord8 codeRateDelta, float totalRate,
                         float frameRate, WebRtc_UWord32 rttTime,
                         WebRtc_UWord8 packetLoss);

   // Set the UEP protection on/off
   bool  SetUepProtection(WebRtc_UWord8 codeRateDelta, float totalRate,
                          WebRtc_UWord8 packetLoss, bool frameType);

private:
    // Previous state of network parameters
    float                        _prevTotalRate;
    WebRtc_UWord32               _prevRttTime;
    WebRtc_UWord8                _prevPacketLoss;

    // Previous FEC rate
    WebRtc_UWord8                _prevCodeRateDelta;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_QM_SELECT_H_
