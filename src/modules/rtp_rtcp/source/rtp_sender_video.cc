/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
    _retransmissionSettings(kRetransmitBaseLayer),

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
    _videoBitrate(clock) {
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

    _retransmissionSettings = kRetransmitBaseLayer;
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

WebRtc_Word32 RTPSenderVideo::RegisterVideoPayload(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_Word8 payloadType,
    const WebRtc_UWord32 maxBitRate,
    ModuleRTPUtility::Payload*& payload) {
  CriticalSectionScoped cs(_sendVideoCritsect);

  RtpVideoCodecTypes videoType = kRtpNoVideo;
  if (ModuleRTPUtility::StringCompare(payloadName, "VP8",3)) {
    videoType = kRtpVp8Video;
  } else if (ModuleRTPUtility::StringCompare(payloadName, "I420", 4)) {
    videoType = kRtpNoVideo;
  } else {
    return -1;
  }
  payload = new ModuleRTPUtility::Payload;
  payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
  strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
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
                                const WebRtc_UWord16 rtpHeaderLength,
                                StorageType storage)
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
        _rtpPacketListFec.push_back(ptrGenericFEC);
        // FEC can only protect up to kMaxMediaPackets packets
        if (_mediaPacketListFec.size() <
            ForwardErrorCorrection::kMaxMediaPackets)
        {
            _mediaPacketListFec.push_back(ptrGenericFEC->pkt);
        }

        // Last packet in frame
        if (markerBit)
        {

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

            std::list<ForwardErrorCorrection::Packet*> fecPacketList;
            retVal = _fec.GenerateFEC(_mediaPacketListFec,
                                      _fecProtectionFactor,
                                      _numberFirstPartition,
                                      _fecUseUepProtection,
                                      &fecPacketList);

            int fecOverheadSent = 0;
            int videoSent = 0;

            while(!_rtpPacketListFec.empty())
            {
                WebRtc_UWord8 newDataBuffer[IP_PACKET_SIZE];
                memset(newDataBuffer, 0, sizeof(newDataBuffer));

                RtpPacket* packetToSend = _rtpPacketListFec.front();

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

                _rtpPacketListFec.pop_front();
                // Check if _mediaPacketListFec is non-empty.
                // This list may be smaller than rtpPacketList, if the frame
                // has more than kMaxMediaPackets.
                if (!_mediaPacketListFec.empty()) {
                  _mediaPacketListFec.pop_front();
                }

                // Send normal packet with RED header
                int packetSuccess = _rtpSender.SendToNetwork(
                    newDataBuffer,
                    packetToSend->pkt->length - packetToSend->rtpHeaderLength +
                    REDForFECHeaderLength,
                    packetToSend->rtpHeaderLength,
                    storage);

                retVal |= packetSuccess;

                if (packetSuccess == 0)
                {
                    videoSent += packetToSend->pkt->length +
                        REDForFECHeaderLength;
                }

                delete packetToSend->pkt;
                delete packetToSend;
                packetToSend = NULL;
            }
            assert(_mediaPacketListFec.empty());
            assert(_rtpPacketListFec.empty());

            while(!fecPacketList.empty())
            {
                WebRtc_UWord8 newDataBuffer[IP_PACKET_SIZE];

                // Build FEC packets
                ForwardErrorCorrection::Packet* packetToSend = fecPacketList.front();

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

                fecPacketList.pop_front();

                // Invalid FEC packet
                assert(packetToSend->length != 0);

                StorageType storage = kDontRetransmit;
                if (_retransmissionSettings & kRetransmitFECPackets) {
                  storage = kAllowRetransmission;
                }

                // No marker bit on FEC packets, last media packet have the
                // marker send FEC packet with RED header
                int packetSuccess = _rtpSender.SendToNetwork(
                        newDataBuffer,
                        packetToSend->length + REDForFECHeaderLength,
                        lastMediaRtpHeader.length,
                        storage);

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
                                          rtpHeaderLength,
                                          storage);
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

    return _rtpSender.SendToNetwork(data, 0, length, kAllowRetransmission);
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
                                 rtpHeaderLength,
                                 kAllowRetransmission))
        {
            return -1;
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
                            maxPayloadLengthVP8, *fragmentation, kAggregate);

    StorageType storage = kAllowRetransmission;
    if (rtpTypeHdr->VP8.temporalIdx == 0 &&
        !(_retransmissionSettings & kRetransmitBaseLayer)) {
      storage = kDontRetransmit;
    }
    if (rtpTypeHdr->VP8.temporalIdx > 0 &&
        !(_retransmissionSettings & kRetransmitHigherLayers)) {
      storage = kDontRetransmit;
    }

    bool last = false;
    _numberFirstPartition = 0;
    while (!last)
    {
        // Write VP8 Payload Descriptor and VP8 payload.
        WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE] = {0};
        int payloadBytesInPacket = 0;
        int packetStartPartition =
            packetizer.NextPacket(&dataBuffer[rtpHeaderLength],
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
            rtpHeaderLength, storage))
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

int RTPSenderVideo::SelectiveRetransmissions() const {
  return _retransmissionSettings;
}

int RTPSenderVideo::SetSelectiveRetransmissions(uint8_t settings) {
  _retransmissionSettings = settings;
  return 0;
}

} // namespace webrtc
