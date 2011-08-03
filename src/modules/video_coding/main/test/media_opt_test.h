/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// VCM Media Optimization Test
#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_MEDIA_OPT_TEST_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_MEDIA_OPT_TEST_H_

#include "video_coding.h"
#include "test_util.h"
#include "video_source.h"

#include <string>

using namespace std;
//

// media optimization test
// This test simulates a complete encode-decode cycle via the RTP module.
// allows error resilience tests, packet loss tests, etc.
// Does not test the media optimization deirectly, but via the VCM API only.
// The test allows two modes:
// 1 - Standard, basic settings, one run
// 2 - Release test - iterates over a number of video sequences, bit rates, packet loss values ,etc.

class VCMTestProtectionCallback: public webrtc::VCMProtectionCallback
{
public:
    VCMTestProtectionCallback();
    virtual ~VCMTestProtectionCallback();
    WebRtc_Word32 ProtectionRequest(const WebRtc_UWord8 deltaFECRate,
                                    const WebRtc_UWord8 keyFECRate,
                                    const bool deltaUseUepProtection,
                                    const bool keyUseUepProtection,
                                    const bool nack);
    enum webrtc::NACKMethod NACKMethod();
    WebRtc_UWord8 FECDeltaRate();
    WebRtc_UWord8 FECKeyRate();
    bool          FECDeltaUepProtection();
    bool          FECKeyUepProtection();
private:
    WebRtc_UWord8     _deltaFECRate;
    WebRtc_UWord8     _keyFECRate;
    bool              _deltaUseUepProtection;
    bool              _keyUseUepProtection;
    enum webrtc::NACKMethod   _nack;

};


class MediaOptTest
{
public:
    MediaOptTest(webrtc::VideoCodingModule* vcm);
    ~MediaOptTest();

    static int RunTest(int testNum, CmdArgs& args);
    // perform encode-decode of an entire sequence
    WebRtc_Word32 Perform();
    // Set up for a single mode test
    void Setup(int testType, CmdArgs& args);
    // General set up - applicable for both modes
    void GeneralSetup();
    // Run release testing
    void RTTest();
    void TearDown();
    // mode = 1; will print to screen, otherwise only to log file
    void Print(int mode);

private:

    webrtc::VideoCodingModule*       _vcm;
    webrtc::RtpRtcp*                 _rtp;
    std::string                      _inname;
    std::string                      _outname;
    std::string                      _actualSourcename;
    std::fstream                     _log;
    FILE*                            _sourceFile;
    FILE*                            _decodedFile;
    FILE*                            _actualSourceFile;
    FILE*                            _outputRes;
    WebRtc_UWord16                   _width;
    WebRtc_UWord16                   _height;
    WebRtc_UWord32                   _lengthSourceFrame;
    WebRtc_UWord32                   _timeStamp;
    float                            _frameRate;
    bool                             _nackEnabled;
    bool                             _fecEnabled;
    bool                             _nackFecEnabled;
    WebRtc_UWord8                    _rttMS;
    float                            _bitRate;
    double                           _lossRate;
    WebRtc_UWord32                   _renderDelayMs;
    WebRtc_Word32                    _frameCnt;
    float                            _sumEncBytes;
    WebRtc_Word32                    _numFramesDropped;
    string                           _codecName;
    webrtc::VideoCodecType           _sendCodecType;
    WebRtc_Word32                    _numberOfCores;

    //for release test#2
    FILE*                            _fpinp;
    FILE*                            _fpout;
    FILE*                            _fpout2;
    int                              _testType;
    int                              _testNum;
    int                              _numParRuns;

}; // end of MediaOptTest class definition


// Feed back from the RTP Module callback
class RTPFeedbackCallback: public webrtc::RtpVideoFeedback
{
public:
    RTPFeedbackCallback(webrtc::VideoCodingModule* vcm) {_vcm = vcm;};
   void OnReceivedIntraFrameRequest(const WebRtc_Word32 id,
                                    const WebRtc_UWord8 message = 0){};

   void OnNetworkChanged(const WebRtc_Word32 id,
                          const WebRtc_UWord16 bitrateTargetKbit,
                          const WebRtc_UWord8 fractionLost,
                          const WebRtc_UWord16 roundTripTimeMs,
                          const WebRtc_UWord32 jitterMS,
                          const WebRtc_UWord16 bwEstimateKbitMin,
                          const WebRtc_UWord16 bwEstimateKbitMax);

private:
   webrtc::VideoCodingModule* _vcm;

};
#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_MEDIA_OPT_TEST_H_
