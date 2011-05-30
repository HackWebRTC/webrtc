/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtp_receiver_audio.h"

#include <cassert> //assert
#include <cstring> // memcpy()
#include <math.h>    // pow()

#include "critical_section_wrapper.h"

namespace webrtc {
RTPReceiverAudio::RTPReceiverAudio(const WebRtc_Word32 id):
    _id(id),
    _lastReceivedFrequency(8000),
    _telephoneEvent(false),
    _telephoneEventForwardToDecoder(false),
    _telephoneEventDetectEndOfTone(false),
    _telephoneEventPayloadType(-1),
    _telephoneEventReported(),
    _cngNBPayloadType(-1),
    _cngWBPayloadType(-1),
    _cngSWBPayloadType(-1),
    _cngPayloadType(-1),
    _G722PayloadType(-1),
    _lastReceivedG722(false),
    _criticalSectionFeedback(*CriticalSectionWrapper::CreateCriticalSection()),
    _cbAudioFeedback(NULL)
{
}

RTPReceiverAudio::~RTPReceiverAudio()
{
    delete &_criticalSectionFeedback;
}

WebRtc_Word32
RTPReceiverAudio::Init()
{
    _lastReceivedFrequency = 8000;
    _telephoneEvent = false;
    _telephoneEventForwardToDecoder = false;
    _telephoneEventDetectEndOfTone = false;
    _telephoneEventPayloadType = -1;

    while(_telephoneEventReported.Size() > 0)
    {
        _telephoneEventReported.Erase(_telephoneEventReported.First());
    }
    _cngNBPayloadType = -1;
    _cngWBPayloadType = -1;
    _cngSWBPayloadType = -1;
    _cngPayloadType = -1;
    _G722PayloadType = -1;
    _lastReceivedG722 = false;
    return 0;
}

void
RTPReceiverAudio::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
}

WebRtc_Word32
RTPReceiverAudio::RegisterIncomingAudioCallback(RtpAudioFeedback* incomingMessagesCallback)
{
    CriticalSectionScoped lock(_criticalSectionFeedback);
    _cbAudioFeedback = incomingMessagesCallback;
    return 0;
}

WebRtc_UWord32
RTPReceiverAudio::AudioFrequency() const
{
    if(_lastReceivedG722)
    {
        return 8000;
    }
    return _lastReceivedFrequency;
}

// Outband TelephoneEvent(DTMF) detection
WebRtc_Word32
RTPReceiverAudio::SetTelephoneEventStatus(const bool enable,
                                          const bool forwardToDecoder,
                                          const bool detectEndOfTone)
{
    _telephoneEvent= enable;
    _telephoneEventDetectEndOfTone = detectEndOfTone;
    _telephoneEventForwardToDecoder = forwardToDecoder;
    return 0;
}

 // Is outband TelephoneEvent(DTMF) turned on/off?
bool
RTPReceiverAudio::TelephoneEvent() const
{
    return _telephoneEvent;
}

// Is forwarding of outband telephone events turned on/off?
bool
RTPReceiverAudio::TelephoneEventForwardToDecoder() const
{
    return _telephoneEventForwardToDecoder;
}

bool
RTPReceiverAudio::TelephoneEventPayloadType(const WebRtc_Word8 payloadType) const
{
    return (_telephoneEventPayloadType == payloadType)?true:false;
}

bool
RTPReceiverAudio::CNGPayloadType(const WebRtc_Word8 payloadType,
                                 WebRtc_UWord32& frequency)
{
    //  we can have three CNG on 8000Hz, 16000Hz and 32000Hz
    if(_cngNBPayloadType == payloadType)
    {
        frequency = 8000;
        if ((_cngPayloadType != -1) &&(_cngPayloadType !=_cngNBPayloadType))
        {
            ResetStatistics();
        }
        _cngPayloadType = _cngNBPayloadType;
        return true;
    } else if(_cngWBPayloadType == payloadType)
    {
        // if last received codec is G.722 we must use frequency 8000
        if(_lastReceivedG722)
        {
            frequency = 8000;
        } else
        {
            frequency = 16000;
        }
        if ((_cngPayloadType != -1) &&(_cngPayloadType !=_cngWBPayloadType))
        {
            ResetStatistics();
        }
        _cngPayloadType = _cngWBPayloadType;
        return true;
    }else if(_cngSWBPayloadType == payloadType)
    {
        frequency = 32000;
        if ((_cngPayloadType != -1) &&(_cngPayloadType !=_cngSWBPayloadType))
        {
            ResetStatistics();
        }
        _cngPayloadType = _cngSWBPayloadType;
        return true;
    }else
    {
        //  not CNG
        if(_G722PayloadType == payloadType)
        {
            _lastReceivedG722 = true;
        }else
        {
            _lastReceivedG722 = false;
        }
    }
    return false;
}

/*
   Sample based or frame based codecs based on RFC 3551

   NOTE! There is one error in the RFC, stating G.722 uses 8 bits/samples.
   The correct rate is 4 bits/sample.

   name of                              sampling              default
   encoding  sample/frame  bits/sample      rate  ms/frame  ms/packet

   Sample based audio codecs
   DVI4      sample        4                var.                   20
   G722      sample        4              16,000                   20
   G726-40   sample        5               8,000                   20
   G726-32   sample        4               8,000                   20
   G726-24   sample        3               8,000                   20
   G726-16   sample        2               8,000                   20
   L8        sample        8                var.                   20
   L16       sample        16               var.                   20
   PCMA      sample        8                var.                   20
   PCMU      sample        8                var.                   20

   Frame based audio codecs
   G723      frame         N/A             8,000        30         30
   G728      frame         N/A             8,000       2.5         20
   G729      frame         N/A             8,000        10         20
   G729D     frame         N/A             8,000        10         20
   G729E     frame         N/A             8,000        10         20
   GSM       frame         N/A             8,000        20         20
   GSM-EFR   frame         N/A             8,000        20         20
   LPC       frame         N/A             8,000        20         20
   MPA       frame         N/A              var.      var.

   G7221     frame         N/A
*/

ModuleRTPUtility::Payload*
RTPReceiverAudio::RegisterReceiveAudioPayload(const WebRtc_Word8 payloadName[RTP_PAYLOAD_NAME_SIZE],
                                              const WebRtc_Word8 payloadType,
                                              const WebRtc_UWord32 frequency,
                                              const WebRtc_UWord8 channels,
                                              const WebRtc_UWord32 rate)
{
    WebRtc_Word32 length = (WebRtc_Word32)strlen(payloadName);
    if(length > RTP_PAYLOAD_NAME_SIZE)
    {
        assert(false);
        return NULL;
    }

    if (ModuleRTPUtility::StringCompare(payloadName,"telephone-event",15))
    {
        _telephoneEventPayloadType = payloadType;
    }
    if (ModuleRTPUtility::StringCompare(payloadName,"cn",2))
    {
        //  we can have three CNG on 8000Hz, 16000Hz and 32000Hz
        if(frequency == 8000)
        {
            _cngNBPayloadType = payloadType;

        } else if(frequency == 16000)
        {
            _cngWBPayloadType = payloadType;

        } else if(frequency == 32000)
        {
            _cngSWBPayloadType = payloadType;
        }else
        {
            assert(false);
            return NULL;
        }
    }
    WebRtc_UWord8 bitsPerSample = 0; // zero implies frame based
    if (ModuleRTPUtility::StringCompare(payloadName,"DVI4",4))
    {
        bitsPerSample = 4;
    } else if(ModuleRTPUtility::StringCompare(payloadName,"G722",4))
    {
        if(ModuleRTPUtility::StringCompare(payloadName,"G7221",5))
        {
            // frame based
        } else
        {
            _G722PayloadType = payloadType;
            bitsPerSample = 4;
        }
    } else if(ModuleRTPUtility::StringCompare(payloadName,"G726-40",7))
    {
        bitsPerSample = 5;
    } else if(ModuleRTPUtility::StringCompare(payloadName,"G726-32",7))
    {
        bitsPerSample = 4;
    } else if(ModuleRTPUtility::StringCompare(payloadName,"G726-24",7))
    {
        bitsPerSample = 3;
    } else if(ModuleRTPUtility::StringCompare(payloadName,"G726-16",7))
    {
        bitsPerSample = 2;
    } else if(ModuleRTPUtility::StringCompare(payloadName,"L8",2))
    {
        bitsPerSample = 8;
    } else if(ModuleRTPUtility::StringCompare(payloadName,"L16",3))
    {
        bitsPerSample = 16;
    } else if(ModuleRTPUtility::StringCompare(payloadName,"PCMU",4))
    {
        bitsPerSample = 8;
    } else if(ModuleRTPUtility::StringCompare(payloadName,"PCMA",4))
    {
        bitsPerSample = 8;
    }

    ModuleRTPUtility::Payload* payload = new ModuleRTPUtility::Payload;
    memcpy(payload->name, payloadName, length+1);
    payload->typeSpecific.Audio.frequency = frequency;
    payload->typeSpecific.Audio.channels = channels;
    payload->typeSpecific.Audio.bitsPerSample = bitsPerSample;
    payload->typeSpecific.Audio.rate = rate;
    payload->audio = true;
    return payload;
}

// we are not allowed to have any critsects when calling CallbackOfReceivedPayloadData
WebRtc_Word32
RTPReceiverAudio::ParseAudioCodecSpecific(WebRtcRTPHeader* rtpHeader,
                                          const WebRtc_UWord8* payloadData,
                                          const WebRtc_UWord16 payloadLength,
                                          const ModuleRTPUtility::AudioPayload& audioSpecific,
                                          const bool isRED)
{
    WebRtc_UWord8 newEvents[MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS];
    WebRtc_UWord8 removedEvents[MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS];
    WebRtc_UWord8 numberOfNewEvents = 0;
    WebRtc_UWord8 numberOfRemovedEvents = 0;
    bool telephoneEventPacket = TelephoneEventPayloadType(rtpHeader->header.payloadType);

    if(payloadLength == 0)
    {
        return 0;
    }

    {
        CriticalSectionScoped lock(_criticalSectionFeedback);

        if(telephoneEventPacket)
        {
            // RFC 4733 2.3
            /*
                0                   1                   2                   3
                0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                |     event     |E|R| volume    |          duration             |
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            */
            if(payloadLength % 4 != 0)
            {
                return -1;
            }
            WebRtc_UWord8 numberOfEvents = payloadLength / 4;

            // sanity
            if(numberOfEvents >= MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS)
            {
                numberOfEvents = MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS;
            }
            for (int n = 0; n < numberOfEvents; n++)
            {
                bool end = (payloadData[(4*n)+1] & 0x80)? true:false;

                if(_telephoneEventReported.Find(payloadData[4*n]) != NULL)
                {
                    // we have already seen this event
                    if(end)
                    {
                        removedEvents[numberOfRemovedEvents]= payloadData[4*n];
                        numberOfRemovedEvents++;
                        _telephoneEventReported.Erase(payloadData[4*n]);
                    }
                }else
                {
                    if(end)
                    {
                        // don't add if it's a end of a tone
                    }else
                    {
                        newEvents[numberOfNewEvents] = payloadData[4*n];
                        numberOfNewEvents++;
                        _telephoneEventReported.Insert(payloadData[4*n],NULL);
                    }
                }
            }

            // RFC 4733 2.5.1.3 & 2.5.2.3 Long-Duration Events
            // should not be a problem since we don't care about the duration

            // RFC 4733 See 2.5.1.5. & 2.5.2.4.  Multiple Events in a Packet
        }

        if(_telephoneEvent && _cbAudioFeedback)
        {
            for (int n = 0; n < numberOfNewEvents; n++)
            {
                _cbAudioFeedback->OnReceivedTelephoneEvent(_id, newEvents[n], false);
            }
            if(_telephoneEventDetectEndOfTone)
            {
                for (int n = 0; n < numberOfRemovedEvents; n++)
                {
                    _cbAudioFeedback->OnReceivedTelephoneEvent(_id, removedEvents[n], true);
                }
            }
        }
    }
    if(! telephoneEventPacket )
    {
        _lastReceivedFrequency = audioSpecific.frequency;
    }

    // Check if this is a CNG packet, receiver might want to know
    WebRtc_UWord32 dummy;
    if(CNGPayloadType(rtpHeader->header.payloadType, dummy))
    {
        rtpHeader->type.Audio.isCNG=true;
        rtpHeader->frameType = kAudioFrameCN;
    }else
    {
        rtpHeader->frameType = kAudioFrameSpeech;
        rtpHeader->type.Audio.isCNG=false;
    }

    // check if it's a DTMF event, hence something we can playout
    if(telephoneEventPacket)
    {
        if(!_telephoneEventForwardToDecoder)
        {
            // don't forward event to decoder
            return 0;
        }
        MapItem* first = _telephoneEventReported.First();
        if(first && first->GetId() > 15)
        {
            // don't forward non DTMF events
            return 0;
        }
    }
    if(isRED && !(payloadData[0] & 0x80))
    {
        // we recive only one frame packed in a RED packet remove the RED wrapper
        rtpHeader->header.payloadType = payloadData[0];

        // only one frame in the RED strip the one byte to help NetEq
        return CallbackOfReceivedPayloadData(payloadData+1,
                                             payloadLength-1,
                                             rtpHeader);
    }
    if(audioSpecific.channels > 1)
    {
        WebRtc_Word32 retVal = 0;
        WebRtc_UWord16 channelLength = payloadLength/audioSpecific.channels;

        if(audioSpecific.bitsPerSample > 0)
        {
            // sanity
            assert((payloadLength*8)%audioSpecific.bitsPerSample == 0);

            // sample based codec

            // build matrix
            WebRtc_UWord8 matrix[IP_PACKET_SIZE];
            WebRtc_UWord32 offsetBytes = 0;
            WebRtc_UWord32 offsetBytesInsert = 0;
            // initialize matrix to 0
            memset(matrix, 0, audioSpecific.channels*channelLength);

            switch(audioSpecific.bitsPerSample)
            {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                {
                    WebRtc_UWord32 offsetSamples = 0;
                    WebRtc_UWord32 offsetSamplesInsert = 0;
                    WebRtc_UWord16 bitMask = (WebRtc_UWord16)ModuleRTPUtility::pow2(audioSpecific.bitsPerSample)-1;
                    WebRtc_UWord16 samplesPerChannel =payloadLength*8/audioSpecific.bitsPerSample/audioSpecific.channels;

                    for(WebRtc_UWord32 i = 0; i < samplesPerChannel; i++)
                    {
                        WebRtc_UWord8 insertShift = (WebRtc_UWord8)((offsetSamplesInsert+audioSpecific.bitsPerSample)%16);
                        insertShift = 16 - insertShift;  // inverse the calculation

                        for(WebRtc_UWord32 j = 0; j < audioSpecific.channels; j++)
                        {
                            // get sample
                            WebRtc_UWord16 s = payloadData[offsetBytes] << 8;

                            // check that we don't read outside the memory
                            if(offsetBytes < (WebRtc_UWord32)payloadLength -2)
                            {
                                s += payloadData[offsetBytes+1];
                            }

                            WebRtc_UWord8 readShift = (WebRtc_UWord8)((offsetSamples+audioSpecific.bitsPerSample)%16);
                            readShift = 16 - readShift;  // inverse the calculation
                            s >>= readShift;
                            s &= bitMask;

                            // prepare for reading next sample
                            offsetSamples += audioSpecific.bitsPerSample;
                            if(readShift <= audioSpecific.bitsPerSample)
                            {
                                // next does not fitt
                                // or fitt exactly
                                offsetSamples -= 8;
                                offsetBytes++;
                            }

                            // insert sample into matrix
                            WebRtc_UWord32 columOffset = j*channelLength;

                            WebRtc_UWord16 insert = s << insertShift;
#if defined(WEBRTC_LITTLE_ENDIAN)
                            matrix[columOffset+offsetBytesInsert]   |= static_cast<WebRtc_UWord8>(insert>>8);
                            matrix[columOffset+offsetBytesInsert+1] |= static_cast<WebRtc_UWord8>(insert);
#else
                            WebRtc_UWord16* matrixU16 = (WebRtc_UWord16*)&(matrix[columOffset+offsetBytesInsert]);
                            matrixU16[0] |= (s << insertShift);
#endif
                        }
                        // prepare for writing next sample
                        offsetSamplesInsert += audioSpecific.bitsPerSample;
                        if(insertShift <= audioSpecific.bitsPerSample)
                        {
                            // next does not fitt
                            // or fitt exactly
                            offsetSamplesInsert -= 8;
                            offsetBytesInsert++;
                        }
                    }
                }
                break;
            case 8:
                {
                    WebRtc_UWord32 sample = 0;
                    for(WebRtc_UWord32 i = 0; i < channelLength; i++)
                    {
                        for(WebRtc_UWord32 j = 0; j < audioSpecific.channels; j++)
                        {
                            WebRtc_UWord32 columOffset = j*channelLength;
                            matrix[columOffset + i] = payloadData[sample++];
                        }
                    }
                }
                break;
            case 16:
                {
                    WebRtc_UWord32 sample = 0;
                    for(WebRtc_UWord32 i = 0; i < channelLength; i +=2)
                    {
                        for(WebRtc_UWord32 j = 0; j < audioSpecific.channels; j++)
                        {
                            WebRtc_UWord32 columOffset = j*channelLength;
                            matrix[columOffset + i] = payloadData[sample++];
                            matrix[columOffset + i + 1] = payloadData[sample++];
                        }
                    }
                }
                break;
            default:
                assert(false);
                return -1;
            }
            // we support 16 bits sample
            // callback for all channels
            for(int channel = 0; channel < audioSpecific.channels && retVal == 0; channel++)
            {
                // one callback per channel
                rtpHeader->type.Audio.channel = channel+1;

                if(channel == 0)
                {
                    // include the original packet only in the first callback
                    retVal = CallbackOfReceivedPayloadData(&matrix[channel*channelLength],
                                                           channelLength,
                                                           rtpHeader);
                } else
                {
                    retVal = CallbackOfReceivedPayloadData(&matrix[channel*channelLength],
                                                           channelLength,
                                                           rtpHeader);
                }
            }
        } else
        {
            for(int channel = 1; channel <= audioSpecific.channels && retVal == 0; channel++)
            {
                // one callback per channel
                rtpHeader->type.Audio.channel = channel;

                if(channel == 1)
                {
                    // include the original packet only in the first callback
                    retVal = CallbackOfReceivedPayloadData(payloadData,
                                                           channelLength,
                                                           rtpHeader);
                } else
                {
                    retVal = CallbackOfReceivedPayloadData(payloadData,
                                                           channelLength,
                                                           rtpHeader);
                }
                payloadData += channelLength;
            }
        }
        return retVal;
    }else
    {
        rtpHeader->type.Audio.channel = 1;
        return CallbackOfReceivedPayloadData(payloadData,
                                             payloadLength,
                                             rtpHeader);
    }
}
} // namespace webrtc
