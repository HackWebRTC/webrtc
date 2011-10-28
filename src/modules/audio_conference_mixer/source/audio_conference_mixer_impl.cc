/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio_conference_mixer_defines.h"
#include "audio_conference_mixer_impl.h"
#include "audio_frame_manipulator.h"
#include "critical_section_wrapper.h"
#include "map_wrapper.h"
#include "trace.h"

namespace webrtc {
MixerParticipant::MixerParticipant()
    : _mixHistory(new MixHistory())
{
}

MixerParticipant::~MixerParticipant()
{
    delete _mixHistory;
}

WebRtc_Word32 MixerParticipant::IsMixed(bool& mixed) const
{
    return _mixHistory->IsMixed(mixed);
}

MixHistory::MixHistory()
    : _isMixed(0)
{
}

MixHistory::~MixHistory()
{
}

WebRtc_Word32 MixHistory::IsMixed(bool& mixed) const
{
    mixed = (_isMixed.Value() == 1);
    return 0;
}

WebRtc_Word32 MixHistory::WasMixed(bool& wasMixed) const
{
    // Was mixed is the same as is mixed depending on perspective. This function
    // is for the perspective of AudioConferenceMixerImpl.
    return IsMixed(wasMixed);
}

WebRtc_Word32 MixHistory::SetIsMixed(const bool mixed)
{
    _isMixed = mixed ? 1 : 0;
    return 0;
}

void MixHistory::ResetMixedStatus()
{
    _isMixed  = 0;
}

AudioConferenceMixer* AudioConferenceMixer::CreateAudioConferenceMixer(
    const WebRtc_Word32 id)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioMixerServer, id,
                 "CreateAudioConferenceMixer");
    return new AudioConferenceMixerImpl(id);
}

AudioConferenceMixerImpl::AudioConferenceMixerImpl(const WebRtc_Word32 id)
    : _scratchParticipantsToMixAmount(0),
      _scratchMixedParticipants(),
      _scratchVadPositiveParticipantsAmount(0),
      _scratchVadPositiveParticipants(),
      _crit(CriticalSectionWrapper::CreateCriticalSection()),
      _cbCrit(CriticalSectionWrapper::CreateCriticalSection()),
      _id(id),
      _minimumMixingFreq(kLowestPossible),
      _mixReceiver(NULL),
      _mixerStatusCallback(NULL),
      _amountOf10MsBetweenCallbacks(1),
      _amountOf10MsUntilNextCallback(0),
      _mixerStatusCb(false),
      _outputFrequency(kDefaultFrequency),
      _sampleSize((_outputFrequency*kProcessPeriodicityInMs)/1000),
      _participantList(),
      _amountOfMixableParticipants(0),
      _timeStamp(0),
      _timeScheduler(kProcessPeriodicityInMs),
      _mixedAudioLevel(),
      _processCalls(0)
{
    MemoryPool<AudioFrame>::CreateMemoryPool(_audioFramePool,
                                             DEFAULT_AUDIO_FRAME_POOLSIZE);
    WEBRTC_TRACE(kTraceMemory, kTraceAudioMixerServer, _id, "%s created",
                 __FUNCTION__);
}

AudioConferenceMixerImpl::~AudioConferenceMixerImpl()
{
    delete _crit;
    delete _cbCrit;

    MemoryPool<AudioFrame>::DeleteMemoryPool(_audioFramePool);
    assert(_audioFramePool==NULL);
    WEBRTC_TRACE(kTraceMemory, kTraceAudioMixerServer, _id, "%s deleted",
                 __FUNCTION__);
}

WebRtc_Word32 AudioConferenceMixerImpl::Version(
    WebRtc_Word8* version,
    WebRtc_UWord32& remainingBufferInBytes,
    WebRtc_UWord32& position) const
{
    WEBRTC_TRACE(
        kTraceModuleCall,
        kTraceAudioMixerServer,
        _id,
        "Version(version%s,remainingBufferLengthInBytes:%d,position:%d)",
        (version==NULL) ? ":NULL" : "",
        remainingBufferInBytes,position);
    return GetVersion(version,remainingBufferInBytes,position);
}

WebRtc_Word32 AudioConferenceMixerImpl::ChangeUniqueId(const WebRtc_Word32 id)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioMixerServer, _id,
                 "ChangeUniqueId(new id:%d)", id);
    _id = id;
    return 0;
}

WebRtc_Word32 AudioConferenceMixer::GetVersion(
    WebRtc_Word8* version,
    WebRtc_UWord32& remainingBufferInBytes,
    WebRtc_UWord32& position)
{
    WEBRTC_TRACE(
        kTraceModuleCall,
        kTraceAudioMixerServer,
        -1,
        "Version(version%s,remainingBufferLengthInBytes:%d,position:%d)",
        (version==NULL) ? "NULL" : "",
        remainingBufferInBytes,
        position);
    if(version == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, -1,
                     "Invalid in argument to GetVersion()");
        return -1;
    }
    const char moduleVersion[] = VERSION_STRING;
    if(remainingBufferInBytes < sizeof(moduleVersion))
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, -1,
                     "Too small buffer in GetVersion()");
        return -1;
    }
    strncpy(version, moduleVersion, sizeof(moduleVersion));  // null termination
    remainingBufferInBytes -= sizeof(moduleVersion);
    position += sizeof(moduleVersion);
    return 0;
}

// Process should be called every kProcessPeriodicityInMs ms
WebRtc_Word32 AudioConferenceMixerImpl::TimeUntilNextProcess()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioMixerServer, _id,
                 "TimeUntilNextProcess()");
    WebRtc_Word32 timeUntilNextProcess = 0;
    CriticalSectionScoped cs(*_crit);
    if(_timeScheduler.TimeToNextUpdate(timeUntilNextProcess) != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, _id,
                     "failed in TimeToNextUpdate() call");
        // Sanity check
        assert(false);
        return -1;
    }
    return timeUntilNextProcess;
}

WebRtc_Word32 AudioConferenceMixerImpl::Process()
{
    WebRtc_UWord32 remainingParticipantsAllowedToMix =
        kMaximumAmountOfMixedParticipants;
    {
        CriticalSectionScoped cs(*_crit);
        assert(_processCalls == 0);
        _processCalls++;

        // Let the scheduler know that we are running one iteration.
        _timeScheduler.UpdateScheduler();
    }

    ListWrapper mixList;
    ListWrapper rampOutList;
    MapWrapper mixedParticipantsMap;
    {
        CriticalSectionScoped cs(*_cbCrit);

        WebRtc_Word32 lowFreq = GetLowestMixingFrequency();
        // SILK can run in 12 kHz and 24 kHz. These frequencies are not
        // supported so use closet higher frequency to not lose any information.
        // TODO (hellner): this is probably more appropriate to do in
        //                 GetLowestMixingFrequency().
        if (lowFreq == 12000)
        {
            lowFreq = 16000;
        } else if (lowFreq == 24000) {
            lowFreq = 32000;
        }
        if(lowFreq <= 0)
        {
            CriticalSectionScoped cs(*_crit);
            _processCalls--;
            return 0;
        } else  {
            switch(lowFreq)
            {
            case 8000:
                if(OutputFrequency() != kNbInHz)
                {
                    SetOutputFrequency(kNbInHz);
                }
                break;
            case 16000:
                if(OutputFrequency() != kWbInHz)
                {
                    SetOutputFrequency(kWbInHz);
                }
                break;
            case 32000:
                if(OutputFrequency() != kSwbInHz)
                {
                    SetOutputFrequency(kSwbInHz);
                }
                break;
            default:
                assert(false);

                CriticalSectionScoped cs(*_crit);
                _processCalls--;
                return -1;
            }
        }

        UpdateToMix(mixList, rampOutList, mixedParticipantsMap,
                    remainingParticipantsAllowedToMix);

        UpdateMixedStatus(mixedParticipantsMap);
        _scratchParticipantsToMixAmount = mixedParticipantsMap.Size();
    }

    // Clear mixedParticipantsMap to avoid memory leak warning.
    // Please note that the mixedParticipantsMap doesn't own any dynamically
    // allocated memory.
    while(mixedParticipantsMap.Erase(mixedParticipantsMap.First()) == 0) {}

    // Get an AudioFrame for mixing from the memory pool.
    AudioFrame* mixedAudio = NULL;
    if(_audioFramePool->PopMemory(mixedAudio) == -1)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceAudioMixerServer, _id,
                     "failed PopMemory() call");
        assert(false);
        return -1;
    }

    bool timeForMixerCallback = false;
    WebRtc_Word32 audioLevel = 0;
    {
        const ListItem* firstItem = mixList.First();
        // Assume mono.
        WebRtc_UWord8 numberOfChannels = 1;
        if(firstItem != NULL)
        {
            // Use the same number of channels as the first frame to be mixed.
            numberOfChannels = static_cast<const AudioFrame*>(
                firstItem->GetItem())->_audioChannel;
        }
        // TODO (hellner): it might be better to decide the number of channels
        //                 with an API instead of dynamically.


        CriticalSectionScoped cs(*_crit);

        mixedAudio->UpdateFrame(-1, _timeStamp, NULL, 0, _outputFrequency,
                                AudioFrame::kNormalSpeech,
                                AudioFrame::kVadPassive, numberOfChannels);

        _timeStamp += _sampleSize;

        MixFromList(*mixedAudio,mixList);

        MixAnonomouslyFromList(*mixedAudio, rampOutList);
        if(mixedAudio->_payloadDataLengthInSamples == 0)
        {
          // Nothing was mixed set the audio samples to silence.
          memset(mixedAudio->_payloadData, 0, _sampleSize);
          mixedAudio->_payloadDataLengthInSamples = _sampleSize;
        }

        _mixedAudioLevel.ComputeLevel(mixedAudio->_payloadData,_sampleSize);
        audioLevel = _mixedAudioLevel.GetLevel();

        if(_mixerStatusCb)
        {
            _scratchVadPositiveParticipantsAmount = 0;
            UpdateVADPositiveParticipants(mixList);
            if(_amountOf10MsUntilNextCallback-- == 0)
            {
                _amountOf10MsUntilNextCallback = _amountOf10MsBetweenCallbacks;
                timeForMixerCallback = true;
            }
        }
    }

    {
        CriticalSectionScoped cs(*_cbCrit);
        if(_mixReceiver != NULL)
        {
            const AudioFrame** dummy = NULL;
            _mixReceiver->NewMixedAudio(
                _id,
                *mixedAudio,
                dummy,
                0);
        }

        if((_mixerStatusCallback != NULL) &&
            timeForMixerCallback)
        {
            _mixerStatusCallback->MixedParticipants(
                _id,
                _scratchMixedParticipants,
                _scratchParticipantsToMixAmount);

            _mixerStatusCallback->VADPositiveParticipants(
                _id,
                _scratchVadPositiveParticipants,
                _scratchVadPositiveParticipantsAmount);
            _mixerStatusCallback->MixedAudioLevel(_id,audioLevel);
        }
    }

    // Reclaim all outstanding memory.
    _audioFramePool->PushMemory(mixedAudio);
    ClearAudioFrameList(mixList);
    ClearAudioFrameList(rampOutList);
    {
        CriticalSectionScoped cs(*_crit);
        _processCalls--;
    }
    return 0;
}

WebRtc_Word32 AudioConferenceMixerImpl::RegisterMixedStreamCallback(
    AudioMixerOutputReceiver& mixReceiver)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioMixerServer, _id,
                 "RegisterMixedStreamCallback(mixReceiver)");
    CriticalSectionScoped cs(*_cbCrit);
    if(_mixReceiver != NULL)
    {
        return -1;
    }
    _mixReceiver = &mixReceiver;
    return 0;
}

WebRtc_Word32 AudioConferenceMixerImpl::UnRegisterMixedStreamCallback()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioMixerServer, _id,
                 "UnRegisterMixedStreamCallback()");
    CriticalSectionScoped cs(*_cbCrit);
    if(_mixReceiver == NULL)
    {
        return -1;
    }
    _mixReceiver = NULL;
    return 0;
}

WebRtc_Word32 AudioConferenceMixerImpl::SetOutputFrequency(
    const Frequency frequency)
{
    CriticalSectionScoped cs(*_crit);
    _outputFrequency = frequency;
    _sampleSize = (_outputFrequency*kProcessPeriodicityInMs) / 1000;
    return 0;
}

AudioConferenceMixer::Frequency
AudioConferenceMixerImpl::OutputFrequency() const
{
    CriticalSectionScoped cs(*_crit);
    return _outputFrequency;
}

WebRtc_Word32 AudioConferenceMixerImpl::RegisterMixerStatusCallback(
    AudioMixerStatusReceiver& mixerStatusCallback,
    const WebRtc_UWord32 amountOf10MsBetweenCallbacks)
{
    WEBRTC_TRACE(
        kTraceModuleCall,
        kTraceAudioMixerServer,
        _id,
        "RegisterMixerStatusCallback(mixerStatusCallback,%d)",
        amountOf10MsBetweenCallbacks);
    if(amountOf10MsBetweenCallbacks == 0)
    {
        WEBRTC_TRACE(
            kTraceWarning,
            kTraceAudioMixerServer,
            _id,
            "amountOf10MsBetweenCallbacks(%d) needs to be larger than 0");
        return -1;
    }
    {
        CriticalSectionScoped cs(*_cbCrit);
        if(_mixerStatusCallback != NULL)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, _id,
                         "Mixer status callback already registered");
            return -1;
        }
        _mixerStatusCallback = &mixerStatusCallback;
    }
    {
        CriticalSectionScoped cs(*_crit);
        _amountOf10MsBetweenCallbacks  = amountOf10MsBetweenCallbacks;
        _amountOf10MsUntilNextCallback = 0;
        _mixerStatusCb                 = true;
    }
    return 0;
}

WebRtc_Word32 AudioConferenceMixerImpl::UnRegisterMixerStatusCallback()
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioMixerServer, _id,
                 "UnRegisterMixerStatusCallback()");
    {
        CriticalSectionScoped cs(*_crit);
        if(!_mixerStatusCb)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, _id,
                         "Mixer status callback not registered");
            return -1;
        }
        _mixerStatusCb = false;
    }
    {
        CriticalSectionScoped cs(*_cbCrit);
        _mixerStatusCallback = NULL;
    }
    return 0;
}

WebRtc_Word32 AudioConferenceMixerImpl::SetMixabilityStatus(
    MixerParticipant& participant,
    const bool mixable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioMixerServer, _id,
                 "SetMixabilityStatus(participant,mixable:%s)",
                 mixable ? "true" : "false");
    WebRtc_UWord32 amountOfMixableParticipants;
    {
        CriticalSectionScoped cs(*_cbCrit);
        const bool isMixed =
            IsParticipantInList(participant,_participantList);
        // API must be called with a new state.
        if(!(mixable ^ isMixed))
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, _id,
                         "Mixable is aready %s",
                         isMixed ? "ON" : "off");
            return -1;
        }
        bool success = false;
        if(mixable)
        {
            success = AddParticipantToList(participant,_participantList);
        }
        else
        {
            success = RemoveParticipantFromList(participant,_participantList);
        }
        if(!success)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, _id,
                         "failed to %s participant",
                         mixable ? "add" : "remove");
            assert(false);
            return -1;
        }
        amountOfMixableParticipants = _participantList.GetSize();
    }
    // A MixerParticipant was added or removed. Make sure the scratch
    // buffer is updated if necessary.
    // Note: The scratch buffer may only be updated in Process().
    CriticalSectionScoped cs(*_crit);
    _amountOfMixableParticipants = amountOfMixableParticipants;
    return 0;
}

WebRtc_Word32 AudioConferenceMixerImpl::MixabilityStatus(
    MixerParticipant& participant,
    bool& mixable)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioMixerServer, _id,
                 "MixabilityStatus(participant,mixable)");
    CriticalSectionScoped cs(*_cbCrit);
     mixable = IsParticipantInList(participant,_participantList);
    return 0;
}

WebRtc_Word32 AudioConferenceMixerImpl::AmountOfMixables(
    WebRtc_UWord32& amountOfMixableParticipants)
{
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioMixerServer, _id,
                 "AmountOfMixables(amountOfMixableParticipants)");
    CriticalSectionScoped cs(*_crit);
    amountOfMixableParticipants = _amountOfMixableParticipants;
    return 0;
}

WebRtc_Word32 AudioConferenceMixerImpl::SetMinimumMixingFrequency(
    Frequency freq)
{
    // Make sure that only allowed sampling frequencies are used. Use closest
    // higher sampling frequency to avoid losing information.
    if (static_cast<int>(freq) == 12000)
    {
         freq = kWbInHz;
    } else if (static_cast<int>(freq) == 24000) {
        freq = kSwbInHz;
    }

    if((freq == kNbInHz) || (freq == kWbInHz) || (freq == kSwbInHz) ||
       (freq == kLowestPossible))
    {
        _minimumMixingFreq=freq;
        return 0;
    }
    else
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioMixerServer, _id,
                     "SetMinimumMixingFrequency incorrect frequency: %i",freq);
        assert(false);
        return -1;
    }
}

// Check all AudioFrames that are to be mixed. The highest sampling frequency
// found is the lowest that can be used without losing information.
WebRtc_Word32 AudioConferenceMixerImpl::GetLowestMixingFrequency()
{
    WebRtc_Word32 highestFreq = 8000;
    ListItem* item = _participantList.First();
    while(item)
    {
        MixerParticipant* participant =
            static_cast<MixerParticipant*>(item->GetItem());
        const WebRtc_Word32 neededFrequency = participant->NeededFrequency(_id);
        if(neededFrequency > highestFreq)
        {
            highestFreq = neededFrequency;
        }
        item = _participantList.Next(item);
    }

    // Check if the user specified a lowest mixing frequency.
    if(_minimumMixingFreq != kLowestPossible)
    {
        if(_minimumMixingFreq > highestFreq)
        {
            return _minimumMixingFreq;
        }
    }
    return highestFreq;
}

void AudioConferenceMixerImpl::UpdateToMix(
    ListWrapper& mixList,
    ListWrapper& rampOutList,
    MapWrapper& mixParticipantList,
    WebRtc_UWord32& maxAudioFrameCounter)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "GetVIPAudio(mixList,rampOutList,mixParticipantList,%d)",
                 maxAudioFrameCounter);
    const WebRtc_UWord32 mixListStartSize = mixList.GetSize();
    ListWrapper activeList; // Elements are AudioFrames
    // Struct needed by the passive lists to keep track of which AudioFrame
    // belongs to which MixerParticipant.
    struct ParticipantFramePair
    {
        MixerParticipant* participant;
        AudioFrame* audioFrame;
    };
    ListWrapper passiveWasNotMixedList; // Elements are MixerParticipant
    ListWrapper passiveWasMixedList;    // Elements are MixerParticipant
    ListItem* item = _participantList.First();
    while(item)
    {
        // Stop keeping track of passive participants if there are already
        // enough participants available (they wont be mixed anyway).
        bool mustAddToPassiveList = (maxAudioFrameCounter >
                                    (activeList.GetSize() +
                                     passiveWasMixedList.GetSize() +
                                     passiveWasNotMixedList.GetSize()));

        MixerParticipant* participant = static_cast<MixerParticipant*>(
            item->GetItem());
        bool wasMixed = false;
        participant->_mixHistory->WasMixed(wasMixed);
        AudioFrame* audioFrame = NULL;
        if(_audioFramePool->PopMemory(audioFrame) == -1)
        {
            WEBRTC_TRACE(kTraceMemory, kTraceAudioMixerServer, _id,
                         "failed PopMemory() call");
            assert(false);
            return;
        }
        audioFrame->_frequencyInHz = _outputFrequency;

        if(participant->GetAudioFrame(_id,*audioFrame) != 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, _id,
                         "failed to GetAudioFrame() from participant");
            _audioFramePool->PushMemory(audioFrame);
            item = _participantList.Next(item);
            continue;
        }
        // TODO(henrike): this assert triggers in some test cases where SRTP is
        // used which prevents NetEQ from making a VAD. Temporarily disable this
        // assert until the problem is fixed on a higher level.
        // assert(audioFrame->_vadActivity != AudioFrame::kVadUnknown);
        if (audioFrame->_vadActivity == AudioFrame::kVadUnknown)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioMixerServer, _id,
                         "invalid VAD state from participant");
        }

        if(audioFrame->_vadActivity == AudioFrame::kVadActive)
        {
            if(!wasMixed)
            {
                RampIn(*audioFrame);
            }

            if(activeList.GetSize() >= maxAudioFrameCounter)
            {
                // There are already more active participants than should be
                // mixed. Only keep the ones with the highest energy.
                ListItem* replaceItem = NULL;
                CalculateEnergy(*audioFrame);
                WebRtc_UWord32 lowestEnergy = audioFrame->_energy;

                ListItem* activeItem = activeList.First();
                while(activeItem)
                {
                    AudioFrame* replaceFrame = static_cast<AudioFrame*>(
                        activeItem->GetItem());
                    CalculateEnergy(*replaceFrame);
                    if(replaceFrame->_energy < lowestEnergy)
                    {
                        replaceItem = activeItem;
                        lowestEnergy = replaceFrame->_energy;
                    }
                    activeItem = activeList.Next(activeItem);
                }
                if(replaceItem != NULL)
                {
                    AudioFrame* replaceFrame = static_cast<AudioFrame*>(
                        replaceItem->GetItem());

                    bool replaceWasMixed = false;
                    MapItem* replaceParticipant = mixParticipantList.Find(
                        replaceFrame->_id);
                    static_cast<MixerParticipant*>(
                        replaceParticipant->GetItem())->_mixHistory->WasMixed(
                            replaceWasMixed);

                    mixParticipantList.Erase(replaceFrame->_id);
                    activeList.Erase(replaceItem);

                    activeList.PushFront(static_cast<void*>(audioFrame));
                    mixParticipantList.Insert(audioFrame->_id,
                                              static_cast<void*>(participant));
                    assert(mixParticipantList.Size() <=
                           kMaximumAmountOfMixedParticipants);

                    if(replaceWasMixed)
                    {
                        RampOut(*replaceFrame);
                        rampOutList.PushBack(static_cast<void*>(replaceFrame));
                        assert(rampOutList.GetSize() <=
                               kMaximumAmountOfMixedParticipants);
                    } else {
                        _audioFramePool->PushMemory(replaceFrame);
                    }
                } else {
                    if(wasMixed)
                    {
                        RampOut(*audioFrame);
                        rampOutList.PushBack(static_cast<void*>(audioFrame));
                        assert(rampOutList.GetSize() <=
                               kMaximumAmountOfMixedParticipants);
                    } else {
                        _audioFramePool->PushMemory(audioFrame);
                    }
                }
            } else {
                activeList.PushFront(static_cast<void*>(audioFrame));
                mixParticipantList.Insert(audioFrame->_id,
                                          static_cast<void*>(participant));
                assert(mixParticipantList.Size() <=
                       kMaximumAmountOfMixedParticipants);
            }
        } else {
            if(wasMixed)
            {
                ParticipantFramePair* pair = new ParticipantFramePair;
                pair->audioFrame  = audioFrame;
                pair->participant = participant;
                passiveWasMixedList.PushBack(static_cast<void*>(pair));
            } else if(mustAddToPassiveList) {
                RampIn(*audioFrame);
                ParticipantFramePair* pair = new ParticipantFramePair;
                pair->audioFrame  = audioFrame;
                pair->participant = participant;
                passiveWasNotMixedList.PushBack(static_cast<void*>(pair));
            } else {
                _audioFramePool->PushMemory(audioFrame);
            }
        }
        item = _participantList.Next(item);
    }
    assert(activeList.GetSize() <= maxAudioFrameCounter);
    // At this point it is known which participants should be mixed. Transfer
    // this information to this functions output parameters.
    while(!activeList.Empty())
    {
        ListItem* mixItem = activeList.First();
        mixList.PushBack(mixItem->GetItem());
        activeList.Erase(mixItem);
    }
    // Always mix a constant number of AudioFrames. If there aren't enough
    // active participants mix passive ones. Starting with those that was mixed
    // last iteration.
    while(!passiveWasMixedList.Empty())
    {
        ListItem* mixItem = passiveWasMixedList.First();
        ParticipantFramePair* pair = static_cast<ParticipantFramePair*>(
            mixItem->GetItem());
        if(mixList.GetSize() <  maxAudioFrameCounter + mixListStartSize)
        {
            mixList.PushBack(pair->audioFrame);
            mixParticipantList.Insert(pair->audioFrame->_id,
                                      static_cast<void*>(pair->participant));
            assert(mixParticipantList.Size() <=
                   kMaximumAmountOfMixedParticipants);
        }
        else
        {
            _audioFramePool->PushMemory(pair->audioFrame);
        }
        delete pair;
        passiveWasMixedList.Erase(mixItem);
    }
    // And finally the ones that have not been mixed for a while.
    while(!passiveWasNotMixedList.Empty())
    {
        ListItem* mixItem = passiveWasNotMixedList.First();
        ParticipantFramePair* pair = static_cast<ParticipantFramePair*>(
            mixItem->GetItem());
        if(mixList.GetSize() <  maxAudioFrameCounter + mixListStartSize)
        {
            mixList.PushBack(pair->audioFrame);
            mixParticipantList.Insert(pair->audioFrame->_id,
                                      static_cast<void*>(pair->participant));
            assert(mixParticipantList.Size() <=
                   kMaximumAmountOfMixedParticipants);
        }
        else
        {
            _audioFramePool->PushMemory(pair->audioFrame);
        }
        delete pair;
        passiveWasNotMixedList.Erase(mixItem);
    }
    assert(maxAudioFrameCounter + mixListStartSize >= mixList.GetSize());
    maxAudioFrameCounter += mixListStartSize - mixList.GetSize();
}

void AudioConferenceMixerImpl::UpdateMixedStatus(
    MapWrapper& mixedParticipantsMap)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "UpdateMixedStatus(mixedParticipantsMap)");
    assert(mixedParticipantsMap.Size() <= kMaximumAmountOfMixedParticipants);

    // Loop through all non-VIP participants. If they are in the mix map they
    // were mixed.
    ListItem* participantItem = _participantList.First();
    while(participantItem != NULL)
    {
        bool isMixed = false;
        MixerParticipant* participant =
            static_cast<MixerParticipant*>(participantItem->GetItem());

        MapItem* mixedItem = mixedParticipantsMap.First();
        while(mixedItem)
        {
            if(participant == mixedItem->GetItem())
            {
                isMixed = true;
                break;
            }
            mixedItem = mixedParticipantsMap.Next(mixedItem);
        }
        participant->_mixHistory->SetIsMixed(isMixed);
        participantItem = _participantList.Next(participantItem);
    }
}

void AudioConferenceMixerImpl::ClearAudioFrameList(ListWrapper& audioFrameList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "ClearAudioFrameList(audioFrameList)");
    ListItem* item = audioFrameList.First();
    while(item)
    {
        AudioFrame* audioFrame = static_cast<AudioFrame*>(item->GetItem());
        _audioFramePool->PushMemory(audioFrame);
        audioFrameList.Erase(item);
        item = audioFrameList.First();
    }
}

void AudioConferenceMixerImpl::UpdateVADPositiveParticipants(
    ListWrapper& mixList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "UpdateVADPositiveParticipants(mixList)");

    ListItem* item = mixList.First();
    while(item != NULL)
    {
        AudioFrame* audioFrame = static_cast<AudioFrame*>(item->GetItem());
        CalculateEnergy(*audioFrame);
        if(audioFrame->_vadActivity == AudioFrame::kVadActive)
        {
            _scratchVadPositiveParticipants[
                _scratchVadPositiveParticipantsAmount].participant =
                audioFrame->_id;
            _scratchVadPositiveParticipants[
                _scratchVadPositiveParticipantsAmount].level =
                audioFrame->_volume;
            _scratchVadPositiveParticipantsAmount++;
        }
        item = mixList.Next(item);
    }
}

bool AudioConferenceMixerImpl::IsParticipantInList(
    MixerParticipant& participant,
    ListWrapper& participantList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "IsParticipantInList(participant,participantList)");
    ListItem* item = participantList.First();
    while(item != NULL)
    {
        MixerParticipant* rhsParticipant =
            static_cast<MixerParticipant*>(item->GetItem());
        if(&participant == rhsParticipant)
        {
            return true;
        }
        item = participantList.Next(item);
    }
    return false;
}

bool AudioConferenceMixerImpl::AddParticipantToList(
    MixerParticipant& participant,
    ListWrapper& participantList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "AddParticipantToList(participant, participantList)");
    if(participantList.PushBack(static_cast<void*>(&participant)) == -1)
    {
        return false;
    }
    // Make sure that the mixed status is correct for new MixerParticipant.
    participant._mixHistory->ResetMixedStatus();
    return true;
}

bool AudioConferenceMixerImpl::RemoveParticipantFromList(
    MixerParticipant& participant,
    ListWrapper& participantList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "RemoveParticipantFromList(participant, participantList)");
    ListItem* item = participantList.First();
    while(item)
    {
        if(item->GetItem() == &participant)
        {
            participantList.Erase(item);
            // Participant is no longer mixed, reset to default.
            participant._mixHistory->ResetMixedStatus();
            return true;
        }
        item = participantList.Next(item);
    }
    return false;
}

WebRtc_Word32 AudioConferenceMixerImpl::MixFromList(AudioFrame& mixedAudioFrame,
                                                    ListWrapper& audioFrameList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "MixFromList(mixedAudioFrame, audioFrameList)");
    WebRtc_UWord32 position = 0;
    ListItem* item = audioFrameList.First();
    while(item != NULL)
    {
        if(position >= kMaximumAmountOfMixedParticipants)
        {
            WEBRTC_TRACE(
                kTraceMemory,
                kTraceAudioMixerServer,
                _id,
                "Trying to mix more than max amount of mixed participants:%d!",
                kMaximumAmountOfMixedParticipants);
            // Assert and avoid crash
            assert(false);
            position = 0;
        }
        AudioFrame* audioFrame = static_cast<AudioFrame*>(item->GetItem());
        mixedAudioFrame += *audioFrame;

        _scratchMixedParticipants[position].participant = audioFrame->_id;
        _scratchMixedParticipants[position].level = audioFrame->_volume;

        position++;
        item = audioFrameList.Next(item);
    }
    return 0;
}

WebRtc_Word32 AudioConferenceMixerImpl::MixAnonomouslyFromList(
    AudioFrame& mixedAudioFrame,
    ListWrapper& audioFrameList)
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioMixerServer, _id,
                 "MixAnonomouslyFromList(mixedAudioFrame, audioFrameList)");
    ListItem* item = audioFrameList.First();
    while(item != NULL)
    {
        AudioFrame* audioFrame = static_cast<AudioFrame*>(item->GetItem());
        mixedAudioFrame += *audioFrame;
        item = audioFrameList.Next(item);
    }
    return 0;
}
} // namespace webrtc
