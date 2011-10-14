/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testExternalCapture.h"
#include "tick_util.h"
#include "process_thread.h"
#include "stdio.h"

namespace webrtc
{

static int testExternalCaptureResult = 0;

#ifdef NDEBUG
#if defined(WEBRTC_MAC_INTEL)

#else
#undef assert
#define assert(p) if(!(p)){printf("Error line %d\n",__LINE__);testExternalCaptureResult=-1;}
#endif
#endif

void testExternalCapture::CreateInterface()
{
    _captureModule = VideoCaptureFactory::Create(1, _captureInteface);
    _captureModule->AddRef();
}
testExternalCapture::testExternalCapture(void)
    : _captureInteface(NULL), _captureModule(NULL)
{
}

int testExternalCapture::CompareFrames(const VideoFrame& frame1,
                                       const VideoFrame& frame2)
{
    assert(frame1.Length()==frame2.Length());
    assert(frame1.Width()==frame2.Width());
    assert(frame1.Height()==frame2.Height());
    //assert(frame1.RenderTimeMs()==frame2.RenderTimeMs());
    for (unsigned int i = 0; i < frame1.Length(); ++i)
        assert(*(frame1.Buffer()+i)==*(frame2.Buffer()+i));
    return 0;

}

testExternalCapture::~testExternalCapture(void)
{
    _captureModule->Release();
}

void testExternalCapture::OnIncomingCapturedFrame(
                                                  const WebRtc_Word32 ID,
                                                  VideoFrame& videoFrame,
                                                  webrtc::VideoCodecType codecType)
{

    _resultFrame.CopyFrame(videoFrame);
    _frameCount++;
}

void testExternalCapture::OnCaptureDelayChanged(const WebRtc_Word32 ID,
                                                const WebRtc_Word32 delay)
{
}

void testExternalCapture::OnCaptureFrameRate(const WebRtc_Word32 id,
                                             const WebRtc_UWord32 frameRate)
{
    printf("OnCaptureFrameRate %d, frameRate %d\n", id, frameRate);
    _reportedFrameRate = frameRate;
}
void testExternalCapture::OnNoPictureAlarm(const WebRtc_Word32 id,
                                           const VideoCaptureAlarm alarm)
{
    printf("OnNoPictureAlarm %d, alarm %d\n", id, alarm);
    _captureAlarm = alarm;
}

int testExternalCapture::DoTest()
{
    int height = 288;
    int width = 352;

    printf("Platform independent test\n");

    CreateInterface();
    ProcessThread* processModule = ProcessThread::CreateProcessThread();
    processModule->Start();
    processModule->RegisterModule(_captureModule);

    _testFrame.VerifyAndAllocate(height * width * 3 / 2);
    _testFrame.SetLength(height * width * 3 / 2);
    _testFrame.SetHeight(height);
    _testFrame.SetWidth(width);
    memset(_testFrame.Buffer(), 0, 1);
    assert(_captureModule->RegisterCaptureDataCallback(*this)==0);
    assert(_captureModule->RegisterCaptureCallback(*this)==0);
    assert(_captureModule->EnableFrameRateCallback(true)==0);
    assert(_captureModule->EnableNoPictureAlarm(true)==0);

    VideoCaptureCapability frameInfo;
    frameInfo.width = width;
    frameInfo.height = height;
    frameInfo.rawType = webrtc::kVideoYV12;

    assert(_captureInteface->IncomingFrame(_testFrame.Buffer(),
                                           _testFrame.Length(),
                                           frameInfo,0)==0);
    CompareFrames(_testFrame, _resultFrame);

    printf("  testing the IncomingFrameI420 interface.\n");
    VideoFrameI420 frame_i420;
    frame_i420.width = width;
    frame_i420.height = height;
    frame_i420.y_plane = _testFrame.Buffer();
    frame_i420.u_plane = frame_i420.y_plane + (width * height);
    frame_i420.v_plane = frame_i420.u_plane + ((width * height) >> 2);
    frame_i420.y_pitch = width;
    frame_i420.u_pitch = width / 2;
    frame_i420.v_pitch = width / 2;
    assert(_captureInteface->IncomingFrameI420(frame_i420, 0) == 0);
    CompareFrames(_testFrame, _resultFrame);

    printf("  testing local frame rate callback and no picture alarm.\n");

    WebRtc_Word64 testTime = 3;
    _reportedFrameRate = 0;
    _captureAlarm = Cleared;

    TickTime startTime = TickTime::Now();
    while ((TickTime::Now() - startTime).Milliseconds() < testTime * 1000)
    {
        assert(_captureInteface->IncomingFrame(_testFrame.Buffer(),
                                               _testFrame.Length(),
                                               frameInfo,0)==0);
        SLEEP(100);
    }
    assert(_reportedFrameRate==10);
    SLEEP(500); // Make sure the no picture alarm is triggered
    assert(_captureAlarm==Raised);

    testTime = 3;
    startTime = TickTime::Now();
    while ((TickTime::Now() - startTime).Milliseconds() < testTime * 1000)
    {
        assert(_captureInteface->IncomingFrame(_testFrame.Buffer(),
                                               _testFrame.Length(),
                                               frameInfo,0)==0);
        SLEEP(33);
    }
    assert(_captureAlarm==Cleared);
    assert(_reportedFrameRate==30);

    //Test start image
    printf("  testing start send image.\n");
    testTime = 3;
    startTime = TickTime::Now();
    _frameCount = 0;
    assert(_captureModule->StartSendImage(_testFrame,15)==0);
    while ((TickTime::Now() - startTime).Milliseconds() < testTime * 1000)
    {
        SLEEP(33);
    }
    assert(_captureModule->StopSendImage()==0);
    assert(_frameCount>=testTime*15-1 && _frameCount<=testTime*15+1);
    assert(_captureAlarm==Raised);
    CompareFrames(_testFrame, _resultFrame);
    SLEEP(1000);
    assert(_frameCount>=testTime*15-1 && _frameCount<=testTime*15+1);

    processModule->Stop();

    ProcessThread::DestroyProcessThread(processModule);

    return testExternalCaptureResult;
}
} // namespace webrtc
