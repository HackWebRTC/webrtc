/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "spatial_resampler.h"


namespace webrtc {

VPMSimpleSpatialResampler::VPMSimpleSpatialResampler()
:
_resamplingMode(kFastRescaling),
_targetWidth(0),
_targetHeight(0),
_interpolatorPtr(NULL)
{
}

VPMSimpleSpatialResampler::~VPMSimpleSpatialResampler()
{
    Release();
}

WebRtc_Word32
VPMSimpleSpatialResampler::Release()
{
    if (_interpolatorPtr != NULL)
    {
        delete _interpolatorPtr;
        _interpolatorPtr = NULL;
    }
    return VPM_OK;
}

WebRtc_Word32
VPMSimpleSpatialResampler::SetTargetFrameSize(WebRtc_UWord32 width,
                                              WebRtc_UWord32 height)
{
    if (_resamplingMode == kNoRescaling)
    {
        return VPM_OK;
    }

    if (width < 1 || height < 1)
    {
        return VPM_PARAMETER_ERROR;
    }

    _targetWidth = width;
    _targetHeight = height;

    return VPM_OK;
}

void
VPMSimpleSpatialResampler::SetInputFrameResampleMode(VideoFrameResampling
                                                     resamplingMode)
{
    _resamplingMode = resamplingMode;
}

void
VPMSimpleSpatialResampler::Reset()
{
    _resamplingMode = kFastRescaling;
    _targetWidth = 0;
    _targetHeight = 0;
}

WebRtc_Word32
VPMSimpleSpatialResampler::ResampleFrame(const VideoFrame& inFrame,
                                         VideoFrame& outFrame)
{
    WebRtc_Word32 ret;

    if (_resamplingMode == kNoRescaling)
    {
        return outFrame.CopyFrame(inFrame);
    }
    else if (_targetWidth < 1 || _targetHeight < 1)
    {
        return VPM_PARAMETER_ERROR;
    }

    // Check if re-sampling is needed
    if ((inFrame.Width() == _targetWidth) &&
        (inFrame.Height() == _targetHeight))
    {
        return outFrame.CopyFrame(inFrame);
    }
    if (_resamplingMode == kBiLinear)
    {
        return BiLinearInterpolation(inFrame, outFrame);
    }

    outFrame.SetTimeStamp(inFrame.TimeStamp());

    if (_targetWidth > inFrame.Width() &&
        ( ExactMultiplier(inFrame.Width(), inFrame.Height())))
    {
        // The codec might want to pad this later... adding 8 pixels
        const WebRtc_UWord32 requiredSize = (_targetWidth + 8) *
                                            (_targetHeight + 8) * 3 / 2;
        outFrame.VerifyAndAllocate(requiredSize);
        return UpsampleFrame(inFrame, outFrame);
    }
    else
    {
        // 1 cut/pad
        // 2 scale factor 2X (in both cases if required)
        WebRtc_UWord32 croppedWidth = inFrame.Width();
        WebRtc_UWord32 croppedHeight = inFrame.Height();

        //Calculates cropped dimensions
        CropSize(inFrame.Width(), inFrame.Height(),
                 croppedWidth, croppedHeight);

        VideoFrame* targetFrame;
        outFrame.VerifyAndAllocate(croppedWidth * croppedHeight * 3 / 2);
        targetFrame = &outFrame;

        ConvertI420ToI420(inFrame.Buffer(), inFrame.Width(), inFrame.Height(),
                          targetFrame->Buffer(), croppedWidth, croppedHeight);
        targetFrame->SetWidth(croppedWidth);
        targetFrame->SetHeight(croppedHeight);
        //We have correct aspect ratio, sub-sample with a multiple of two to get
        //close to the target size
        ret = SubsampleMultipleOf2(*targetFrame);

        if (ret != VPM_OK)
        {
            return ret;
        }
    }

    return VPM_OK;
}

WebRtc_Word32
VPMSimpleSpatialResampler::UpsampleFrame(const VideoFrame& inFrame,
                                         VideoFrame& outFrame)
{
    outFrame.CopyFrame(inFrame);
    WebRtc_UWord32 currentLength = inFrame.Width() * inFrame.Height() * 3 / 2;

    float ratioWidth = _targetWidth / (float)inFrame.Width();
    float ratioHeight = _targetHeight / (float)inFrame.Height();

    WebRtc_UWord32 scaledWidth = 0;
    WebRtc_UWord32 scaledHeight = 0;

    if(ratioWidth > 1 || ratioHeight > 1)
    {
        // scale up
        if(ratioWidth <= 1.5 && ratioHeight <= 1.5)
        {
            // scale up 1.5
            currentLength = ScaleI420Up3_2(inFrame.Width(), inFrame.Height(),
                                           outFrame.Buffer(), outFrame.Size(),
                                           scaledWidth, scaledHeight);
        }
        else if(ratioWidth <= 2 && ratioHeight <= 2)
        {
            // scale up 2
            currentLength = ScaleI420Up2(inFrame.Width(), inFrame.Height(),
                                         outFrame.Buffer(), outFrame.Size(),
                                         scaledWidth, scaledHeight);
        }
        else if(ratioWidth <= 2.25 && ratioHeight <= 2.25)
        {
            // scale up 2.25
            currentLength = ScaleI420Up3_2(inFrame.Width(), inFrame.Height(),
                                           outFrame.Buffer(), outFrame.Size(),
                                           scaledWidth, scaledHeight);
            currentLength = ScaleI420Up3_2(scaledWidth, scaledHeight,
                                           outFrame.Buffer(), outFrame.Size(),
                                           scaledWidth, scaledHeight);
        }
        else if(ratioWidth <= 3 && ratioHeight <= 3)
        {
            // scale up 3
            currentLength = ScaleI420Up2(inFrame.Width(), inFrame.Height(),
                                         outFrame.Buffer(), outFrame.Size(),
                                         scaledWidth, scaledHeight);
            currentLength = ScaleI420Up3_2(scaledWidth, scaledHeight,
                                           outFrame.Buffer(), outFrame.Size(),
                                           scaledWidth, scaledHeight);
        }
        else if(ratioWidth <= 4 && ratioHeight <= 4)
        {
            // scale up 4
            currentLength = ScaleI420Up2(inFrame.Width(), inFrame.Height(),
                                         outFrame.Buffer(), outFrame.Size(),
                                         scaledWidth, scaledHeight);
            currentLength = ScaleI420Up2(scaledWidth, scaledHeight,
                                         outFrame.Buffer(), outFrame.Size(),
                                         scaledWidth, scaledHeight);
        }

        //TODO: what if ratioWidth/Height >= 8 ?

        if (scaledWidth <= 0 || scaledHeight <= 0)
        {
            return VPM_GENERAL_ERROR;
        }

        if ((static_cast<WebRtc_UWord32>(scaledWidth) > _targetWidth) ||
            (static_cast<WebRtc_UWord32>(scaledHeight) > _targetHeight))
        {
            currentLength = CutI420Frame(outFrame.Buffer(), scaledWidth,
                                         scaledHeight, _targetWidth,
                                         _targetHeight);
        }
    }
    else
    {
        return VPM_GENERAL_ERROR;
    }

    outFrame.SetWidth(_targetWidth);
    outFrame.SetHeight(_targetHeight);
    outFrame.SetLength(_targetWidth * _targetHeight * 3 / 2);

    return VPM_OK;
}

WebRtc_Word32
VPMSimpleSpatialResampler::CropSize(WebRtc_UWord32 width, WebRtc_UWord32 height,
                                    WebRtc_UWord32& croppedWidth,
                                    WebRtc_UWord32& croppedHeight) const
{
    // Crop the image to a width and height  which is a
    // multiple of two, so that we can do a simpler scaling.
    croppedWidth = _targetWidth;
    croppedHeight = _targetHeight;

    if (width >= 8 * _targetWidth && height >= 8 * _targetHeight)
    {
        croppedWidth = 8 * _targetWidth;
        croppedHeight = 8 * _targetHeight;
    }
    else if (width >= 4 * _targetWidth && height >= 4 * _targetHeight)
    {
        croppedWidth = 4 * _targetWidth;
        croppedHeight = 4 * _targetHeight;
    }
    else if (width >= 2 * _targetWidth && height >= 2 * _targetHeight)
    {
        croppedWidth = 2 * _targetWidth;
        croppedHeight = 2 * _targetHeight;
    }
    return VPM_OK;
}

WebRtc_Word32
VPMSimpleSpatialResampler::SubsampleMultipleOf2(VideoFrame& frame)
{
    WebRtc_UWord32 tempWidth = frame.Width();
    WebRtc_UWord32 tempHeight = frame.Height();

    while (tempWidth / _targetWidth >= 2 && tempHeight / _targetHeight >= 2)
    {
        ScaleI420FrameQuarter(tempWidth, tempHeight, frame.Buffer());
        tempWidth /= 2;
        tempHeight /= 2;
    }
    frame.SetWidth(tempWidth);
    frame.SetHeight(tempHeight);
    frame.SetLength(frame.Width() * frame.Height() * 3 / 2);

    return VPM_OK;
}


bool
VPMSimpleSpatialResampler::ExactMultiplier(WebRtc_UWord32 width,
                                           WebRtc_UWord32 height) const
{
    bool exactMultiplier = false;
    if (_targetWidth % width == 0 && _targetHeight % height == 0)
    {
        // we have a multiple, is it an even multiple?
        WebRtc_Word32 widthMultiple = _targetWidth / width;
        WebRtc_Word32 heightMultiple = _targetHeight / height;
        if ((widthMultiple == 2 && heightMultiple == 2) ||
            (widthMultiple == 4 && heightMultiple == 4) ||
            (widthMultiple == 8 && heightMultiple == 8) ||
            (widthMultiple == 1 && heightMultiple == 1))
        {
            exactMultiplier = true;
        }
    }
    return exactMultiplier;
}

WebRtc_Word32
VPMSimpleSpatialResampler::BiLinearInterpolation(const VideoFrame& inFrame,
                                                 VideoFrame& outFrame)
{
    WebRtc_Word32 retVal;

   if (_interpolatorPtr == NULL)
   {
       _interpolatorPtr = new interpolator();
   }
   // set bi-linear interpolator
   retVal =  _interpolatorPtr->Set(inFrame.Width(), inFrame.Height(),
                                   _targetWidth, _targetHeight,
                                   kI420, kI420, kBilinear );
    if (retVal < 0 )
    {
        return retVal;
    }

    // Verify size of output buffer
    outFrame.VerifyAndAllocate(_targetHeight * _targetWidth * 3 >> 1);
    WebRtc_UWord32 outSz = outFrame.Size();

    // interpolate frame
    retVal = _interpolatorPtr->Interpolate(inFrame.Buffer(),
                                           outFrame.Buffer(), outSz);

    assert(outSz <= outFrame.Size());

    // returns height
    if (retVal < 0)
    {
        return retVal;
    }

    // Set output frame parameters
    outFrame.SetHeight(_targetHeight);
    outFrame.SetWidth(_targetWidth);
    outFrame.SetLength(outSz);
    outFrame.SetTimeStamp(inFrame.TimeStamp());
    return VPM_OK;
}

WebRtc_UWord32
VPMSimpleSpatialResampler::TargetHeight()
{
    return _targetHeight;
}

WebRtc_UWord32
VPMSimpleSpatialResampler::TargetWidth()
{
    return _targetWidth;
}


} //namespace
