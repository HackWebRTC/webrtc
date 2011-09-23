/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_TEST_UTIL_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_TEST_UTIL_H_

/*
 * General declarations used through out VCM offline tests.
 */

#include "module_common_types.h"

#include <string.h>
#include <fstream>
#include <cstdlib>


// Class used for passing command line arguments to tests
class CmdArgs
{
public:
  CmdArgs() : codecName(""), codecType(webrtc::kVideoCodecVP8), width(-1),
             height(-1), bitRate(-1), frameRate(-1), packetLoss(0), rtt(0),
             protectionMode(0), camaEnable(0), inputFile(""), outputFile(""),
             testNum(-1)
     {}
     std::string codecName;
     webrtc::VideoCodecType codecType;
     int width;
     int height;
     int bitRate;
     int frameRate;
     int packetLoss;
     int rtt;
     int protectionMode;
     int camaEnable;
     std::string inputFile;
     std::string outputFile;
     int testNum;
};

// forward declaration
int MTRxTxTest(CmdArgs& args);
double NormalDist(double mean, double stdDev);

typedef struct
{
        WebRtc_Word8 data[1650]; // max packet size
        WebRtc_Word32 length;
        WebRtc_Word64 receiveTime;
} rtpPacket;


// Codec type conversion
webrtc::RTPVideoCodecTypes
ConvertCodecType(const char* plname);

#endif
