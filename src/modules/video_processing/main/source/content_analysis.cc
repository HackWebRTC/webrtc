/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "content_analysis.h"
#include "tick_util.h"

#include <math.h>
#include <stdlib.h>

namespace webrtc {

VPMContentAnalysis::VPMContentAnalysis():
_origFrame(NULL),
_prevFrame(NULL),
_width(0),
_height(0),
_motionMagnitudeNZ(0.0f),
_spatialPredErr(0.0f),
_spatialPredErrH(0.0f),
_spatialPredErrV(0.0f),
_sizeZeroMotion(0.0f),
_motionPredErr(0.0f),
_motionHorizontalness(0.0f),
_motionClusterDistortion(0.0f),
_firstFrame(true),
_CAInit(false),
_cMetrics(NULL)
{
    Release();
}

VPMContentAnalysis::~VPMContentAnalysis()
{
    Release();
}



VideoContentMetrics*
VPMContentAnalysis::ComputeContentMetrics(const VideoFrame* inputFrame)
{
    if (inputFrame == NULL)
    {
        return NULL;
    }

    //Init if needed (native dimension change)
    if (_width != inputFrame->Width() || _height != inputFrame->Height())
    {
        Initialize((WebRtc_UWord16)inputFrame->Width(), (WebRtc_UWord16)inputFrame->Height());
    }

    _origFrame = inputFrame->Buffer();

    //compute spatial metrics: 3 spatial prediction errors
    ComputeSpatialMetrics();

    //compute motion metrics
    if (_firstFrame == false)
        ComputeMotionMetrics();

   // saving current frame as previous one: Y only
   memcpy(_prevFrame, _origFrame, _width * _height);

   _firstFrame =  false;
   _CAInit = true;

    return ContentMetrics();
}

WebRtc_Word32
VPMContentAnalysis::Release()
{
    if (_cMetrics != NULL)
    {
        delete _cMetrics;
       _cMetrics = NULL;
    }

    if (_prevFrame != NULL)
    {
        delete [] _prevFrame;
        _prevFrame = NULL;
    }

    _width = 0;
    _height = 0;
    _firstFrame = true;

    return VPM_OK;
}

WebRtc_Word32
VPMContentAnalysis::Initialize(WebRtc_UWord16 width, WebRtc_UWord16 height)
{
   _width = width;
   _height = height;
   _firstFrame = true;

    if (_cMetrics != NULL)
    {
        delete _cMetrics;
    }
    _cMetrics = new VideoContentMetrics();
    if (_cMetrics == NULL)
    {
        return VPM_MEMORY;
    }

    if (_prevFrame != NULL)
    {
        delete [] _prevFrame;
    }
    _prevFrame = new WebRtc_UWord8[_width * _height] ; // Y only
    if (_prevFrame == NULL)
    {
        return VPM_MEMORY;
    }

    return VPM_OK;
}


//Compute motion metrics: magnitude over non-zero motion vectors, and size of zero cluster
WebRtc_Word32
VPMContentAnalysis::ComputeMotionMetrics()
{

    //Motion metrics: only one is derived from normalized  (MAD) temporal difference
    TemporalDiffMetric();

	  return VPM_OK;
}


//Normalized temporal difference (MAD): used as a motion level metric
//Normalize MAD by spatial contrast: images with more contrast (pixel variance) likely have larger temporal difference
//To reduce complexity, we compute the metric for a reduced set of points.
WebRtc_Word32
VPMContentAnalysis::TemporalDiffMetric()
{

    //size of original frame
    WebRtc_UWord16 sizei = _height;
    WebRtc_UWord16 sizej = _width;

    //skip parameter: # of skipped pixels along x & y direction: for complexity reduction
    WebRtc_UWord8 skipNum = 1; // 1 == all pixels, 2 == 1/4 reduction, 3 == 1/9 reduction

    //use skipNum = 2 for 4CIF, WHD
    if ( (sizei >=  576) && (sizej >= 704) )
    {
        skipNum = 2;
    }
    //use skipNum = 3 for FULLL_HD images
    if ( (sizei >=  1080) && (sizej >= 1920) )
    {
        skipNum = 3;
    }

    float contrast = 0.0f;
    float tempDiffAvg = 0.0f;
    float pixelSumAvg = 0.0f;
    float pixelSqSumAvg = 0.0f;

    WebRtc_UWord32 tempDiffSum = 0;
    WebRtc_UWord32 pixelSum = 0;
    WebRtc_UWord32 pixelSqSum = 0;

    WebRtc_UWord8 bord = 8; //avoid boundary
    WebRtc_UWord32 numPixels = 0; //counter for # of pixels
    WebRtc_UWord32 ssn;

    for(WebRtc_UWord16 i = bord; i < sizei - bord; i += skipNum)
    for(WebRtc_UWord16 j = bord; j < sizej - bord; j += skipNum)
    {
        numPixels += 1;
        ssn =  i * sizej + j;

        WebRtc_UWord8 currPixel  = _origFrame[ssn];
        WebRtc_UWord8 prevPixel  = _prevFrame[ssn];

        tempDiffSum += (WebRtc_UWord32) abs((WebRtc_Word16)(currPixel - prevPixel));
        pixelSum += (WebRtc_UWord32) _origFrame[ssn];
        pixelSqSum += (WebRtc_UWord32) (_origFrame[ssn] * _origFrame[ssn]);
    }

    //default
    _motionMagnitudeNZ = 0.0f;

    if (tempDiffSum == 0)
    {
        return VPM_OK;
    }

    //normalize over all pixels
    tempDiffAvg = (float)tempDiffSum / (float)(numPixels);
    pixelSumAvg = (float)pixelSum / (float)(numPixels);
    pixelSqSumAvg = (float)pixelSqSum / (float)(numPixels);
    contrast = pixelSqSumAvg - (pixelSumAvg * pixelSumAvg);

    if (contrast > 0.0)
    {
        contrast = sqrt(contrast);
       _motionMagnitudeNZ = tempDiffAvg/contrast;
    }

    return VPM_OK;

}


//Compute spatial metrics: 
//To reduce complexity, we compute the metric for a reduced set of points.
//The spatial metrics are rough estimates of the prediction error cost for each QM spatial mode: 2x2,1x2,2x1
//The metrics are a simple estimate of the up-sampling prediction error, estimated assuming sub-sampling for decimation (no filtering),
//and up-sampling back up with simple bilinear interpolation.
WebRtc_Word32
VPMContentAnalysis::ComputeSpatialMetrics()
{
    //size of original frame
    WebRtc_UWord16 sizei = _height;
    WebRtc_UWord16 sizej = _width;

    //skip parameter: # of skipped pixels along x & y direction: for complexity reduction
    WebRtc_UWord8 skipNum = 1; // 1 == all pixels, 2 == 1/4 reduction, 3 == 1/9 reduction

    //use skipNum = 2 for 4CIF, WHD
    if ( (sizei >=  576) && (sizej >= 704) )
    {
        skipNum = 2;
    }
    //use skipNum = 3 for FULLL_HD images
    if ( (sizei >=  1080) && (sizej >= 1920) )
    {
        skipNum = 3;
    }

    float spatialErr = 0.0f;
    float spatialErrH = 0.0f;
    float spatialErrV = 0.0f;

    //pixel mean square average: used to normalize the spatial metrics
    float pixelMSA = 0;
    float norm = 1.0f;

    WebRtc_UWord8 bord = 8; //avoid boundary
    WebRtc_UWord32 numPixels = 0; //counter for # of pixels

    WebRtc_UWord32 ssn1,ssn2,ssn3,ssn4,ssn5;

    WebRtc_UWord32 spatialErrSum = 0;
    WebRtc_UWord32 spatialErrVSum = 0;
    WebRtc_UWord32 spatialErrHSum = 0;

    for(WebRtc_UWord16 i = bord; i < sizei - bord; i += skipNum)
    for(WebRtc_UWord16 j = bord; j < sizej - bord; j += skipNum)
    {
        numPixels += 1;
        ssn1=  i * sizej + j;
        ssn2 = (i + 1) * sizej + j;	//bottom
        ssn3 = (i - 1) * sizej + j; //top
        ssn4 = i * sizej + j + 1;	//right
        ssn5 = i * sizej + j - 1;	//left

        WebRtc_UWord16 refPixel1  = _origFrame[ssn1] << 1;
        WebRtc_UWord16 refPixel2  = _origFrame[ssn1] << 2;

        WebRtc_UWord8 bottPixel = _origFrame[ssn2];
        WebRtc_UWord8 topPixel = _origFrame[ssn3];
        WebRtc_UWord8 rightPixel = _origFrame[ssn4];
        WebRtc_UWord8 leftPixel = _origFrame[ssn5];

        spatialErrSum +=  (WebRtc_UWord32) abs((WebRtc_Word16)(refPixel2 - (WebRtc_UWord16)(bottPixel + topPixel + leftPixel + rightPixel)));
        spatialErrVSum +=  (WebRtc_UWord32) abs((WebRtc_Word16)(refPixel1 - (WebRtc_UWord16)(bottPixel + topPixel)));
        spatialErrHSum +=  (WebRtc_UWord32) abs((WebRtc_Word16)(refPixel1 - (WebRtc_UWord16)(leftPixel + rightPixel)));

        pixelMSA += (float)_origFrame[ssn1];
    }

    //normalize over all pixels
    spatialErr = (float)spatialErrSum / (float)(4 * numPixels);
    spatialErrH = (float)spatialErrHSum / (float)(2 * numPixels);
    spatialErrV = (float)spatialErrVSum / (float)(2 * numPixels);
    norm = (float)pixelMSA / float(numPixels);

   
    //normalize to RMS pixel level: use avg pixel level for now

    //2X2:
    _spatialPredErr = spatialErr / (norm);

    //1X2:
    _spatialPredErrH = spatialErrH / (norm);

    //2X1:
    _spatialPredErrV = spatialErrV / (norm);


    return VPM_OK;
}


VideoContentMetrics*
VPMContentAnalysis::ContentMetrics()
{
    if (_CAInit == false)
    {
        return NULL;
    }


    _cMetrics->spatialPredErr = _spatialPredErr;
    _cMetrics->spatialPredErrH = _spatialPredErrH;
    _cMetrics->spatialPredErrV = _spatialPredErrV;
    //normalized temporal difference (MAD)
    _cMetrics->motionMagnitudeNZ = _motionMagnitudeNZ;

    //Set to zero: not computed
    _cMetrics->motionPredErr = _motionPredErr;
    _cMetrics->sizeZeroMotion = _sizeZeroMotion;
    _cMetrics->motionHorizontalness = _motionHorizontalness;
    _cMetrics->motionClusterDistortion = _motionClusterDistortion;

    return _cMetrics;

}

} //namespace
