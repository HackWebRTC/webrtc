/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test_util.h"
#include "test_macros.h"
#include "rtp_dump.h"
#include <cmath>

using namespace webrtc;

// Normal Distribution
#define PI  3.14159265
double
NormalDist(double mean, double stdDev)
{
    // Creating a Normal distribution variable from two independent uniform
    // variables based on the Box-Muller transform
    double uniform1 = (std::rand() + 1.0) / (RAND_MAX + 1.0);
    double uniform2 = (std::rand() + 1.0) / (RAND_MAX + 1.0);
    return (mean + stdDev * sqrt(-2 * log(uniform1)) * cos(2 * PI * uniform2));
}


/******************************
 *  VCMEncodeCompleteCallback
 *****************************/
// Basic callback implementation
// passes the encoded frame directly to the encoder
// Packetization callback implementation
VCMEncodeCompleteCallback::VCMEncodeCompleteCallback(FILE* encodedFile):
    _encodedFile(encodedFile),
    _encodedBytes(0),
    _VCMReceiver(NULL),
    _seqNo(0),
    _encodeComplete(false),
    _width(0),
    _height(0),
    _codecType(kRTPVideoNoVideo)
{
    //
}
VCMEncodeCompleteCallback::~VCMEncodeCompleteCallback()
{
}

void
VCMEncodeCompleteCallback::RegisterTransportCallback(
                                            VCMPacketizationCallback* transport)
{
}

WebRtc_Word32
VCMEncodeCompleteCallback::SendData(
        const FrameType frameType,
        const WebRtc_UWord8  payloadType,
        const WebRtc_UWord32 timeStamp,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord32 payloadSize,
        const RTPFragmentationHeader& fragmentationHeader,
        const webrtc::RTPVideoTypeHeader* videoTypeHdr)
{
    // will call the VCMReceiver input packet
    _frameType = frameType;
    // writing encodedData into file
    fwrite(payloadData, 1, payloadSize, _encodedFile);
    WebRtcRTPHeader rtpInfo;
    rtpInfo.header.markerBit = true; // end of frame
    rtpInfo.type.Video.isFirstPacket = true;
    rtpInfo.type.Video.codec = _codecType;
    switch (_codecType)
    {
    case webrtc::kRTPVideoH263:
        rtpInfo.type.Video.codecHeader.H263.bits = false;
        rtpInfo.type.Video.codecHeader.H263.independentlyDecodable = false;
        rtpInfo.type.Video.height = (WebRtc_UWord16)_height;
        rtpInfo.type.Video.width = (WebRtc_UWord16)_width;
        break;
    case webrtc::kRTPVideoVP8:
        break;
    default:
        assert(false);
        return -1;
    }

    rtpInfo.header.payloadType = payloadType;
    rtpInfo.header.sequenceNumber = _seqNo++;
    rtpInfo.header.ssrc = 0;
    rtpInfo.header.timestamp = timeStamp;
    rtpInfo.frameType = frameType;
    // Size should also be received from that table, since the payload type
    // defines the size.

    _encodedBytes += payloadSize;
    // directly to receiver
    // TODO(hlundin): Remove assert once we've piped PictureID into VCM
    // through the WebRtcRTPHeader.
    assert(rtpInfo.type.Video.codec != kRTPVideoVP8);
    int ret = _VCMReceiver->IncomingPacket(payloadData, payloadSize, rtpInfo);
    _encodeComplete = true;

    return ret;
}

float
VCMEncodeCompleteCallback::EncodedBytes()
{
    return _encodedBytes;
}

bool
VCMEncodeCompleteCallback::EncodeComplete()
{
    if (_encodeComplete)
    {
        _encodeComplete = false;
        return true;
    }
    return false;
}

void
VCMEncodeCompleteCallback::Initialize()
{
    _encodeComplete = false;
    _encodedBytes = 0;
    _seqNo = 0;
    return;
}

void
VCMEncodeCompleteCallback::ResetByteCount()
{
    _encodedBytes = 0;
}

/***********************************/
/*   VCMRTPEncodeCompleteCallback  */
/***********************************/
// Encode Complete callback implementation
// passes the encoded frame via the RTP module to the decoder
// Packetization callback implementation

WebRtc_Word32
VCMRTPEncodeCompleteCallback::SendData(
        const FrameType frameType,
        const WebRtc_UWord8  payloadType,
        const WebRtc_UWord32 timeStamp,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord32 payloadSize,
        const RTPFragmentationHeader& fragmentationHeader,
        const webrtc::RTPVideoTypeHeader* videoTypeHdr)
{
    _frameType = frameType;
    _encodedBytes+= payloadSize;
    _encodeComplete = true;
    return _RTPModule->SendOutgoingData(frameType,
                                        payloadType,
                                        timeStamp,
                                        payloadData,
                                        payloadSize,
                                        &fragmentationHeader,
                                        videoTypeHdr);
}

float
VCMRTPEncodeCompleteCallback::EncodedBytes()
{
    // only good for one call  - after which will reset value;
    float tmp = _encodedBytes;
    _encodedBytes = 0;
    return tmp;
 }

bool
VCMRTPEncodeCompleteCallback::EncodeComplete()
{
    if (_encodeComplete)
    {
        _encodeComplete = false;
        return true;
    }
    return false;
}

// Decoded Frame Callback Implementation

WebRtc_Word32
VCMDecodeCompleteCallback::FrameToRender(VideoFrame& videoFrame)
{
    fwrite(videoFrame.Buffer(), 1, videoFrame.Length(), _decodedFile);
    _decodedBytes+= videoFrame.Length();
    return VCM_OK;
 }

WebRtc_Word32
VCMDecodeCompleteCallback::DecodedBytes()
{
    return _decodedBytes;
}

RTPSendCompleteCallback::RTPSendCompleteCallback(RtpRtcp* rtp,
                                                 const char* filename):
    _sendCount(0),
    _rtp(rtp),
    _lossPct(0),
    _burstLength(0),
    _networkDelayMs(0),
    _jitterVar(0),
    _prevLossState(0),
    _totalSentLength(0),
    _rtpPackets(),
    _rtpDump(NULL)
{
    if (filename != NULL)
    {
        _rtpDump = RtpDump::CreateRtpDump();
        _rtpDump->Start(filename);
    }
}

RTPSendCompleteCallback::~RTPSendCompleteCallback()
{
    if (_rtpDump != NULL)
    {
        _rtpDump->Stop();
        RtpDump::DestroyRtpDump(_rtpDump);
    }
    // Delete remaining packets
    while (!_rtpPackets.Empty())
    {
         // Take first packet in list
         delete static_cast<rtpPacket*>((_rtpPackets.First())->GetItem());
         _rtpPackets.PopFront();
    }
}

int
RTPSendCompleteCallback::SendPacket(int channel, const void *data, int len)
{
    _sendCount++;
    _totalSentLength += len;
    bool transmitPacket = true;

    // Packet Loss

    if (_burstLength <= 1.0)
    {
        // Random loss: if _burstLength parameter is not set, or <=1
        if (PacketLoss(_lossPct))
        {
            // drop
            transmitPacket = false;
        }
    }
    else
    {
        // Simulate bursty channel (Gilbert model)
        // (1st order) Markov chain model with memory of the previous/last
        // packet state (loss or received)

        // 0 = received state
        // 1 = loss state

        // probTrans10: if previous packet is lost, prob. to -> received state
        // probTrans11: if previous packet is lost, prob. to -> loss state

        // probTrans01: if previous packet is received, prob. to -> loss state
        // probTrans00: if previous packet is received, prob. to -> received

        // Map the two channel parameters (average loss rate and burst length)
        // to the transition probabilities:
        double probTrans10 = 100 * (1.0 / _burstLength);
        double probTrans11 = (100.0 - probTrans10);
        double probTrans01 = (probTrans10 * ( _lossPct / (100.0 - _lossPct)));

        // Note: Random loss (Bernoulli) model is a special case where:
        // burstLength = 100.0 / (100.0 - _lossPct) (i.e., p10 + p01 = 100)

        if (_prevLossState == 0 )
        {
            // previous packet was received
            if (PacketLoss(probTrans01))
            {
                // drop, update previous state to loss
                _prevLossState = 1;
                transmitPacket = false;
            }
        }
        else if (_prevLossState == 1)
        {
            _prevLossState = 0;
            // previous packet was lost
            if (PacketLoss(probTrans11))
            {
                // drop, update previous state to loss
                _prevLossState = 1;
                transmitPacket = false;
            }
        }
    }

    if (_rtpDump != NULL)
    {
        if (_rtpDump->DumpPacket((const WebRtc_UWord8*)data, len) != 0)
        {
            return -1;
        }
    }

    WebRtc_UWord64 now = VCMTickTime::MillisecondTimestamp();
    // Insert outgoing packet into list
    if (transmitPacket)
    {
        rtpPacket* newPacket = new rtpPacket();
        memcpy(newPacket->data, data, len);
        newPacket->length = len;
        // Simulate receive time = network delay + packet jitter
        // simulated as a Normal distribution random variable with
        // mean = networkDelay and variance = jitterVar
        WebRtc_Word32
        simulatedDelay = (WebRtc_Word32)NormalDist(_networkDelayMs,
                                                   sqrt(_jitterVar));
        newPacket->receiveTime = now + simulatedDelay;
        _rtpPackets.PushBack(newPacket);
    }

    // Are we ready to send packets to the receiver?
    rtpPacket* packet = NULL;

    while (!_rtpPackets.Empty())
    {
        // Take first packet in list
        packet = static_cast<rtpPacket*>((_rtpPackets.First())->GetItem());
        WebRtc_Word64 timeToReceive = packet->receiveTime - now;
        if (timeToReceive > 0)
        {
            // No available packets to send
            break;
        }

        _rtpPackets.PopFront();
        // Send to receive side
        if (_rtp->IncomingPacket((const WebRtc_UWord8*)packet->data,
                                     packet->length) < 0)
        {
            delete packet;
            packet = NULL;
            // Will return an error after the first packet that goes wrong
            return -1;
        }
        delete packet;
        packet = NULL;
    }
    return len; // OK
}

int
RTPSendCompleteCallback::SendRTCPPacket(int channel, const void *data, int len)
{
    if (_rtp->IncomingPacket((const WebRtc_UWord8*)data, len) == 0)
    {
        return len;
    }
    return -1;
}

void
RTPSendCompleteCallback::SetLossPct(double lossPct)
{
    _lossPct = lossPct;
    return;
}

void
RTPSendCompleteCallback::SetBurstLength(double burstLength)
{
    _burstLength = burstLength;
    return;
}

bool
RTPSendCompleteCallback::PacketLoss(double lossPct)
{
    double randVal = (std::rand() + 1.0)/(RAND_MAX + 1.0);
    return randVal < lossPct/100;
}

WebRtc_Word32
PacketRequester::ResendPackets(const WebRtc_UWord16* sequenceNumbers,
                               WebRtc_UWord16 length)
{
    return _rtp.SendNACK(sequenceNumbers, length);
}


RTPVideoCodecTypes
ConvertCodecType(const char* plname)
{
    if (strncmp(plname,"VP8" , 3) == 0)
    {
        return kRTPVideoVP8;
    }
    else if (strncmp(plname,"H263" , 5) == 0)
    {
        return kRTPVideoH263;
    }
    else if (strncmp(plname, "H263-1998",10) == 0)
    {
        return kRTPVideoH263;
    }
    else if (strncmp(plname,"I420" , 5) == 0)
    {
        return kRTPVideoI420;
    }
    else
    {
        return kRTPVideoNoVideo; // Default value
    }

}

WebRtc_Word32
SendStatsTest::SendStatistics(const WebRtc_UWord32 bitRate,
                              const WebRtc_UWord32 frameRate)
{
    TEST(frameRate <= _frameRate);
    TEST(bitRate > 0 && bitRate < 100000);
    printf("VCM 1 sec: Bit rate: %u\tFrame rate: %u\n", bitRate, frameRate);
    return 0;
}

WebRtc_Word32
KeyFrameReqTest::FrameTypeRequest(const FrameType frameType)
{
    TEST(frameType == kVideoFrameKey);
    if (frameType == kVideoFrameKey)
    {
        printf("Key frame requested\n");
    }
    else
    {
        printf("Non-key frame requested: %d\n", frameType);
    }
    return 0;
}
