/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include "video_coding.h"
#include "rtp_rtcp.h"
#include "trace.h"
#include "module_common_types.h"
#include "tick_time.h"
#include "test_macros.h"
#include "test_util.h"

#include <string.h>
#include <fstream>
#include <cstdlib>

enum { kMaxWaitEncTimeMs = 100 };

// Class used for passing command line arguments to tests
class CmdArgs
{
public:
    CmdArgs() : codecName(""), codecType(webrtc::kVideoCodecVP8), width(-1),
            height(-1), bitRate(-1), frameRate(-1),
            inputFile(""), outputFile(""), testNum(-1)
    {}
    std::string codecName;
    webrtc::VideoCodecType codecType;
    int width;
    int height;
    int bitRate;
    int frameRate;
    std::string inputFile;
    std::string outputFile;
    int testNum;
};

// forward declaration
int MTRxTxTest(CmdArgs& args);
namespace webrtc
{
    class RtpDump;
}

// definition of general test function to be used by VCM tester (mainly send side)
/*
 Includes the following:
 1. General Callback definition for VCM test functions - no RTP.
 2. EncodeComplete callback:
 2a. Transfer encoded data directly to the decoder
 2b. Pass encoded data via the RTP module
 3. Caluclate PSNR from file function (for now: does not deal with frame drops)
 */

//Send Side - Packetization callback - send an encoded frame directly to the VCMReceiver
class VCMEncodeCompleteCallback: public webrtc::VCMPacketizationCallback
{
public:
    // constructor input: file in which encoded data will be written, and test parameters
    VCMEncodeCompleteCallback(FILE* encodedFile);
    virtual ~VCMEncodeCompleteCallback();
    // Register transport callback
    void RegisterTransportCallback(webrtc::VCMPacketizationCallback* transport);
    // process encoded data received from the encoder, pass stream to the VCMReceiver module
    WebRtc_Word32 SendData(const webrtc::FrameType frameType,
            const WebRtc_UWord8 payloadType, const WebRtc_UWord32 timeStamp,
            const WebRtc_UWord8* payloadData, const WebRtc_UWord32 payloadSize,
            const webrtc::RTPFragmentationHeader& fragmentationHeader,
            const webrtc::RTPVideoTypeHeader* videoTypeHdr);
    // Register exisitng VCM. Currently - encode and decode with the same vcm module.
    void RegisterReceiverVCM(webrtc::VideoCodingModule *vcm) { _VCMReceiver = vcm; }
    // Return size of last encoded frame encoded data (all frames in the sequence)
    // Good for only one call - after which will reset value (to allow detection of frame drop)
    float EncodedBytes();
    // return encode complete (true/false)
    bool EncodeComplete();
    // Inform callback of codec used
    void SetCodecType(webrtc::RTPVideoCodecTypes codecType) { _codecType = codecType; }
    // inform callback of frame dimensions
    void SetFrameDimensions(WebRtc_Word32 width, WebRtc_Word32 height)
    {
        _width = width;
        _height = height;
    }
    //Initialize callback data
    void Initialize();
    void ResetByteCount();

    // conversion function for payload type (needed for the callback function)
    //    RTPVideoVideoCodecTypes ConvertPayloadType(WebRtc_UWord8 payloadType);

private:
    FILE* _encodedFile;
    float _encodedBytes;
    webrtc::VideoCodingModule* _VCMReceiver;
    webrtc::FrameType _frameType;
    WebRtc_UWord8* _payloadData;
    WebRtc_UWord8 _seqNo;
    bool _encodeComplete;
    WebRtc_Word32 _width;
    WebRtc_Word32 _height;
    webrtc::RTPVideoCodecTypes _codecType;
    WebRtc_UWord8 _layerPacketId;

}; // end of VCMEncodeCompleteCallback

//Send Side - Packetization callback - packetize an encoded frame via the RTP module
class VCMRTPEncodeCompleteCallback: public webrtc::VCMPacketizationCallback
{
public:
    VCMRTPEncodeCompleteCallback(webrtc::RtpRtcp* rtp) :
        _seqNo(0), _encodedBytes(0), _RTPModule(rtp), _encodeComplete(false) {}
    virtual ~VCMRTPEncodeCompleteCallback() {}
    // process encoded data received from the encoder, pass stream to the RTP module
    WebRtc_Word32 SendData(const webrtc::FrameType frameType,
            const WebRtc_UWord8 payloadType, const WebRtc_UWord32 timeStamp,
            const WebRtc_UWord8* payloadData, const WebRtc_UWord32 payloadSize,
            const webrtc::RTPFragmentationHeader& fragmentationHeader,
            const webrtc::RTPVideoTypeHeader* videoTypeHdr);
    // Return size of last encoded frame. Value good for one call
    // (resets to zero after call to inform test of frame drop)
    float EncodedBytes();
    // return encode complete (true/false)
    bool EncodeComplete();
    // Inform callback of codec used
    void SetCodecType(webrtc::RTPVideoCodecTypes codecType) { _codecType = codecType; }

    // inform callback of frame dimensions
    void SetFrameDimensions(WebRtc_Word16 width, WebRtc_Word16 height)
    {
        _width = width;
        _height = height;
    }

private:
    float _encodedBytes;
    webrtc::FrameType _frameType;
    WebRtc_UWord8* _payloadData;
    WebRtc_UWord16 _seqNo;
    bool _encodeComplete;
    webrtc::RtpRtcp* _RTPModule;
    WebRtc_Word16 _width;
    WebRtc_Word16 _height;
    webrtc::RTPVideoCodecTypes _codecType;
}; // end of VCMEncodeCompleteCallback

class VCMDecodeCompleteCallback: public webrtc::VCMReceiveCallback
{
public:
    VCMDecodeCompleteCallback(FILE* decodedFile) :
        _decodedFile(decodedFile), _decodedBytes(0) {}
    virtual ~VCMDecodeCompleteCallback() {}
    // will write decoded frame into file
    WebRtc_Word32 FrameToRender(webrtc::VideoFrame& videoFrame);
    WebRtc_Word32 DecodedBytes();
    int PSNRLastFrame(const webrtc::VideoFrame& sourceFrame, double *YPSNRptr);
private:
    FILE*               _decodedFile;
    WebRtc_UWord32      _decodedBytes;
    webrtc::VideoFrame  _lastDecodedFrame;
}; // end of VCMDecodeCompleCallback class

///
class RTPSendCompleteCallback: public webrtc::Transport
{
public:
    // constructor input: (reeive side) rtp module to send encoded data to
            RTPSendCompleteCallback(webrtc::RtpRtcp* rtp,
                    const char* filename = NULL);
    virtual ~RTPSendCompleteCallback();
    // Send Packet to receive side RTP module
    virtual int SendPacket(int channel, const void *data, int len);
    // Send RTCP Packet to receive side RTP module
    virtual int SendRTCPPacket(int channel, const void *data, int len);
    // Set percentage of channel loss in the network
    void SetLossPct(double lossPct);
    // return send count
    int SendCount() { return _sendCount; }
private:
    // randomly decide weather to drop a packet or not, based on the channel model
    bool PacketLoss(double lossPct);

    WebRtc_UWord32    _sendCount;
    webrtc::RtpRtcp*  _rtp;
    double            _lossPct;
    webrtc::RtpDump*  _rtpDump;
};

// used in multi thread test
class SendSharedState
{
public:
    SendSharedState(webrtc::VideoCodingModule& vcm, webrtc::RtpRtcp& rtp,
            CmdArgs args) :
        _rtp(rtp), _vcm(vcm), _args(args), _sourceFile(NULL), _frameCnt(0),
                _timestamp(0) {}

    webrtc::VideoCodingModule&  _vcm;
    webrtc::RtpRtcp&          _rtp;
    CmdArgs                     _args;
    FILE*                       _sourceFile;
    WebRtc_Word32               _frameCnt;
    WebRtc_Word32               _timestamp;
};

class PacketRequester: public webrtc::VCMPacketRequestCallback
{
public:
    PacketRequester(webrtc::RtpRtcp& rtp) :
        _rtp(rtp) {}
    WebRtc_Word32 ResendPackets(const WebRtc_UWord16* sequenceNumbers,
            WebRtc_UWord16 length);

private:
    webrtc::RtpRtcp& _rtp;
};

// PSNR & SSIM calculations
WebRtc_Word32
PSNRfromFiles(const WebRtc_Word8 *refFileName,
        const WebRtc_Word8 *testFileName, WebRtc_Word32 width,
        WebRtc_Word32 height, double *YPSNRptr);

WebRtc_Word32
SSIMfromFiles(const WebRtc_Word8 *refFileName,
        const WebRtc_Word8 *testFileName, WebRtc_Word32 width,
        WebRtc_Word32 height, double *SSIMptr);

// codec type conversion
webrtc::RTPVideoCodecTypes
ConvertCodecType(const char* plname);

class SendStatsTest: public webrtc::VCMSendStatisticsCallback
{
public:
    SendStatsTest() : _frameRate(15) {}
    WebRtc_Word32 SendStatistics(const WebRtc_UWord32 bitRate,
            const WebRtc_UWord32 frameRate);
    void SetTargetFrameRate(WebRtc_UWord32 frameRate) { _frameRate = frameRate; }
private:
    WebRtc_UWord32 _frameRate;
};

class KeyFrameReqTest: public webrtc::VCMFrameTypeCallback
{
public:
    WebRtc_Word32 FrameTypeRequest(const webrtc::FrameType frameType);
};

#endif
