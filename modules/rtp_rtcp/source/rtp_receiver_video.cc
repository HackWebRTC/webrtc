/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert> //assert
#include <cstring>  // memcpy()
#include <math.h>

#include "rtp_receiver_video.h"

#include "trace.h"
#include "critical_section_wrapper.h"
#include "tick_util.h"

#include "receiver_fec.h"

namespace webrtc {
WebRtc_UWord32 BitRateBPS(WebRtc_UWord16 x )
{
    return (x & 0x3fff) * WebRtc_UWord32(pow(10.0f,(2 + (x >> 14))));
}

RTPReceiverVideo::RTPReceiverVideo(const WebRtc_Word32 id,
                                   ModuleRtpRtcpPrivate& callback):
    _id(id),
    _criticalSectionFeedback(*CriticalSectionWrapper::CreateCriticalSection()),
    _cbVideoFeedback(NULL),
    _cbPrivateFeedback(callback),
    _criticalSectionReceiverVideo(*CriticalSectionWrapper::CreateCriticalSection()),

    _completeFrame(false),
    _receiveFEC(NULL),
    _packetStartTimeMs(0),
    _receivedBW(),
    _estimatedBW(0),
    _currentFecFrameDecoded(false),
    _h263InverseLogic(false),
    _overUseDetector(),
    _videoBitRate(),
    _lastBitRateChange(0),
    _packetOverHead(28)
{
    memset(_receivedBW, 0,sizeof(_receivedBW));
}

RTPReceiverVideo::~RTPReceiverVideo()
{
    delete &_criticalSectionFeedback;
    delete &_criticalSectionReceiverVideo;
    delete _receiveFEC;
}

WebRtc_Word32
RTPReceiverVideo::Init()
{
    _completeFrame = false;
    _packetStartTimeMs = 0;
    _estimatedBW = 0;
    _currentFecFrameDecoded = false;
    _packetOverHead = 28;
    for (int i = 0; i < BW_HISTORY_SIZE; i++)
    {
        _receivedBW[i] = 0;
    }
    ResetOverUseDetector();
    return 0;
}

void
RTPReceiverVideo::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
}

WebRtc_Word32
RTPReceiverVideo::RegisterIncomingVideoCallback(RtpVideoFeedback* incomingMessagesCallback)
{
    CriticalSectionScoped lock(_criticalSectionFeedback);
    _cbVideoFeedback = incomingMessagesCallback;
    return 0;
}

void
RTPReceiverVideo::UpdateBandwidthManagement(const WebRtc_UWord32 minBitrateBps,
                                            const WebRtc_UWord32 maxBitrateBps,
                                            const WebRtc_UWord8 fractionLost,
                                            const WebRtc_UWord16 roundTripTimeMs,
                                            const WebRtc_UWord16 bwEstimateKbitMin,
                                            const WebRtc_UWord16 bwEstimateKbitMax)
{
    CriticalSectionScoped lock(_criticalSectionFeedback);
    if(_cbVideoFeedback)
    {
        _cbVideoFeedback->OnNetworkChanged(_id, minBitrateBps, maxBitrateBps, fractionLost, roundTripTimeMs, bwEstimateKbitMin, bwEstimateKbitMax);
    }
}

ModuleRTPUtility::Payload*
RTPReceiverVideo::RegisterReceiveVideoPayload(const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                              const WebRtc_Word8 payloadType,
                                              const WebRtc_UWord32 maxRate)
{
    RtpVideoCodecTypes videoType = kRtpNoVideo;
    if (ModuleRTPUtility::StringCompare(payloadName, "VP8",3))
    {
        videoType = kRtpVp8Video;

    } else if ((ModuleRTPUtility::StringCompare(payloadName, "H263-1998", 9)) ||
               (ModuleRTPUtility::StringCompare(payloadName, "H263-2000", 9)))
    {
        videoType = kRtpH2631998Video;

    } else if (ModuleRTPUtility::StringCompare(payloadName, "H263", 4))
    {
        videoType = kRtpH263Video;

    } else if (ModuleRTPUtility::StringCompare(payloadName, "MP4V-ES", 7))
    {
        videoType = kRtpMpeg4Video;

    } else if (ModuleRTPUtility::StringCompare(payloadName, "I420", 4))
    {
        videoType = kRtpNoVideo;

    } else if (ModuleRTPUtility::StringCompare(payloadName, "ULPFEC", 6))
    {
        // store this
        if(_receiveFEC == NULL)
        {
            _receiveFEC = new ReceiverFEC(_id, this);
        }
        _receiveFEC->SetPayloadTypeFEC(payloadType);
        videoType = kRtpFecVideo;
    }else
    {
        return NULL;
    }

    ModuleRTPUtility::Payload* payload =  new ModuleRTPUtility::Payload;
    memcpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE);
    payload->typeSpecific.Video.videoCodecType = videoType;
    payload->typeSpecific.Video.maxRate = maxRate;
    payload->audio = false;
    return payload;
}

void RTPReceiverVideo::ResetOverUseDetector()
{
    _overUseDetector.Reset();
    _videoBitRate.Init();
    _lastBitRateChange = 0;
}

// called under _criticalSectionReceiverVideo
WebRtc_UWord16
RTPReceiverVideo::EstimateBandwidth(const WebRtc_UWord16 bandwidth)
{
    // received fragments
    // estimate BW

    WebRtc_UWord16 bwSort[BW_HISTORY_SIZE];
    for(int i = 0; i < BW_HISTORY_SIZE-1; i++)
    {
        _receivedBW[i] = _receivedBW[i+1];
        bwSort[i] = _receivedBW[i+1];
    }
    _receivedBW[BW_HISTORY_SIZE-1] = bandwidth;
    bwSort[BW_HISTORY_SIZE-1] = bandwidth;

    WebRtc_UWord16 temp;
    for (int i = BW_HISTORY_SIZE-1; i >= 0; i--)
    {
        for (int j = 1; j <= i; j++)
        {
            if (bwSort[j-1] > bwSort[j])
            {
                temp = bwSort[j-1];
                bwSort[j-1] = bwSort[j];
                bwSort[j] = temp;
            }
        }
    }
    int zeroCount = 0;
    for (; zeroCount < BW_HISTORY_SIZE; zeroCount++)
    {
        if (bwSort[zeroCount]!= 0)
        {
            break;
        }
    }
    WebRtc_UWord32 indexMedian = (BW_HISTORY_SIZE -1) - (BW_HISTORY_SIZE-zeroCount)/2;
    WebRtc_UWord16 bandwidthMedian = bwSort[indexMedian];

    if (bandwidthMedian > 0)
    {
        if (_estimatedBW == bandwidth)
        {
            // don't trigger a callback
            bandwidthMedian = 0;
        } else
        {
            _estimatedBW = bandwidthMedian;
        }
    } else
    {
        // can't be negative
        bandwidthMedian = 0;
    }

    return bandwidthMedian;
}

// we have no critext when calling this
// we are not allowed to have any critsects when calling CallbackOfReceivedPayloadData
WebRtc_Word32
RTPReceiverVideo::ParseVideoCodecSpecific(WebRtcRTPHeader* rtpHeader,
                                          const WebRtc_UWord8* payloadData,
                                          const WebRtc_UWord16 payloadDataLength,
                                          const RtpVideoCodecTypes videoType,
                                          const bool isRED,
                                          const WebRtc_UWord8* incomingRtpPacket,
                                          const WebRtc_UWord16 incomingRtpPacketSize)
{
    WebRtc_Word32 retVal = 0;

    _criticalSectionReceiverVideo.Enter();

    _videoBitRate.Update(payloadDataLength, TickTime::MillisecondTimestamp());

    // Add headers, ideally we would like to include for instance
    // Ethernet header here as well.
    const WebRtc_UWord16 packetSize = payloadDataLength + _packetOverHead +
        rtpHeader->header.headerLength + rtpHeader->header.paddingLength;
    _overUseDetector.Update(*rtpHeader, packetSize);

    if (isRED)
    {
        if(_receiveFEC == NULL)
        {
            _criticalSectionReceiverVideo.Leave();
            return -1;
        }
        if (rtpHeader->header.timestamp != TimeStamp())
        {
            // We have a new frame. Force a decode with the existing packets.
            retVal = _receiveFEC->ProcessReceivedFEC(true);
            _currentFecFrameDecoded = false;
        }

        bool FECpacket = false;
        if(retVal != -1)
        {
            if (!_currentFecFrameDecoded)
            {
                retVal = _receiveFEC->AddReceivedFECPacket(rtpHeader, incomingRtpPacket, payloadDataLength, FECpacket);

                if (retVal != -1 && (FECpacket || rtpHeader->header.markerBit))
                {
                    // Only attempt a decode after receiving the last media packet.
                    retVal = _receiveFEC->ProcessReceivedFEC(false);
                }
            }else
            {
                _receiveFEC->AddReceivedFECInfo(rtpHeader,incomingRtpPacket, FECpacket);
            }
        }
        _criticalSectionReceiverVideo.Leave();

        if(retVal == 0 && FECpacket )
        {
            // callback with the received FEC packet, the normal packets are deliverd after parsing
            // this contain the original RTP packet header but with empty payload and data length
            rtpHeader->frameType = kFrameEmpty;
            WebRtc_Word32 retVal = SetCodecType(videoType, rtpHeader);       //we need this for the routing
            if(retVal != 0)
            {
                return retVal;
            }
            retVal =CallbackOfReceivedPayloadData(NULL,
                                                  0,
                                                  rtpHeader);
        }
    }else
    {
        // will leave the _criticalSectionReceiverVideo critsect
        retVal = ParseVideoCodecSpecificSwitch(rtpHeader,
                                               payloadData,
                                               payloadDataLength,
                                               videoType);
    }

    // Update the remote rate control object and update the overuse
    // detector with the current rate control region.
    _criticalSectionReceiverVideo.Enter();
    const RateControlInput input(_overUseDetector.State(), _videoBitRate.BitRateNow(), _overUseDetector.NoiseVar());
    _criticalSectionReceiverVideo.Leave();

    // Call the callback outside critical section
    const RateControlRegion region = _cbPrivateFeedback.OnOverUseStateUpdate(input);

    _criticalSectionReceiverVideo.Enter();
    _overUseDetector.SetRateControlRegion(region);
    _criticalSectionReceiverVideo.Leave();

    return retVal;
}

WebRtc_Word32
RTPReceiverVideo::BuildRTPheader(const WebRtcRTPHeader* rtpHeader,
                                 WebRtc_UWord8* dataBuffer) const
{
    dataBuffer[0] = static_cast<WebRtc_UWord8>(0x80);            // version 2
    dataBuffer[1] = static_cast<WebRtc_UWord8>(rtpHeader->header.payloadType);
    if (rtpHeader->header.markerBit)
    {
        dataBuffer[1] |= kRtpMarkerBitMask;  // MarkerBit is 1
    }

    ModuleRTPUtility::AssignUWord16ToBuffer(dataBuffer+2, rtpHeader->header.sequenceNumber);
    ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer+4, rtpHeader->header.timestamp);
    ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer+8, rtpHeader->header.ssrc);

    WebRtc_Word32 rtpHeaderLength = 12;

    // Add the CSRCs if any
    if (rtpHeader->header.numCSRCs > 0)
    {
        if(rtpHeader->header.numCSRCs > 16)
        {
            // error
            assert(false);
        }
        WebRtc_UWord8* ptr = &dataBuffer[rtpHeaderLength];
        for (WebRtc_UWord32 i = 0; i < rtpHeader->header.numCSRCs; ++i)
        {
            ModuleRTPUtility::AssignUWord32ToBuffer(ptr, rtpHeader->header.arrOfCSRCs[i]);
            ptr +=4;
        }
        dataBuffer[0] = (dataBuffer[0]&0xf0) | rtpHeader->header.numCSRCs;

        // Update length of header
        rtpHeaderLength += sizeof(WebRtc_UWord32)*rtpHeader->header.numCSRCs;
    }
    return rtpHeaderLength;
}

WebRtc_Word32
RTPReceiverVideo::ReceiveRecoveredPacketCallback(WebRtcRTPHeader* rtpHeader,
                                                 const WebRtc_UWord8* payloadData,
                                                 const WebRtc_UWord16 payloadDataLength)
{
     _criticalSectionReceiverVideo.Enter();

    _currentFecFrameDecoded = true;

    ModuleRTPUtility::Payload* payload = NULL;
    if (PayloadTypeToPayload(rtpHeader->header.payloadType, payload) != 0)
    {
        return -1;
    }
    // here we can re-create the original lost packet so that we can use it for the relay
    // we need to re-create the RED header too
    WebRtc_UWord8 recoveredPacket[IP_PACKET_SIZE];
    WebRtc_UWord16 rtpHeaderLength = (WebRtc_UWord16)BuildRTPheader(rtpHeader, recoveredPacket);

    const WebRtc_UWord8 REDForFECHeaderLength = 1;

    // replace pltype
    recoveredPacket[1] &= 0x80;             // reset
    recoveredPacket[1] += REDPayloadType(); // replace with RED payload type

    // add RED header
    recoveredPacket[rtpHeaderLength] = rtpHeader->header.payloadType; // f-bit always 0

    memcpy(recoveredPacket + rtpHeaderLength + REDForFECHeaderLength, payloadData, payloadDataLength);

    return ParseVideoCodecSpecificSwitch(rtpHeader,
                                         payloadData,
                                         payloadDataLength,
                                         payload->typeSpecific.Video.videoCodecType);
}

WebRtc_Word32
RTPReceiverVideo::SetCodecType(const RtpVideoCodecTypes videoType,
                               WebRtcRTPHeader* rtpHeader) const
{
    switch (videoType)
    {
    case kRtpNoVideo:
        rtpHeader->type.Video.codec = kRTPVideoGeneric;
        break;
    case kRtpVp8Video:
        rtpHeader->type.Video.codec = kRTPVideoVP8;
        break;
    case kRtpH263Video:
        rtpHeader->type.Video.codec = kRTPVideoH263;
        break;
    case kRtpH2631998Video:
        rtpHeader->type.Video.codec = kRTPVideoH263;
        break;
    case kRtpMpeg4Video:
        rtpHeader->type.Video.codec = kRTPVideoMPEG4;
        break;
    case kRtpFecVideo:
        rtpHeader->type.Video.codec = kRTPVideoFEC;
        break;
    default:
        assert(((void)"ParseCodecSpecific videoType can not be unknown here!", false));
        return -1;
    }
    return 0;
}


WebRtc_Word32
RTPReceiverVideo::ParseVideoCodecSpecificSwitch(WebRtcRTPHeader* rtpHeader,
                                                const WebRtc_UWord8* payloadData,
                                                const WebRtc_UWord16 payloadDataLength,
                                                const RtpVideoCodecTypes videoType)
{
    WebRtc_Word32 retVal = SetCodecType(videoType, rtpHeader);
    if(retVal != 0)
    {
        return retVal;
    }

    // all receive functions release _criticalSectionReceiverVideo before returning
    switch (videoType)
    {
    case kRtpNoVideo:
        retVal = ReceiveGenericCodec(rtpHeader, payloadData, payloadDataLength);
        break;
    case kRtpVp8Video:
        retVal = ReceiveVp8Codec(rtpHeader, payloadData, payloadDataLength);
        break;
    case kRtpH263Video:
        retVal = ReceiveH263Codec(rtpHeader, payloadData, payloadDataLength);
        break;
    case kRtpH2631998Video:
        retVal = ReceiveH2631998Codec(rtpHeader,payloadData, payloadDataLength);
        break;
    case kRtpMpeg4Video:
        retVal = ReceiveMPEG4Codec(rtpHeader,payloadData, payloadDataLength);
        break;
    default:
        _criticalSectionReceiverVideo.Leave();
        assert(((void)"ParseCodecSpecific videoType can not be unknown here!", false));
        return -1;
    }
    return retVal;
}

WebRtc_Word32
RTPReceiverVideo::ReceiveH263Codec(WebRtcRTPHeader* rtpHeader,
                                   const WebRtc_UWord8* payloadData,
                                   const WebRtc_UWord16 payloadDataLength)
{
    ModuleRTPUtility::RTPPayloadParser rtpPayloadParser(kRtpH263Video,
                                                        payloadData,
                                                        payloadDataLength);
    ModuleRTPUtility::RTPPayload parsedPacket;
    const bool success = rtpPayloadParser.Parse(parsedPacket);

    // from here down we only work on local data
    _criticalSectionReceiverVideo.Leave();

    if (!success)
    {
        return -1;
    }
    if (IP_PACKET_SIZE < parsedPacket.info.H263.dataLength + parsedPacket.info.H263.insert2byteStartCode? 2:0)
    {
        return -1;
    }
    return ReceiveH263CodecCommon(parsedPacket, rtpHeader);
}

WebRtc_Word32
RTPReceiverVideo::ReceiveH2631998Codec(WebRtcRTPHeader* rtpHeader,
                                       const WebRtc_UWord8* payloadData,
                                       const WebRtc_UWord16 payloadDataLength)
{
    ModuleRTPUtility::RTPPayloadParser rtpPayloadParser(kRtpH2631998Video,
                                                        payloadData,
                                                        payloadDataLength);

    ModuleRTPUtility::RTPPayload parsedPacket;
    const bool success = rtpPayloadParser.Parse(parsedPacket);
    if (!success)
    {
        _criticalSectionReceiverVideo.Leave();
        return -1;
    }
    if (IP_PACKET_SIZE < parsedPacket.info.H263.dataLength + parsedPacket.info.H263.insert2byteStartCode? 2:0)
    {
        _criticalSectionReceiverVideo.Leave();
        return -1;
    }
    // from here down we only work on local data
    _criticalSectionReceiverVideo.Leave();

    return ReceiveH263CodecCommon(parsedPacket, rtpHeader);
}

WebRtc_Word32
RTPReceiverVideo::ReceiveH263CodecCommon(ModuleRTPUtility::RTPPayload& parsedPacket,
                                         WebRtcRTPHeader* rtpHeader)
{
    rtpHeader->frameType = (parsedPacket.frameType == ModuleRTPUtility::kIFrame) ? kVideoFrameKey : kVideoFrameDelta;
    if (_h263InverseLogic) // Microsoft H263 bug
    {
        if (rtpHeader->frameType == kVideoFrameKey)
            rtpHeader->frameType = kVideoFrameDelta;
        else
            rtpHeader->frameType = kVideoFrameKey;
    }
    rtpHeader->type.Video.isFirstPacket = parsedPacket.info.H263.hasPictureStartCode;

    // if p == 0
    // it's a follow-on packet, hence it's not independently decodable
    rtpHeader->type.Video.codecHeader.H263.independentlyDecodable = parsedPacket.info.H263.hasPbit;

    if (parsedPacket.info.H263.hasPictureStartCode)
    {
        rtpHeader->type.Video.width = parsedPacket.info.H263.frameWidth;
        rtpHeader->type.Video.height = parsedPacket.info.H263.frameHeight;
    } else
    {
        rtpHeader->type.Video.width = 0;
        rtpHeader->type.Video.height = 0;
    }
    rtpHeader->type.Video.codecHeader.H263.bits = (parsedPacket.info.H263.startBits > 0)?true:false;

    // copy to a local buffer
    WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE];
    WebRtc_UWord16 dataLength = 0;

    // we need to copy since we modify the first byte
    if(parsedPacket.info.H263.insert2byteStartCode)
    {
        dataBuffer[0] = 0;
        dataBuffer[1] = 0;
        memcpy(dataBuffer+2, parsedPacket.info.H263.data, parsedPacket.info.H263.dataLength);
        dataLength = 2 + parsedPacket.info.H263.dataLength;
    } else
    {
        memcpy(dataBuffer, parsedPacket.info.H263.data, parsedPacket.info.H263.dataLength);
        dataLength = parsedPacket.info.H263.dataLength;
    }

    if(parsedPacket.info.H263.dataLength > 0)
    {
        if(parsedPacket.info.H263.startBits > 0)
        {
            // make sure that the ignored start bits are zero
            dataBuffer[0] &= (0xff >> parsedPacket.info.H263.startBits);
        }
        if(parsedPacket.info.H263.endBits > 0)
        {
            // make sure that the ignored end bits are zero
            dataBuffer[parsedPacket.info.H263.dataLength -1] &= ((0xff << parsedPacket.info.H263.endBits) & 0xff);
        }
    }

    return CallbackOfReceivedPayloadData(dataBuffer, dataLength, rtpHeader);
}

WebRtc_Word32
RTPReceiverVideo::ReceiveMPEG4Codec(WebRtcRTPHeader* rtpHeader,
                                    const WebRtc_UWord8* payloadData,
                                    const WebRtc_UWord16 payloadDataLength)
{
    ModuleRTPUtility::RTPPayloadParser rtpPayloadParser(kRtpMpeg4Video,
                                                        payloadData,
                                                        payloadDataLength);

    ModuleRTPUtility::RTPPayload parsedPacket;
    const bool success = rtpPayloadParser.Parse(parsedPacket);
    if (!success)
    {
        _criticalSectionReceiverVideo.Leave();
        return -1;
    }
    // from here down we only work on local data
    _criticalSectionReceiverVideo.Leave();

    rtpHeader->frameType = (parsedPacket.frameType == ModuleRTPUtility::kIFrame) ? kVideoFrameKey : kVideoFrameDelta;
    rtpHeader->type.Video.isFirstPacket = parsedPacket.info.MPEG4.isFirstPacket;

    if(CallbackOfReceivedPayloadData(parsedPacket.info.MPEG4.data,
                                     parsedPacket.info.MPEG4.dataLength,
                                     rtpHeader) != 0)
                        {
                            return -1;
                        }
            return 0;
        }

WebRtc_Word32
RTPReceiverVideo::ReceiveVp8Codec(WebRtcRTPHeader* rtpHeader,
                                  const WebRtc_UWord8* payloadData,
                                  const WebRtc_UWord16 payloadDataLength)
{
    ModuleRTPUtility::RTPPayloadParser rtpPayloadParser(kRtpVp8Video,
                                                        payloadData,
                                                        payloadDataLength);

    ModuleRTPUtility::RTPPayload parsedPacket;
    const bool success = rtpPayloadParser.Parse(parsedPacket);

    // from here down we only work on local data
    _criticalSectionReceiverVideo.Leave();

    if (!success)
    {
        return -1;
    }
    if (parsedPacket.info.VP8.dataLength == 0)
    {
        // we have an "empty" VP8 packet, it's ok, could be one way video
        return 0;
    }
    rtpHeader->frameType = (parsedPacket.frameType == ModuleRTPUtility::kIFrame) ? kVideoFrameKey : kVideoFrameDelta;

    rtpHeader->type.Video.codecHeader.VP8.startBit = parsedPacket.info.VP8.startFragment;   // Start of partition
    rtpHeader->type.Video.codecHeader.VP8.stopBit= parsedPacket.info.VP8.stopFragment;    // Stop of partition

    rtpHeader->type.Video.isFirstPacket = parsedPacket.info.VP8.beginningOfFrame;

    if(CallbackOfReceivedPayloadData(parsedPacket.info.VP8.data,
                                     parsedPacket.info.VP8.dataLength,
                                     rtpHeader) != 0)
    {
        return -1;
    }
    return 0;
}


WebRtc_Word32
RTPReceiverVideo::ReceiveGenericCodec(WebRtcRTPHeader* rtpHeader,
                                      const WebRtc_UWord8* payloadData,
                                      const WebRtc_UWord16 payloadDataLength)
{
    rtpHeader->frameType = kVideoFrameKey;

    if(((SequenceNumber() + 1) == rtpHeader->header.sequenceNumber) &&
        (TimeStamp() != rtpHeader->header.timestamp))
    {
        rtpHeader->type.Video.isFirstPacket = true;
    }
    _criticalSectionReceiverVideo.Leave();

    if(CallbackOfReceivedPayloadData(payloadData, payloadDataLength, rtpHeader) != 0)
    {
        return -1;
    }
    return 0;
}

WebRtc_Word32 RTPReceiverVideo::SetH263InverseLogic(const bool enable)
{
    _h263InverseLogic = enable;
    return 0;
}

void RTPReceiverVideo::SetPacketOverHead(WebRtc_UWord16 packetOverHead)
{
    _packetOverHead = packetOverHead;
}
} // namespace webrtc
