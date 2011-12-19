/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test_callbacks.h"

#include <cmath>

#include "rtp_dump.h"
#include "test_macros.h"

namespace webrtc {

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
        const RTPVideoHeader* videoHdr)
{
    // will call the VCMReceiver input packet
    _frameType = frameType;
    // writing encodedData into file
    fwrite(payloadData, 1, payloadSize, _encodedFile);
    WebRtcRTPHeader rtpInfo;
    rtpInfo.header.markerBit = true; // end of frame
    rtpInfo.type.Video.isFirstPacket = true;
    rtpInfo.type.Video.codec = _codecType;
    rtpInfo.type.Video.height = (WebRtc_UWord16)_height;
    rtpInfo.type.Video.width = (WebRtc_UWord16)_width;
    switch (_codecType)
    {
    case webrtc::kRTPVideoH263:
        rtpInfo.type.Video.codecHeader.H263.bits = false;
        rtpInfo.type.Video.codecHeader.H263.independentlyDecodable = false;
        break;
    case webrtc::kRTPVideoVP8:
        rtpInfo.type.Video.codecHeader.VP8.InitRTPVideoHeaderVP8();
        rtpInfo.type.Video.codecHeader.VP8.nonReference =
            videoHdr->codecHeader.VP8.nonReference;
        rtpInfo.type.Video.codecHeader.VP8.pictureId =
            videoHdr->codecHeader.VP8.pictureId;
        break;
    case webrtc::kRTPVideoI420:
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
        const RTPVideoHeader* videoHdr)
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
                                        videoHdr);
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

    if (_rtpDump != NULL)
    {
        if (_rtpDump->DumpPacket((const WebRtc_UWord8*)data, len) != 0)
        {
            return -1;
        }
    }

    bool transmitPacket = true;
    transmitPacket = PacketLoss();

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
    // Incorporate network conditions
    return SendPacket(channel, data, len);
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
RTPSendCompleteCallback::PacketLoss()
{
    bool transmitPacket = true;
    if (_burstLength <= 1.0)
    {
        // Random loss: if _burstLength parameter is not set, or <=1
        if (UnifomLoss(_lossPct))
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
            if (UnifomLoss(probTrans01))
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
            if (UnifomLoss(probTrans11))
            {
                // drop, update previous state to loss
                _prevLossState = 1;
                transmitPacket = false;
             }
        }
    }
    return transmitPacket;
}


bool
RTPSendCompleteCallback::UnifomLoss(double lossPct)
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


VideoProtectionCallback::VideoProtectionCallback():
_deltaFECRate(0),
_keyFECRate(0),
_deltaUseUepProtection(0),
_keyUseUepProtection(0),
_nack(kNackOff)
{
    //
}

VideoProtectionCallback::~VideoProtectionCallback()
{
    //
}

WebRtc_Word32
VideoProtectionCallback::ProtectionRequest(WebRtc_UWord8 deltaFECRate,
                                           WebRtc_UWord8 keyFECRate,
                                           bool deltaUseUepProtection,
                                           bool keyUseUepProtection,
                                           bool nack_enabled,
                                           WebRtc_UWord32* sent_video_rate_bps,
                                           WebRtc_UWord32* sent_nack_rate_bps,
                                           WebRtc_UWord32* sent_fec_rate_bps)
{
    _deltaFECRate = deltaFECRate;
    _keyFECRate = keyFECRate;
    _deltaUseUepProtection = deltaUseUepProtection;
    _keyUseUepProtection = keyUseUepProtection;
    if (nack_enabled)
    {
        _nack = kNackRtcp;
    }
    else
    {
        _nack = kNackOff;
    }

    // Update RTP
    if (_rtp->SetFECCodeRate(keyFECRate, deltaFECRate) != 0)
    {
        printf("Error in Setting FEC rate\n");
        return -1;

    }
    if (_rtp->SetFECUepProtection(keyUseUepProtection,
                                  deltaUseUepProtection) != 0)
    {
        printf("Error in Setting FEC UEP protection\n");
        return -1;
    }
    return 0;

}
NACKMethod
VideoProtectionCallback::NACKMethod()
{
    return _nack;
}

WebRtc_UWord8
VideoProtectionCallback::FECDeltaRate()
{
    return _deltaFECRate;
}

WebRtc_UWord8
VideoProtectionCallback::FECKeyRate()
{
    return _keyFECRate;
}

bool
VideoProtectionCallback::FECDeltaUepProtection()
{
    return _deltaUseUepProtection;
}

bool
VideoProtectionCallback::FECKeyUepProtection()
{
    return _keyUseUepProtection;
}

void
RTPFeedbackCallback::OnNetworkChanged(const WebRtc_Word32 id,
                                      const WebRtc_UWord32 bitrateBps,
                                      const WebRtc_UWord8 fractionLost,
                                      const WebRtc_UWord16 roundTripTimeMs)
{

    _vcm->SetChannelParameters(bitrateBps / 1000, fractionLost,
                               (WebRtc_UWord8)roundTripTimeMs);
}

}  // namespace webrtc
