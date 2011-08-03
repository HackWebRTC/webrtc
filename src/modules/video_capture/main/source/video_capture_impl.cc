/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_capture_impl.h"
#include "trace.h"
#include "critical_section_wrapper.h"
#include "tick_util.h"
#include "vplib_conversions.h"
#include "video_capture_config.h"
#include "module_common_types.h"

#ifdef WEBRTC_ANDROID
#include "video_capture_android.h" // Need inclusion here to set Java environment.
#endif

namespace webrtc
{

VideoCaptureModule* VideoCaptureModule::Create(const WebRtc_Word32 id,
                                               VideoCaptureExternal*& externalCapture)
{
    videocapturemodule::VideoCaptureImpl* implementation =
                        new videocapturemodule::VideoCaptureImpl(id);
    externalCapture = implementation;
    return implementation;
}


void VideoCaptureModule::Destroy(VideoCaptureModule* module)
{
    delete module;
}

#ifdef WEBRTC_ANDROID
WebRtc_Word32 VideoCaptureModule::SetAndroidObjects(void* javaVM,void* javaContext)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, 0, "SetAndroidObjects");
    return videocapturemodule::VideoCaptureAndroid::SetAndroidObjects(javaVM,javaContext);
}
#endif

namespace videocapturemodule
{

WebRtc_Word32 VideoCaptureImpl::Version(WebRtc_Word8* version,
                                              WebRtc_UWord32& remainingBufferInBytes,
                                              WebRtc_UWord32& position) const
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, _id, "Version(bufferLength:%u)",
               (unsigned int) remainingBufferInBytes);
    return GetVersion(version, remainingBufferInBytes, position);
}

WebRtc_Word32 VideoCaptureImpl::GetVersion(WebRtc_Word8* version,
                                                 WebRtc_UWord32& remainingBufferInBytes,
                                                 WebRtc_UWord32& position)
{
    if (version == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, -1,
                   "Invalid in argument to Version()");
        return -1;
    }
    WebRtc_Word8 ourVersion[] = "VideoCaptureModule 1.1.0";
    WebRtc_UWord32 ourLength = (WebRtc_UWord32) strlen(ourVersion);
    if (remainingBufferInBytes < ourLength + 1)
    {
        return -1;
    }
    memcpy(version, ourVersion, ourLength);
    version[ourLength] = '\0'; // null terminaion
    remainingBufferInBytes -= (ourLength + 1);
    position += (ourLength + 1);
    return 0;
}

const WebRtc_UWord8* VideoCaptureImpl::CurrentDeviceName() const
{
    return _deviceUniqueId;
}

WebRtc_Word32 VideoCaptureImpl::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
    return 0;
}

// returns the number of milliseconds until the module want a worker thread to call Process
WebRtc_Word32 VideoCaptureImpl::TimeUntilNextProcess()
{
    TickTime timeNow = TickTime::Now();

    WebRtc_Word32 timeToNormalProcess = kProcessInterval
        - (WebRtc_Word32)((TickTime::Now() - _lastProcessTime).Milliseconds());
    WebRtc_Word32 timeToStartImage = timeToNormalProcess;
    if (_startImageFrameIntervall)
    {
        timeToStartImage = _startImageFrameIntervall
            - (WebRtc_Word32)((timeNow - _lastSentStartImageTime).Milliseconds());
        if (timeToStartImage < 0)
        {
            timeToStartImage = 0;
        }
    }
    return (timeToStartImage < timeToNormalProcess)
            ? timeToStartImage : timeToNormalProcess;
}

// Process any pending tasks such as timeouts
WebRtc_Word32 VideoCaptureImpl::Process()
{
    CriticalSectionScoped cs(_callBackCs);

    const TickTime now = TickTime::Now();
    _lastProcessTime = TickTime::Now();

    // Handle No picture alarm

    if (_lastProcessFrameCount.Ticks() == _incomingFrameTimes[0].Ticks() &&
        _captureAlarm != Raised)
    {
        if (_noPictureAlarmCallBack && _captureCallBack)
        {
            _captureAlarm = Raised;
            _captureCallBack->OnNoPictureAlarm(_id, _captureAlarm);
        }
    }
    else if (_lastProcessFrameCount.Ticks() != _incomingFrameTimes[0].Ticks() &&
             _captureAlarm != Cleared)
    {
        if (_noPictureAlarmCallBack && _captureCallBack)
        {
            _captureAlarm = Cleared;
            _captureCallBack->OnNoPictureAlarm(_id, _captureAlarm);

        }
    }

    // Handle frame rate callback
    if ((now - _lastFrameRateCallbackTime).Milliseconds()
        > kFrameRateCallbackInterval)
    {
        if (_frameRateCallBack && _captureCallBack)
        {
            const WebRtc_UWord32 frameRate = CalculateFrameRate(now);
            _captureCallBack->OnCaptureFrameRate(_id, frameRate);
        }
        _lastFrameRateCallbackTime = now; // Can be set by EnableFrameRateCallback

    }

    _lastProcessFrameCount = _incomingFrameTimes[0];

    // Handle start image frame rates.
    if (_startImageFrameIntervall
        && (now - _lastSentStartImageTime).Milliseconds() >= _startImageFrameIntervall)
    {
        _lastSentStartImageTime = now;
        if (_dataCallBack)
        {
            _captureFrame.CopyFrame(_startImage);
            _captureFrame.SetRenderTime(TickTime::MillisecondTimestamp());
            _dataCallBack->OnIncomingCapturedFrame(_id, _captureFrame,
                                                   kVideoCodecUnknown);
        }
    }
    return 0;
}

VideoCaptureImpl::VideoCaptureImpl(const WebRtc_Word32 id)
    : _id(id), _deviceUniqueId(NULL), _apiCs(*CriticalSectionWrapper::CreateCriticalSection()),
      _captureDelay(0), _requestedCapability(),
      _callBackCs(*CriticalSectionWrapper::CreateCriticalSection()),
      _lastProcessTime(TickTime::Now()),
      _lastFrameRateCallbackTime(TickTime::Now()), _frameRateCallBack(false),
      _noPictureAlarmCallBack(false), _captureAlarm(Cleared), _setCaptureDelay(0),
      _dataCallBack(NULL), _captureCallBack(NULL), _startImageFrameIntervall(0),
      _lastProcessFrameCount(TickTime::Now()), _rotateFrame(kRotateNone)

{
    _requestedCapability.width = kDefaultWidth;
    _requestedCapability.height = kDefaultHeight;
    _requestedCapability.maxFPS = 30;
    _requestedCapability.rawType = kVideoI420;
    _requestedCapability.codecType = kVideoCodecUnknown;
    memset(_incomingFrameTimes, 0, sizeof(_incomingFrameTimes));
}

VideoCaptureImpl::~VideoCaptureImpl()
{
    DeRegisterCaptureDataCallback();
    DeRegisterCaptureCallback();
    delete &_callBackCs;
    delete &_apiCs;

    if (_deviceUniqueId)
        delete[] _deviceUniqueId;
}

WebRtc_Word32 VideoCaptureImpl::RegisterCaptureDataCallback(
                                        VideoCaptureDataCallback& dataCallBack)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, _id,
               "RegisterCaptureDataCallback");
    CriticalSectionScoped cs(_apiCs);
    CriticalSectionScoped cs2(_callBackCs);
    _dataCallBack = &dataCallBack;

    return 0;
}

WebRtc_Word32 VideoCaptureImpl::DeRegisterCaptureDataCallback()
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, _id,
               "DeRegisterCaptureDataCallback");
    CriticalSectionScoped cs(_apiCs);
    CriticalSectionScoped cs2(_callBackCs);
    _dataCallBack = NULL;
    return 0;
}
WebRtc_Word32 VideoCaptureImpl::RegisterCaptureCallback(VideoCaptureFeedBack& callBack)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, _id, "RegisterCaptureCallback %x");

    CriticalSectionScoped cs(_apiCs);
    CriticalSectionScoped cs2(_callBackCs);
    _captureCallBack = &callBack;
    return 0;
}
WebRtc_Word32 VideoCaptureImpl::DeRegisterCaptureCallback()
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, _id, "DeRegisterCaptureCallback");

    CriticalSectionScoped cs(_apiCs);
    CriticalSectionScoped cs2(_callBackCs);
    _captureCallBack = NULL;
    return 0;

}
WebRtc_Word32 VideoCaptureImpl::SetCaptureDelay(WebRtc_Word32 delayMS)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, _id, "SetCaptureDelay %d",
               (int) delayMS);
    CriticalSectionScoped cs(_apiCs);
    _captureDelay = delayMS;
    return 0;
}
WebRtc_Word32 VideoCaptureImpl::CaptureDelay()
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, _id, "CaptureDelay %d",
               (int) _captureDelay);
    CriticalSectionScoped cs(_apiCs);
    return _setCaptureDelay;
}
WebRtc_Word32 VideoCaptureImpl::IncomingFrame(WebRtc_UWord8* videoFrame,
                                                    WebRtc_Word32 videoFrameLength,
                                                    const VideoCaptureCapability& frameInfo,
                                                    WebRtc_Word64 captureTime/*=0*/)
{
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideoCapture, _id,
               "IncomingFrame width %d, height %d", (int) frameInfo.width,
               (int) frameInfo.height);

    TickTime startProcessTime = TickTime::Now();

    CriticalSectionScoped cs(_callBackCs);

    const WebRtc_Word32 width = frameInfo.width;
    const WebRtc_Word32 height = frameInfo.height;

    UpdateFrameCount();// frame count used for local frame rate callback.

    _startImageFrameIntervall = 0; // prevent the start image to be displayed.

    if (frameInfo.codecType == kVideoCodecUnknown) // None encoded. Convert to I420.
    {
        const VideoType vpLibType =RawVideoTypeToVplibVideoType(frameInfo.rawType);
        int size = CalcBufferSize(vpLibType, width, height);
        if (size != videoFrameLength)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                       "Wrong incoming frame length.");
            return -1;
        }

        // Allocate I420 buffer
        _captureFrame.VerifyAndAllocate(CalcBufferSize(kI420, width, height));
        if (!_captureFrame.Buffer())
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                       "Failed to allocate frame buffer.");
            return -1;
        }

        memset(_captureFrame.Buffer(), 0, _captureFrame.Size());
        const WebRtc_Word32 conversionResult = ConvertToI420(vpLibType, videoFrame,
                                                             width, height,
                                                             _captureFrame.Buffer(),
                                                             _requestedCapability.interlaced,
                                                             _rotateFrame);
        if (conversionResult <= 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                       "Failed to convert capture frame from type %d to I420",
                       frameInfo.rawType);
            return -1;
        }
        _captureFrame.SetLength(conversionResult);
    }
    else // Encoded format
    {
        if (_captureFrame.CopyFrame(videoFrameLength, videoFrame) != 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                       "Failed to copy captured frame of length %d", (int) videoFrameLength);
        }
    }

    const bool callOnCaptureDelayChanged = _setCaptureDelay != _captureDelay;
    if (_setCaptureDelay != _captureDelay) // Capture delay changed
    {
        _setCaptureDelay = _captureDelay;
    }

    // Set the capture time
    if (captureTime != 0)
    {
        _captureFrame.SetRenderTime(captureTime);
    }
    else
    {
        _captureFrame.SetRenderTime(TickTime::MillisecondTimestamp());
    }

    _captureFrame.SetHeight(height);
    _captureFrame.SetWidth(width);

    if (_dataCallBack)
    {
        if (callOnCaptureDelayChanged)
        {
            _dataCallBack->OnCaptureDelayChanged(_id, _captureDelay);
        }
        _dataCallBack->OnIncomingCapturedFrame(_id, _captureFrame, frameInfo.codecType);
    }

    const WebRtc_UWord32 processTime =
        (WebRtc_UWord32)(TickTime::Now() - startProcessTime).Milliseconds();
    if (processTime > 10) // If the process time is too long MJPG will not work well.
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCapture, _id,
                   "Too long processing time of Incoming frame: %ums",
                   (unsigned int) processTime);
    }

    return 0;

}

WebRtc_Word32 VideoCaptureImpl::SetCaptureRotation(VideoCaptureRotation rotation)
{
    CriticalSectionScoped cs(_apiCs);
    CriticalSectionScoped cs2(_callBackCs);
    switch (rotation)
    {
        case kCameraRotate0:
            _rotateFrame = kRotateNone;
            break;
        case kCameraRotate90:
            _rotateFrame = kRotateClockwise;
            break;
        case kCameraRotate180:
            _rotateFrame = kRotate180;
            break;
        case kCameraRotate270:
            _rotateFrame = kRotateAntiClockwise;
            break;
    }
    return 0;
}

WebRtc_Word32 VideoCaptureImpl::StartSendImage(const VideoFrame& videoFrame,
                                                     WebRtc_Word32 frameRate)
{
    WEBRTC_TRACE(webrtc::kTraceModuleCall, webrtc::kTraceVideoCapture, _id,
               "StartSendImage, frameRate %d", (int) frameRate);
    CriticalSectionScoped cs(_apiCs);
    CriticalSectionScoped cs2(_callBackCs);
    if (frameRate < 1 || frameRate > kMaxFrameRate)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "StartSendImage Invalid parameter. frameRate %d", (int) frameRate);
        return -1;;
    }
    _startImage.CopyFrame(videoFrame);
    _startImageFrameIntervall = 1000 / frameRate;
    _lastSentStartImageTime = TickTime::Now();
    return 0;

}
WebRtc_Word32 VideoCaptureImpl::StopSendImage()
{
    CriticalSectionScoped cs(_apiCs);
    CriticalSectionScoped cs2(_callBackCs);
    _startImageFrameIntervall = 0;
    return 0;
}

WebRtc_Word32 VideoCaptureImpl::EnableFrameRateCallback(const bool enable)
{
    CriticalSectionScoped cs(_apiCs);
    CriticalSectionScoped cs2(_callBackCs);
    _frameRateCallBack = enable;
    if (enable)
    {
        _lastFrameRateCallbackTime = TickTime::Now();
    }
    return 0;
}

WebRtc_Word32 VideoCaptureImpl::EnableNoPictureAlarm(const bool enable)
{
    CriticalSectionScoped cs(_apiCs);
    CriticalSectionScoped cs2(_callBackCs);
    _noPictureAlarmCallBack = enable;
    return 0;
}

void VideoCaptureImpl::UpdateFrameCount()
{
    if (_incomingFrameTimes[0].MicrosecondTimestamp() == 0)
    {
        // first no shift
    }
    else
    {
        // shift
        for (int i = (kFrameRateCountHistorySize - 2); i >= 0; i--)
        {
            _incomingFrameTimes[i + 1] = _incomingFrameTimes[i];
        }
    }
    _incomingFrameTimes[0] = TickTime::Now();
}

WebRtc_UWord32 VideoCaptureImpl::CalculateFrameRate(const TickTime& now)
{
    WebRtc_Word32 num = 0;
    WebRtc_Word32 nrOfFrames = 0;
    for (num = 1; num < (kFrameRateCountHistorySize - 1); num++)
    {
        if (_incomingFrameTimes[num].Ticks() <= 0
            || (now - _incomingFrameTimes[num]).Milliseconds() > kFrameRateHistoryWindowMs) // don't use data older than 2sec
        {
            break;
        }
        else
        {
            nrOfFrames++;
        }
    }
    if (num > 1)
    {
        WebRtc_Word64 diff = (now - _incomingFrameTimes[num - 1]).Milliseconds();
        if (diff > 0)
        {
            return WebRtc_UWord32((nrOfFrames * 1000.0f / diff) + 0.5f);
        }
    }

    return nrOfFrames;
}
} //namespace videocapturemodule
} // namespace webrtc
