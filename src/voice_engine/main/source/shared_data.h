/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_SHARED_DATA_H
#define WEBRTC_VOICE_ENGINE_SHARED_DATA_H

#include "voice_engine_defines.h"

#include "channel_manager.h"
#include "statistics.h"
#include "process_thread.h"

#include "audio_device.h"
#include "audio_processing.h"

class ProcessThread;

namespace webrtc {
class CriticalSectionWrapper;

namespace voe {

class TransmitMixer;
class OutputMixer;
class SharedData

{
protected:
    WebRtc_UWord16 NumOfSendingChannels();
protected:
    const WebRtc_UWord32 _instanceId;
    CriticalSectionWrapper* _apiCritPtr;
    ChannelManager _channelManager;
    Statistics _engineStatistics;
    AudioDeviceModule* _audioDevicePtr;
    OutputMixer* _outputMixerPtr;
    TransmitMixer* _transmitMixerPtr;
    AudioProcessing* _audioProcessingModulePtr;
    ProcessThread* _moduleProcessThreadPtr;

protected:
    bool _externalRecording;
    bool _externalPlayout;

    AudioDeviceModule::AudioLayer _audioDeviceLayer;

protected:
    SharedData();
    virtual ~SharedData();
};

} //  namespace voe

} //  namespace webrtc
#endif // WEBRTC_VOICE_ENGINE_SHARED_DATA_H
