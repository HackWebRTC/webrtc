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

RTPSenderVideo::RTPSenderVideo(const WebRtc_Word32 id,
                               RtpRtcpClock* clock,
                               RTPSenderInterface* rtpSender) :
    _id(id),
    _rtpSender(*rtpSender),
    _sendVideoCritsect(CriticalSectionWrapper::CreateCriticalSection()),

    _videoType(kRtpNoVideo),
    _videoCodecInformation(NULL),
    _maxBitrate(0),

    // Generic FEC
    _fec(id),
    _fecEnabled(false),
    _payloadTypeRED(-1),
    _payloadTypeFEC(-1),
    _codeRateKey(0),
    _codeRateDelta(0),
    _useUepProtectionKey(false),
    _useUepProtectionDelta(false),
    _fecProtectionFactor(0),
    _fecUseUepProtection(false),
    _numberFirstPartition(0),
    _fecOverheadRate(clock),
    _videoBitrate(clock),

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
    delete _sendVideoCritsect;
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
    _useUepProtectionKey = false;
    _useUepProtectionDelta = false;
    _fecProtectionFactor = 0;
    _fecUseUepProtection = false;
    _numberFirstPartition = 0;
    _fecOverheadRate.Init();
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
RTPSenderVideo::RegisterVideoPayload(
        const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
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
        memcpy(ptrGenericFEC->pkt->data, dataBuffer,
               ptrGenericFEC->pkt->length);

        // Add packet to FEC list
        _rtpPacketListFec.PushBack(ptrGenericFEC);
        // FEC can only protect up to kMaxMediaPackets packets
        if (static_cast<int>(_mediaPacketListFec.GetSize()) <
            ForwardErrorCorrection::kMaxMediaPackets)
        {
            _mediaPacketListFec.PushBack(ptrGenericFEC->pkt);
        }

        // Last packet in frame
        if (markerBit)
        {
            // Interface for FEC
            ListWrapper fecPacketList;

            // Retain the RTP header of the last media packet to construct FEC
            // packet RTP headers.
            ForwardErrorCorrection::Packet lastMediaRtpHeader;
            memcpy(lastMediaRtpHeader.data,
                   ptrGenericFEC->pkt->data,
                   ptrGenericFEC->rtpHeaderLength);

            lastMediaRtpHeader.length = ptrGenericFEC->rtpHeaderLength;
            // Replace payload and clear marker bit.
            lastMediaRtpHeader.data[1] = _payloadTypeRED;

            // Number of first partition packets cannot exceed kMaxMediaPackets
            if (_numberFirstPartition >
                ForwardErrorCorrection::kMaxMediaPackets)
            {
                _numberFirstPartition =
                    ForwardErrorCorrection::kMaxMediaPackets;
            }

            retVal = _fec.GenerateFEC(_mediaPacketListFec,
                                      _fecProtectionFactor,
                                      _numberFirstPartition,
                                      _fecUseUepProtection,
                                      fecPacketList);

            int fecOverheadSent = 0;
            int videoSent = 0;

            while(!_rtpPacketListFec.Empty())
            {
                WebRtc_UWord8 newDataBuffer[IP_PACKET_SIZE];
                memset(newDataBuffer, 0, sizeof(newDataBuffer));

                ListItem* item = _rtpPacketListFec.First();
                RtpPacket* packetToSend =
                    static_cast<RtpPacket*>(item->GetItem());

                item = _mediaPacketListFec.First();
                ForwardErrorCorrection::Packet* mediaPacket =
                  static_cast<ForwardErrorCorrection::Packet*>(item->GetItem());

                // Copy RTP header
                memcpy(newDataBuffer, packetToSend->pkt->data,
                       packetToSend->rtpHeaderLength);

                // Get codec pltype
                WebRtc_UWord8 payloadType = newDataBuffer[1] & 0x7f;

                // Replace pltype
                newDataBuffer[1] &= 0x80;            // reset
                newDataBuffer[1] += _payloadTypeRED; // replace

                // Add RED header
                // f-bit always 0
                newDataBuffer[packetToSend->rtpHeaderLength] = payloadType;

                // Copy payload data
                memcpy(newDataBuffer + packetToSend->rtpHeaderLength +
                           REDForFECHeaderLength,
                       packetToSend->pkt->data + packetToSend->rtpHeaderLength,
                       packetToSend->pkt->length -
                           packetToSend->rtpHeaderLength);

                _rtpPacketListFec.PopFront();
                _mediaPacketListFec.PopFront();

                // Send normal packet with RED header
                int packetSuccess = _rtpSender.SendToNetwork(
                        newDataBuffer,
                        packetToSend->pkt->length -
                           packetToSend->rtpHeaderLength +
                           REDForFECHeaderLength,
                        packetToSend->rtpHeaderLength);

                retVal |= packetSuccess;

                if (packetSuccess == 0)
                {
                    videoSent += mediaPacket->length;
                    fecOverheadSent += (packetToSend->pkt->length -
                        mediaPacket->length +
                        packetToSend->rtpHeaderLength +
                        REDForFECHeaderLength);
                }

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

                // Build FEC packets
                ForwardErrorCorrection::Packet* packetToSend =
                    static_cast<ForwardErrorCorrection::Packet*>
                    (item->GetItem());

                // The returned FEC packets have no RTP headers.
                // Copy the last media packet's modified RTP header.
                memcpy(newDataBuffer, lastMediaRtpHeader.data,
                       lastMediaRtpHeader.length);

                // Add sequence number
                ModuleRTPUtility::AssignUWord16ToBuffer(
                    &newDataBuffer[2], _rtpSender.IncrementSequenceNumber());

                // Add RED header
                // f-bit always 0
                newDataBuffer[lastMediaRtpHeader.length] = _payloadTypeFEC;

                // Copy payload data
                memcpy(newDataBuffer + lastMediaRtpHeader.length +
                           REDForFECHeaderLength,
                       packetToSend->data,
                       packetToSend->length);

                fecPacketList.PopFront();

                // Invalid FEC packet
                assert(packetToSend->length != 0);

                // No marker bit on FEC packets, last media packet have the
                // marker send FEC packet with RED header
                int packetSuccess = _rtpSender.SendToNetwork(
                        newDataBuffer,
                        packetToSend->length + REDForFECHeaderLength,
                        lastMediaRtpHeader.length);

                retVal |= packetSuccess;

                if (packetSuccess == 0)
                {
                    fecOverheadSent += packetToSend->length +
                      REDForFECHeaderLength + lastMediaRtpHeader.length;
                }
            }
            _videoBitrate.Update(videoSent);
            _fecOverheadRate.Update(fecOverheadSent);
        }
        return retVal;
    }
    int retVal = _rtpSender.SendToNetwork(dataBuffer,
                                          payloadLength,
                                          rtpHeaderLength);
    if (retVal == 0)
    {
        _videoBitrate.Update(payloadLength + rtpHeaderLength);
    }
    return retVal;
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
    _useUepProtectionKey = false;
    _useUepProtectionDelta = false;
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
        return ForwardErrorCorrection::PacketOverhead() +
            REDForFECHeaderLength;
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
RTPSenderVideo::SetFECUepProtection(const bool keyUseUepProtection,
                                    const bool deltaUseUepProtection)
{
    _useUepProtectionKey = keyUseUepProtection;
    _useUepProtectionDelta = deltaUseUepProtection;
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
                          VideoCodecInformation* codecInfo,
                          const RTPVideoTypeHeader* rtpTypeHdr)
{
    if( payloadSize == 0)
    {
        return -1;
    }

    if (frameType == kVideoFrameKey)
    {
        _fecProtectionFactor = _codeRateKey;
        _fecUseUepProtection = _useUepProtectionKey;
    } else if (videoType == kRtpVp8Video && rtpTypeHdr->VP8.temporalIdx > 0)
    {
        // In current version, we only apply FEC on the base layer.
        _fecProtectionFactor = 0;
        _fecUseUepProtection = false;
    } else
    {
        _fecProtectionFactor = _codeRateDelta;
        _fecUseUepProtection = _useUepProtectionDelta;
    }

    // Default setting for number of first partition packets:
    // Will be extracted in SendVP8 for VP8 codec; other codecs use 0
    _numberFirstPartition = 0;

    WebRtc_Word32 retVal = -1;
    switch(videoType)
    {
    case kRtpNoVideo:
        retVal = SendGeneric(payloadType,captureTimeStamp, payloadData,
                             payloadSize);
        break;
    case kRtpH263Video:
        retVal = SendH263(frameType,payloadType, captureTimeStamp, payloadData,
                          payloadSize, codecInfo);
        break;
    case kRtpH2631998Video: //RFC 4629
        retVal = SendH2631998(frameType,payloadType, captureTimeStamp,
                              payloadData, payloadSize, codecInfo);
        break;
    case kRtpMpeg4Video:   // RFC 3016
        retVal = SendMPEG4(frameType,payloadType, captureTimeStamp,
                           payloadData, payloadSize);
        break;
    case kRtpVp8Video:
        retVal = SendVP8(frameType, payloadType, captureTimeStamp,
                payloadData, payloadSize, fragmentation, rtpTypeHdr);
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
    WebRtc_UWord16 maxLength = _rtpSender.MaxPayloadLength() -
        FECPacketOverhead() - rtpHeaderLength;
    WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE];

    // Fragment packet into packets of max MaxPayloadLength bytes payload.
    while (payloadBytesToSend > 0)
    {
        if (payloadBytesToSend > maxLength)
        {
            payloadBytesInPacket = maxLength;
            payloadBytesToSend -= payloadBytesInPacket;
            // MarkerBit is 0
            if(_rtpSender.BuildRTPheader(dataBuffer,
                                         payloadType,
                                         false,
                                         captureTimeStamp) != rtpHeaderLength)
            {
                return -1;
           }
        }
        else
        {
            payloadBytesInPacket = (WebRtc_UWord16)payloadBytesToSend;
            payloadBytesToSend = 0;
            // MarkerBit is 1
            if(_rtpSender.BuildRTPheader(dataBuffer, payloadType, true,
                                         captureTimeStamp) != rtpHeaderLength)
            {
                return -1;
            }
        }

        // Put payload in packet
        memcpy(&dataBuffer[rtpHeaderLength], &data[bytesSent],
               payloadBytesInPacket);
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

void RTPSenderVideo::SendPadData(WebRtc_Word8 payload_type,
                                 WebRtc_UWord32 capture_timestamp,
                                 WebRtc_Word32 bytes) {
  // Max in the RFC 3550 is 255 bytes, we limit it to be modulus 32 for SRTP.
  int max_length = 224;
  WebRtc_UWord8 data_buffer[IP_PACKET_SIZE];

  for (; bytes > 0; bytes -= max_length) {
    WebRtc_Word32 header_length;
    {
      CriticalSectionScoped cs(_sendVideoCritsect);

      // Correct seq num, timestamp and payload type.
      header_length = _rtpSender.BuildRTPheader(
          data_buffer,
          payload_type,
          false,  // No markerbit.
          capture_timestamp,
          true,  // Timestamp provided.
          true);  // Increment sequence number.

    }
    data_buffer[0] |= 0x20;  // Set padding bit.
    WebRtc_Word32* data =
        reinterpret_cast<WebRtc_Word32*>(&(data_buffer[header_length]));

    int padding_bytes_in_packet = max_length;
    if (bytes < max_length) {
      padding_bytes_in_packet = (bytes + 16) & 0xffe0;  // Keep our modulus 32.
    }
    if (padding_bytes_in_packet < 32) {
       // Sanity don't send empty packets.
       return;
    }
    // Fill data buffer with random data.
    for(int j = 0; j < (padding_bytes_in_packet >> 2); j++) {
      data[j] = rand();
    }
    // Set number of padding bytes in the last byte of the packet.
    data_buffer[header_length + padding_bytes_in_packet - 1] =
        padding_bytes_in_packet;
    // Send the packet
    _rtpSender.SendToNetwork(data_buffer,
                             padding_bytes_in_packet,
                             header_length);
  }
}

WebRtc_Word32
RTPSenderVideo::SendMPEG4(const FrameType frameType,
                          const WebRtc_Word8 payloadType,
                          const WebRtc_UWord32 captureTimeStamp,
                          const WebRtc_UWord8* payloadData,
                          const WebRtc_UWord32 payloadSize)
{
    WebRtc_Word32 payloadBytesToSend = payloadSize;
    WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();
    WebRtc_UWord16 maxLength = _rtpSender.MaxPayloadLength() -
        FECPacketOverhead() - rtpHeaderLength;
    WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE];

    // Fragment packet of max MaxPayloadLength bytes payload.
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
               // Last in frame
                markerBit = true;
                size = payloadBytesToSend;
            }
            if(size <= 0)
            {
                return -1;
            }
            if(size > maxLength)
            {
                // We need to fragment NALU
                return -1;
            }

            if(payloadBytes == 0)
            {
                // Build RTP header
                if(_rtpSender.BuildRTPheader(
                        dataBuffer,
                        payloadType,
                        markerBit,
                        captureTimeStamp) != rtpHeaderLength)
                {
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
                // Send packet
                break;
            }
        }while(payloadBytesToSend);

        if (-1 == SendVideoPacket(frameType, dataBuffer, payloadBytes,
                                  rtpHeaderLength))
        {
            return -1;
        }
    }
    return 0;
}

WebRtc_Word32
RTPSenderVideo::FindMPEG4NALU(const WebRtc_UWord8* inData,
                              WebRtc_Word32 maxLength)
{
    WebRtc_Word32 size = 0;
    for (WebRtc_Word32 i = maxLength; i > 4; i-=2) // Find NAL
    {
        // Scan down
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

    // -2: one byte is possible old ebit -> sBit,
    // one byte is new ebit if next GOB header is not byte aligned
    // (eventual sBit, eBit)
    WebRtc_UWord16 maxPayloadLengthH263 = _rtpSender.MaxPayloadLength() -
        FECPacketOverhead() - rtpHeaderLength - h263HeaderLength - 2;

    // Fragment packet into packets of max MaxPayloadLength bytes payload.
    WebRtc_UWord8 numOfGOB = 0;
    WebRtc_UWord16 prevOK = 0;
    WebRtc_UWord32 payloadBytesSent = 0;
    WebRtc_UWord8 sbit = 0;
    _eBit = 0;

    H263Information* h263Information = NULL;
    if(codecInfo)
    {
        // Another channel have already parsed this data
        h263Information = static_cast<H263Information*>(codecInfo);

    } else
    {
        if(_videoCodecInformation)
        {
            if(_videoCodecInformation->Type() != kRtpH263Video)
            {
                // Wrong codec
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
                if (WebRtc_Word32(ptrH263Info->ptrGOBbuffer[numOfGOB+1] -
                            payloadBytesSent) < maxPayloadLengthH263)
                {
                    prevOK = static_cast<WebRtc_UWord16>(
                            ptrH263Info->ptrGOBbuffer[numOfGOB+1] -
                            payloadBytesSent);
                }else
                {
                    break;
                }
            }
            if (!prevOK)
            {
                // GOB larger than max MaxPayloadLength bytes => Mode B required
                // Fragment stream at MB boundaries
                modeA = false;

                // Get MB positions within GOB
                const H263MBInfo* ptrInfoMB = NULL;
                if (-1 == h263Information->GetMBInfo(payloadData, payloadSize,
                                                     numOfGOB, ptrInfoMB))
                {
                    return -1;
                }
                WebRtc_Word32 offset = ptrH263Info->
                    CalculateMBOffset(numOfGOB);
                if(offset < 0)
                {
                    return -1;
                }
                // Send packets fragmented at MB boundaries
                if (-1 == SendH263MBs(frameType, payloadType, captureTimeStamp,
                                      dataBuffer, data, rtpHeaderLength,
                                      numOfGOB, *ptrH263Info,*ptrInfoMB, offset))
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
                    // In case our GOB is not byte alligned
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
            // First bit 0 == mode A, (00 000 000)
            dataBuffer[rtpHeaderLength] = 0;
            dataBuffer[rtpHeaderLength+1] = ptrH263Info->uiH263PTypeFmt << 5;
            // Last bit 0
            dataBuffer[rtpHeaderLength + 1] += ptrH263Info->codecBits << 1;
            // First 3 bits 0
            dataBuffer[rtpHeaderLength + 2] = 0;
            // No pb frame
            dataBuffer[rtpHeaderLength + 3] = 0;

            // Last packet eBit -> current packet sBit
            sbit = (8 - _eBit) % 8;

            if (payloadBytesToSend > maxPayloadLengthH263)
            {
                if (numOfGOB > 0)
                {
                    // Check if GOB header is byte aligned
                    if(ptrH263Info->ptrGOBbufferSBit)
                    {
                        _eBit = (8 -
                            ptrH263Info->ptrGOBbufferSBit[numOfGOB - 1]) % 8;
                    } else
                    {
                        _eBit = 0;
                    }
                }
                if (_eBit)
                {
                    // Next GOB header is not byte aligned,
                    // include this byte in packet
                    // Send the byte with eBits
                    prevOK++;
                }
            }

            if (payloadBytesToSend > maxPayloadLengthH263)
            {
                payloadBytesInPacket = prevOK;
                payloadBytesToSend -= payloadBytesInPacket;
                _rtpSender.BuildRTPheader(dataBuffer, payloadType,
                                          false, captureTimeStamp);

            } else
            {
                payloadBytesInPacket = (WebRtc_UWord16)payloadBytesToSend;
                payloadBytesToSend = 0;
                _rtpSender.BuildRTPheader(dataBuffer, payloadType,
                                          true, captureTimeStamp);
                _eBit = 0;
            }

            if (sbit)
            {
                // Add last sent byte and put payload in packet
                // Set sBit
                dataBuffer[rtpHeaderLength] = dataBuffer[rtpHeaderLength] |
                    ((sbit & 0x7) << 3);
                memcpy(&dataBuffer[rtpHeaderLength + h263HeaderLength],
                       &_savedByte, 1);
                memcpy(&dataBuffer[rtpHeaderLength + h263HeaderLength + 1],
                       data, payloadBytesInPacket);
                h263HeaderLength++;

            }else
            {
                // Put payload in packet
                memcpy(&dataBuffer[rtpHeaderLength + h263HeaderLength], data,
                       payloadBytesInPacket);
            }
            if (_eBit)
            {
                // Save last byte to paste in next packet
                // Set eBit
                dataBuffer[rtpHeaderLength] |= (_eBit & 0x7);
                _savedByte = dataBuffer[payloadBytesInPacket +
                                        h263HeaderLength + rtpHeaderLength-1];
            }
            if (-1 == SendVideoPacket(frameType,
                                      dataBuffer,
                                      payloadBytesInPacket + h263HeaderLength,
                                      rtpHeaderLength))
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
    // No extra header included
    const WebRtc_UWord8 pLen = 0;
    const WebRtc_UWord8 peBit = 0;
    bool fragment = false;
    WebRtc_UWord16 payloadBytesInPacket = 0;
    WebRtc_Word32 payloadBytesToSend = payloadSize;
    WebRtc_UWord16 numPayloadBytesToSend = 0;
    WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();

    // P is not set in all packets,
    // only packets that has a PictureStart or a GOB header
    WebRtc_UWord8 p = 2;

    H263Information* h263Information = NULL;
    if(codecInfo)
    {
        // Another channel have already parsed this data
        h263Information = static_cast<H263Information*>(codecInfo);

    } else
    {
        if(_videoCodecInformation)
        {
            if(_videoCodecInformation->Type() != kRtpH263Video)
            {
                // Wrong codec
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
    const WebRtc_UWord16 maxPayloadLengthH2631998 =
        _rtpSender.MaxPayloadLength() - FECPacketOverhead() - rtpHeaderLength -
        h2631998HeaderLength;
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
            if (static_cast<WebRtc_Word32>(
                    ptrH263Info->ptrGOBbuffer[numOfGOB+1] -
                    payloadBytesSent) <=
                    (maxPayloadLengthH2631998 + p))
            {
                prevOK = static_cast<WebRtc_UWord16>(
                        ptrH263Info->ptrGOBbuffer[numOfGOB+1] -
                        payloadBytesSent);
                if(fragment)
                {
                    // This is a fragment, send it
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
           // Increment data ptr
           // (do not send first two bytes of picture or GOB start code)
            data += 2;
            payloadBytesToSend -= 2;
        }

        if(payloadBytesToSend > maxPayloadLengthH2631998)
        {
            payloadBytesInPacket = numPayloadBytesToSend;
            payloadBytesToSend -= payloadBytesInPacket;

            _rtpSender.BuildRTPheader(dataBuffer, payloadType,
                                      false, captureTimeStamp);
        }else
        {
            payloadBytesInPacket = (WebRtc_UWord16)payloadBytesToSend;
            payloadBytesToSend = 0;
            // MarkerBit is 1
            _rtpSender.BuildRTPheader(dataBuffer, payloadType,
                                      true, captureTimeStamp);
        }
        // Put payload in packet
        memcpy(&dataBuffer[rtpHeaderLength + h2631998HeaderLength],
               data, payloadBytesInPacket);

        if(-1 == SendVideoPacket(frameType, dataBuffer, payloadBytesInPacket +
                                 h2631998HeaderLength, rtpHeaderLength))
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
    WebRtc_Word32 payloadBytesToSend =
        sizeOfMBs[info.ptrNumOfMBs[numOfGOB]-1] / 8;
    WebRtc_UWord8 eBitLastByte = (WebRtc_UWord8)((8 -
        (sizeOfMBs[info.ptrNumOfMBs[numOfGOB]-1] % 8)) % 8);
    WebRtc_Word32 sBit = 0;
    WebRtc_Word32 firstMB = 0;
    WebRtc_UWord32 bitsRem = 0;
    WebRtc_UWord32 payloadBytesSent = 0;
    WebRtc_Word32 numOfMB = 0;
    WebRtc_Word32 prevOK = 0;

    // (Eventual sBit, eBit)
    WebRtc_UWord16 maxPayloadLengthH263MB = _rtpSender.MaxPayloadLength() -
        FECPacketOverhead() - rtpHeaderLength - h263HeaderLength - 2;
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
                if (WebRtc_Word32(sizeOfMBs[numOfMB] / 8 - payloadBytesSent) <
                    maxPayloadLengthH263MB)
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
        // First bit 1 == mode B, 10 000 000
        dataBuffer[rtpHeaderLength] = (WebRtc_UWord8)0x80;
        // Source format
        dataBuffer[rtpHeaderLength + 1] = (info.uiH263PTypeFmt) << 5;
        if (numOfGOB == 0)
        {
           // Quantization value for first MB in packet
            dataBuffer[rtpHeaderLength + 1] += info.pQuant;
        }
        if (numOfGOB > 0 && firstMB > 0)
        {
           // Quantization value for first MB in packet
           // (0 if packet begins w/ a GOB header)
            dataBuffer[rtpHeaderLength + 1] += info.ptrGQuant[numOfGOB];
        }
        // GOB #
        dataBuffer[rtpHeaderLength + 2] = numOfGOB << 3;
        // First MB in the packet
        dataBuffer[rtpHeaderLength + 2] += (WebRtc_UWord8)((firstMB >> 6)& 0x7);
        dataBuffer[rtpHeaderLength + 3] = (WebRtc_UWord8)(firstMB << 2);
        dataBuffer[rtpHeaderLength + 4] = (info.codecBits) << 4;
        // Horizontal motion vector
        dataBuffer[rtpHeaderLength + 4] += (hmv1[firstMB] & 0x7F) >> 3;
        dataBuffer[rtpHeaderLength + 5] = hmv1[firstMB] << 5;
        // Vertical motion vector
        dataBuffer[rtpHeaderLength + 5] += (vmv1[firstMB] & 0x7F) >> 2;
        dataBuffer[rtpHeaderLength + 6] = vmv1[firstMB] << 6;
        dataBuffer[rtpHeaderLength + 7] = 0;

        sBit = (8 - _eBit) % 8;

        if (payloadBytesToSend > maxPayloadLengthH263MB)
        {
            payloadBytesInPacket = (WebRtc_UWord16)prevOK;
            payloadBytesToSend -= payloadBytesInPacket;

            _rtpSender.BuildRTPheader(dataBuffer, payloadType, false,
                                      captureTimeStamp);

            _eBit = (WebRtc_UWord8)((8 - bitsRem) % 8);
        }
        else
        {
            payloadBytesInPacket = (WebRtc_UWord16)payloadBytesToSend;
            payloadBytesToSend = 0;

            if (numOfGOB == (info.numOfGOBs - 1))
            {
                _rtpSender.BuildRTPheader(dataBuffer, payloadType, true,
                                          captureTimeStamp);
                _eBit = 0;
            }
            else
            {
                _rtpSender.BuildRTPheader(dataBuffer, payloadType, false,
                                          captureTimeStamp);
                _eBit = eBitLastByte;
            }
        }


        if (sBit)
        {
            // Add last sent byte and put payload in packet
            dataBuffer[rtpHeaderLength] |= ((sBit & 0x7) << 3);
            dataBuffer[rtpHeaderLength + h263HeaderLength] = _savedByte;
            memcpy(&dataBuffer[rtpHeaderLength + h263HeaderLength + 1],
                   data, payloadBytesInPacket);
            h263HeaderLength++;
        } else
        {
            // Put payload in packet
            memcpy(&dataBuffer[rtpHeaderLength + h263HeaderLength],
                   data, payloadBytesInPacket);
        }
        if (_eBit)
        {
            // Save last byte to paste in next packet
            dataBuffer[rtpHeaderLength] |= (_eBit & 0x7);
            _savedByte = dataBuffer[rtpHeaderLength +
                                    h263HeaderLength +
                                    payloadBytesInPacket - 1];
        }
        if (-1 == SendVideoPacket(frameType,
                                  dataBuffer,
                                  payloadBytesInPacket + h263HeaderLength,
                                  rtpHeaderLength))
        {
            return -1;
        }

        data += payloadBytesInPacket;
        payloadBytesSent += payloadBytesInPacket;
    }
    return 0;
}

WebRtc_Word32
RTPSenderVideo::SendVP8(const FrameType frameType,
                        const WebRtc_Word8 payloadType,
                        const WebRtc_UWord32 captureTimeStamp,
                        const WebRtc_UWord8* payloadData,
                        const WebRtc_UWord32 payloadSize,
                        const RTPFragmentationHeader* fragmentation,
                        const RTPVideoTypeHeader* rtpTypeHdr)
{
    const WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();

    WebRtc_Word32 payloadBytesToSend = payloadSize;
    const WebRtc_UWord8* data = payloadData;

    WebRtc_UWord16 maxPayloadLengthVP8 = _rtpSender.MaxDataPayloadLength();

    assert(rtpTypeHdr);
    RtpFormatVp8 packetizer(data, payloadBytesToSend, rtpTypeHdr->VP8,
                            *fragmentation, kAggregate);

    bool last = false;
    _numberFirstPartition = 0;
    while (!last)
    {
        // Write VP8 Payload Descriptor and VP8 payload.
        WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE] = {0};
        int payloadBytesInPacket = 0;
        int packetStartPartition =
            packetizer.NextPacket(maxPayloadLengthVP8,
                                  &dataBuffer[rtpHeaderLength],
                                  &payloadBytesInPacket, &last);
        if (packetStartPartition == 0)
        {
            ++_numberFirstPartition;
        }
        else if (packetStartPartition < 0)
        {
            return -1;
        }

        // Write RTP header.
        // Set marker bit true if this is the last packet in frame.
        _rtpSender.BuildRTPheader(dataBuffer, payloadType, last,
            captureTimeStamp);
        if (-1 == SendVideoPacket(frameType, dataBuffer, payloadBytesInPacket,
            rtpHeaderLength))
        {
          WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                       "RTPSenderVideo::SendVP8 failed to send packet number"
                       " %d", _rtpSender.SequenceNumber());
        }
    }
    return 0;
}

void RTPSenderVideo::ProcessBitrate() {
  _videoBitrate.Process();
  _fecOverheadRate.Process();
}

WebRtc_UWord32 RTPSenderVideo::VideoBitrateSent() const {
  return _videoBitrate.BitrateLast();
}

WebRtc_UWord32 RTPSenderVideo::FecOverheadRate() const {
  return _fecOverheadRate.BitrateLast();
}

} // namespace webrtc
