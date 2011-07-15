/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_

#include "typedefs.h"
#include "common_types.h"               // Transport
#include "rtp_rtcp_config.h"

#include "rtp_rtcp_defines.h"
#include "rtp_utility.h"
#include "list_wrapper.h"

#include "video_codec_information.h"
#include "h263_information.h"
#include "forward_error_correction.h"
#include "Bitrate.h"

#include "rtp_sender.h"

namespace webrtc {
class CriticalSectionWrapper;
class RTPSenderVideo
{
public:
    RTPSenderVideo(const WebRtc_Word32 id, RTPSenderInterface* rtpSender);
    virtual ~RTPSenderVideo();

    WebRtc_Word32 Init();

    virtual void ChangeUniqueId(const WebRtc_Word32 id);

    virtual RtpVideoCodecTypes VideoCodecType() const;

    WebRtc_UWord16 FECPacketOverhead() const;

    WebRtc_Word32 RegisterVideoPayload(const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                     const WebRtc_Word8 payloadType,
                                     const WebRtc_UWord32 maxBitRate,
                                     ModuleRTPUtility::Payload*& payload);

    WebRtc_Word32 SendVideo(const RtpVideoCodecTypes videoType,
                          const FrameType frameType,
                          const WebRtc_Word8 payloadType,
                          const WebRtc_UWord32 captureTimeStamp,
                          const WebRtc_UWord8* payloadData,
                          const WebRtc_UWord32 payloadSize,
                          const RTPFragmentationHeader* fragmentation,
                          VideoCodecInformation* codecInfo,
                          const RTPVideoTypeHeader* rtpTypeHdr);

    WebRtc_Word32 SendRTPIntraRequest();

    void SetVideoCodecType(RtpVideoCodecTypes type);

    VideoCodecInformation* CodecInformationVideo();

    void SetMaxConfiguredBitrateVideo(const WebRtc_UWord32 maxBitrate);

    WebRtc_UWord32 MaxConfiguredBitrateVideo() const;

    WebRtc_Word32 SendPadData(const WebRtcRTPHeader* rtpHeader,
                              const WebRtc_UWord32 bytes);

    // FEC
    WebRtc_Word32 SetGenericFECStatus(const bool enable,
                                    const WebRtc_UWord8 payloadTypeRED,
                                    const WebRtc_UWord8 payloadTypeFEC);

    WebRtc_Word32 GenericFECStatus(bool& enable,
                                 WebRtc_UWord8& payloadTypeRED,
                                 WebRtc_UWord8& payloadTypeFEC) const;

    WebRtc_Word32 SetFECCodeRate(const WebRtc_UWord8 keyFrameCodeRate,
                                 const WebRtc_UWord8 deltaFrameCodeRate);

    WebRtc_Word32 SetFECUepProtection(const bool keyUseUepProtection,
                                      const bool deltaUseUepProtection);

protected:
    virtual WebRtc_Word32 SendVideoPacket(const FrameType frameType,
                                        const WebRtc_UWord8* dataBuffer,
                                        const WebRtc_UWord16 payloadLength,
                                        const WebRtc_UWord16 rtpHeaderLength);

private:
    WebRtc_Word32 SendGeneric(const WebRtc_Word8 payloadType,
                            const WebRtc_UWord32 captureTimeStamp,
                            const WebRtc_UWord8* payloadData,
                            const WebRtc_UWord32 payloadSize);

    WebRtc_Word32 SendH263(const FrameType frameType,
                         const WebRtc_Word8 payloadType,
                         const WebRtc_UWord32 captureTimeStamp,
                         const WebRtc_UWord8* payloadData,
                         const WebRtc_UWord32 payloadSize,
                         VideoCodecInformation* codecInfo);

    WebRtc_Word32 SendH2631998(const FrameType frameType,
                             const WebRtc_Word8 payloadType,
                             const WebRtc_UWord32 captureTimeStamp,
                             const WebRtc_UWord8* payloadData,
                             const WebRtc_UWord32 payloadSize,
                             VideoCodecInformation* codecInfo);

    WebRtc_Word32 SendMPEG4(const FrameType frameType,
                          const WebRtc_Word8 payloadType,
                          const WebRtc_UWord32 captureTimeStamp,
                          const WebRtc_UWord8* payloadData,
                          const WebRtc_UWord32 payloadSize);

    WebRtc_Word32 SendVP8(const FrameType frameType,
                        const WebRtc_Word8 payloadType,
                        const WebRtc_UWord32 captureTimeStamp,
                        const WebRtc_UWord8* payloadData,
                        const WebRtc_UWord32 payloadSize,
                        const RTPFragmentationHeader* fragmentation,
                        const RTPVideoTypeHeader* rtpTypeHdr);

    // MPEG 4
    WebRtc_Word32 FindMPEG4NALU(const WebRtc_UWord8* inData ,WebRtc_Word32 MaxPayloadLength);

    // H263
    WebRtc_Word32 SendH263MBs(const FrameType frameType,
                            const WebRtc_Word8 payloadType,
                            const WebRtc_UWord32 captureTimeStamp,
                            WebRtc_UWord8* dataBuffer,
                            const WebRtc_UWord8 *data,
                            const WebRtc_UWord16 rtpHeaderLength,
                            const WebRtc_UWord8 numOfGOB,
                            const H263Info& info,
                            const H263MBInfo& infoMB,
                            const WebRtc_Word32 offset);

private:
    WebRtc_Word32             _id;
    RTPSenderInterface&        _rtpSender;

    CriticalSectionWrapper&    _sendVideoCritsect;
    RtpVideoCodecTypes  _videoType;
    VideoCodecInformation*  _videoCodecInformation;
    WebRtc_UWord32            _maxBitrate;

    // FEC
    ForwardErrorCorrection  _fec;
    bool                    _fecEnabled;
    WebRtc_Word8              _payloadTypeRED;
    WebRtc_Word8              _payloadTypeFEC;
    WebRtc_UWord8             _codeRateKey;
    WebRtc_UWord8             _codeRateDelta;
    bool                      _useUepProtectionKey;
    bool                      _useUepProtectionDelta;
    WebRtc_UWord8             _fecProtectionFactor;
    bool                      _fecUseUepProtection;
    WebRtc_UWord32            _numberFirstPartition;
    ListWrapper               _mediaPacketListFec;
    ListWrapper               _rtpPacketListFec;

    // H263
    WebRtc_UWord8             _savedByte;
    WebRtc_UWord8             _eBit;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
