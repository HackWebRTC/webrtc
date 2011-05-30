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

VCMQmSelect::VCMQmSelect()
{
    _qm = new VCMQualityMode();
     Reset();
}

VCMQmSelect::~VCMQmSelect()
{
    delete _qm;
}

void
VCMQmSelect::ResetQM()
{
    _motion.Reset();
    _spatial.Reset();
    _coherence.Reset();
    _stationaryMotion = 0;
    _aspectRatio = 1;
    _maxRateQM = 0;
    _imageType = 1;
    _userResolutionPref = 50; // Neutral
    _qm->Reset();
    return;
}

void
VCMQmSelect::ResetRates()
{
    _sumEncodedBytes = 0;
    _sumTargetRate = 0;
    _sumIncomingFrameRate = 0;
    _sumFrameRateMM = 0;
    _sumSeqRateMM = 0;
    _frameCnt = 0;
    _frameCntDelta = 0;
    _lowBufferCnt = 0;
    _updateRateCnt = 0;
    return;
}

void
VCMQmSelect::Reset()
{
   _stateDecFactorSpatial = 1;
   _stateDecFactorTemp  = 1;
   _bufferLevel = 0;
   _targetBitRate = 0;
   _incomingFrameRate = 0;
   _userFrameRate = 0;
   _perFrameBandwidth =0;
    ResetQM();
    ResetRates();
    return;
}

//Initialize after reset of encoder
WebRtc_Word32
VCMQmSelect::Initialize(float bitRate, float userFrameRate, WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (userFrameRate == 0.0f || width == 0 || height == 0)
    {
        return VCM_PARAMETER_ERROR;
    }
    _targetBitRate = bitRate;
    _userFrameRate = userFrameRate;
    //Encoder width and height
    _width = width;
    _height = height;
    //Initial buffer level
    _bufferLevel = INIT_BUFFER_LEVEL * _targetBitRate;
    if ( _incomingFrameRate == 0 )
    {
        _perFrameBandwidth = _targetBitRate / _userFrameRate;
        _incomingFrameRate = _userFrameRate;
    }
    else
    {
    //Take average: this is due to delay in update of new frame rate in encoder:
    //userFrameRate is the new one, incomingFrameRate is the old one (based on previous ~ 1sec)
        _perFrameBandwidth = 0.5 *( _targetBitRate / _userFrameRate + _targetBitRate / _incomingFrameRate );
    }
    _init  = true;


    return VCM_OK;
}

WebRtc_Word32
VCMQmSelect::SetPreferences(WebRtc_Word8 resolPref)
{
    // Preference setting for temporal over spatial resolution
    // 100 means temporal, 0 means spatial, 50 is neutral (we decide)
    _userResolutionPref = resolPref;

    return VCM_OK;
}

//Update after every encoded frame
void
VCMQmSelect::UpdateEncodedSize(WebRtc_Word64 encodedSize, FrameType encodedFrameType)
{
    //Update encoded size;
    _sumEncodedBytes += encodedSize;
    _frameCnt++;

    //Convert to Kbps
    float encodedSizeKbits = (float)((encodedSize * 8.0) / 1000.0);

    //Update the buffer level: per_frame_BW is updated when encoder is updated, every ~1sec
    _bufferLevel += _perFrameBandwidth - encodedSizeKbits;

    const bool deltaFrame = (encodedFrameType != kVideoFrameKey &&
                             encodedFrameType != kVideoFrameGolden);

    //Sum the frame mismatch:
    //Mismatch here is based on difference of actual encoded frame size and per-frame bandwidth, for delta frames
    //This is a much stronger condition on rate mismatch than sumSeqRateMM
    // Note: not used in this version
    /*
    if (deltaFrame)
    {
         _frameCntDelta++;
         if (encodedSizeKbits > 0)
            _sumFrameRateMM += (float) (fabs(encodedSizeKbits - _perFrameBandwidth) / encodedSizeKbits);
    }
    */

    //Counter for occurrences of low buffer level
    if (_bufferLevel <= PERC_BUFFER_THR * INIT_BUFFER_LEVEL * _targetBitRate)
    {
        _lowBufferCnt++;
    }



}

//Update after SetTargetRates in MediaOpt (every ~1sec)
void
VCMQmSelect::UpdateRates(float targetBitRate, float avgSentBitRate, float incomingFrameRate)
{

    //Sum the target bitrate and incoming frame rate: these values are the encoder rates (from previous ~1sec),
    //i.e, before the update for next ~1sec
    _sumTargetRate += _targetBitRate;
    _sumIncomingFrameRate  += _incomingFrameRate;
    _updateRateCnt++;

    //Convert to kbps
    float avgSentBitRatekbps = avgSentBitRate / 1000.0f;

    //Sum the sequence rate mismatch:
    //Mismatch here is based on difference between target rate the encoder used (in previous ~1sec) and the average actual
    //encoding rate at current time
    if (fabs(_targetBitRate - avgSentBitRatekbps) <  THRESH_SUM_MM && _targetBitRate > 0.0 )
        _sumSeqRateMM += (float) (fabs(_targetBitRate - avgSentBitRatekbps) / _targetBitRate );

    //Update QM with the current new target and frame rate: these values are ones the encoder will use for the current/next ~1sec
    _targetBitRate =  targetBitRate;
    _incomingFrameRate = incomingFrameRate;

    //Update QM with an (average) encoder per_frame_bandwidth: this is the per_frame_bw for the next ~1sec
    _perFrameBandwidth  = 0.0f;
    if (_incomingFrameRate > 0.0f)
    {
        _perFrameBandwidth = _targetBitRate / _incomingFrameRate;
    }

}

WebRtc_Word32
VCMQmSelect::SelectQuality(const VideoContentMetrics* contentMetrics, VCMQualityMode** qm)
{
    if (!_init)
    {
        return VCM_UNINITIALIZED;
    }
    if (contentMetrics == NULL)
    {
        Reset(); //default values
        *qm =  _qm;
        return VCM_OK;
    }

    //Default settings
    _qm->spatialWidthFact = 1;
    _qm->spatialHeightFact = 1;
    _qm->temporalFact = 1;

    _contentMetrics = contentMetrics;

    //Update native values
    _nativeWidth = _contentMetrics->nativeWidth;
    _nativeHeight = _contentMetrics->nativeHeight;
    _nativeFrameRate = _contentMetrics->nativeFrameRate;

    //Aspect ratio: used for selection of 1x2,2x1,2x2
    _aspectRatio = (float)_width / (float)_height;

    float avgTargetRate = 0.0f;
    float avgIncomingFrameRate = 0.0f;
    float ratioBufferLow = 0.0f;
    float rateMisMatch = 0.0f;
    if (_frameCnt > 0)
    {
        ratioBufferLow = (float)_lowBufferCnt / (float)_frameCnt;
    }
    if (_updateRateCnt > 0)
    {
        //use seq-rate mismatch for now
        rateMisMatch = (float)_sumSeqRateMM / (float)_updateRateCnt;
        //rateMisMatch = (float)_sumFrameRateMM / (float)_frameCntDelta;

        //average target and incoming frame rates
        avgTargetRate = (float)_sumTargetRate / (float)_updateRateCnt;
        avgIncomingFrameRate = (float)_sumIncomingFrameRate / (float)_updateRateCnt;
    }

    //For qm selection below, may want to weight the average encoder rates with the current (for next ~1sec) rate values
    //uniform average for now:
    float w1 = 0.5f;
    float w2 = 0.5f;
    avgTargetRate = w1 * avgTargetRate + w2 * _targetBitRate;
    avgIncomingFrameRate = w1 * avgIncomingFrameRate + w2 * _incomingFrameRate;

    //Set the maximum transitional rate and image type: for up-sampled spatial dimensions
    //Needed to get the transRate for going back up in spatial resolution (only 2x2 allowed in this version)
    SetMaxRateForQM(2 * _width, 2 * _height);
    WebRtc_UWord8  imageType2  = _imageType;
    WebRtc_UWord32 maxRateQM2 = _maxRateQM;

    //Set the maximum transitional rate and image type: for the input/encoder spatial dimensions
    SetMaxRateForQM(_width, _height);

    //Compute metric features
    MotionNFD();
    Spatial();

    //
    //Get transitional rate from table, based on image type and content class
    //

    //Get image size class: map _imageType to 2 classes
    WebRtc_UWord8 imageClass = 1;
    if (_imageType <= 3) imageClass = 0;

    WebRtc_UWord8 contentClass  = 3 * _motion.level + _spatial.level;
    WebRtc_UWord8 tableIndex = imageClass * 9 + contentClass;
    float scaleTransRate = kScaleTransRateQm[tableIndex];

    // for transRate for going back up spatially
    WebRtc_UWord8 imageClass2 = 1;
    if (imageType2 <= 3) imageClass2 = 0;
    WebRtc_UWord8 tableIndex2 = imageClass2 * 9 + contentClass;
    float scaleTransRate2 = kScaleTransRateQm[tableIndex2];
    //

    WebRtc_UWord32 estimatedTransRateDown = (WebRtc_UWord32) (_incomingFrameRate * scaleTransRate * _maxRateQM / 30);
    WebRtc_UWord32 estimatedTransRateUpT =  (WebRtc_UWord32) (TRANS_RATE_SCALE_UP_TEMP * 2 * _incomingFrameRate * scaleTransRate * _maxRateQM / 30);
    WebRtc_UWord32 estimatedTransRateUpS =  (WebRtc_UWord32) (TRANS_RATE_SCALE_UP_SPATIAL * _incomingFrameRate * scaleTransRate2 * maxRateQM2 / 30);

    //
    //done with transitional rate
    //

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideo, -1,
                   "Content Metrics: Motion = %d , Spatial = %d, Est. Trans. BR = %d",
                   _motion.level, _spatial.level, estimatedTransRateDown);



    //
    //CHECK FOR GOING BACK UP IN RESOLUTION
    //
    bool selectedUp = false;
    //Check if native has been spatially down-sampled
    if (_stateDecFactorSpatial > 1)
    {
        //check conditions on frame_skip and rate_mismatch
        if ( (avgTargetRate > estimatedTransRateUpS) &&
             (ratioBufferLow < MAX_BUFFER_LOW) && (rateMisMatch < MAX_RATE_MM) )
        {
            //width/height scaled back up: setting 0 indicates scaling back to native
            _qm->spatialHeightFact = 0;
            _qm->spatialWidthFact = 0;
            selectedUp = true;
        }
    }
    //Check if native has been temporally down-sampled
    if (_stateDecFactorTemp > 1)
    {
        if ( (avgTargetRate > estimatedTransRateUpT) &&
             (ratioBufferLow < MAX_BUFFER_LOW) && (rateMisMatch < MAX_RATE_MM) )
        {
            //temporal scale back up: setting 0 indicates scaling back to native
            _qm->temporalFact = 0;
            selectedUp = true;
        }
    }

    //leave QM if we selected to go back up in either spatial or temporal resolution
    if (selectedUp == true)
    {
        //Update down-sampling state
        //Note: only temp reduction by 2 is allowed
        if (_qm->temporalFact == 0)
        {
            _stateDecFactorTemp = _stateDecFactorTemp / 2;
        }
        //Update down-sampling state
        //Note: only spatial reduction by 2x2 is allowed
        if (_qm->spatialHeightFact == 0 && _qm->spatialWidthFact == 0 )
        {
            _stateDecFactorSpatial = _stateDecFactorSpatial / 4;
        }
       *qm = _qm;
       return VCM_OK;
    }

    //
    //done with checking for going back up
    //

    //
    //CHECK FOR RESOLUTION REDUCTION
    //

    //ST QM extraction if:
    // (1) target rate is lower than transitional rate (with safety margin), or
    // (2) frame skip is larger than threshold, or
    // (3) rate mismatch is larger than threshold

    if ( (avgTargetRate < estimatedTransRateDown ) || (ratioBufferLow > MAX_BUFFER_LOW)
         || (rateMisMatch > MAX_RATE_MM) )
    {

        WebRtc_UWord8 spatialFact = 1;
        WebRtc_UWord8 tempFact = 1;

        //Get the Action:
        //Note: only consider spatial by 2x2 OR temporal reduction by 2 in this version
        if (_motion.level == kLow && _spatial.level == kLow)
        {
            spatialFact = 1;
            tempFact = 1;
        }
        else if (_motion.level == kLow && _spatial.level == kHigh)
        {
            spatialFact = 1;
            tempFact = 2;
        }
        else if (_motion.level == kLow && _spatial.level == kDefault)
        {
            spatialFact = 1;
            tempFact = 2;
        }
        else if (_motion.level == kHigh && _spatial.level == kLow)
        {
            spatialFact = 4;
            tempFact = 1;
        }
        else if (_motion.level == kHigh && _spatial.level == kHigh)
        {
            spatialFact = 1;
            tempFact = 2;
        }
        else if (_motion.level == kHigh && _spatial.level == kDefault)
        {
            spatialFact = 4;
            tempFact = 1;
        }
        else if (_motion.level == kDefault && _spatial.level == kLow)
        {
            spatialFact = 4;
            tempFact = 1;
        }
        else if (_motion.level == kDefault && _spatial.level == kHigh)
        {
            spatialFact = 1;
            tempFact = 2;
        }
        else if (_motion.level == kDefault && _spatial.level == kDefault)
        {
            spatialFact = 1;
            tempFact = 1;
        }
        //
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
            //Select 1x2,2x1, or back to 2x2: depends on prediction errors, aspect ratio, and horizontalness of motion
            //Note: directional selection not used in this version
            //SelectSpatialDirectionMode((float) estimatedTransRateDown);
            break;
        default:
            _qm->spatialWidthFact = 1;
            _qm->spatialHeightFact = 1;
            break;
        }
        _qm->temporalFact = tempFact;

        //Sanity check on ST QM selection: override the settings for too small image size and frame rate
        //Also check limit the current down-sampling state

        //No spatial sampling if image size is too small (QCIF)
        if ( (_width * _height) <= MIN_IMAGE_SIZE  || _stateDecFactorSpatial >= MAX_SPATIAL_DOWN_FACT)
        {
            _qm->spatialWidthFact = 1;
            _qm->spatialHeightFact = 1;
        }

        //No frame rate reduction below some point: use the (average) incoming frame rate
        if ( avgIncomingFrameRate <= MIN_FRAME_RATE_QM  || _stateDecFactorTemp >= MAX_TEMP_DOWN_FACT)
        {
            _qm->temporalFact = 1;
        }

        //No down-sampling if current spatial-temporal downsampling state is above threshold
        if (_stateDecFactorTemp * _stateDecFactorSpatial >= MAX_SPATIAL_TEMP_DOWN_FACT)
        {
            _qm->spatialWidthFact = 1;
            _qm->spatialHeightFact = 1;
            _qm->temporalFact = 1;
        }
        //
        //done with sanity checks on ST QM selection
        //

        //Note: to disable spatial down-sampling
        // _qm->spatialWidthFact = 1;
        // _qm->spatialHeightFact = 1;

        //Update down-sampling states
        _stateDecFactorSpatial = _stateDecFactorSpatial * _qm->spatialWidthFact * _qm->spatialHeightFact;
        _stateDecFactorTemp = _stateDecFactorTemp * _qm->temporalFact;

    }
    else
    {
      *qm = _qm;
      return VCM_OK;
    }
    // done with checking for resolution reduction

    *qm = _qm;
    return VCM_OK;


}

WebRtc_Word32
VCMQmSelect::SelectSpatialDirectionMode(float transRate)
{
    //Default is 1x2 (H)

    //For bit rates well below transitional rate, we select 2x2
    if ( _targetBitRate < transRate * RATE_RED_SPATIAL_2X2 )
    {
        _qm->spatialWidthFact = 2;
        _qm->spatialHeightFact = 2;
        return VCM_OK;
    }

    //Otherwise check prediction errors, aspect ratio, horizonalness of motion

    float spatialErr = _contentMetrics->spatialPredErr;
    float spatialErrH = _contentMetrics->spatialPredErrH;
    float spatialErrV = _contentMetrics->spatialPredErrV;

    //favor 1x2 if aspect_ratio is 16:9
    if (_aspectRatio >= 16.0f / 9.0f )
    {
        //check if 1x2 has lowest prediction error
        if (spatialErrH < spatialErr && spatialErrH < spatialErrV)
        {
            return VCM_OK;
        }
    }

    //check for 2x2 selection: favor 2x2 over 1x2 and 2x1
    if (spatialErr < spatialErrH * (1.0f + SPATIAL_ERR_2X2_VS_H) &&
        spatialErr < spatialErrV * (1.0f + SPATIAL_ERR_2X2_VS_V))
    {
        _qm->spatialWidthFact = 2;
        _qm->spatialHeightFact = 2;
         return VCM_OK;
    }

    //check for 2x1 selection:
    if (spatialErrV < spatialErrH * (1.0f - SPATIAL_ERR_V_VS_H) &&
        spatialErrV < spatialErr * (1.0f - SPATIAL_ERR_2X2_VS_V))
    {
        _qm->spatialWidthFact = 1;
        _qm->spatialHeightFact = 2;
         return VCM_OK;
    }

    return VCM_OK;
}

void
VCMQmSelect::Coherence()
{
    float horizNZ  = _contentMetrics->motionHorizontalness;
    float distortionNZ  = _contentMetrics->motionClusterDistortion;

    //Coherence measure: combine horizontalness with cluster distortion
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

void
VCMQmSelect::MotionNFD()
{
    _motion.value = _contentMetrics->motionMagnitudeNZ;

    // determine motion level
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
VCMQmSelect::Motion()
{

    float sizeZeroMotion = _contentMetrics->sizeZeroMotion;
    float motionMagNZ = _contentMetrics->motionMagnitudeNZ;

    //take product of size and magnitude with equal weight for now
    _motion.value = (1.0f - sizeZeroMotion) * motionMagNZ;

    //stabilize: motionMagNZ could be large when only few motion blocks are non-zero
    _stationaryMotion = false;
    if (sizeZeroMotion > HIGH_ZERO_MOTION_SIZE)
    {
        _motion.value = 0.0f;
        _stationaryMotion = true;
    }
    // determine motion level
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
VCMQmSelect::Spatial()
{
    float spatialErr =  _contentMetrics->spatialPredErr;
    float spatialErrH = _contentMetrics->spatialPredErrH;
    float spatialErrV = _contentMetrics->spatialPredErrV;
    //Spatial measure: take average of 3 prediction errors
    _spatial.value = (spatialErr + spatialErrH + spatialErrV) / 3.0f;

    float scale = 1.0f;
    //Reduce thresholds for HD scenes
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


WebRtc_Word32
VCMQmSelect::SetMaxRateForQM(WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    // Match image type
    WebRtc_UWord32 imageSize = width * height;

    if (imageSize < kFrameSizeTh[0])
    {
        _imageType  = 0;
    }
    else if (imageSize < kFrameSizeTh[1])
    {
        _imageType  = 1;
    }
    else if (imageSize < kFrameSizeTh[2])
    {
        _imageType  = 2;
    }
    else if (imageSize < kFrameSizeTh[3])
    {
        _imageType  = 3;
    }
    else if (imageSize < kFrameSizeTh[4])
    {
        _imageType  = 4;
    }
    else if (imageSize < kFrameSizeTh[5])
    {
        _imageType  = 5;
    }
    else
    {
        _imageType  = 6;
    }

    // set max rate based on image size
    _maxRateQM = kMaxRateQm[_imageType];

    return VCM_OK;
}

}
