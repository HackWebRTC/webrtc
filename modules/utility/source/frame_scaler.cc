/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef WEBRTC_MODULE_UTILITY_VIDEO
#include "frame_scaler.h"

#include "trace.h"
#include "vplib.h"

#ifndef NO_INTERPOLATOR
    #include "InterpolatorInterface.h"
#endif

namespace webrtc {
FrameScaler::FrameScaler()
    : _ptrVideoInterpolator(0),
      _outWidth(0),
      _outHeight(0),
      _inWidth(0),
      _inHeight(0)
{
}

FrameScaler::~FrameScaler( )
{
#ifndef NO_INTERPOLATOR
    if( _ptrVideoInterpolator != 0)
    {
        deleteInterpolator(_ptrVideoInterpolator);
    }
 #endif
}

WebRtc_Word32 FrameScaler::ResizeFrameIfNeeded(VideoFrame& videoFrame,
                                               WebRtc_UWord32 outWidth,
                                               WebRtc_UWord32 outHeight)
{
    if( videoFrame.Length( ) == 0)
    {
        return -1;
    }

    if((videoFrame.Width() != outWidth) || ( videoFrame.Height() != outHeight))
    {
        // Scale down by factor 2-4.
        if(videoFrame.Width() % outWidth == 0 &&
           videoFrame.Height() % outHeight == 0 &&
           (videoFrame.Width() / outWidth) == (videoFrame.Height() / outHeight))
        {
            const WebRtc_Word32 multiple = videoFrame.Width() / outWidth;
            WebRtc_UWord32 scaledWidth;
            WebRtc_UWord32 scaledHeight;
            switch(multiple)
            {
            case 2:
                ScaleI420FrameQuarter(videoFrame.Width(), videoFrame.Height(),
                                      videoFrame.Buffer());

                videoFrame.SetLength(outWidth * outHeight * 3 / 2);
                videoFrame.SetWidth( outWidth);
                videoFrame.SetHeight(outHeight);
                return 0;
            case 3:
                ScaleI420Down1_3(videoFrame.Width(), videoFrame.Height(),
                                 videoFrame.Buffer(), videoFrame.Size(),
                                 scaledWidth, scaledHeight);
                videoFrame.SetLength((outWidth * outHeight * 3) / 2);
                videoFrame.SetWidth(outWidth);
                videoFrame.SetHeight(outHeight);
                return 0;
            case 4:
                ScaleI420FrameQuarter(videoFrame.Width(), videoFrame.Height(),
                                      videoFrame.Buffer());

                ScaleI420FrameQuarter(videoFrame.Width() >> 1,
                                      videoFrame.Height() >> 1,
                                      videoFrame.Buffer());

                videoFrame.SetLength((outWidth * outHeight * 3)/ 2);
                videoFrame.SetWidth(outWidth);
                videoFrame.SetHeight(outHeight);
                return 0;
            default:
                break;
            }
        }
        // Scale up by factor 2-4.
        if(outWidth % videoFrame.Width() == 0 &&
           outHeight % videoFrame.Height() == 0 &&
           (outWidth / videoFrame.Width()) == (outHeight / videoFrame.Height()))
        {
            const WebRtc_Word32 multiple = outWidth / videoFrame.Width();
            WebRtc_UWord32 scaledWidth = 0;
            WebRtc_UWord32 scaledHeight = 0;
            switch(multiple)
            {
            case 2:
                videoFrame.VerifyAndAllocate((outHeight * outWidth * 3) / 2);
                ScaleI420Up2(videoFrame.Width(), videoFrame.Height(),
                             videoFrame.Buffer(), videoFrame.Size(),
                             scaledWidth, scaledHeight);
                videoFrame.SetLength((outWidth * outHeight * 3) / 2);
                videoFrame.SetWidth(outWidth);
                videoFrame.SetHeight(outHeight);
                return 0;
            case 3:
                videoFrame.VerifyAndAllocate((outWidth * outHeight * 3) / 2);
                ScaleI420Up2(videoFrame.Width(), videoFrame.Height(),
                             videoFrame.Buffer(), videoFrame.Size(),
                             scaledWidth, scaledHeight);

                ScaleI420Up3_2(scaledWidth, scaledHeight, videoFrame.Buffer(),
                               videoFrame.Size(), scaledWidth, scaledHeight);
                videoFrame.SetLength((outWidth * outHeight * 3) / 2);
                videoFrame.SetWidth(outWidth);
                videoFrame.SetHeight(outHeight);
                return 0;
            case 4:
                videoFrame.VerifyAndAllocate((outWidth * outHeight * 3) / 2);
                ScaleI420Up2(videoFrame.Width(), videoFrame.Height(),
                             videoFrame.Buffer(), videoFrame.Size(),
                             scaledWidth, scaledHeight);
                ScaleI420Up2(scaledWidth, scaledHeight, videoFrame.Buffer(),
                             videoFrame.Size(), scaledWidth, scaledHeight);
                videoFrame.SetLength((outWidth * outHeight * 3) / 2);
                videoFrame.SetWidth(outWidth);
                videoFrame.SetHeight(outHeight);
                return 0;
            default:
                break;
            }
        }
        // Use interpolator
#ifdef NO_INTERPOLATOR
        assert(!"Interpolation not available");
#else
        // Create new interpolator if the scaling changed.
        if((_outWidth != outWidth) || (_outHeight != outHeight) ||
           (_inWidth  != videoFrame.Width()) ||
           (_inHeight != videoFrame.Height()))
        {
            if(_ptrVideoInterpolator != 0)
            {
                deleteInterpolator(_ptrVideoInterpolator);
                _ptrVideoInterpolator = 0;
            }

            _outWidth  = outWidth;
            _outHeight = outHeight;
            _inWidth   = videoFrame.Width();
            _inHeight  = videoFrame.Height();
        }


        if (!_ptrVideoInterpolator)
        {
            InterpolatorType interpolator = BiCubicBSpline;

            if((_inWidth  > ( _outWidth * 2))  ||
               (_inWidth  < ( _outWidth / 2))  ||
               (_inHeight > ( _outHeight * 2)) ||
               (_inHeight < ( _outHeight / 2)))

            {
                interpolator = BiCubicSine;
            }

            VideoFrameFormat inputFormat;
            VideoFrameFormat outputFormat;

            inputFormat.videoType = YUV420P;
            inputFormat.xChannels = static_cast<short>(_inWidth);
            inputFormat.yChannels = static_cast<short>(_inHeight);

            outputFormat.videoType = YUV420P;
            outputFormat.xChannels = static_cast<short>(_outWidth);
            outputFormat.yChannels = static_cast<short>(_outHeight);

            _interpolatorBuffer.VerifyAndAllocate(_outWidth * _outHeight *
                                                  3 / 2);

            _ptrVideoInterpolator = createInterpolator(
                interpolator,
                &inputFormat,
                &outputFormat);
            if (_ptrVideoInterpolator == NULL)
            {
                WEBRTC_TRACE(
                    kTraceError,
                    kTraceVideo,
                    -1,
                    "FrameScaler::ResizeFrame(): Could not create\
 interpolator");
                return -1;
            }
        }

        interpolateFrame(_ptrVideoInterpolator, videoFrame.Buffer(),
                         _interpolatorBuffer.Buffer());

        videoFrame.VerifyAndAllocate(_interpolatorBuffer.Size());
        videoFrame.SetLength(_outWidth * _outHeight * 3 / 2);
        videoFrame.CopyFrame(videoFrame.Length(), _interpolatorBuffer.Buffer());
        videoFrame.SetWidth(_outWidth);
        videoFrame.SetHeight(_outHeight);
#endif // NO_INTERPOLATOR
    }
    return 0;
}
} // namespace webrtc

#endif // WEBRTC_MODULE_UTILITY_VIDEO
