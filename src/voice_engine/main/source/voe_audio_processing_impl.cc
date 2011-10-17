/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_audio_processing_impl.h"

#include "audio_processing.h"
#include "channel.h"
#include "critical_section_wrapper.h"
#include "trace.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

namespace webrtc {

VoEAudioProcessing* VoEAudioProcessing::GetInterface(
    VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s = reinterpret_cast<VoiceEngineImpl*> (voiceEngine);
    VoEAudioProcessingImpl* d = s;
    (*d)++;
    return (d);
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API
VoEAudioProcessingImpl::VoEAudioProcessingImpl():
    _isAecMode(WEBRTC_VOICE_ENGINE_EC_DEFAULT_MODE == EcAec?
        true : false)
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEAudioProcessingImpl::VoEAudioProcessingImpl() - ctor");
}

VoEAudioProcessingImpl::~VoEAudioProcessingImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEAudioProcessingImpl::~VoEAudioProcessingImpl() - dtor");
}

int VoEAudioProcessingImpl::Release()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "VoEAudioProcessing::Release()");
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
                 "VoEAudioProcessing reference counter = %d", refCount);
    return (refCount);
}

int VoEAudioProcessingImpl::SetNsStatus(bool enable, NsModes mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetNsStatus(enable=%d, mode=%d)", enable, mode);
#ifdef WEBRTC_VOICE_ENGINE_NR
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    NoiseSuppression::Level nsLevel(
        (NoiseSuppression::Level)WEBRTC_VOICE_ENGINE_NS_DEFAULT_MODE);
    switch (mode)
    {
    case kNsDefault:
        nsLevel = (NoiseSuppression::Level)WEBRTC_VOICE_ENGINE_NS_DEFAULT_MODE;
        break; 
    case kNsUnchanged:
        nsLevel = _audioProcessingModulePtr->noise_suppression()->level();
        break;
    case kNsConference:
        nsLevel = NoiseSuppression::kHigh;
        break;
    case kNsLowSuppression:
        nsLevel = NoiseSuppression::kLow;
        break;
    case kNsModerateSuppression:
        nsLevel = NoiseSuppression::kModerate;
        break;
    case kNsHighSuppression:
        nsLevel = NoiseSuppression::kHigh;
        break;
    case kNsVeryHighSuppression:
        nsLevel = NoiseSuppression::kVeryHigh;
        break;
    default:
        _engineStatistics.SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SetNsStatus() invalid Ns mode");
        return -1;
    }

    if (_audioProcessingModulePtr->noise_suppression()->set_level(nsLevel) != 0)
    {
        _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                                       "SetNsStatus() failed to set Ns mode");
        return -1;
    }
    if (_audioProcessingModulePtr->noise_suppression()->Enable(enable) != 0)
    {
        _engineStatistics.SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetNsStatus() failed to set Ns state");
        return -1;
    }

    return 0;
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetNsStatus() Ns is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::GetNsStatus(bool& enabled, NsModes& mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetNsStatus(enabled=?, mode=?)");
#ifdef WEBRTC_VOICE_ENGINE_NR
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    bool enable(false);
    NoiseSuppression::Level nsLevel(
        (NoiseSuppression::Level)WEBRTC_VOICE_ENGINE_NS_DEFAULT_MODE);

    enable = _audioProcessingModulePtr->noise_suppression()->is_enabled();
    nsLevel = _audioProcessingModulePtr->noise_suppression()->level();

    enabled = enable;

    switch (nsLevel)
    {
        case NoiseSuppression::kLow:
            mode = kNsLowSuppression;
            break;
        case NoiseSuppression::kModerate:
            mode = kNsModerateSuppression;
            break;
        case NoiseSuppression::kHigh:
            mode = kNsHighSuppression;
            break;
        case NoiseSuppression::kVeryHigh:
        mode = kNsVeryHighSuppression;
        break;
        default:
            _engineStatistics.SetLastError(
                            VE_APM_ERROR, kTraceError,
                            "GetNsStatus() invalid Ns mode");
            return -1;
    }

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "GetNsStatus() => enabled=% d, mode=%d",enabled, mode);
    return 0;
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "GetNsStatus() Ns is not supported");
    return -1;
#endif	
}

int VoEAudioProcessingImpl::SetAgcStatus(bool enable, AgcModes mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetAgcStatus(enable=%d, mode=%d)", enable, mode);
#ifdef WEBRTC_VOICE_ENGINE_AGC
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

#if defined(MAC_IPHONE) || defined(ATA) || defined(WEBRTC_ANDROID)
    if (mode == kAgcAdaptiveAnalog)
    {
        _engineStatistics.SetLastError(
            VE_INVALID_ARGUMENT, kTraceError,
            "SetAgcStatus() invalid Agc mode for mobile device");
        return -1;
    }
#endif

    GainControl::Mode agcMode(
        (GainControl::Mode)WEBRTC_VOICE_ENGINE_AGC_DEFAULT_MODE);
    switch (mode)
    {
    case kAgcDefault:
        agcMode = (GainControl::Mode)WEBRTC_VOICE_ENGINE_AGC_DEFAULT_MODE;
        break; 
    case kAgcUnchanged:
        agcMode = _audioProcessingModulePtr->gain_control()->mode();;
        break;
    case kAgcFixedDigital:
        agcMode = GainControl::kFixedDigital;
        break;
    case kAgcAdaptiveAnalog:
        agcMode = GainControl::kAdaptiveAnalog;
        break;
    case kAgcAdaptiveDigital:
        agcMode = GainControl::kAdaptiveDigital;
        break;
    default:
        _engineStatistics.SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                                       "SetAgcStatus() invalid Agc mode");
        return -1;
    }

    if (_audioProcessingModulePtr->gain_control()->set_mode(agcMode) != 0)
    {
        _engineStatistics.SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetAgcStatus() failed to set Agc mode");
        return -1;
    }
    if (_audioProcessingModulePtr->gain_control()->Enable(enable) != 0)
    {
        _engineStatistics.SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetAgcStatus() failed to set Agc state");
        return -1;
    }

    if (agcMode != GainControl::kFixedDigital)
    {
        // Set Agc state in the ADM when adaptive Agc mode has been selected.
        // Note that we also enable the ADM Agc when Adaptive Digital mode is
        // used since we want to be able to provide the APM with updated mic
        // levels when the user modifies the mic level manually.
        if (_audioDevicePtr->SetAGC(enable) != 0)
        {
            _engineStatistics.SetLastError(
                VE_AUDIO_DEVICE_MODULE_ERROR, kTraceWarning,
                "SetAgcStatus() failed to set Agc mode");
        }
    }

    return 0;
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetAgcStatus() Agc is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::GetAgcStatus(bool& enabled, AgcModes& mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetAgcStatus(enabled=?, mode=?)");
#ifdef WEBRTC_VOICE_ENGINE_AGC
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    bool enable(false);
    GainControl::Mode agcMode(
        (GainControl::Mode)WEBRTC_VOICE_ENGINE_AGC_DEFAULT_MODE);

    enable = _audioProcessingModulePtr->gain_control()->is_enabled();
    agcMode = _audioProcessingModulePtr->gain_control()->mode();

    enabled = enable;

    switch (agcMode)
    {
        case GainControl::kFixedDigital:
            mode = kAgcFixedDigital;
            break;
        case GainControl::kAdaptiveAnalog:
            mode = kAgcAdaptiveAnalog;
            break;
        case GainControl::kAdaptiveDigital:
            mode = kAgcAdaptiveDigital;
            break;
        default:
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                                           "GetAgcStatus() invalid Agc mode");
            return -1;
    }

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "GetAgcStatus() => enabled=%d, mode=%d", enabled, mode);
    return 0;
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "GetAgcStatus() Agc is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::SetAgcConfig(const AgcConfig config)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetAgcConfig()");
#ifdef WEBRTC_VOICE_ENGINE_AGC
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    if (_audioProcessingModulePtr->gain_control()->set_target_level_dbfs(
                    config.targetLeveldBOv) != 0)
    {
        _engineStatistics.SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetAgcConfig() failed to set target peak |level|"
            " (or envelope) of the Agc");
        return -1;
    }
    if (_audioProcessingModulePtr->gain_control()->set_compression_gain_db(
        config.digitalCompressionGaindB) != 0)
    {
        _engineStatistics.SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetAgcConfig() failed to set the range in |gain|"
            "the digital compression stage may apply");
        return -1;
    }
    if (_audioProcessingModulePtr->gain_control()->enable_limiter(
        config.limiterEnable) != 0)
    {
        _engineStatistics.SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetAgcConfig() failed to set hard limiter to the signal");
        return -1;
    }

    return 0;
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetAgcConfig() EC is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::GetAgcConfig(AgcConfig &config)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetAgcConfig(config=?)");
#ifdef WEBRTC_VOICE_ENGINE_AGC
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    config.targetLeveldBOv =
        _audioProcessingModulePtr->gain_control()->target_level_dbfs();
    config.digitalCompressionGaindB =
        _audioProcessingModulePtr->gain_control()->compression_gain_db();
    config.limiterEnable =
        _audioProcessingModulePtr->gain_control()->is_limiter_enabled();

    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
               "GetAgcConfig() => targetLeveldBOv=%u, "
               "digitalCompressionGaindB=%u, limiterEnable=%d",
		config.targetLeveldBOv,
		config.digitalCompressionGaindB,
		config.limiterEnable);

    return 0;
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "GetAgcConfig() EC is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::SetRxNsStatus(int channel,
                                          bool enable,
                                          NsModes mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetRxNsStatus(channel=%d, enable=%d, mode=%d)",
                 channel, (int)enable, (int)mode);
#ifdef WEBRTC_VOICE_ENGINE_AGC
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
            "SetRxNsStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->SetRxNsStatus(enable, mode);
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetRxNsStatus() AGC is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::GetRxNsStatus(int channel,
                                          bool& enabled,
                                          NsModes& mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRxNsStatus(channel=%d, enable=?, mode=?)", channel);
#ifdef WEBRTC_VOICE_ENGINE_AGC
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
            "GetRxNsStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRxNsStatus(enabled, mode);
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "GetRxNsStatus() Agc is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::SetRxAgcStatus(int channel,
                                           bool enable,
                                           AgcModes mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetRxAgcStatus(channel=%d, enable=%d, mode=%d)",
                 channel, (int)enable, (int)mode);
#ifdef WEBRTC_VOICE_ENGINE_AGC
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
            "SetRxAgcStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->SetRxAgcStatus(enable, mode);
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetRxAgcStatus() Agc is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::GetRxAgcStatus(int channel,
                                           bool& enabled,
                                           AgcModes& mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRxAgcStatus(channel=%d, enable=?, mode=?)", channel);
#ifdef WEBRTC_VOICE_ENGINE_AGC
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
            "GetRxAgcStatus() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRxAgcStatus(enabled, mode);
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "GetRxAgcStatus() Agc is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::SetRxAgcConfig(int channel, const AgcConfig config)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetRxAgcConfig(channel=%d)", channel);
#ifdef WEBRTC_VOICE_ENGINE_AGC
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
            "SetRxAgcConfig() failed to locate channel");
        return -1;
    }
    return channelPtr->SetRxAgcConfig(config);
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetRxAgcConfig() Agc is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::GetRxAgcConfig(int channel, AgcConfig& config)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetRxAgcConfig(channel=%d)", channel);
#ifdef WEBRTC_VOICE_ENGINE_AGC
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
            "GetRxAgcConfig() failed to locate channel");
        return -1;
    }
    return channelPtr->GetRxAgcConfig(config);
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "GetRxAgcConfig() Agc is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::SetEcStatus(bool enable, EcModes mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetEcStatus(enable=%d, mode=%d)", enable, mode);
#ifdef WEBRTC_VOICE_ENGINE_ECHO
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    // AEC mode
    if ((mode == kEcDefault) ||
        (mode == kEcConference) ||
        (mode == kEcAec) ||
        ((mode == kEcUnchanged) &&
        (_isAecMode == true)))
    {
        if (enable)
        {
            // Disable the AECM before enable the AEC
            if (_audioProcessingModulePtr->echo_control_mobile()->is_enabled())
            {
                _engineStatistics.SetLastError(
                    VE_APM_ERROR, kTraceWarning,
                    "SetEcStatus() disable AECM before enabling AEC");
                if (_audioProcessingModulePtr->echo_control_mobile()->
                    Enable(false) != 0)
                {
                    _engineStatistics.SetLastError(
                        VE_APM_ERROR, kTraceError,
                        "SetEcStatus() failed to disable AECM");
                    return -1;
                }
            }
        }
        if (_audioProcessingModulePtr->echo_cancellation()->Enable(enable) != 0)
        {
            _engineStatistics.SetLastError(
                VE_APM_ERROR, kTraceError,
                "SetEcStatus() failed to set AEC state");
            return -1;
        }
#ifdef CLOCK_SKEW_COMP
        if (_audioProcessingModulePtr->echo_cancellation()->
            enable_drift_compensation(true) != 0)
        {
            _engineStatistics.SetLastError(
                VE_APM_ERROR, kTraceError,
                "SetEcStatus() failed to enable drift compensation");
            return -1;
        }
#else
        if (_audioProcessingModulePtr->echo_cancellation()->
            enable_drift_compensation(false) != 0)
        {
            _engineStatistics.SetLastError(
                VE_APM_ERROR, kTraceError,
                "SetEcStatus() failed to disable drift compensation");
            return -1;
        }
#endif
        if (mode == kEcConference)
        {
            if (_audioProcessingModulePtr->echo_cancellation()->
                set_suppression_level(EchoCancellation::kHighSuppression) != 0)
           {
                _engineStatistics.SetLastError(
                    VE_APM_ERROR, kTraceError,
                    "SetEcStatus() failed to set aggressiveness to high");
                return -1;
            }
        }
        else
        {
           if (_audioProcessingModulePtr->echo_cancellation()->
               set_suppression_level(
                   EchoCancellation::kModerateSuppression) != 0)
           {
                _engineStatistics.SetLastError(
                    VE_APM_ERROR, kTraceError,
                    "SetEcStatus() failed to set aggressiveness to moderate");
                return -1;
           }
        }

        _isAecMode = true;
    }
    else if ((mode == kEcAecm) ||
            ((mode == kEcUnchanged) &&
            (_isAecMode == false)))
    {
        if (enable)
        {
            // Disable the AEC before enable the AECM
            if (_audioProcessingModulePtr->echo_cancellation()->is_enabled())
            {
                _engineStatistics.SetLastError(
                    VE_APM_ERROR, kTraceWarning,
                    "SetEcStatus() disable AEC before enabling AECM");
                if (_audioProcessingModulePtr->echo_cancellation()->
                    Enable(false) != 0)
                {
                    _engineStatistics.SetLastError(
                        VE_APM_ERROR, kTraceError,
                        "SetEcStatus() failed to disable AEC");
                    return -1;
                }
            }
        }
        if (_audioProcessingModulePtr->echo_control_mobile()->
            Enable(enable) != 0)
        {
            _engineStatistics.SetLastError(
                VE_APM_ERROR, kTraceError,
                "SetEcStatus() failed to set AECM state");
            return -1;
        }
        _isAecMode = false;
    }
    else
    {
        _engineStatistics.SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                                       "SetEcStatus() invalid EC mode");
        return -1;
    }

    return 0;
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetEcStatus() EC is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::GetEcStatus(bool& enabled, EcModes& mode)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetEcStatus()");
#ifdef WEBRTC_VOICE_ENGINE_ECHO
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    if (_isAecMode == true)
    {
        mode = kEcAec;
        enabled = _audioProcessingModulePtr->echo_cancellation()->is_enabled();
    }
    else
    {
        mode = kEcAecm;
        enabled = _audioProcessingModulePtr->echo_control_mobile()->
            is_enabled();
    }
    
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
                 "GetEcStatus() => enabled=%i, mode=%i",
                 enabled, (int)mode);
	return 0;
#else
	_engineStatistics.SetLastError(
	    VE_FUNC_NOT_SUPPORTED, kTraceError,
	    "GetEcStatus() EC is not supported");
	return -1;
#endif
}

int VoEAudioProcessingImpl::SetAecmMode(AecmModes mode, bool enableCNG)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetAECMMode(mode = %d)", mode);
#ifdef WEBRTC_VOICE_ENGINE_ECHO
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
	
    EchoControlMobile::RoutingMode aecmMode(
        EchoControlMobile::kQuietEarpieceOrHeadset);

    switch (mode)
    {
        case kAecmQuietEarpieceOrHeadset:
            aecmMode = EchoControlMobile::kQuietEarpieceOrHeadset;
            break;
        case kAecmEarpiece:
            aecmMode = EchoControlMobile::kEarpiece;
            break;
        case kAecmLoudEarpiece:
            aecmMode = EchoControlMobile::kLoudEarpiece;
            break;
        case kAecmSpeakerphone:
            aecmMode = EchoControlMobile::kSpeakerphone;
            break;
        case kAecmLoudSpeakerphone:
            aecmMode = EchoControlMobile::kLoudSpeakerphone;
            break;
        default:
            _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                                           "GetEcStatus() invalid EC mode");
            return -1;
    }


    if (_audioProcessingModulePtr->echo_control_mobile()->
        set_routing_mode(aecmMode) != 0)
    {
        _engineStatistics.SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetAECMMode() failed to set AECM routing mode");
        return -1;
    }
    if (_audioProcessingModulePtr->echo_control_mobile()->
        enable_comfort_noise(enableCNG) != 0)
    {
        _engineStatistics.SetLastError(
            VE_APM_ERROR, kTraceError,
            "SetAECMMode() failed to set comfort noise state for AECM");
        return -1;
    }

    return 0;
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetAECMMode() EC is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::GetAecmMode(AecmModes& mode, bool& enabledCNG)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetAECMMode(mode=?)");
#ifdef WEBRTC_VOICE_ENGINE_ECHO
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
	
    enabledCNG = false;

    EchoControlMobile::RoutingMode aecmMode =
        _audioProcessingModulePtr->echo_control_mobile()->routing_mode();
    enabledCNG = _audioProcessingModulePtr->echo_control_mobile()->
        is_comfort_noise_enabled();

    switch (aecmMode)
    {
        case EchoControlMobile::kQuietEarpieceOrHeadset:
            mode = kAecmQuietEarpieceOrHeadset;
            break;
        case EchoControlMobile::kEarpiece:
            mode = kAecmEarpiece;
            break;
        case EchoControlMobile::kLoudEarpiece:
            mode = kAecmLoudEarpiece;
            break;
        case EchoControlMobile::kSpeakerphone:
            mode = kAecmSpeakerphone;
            break;
        case EchoControlMobile::kLoudSpeakerphone:
            mode = kAecmLoudSpeakerphone;
            break;
        default:
            _engineStatistics.SetLastError(
                VE_APM_ERROR, kTraceError,
                "GetAECMMode() invalid EC mode");
            return -1;
	}

    return 0;
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "GetAECMMode() EC is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::RegisterRxVadObserver(
    int channel,
    VoERxVadCallback &observer)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "RegisterRxVadObserver()");
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
            "RegisterRxVadObserver() failed to locate channel");
        return -1;
    }
    return channelPtr->RegisterRxVadObserver(observer);
}

int VoEAudioProcessingImpl::DeRegisterRxVadObserver(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "DeRegisterRxVadObserver()");
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
            "DeRegisterRxVadObserver() failed to locate channel");
        return -1;
    }

    return channelPtr->DeRegisterRxVadObserver();
}

int VoEAudioProcessingImpl::VoiceActivityIndicator(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "VoiceActivityIndicator(channel=%d)", channel);
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
            "DeRegisterRxVadObserver() failed to locate channel");
        return -1;
    }
    int activity(-1);
    channelPtr->VoiceActivityIndicator(activity);

    return activity;
}

int VoEAudioProcessingImpl::SetEcMetricsStatus(bool enable) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
               "SetEcMetricsStatus(enable=%d)", enable);
  ANDROID_NOT_SUPPORTED();
  IPHONE_NOT_SUPPORTED();

#ifdef WEBRTC_VOICE_ENGINE_ECHO
  if (!_engineStatistics.Initialized()) {
    _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }

  if ((_audioProcessingModulePtr->echo_cancellation()->enable_metrics(enable)
      != 0) ||
      (_audioProcessingModulePtr->echo_cancellation()->enable_delay_logging(
          enable) != 0)) {
    _engineStatistics.SetLastError(
        VE_APM_ERROR, kTraceError,
        "SetEcMetricsStatus() unable to set EC metrics mode");
    return -1;
  }
  return 0;
#else
  _engineStatistics.SetLastError(
      VE_FUNC_NOT_SUPPORTED, kTraceError, "SetEcStatus() EC is not supported");
  return -1;
#endif
}

int VoEAudioProcessingImpl::GetEcMetricsStatus(bool& enabled) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
               "GetEcMetricsStatus(enabled=?)");
  ANDROID_NOT_SUPPORTED();
  IPHONE_NOT_SUPPORTED();

#ifdef WEBRTC_VOICE_ENGINE_ECHO
  if (!_engineStatistics.Initialized()) {
    _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }

  bool echo_mode =
    _audioProcessingModulePtr->echo_cancellation()->are_metrics_enabled();
  bool delay_mode =
    _audioProcessingModulePtr->echo_cancellation()->is_delay_logging_enabled();

  if (echo_mode != delay_mode) {
    _engineStatistics.SetLastError(
        VE_APM_ERROR, kTraceError,
        "GetEcMetricsStatus() delay logging and echo mode are not the same");
    return -1;
  }

  enabled = echo_mode;

  WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
               "GetEcMetricsStatus() => enabled=%d", enabled);
  return 0;
#else
  _engineStatistics.SetLastError(
      VE_FUNC_NOT_SUPPORTED, kTraceError, "SetEcStatus() EC is not supported");
  return -1;
#endif
}

int VoEAudioProcessingImpl::GetEchoMetrics(int& ERL,
                                           int& ERLE,
                                           int& RERL,
                                           int& A_NLP) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
               "GetEchoMetrics(ERL=?, ERLE=?, RERL=?, A_NLP=?)");
  ANDROID_NOT_SUPPORTED();
  IPHONE_NOT_SUPPORTED();

#ifdef WEBRTC_VOICE_ENGINE_ECHO
  if (!_engineStatistics.Initialized()) {
    _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (!_audioProcessingModulePtr->echo_cancellation()->is_enabled()) {
    _engineStatistics.SetLastError(
        VE_APM_ERROR, kTraceWarning,
        "GetEchoMetrics() AudioProcessingModule AEC is not enabled");
    return -1;
  }

  // Get Echo Metrics from Audio Processing Module.
  EchoCancellation::Metrics echoMetrics;
  if (_audioProcessingModulePtr->echo_cancellation()->GetMetrics(&echoMetrics))
  {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                 "GetEchoMetrics(), AudioProcessingModule metrics error");
    return -1;
  }

  // Echo quality metrics.
  ERL = echoMetrics.echo_return_loss.instant;
  ERLE = echoMetrics.echo_return_loss_enhancement.instant;
  RERL = echoMetrics.residual_echo_return_loss.instant;
  A_NLP = echoMetrics.a_nlp.instant;

  WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
               "GetEchoMetrics() => ERL=%d, ERLE=%d, RERL=%d, A_NLP=%d",
               ERL, ERLE, RERL, A_NLP);
  return 0;
#else
  _engineStatistics.SetLastError(
      VE_FUNC_NOT_SUPPORTED, kTraceError, "SetEcStatus() EC is not supported");
  return -1;
#endif
}

int VoEAudioProcessingImpl::GetEcDelayMetrics(int& delay_median,
                                              int& delay_std) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
               "GetEcDelayMetrics(median=?, std=?)");
  ANDROID_NOT_SUPPORTED();
  IPHONE_NOT_SUPPORTED();

#ifdef WEBRTC_VOICE_ENGINE_ECHO
  if (!_engineStatistics.Initialized()) {
    _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (!_audioProcessingModulePtr->echo_cancellation()->is_enabled()) {
    _engineStatistics.SetLastError(
        VE_APM_ERROR, kTraceWarning,
        "GetEcDelayMetrics() AudioProcessingModule AEC is not enabled");
    return -1;
  }

  int median = 0;
  int std = 0;
  // Get delay-logging values from Audio Processing Module.
  if (_audioProcessingModulePtr->echo_cancellation()->GetDelayMetrics(
      &median, &std)) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_instanceId,-1),
                 "GetEcDelayMetrics(), AudioProcessingModule delay-logging "
                 "error");
    return -1;
  }

  // EC delay-logging metrics
  delay_median = median;
  delay_std = std;

  WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId,-1),
               "GetEcDelayMetrics() => delay_median=%d, delay_std=%d",
               delay_median, delay_std);
  return 0;
#else
  _engineStatistics.SetLastError(
      VE_FUNC_NOT_SUPPORTED, kTraceError, "SetEcStatus() EC is not supported");
  return -1;
#endif
}

int VoEAudioProcessingImpl::StartDebugRecording(const char* fileNameUTF8)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
               "StartDebugRecording()");
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    return _audioProcessingModulePtr->StartDebugRecording(fileNameUTF8);

}

int VoEAudioProcessingImpl::StopDebugRecording()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "StopDebugRecording()");
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    return _audioProcessingModulePtr->StopDebugRecording();
}

int VoEAudioProcessingImpl::SetTypingDetectionStatus(bool enable)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "SetTypingDetectionStatus()");
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();
#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    // Just use the VAD state to determine if we should enable typing detection
    // or not

    if (_audioProcessingModulePtr->voice_detection()->Enable(enable))
    {
        _engineStatistics.SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "SetTypingDetectionStatus() failed to set VAD state");
        return -1;
    }
    if (_audioProcessingModulePtr->voice_detection()->set_likelihood(
        VoiceDetection::kHighLikelihood))
    {
        _engineStatistics.SetLastError(
            VE_APM_ERROR, kTraceWarning,
            "SetTypingDetectionStatus() failed to set VAD likelihood to high");
        return -1;
    }

    return 0;
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetTypingDetectionStatus is not supported");
    return -1;
#endif
}

int VoEAudioProcessingImpl::GetTypingDetectionStatus(bool& enabled)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId,-1),
                 "GetTypingDetectionStatus()");
    ANDROID_NOT_SUPPORTED();
    IPHONE_NOT_SUPPORTED();

#ifdef WEBRTC_VOICE_ENGINE_TYPING_DETECTION
    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
  	// Just use the VAD state to determine if we should enable typing
    // detection or not

    enabled = _audioProcessingModulePtr->voice_detection()->is_enabled();

    return(0);
#else
    _engineStatistics.SetLastError(
        VE_FUNC_NOT_SUPPORTED, kTraceError,
        "SetTypingDetectionStatus is not supported");
    return(-1);
#endif
}

#endif  // #ifdef WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API

}   // namespace webrtc
