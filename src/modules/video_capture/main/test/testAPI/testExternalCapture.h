/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_TEST_TESTAPI_TESTEXTERNALCAPTURE_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_TEST_TESTAPI_TESTEXTERNALCAPTURE_H_

#include "testDefines.h"
#include "video_capture_factory.h"

namespace webrtc
{

class testExternalCapture
    : public VideoCaptureDataCallback, public VideoCaptureFeedBack
{
public:
    testExternalCapture(void);
    ~testExternalCapture(void);

    void CreateInterface();
    int DoTest();

    // from VideoCaptureDataCallback
    virtual void OnIncomingCapturedFrame(const WebRtc_Word32 id,
                                         VideoFrame& videoFrame,
                                         VideoCodecType = kVideoCodecUnknown);

    virtual void OnCaptureDelayChanged(const WebRtc_Word32 id,
                                       const WebRtc_Word32 delay);

    //VideoCaptureFeedBack
    virtual void OnCaptureFrameRate(const WebRtc_Word32 id,
                                    const WebRtc_UWord32 frameRate);
    //VideoCaptureFeedBack
    virtual void OnNoPictureAlarm(const WebRtc_Word32 id,
                                  const VideoCaptureAlarm alarm);

private:

    int CompareFrames(const VideoFrame& frame1, const VideoFrame& frame2);

    VideoCaptureExternal* _captureInteface;
    VideoCaptureModule* _captureModule;

    VideoFrame _testFrame;

    VideoFrame _resultFrame;
    WebRtc_Word32 _reportedFrameRate;
    VideoCaptureAlarm _captureAlarm;
    WebRtc_Word32 _frameCount;

};
} //namespace webrtc
#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_TEST_TESTAPI_TESTEXTERNALCAPTURE_H_
