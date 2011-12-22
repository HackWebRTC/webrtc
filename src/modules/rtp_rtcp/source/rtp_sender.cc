/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib> // srand

#include "rtp_sender.h"

#include "critical_section_wrapper.h"
#include "trace.h"

#include "rtp_sender_audio.h"
#include "rtp_sender_video.h"

namespace webrtc {
RTPSender::RTPSender(const WebRtc_Word32 id,
                     const bool audio,
                     RtpRtcpClock* clock) :
    Bitrate(clock),
    _id(id),
    _audioConfigured(audio),
    _audio(NULL),
    _video(NULL),
    _sendCritsect(CriticalSectionWrapper::CreateCriticalSection()),
    _transportCritsect(CriticalSectionWrapper::CreateCriticalSection()),

    _transport(NULL),

    _sendingMedia(true), // Default to sending media

    _maxPayloadLength(IP_PACKET_SIZE-28), // default is IP/UDP
    _targetSendBitrate(0),
    _packetOverHead(28),

    _payloadType(-1),
    _payloadTypeMap(),

    _rtpHeaderExtensionMap(),
    _transmissionTimeOffset(0),

    _keepAliveIsActive(false),
    _keepAlivePayloadType(-1),
    _keepAliveLastSent(0),
    _keepAliveDeltaTimeSend(0),

    _storeSentPackets(false),
    _storeSentPacketsNumber(0),
    _prevSentPacketsCritsect(CriticalSectionWrapper::CreateCriticalSection()),
    _prevSentPacketsIndex(0),
    _ptrPrevSentPackets(NULL),
    _prevSentPacketsSeqNum(NULL),
    _prevSentPacketsLength(NULL),
    _prevSentPacketsResendTime(NULL),

    // NACK
    _nackByteCountTimes(),
    _nackByteCount(),
    _nackBitrate(clock),

    // statistics
    _packetsSent(0),
    _payloadBytesSent(0),

    // RTP variables
    _startTimeStampForced(false),
    _startTimeStamp(0),
    _ssrcDB(*SSRCDatabase::GetSSRCDatabase()),
    _remoteSSRC(0),
    _sequenceNumberForced(false),
    _sequenceNumber(0),
    _ssrcForced(false),
    _ssrc(0),
    _timeStamp(0),
    _CSRCs(0),
    _CSRC(),
    _includeCSRCs(true)
{
    memset(_nackByteCountTimes, 0, sizeof(_nackByteCountTimes));
    memset(_nackByteCount, 0, sizeof(_nackByteCount));

    memset(_CSRC, 0, sizeof(_CSRC));

    // we need to seed the random generator, otherwise we get 26500 each time, hardly a random value :)
    srand( (WebRtc_UWord32)_clock.GetTimeInMS() );

    _ssrc = _ssrcDB.CreateSSRC(); // can't be 0

    if(audio)
    {
        _audio = new RTPSenderAudio(id, &_clock, this);
    } else
    {
        _video = new RTPSenderVideo(id, &_clock, this);
    }
    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id, "%s created", __FUNCTION__);
}

RTPSender::~RTPSender()
{
    if(_remoteSSRC != 0)
    {
        _ssrcDB.ReturnSSRC(_remoteSSRC);
    }
    _ssrcDB.ReturnSSRC(_ssrc);

    SSRCDatabase::ReturnSSRCDatabase();
    delete _prevSentPacketsCritsect;
    delete _sendCritsect;
    delete _transportCritsect;

    // empty map
    bool loop = true;
    do
    {
        MapItem* item = _payloadTypeMap.First();
        if(item)
        {
            // delete
            ModuleRTPUtility::Payload* payload= ((ModuleRTPUtility::Payload*)item->GetItem());
            delete payload;

            // remove from map and delete Item
            _payloadTypeMap.Erase(item);
        } else
        {
            loop = false;
        }
    } while (loop);

    for(WebRtc_Word32 i=0; i< _storeSentPacketsNumber; i++)
    {
        if(_ptrPrevSentPackets[i])
        {
            delete [] _ptrPrevSentPackets[i];
            _ptrPrevSentPackets[i] = 0;
        }
    }
    delete [] _ptrPrevSentPackets;
    delete [] _prevSentPacketsSeqNum;
    delete [] _prevSentPacketsLength;
    delete [] _prevSentPacketsResendTime;

    delete _audio;
    delete _video;

    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, _id, "%s deleted", __FUNCTION__);
}

WebRtc_Word32
RTPSender::Init(const WebRtc_UWord32 remoteSSRC)
{
    CriticalSectionScoped cs(_sendCritsect);

    // reset to default generation
    _ssrcForced = false;
    _startTimeStampForced = false;

    // register a remote SSRC if we have it to avoid collisions
    if(remoteSSRC != 0)
    {
        if(_ssrc == remoteSSRC)
        {
            // collision detected
            _ssrc = _ssrcDB.CreateSSRC(); // can't be 0
        }
        _remoteSSRC = remoteSSRC;
        _ssrcDB.RegisterSSRC(remoteSSRC);
    }
    _sequenceNumber = rand() / (RAND_MAX / MAX_INIT_RTP_SEQ_NUMBER);
    _packetsSent = 0;
    _payloadBytesSent = 0;
    _packetOverHead = 28;

    _keepAlivePayloadType = -1;

    _rtpHeaderExtensionMap.Erase();

    bool loop = true;
    do
    {
        MapItem* item = _payloadTypeMap.First();
        if(item)
        {
            ModuleRTPUtility::Payload* payload= ((ModuleRTPUtility::Payload*)item->GetItem());
            delete payload;
            _payloadTypeMap.Erase(item);
        } else
        {
            loop = false;
        }
    } while (loop);

    memset(_CSRC, 0, sizeof(_CSRC));

    memset(_nackByteCount, 0, sizeof(_nackByteCount));
    memset(_nackByteCountTimes, 0, sizeof(_nackByteCountTimes));
    _nackBitrate.Init();

    SetStorePacketsStatus(false, 0);

    Bitrate::Init();

    if(_audioConfigured)
    {
        _audio->Init();
    } else
    {
        _video->Init();
    }
    return(0);
}

void
RTPSender::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
    if(_audioConfigured)
    {
        _audio->ChangeUniqueId(id);
    } else
    {
        _video->ChangeUniqueId(id);
    }
}

WebRtc_Word32
RTPSender::SetTargetSendBitrate(const WebRtc_UWord32 bits)
{
    _targetSendBitrate = (WebRtc_UWord16)(bits/1000);
    return 0;
}

WebRtc_UWord16
RTPSender::TargetSendBitrateKbit() const
{
    return _targetSendBitrate;
}

WebRtc_UWord16
RTPSender::ActualSendBitrateKbit() const
{
    return (WebRtc_UWord16) (Bitrate::BitrateNow()/1000);
}

WebRtc_UWord32
RTPSender::VideoBitrateSent() const {
  if (_video)
    return _video->VideoBitrateSent();
  else
    return 0;
}

WebRtc_UWord32
RTPSender::FecOverheadRate() const {
  if (_video)
    return _video->FecOverheadRate();
  else
    return 0;
}

WebRtc_UWord32
RTPSender::NackOverheadRate() const {
  return _nackBitrate.BitrateLast();
}

WebRtc_Word32
RTPSender::SetTransmissionTimeOffset(
    const WebRtc_Word32 transmissionTimeOffset)
{
    if (transmissionTimeOffset > (0x800000 - 1) ||
        transmissionTimeOffset < -(0x800000 - 1))  // Word24
    {
        return -1;
    }
    CriticalSectionScoped cs(_sendCritsect);
    _transmissionTimeOffset = transmissionTimeOffset;
    return 0;
}

WebRtc_Word32
RTPSender::RegisterRtpHeaderExtension(const RTPExtensionType type,
                                      const WebRtc_UWord8 id)
{
    CriticalSectionScoped cs(_sendCritsect);
    return _rtpHeaderExtensionMap.Register(type, id);
}

WebRtc_Word32
RTPSender::DeregisterRtpHeaderExtension(const RTPExtensionType type)
{
    CriticalSectionScoped cs(_sendCritsect);
    return _rtpHeaderExtensionMap.Deregister(type);
}

WebRtc_UWord16
RTPSender::RtpHeaderExtensionTotalLength() const
{
    CriticalSectionScoped cs(_sendCritsect);
    return _rtpHeaderExtensionMap.GetTotalLengthInBytes();
}

//can be called multiple times
WebRtc_Word32
RTPSender::RegisterPayload(const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                           const WebRtc_Word8 payloadNumber,
                           const WebRtc_UWord32 frequency,
                           const WebRtc_UWord8 channels,
                           const WebRtc_UWord32 rate)
{
    if (!payloadName)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
        return -1;
    }

    CriticalSectionScoped cs(_sendCritsect);

    if(payloadNumber == _keepAlivePayloadType)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id, "invalid state", __FUNCTION__);
        return -1;
    }

    MapItem* item = _payloadTypeMap.Find(payloadNumber);
    if( NULL != item)
    {
        // we already use this payload type

        ModuleRTPUtility::Payload* payload = (ModuleRTPUtility::Payload*)item->GetItem();
        assert(payload);

        // check if it's the same as we already have
        WebRtc_Word32 payloadNameLength = (WebRtc_Word32)strlen(payloadName);
        WebRtc_Word32 nameLength = (WebRtc_Word32)strlen(payload->name);
        if(payloadNameLength == nameLength && ModuleRTPUtility::StringCompare(payload->name, payloadName, nameLength))
        {
            if(_audioConfigured && payload->audio &&
                payload->typeSpecific.Audio.frequency == frequency &&
                (payload->typeSpecific.Audio.rate == rate || payload->typeSpecific.Audio.rate == 0 || rate == 0))
            {
                payload->typeSpecific.Audio.rate = rate; // Ensure that we update the rate if new or old is zero
                return 0;
            }
            if(!_audioConfigured && !payload->audio)
            {
                return 0;
            }
        }
        return -1;
    }

    WebRtc_Word32 retVal = -1;
    ModuleRTPUtility::Payload* payload = NULL;

    if(_audioConfigured)
    {
        retVal = _audio->RegisterAudioPayload(payloadName, payloadNumber, frequency, channels, rate, payload);
    } else
    {
        retVal = _video->RegisterVideoPayload(payloadName, payloadNumber, rate, payload);
    }
    if(payload)
    {
        _payloadTypeMap.Insert(payloadNumber, payload);
    }
    return retVal;
}

WebRtc_Word32
RTPSender::DeRegisterSendPayload(const WebRtc_Word8 payloadType)
{
    CriticalSectionScoped lock(_sendCritsect);

    MapItem* item = _payloadTypeMap.Find(payloadType);
    if( NULL != item)
    {
        ModuleRTPUtility::Payload* payload = (ModuleRTPUtility::Payload*)item->GetItem();
        delete payload;

        _payloadTypeMap.Erase(item);
        return 0;
    }
    return -1;
}


WebRtc_Word8 RTPSender::SendPayloadType() const
{
    return _payloadType;
}


int RTPSender::SendPayloadFrequency() const
{
    return _audio->AudioFrequency();
}


//  See http://www.ietf.org/internet-drafts/draft-ietf-avt-app-rtp-keepalive-04.txt
//  for details about this method. Only Section 4.6 is implemented so far.
bool
RTPSender::RTPKeepalive() const
{
    return _keepAliveIsActive;
}

WebRtc_Word32
RTPSender::RTPKeepaliveStatus(bool* enable,
                              WebRtc_Word8* unknownPayloadType,
                              WebRtc_UWord16* deltaTransmitTimeMS) const
{
    CriticalSectionScoped cs(_sendCritsect);

    if(enable)
    {
        *enable = _keepAliveIsActive;
    }
    if(unknownPayloadType)
    {
        *unknownPayloadType = _keepAlivePayloadType;
    }
    if(deltaTransmitTimeMS)
    {
        *deltaTransmitTimeMS =_keepAliveDeltaTimeSend;
    }
    return 0;
}

WebRtc_Word32
RTPSender::EnableRTPKeepalive( const WebRtc_Word8 unknownPayloadType,
                               const WebRtc_UWord16 deltaTransmitTimeMS)
{
    CriticalSectionScoped cs(_sendCritsect);

    if( NULL != _payloadTypeMap.Find(unknownPayloadType))
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
        return -1;
    }

    _keepAliveIsActive = true;
    _keepAlivePayloadType = unknownPayloadType;
    _keepAliveLastSent = _clock.GetTimeInMS();
    _keepAliveDeltaTimeSend = deltaTransmitTimeMS;
    return 0;
}

WebRtc_Word32
RTPSender::DisableRTPKeepalive()
{
    _keepAliveIsActive = false;
    return 0;
}

bool
RTPSender::TimeToSendRTPKeepalive() const
{
    CriticalSectionScoped cs(_sendCritsect);

    bool timeToSend(false);

    WebRtc_UWord32 dT = _clock.GetTimeInMS() - _keepAliveLastSent;
    if (dT > _keepAliveDeltaTimeSend)
    {
        timeToSend = true;
    }
    return timeToSend;
}

// ----------------------------------------------------------------------------
//  From the RFC draft:
//
//  4.6.  RTP Packet with Unknown Payload Type
//
//     The application sends an RTP packet of 0 length with a dynamic
//     payload type that has not been negotiated by the peers (e.g. not
//     negotiated within the SDP offer/answer, and thus not mapped to any
//     media format).
//
//     The sequence number is incremented by one for each packet, as it is
//     sent within the same RTP session as the actual media.  The timestamp
//     contains the same value a media packet would have at this time.  The
//     marker bit is not significant for the keepalive packets and is thus
//     set to zero.
//
//     Normally the peer will ignore this packet, as RTP [RFC3550] states
//     that "a receiver MUST ignore packets with payload types that it does
//     not understand".
//
//     Cons:
//     o  [RFC4566] and [RFC3264] mandate not to send media with inactive
//        and recvonly attributes, however this is mitigated as no real
//        media is sent with this mechanism.
//
//     Recommendation:
//     o  This method should be used for RTP keepalive.
//
//  7.  Timing and Transport Considerations
//
//     An application supporting this specification must transmit keepalive
//     packets every Tr seconds during the whole duration of the media
//     session.  Tr SHOULD be configurable, and otherwise MUST default to 15
//     seconds.
//
//     Keepalives packets within a particular RTP session MUST use the tuple
//     (source IP address, source TCP/UDP ports, target IP address, target
//     TCP/UDP Port) of the regular RTP packets.
//
//     The agent SHOULD only send RTP keepalive when it does not send
//     regular RTP packets.
//
//  http://www.ietf.org/internet-drafts/draft-ietf-avt-app-rtp-keepalive-04.txt
// ----------------------------------------------------------------------------

WebRtc_Word32
RTPSender::SendRTPKeepalivePacket()
{
    // RFC summary:
    //
    // - Send an RTP packet of 0 length;
    // - dynamic payload type has not been negotiated (not mapped to any media);
    // - sequence number is incremented by one for each packet;
    // - timestamp contains the same value a media packet would have at this time;
    // - marker bit is set to zero.

    WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE];
    WebRtc_UWord16 rtpHeaderLength = 12;
    {
        CriticalSectionScoped cs(_sendCritsect);

        WebRtc_UWord32 now = _clock.GetTimeInMS();
        WebRtc_UWord32 dT = now -_keepAliveLastSent; // delta time in MS

        WebRtc_UWord32 freqKHz = 90; // video
        if(_audioConfigured)
        {
            freqKHz = _audio->AudioFrequency()/1000;
        }
        WebRtc_UWord32 dSamples = dT*freqKHz;

        // set timestamp
        _timeStamp += dSamples;
        _keepAliveLastSent = now;

        rtpHeaderLength = RTPHeaderLength();

        // correct seq num, time stamp and payloadtype
        BuildRTPheader(dataBuffer, _keepAlivePayloadType, false, 0, false);
    }

    return SendToNetwork(dataBuffer, 0, rtpHeaderLength, kAllowRetransmission);
}

WebRtc_Word32
RTPSender::SetMaxPayloadLength(const WebRtc_UWord16 maxPayloadLength, const WebRtc_UWord16 packetOverHead)
{
    // sanity check
    if(maxPayloadLength < 100 || maxPayloadLength > IP_PACKET_SIZE)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
        return -1;
    }
    if(maxPayloadLength > _maxPayloadLength)
    {
        CriticalSectionScoped lock(_prevSentPacketsCritsect);
        if(_storeSentPackets)
        {
            // we need to free the memmory allocated for storing sent packets
            // will be allocated in SendToNetwork
            for(WebRtc_Word32 i=0; i< _storeSentPacketsNumber; i++)
            {
                if(_ptrPrevSentPackets[i])
                {
                    delete [] _ptrPrevSentPackets[i];
                    _ptrPrevSentPackets[i] = NULL;
                }
            }
        }
    }

    CriticalSectionScoped cs(_sendCritsect);
    _maxPayloadLength = maxPayloadLength;
    _packetOverHead = packetOverHead;

    WEBRTC_TRACE(kTraceInfo, kTraceRtpRtcp, _id, "SetMaxPayloadLength to %d.", maxPayloadLength);
    return 0;
}

WebRtc_UWord16
RTPSender::MaxDataPayloadLength() const
{
    if(_audioConfigured)
    {
        return _maxPayloadLength - RTPHeaderLength();
    } else
    {
        return _maxPayloadLength - RTPHeaderLength() - _video->FECPacketOverhead(); // Include the FEC/ULP/RED overhead.
    }
}

WebRtc_UWord16
RTPSender::MaxPayloadLength() const
{
    return _maxPayloadLength;
}

WebRtc_UWord16
RTPSender::PacketOverHead() const
{
    return _packetOverHead;
}

WebRtc_Word32
RTPSender::CheckPayloadType(const WebRtc_Word8 payloadType,
                            RtpVideoCodecTypes& videoType)
{
    CriticalSectionScoped cs(_sendCritsect);

    if(payloadType < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "\tinvalid payloadType (%d)", payloadType);
        return -1;
    }

    if(_audioConfigured)
    {
        WebRtc_Word8 redPlType = -1;
        if(_audio->RED(redPlType) == 0)
        {
            // we have configured RED
            if(redPlType == payloadType)
            {
                // and it's a match
                return 0;
            }
        }
    }

    if(_payloadType != payloadType)
    {
        MapItem* item = _payloadTypeMap.Find(payloadType);
        if( NULL == item)
        {
            WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "\tpayloadType:%d not registered", payloadType);
            return -1;
        }
        _payloadType = payloadType;
        ModuleRTPUtility::Payload* payload = (ModuleRTPUtility::Payload*)item->GetItem();
        if(payload)
        {
            if(payload->audio)
            {
                if(_audioConfigured)
                {
                    // Extract payload frequency
                    int payloadFreqHz;
                    if(ModuleRTPUtility::StringCompare(payload->name,"g722",4)&&
                        (payload->name[4] == 0)) //Check that strings end there, g722.1...
                    {
                        // Special case for G.722, bug in spec
                        payloadFreqHz=8000;
                    }
                    else
                    {
                        payloadFreqHz=payload->typeSpecific.Audio.frequency;
                    }

                    //we don't do anything if it's CN
                    if((_audio->AudioFrequency() != payloadFreqHz)&&
                        (!ModuleRTPUtility::StringCompare(payload->name,"cn",2)))
                    {
                        _audio->SetAudioFrequency(payloadFreqHz);
                        // We need to correct the timestamp again,
                        // since this might happen after we've set it
                        WebRtc_UWord32 RTPtime =
                            ModuleRTPUtility::GetCurrentRTP(&_clock, payloadFreqHz);
                        SetStartTimestamp(RTPtime);
                        // will be ignored if it's already configured via API
                    }
                }
            }else
            {
                if(!_audioConfigured)
                {
                    _video->SetVideoCodecType(payload->typeSpecific.Video.videoCodecType);
                    videoType = payload->typeSpecific.Video.videoCodecType;
                    _video->SetMaxConfiguredBitrateVideo(payload->typeSpecific.Video.maxRate);
                }
            }
        }
    } else
    {
        if(!_audioConfigured)
        {
            videoType = _video->VideoCodecType();
        }
    }
    return 0;
}

WebRtc_Word32
RTPSender::SendOutgoingData(const FrameType frameType,
                            const WebRtc_Word8 payloadType,
                            const WebRtc_UWord32 captureTimeStamp,
                            const WebRtc_UWord8* payloadData,
                            const WebRtc_UWord32 payloadSize,
                            const RTPFragmentationHeader* fragmentation,
                            VideoCodecInformation* codecInfo,
                            const RTPVideoTypeHeader* rtpTypeHdr)
{
    {
        // Drop this packet if we're not sending media packets
        CriticalSectionScoped cs(_sendCritsect);
        if (!_sendingMedia)
        {
            return 0;
        }
    }
    RtpVideoCodecTypes videoType;
    if(CheckPayloadType(payloadType, videoType) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument failed to find payloadType:%d", __FUNCTION__, payloadType);
        return -1;
    }
    // update keepalive so that we don't trigger keepalive messages while sending data
    _keepAliveLastSent = _clock.GetTimeInMS();

    if(_audioConfigured)
    {
        // assert video frameTypes
        assert(frameType == kAudioFrameSpeech ||
               frameType == kAudioFrameCN ||
               frameType == kFrameEmpty);

        return _audio->SendAudio(frameType, payloadType, captureTimeStamp, payloadData, payloadSize,fragmentation);
    } else
    {
        // assert audio frameTypes
        assert(frameType == kVideoFrameKey ||
               frameType == kVideoFrameDelta ||
               frameType == kVideoFrameGolden ||
               frameType == kVideoFrameAltRef);

        return _video->SendVideo(videoType,
                                 frameType,
                                 payloadType,
                                 captureTimeStamp,
                                 payloadData,
                                 payloadSize,
                                 fragmentation,
                                 codecInfo,
                                 rtpTypeHdr);
    }
}

WebRtc_Word32
RTPSender::SetStorePacketsStatus(const bool enable, const WebRtc_UWord16 numberToStore)
{
    CriticalSectionScoped lock(_prevSentPacketsCritsect);

    if(enable)
    {
        if(_storeSentPackets)
        {
            // already enabled
            return -1;
        }
        if(numberToStore > 0)
        {
            _storeSentPackets = enable;
            _storeSentPacketsNumber = numberToStore;

            _ptrPrevSentPackets = new WebRtc_Word8*[numberToStore],
            _prevSentPacketsSeqNum = new WebRtc_UWord16[numberToStore];
            _prevSentPacketsLength = new WebRtc_UWord16[numberToStore];
            _prevSentPacketsResendTime = new WebRtc_UWord32[numberToStore];

            memset(_ptrPrevSentPackets,0, sizeof(WebRtc_Word8*)*numberToStore);
            memset(_prevSentPacketsSeqNum,0, sizeof(WebRtc_UWord16)*numberToStore);
            memset(_prevSentPacketsLength,0, sizeof(WebRtc_UWord16)*numberToStore);
            memset(_prevSentPacketsResendTime,0,sizeof(WebRtc_UWord32)*numberToStore);
        } else
        {
            // storing 0 packets does not make sence
            return -1;
        }
    } else
    {
        _storeSentPackets = enable;
        if(_storeSentPacketsNumber > 0)
        {
            for(WebRtc_Word32 i=0; i< _storeSentPacketsNumber; i++)
            {
                if(_ptrPrevSentPackets[i])
                {
                    delete [] _ptrPrevSentPackets[i];
                    _ptrPrevSentPackets[i] = 0;
                }
            }
            delete [] _ptrPrevSentPackets;
            delete [] _prevSentPacketsSeqNum;
            delete [] _prevSentPacketsLength;
            delete [] _prevSentPacketsResendTime;

            _ptrPrevSentPackets = NULL;
            _prevSentPacketsSeqNum = NULL;
            _prevSentPacketsLength = NULL;
            _prevSentPacketsResendTime = NULL;

            _storeSentPacketsNumber = 0;
        }
    }
    return 0;
}

bool
RTPSender::StorePackets() const
{
    return _storeSentPackets;
}

WebRtc_Word32
RTPSender::ReSendToNetwork(WebRtc_UWord16 packetID,
                           WebRtc_UWord32 minResendTime)
{
#ifdef DEBUG_RTP_SEQUENCE_NUMBER
    char str[256];
    sprintf(str,"Re-Send sequenceNumber %d\n", packetID) ;
    OutputDebugString(str);
#endif

    WebRtc_Word32 i = -1;
    WebRtc_Word32 length = 0;
    WebRtc_Word32 index =0;
    WebRtc_UWord8 dataBuffer[IP_PACKET_SIZE];

    {
        CriticalSectionScoped lock(_prevSentPacketsCritsect);

        WebRtc_UWord16 seqNum = 0;
        if(_storeSentPackets)
        {
            if(_prevSentPacketsIndex)
            {
                seqNum = _prevSentPacketsSeqNum[_prevSentPacketsIndex-1];
            }else
            {
                seqNum = _prevSentPacketsSeqNum[_storeSentPacketsNumber-1];
            }
            index = (_prevSentPacketsIndex-1) - (seqNum - packetID);
            if (index >= 0 && index < _storeSentPacketsNumber)
            {
                seqNum = _prevSentPacketsSeqNum[index];
            }
            if(seqNum != packetID)
            {
                //we did not found a match, search all
                for (WebRtc_Word32 m = 0; m < _storeSentPacketsNumber ;m++)
                {
                    if(_prevSentPacketsSeqNum[m] == packetID)
                    {
                        index = m;
                        seqNum = _prevSentPacketsSeqNum[index];
                        break;
                    }
                }
            }
            if(seqNum == packetID)
            {
                WebRtc_UWord32 timeNow= _clock.GetTimeInMS();
                if(minResendTime>0 && (timeNow-_prevSentPacketsResendTime[index]<minResendTime))
                {
                    // No point in sending the packet again yet. Get out of here
                    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id, "Skipping to resend RTP packet %d because it was just resent", seqNum);
                    return 0;
                }

                length = _prevSentPacketsLength[index];

                if(length > _maxPayloadLength || _ptrPrevSentPackets[index] == 0)
                {
                    WEBRTC_TRACE(
                        kTraceWarning, kTraceRtpRtcp, _id,
                        "Failed to resend seqNum %u: length = %d index = %d",
                        seqNum, length, index);
                    return -1;
                }
            } else
            {
                WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                             "No match for resending seqNum %u and packetId %u",
                             seqNum, packetID);
                return -1;
            }
        }
        if (length == 0)
        {
            // This is a valid case since packets which we decide not to
            // retransmit are stored but with length zero.
            return 0;
        }

        // copy to local buffer for callback
        memcpy(dataBuffer, _ptrPrevSentPackets[index], length);
    }
    {
        CriticalSectionScoped lock(_transportCritsect);
        if(_transport)
        {
            i = _transport->SendPacket(_id, dataBuffer, length);
        }
    }
    if(i > 0)
    {
        CriticalSectionScoped cs(_sendCritsect);

        Bitrate::Update(i);

        _packetsSent++;

        // we on purpose don't add to _payloadBytesSent since this is a re-transmit and not new payload data
    }
    if(_storeSentPackets && i > 0)
    {
        CriticalSectionScoped lock(_prevSentPacketsCritsect);

        if(_prevSentPacketsSeqNum[index] == packetID) // Make sure the  packet is still in the array
        {
            // Store the time when the frame was last resent.
            _prevSentPacketsResendTime[index]= _clock.GetTimeInMS();
        }
        return i; //bytes sent over network
    }
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                 "Transport failed to resend packetID %u", packetID);
    return -1;
}

int RTPSender::SelectiveRetransmissions() const {
  if (!_video) return -1;
  return _video->SelectiveRetransmissions();
}

int RTPSender::SetSelectiveRetransmissions(uint8_t settings) {
  if (!_video) return -1;
  return _video->SetSelectiveRetransmissions(settings);
}

void
RTPSender::OnReceivedNACK(const WebRtc_UWord16 nackSequenceNumbersLength,
                          const WebRtc_UWord16* nackSequenceNumbers,
                          const WebRtc_UWord16 avgRTT)
{
    const WebRtc_UWord32 now = _clock.GetTimeInMS();
    WebRtc_UWord32 bytesReSent = 0;

     // Enough bandwith to send NACK?
    if(ProcessNACKBitRate(now))
    {
        for (WebRtc_UWord16 i = 0; i < nackSequenceNumbersLength; ++i)
        {
            const WebRtc_Word32 bytesSent = ReSendToNetwork(nackSequenceNumbers[i],
                                                          5+avgRTT);
            if (bytesSent > 0)
            {
                bytesReSent += bytesSent;

            } else if(bytesSent==0)
            {
                continue; // The packet has previously been resent. Try resending next packet in the list.

            } else if(bytesSent<0) // Failed to send one Sequence number. Give up the rest in this nack.
            {
                WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id, "Failed resending RTP packet %d, Discard rest of NACK RTP packets", nackSequenceNumbers[i]);
                break;
            }
            // delay bandwidth estimate (RTT * BW)
            if(TargetSendBitrateKbit() != 0 && avgRTT)
            {
                if(bytesReSent > (WebRtc_UWord32)(TargetSendBitrateKbit() * avgRTT)>>3 ) // kbits/s * ms= bits/8 = bytes
                {
                    break; // ignore the rest of the packets in the list
                }
            }
        }
        if (bytesReSent > 0)
        {
            UpdateNACKBitRate(bytesReSent,now); // Update the nack bit rate
            _nackBitrate.Update(bytesReSent);
        }
    }else
    {
        WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id, "NACK bitrate reached. Skipp sending NACK response. Target %d",TargetSendBitrateKbit());
    }
}

/**
*    @return true if the nack bitrate is lower than the requested max bitrate
*/
bool
RTPSender::ProcessNACKBitRate(const WebRtc_UWord32 now)
{
    WebRtc_UWord32 num = 0;
    WebRtc_Word32 byteCount = 0;
    const WebRtc_UWord32 avgInterval=1000;

    CriticalSectionScoped cs(_sendCritsect);

    if(_targetSendBitrate == 0)
    {
        return true;
    }

    for(num = 0; num < NACK_BYTECOUNT_SIZE; num++)
    {
        if((now - _nackByteCountTimes[num]) > avgInterval)
        {
            // don't use data older than 1sec
            break;
        } else
        {
            byteCount += _nackByteCount[num];
        }
    }
    WebRtc_Word32 timeInterval = avgInterval;
    if (num == NACK_BYTECOUNT_SIZE)
    {
        // More than NACK_BYTECOUNT_SIZE nack messages has been received
        // during the last msgInterval
        timeInterval = now - _nackByteCountTimes[num-1];
        if(timeInterval < 0)
        {
            timeInterval = avgInterval;
        }
    }
    return (byteCount*8) < (_targetSendBitrate * timeInterval);
}

void
RTPSender::UpdateNACKBitRate(const WebRtc_UWord32 bytes,
                             const WebRtc_UWord32 now)
{
    CriticalSectionScoped cs(_sendCritsect);

    // save bitrate statistics
    if(bytes > 0)
    {
        if(now == 0)
        {
            // add padding length
            _nackByteCount[0] += bytes;
        } else
        {
            if(_nackByteCountTimes[0] == 0)
            {
                // first no shift
            } else
            {
                // shift
                for(int i = (NACK_BYTECOUNT_SIZE-2); i >= 0 ; i--)
                {
                    _nackByteCount[i+1] = _nackByteCount[i];
                    _nackByteCountTimes[i+1] = _nackByteCountTimes[i];
                }
            }
            _nackByteCount[0] = bytes;
            _nackByteCountTimes[0] = now;
        }
    }
}

WebRtc_Word32
RTPSender::SendToNetwork(const WebRtc_UWord8* buffer,
                         const WebRtc_UWord16 length,
                         const WebRtc_UWord16 rtpLength,
                         const StorageType storage)
{
    WebRtc_Word32 retVal = -1;
    // sanity
    if(length + rtpLength > _maxPayloadLength)
    {
        return -1;
    }

    // Make sure the packet is big enough for us to parse the sequence number.
    assert(length + rtpLength > 3);
    // Parse the sequence number from the RTP header.
    WebRtc_UWord16 sequenceNumber = (buffer[2] << 8) + buffer[3];
    switch (storage) {
      case kAllowRetransmission:
        StorePacket(buffer, length + rtpLength, sequenceNumber);
        break;
      case kDontRetransmit:
        // Store an empty packet. Won't be retransmitted if NACKed.
        StorePacket(NULL, 0, sequenceNumber);
        break;
      case kDontStore:
        break;
      default:
        assert(false);
    }
    // Send packet
    {
        CriticalSectionScoped cs(_transportCritsect);
        if(_transport)
        {
            retVal = _transport->SendPacket(_id, buffer, length + rtpLength);
        }
    }
    // success?
    if(retVal > 0)
    {
        CriticalSectionScoped cs(_sendCritsect);

        Bitrate::Update(retVal);

        _packetsSent++;

        if(retVal > rtpLength)
        {
            _payloadBytesSent += retVal-rtpLength;
        }
        return 0;
    }
    return -1;
}

void RTPSender::StorePacket(const uint8_t* buffer, uint16_t length,
                            uint16_t sequence_number) {
  // Store packet to be used for NACK.
  CriticalSectionScoped lock(_prevSentPacketsCritsect);
  if(_storeSentPackets) {
    if(_ptrPrevSentPackets[0] == NULL) {
      for(WebRtc_Word32 i = 0; i < _storeSentPacketsNumber; i++) {
          _ptrPrevSentPackets[i] = new char[_maxPayloadLength];
          memset(_ptrPrevSentPackets[i], 0, _maxPayloadLength);
      }
    }

    if (buffer != NULL && length > 0) {
      memcpy(_ptrPrevSentPackets[_prevSentPacketsIndex], buffer, length);
    }
    _prevSentPacketsSeqNum[_prevSentPacketsIndex] = sequence_number;
    _prevSentPacketsLength[_prevSentPacketsIndex] = length;
    // Packet has not been re-sent.
    _prevSentPacketsResendTime[_prevSentPacketsIndex] = 0;
    _prevSentPacketsIndex++;
    if(_prevSentPacketsIndex >= _storeSentPacketsNumber) {
      _prevSentPacketsIndex = 0;
    }
  }
}

void
RTPSender::ProcessBitrate()
{
    CriticalSectionScoped cs(_sendCritsect);

    Bitrate::Process();
    _nackBitrate.Process();

    if (_audioConfigured)
      return;
    _video->ProcessBitrate();
}

WebRtc_UWord16
RTPSender::RTPHeaderLength() const
{
    WebRtc_UWord16 rtpHeaderLength = 12;

    if(_includeCSRCs)
    {
        rtpHeaderLength += sizeof(WebRtc_UWord32)*_CSRCs;
    }
    rtpHeaderLength += RtpHeaderExtensionTotalLength();

    return rtpHeaderLength;
}

WebRtc_UWord16
RTPSender::IncrementSequenceNumber()
{
    CriticalSectionScoped cs(_sendCritsect);
    return _sequenceNumber++;
}

WebRtc_Word32
RTPSender::ResetDataCounters()
{
    _packetsSent = 0;
    _payloadBytesSent = 0;

    return 0;
}

// number of sent RTP packets
// dont use critsect to avoid potental deadlock
WebRtc_UWord32
RTPSender::Packets() const
{
    return _packetsSent;
}

// number of sent RTP bytes
// dont use critsect to avoid potental deadlock
WebRtc_UWord32
RTPSender::Bytes() const
{
    return _payloadBytesSent;
}

WebRtc_Word32
RTPSender::BuildRTPheader(WebRtc_UWord8* dataBuffer,
                          const WebRtc_Word8 payloadType,
                          const bool markerBit,
                          const WebRtc_UWord32 captureTimeStamp,
                          const bool timeStampProvided,
                          const bool incSequenceNumber)
{
    assert(payloadType>=0);

    CriticalSectionScoped cs(_sendCritsect);

    dataBuffer[0] = static_cast<WebRtc_UWord8>(0x80);            // version 2
    dataBuffer[1] = static_cast<WebRtc_UWord8>(payloadType);
    if (markerBit)
    {
        dataBuffer[1] |= kRtpMarkerBitMask;  // MarkerBit is set
    }

    if(timeStampProvided)
    {
        _timeStamp = _startTimeStamp + captureTimeStamp;
    } else
    {
        // make a unique time stamp
        // used for inband signaling
        // we can't inc by the actual time, since then we increase the risk of back timing
        _timeStamp++;
    }

    ModuleRTPUtility::AssignUWord16ToBuffer(dataBuffer+2, _sequenceNumber);
    ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer+4, _timeStamp);
    ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer+8, _ssrc);

    WebRtc_Word32 rtpHeaderLength = 12;

    // Add the CSRCs if any
    if (_includeCSRCs && _CSRCs > 0)
    {
        if(_CSRCs > kRtpCsrcSize)
        {
            // error
            assert(false);
            return -1;
        }
        WebRtc_UWord8* ptr = &dataBuffer[rtpHeaderLength];
        for (WebRtc_UWord32 i = 0; i < _CSRCs; ++i)
        {
            ModuleRTPUtility::AssignUWord32ToBuffer(ptr, _CSRC[i]);
            ptr +=4;
        }
        dataBuffer[0] = (dataBuffer[0]&0xf0) | _CSRCs;

        // Update length of header
        rtpHeaderLength += sizeof(WebRtc_UWord32)*_CSRCs;
    }
    {
        _sequenceNumber++; // prepare for next packet
    }

    WebRtc_UWord16 len = BuildRTPHeaderExtension(dataBuffer + rtpHeaderLength);
    if (len)
    {
      dataBuffer[0] |= 0x10;  // set eXtension bit
      rtpHeaderLength += len;
    }

    return rtpHeaderLength;
}

WebRtc_UWord16
RTPSender::BuildRTPHeaderExtension(WebRtc_UWord8* dataBuffer) const
{
    if (_rtpHeaderExtensionMap.Size() <= 0) {
       return 0;
    }

    /* RTP header extension, RFC 3550.
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      defined by profile       |           length              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        header extension                       |
    |                             ....                              |
    */

    const WebRtc_UWord32 kPosLength = 2;
    const WebRtc_UWord32 kHeaderLength = RTP_ONE_BYTE_HEADER_LENGTH_IN_BYTES;

    // Add extension ID (0xBEDE).
    ModuleRTPUtility::AssignUWord16ToBuffer(dataBuffer,
                                            RTP_ONE_BYTE_HEADER_EXTENSION);

    // Add extensions.
    WebRtc_UWord16 total_block_length = 0;

    RTPExtensionType type = _rtpHeaderExtensionMap.First();
    while (type != NONE)
    {
        WebRtc_UWord8 block_length = 0;
        if (type == TRANSMISSION_TIME_OFFSET)
        {
            block_length = BuildTransmissionTimeOffsetExtension(
                dataBuffer + kHeaderLength + total_block_length);
        }
        total_block_length += block_length;
        type = _rtpHeaderExtensionMap.Next(type);
    }

    if (total_block_length == 0)
    {
        // No extension added.
        return 0;
    }

    // Set header length (in number of Word32, header excluded).
    assert(total_block_length % 4 == 0);
    ModuleRTPUtility::AssignUWord16ToBuffer(dataBuffer + kPosLength,
                                            total_block_length / 4);

    // Total added length.
    return kHeaderLength + total_block_length;
}

WebRtc_UWord8
RTPSender::BuildTransmissionTimeOffsetExtension(WebRtc_UWord8* dataBuffer) const
{
   // From RFC 5450: Transmission Time Offsets in RTP Streams.
   //
   // The transmission time is signaled to the receiver in-band using the
   // general mechanism for RTP header extensions [RFC5285]. The payload
   // of this extension (the transmitted value) is a 24-bit signed integer.
   // When added to the RTP timestamp of the packet, it represents the
   // "effective" RTP transmission time of the packet, on the RTP
   // timescale.
   //
   // The form of the transmission offset extension block:
   //
   //    0                   1                   2                   3
   //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   //   |  ID   | len=2 |              transmission offset              |
   //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    // Get id defined by user.
    WebRtc_UWord8 id;
    if (_rtpHeaderExtensionMap.GetId(TRANSMISSION_TIME_OFFSET, &id) != 0) {
      // Not registered.
      return 0;
    }

    int pos = 0;
    const WebRtc_UWord8 len = 2;
    dataBuffer[pos++] = (id << 4) + len;
    ModuleRTPUtility::AssignUWord24ToBuffer(dataBuffer + pos,
                                            _transmissionTimeOffset);
    pos += 3;
    assert(pos == TRANSMISSION_TIME_OFFSET_LENGTH_IN_BYTES);
    return TRANSMISSION_TIME_OFFSET_LENGTH_IN_BYTES;
}

WebRtc_Word32
RTPSender::RegisterSendTransport(Transport* transport)
{
     CriticalSectionScoped cs(_transportCritsect);
    _transport = transport;
    return 0;
}

void
RTPSender::SetSendingStatus(const bool enabled)
{
    if(enabled)
    {
        WebRtc_UWord32 freq;
        if(_audioConfigured)
        {
            WebRtc_UWord32 frequency = _audio->AudioFrequency();

            // sanity
            switch(frequency)
            {
            case 8000:
            case 12000:
            case 16000:
            case 24000:
            case 32000:
                break;
            default:
                assert(false);
                return;
            }
            freq = frequency;
        } else
        {
            freq = 90000; // 90 KHz for all video
        }
        WebRtc_UWord32 RTPtime = ModuleRTPUtility::GetCurrentRTP(&_clock, freq);

        SetStartTimestamp(RTPtime); // will be ignored if it's already configured via API

    } else
    {
        if(!_ssrcForced)
        {
            // generate a new SSRC
            _ssrcDB.ReturnSSRC(_ssrc);
            _ssrc = _ssrcDB.CreateSSRC();   // can't be 0

        }
        if(!_sequenceNumberForced && !_ssrcForced) // don't initialize seq number if SSRC passed externally
        {
            // generate a new sequence number
            _sequenceNumber = rand() / (RAND_MAX / MAX_INIT_RTP_SEQ_NUMBER);
        }
    }
}

void
RTPSender::SetSendingMediaStatus(const bool enabled)
{
    CriticalSectionScoped cs(_sendCritsect);
    _sendingMedia = enabled;
}

bool
RTPSender::SendingMedia() const
{
    CriticalSectionScoped cs(_sendCritsect);
    return _sendingMedia;
}

WebRtc_UWord32
RTPSender::Timestamp() const
{
    CriticalSectionScoped cs(_sendCritsect);
    return _timeStamp;
}


WebRtc_Word32
RTPSender::SetStartTimestamp( const WebRtc_UWord32 timestamp, const bool force)
{
    CriticalSectionScoped cs(_sendCritsect);
    if(force)
    {
        _startTimeStampForced = force;
        _startTimeStamp = timestamp;
    } else
    {
        if(!_startTimeStampForced)
        {
            _startTimeStamp = timestamp;
        }
    }
    return 0;
}

WebRtc_UWord32
RTPSender::StartTimestamp() const
{
    CriticalSectionScoped cs(_sendCritsect);
    return _startTimeStamp;
}

WebRtc_UWord32
RTPSender::GenerateNewSSRC()
{
    // if configured via API, return 0
    CriticalSectionScoped cs(_sendCritsect);

    if(_ssrcForced)
    {
        return 0;
    }
    _ssrc = _ssrcDB.CreateSSRC();   // can't be 0
    return _ssrc;
}

WebRtc_Word32
RTPSender::SetSSRC(WebRtc_UWord32 ssrc)
{
    // this is configured via the API
    CriticalSectionScoped cs(_sendCritsect);

    if (_ssrc == ssrc && _ssrcForced)
    {
        return 0; // since it's same ssrc, don't reset anything
    }

    _ssrcForced = true;

    _ssrcDB.ReturnSSRC(_ssrc);
    _ssrcDB.RegisterSSRC(ssrc);
    _ssrc = ssrc;

    if(!_sequenceNumberForced)
    {
        _sequenceNumber = rand() / (RAND_MAX / MAX_INIT_RTP_SEQ_NUMBER);
    }
    return 0;
}

WebRtc_UWord32
RTPSender::SSRC() const
{
    CriticalSectionScoped cs(_sendCritsect);
    return _ssrc;
}

WebRtc_Word32
RTPSender::SetCSRCStatus(const bool include)
{
    _includeCSRCs = include;
    return 0;
}

WebRtc_Word32
RTPSender::SetCSRCs(const WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize],
                    const WebRtc_UWord8 arrLength)
{
    if(arrLength > kRtpCsrcSize)
    {
        assert(false);
        return -1;
    }

    CriticalSectionScoped cs(_sendCritsect);

    for(int i = 0; i < arrLength;i++)
    {
        _CSRC[i] = arrOfCSRC[i];
    }
    _CSRCs = arrLength;
    return 0;
}

WebRtc_Word32
RTPSender::CSRCs(WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const
{
    CriticalSectionScoped cs(_sendCritsect);

    if(arrOfCSRC == NULL)
    {
        assert(false);
        return -1;
    }
    for(int i = 0; i < _CSRCs && i < kRtpCsrcSize;i++)
    {
        arrOfCSRC[i] = _CSRC[i];
    }
    return _CSRCs;
}

WebRtc_Word32
RTPSender::SetSequenceNumber(WebRtc_UWord16 seq)
{
    CriticalSectionScoped cs(_sendCritsect);
    _sequenceNumberForced = true;
    _sequenceNumber = seq;
    return 0;
}

WebRtc_UWord16
RTPSender::SequenceNumber() const
{
    CriticalSectionScoped cs(_sendCritsect);
    return _sequenceNumber;
}


    /*
    *    Audio
    */
WebRtc_Word32
RTPSender::RegisterAudioCallback(RtpAudioFeedback* messagesCallback)
{
    if(!_audioConfigured)
    {
        return -1;
    }
    return _audio->RegisterAudioCallback(messagesCallback);
}

    // Send a DTMF tone, RFC 2833 (4733)
WebRtc_Word32
RTPSender::SendTelephoneEvent(const WebRtc_UWord8 key,
                              const WebRtc_UWord16 time_ms,
                              const WebRtc_UWord8 level)
{
    if(!_audioConfigured)
    {
        return -1;
    }
    return _audio->SendTelephoneEvent(key, time_ms, level);
}

bool
RTPSender::SendTelephoneEventActive(WebRtc_Word8& telephoneEvent) const
{
    if(!_audioConfigured)
    {
        return false;
    }
    return _audio->SendTelephoneEventActive(telephoneEvent);
}

    // set audio packet size, used to determine when it's time to send a DTMF packet in silence (CNG)
WebRtc_Word32
RTPSender::SetAudioPacketSize(const WebRtc_UWord16 packetSizeSamples)
{
    if(!_audioConfigured)
    {
        return -1;
    }
    return _audio->SetAudioPacketSize(packetSizeSamples);
}

WebRtc_Word32
RTPSender::SetAudioLevelIndicationStatus(const bool enable,
                                         const WebRtc_UWord8 ID)
{
    if(!_audioConfigured)
    {
        return -1;
    }
    return _audio->SetAudioLevelIndicationStatus(enable, ID);
}

WebRtc_Word32
RTPSender::AudioLevelIndicationStatus(bool& enable,
                                      WebRtc_UWord8& ID) const
{
    return _audio->AudioLevelIndicationStatus(enable, ID);
}

WebRtc_Word32
RTPSender::SetAudioLevel(const WebRtc_UWord8 level_dBov)
{
    return _audio->SetAudioLevel(level_dBov);
}

    // Set payload type for Redundant Audio Data RFC 2198
WebRtc_Word32
RTPSender::SetRED(const WebRtc_Word8 payloadType)
{
    if(!_audioConfigured)
    {
        return -1;
    }
    return _audio->SetRED(payloadType);
}

    // Get payload type for Redundant Audio Data RFC 2198
WebRtc_Word32
RTPSender::RED(WebRtc_Word8& payloadType) const
{
    if(!_audioConfigured)
    {
        return -1;
    }
    return _audio->RED(payloadType);
}

    /*
    *    Video
    */
VideoCodecInformation*
RTPSender::CodecInformationVideo()
{
    if(_audioConfigured)
    {
        return NULL;
    }
    return _video->CodecInformationVideo();
}

RtpVideoCodecTypes
RTPSender::VideoCodecType() const
{
    if(_audioConfigured)
    {
        return kRtpNoVideo;
    }
    return _video->VideoCodecType();
}

WebRtc_UWord32
RTPSender::MaxConfiguredBitrateVideo() const
{
    if(_audioConfigured)
    {
        return 0;
    }
    return _video->MaxConfiguredBitrateVideo();
}

WebRtc_Word32
RTPSender::SendRTPIntraRequest()
{
    if(_audioConfigured)
    {
        return -1;
    }
    return _video->SendRTPIntraRequest();
}

// FEC
WebRtc_Word32
RTPSender::SetGenericFECStatus(const bool enable,
                               const WebRtc_UWord8 payloadTypeRED,
                               const WebRtc_UWord8 payloadTypeFEC)
{
    if(_audioConfigured)
    {
        return -1;
    }
    return _video->SetGenericFECStatus(enable, payloadTypeRED, payloadTypeFEC);
}

WebRtc_Word32
RTPSender::GenericFECStatus(bool& enable,
                            WebRtc_UWord8& payloadTypeRED,
                            WebRtc_UWord8& payloadTypeFEC) const
{
    if(_audioConfigured)
    {
        return -1;
    }
    return _video->GenericFECStatus(enable, payloadTypeRED, payloadTypeFEC);
}

WebRtc_Word32
RTPSender::SetFECCodeRate(const WebRtc_UWord8 keyFrameCodeRate,
                          const WebRtc_UWord8 deltaFrameCodeRate)
{
    if(_audioConfigured)
    {
        return -1;
    }
    return _video->SetFECCodeRate(keyFrameCodeRate, deltaFrameCodeRate);
}

WebRtc_Word32
RTPSender::SetFECUepProtection(const bool keyUseUepProtection,
                               const bool deltaUseUepProtection)

{
    if(_audioConfigured)
    {
        return -1;
    }
    return _video->SetFECUepProtection(keyUseUepProtection,
                                       deltaUseUepProtection);
}
} // namespace webrtc
