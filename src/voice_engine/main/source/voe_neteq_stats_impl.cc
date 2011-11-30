/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_neteq_stats_impl.h"

#include "audio_coding_module.h"
#include "channel.h"
#include "critical_section_wrapper.h"
#include "trace.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"


namespace webrtc {

VoENetEqStats* VoENetEqStats::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s =
        reinterpret_cast<VoiceEngineImpl*> (voiceEngine);
    VoENetEqStatsImpl* d = s;
    (*d)++;
    return (d);
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API

VoENetEqStatsImpl::VoENetEqStatsImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "VoENetEqStatsImpl::VoENetEqStatsImpl() - ctor");
}

VoENetEqStatsImpl::~VoENetEqStatsImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "VoENetEqStatsImpl::~VoENetEqStatsImpl() - dtor");
}

int VoENetEqStatsImpl::Release()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "VoENetEqStats::Release()");
    (*this)--;
    int refCount = GetCount();
    if (refCount < 0)
    {
        Reset();  // reset reference counter to zero => OK to delete VE
        _engineStatistics.SetLastError(
            VE_INTERFACE_NOT_FOUND, kTraceWarning);
        return (-1);
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "VoENetEqStats reference counter = %d", refCount);
    return (refCount);
}

int VoENetEqStatsImpl::GetNetworkStatistics(int channel,
                                            NetworkStatistics& stats)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetNetworkStatistics(channel=%d, stats=?)", channel);
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();
	
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
	voe::ScopedChannel sc(_channelManager, channel);
	voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "GetNetworkStatistics() failed to locate channel");
        return -1;
    }

    return channelPtr->GetNetworkStatistics(stats);
}

#endif  // #ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API

}   // namespace webrtc
