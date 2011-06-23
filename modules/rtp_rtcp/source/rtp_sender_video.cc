/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtp_sender_video.h"

#include "critical_section_wrapper.h"
#include "trace.h"

#include "rtp_utility.h"

#include <string.h> // memcpy
#include <cassert>  // assert
#include <cstdlib>  // srand

#include "h263_information.h"
#include "rtp_format_vp8.h"

namespace webrtc {
enum { REDForFECHeaderLength = 1 };

RTPSenderVideo::RTPSenderVideo(const WebRtc_Word32 id, RTPSenderInterface* rtpSender) :
    _id(id),
    _rtpSender(*rtpSender),
    _sendVideoCritsect(*CriticalSectionWrapper::CreateCriticalSection()),

    _videoType(kRtpNoVideo),
    _videoCodecInformation(NULL),
    _maxBitrate(0),

    // generic FEC
    _fec(id),
    _fecEnabled(false),
    _payloadTypeRED(-1),
    _payloadTypeFEC(-1),
    _codeRateKey(0),
    _codeRateDelta(0),
    _fecProtectionFactor(0),
    _numberFirstPartition(0),

    // H263
    _savedByte(0),
    _eBit(0)
{
}

RTPSenderVideo::~RTPSenderVideo()
{
    if(_videoCodecInformation)
    {
        delete _videoCodecInformation;
    }
    delete &_sendVideoCritsect;
}

WebRtc_Word32
RTPSenderVideo::Init()
{
    CriticalSectionScoped cs(_sendVideoCritsect);

    _fecEnabled = false;
    _payloadTypeRED = -1;
    _payloadTypeFEC = -1;
    _codeRateKey = 0;
    _codeRateDelta = 0;
    _fecProtectionFactor = 0;
    _numberFirstPartition = 0;
    return 0;
}

void
RTPSenderVideo::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
}

void
RTPSenderVideo::SetVideoCodecType(RtpVideoCodecTypes videoType)
{
    CriticalSectionScoped cs(_sendVideoCritsect);
    _videoType = videoType;
}

RtpVideoCodecTypes
RTPSenderVideo::VideoCodecType() const
{
    return _videoType;
}

WebRtc_Word32
RTPSenderVideo::RegisterVideoPayload(const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                     const WebRtc_Word8 payloadType,
                                     const WebRtc_UWord32 maxBitRate,
                                     ModuleRTPUtility::Payload*& payload)
{
    CriticalSectionScoped cs(_sendVideoCritsect);

    RtpVideoCodecTypes videoType = kRtpNoVideo;
    if (ModuleRTPUtility::StringCompare(payloadName, "VP8",3))
    {
        videoType = kRtpVp8Video;
    }
    else if ((ModuleRTPUtility::StringCompare(payloadName, "H263-1998", 9)) ||
             (ModuleRTPUtility::StringCompare(payloadName, "H263-2000", 9)))
    {
        videoType = kRtpH2631998Video;
    }
    else if (ModuleRTPUtility::StringCompare(payloadName, "H263", 4))
    {
        videoType = kRtpH263Video;
    }
    else if (ModuleRTPUtility::StringCompare(payloadName, "MP4V-ES", 7))
    {
        videoType = kRtpMpeg4Video;
    } else if (ModuleRTPUtility::StringCompare(payloadName, "I420", 4))
    {
        videoType = kRtpNoVideo;
    }else
    {
        videoType = kRtpNoVideo;
        return -1;
    }
    payload = new ModuleRTPUtility::Payload;
    strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE);
    payload->typeSpecific.Video.videoCodecType = videoType;
    payload->typeSpecific.Video.maxRate = maxBitRate;
    payload->audio = false;

    return 0;
}

struct RtpPacket
{
    WebRtc_UWord16 rtpHeaderLength;
    ForwardErrorCorrection::Packet* pkt;
};

WebRtc_Word32
RTPSenderVideo::SendVideoPacket(const FrameType frameType,
                                const WebRtc_UWord8* dataBuffer,
                                const WebRtc_UWord16 payloadLength,
                                const WebRtc_UWord16 rtpHeaderLength)
{
    if(_fecEnabled)
    {
        WebRtc_Word32 retVal = 0;

        const bool markerBit = (dataBuffer[1] & kRtpMarkerBitMask)?true:false;
        RtpPacket* ptrGenericFEC = new RtpPacket;
        ptrGenericFEC->pkt = new ForwardErrorCorrection::Packet;
        ptrGenericFEC->pkt->length = payloadLength + rtpHeaderLength;
        ptrGenericFEC->rtpHeaderLength = rtpHeaderLength;
        memcpy(ptrGenericFEC->pkt->data, dataBuffer, ptrGenericFEC->pkt->length);

        // add packet to FEC list
        _rtpPacketListFec.PushBack(ptrGenericFEC);
        _mediaPacketListFec.PushBack(ptrGenericFEC->pkt);

        if (markerBit)  // last packet in frame
        {
            // interface for FEC
            ListWrapper fecPacketList;

            // Retain the RTP header of the last media packet to construct the FEC
            // packet RTP headers.
            ForwardErrorCorrection::Packet lastMediaRtpHeader;
            memcpy(lastMediaRtpHeader.data,
                   ptrGenericFEC->pkt->data,
                   ptrGenericFEC->rtpHeaderLength);

            lastMediaRtpHeader.length = ptrGenericFEC->rtpHeaderLength;
            lastMediaRtpHeader.data[1] = _payloadTypeRED; // Replace payload and clear
                                                          // marker bit.

            retVal = _fec.GenerateFEC(_mediaPacketListFec, _fecProtectionFactor,
                                      _numberFirstPartition, fecPacketList);
            while(!_rtpPacketListFec.Empty())
            {
                WebRtc_UWord8 newDataBuffer[IP_PACKET_SIZE];
                memset(newDataBuffer, 0, sizeof(newDataBuffer));

                ListItem* item = _rtpPacketListFec.First();
                RtpPacket* packetToSend =
                    static_cast<RtpPacket*>(item->GetItem());

                // copy RTP header
                memcpy(newDataBuffer, packetToSend->pkt->data, packetToSend->rtpHeaderLength);

                // get codec pltype
                WebRtc_UWord8 payloadType = newDataBuffer[1] & 0x7f;

                // replace pltype
                newDataBuffer[1] &= 0x80;            // reset
                newDataBuffer[1] += _payloadTypeRED; // replace

                // add RED header
                newDataBuffer[packetToSend->rtpHeaderLength] = payloadType; // f-bit always 0

                // copy payload data
                memcpy(newDataBuffer + packetToSend->rtpHeaderLength + REDForFECHeaderLength,
                       packetToSend->pkt->data + packetToSend->rtpHeaderLength,
                       packetToSend->pkt->length - packetToSend->rtpHeaderLength);

                _rtpPacketListFec.PopFront();
                _mediaPacketListFec.PopFront();

                // send normal packet with RED header
                retVal |= _rtpSender.SendToNetwork(newDataBuffer,
                                        packetToSend->pkt->length - packetToSend->rtpHeaderLength + REDForFECHeaderLength,
                                        packetToSend->rtpHeaderLength);

                delete packetToSend->pkt;
                delete packetToSend;
                packetToSend = NULL;
            }
            assert(_mediaPacketListFec.Empty());
            assert(_rtpPacketListFec.Empty());

            while(!fecPacketList.Empty())
            {
                WebRtc_UWord8 newDataBuffer[IP_PACKET_SIZE];

                ListItem* item = fecPacketList.First();

                // build FEC packets
                ForwardErrorCorrection::Packet* packetToSend =
                    static_cast<ForwardErrorCorrection::Packet*>(item->GetItem());

                // The returned FEC packets have no RTP headers.
                // Copy the last media packet's modified RTP header.
                memcpy(newDataBuffer, lastMediaRtpHeader.data, lastMediaRtpHeader.length);

                // add sequence number
                ModuleRTPUtility::AssignUWord16ToBuffer(&newDataBuffer[2],_rtpSender.IncrementSequenceNumber());

                // add RED header
                newDataBuffer[lastMediaRtpHeader.length] = _payloadTypeFEC; // f-bit always 0

                // copy payload data
                memcpy(newDataBuffer + lastMediaRtpHeader.length + REDForFECHeaderLength,
                       packetToSend->data,
                       packetToSend->length);

                fecPacketList.PopFront();

                assert(packetToSend->length != 0); // invalid FEC packet

                // No marker bit on FEC packets, last media packet have the marker
                // send FEC packet with RED header
                retVal |= _rtpSender.SendToNetwork(newDataBuffer,
                                                   packetToSend->length + REDForFECHeaderLength,
                                                   lastMediaRtpHeader.length);
            }
        }
        return retVal;
    }
    return _rtpSender.SendToNetwork(dataBuffer,
                                    payloadLength,
                                    rtpHeaderLength);
}

WebRtc_Word32
RTPSenderVideo::SendRTPIntraRequest()
{
    // RFC 2032
    // 5.2.1.  Full intra-frame Request (FIR) packet

    WebRtc_UWord16 length = 8;
    WebRtc_UWord8 data[8];
    data[0] = 0x80;
    data[1] = 192;
    data[2] = 0;
    data[3] = 1; // length

    ModuleRTPUtility::AssignUWord32ToBuffer(data+4, _rtpSender.SSRC());

    return _rtpSender.SendToNetwork(data, 0, length);
}

WebRtc_Word32
RTPSenderVideo::SetGenericFECStatus(const bool enable,
                                    const WebRtc_UWord8 payloadTypeRED,
                                    const WebRtc_UWord8 payloadTypeFEC)
{
    _fecEnabled = enable;
    _payloadTypeRED = payloadTypeRED;
    _payloadTypeFEC = payloadTypeFEC;
    _codeRateKey = 0;
    _codeRateDelta = 0;

    return 0;
}

WebRtc_Word32
RTPSenderVideo::GenericFECStatus(bool& enable,
                                 WebRtc_UWord8& payloadTypeRED,
                                 WebRtc_UWord8& payloadTypeFEC) const
{
    enable = _fecEnabled;
    payloadTypeRED = _payloadTypeRED;
    payloadTypeFEC = _payloadTypeFEC;
    return 0;
}

WebRtc_UWord16
RTPSenderVideo::FECPacketOverhead() const
{
    if (_fecEnabled)
    {
        return ForwardErrorCorrection::PacketOverhead() + REDForFECHeaderLength;
    }
    return 0;
}

WebRtc_Word32
RTPSenderVideo::SetFECCodeRate(const WebRtc_UWord8 keyFrameCodeRate,
                               const WebRtc_UWord8 deltaFrameCodeRate)
{
    _codeRateKey = keyFrameCodeRate;
    _codeRateDelta = deltaFrameCodeRate;
    return 0;
}

WebRtc_Word32
RTPSenderVideo::SendVideo(const RtpVideoCodecTypes videoType,
                          const FrameType frameType,
                          const WebRtc_Word8 payloadType,
                          const WebRtc_UWord32 captureTimeStamp,
                          const WebRtc_UWord8* payloadData,
                          const WebRtc_UWord32 payloadSize,
                          const RTPFragmentationHeader* fragmentation,
                          VideoCodecInformation* codecInfo)
{
    if( payloadSize == 0)
    {
        return -1;
    }

    if (frameType == kVideoFrameKey)
    {
        _fecProtectionFactor = _codeRateKey;
    } else
    {
        _fecProtectionFactor = _codeRateDelta;
    }

    // Default setting for number of first partition packets:
    // Will be extracted in SendVP8 for VP8 codec; other codecs use 0
    _numberFirstPartition = 0;

    WebRtc_Word32 retVal = -1;
    switch(videoType)
    {
    case kRtpNoVideo:
        retVal = SendGeneric(payloadType,captureTimeStamp, payloadData, payloadSize );
        break;
    case kRtpH263Video:
        retVal = SendH263(frameType,payloadType, captureTimeStamp, payloadData, payloadSize, codecInfo);
        break;
    case kRtpH2631998Video: //RFC 4629
        retVal = SendH2631998(frameType,payloadType, captureTimeStamp, payloadData, payloadSize, codecInfo);
        break;
    case kRtpMpeg4Video:   // RFC 3016
        retVal = SendMPEG4(frameType,payloadType, captureTimeStamp, payloadData, payloadSize);
        break;
    case kRtpVp8Video:
        retVal = SendVP8(frameType, payloadType, captureTimeStamp, payloadData, payloadSize, fragmentation);
        break;
    default:
        assert(false);
        break;
    }
    if(retVal <= 0)
    {
        return retVal;
    }
    return 0;
}

WebRtc_Word32
RTPSenderVideo::SendGeneric(const WebRtc_Word8 payloadType,
                            const WebRtc_UWord32 captureTimeStamp,
                            const WebRtc_UWord8* payloadData,
                            const WebRtc_UWord32 payloadSize)
{
    WebRtc_UWord16 payloadBytesInPacket = 0;
    WebRtc_UWord32 bytesSent = 0;
    WebRtc_Word32 payloadBytesToSend = payloadSize;

    const WebRtc_UWord8* data = payloadData;
    WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();
    WebRtc_UWord16 maxLength = _rtpSender.MaxPayloadLength() - FECPacketOverhead() - rtpHeaderLength;
    WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE];

    // Fragment packet into packets of max MaxPayloadLength bytes payload.
    while (payloadBytesToSend > 0)
    {
        if (payloadBytesToSend > maxLength)
        {
            payloadBytesInPacket = maxLength;
            payloadBytesToSend -= payloadBytesInPacket;

            if(_rtpSender.BuildRTPheader(dataBuffer, payloadType, false, captureTimeStamp) != rtpHeaderLength)    //markerBit is 0
            {
                // error
                return -1;
           }
        }
        else
        {
            payloadBytesInPacket = (WebRtc_UWord16)payloadBytesToSend;
            payloadBytesToSend = 0;

            if(_rtpSender.BuildRTPheader(dataBuffer, payloadType, true, captureTimeStamp) != rtpHeaderLength)    //markerBit is 1
            {
                // error
                return -1;
            }
        }

        // Put payload in packet
        memcpy(&dataBuffer[rtpHeaderLength], &data[bytesSent], payloadBytesInPacket);
        bytesSent += payloadBytesInPacket;

        if(-1 == SendVideoPacket(kVideoFrameKey,
                                 dataBuffer,
                                 payloadBytesInPacket,
                                 rtpHeaderLength))
        {
            return -1;
        }
    }
    return 0;
}

WebRtc_Word32
RTPSenderVideo::SendPadData(const WebRtcRTPHeader* rtpHeader,
                            const WebRtc_UWord32 bytes)
{
    const WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();
    WebRtc_UWord32 maxLength = _rtpSender.MaxPayloadLength() - FECPacketOverhead() - rtpHeaderLength;
    WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE];

    if(bytes<maxLength)
    {
        // for a small packet don't spend too much time
        maxLength = bytes;
    }

    {
        CriticalSectionScoped cs(_sendVideoCritsect);

        // send paded data
        // correct seq num, time stamp and payloadtype
        // we reuse the last seq number
        _rtpSender.BuildRTPheader(dataBuffer, rtpHeader->header.payloadType, false,0, false, false);

        // version 0 to be compatible with old ViE
        dataBuffer[0] &= !0x80;

        // set relay SSRC
        ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer+8, rtpHeader->header.ssrc);

        WebRtc_Word32* data = (WebRtc_Word32*)&(dataBuffer[12]); // start at 12

        // build data buffer
        for(WebRtc_UWord32 j = 0; j < ((maxLength>>2)-4) && j < (bytes>>4); j++)
        {
            data[j] = rand();
        }
    }
    // min
    WebRtc_UWord16 length = (WebRtc_UWord16)(bytes<maxLength?bytes:maxLength);

    // Send the packet
    return _rtpSender.SendToNetwork(dataBuffer, length, rtpHeaderLength, true);
}

/*
*   MPEG4
*/

WebRtc_Word32
RTPSenderVideo::SendMPEG4(const FrameType frameType,
                          const WebRtc_Word8 payloadType,
                          const WebRtc_UWord32 captureTimeStamp,
                          const WebRtc_UWord8* payloadData,
                          const WebRtc_UWord32 payloadSize)
{
    WebRtc_Word32 payloadBytesToSend = payloadSize;
    WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();
    WebRtc_UWord16 maxLength = _rtpSender.MaxPayloadLength() - FECPacketOverhead() - rtpHeaderLength;
    WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE];

    // Fragment packet WebRtc_Word32 packets of max MaxPayloadLength bytes payload.
    const WebRtc_UWord8* data = payloadData;

    while (payloadBytesToSend > 0)
    {
        WebRtc_UWord16 payloadBytes = 0;
        WebRtc_Word32 dataOffset = rtpHeaderLength;

        do
        {
            WebRtc_Word32 size = 0;
            bool markerBit = false;
            if(payloadBytesToSend > maxLength)
            {
                size = FindMPEG4NALU(data, maxLength);
            }else
            {
                markerBit = true; // last in frame
                size = payloadBytesToSend;
            }
            if(size <= 0)
            {
                return -1;
            }
            if(size > maxLength)
            {
                // we need to fragment NALU
                return -1;
            }

            if(payloadBytes == 0)
            {
                // build RTP header
                if(_rtpSender.BuildRTPheader(dataBuffer, payloadType, markerBit, captureTimeStamp) != rtpHeaderLength)
                {
                    // error
                    return -1;
                }
            }

            if( size + payloadBytes <= maxLength)
            {
                // Put payload in packet
                memcpy(&dataBuffer[dataOffset], data, size);
                dataOffset += size; //advance frame ptr
                data += size; //advance packet ptr
                payloadBytes += (WebRtc_UWord16)size;
                payloadBytesToSend -= size;
            } else
            {
                break; // send packet
            }
        }while(payloadBytesToSend);

        if(-1 == SendVideoPacket(frameType, dataBuffer, payloadBytes, rtpHeaderLength))
        {
            return -1;
        }
    }
    return 0;
}

WebRtc_Word32
RTPSenderVideo::FindMPEG4NALU(const WebRtc_UWord8* inData, WebRtc_Word32 maxLength)
{
    WebRtc_Word32 size = 0;
    for (WebRtc_Word32 i = maxLength; i > 4; i-=2) // Find NAL
    {
        // scan down
        if (inData[i] == 0)
        {
            if (inData[i-1] == 0)
            {
                // i point at the last zero
                size = i-1;
            }else if(inData[i+1] == 0)
            {
                size = i;
            }
            if(size > 0)
            {
                return size;
            }
        }
    }
    return 0;
}

VideoCodecInformation*
RTPSenderVideo::CodecInformationVideo()
{
    return _videoCodecInformation;
}

void
RTPSenderVideo::SetMaxConfiguredBitrateVideo(const WebRtc_UWord32 maxBitrate)
{
    _maxBitrate = maxBitrate;
}

WebRtc_UWord32
RTPSenderVideo::MaxConfiguredBitrateVideo() const
{
    return _maxBitrate;
}

/*
*   H.263
*/

WebRtc_Word32
RTPSenderVideo::SendH263(const FrameType frameType,
                         const WebRtc_Word8 payloadType,
                         const WebRtc_UWord32 captureTimeStamp,
                         const WebRtc_UWord8* payloadData,
                         const WebRtc_UWord32 payloadSize,
                         VideoCodecInformation* codecInfo)
{
    bool modeA = true;
    WebRtc_UWord16 h263HeaderLength = 4;
    WebRtc_UWord16 payloadBytesInPacket = 0;
    WebRtc_Word32 payloadBytesToSend = payloadSize;
    WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();

    // -2: one byte is possible old ebit -> sBit, one byte is new ebit if next GOB header is not byte aligned
    WebRtc_UWord16 maxPayloadLengthH263 = _rtpSender.MaxPayloadLength() - FECPacketOverhead() - rtpHeaderLength - h263HeaderLength - 2; // (eventual sBit, eBit)

    // Fragment packet into packets of max MaxPayloadLength bytes payload.
    WebRtc_UWord8 numOfGOB = 0;
    WebRtc_UWord16 prevOK = 0;
    WebRtc_UWord32 payloadBytesSent = 0;
    WebRtc_UWord8 sbit = 0;
    _eBit = 0;

    H263Information* h263Information = NULL;
    if(codecInfo)
    {
        // another channel have already parsed this data
        h263Information = static_cast<H263Information*>(codecInfo);

    } else
    {
        if(_videoCodecInformation)
        {
            if(_videoCodecInformation->Type() != kRtpH263Video)
            {
                // wrong codec
                delete _videoCodecInformation;
                _videoCodecInformation = new H263Information();
            } else
            {
                _videoCodecInformation->Reset();
            }
        } else
        {
            _videoCodecInformation = new H263Information();
        }
        h263Information = static_cast<H263Information*>(_videoCodecInformation);
    }

    WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE];
    const WebRtc_UWord8* data = payloadData;
    const H263Info* ptrH263Info = NULL;

    if (h263Information->GetInfo(payloadData,payloadSize, ptrH263Info) == -1)
    {
        return -1;
    }

    while (payloadBytesToSend > 0)
    {
        prevOK = 0;
        modeA = true;

        if (payloadBytesToSend > maxPayloadLengthH263)
        {
            // Fragment packet at GOB boundary
            for (; numOfGOB < ptrH263Info->numOfGOBs; numOfGOB++)
            {
                // Fit one or more GOBs into packet
                if (WebRtc_Word32(ptrH263Info->ptrGOBbuffer[numOfGOB+1] - payloadBytesSent) < maxPayloadLengthH263)
                {
                    prevOK = WebRtc_UWord16(ptrH263Info->ptrGOBbuffer[numOfGOB+1] - payloadBytesSent);
                }else
                {
                    break;
                }
            }
            if (!prevOK)
            {
                // GOB larger than max MaxPayloadLength bytes -> Mode B required
                // Fragment stream at MB boundaries
                modeA = false;

                // Get MB positions within GOB
                const H263MBInfo* ptrInfoMB = NULL;
                if (-1 == h263Information->GetMBInfo(payloadData,payloadSize,numOfGOB, ptrInfoMB))
                {
                    return -1;
                }
                WebRtc_Word32 offset = ptrH263Info->CalculateMBOffset(numOfGOB);
                if(offset < 0)
                {
                    return -1;
                }
                // Send packets fragmented at MB boundaries
                if (-1 == SendH263MBs(frameType, payloadType, captureTimeStamp, dataBuffer, data, rtpHeaderLength, numOfGOB, *ptrH263Info, *ptrInfoMB, offset))
                {
                    return -1;
                }
                offset = ptrH263Info->CalculateMBOffset(numOfGOB+1);
                if(offset < 0)
                {
                    return -1;
                }
                WebRtc_Word32 numBytes    = ptrInfoMB->ptrBuffer[offset-1] / 8;
                WebRtc_Word32 numBytesRem = ptrInfoMB->ptrBuffer[offset-1] % 8;
                if (numBytesRem)
                {
                    // incase our GOB is not byte alligned
                    numBytes++;
                }
                payloadBytesToSend -= numBytes;
                data += numBytes;
                payloadBytesSent += numBytes;
                numOfGOB++;
            }
        }
        if (modeA)
        {
            h263HeaderLength = 4;
            WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();

            // H.263 payload header (4 bytes)
            dataBuffer[rtpHeaderLength] = 0;                            // First bit 0 == mode A, (00 000 000)
            dataBuffer[rtpHeaderLength+1] = ptrH263Info->uiH263PTypeFmt << 5;
            dataBuffer[rtpHeaderLength + 1] += ptrH263Info->codecBits << 1; // Last bit 0
            dataBuffer[rtpHeaderLength + 2] = 0;                        // First 3 bits 0
            dataBuffer[rtpHeaderLength + 3] = 0;                        // No pb frame

            sbit = (8 - _eBit) % 8;  // last packet eBit -> current packet sBit

            if (payloadBytesToSend > maxPayloadLengthH263)
            {
                if (numOfGOB > 0)
                {
                    // Check if GOB header is byte aligned
                    if(ptrH263Info->ptrGOBbufferSBit)
                    {
                        _eBit = (8 - ptrH263Info->ptrGOBbufferSBit[numOfGOB - 1]) % 8;
                    } else
                    {
                        _eBit = 0;
                    }
                }
                if (_eBit)
                {
                    // next GOB header is not byte aligned, include this byte in packet
                    // Send the byte with eBits
                    prevOK++;
                }
            }

            if (payloadBytesToSend > maxPayloadLengthH263)
            {
                payloadBytesInPacket = prevOK;
                payloadBytesToSend -= payloadBytesInPacket;
                _rtpSender.BuildRTPheader(dataBuffer, payloadType, false, captureTimeStamp);

            } else
            {
                payloadBytesInPacket = (WebRtc_UWord16)payloadBytesToSend;
                payloadBytesToSend = 0;
                _rtpSender.BuildRTPheader(dataBuffer, payloadType, true, captureTimeStamp);
                _eBit = 0;
            }

            if (sbit)
            {
                // Add last sent byte and put payload in packet
                dataBuffer[rtpHeaderLength] = dataBuffer[rtpHeaderLength] | ((sbit & 0x7) << 3); // Set sBit
                memcpy(&dataBuffer[rtpHeaderLength + h263HeaderLength], &_savedByte, 1);
                memcpy(&dataBuffer[rtpHeaderLength + h263HeaderLength + 1], data, payloadBytesInPacket);
                h263HeaderLength++;

            }else
            {
                // Put payload in packet
                memcpy(&dataBuffer[rtpHeaderLength + h263HeaderLength], data, payloadBytesInPacket);
            }
            if (_eBit)
            {
                // Save last byte to paste in next packet
                dataBuffer[rtpHeaderLength] |= (_eBit & 0x7);    // Set eBit
                _savedByte = dataBuffer[payloadBytesInPacket + h263HeaderLength + rtpHeaderLength-1];
            }
            if (-1 == SendVideoPacket(frameType, dataBuffer, payloadBytesInPacket + h263HeaderLength, rtpHeaderLength))
            {
                return -1;
            }
            payloadBytesSent += payloadBytesInPacket;
            data += payloadBytesInPacket;
        }
    }
    return 0;
}

WebRtc_Word32
RTPSenderVideo::SendH2631998(const FrameType frameType,
                             const WebRtc_Word8 payloadType,
                             const WebRtc_UWord32 captureTimeStamp,
                             const WebRtc_UWord8* payloadData,
                             const WebRtc_UWord32 payloadSize,
                             VideoCodecInformation* codecInfo)
{
    const WebRtc_UWord16 h2631998HeaderLength = 2;
    const WebRtc_UWord8 pLen = 0;                     // No extra header included
    const WebRtc_UWord8 peBit = 0;
    bool fragment = false;
    WebRtc_UWord16 payloadBytesInPacket = 0;
    WebRtc_Word32 payloadBytesToSend = payloadSize;
    WebRtc_UWord16 numPayloadBytesToSend = 0;
    WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();

    WebRtc_UWord8 p = 2;  // P is not set in all packets, only packets that has a PictureStart or a GOB header

    H263Information* h263Information = NULL;
    if(codecInfo)
    {
        // another channel have already parsed this data
        h263Information = static_cast<H263Information*>(codecInfo);

    } else
    {
        if(_videoCodecInformation)
        {
            if(_videoCodecInformation->Type() != kRtpH263Video)
            {
                // wrong codec
                delete _videoCodecInformation;
                _videoCodecInformation = new H263Information();
            } else
            {
                _videoCodecInformation->Reset();
            }
        } else
        {
            _videoCodecInformation = new H263Information();
        }
        h263Information = static_cast<H263Information*>(_videoCodecInformation);
    }
    const H263Info* ptrH263Info = NULL;
    if (h263Information->GetInfo(payloadData,payloadSize, ptrH263Info) == -1)
    {
        return -1;
    }

    WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE];
    const WebRtc_UWord16 maxPayloadLengthH2631998 = _rtpSender.MaxPayloadLength() - FECPacketOverhead() - rtpHeaderLength - h2631998HeaderLength;
    const WebRtc_UWord8* data = payloadData;
    WebRtc_UWord8 numOfGOB = 0;
    WebRtc_UWord32 payloadBytesSent = 0;

    while(payloadBytesToSend > 0)
    {
        WebRtc_Word32 prevOK = 0;

        // Fragment packets at GOB boundaries
        for (; numOfGOB < ptrH263Info->numOfGOBs; numOfGOB++)
        {
            // Fit one or more GOBs into packet
            if (WebRtc_Word32(ptrH263Info->ptrGOBbuffer[numOfGOB+1] - payloadBytesSent) <= (maxPayloadLengthH2631998 + p))
            {
                prevOK = WebRtc_UWord16(ptrH263Info->ptrGOBbuffer[numOfGOB+1] - payloadBytesSent);
                if(fragment)
                {
                    // this is a fragment, send it
                    break;
                }
            }else
            {
                break;
            }
        }
        if(!prevOK)
        {
            // GOB larger than MaxPayloadLength bytes
            fragment = true;
            numPayloadBytesToSend = maxPayloadLengthH2631998;
        } else
        {
            fragment = false;
            numPayloadBytesToSend = WebRtc_UWord16(prevOK - p);
        }
        dataBuffer[rtpHeaderLength] = (p << 1) + ((pLen >> 5) & 0x01);
        dataBuffer[rtpHeaderLength+1] = ((pLen & 0x1F) << 3) + peBit;

        if(p == 2)
        {
            data += 2; // inc data ptr (do not send first two bytes of picture or GOB start code)
            payloadBytesToSend -= 2;
        }

        if(payloadBytesToSend > maxPayloadLengthH2631998)
        {
            payloadBytesInPacket = numPayloadBytesToSend;
            payloadBytesToSend -= payloadBytesInPacket;

            _rtpSender.BuildRTPheader(dataBuffer,payloadType, false, captureTimeStamp);
        }else
        {
            payloadBytesInPacket = (WebRtc_UWord16)payloadBytesToSend;
            payloadBytesToSend = 0;

            _rtpSender.BuildRTPheader(dataBuffer, payloadType, true, captureTimeStamp);  // markerBit is 1
        }
        // Put payload in packet
        memcpy(&dataBuffer[rtpHeaderLength + h2631998HeaderLength], data, payloadBytesInPacket);

        if(-1 == SendVideoPacket(frameType, dataBuffer, payloadBytesInPacket + h2631998HeaderLength, rtpHeaderLength))
        {
            return -1;
        }
        data += payloadBytesInPacket;
        payloadBytesSent += payloadBytesInPacket + p;
        if(fragment)
        {
            p = 0;
        }else
        {
            p = 2;
        }
    }
    return 0;
}

WebRtc_Word32
RTPSenderVideo::SendH263MBs(const FrameType frameType,
                            const WebRtc_Word8 payloadType,
                            const WebRtc_UWord32 captureTimeStamp,
                            WebRtc_UWord8* dataBuffer,
                            const WebRtc_UWord8 *data,
                            const WebRtc_UWord16 rtpHeaderLength,
                            const WebRtc_UWord8 numOfGOB,
                            const H263Info& info,
                            const H263MBInfo& infoMB,
                            const WebRtc_Word32 offset)
{
    // Mode B
    WebRtc_UWord32 *sizeOfMBs = &infoMB.ptrBuffer[offset];
    WebRtc_UWord8 *hmv1 = &infoMB.ptrBufferHMV[offset];
    WebRtc_UWord8 *vmv1 = &infoMB.ptrBufferVMV[offset];

    WebRtc_UWord16 h263HeaderLength = 8;
    WebRtc_UWord16 payloadBytesInPacket = 0;
    WebRtc_Word32 payloadBytesToSend = sizeOfMBs[info.ptrNumOfMBs[numOfGOB]-1] / 8;
    WebRtc_UWord8 eBitLastByte = (WebRtc_UWord8)((8 - (sizeOfMBs[info.ptrNumOfMBs[numOfGOB]-1] % 8)) % 8);
    WebRtc_Word32 sBit = 0;
    WebRtc_Word32 firstMB = 0;
    WebRtc_UWord32 bitsRem = 0;
    WebRtc_UWord32 payloadBytesSent = 0;
    WebRtc_Word32 numOfMB = 0;
    WebRtc_Word32 prevOK = 0;

    WebRtc_UWord16 maxPayloadLengthH263MB = _rtpSender.MaxPayloadLength() - FECPacketOverhead() - rtpHeaderLength - h263HeaderLength - 2; // (eventual sBit, eBit)

    if (eBitLastByte)
    {
        payloadBytesToSend++;
    }

    // Fragment packet into packets of max MaxPayloadLength bytes payload.
    while (payloadBytesToSend > 0)
    {
        prevOK = 0;
        firstMB = numOfMB;
        if (payloadBytesToSend > maxPayloadLengthH263MB)
        {
            // Fragment packet at MB boundary
            for (; numOfMB < info.ptrNumOfMBs[numOfGOB]; numOfMB++)
            {
                // Fit one or more MBs into packet
                if (WebRtc_Word32(sizeOfMBs[numOfMB] / 8 - payloadBytesSent) < maxPayloadLengthH263MB)
                {
                    prevOK = sizeOfMBs[numOfMB] / 8 - payloadBytesSent;
                    bitsRem = sizeOfMBs[numOfMB] % 8;
                    if (bitsRem)
                    {
                        prevOK++;
                    }
                }else
                {
                    break;
                }
            }

            if (!prevOK)
            {
                // MB does not fit in packet
                return -1;
            }
        }


        // H.263 payload header (8 bytes)
        h263HeaderLength = 8;
        dataBuffer[rtpHeaderLength] = (WebRtc_UWord8)0x80;                   // First bit 1 == mode B, 10 000 000
        dataBuffer[rtpHeaderLength + 1] = (info.uiH263PTypeFmt) << 5;      // Source format
        if (numOfGOB == 0)
        {
            dataBuffer[rtpHeaderLength + 1] += info.pQuant;                // Quantization value for first MB in packet
        }
        if (numOfGOB > 0 && firstMB > 0)
        {
            dataBuffer[rtpHeaderLength + 1] += info.ptrGQuant[numOfGOB];   // Quantization value for first MB in packet (0 if packet begins w/ a GOB header)
        }
        dataBuffer[rtpHeaderLength + 2] = numOfGOB << 3;                   // GOB #
        dataBuffer[rtpHeaderLength + 2] += (WebRtc_UWord8)((firstMB >> 6)& 0x7);  // First MB in the packet
        dataBuffer[rtpHeaderLength + 3] = (WebRtc_UWord8)(firstMB << 2);
        dataBuffer[rtpHeaderLength + 4] = (info.codecBits) << 4;
        dataBuffer[rtpHeaderLength + 4] += (hmv1[firstMB] & 0x7F) >> 3;    // Horizontal motion vector
        dataBuffer[rtpHeaderLength + 5] = hmv1[firstMB] << 5;
        dataBuffer[rtpHeaderLength + 5] += (vmv1[firstMB] & 0x7F) >> 2;    // Vertical motion vector
        dataBuffer[rtpHeaderLength + 6] = vmv1[firstMB] << 6;
        dataBuffer[rtpHeaderLength + 7] = 0;

        sBit = (8 - _eBit) % 8;

        if (payloadBytesToSend > maxPayloadLengthH263MB)
        {
            payloadBytesInPacket = (WebRtc_UWord16)prevOK;
            payloadBytesToSend -= payloadBytesInPacket;

            _rtpSender.BuildRTPheader(dataBuffer, payloadType, false, captureTimeStamp);

            _eBit = (WebRtc_UWord8)((8 - bitsRem) % 8);
        }
        else
        {
            payloadBytesInPacket = (WebRtc_UWord16)payloadBytesToSend;
            payloadBytesToSend = 0;

            if (numOfGOB == (info.numOfGOBs - 1))
            {
                _rtpSender.BuildRTPheader(dataBuffer, payloadType, true, captureTimeStamp);
                _eBit = 0;
            }
            else
            {
                _rtpSender.BuildRTPheader(dataBuffer, payloadType, false, captureTimeStamp);
                _eBit = eBitLastByte;
            }
        }


        if (sBit)
        {
            // Add last sent byte and put payload in packet
            dataBuffer[rtpHeaderLength] |= ((sBit & 0x7) << 3);
            dataBuffer[rtpHeaderLength + h263HeaderLength] = _savedByte;
            memcpy(&dataBuffer[rtpHeaderLength + h263HeaderLength + 1], data, payloadBytesInPacket);
            h263HeaderLength++;
        } else
        {
            // Put payload in packet
            memcpy(&dataBuffer[rtpHeaderLength + h263HeaderLength], data, payloadBytesInPacket);
        }
        if (_eBit)
        {
            // Save last byte to paste in next packet
            dataBuffer[rtpHeaderLength] |= (_eBit & 0x7);
            _savedByte = dataBuffer[rtpHeaderLength + h263HeaderLength + payloadBytesInPacket - 1];
        }
        if (-1 == SendVideoPacket(frameType, dataBuffer, payloadBytesInPacket + h263HeaderLength, rtpHeaderLength))
        {
            return -1;
        }

        data += payloadBytesInPacket;
        payloadBytesSent += payloadBytesInPacket;
    }
    return 0;
}

/*
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| RSV |I|N|FI |B|     PictureID (integer #bytes)                |
+-+-+-+-+-+-+-+-+                                               |
:                                                               :
|               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|               : (VP8 data or VP8 payload header; byte aligned)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
WebRtc_Word32
RTPSenderVideo::SendVP8(const FrameType frameType,
                        const WebRtc_Word8 payloadType,
                        const WebRtc_UWord32 captureTimeStamp,
                        const WebRtc_UWord8* payloadData,
                        const WebRtc_UWord32 payloadSize,
                        const RTPFragmentationHeader* fragmentation)
{
    const WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();
    WebRtc_UWord16 vp8HeaderLength = 1;
    int payloadBytesInPacket = 0;
    WebRtc_Word32 bytesSent = 0;

    WebRtc_Word32 payloadBytesToSend = payloadSize;
    const WebRtc_UWord8* data = payloadData;

    WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE];
    WebRtc_UWord16 maxPayloadLengthVP8 = _rtpSender.MaxPayloadLength()
        - FECPacketOverhead() - rtpHeaderLength;

    RtpFormatVp8 packetizer(data, payloadBytesToSend, *fragmentation, kStrict);

    bool last = false;
    while (!last)
    {
        // Write VP8 Payload Descriptor and VP8 payload.
        if (packetizer.NextPacket(maxPayloadLengthVP8,
            &dataBuffer[rtpHeaderLength], &payloadBytesInPacket, &last) < 0)
        {
            return -1;
        }

        // Write RTP header.
        // Set marker bit true if this is the last packet in frame.
        _rtpSender.BuildRTPheader(dataBuffer, payloadType, last,
            captureTimeStamp);

        // TODO (marpan): Set _numberFirstPartition here:
        // Equal to the first packet that contains last fragment of first partition

        if (-1 == SendVideoPacket(frameType, dataBuffer, payloadBytesInPacket,
            rtpHeaderLength))
        {
            return -1;
        }
    }
    return 0;
}
} // namespace webrtc
