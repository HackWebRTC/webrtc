/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "qm_select.h"
#include "internal_defines.h"
#include "qm_select_data.h"

#include "module_common_types.h"
#include "video_coding_defines.h"
#include "trace.h"

#include <math.h>

namespace webrtc {

// QM-METHOD class

VCMQmMethod::VCMQmMethod()
{
    _contentMetrics = new VideoContentMetrics();
    ResetQM();
}

VCMQmMethod::~VCMQmMethod()
{
    delete _contentMetrics;
}

void
VCMQmMethod::ResetQM()
{
    _motion.Reset();
    _spatial.Reset();
    _coherence.Reset();
    _stationaryMotion = 0;
    _aspectRatio = 1;
    _imageType = 2;
    return;
}

void
VCMQmMethod::UpdateContent(const VideoContentMetrics*  contentMetrics)
{
    _contentMetrics = contentMetrics;
}

void
VCMQmMethod::MotionNFD()
{
    _motion.value = _contentMetrics->motionMagnitudeNZ;

    // Determine motion level
    if (_motion.value < LOW_MOTION_NFD)
    {
        _motion.level = kLow;
    }
    else if (_motion.value > HIGH_MOTION_NFD)
    {
        _motion.level  = kHigh;
    }
    else
    {
        _motion.level = kDefault;
    }

}

void
VCMQmMethod::Motion()
{

    float sizeZeroMotion = _contentMetrics->sizeZeroMotion;
    float motionMagNZ = _contentMetrics->motionMagnitudeNZ;

    // Take product of size and magnitude with equal weight
    _motion.value = (1.0f - sizeZeroMotion) * motionMagNZ;

    // Stabilize: motionMagNZ could be large when only a
    // few motion blocks are non-zero
    _stationaryMotion = false;
    if (sizeZeroMotion > HIGH_ZERO_MOTION_SIZE)
    {
        _motion.value = 0.0f;
        _stationaryMotion = true;
    }
    // Determine motion level
    if (_motion.value < LOW_MOTION)
    {
        _motion.level = kLow;
    }
    else if (_motion.value > HIGH_MOTION)
    {
        _motion.level  = kHigh;
    }
    else
    {
        _motion.level = kDefault;
    }
}


void
VCMQmMethod::Spatial()
{
    float spatialErr =  _contentMetrics->spatialPredErr;
    float spatialErrH = _contentMetrics->spatialPredErrH;
    float spatialErrV = _contentMetrics->spatialPredErrV;
    // Spatial measure: take average of 3 prediction errors
    _spatial.value = (spatialErr + spatialErrH + spatialErrV) / 3.0f;

    float scale = 1.0f;
    // Reduce thresholds for HD scenes
    if (_imageType > 3)
    {
        scale = (float)SCALE_TEXTURE_HD;
    }

    if (_spatial.value > scale * HIGH_TEXTURE)
    {
        _spatial.level = kHigh;
    }
    else if (_spatial.value < scale * LOW_TEXTURE)
    {
        _spatial.level = kLow;
    }
    else
    {
         _spatial.level = kDefault;
    }
}

void
VCMQmMethod::Coherence()
{
    float horizNZ  = _contentMetrics->motionHorizontalness;
    float distortionNZ  = _contentMetrics->motionClusterDistortion;

    // Coherence measure: combine horizontalness with cluster distortion
    _coherence.value = COH_MAX;
    if (distortionNZ > 0.)
    {
        _coherence.value = horizNZ / distortionNZ;
    }
    _coherence.value = VCM_MIN(COH_MAX, _coherence.value);

    if (_coherence.value < COHERENCE_THR)
    {
        _coherence.level = kLow;
    }
    else
    {
        _coherence.level = kHigh;
    }

}

WebRtc_Word8
VCMQmMethod::GetImageType(WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    // Match image type
    WebRtc_UWord32 imageSize = width * height;
    WebRtc_Word8 imageType;

    if (imageSize < kFrameSizeTh[0])
    {
        imageType  = 0;
    }
    else if (imageSize < kFrameSizeTh[1])
    {
        imageType  = 1;
    }
    else if (imageSize < kFrameSizeTh[2])
    {
        imageType  = 2;
    }
    else if (imageSize < kFrameSizeTh[3])
    {
        imageType  = 3;
    }
    else if (imageSize < kFrameSizeTh[4])
    {
        imageType  = 4;
    }
    else if (imageSize < kFrameSizeTh[5])
    {
        imageType  = 5;
    }
    else
    {
        imageType  = 6;
    }

    return imageType;
}

// DONE WITH QM CLASS


//RESOLUTION CLASS

VCMQmResolution::VCMQmResolution()
{
     _qm = new VCMResolutionScale();
     Reset();
}

VCMQmResolution::~VCMQmResolution()
{
    delete _qm;
}

void
VCMQmResolution::ResetRates()
{
    _sumEncodedBytes = 0;
    _sumTargetRate = 0.0f;
    _sumIncomingFrameRate = 0.0f;
    _sumFrameRateMM = 0.0f;
    _sumSeqRateMM = 0.0f;
    _sumPacketLoss = 0.0f;
    _frameCnt = 0;
    _frameCntDelta = 0;
    _lowBufferCnt = 0;
    _updateRateCnt = 0;
    return;
}

void
VCMQmResolution::Reset()
{
    _stateDecFactorSpatial = 1;
    _stateDecFactorTemp  = 1;
    _bufferLevel = 0.0f;
    _targetBitRate = 0.0f;
    _incomingFrameRate = 0.0f;
    _userFrameRate = 0.0f;
    _perFrameBandwidth =0.0f;
    ResetRates();
    ResetQM();
    return;
}

// Initialize rate control quantities after reset of encoder
WebRtc_Word32
VCMQmResolution::Initialize(float bitRate, float userFrameRate,
                        WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (userFrameRate == 0.0f || width == 0 || height == 0)
    {
        return VCM_PARAMETER_ERROR;
    }
    _targetBitRate = bitRate;
    _userFrameRate = userFrameRate;

    // Encoder width and height
    _width = width;
    _height = height;

    // Aspect ratio: used for selection of 1x2,2x1,2x2
    _aspectRatio = static_cast<float>(_width) / static_cast<float>(_height);

    // Set the imageType for the encoder width/height.
    _imageType = GetImageType(_width, _height);

    // Initial buffer level
    _bufferLevel = INIT_BUFFER_LEVEL * _targetBitRate;

    // Per-frame bandwidth
    if ( _incomingFrameRate == 0 )
    {
        _perFrameBandwidth = _targetBitRate / _userFrameRate;
        _incomingFrameRate = _userFrameRate;
    }
    else
    {
    // Take average: this is due to delay in update of new encoder frame rate:
    // userFrameRate is the new one,
    // incomingFrameRate is the old one (based on previous ~ 1sec/RTCP report)
        _perFrameBandwidth = 0.5 *( _targetBitRate / _userFrameRate +
            _targetBitRate / _incomingFrameRate );
    }
    _init  = true;


    return VCM_OK;
}

// Update after every encoded frame
void
VCMQmResolution::UpdateEncodedSize(WebRtc_Word64 encodedSize,
                               FrameType encodedFrameType)
{
    // Update encoded size;
    _sumEncodedBytes += encodedSize;
    _frameCnt++;

    // Convert to Kbps
    float encodedSizeKbits = (float)((encodedSize * 8.0) / 1000.0);

    // Update the buffer level:
    // per_frame_BW is updated when encoder is updated, every RTCP reports
    _bufferLevel += _perFrameBandwidth - encodedSizeKbits;

    // Mismatch here is based on difference of actual encoded frame size and
    // per-frame bandwidth, for delta frames
    // This is a much stronger condition on rate mismatch than sumSeqRateMM
    // Note: not used in this version
    /*
    const bool deltaFrame = (encodedFrameType != kVideoFrameKey &&
                             encodedFrameType != kVideoFrameGolden);

    // Sum the frame mismatch:
    if (deltaFrame)
    {
         _frameCntDelta++;
         if (encodedSizeKbits > 0)
            _sumFrameRateMM +=
            (float) (fabs(encodedSizeKbits - _perFrameBandwidth) /
            encodedSizeKbits);
    }
    */

    // Counter for occurrences of low buffer level
    if (_bufferLevel <= PERC_BUFFER_THR * OPT_BUFFER_LEVEL * _targetBitRate)
    {
        _lowBufferCnt++;
    }

}

// Update various quantities after SetTargetRates in MediaOpt
void
VCMQmResolution::UpdateRates(float targetBitRate, float avgSentBitRate,
                         float incomingFrameRate, WebRtc_UWord8 packetLoss)
{

    // Sum the target bitrate and incoming frame rate:
    // these values are the encoder rates (from previous update ~1sec),
    // i.e, before the update for next ~1sec
    _sumTargetRate += _targetBitRate;
    _sumIncomingFrameRate  += _incomingFrameRate;
    _updateRateCnt++;

    // Sum the received (from RTCP reports) packet loss rates
    _sumPacketLoss += (float) packetLoss / 255.0f;

    // Convert average sent bitrate to kbps
    float avgSentBitRatekbps = avgSentBitRate / 1000.0f;

    // Sum the sequence rate mismatch:
    // Mismatch here is based on difference between target rate the encoder
    // used (in previous ~1sec) and the average actual
    // encoding rate measured at current time
    if (fabs(_targetBitRate - avgSentBitRatekbps) <  THRESH_SUM_MM &&
        _targetBitRate > 0.0 )
    {
        _sumSeqRateMM += (float)
            (fabs(_targetBitRate - avgSentBitRatekbps) / _targetBitRate );
    }

    // Update QM with the current new target and frame rate:
    // these values are ones the encoder will use for the current/next ~1sec
    _targetBitRate =  targetBitRate;
    _incomingFrameRate = incomingFrameRate;

    // Update QM with an (average) encoder per_frame_bandwidth:
    // this is the per_frame_bw for the current/next ~1sec
    _perFrameBandwidth  = 0.0f;
    if (_incomingFrameRate > 0.0f)
    {
        _perFrameBandwidth = _targetBitRate / _incomingFrameRate;
    }

}

// Select the resolution factors: frame size and frame rate change: (QM modes)
// Selection is for going back up in resolution, or going down in.
WebRtc_Word32
VCMQmResolution::SelectResolution(VCMResolutionScale** qm)
{
    if (!_init)
    {
        return VCM_UNINITIALIZED;
    }
    if (_contentMetrics == NULL)
    {
        Reset(); //default values
        *qm =  _qm;
        return VCM_OK;
    }

    // Default settings
    _qm->spatialWidthFact = 1;
    _qm->spatialHeightFact = 1;
    _qm->temporalFact = 1;

    // Update native values
    _nativeWidth = _contentMetrics->nativeWidth;
    _nativeHeight = _contentMetrics->nativeHeight;
    _nativeFrameRate = _contentMetrics->nativeFrameRate;

    float avgTargetRate = 0.0f;
    float avgIncomingFrameRate = 0.0f;
    float ratioBufferLow = 0.0f;
    float rateMisMatch = 0.0f;
    float avgPacketLoss = 0.0f;
    if (_frameCnt > 0)
    {
        ratioBufferLow = (float)_lowBufferCnt / (float)_frameCnt;
    }
    if (_updateRateCnt > 0)
    {
        // Use seq-rate mismatch for now
        rateMisMatch = (float)_sumSeqRateMM / (float)_updateRateCnt;
        //rateMisMatch = (float)_sumFrameRateMM / (float)_frameCntDelta;

        // Average target and incoming frame rates
        avgTargetRate = (float)_sumTargetRate / (float)_updateRateCnt;
        avgIncomingFrameRate = (float)_sumIncomingFrameRate /
            (float)_updateRateCnt;

        // Average received packet loss rate
        avgPacketLoss =  (float)_sumPacketLoss / (float)_updateRateCnt;
    }

    // For QM selection below, may want to weight the average encoder rates
    // with the current (for next ~1sec) rate values.
    // Uniform average for now:
    float w1 = 0.5f;
    float w2 = 0.5f;
    avgTargetRate = w1 * avgTargetRate + w2 * _targetBitRate;
    avgIncomingFrameRate = w1 * avgIncomingFrameRate + w2 * _incomingFrameRate;

    // Set the maximum transitional rate and image type:
    // for up-sampled spatial dimensions.
    // This is needed to get the transRate for going back up in
    // spatial resolution (only 2x2 allowed in this version).
    WebRtc_UWord8  imageType2 = GetImageType(2 * _width, 2 * _height);
    WebRtc_UWord32 maxRateQM2 = kMaxRateQm[imageType2];

    // Set the maximum transitional rate and image type:
    // for the encoder spatial dimensions.
    WebRtc_UWord32 maxRateQM = kMaxRateQm[_imageType];

    // Compute class state of the content.
    MotionNFD();
    Spatial();

    //
    // Get transitional rate from table, based on image type and content class.
    //

    // Get image class and content class: for going down spatially
    WebRtc_UWord8 imageClass = 1;
    if (_imageType <= 3) imageClass = 0;
    WebRtc_UWord8 contentClass  = 3 * _motion.level + _spatial.level;
    WebRtc_UWord8 tableIndex = imageClass * 9 + contentClass;
    float scaleTransRate = kScaleTransRateQm[tableIndex];

    // Get image class and content class: for going up spatially
    WebRtc_UWord8 imageClass2 = 1;
    if (imageType2 <= 3)
    {
        imageClass2 = 0;
    }
    WebRtc_UWord8 tableIndex2 = imageClass2 * 9 + contentClass;
    float scaleTransRate2 = kScaleTransRateQm[tableIndex2];

    // Transitonal rate for going down
    WebRtc_UWord32 estimatedTransRateDown = static_cast<WebRtc_UWord32>
        (_incomingFrameRate * scaleTransRate * maxRateQM / 30);

    // Transitional rate for going up temporally
    WebRtc_UWord32 estimatedTransRateUpT = static_cast<WebRtc_UWord32>
        (TRANS_RATE_SCALE_UP_TEMP * 2 * _incomingFrameRate *
         scaleTransRate * maxRateQM / 30);

   // Transitional rate for going up spatially
    WebRtc_UWord32 estimatedTransRateUpS = static_cast<WebRtc_UWord32>
        (TRANS_RATE_SCALE_UP_SPATIAL * _incomingFrameRate *
        scaleTransRate2 * maxRateQM2 / 30);

    //
    // Done with transitional rates
    //

    //
    //CHECK FOR GOING BACK UP IN RESOLUTION
    //
    bool selectedUp = false;
    // Check if native has been spatially down-sampled
    if (_stateDecFactorSpatial > 1)
    {
        // Check conditions on buffer level and rate_mismatch
        if ( (avgTargetRate > estimatedTransRateUpS) &&
             (ratioBufferLow < MAX_BUFFER_LOW) && (rateMisMatch < MAX_RATE_MM))
        {
            // width/height scaled back up:
            // setting 0 indicates scaling back to native
            _qm->spatialHeightFact = 0;
            _qm->spatialWidthFact = 0;
            selectedUp = true;
        }
    }
    //Check if native has been temporally down-sampled
    if (_stateDecFactorTemp > 1)
    {
        if ( (avgTargetRate > estimatedTransRateUpT) &&
             (ratioBufferLow < MAX_BUFFER_LOW) && (rateMisMatch < MAX_RATE_MM))
        {
            // temporal scale back up:
            // setting 0 indicates scaling back to native
            _qm->temporalFact = 0;
            selectedUp = true;
        }
    }

    // Leave QM if we selected to go back up in either spatial or temporal
    if (selectedUp == true)
    {
        // Update down-sampling state
        // Note: only temp reduction by 2 is allowed
        if (_qm->temporalFact == 0)
        {
            _stateDecFactorTemp = _stateDecFactorTemp / 2;
        }
        // Update down-sampling state
        // Note: only spatial reduction by 2x2 is allowed
        if (_qm->spatialHeightFact == 0 && _qm->spatialWidthFact == 0 )
        {
            _stateDecFactorSpatial = _stateDecFactorSpatial / 4;
        }
       *qm = _qm;
       return VCM_OK;
    }

    //
    // Done with checking for going back up in resolution
    //

    //
    //CHECK FOR RESOLUTION REDUCTION
    //

    // Resolution reduction if:
    // (1) target rate is lower than transitional rate, or
    // (2) buffer level is not stable, or
    // (3) rate mismatch is larger than threshold

    // Bias down-sampling based on packet loss conditions
    if (avgPacketLoss > LOSS_THR)
    {
        estimatedTransRateDown = LOSS_RATE_FAC * estimatedTransRateDown;
    }

    if ((avgTargetRate < estimatedTransRateDown ) ||
        (ratioBufferLow > MAX_BUFFER_LOW)
        || (rateMisMatch > MAX_RATE_MM))
    {

        WebRtc_UWord8 spatialFact = 1;
        WebRtc_UWord8 tempFact = 1;

        // Get the action
        spatialFact = kSpatialAction[contentClass];
        tempFact = kTemporalAction[contentClass];

        switch(spatialFact)
        {
        case 4:
            _qm->spatialWidthFact = 2;
            _qm->spatialHeightFact = 2;
            break;
        case 2:
             //default is 1x2 (H)
            _qm->spatialWidthFact = 2;
            _qm->spatialHeightFact = 1;
            // Select 1x2,2x1, or back to 2x2
            // Note: directional selection not used in this version
            // SelectSpatialDirectionMode((float) estimatedTransRateDown);
            break;
        default:
            _qm->spatialWidthFact = 1;
            _qm->spatialHeightFact = 1;
            break;
        }
        _qm->temporalFact = tempFact;

        // Sanity check on ST QM selection:
        // override the settings for too small image size and frame rate
        // Also check the limit on current down-sampling state

        // No spatial sampling if image size is too small (QCIF)
        if ( (_width * _height) <= MIN_IMAGE_SIZE  ||
            _stateDecFactorSpatial >= MAX_SPATIAL_DOWN_FACT)
        {
            _qm->spatialWidthFact = 1;
            _qm->spatialHeightFact = 1;
        }

        // No frame rate reduction below some point:
        // use the (average) incoming frame rate
        if ( avgIncomingFrameRate <= MIN_FRAME_RATE_QM  ||
            _stateDecFactorTemp >= MAX_TEMP_DOWN_FACT)
        {
            _qm->temporalFact = 1;
        }

        // No down-sampling if current downsampling state is above threshold
        if (_stateDecFactorTemp * _stateDecFactorSpatial >=
            MAX_SPATIAL_TEMP_DOWN_FACT)
        {
            _qm->spatialWidthFact = 1;
            _qm->spatialHeightFact = 1;
            _qm->temporalFact = 1;
        }
        //
        // Done with sanity checks on ST QM selection
        //

        // Update down-sampling states
        _stateDecFactorSpatial = _stateDecFactorSpatial * _qm->spatialWidthFact
            * _qm->spatialHeightFact;
        _stateDecFactorTemp = _stateDecFactorTemp * _qm->temporalFact;

        if (_qm->spatialWidthFact != 1 || _qm->spatialHeightFact != 1 ||
            _qm->temporalFact != 1)
        {

            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideo, -1,
                         "Resolution reduction occurred"
                         "Content Metrics are: Motion = %d , Spatial = %d, "
                         "Rates are: Est. Trans. BR = %d, Avg.Target BR = %f",
                         _motion.level, _spatial.level,
                         estimatedTransRateDown, avgTargetRate);
        }

    }
    else
    {
      *qm = _qm;
      return VCM_OK;
    }
    // Done with checking for resolution reduction

    *qm = _qm;
    return VCM_OK;


}

WebRtc_Word32
VCMQmResolution::SelectSpatialDirectionMode(float transRate)
{
    // Default is 1x2 (H)

    // For bit rates well below transitional rate, we select 2x2
    if ( _targetBitRate < transRate * RATE_RED_SPATIAL_2X2 )
    {
        _qm->spatialWidthFact = 2;
        _qm->spatialHeightFact = 2;
        return VCM_OK;
    }

    // Otherwise check prediction errors, aspect ratio, horizontalness

    float spatialErr = _contentMetrics->spatialPredErr;
    float spatialErrH = _contentMetrics->spatialPredErrH;
    float spatialErrV = _contentMetrics->spatialPredErrV;

    // Favor 1x2 if aspect_ratio is 16:9
    if (_aspectRatio >= 16.0f / 9.0f )
    {
        //check if 1x2 has lowest prediction error
        if (spatialErrH < spatialErr && spatialErrH < spatialErrV)
        {
            return VCM_OK;
        }
    }

    // Check for 2x2 selection: favor 2x2 over 1x2 and 2x1
    if (spatialErr < spatialErrH * (1.0f + SPATIAL_ERR_2X2_VS_H) &&
        spatialErr < spatialErrV * (1.0f + SPATIAL_ERR_2X2_VS_V))
    {
        _qm->spatialWidthFact = 2;
        _qm->spatialHeightFact = 2;
         return VCM_OK;
    }

    // Check for 2x1 selection:
    if (spatialErrV < spatialErrH * (1.0f - SPATIAL_ERR_V_VS_H) &&
        spatialErrV < spatialErr * (1.0f - SPATIAL_ERR_2X2_VS_V))
    {
        _qm->spatialWidthFact = 1;
        _qm->spatialHeightFact = 2;
         return VCM_OK;
    }

    return VCM_OK;
}

// DONE WITH RESOLUTION CLASS


// ROBUSTNESS CLASS

VCMQmRobustness::VCMQmRobustness()
{
    Reset();
}

VCMQmRobustness::~VCMQmRobustness()
{

}

void
VCMQmRobustness::Reset()
{
    _prevTotalRate = 0.0f;
    _prevRttTime = 0;
    _prevPacketLoss = 0;
    ResetQM();
    return;
}

// Adjust the FEC rate based on the content and the network state
// (packet loss rate, total rate/bandwidth, round trip time).
// Note that packetLoss here is the filtered loss value.
WebRtc_UWord8
VCMQmRobustness::AdjustFecFactor(WebRtc_UWord8 codeRateDelta, float totalRate,
                                 float frameRate,WebRtc_UWord32 rttTime,
                                 WebRtc_UWord8 packetLoss)
{
    if (_contentMetrics == NULL)
    {
        return VCM_OK;
    }

    // Default: no adjustment
    WebRtc_UWord8 codeRateDeltaAdjust = codeRateDelta;
    float adjustFec =  1.0f;

    // Compute class state of the content.
    MotionNFD();
    Spatial();

    // TODO (marpan):
    // Set FEC adjustment factor

    codeRateDeltaAdjust = static_cast<WebRtc_UWord8>(codeRateDelta * adjustFec);

    // Keep track of previous values of network state:
    // adjustment may be also based on pattern of changes in network state
    _prevTotalRate = totalRate;
    _prevRttTime = rttTime;
    _prevPacketLoss = packetLoss;

    _prevCodeRateDelta = codeRateDelta;

    return codeRateDeltaAdjust;
}

// Set the UEP (unequal-protection) on/off for the FEC
bool
VCMQmRobustness::SetUepProtection(WebRtc_UWord8 codeRateDelta, float totalRate,
                                  WebRtc_UWord8 packetLoss, bool frameType)
{
    if (_contentMetrics == NULL)
    {
        return VCM_OK;
    }

    // Default: UEP on
    bool uepProtection  = true;

    return uepProtection;
}

} // end of namespace
