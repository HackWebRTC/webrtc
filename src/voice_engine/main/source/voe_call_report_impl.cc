/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voe_call_report_impl.h"

#include "audio_processing.h"
#include "channel.h"
#include "critical_section_wrapper.h"
#include "file_wrapper.h"
#include "trace.h"
#include "voe_errors.h"
#include "voice_engine_impl.h"

namespace webrtc
{

VoECallReport* VoECallReport::GetInterface(VoiceEngine* voiceEngine)
{
#ifndef WEBRTC_VOICE_ENGINE_CALL_REPORT_API
    return NULL;
#else
    if (NULL == voiceEngine)
    {
        return NULL;
    }
    VoiceEngineImpl* s =
            reinterpret_cast<VoiceEngineImpl*> (voiceEngine);
    VoECallReportImpl* d = s;
    (*d)++;
    return (d);
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_CALL_REPORT_API

VoECallReportImpl::VoECallReportImpl() :
    _file(*FileWrapper::Create())
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "VoECallReportImpl() - ctor");
}

VoECallReportImpl::~VoECallReportImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_instanceId, -1),
                 "~VoECallReportImpl() - dtor");
    delete &_file;
}

int VoECallReportImpl::Release()
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "VoECallReportImpl::Release()");
    (*this)--;
    int refCount = GetCount();
    if (refCount < 0)
    {
        Reset();
        _engineStatistics.SetLastError(VE_INTERFACE_NOT_FOUND,
                                       kTraceWarning);
        return (-1);
    }
    WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "VoECallReportImpl reference counter = %d", refCount);
    return (refCount);
}

int VoECallReportImpl::ResetCallReportStatistics(int channel)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "ResetCallReportStatistics(channel=%d)", channel);
    ANDROID_NOT_SUPPORTED();IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    assert(_audioProcessingModulePtr != NULL);

    bool levelMode =
        _audioProcessingModulePtr->level_estimator()->is_enabled();
    bool echoMode =
        _audioProcessingModulePtr->echo_cancellation()->are_metrics_enabled();

    // We always set the same mode for the level and echo
    if (levelMode != echoMode)
    {
        _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                                       "ResetCallReportStatistics() level mode "
                                       "and echo mode are not the same");
        return -1;
    }
    WEBRTC_TRACE(kTraceInfo, kTraceVoice, VoEId(_instanceId, -1),
                 "  current AudioProcessingModule metric currentState %d",
                 levelMode);
    // Reset the APM statistics
    if ((_audioProcessingModulePtr->level_estimator()->Enable(true) != 0)
      || (_audioProcessingModulePtr->echo_cancellation()->enable_metrics(true)
      != 0))
    {
        _engineStatistics.SetLastError(VE_APM_ERROR, kTraceError,
                                       "ResetCallReportStatistics() unable to "
                                       "set the AudioProcessingModule metrics "
                                       "state");
        return -1;
    }
    // Restore metric states
    _audioProcessingModulePtr->level_estimator()->Enable(levelMode);
    _audioProcessingModulePtr->echo_cancellation()->enable_metrics(echoMode);

    // Reset channel dependent statistics
    if (channel != -1)
    {
        voe::ScopedChannel sc(_channelManager, channel);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr == NULL)
        {
            _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                           "ResetCallReportStatistics() failed "
                                           "to locate channel");
            return -1;
        }
        channelPtr->ResetDeadOrAliveCounters();
        channelPtr->ResetRTCPStatistics();
    }
    else
    {
        WebRtc_Word32 numOfChannels = _channelManager.NumOfChannels();
        if (numOfChannels <= 0)
        {
            return 0;
        }
        WebRtc_Word32* channelsArray = new WebRtc_Word32[numOfChannels];
        _channelManager.GetChannelIds(channelsArray, numOfChannels);
        for (int i = 0; i < numOfChannels; i++)
        {
            voe::ScopedChannel sc(_channelManager, channelsArray[i]);
            voe::Channel* channelPtr = sc.ChannelPtr();
            if (channelPtr)
            {
                channelPtr->ResetDeadOrAliveCounters();
                channelPtr->ResetRTCPStatistics();
            }
        }
        delete[] channelsArray;
    }

    return 0;
}

int VoECallReportImpl::GetSpeechAndNoiseSummary(LevelStatistics& stats)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetSpeechAndNoiseSummary()");
    ANDROID_NOT_SUPPORTED();IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    assert(_audioProcessingModulePtr != NULL);

    return (GetSpeechAndNoiseSummaryInternal(stats));
}

int VoECallReportImpl::GetSpeechAndNoiseSummaryInternal(LevelStatistics& stats)
{
    int ret(0);
    bool mode(false);
    LevelEstimator::Metrics metrics;
    LevelEstimator::Metrics reverseMetrics;

    // Ensure that level metrics is enabled
    mode = _audioProcessingModulePtr->level_estimator()->is_enabled();
    if (mode != false)
    {
        ret = _audioProcessingModulePtr->level_estimator()->GetMetrics(
            &metrics, &reverseMetrics);
        if (ret != 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                       "  GetSpeechAndNoiseSummary(), AudioProcessingModule "
                       "level metrics error");
        }
    }
    else
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "  GetSpeechAndNoiseSummary(), AudioProcessingModule level "
                   "metrics is not enabled");
    }

    if ((ret != 0) || (mode == false))
    {
        // Mark complete struct as invalid (-100 dBm0)
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "  unable to retrieve level metrics from the "
                   "AudioProcessingModule");
        stats.noise_rx.min = -100;
        stats.noise_rx.max = -100;
        stats.noise_rx.average = -100;
        stats.speech_rx.min = -100;
        stats.speech_rx.max = -100;
        stats.speech_rx.average = -100;
        stats.noise_tx.min = -100;
        stats.noise_tx.max = -100;
        stats.noise_tx.average = -100;
        stats.speech_tx.min = -100;
        stats.speech_tx.max = -100;
        stats.speech_tx.average = -100;
    }
    else
    {
        // Deliver output results to user
        stats.noise_rx.min = reverseMetrics.noise.minimum;
        stats.noise_rx.max = reverseMetrics.noise.maximum;
        stats.noise_rx.average = reverseMetrics.noise.average;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                   "  noise_rx: min=%d, max=%d, avg=%d", stats.noise_rx.min,
                   stats.noise_rx.max, stats.noise_rx.average);

        stats.noise_tx.min = metrics.noise.minimum;
        stats.noise_tx.max = metrics.noise.maximum;
        stats.noise_tx.average = metrics.noise.average;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                   "  noise_tx: min=%d, max=%d, avg=%d", stats.noise_tx.min,
                   stats.noise_tx.max, stats.noise_tx.average);

        stats.speech_rx.min = reverseMetrics.speech.minimum;
        stats.speech_rx.max = reverseMetrics.speech.maximum;
        stats.speech_rx.average = reverseMetrics.speech.average;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                   "  speech_rx: min=%d, max=%d, avg=%d", stats.speech_rx.min,
                   stats.speech_rx.max, stats.speech_rx.average);

        stats.speech_tx.min = metrics.speech.minimum;
        stats.speech_tx.max = metrics.speech.maximum;
        stats.speech_tx.average = metrics.speech.average;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                   "  speech_tx: min=%d, max=%d, avg=%d", stats.speech_tx.min,
                   stats.speech_tx.max, stats.speech_tx.average);
    }
    return 0;
}

int VoECallReportImpl::GetEchoMetricSummary(EchoStatistics& stats)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetEchoMetricSummary()");
    ANDROID_NOT_SUPPORTED();IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    assert(_audioProcessingModulePtr != NULL);

    return (GetEchoMetricSummaryInternal(stats));
}

int VoECallReportImpl::GetEchoMetricSummaryInternal(EchoStatistics& stats)
{
    // Retrieve echo metrics from the AudioProcessingModule
    int ret(0);
    bool mode(false);
    EchoCancellation::Metrics metrics;

    // Ensure that echo metrics is enabled

    mode =
        _audioProcessingModulePtr->echo_cancellation()->are_metrics_enabled();
    if (mode != false)
    {
        ret =
          _audioProcessingModulePtr->echo_cancellation()->GetMetrics(&metrics);
        if (ret != 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                       "  AudioProcessingModule GetMetrics() => error");
        }
    }
    else
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "  AudioProcessingModule echo metrics is not enabled");
    }

    if ((ret != 0) || (mode == false))
    {
        // Mark complete struct as invalid (-100 dB)
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, VoEId(_instanceId, -1),
                   "  unable to retrieve echo metrics from the "
                   "AudioProcessingModule");
        stats.erl.min = -100;
        stats.erl.max = -100;
        stats.erl.average = -100;
        stats.erle.min = -100;
        stats.erle.max = -100;
        stats.erle.average = -100;
        stats.rerl.min = -100;
        stats.rerl.max = -100;
        stats.rerl.average = -100;
        stats.a_nlp.min = -100;
        stats.a_nlp.max = -100;
        stats.a_nlp.average = -100;
    }
    else
    {

        // Deliver output results to user
        stats.erl.min = metrics.echo_return_loss.minimum;
        stats.erl.max = metrics.echo_return_loss.maximum;
        stats.erl.average = metrics.echo_return_loss.average;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                   "  erl: min=%d, max=%d, avg=%d", stats.erl.min,
                   stats.erl.max, stats.erl.average);

        stats.erle.min = metrics.echo_return_loss_enhancement.minimum;
        stats.erle.max = metrics.echo_return_loss_enhancement.maximum;
        stats.erle.average = metrics.echo_return_loss_enhancement.average;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                   "  erle: min=%d, max=%d, avg=%d", stats.erle.min,
                   stats.erle.max, stats.erle.average);

        stats.rerl.min = metrics.residual_echo_return_loss.minimum;
        stats.rerl.max = metrics.residual_echo_return_loss.maximum;
        stats.rerl.average = metrics.residual_echo_return_loss.average;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                   "  rerl: min=%d, max=%d, avg=%d", stats.rerl.min,
                   stats.rerl.max, stats.rerl.average);

        stats.a_nlp.min = metrics.a_nlp.minimum;
        stats.a_nlp.max = metrics.a_nlp.maximum;
        stats.a_nlp.average = metrics.a_nlp.average;
        WEBRTC_TRACE(kTraceStateInfo, kTraceVoice, VoEId(_instanceId, -1),
                   "  a_nlp: min=%d, max=%d, avg=%d", stats.a_nlp.min,
                   stats.a_nlp.max, stats.a_nlp.average);
    }
    return 0;
}

int VoECallReportImpl::GetRoundTripTimeSummary(int channel, StatVal& delaysMs)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetRoundTripTimeSummary()");
    ANDROID_NOT_SUPPORTED();IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "GetRoundTripTimeSummary() failed to "
                                       "locate channel");
        return -1;
    }

    return channelPtr->GetRoundTripTimeSummary(delaysMs);
}

int VoECallReportImpl::GetDeadOrAliveSummary(int channel,
                                             int& numOfDeadDetections,
                                             int& numOfAliveDetections)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetDeadOrAliveSummary(channel=%d)", channel);
    ANDROID_NOT_SUPPORTED();IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    return (GetDeadOrAliveSummaryInternal(channel, numOfDeadDetections,
                                          numOfAliveDetections));
}

int VoECallReportImpl::GetDeadOrAliveSummaryInternal(int channel,
                                                     int& numOfDeadDetections,
                                                     int& numOfAliveDetections)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "GetDeadOrAliveSummary(channel=%d)", channel);

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }
    voe::ScopedChannel sc(_channelManager, channel);
    voe::Channel* channelPtr = sc.ChannelPtr();
    if (channelPtr == NULL)
    {
        _engineStatistics.SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                                       "GetRoundTripTimeSummary() failed to "
                                       "locate channel");
        return -1;
    }

    return channelPtr->GetDeadOrAliveCounters(numOfDeadDetections,
                                              numOfAliveDetections);
}

int VoECallReportImpl::WriteReportToFile(const char* fileNameUTF8)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_instanceId, -1),
                 "WriteReportToFile(fileNameUTF8=%s)", fileNameUTF8);
    ANDROID_NOT_SUPPORTED();IPHONE_NOT_SUPPORTED();

    if (!_engineStatistics.Initialized())
    {
        _engineStatistics.SetLastError(VE_NOT_INITED, kTraceError);
        return -1;
    }

    if (NULL == fileNameUTF8)
    {
        _engineStatistics.SetLastError(VE_INVALID_ARGUMENT, kTraceError,
                                       "WriteReportToFile() invalid filename");
        return -1;
    }

    if (_file.Open())
    {
        _file.CloseFile();
    }

    // Open text file in write mode
    if (_file.OpenFile(fileNameUTF8, false, false, true) != 0)
    {
        _engineStatistics.SetLastError(VE_BAD_FILE, kTraceError,
                                       "WriteReportToFile() unable to open the "
                                       "file");
        return -1;
    }

    // Summarize information and add it to the open file
    //
    _file.WriteText("WebRtc VoiceEngine Call Report\n");
    _file.WriteText("==============================\n");
    _file.WriteText("\nNetwork Packet Round Trip Time (RTT)\n");
    _file.WriteText("------------------------------------\n\n");

    WebRtc_Word32 numOfChannels = _channelManager.NumOfChannels();
    if (numOfChannels <= 0)
    {
        return 0;
    }
    WebRtc_Word32* channelsArray = new WebRtc_Word32[numOfChannels];
    _channelManager.GetChannelIds(channelsArray, numOfChannels);
    for (int ch = 0; ch < numOfChannels; ch++)
    {
        voe::ScopedChannel sc(_channelManager, channelsArray[ch]);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr)
        {
            StatVal delaysMs;
            _file.WriteText("channel %d:\n", ch);
            channelPtr->GetRoundTripTimeSummary(delaysMs);
            _file.WriteText("  min:%5d [ms]\n", delaysMs.min);
            _file.WriteText("  max:%5d [ms]\n", delaysMs.max);
            _file.WriteText("  avg:%5d [ms]\n", delaysMs.average);
        }
    }

    _file.WriteText("\nDead-or-Alive Connection Detections\n");
    _file.WriteText("------------------------------------\n\n");

    for (int ch = 0; ch < numOfChannels; ch++)
    {
        voe::ScopedChannel sc(_channelManager, channelsArray[ch]);
        voe::Channel* channelPtr = sc.ChannelPtr();
        if (channelPtr)
        {
            int nDead(0);
            int nAlive(0);
            _file.WriteText("channel %d:\n", ch);
            GetDeadOrAliveSummary(ch, nDead, nAlive);
            _file.WriteText("  #dead :%6d\n", nDead);
            _file.WriteText("  #alive:%6d\n", nAlive);
        }
    }

    delete[] channelsArray;

    LevelStatistics stats;
    GetSpeechAndNoiseSummary(stats);

    _file.WriteText("\nLong-term Speech Levels\n");
    _file.WriteText("-----------------------\n\n");

    _file.WriteText("Transmitting side:\n");
    _file.WriteText("  min:%5d [dBm0]\n", stats.speech_tx.min);
    _file.WriteText("  max:%5d [dBm0]\n", stats.speech_tx.max);
    _file.WriteText("  avg:%5d [dBm0]\n", stats.speech_tx.average);
    _file.WriteText("\nReceiving side:\n");
    _file.WriteText("  min:%5d [dBm0]\n", stats.speech_rx.min);
    _file.WriteText("  max:%5d [dBm0]\n", stats.speech_rx.max);
    _file.WriteText("  avg:%5d [dBm0]\n", stats.speech_rx.average);

    _file.WriteText("\nLong-term Noise Levels\n");
    _file.WriteText("----------------------\n\n");

    _file.WriteText("Transmitting side:\n");
    _file.WriteText("  min:%5d [dBm0]\n", stats.noise_tx.min);
    _file.WriteText("  max:%5d [dBm0]\n", stats.noise_tx.max);
    _file.WriteText("  avg:%5d [dBm0]\n", stats.noise_tx.average);
    _file.WriteText("\nReceiving side:\n");
    _file.WriteText("  min:%5d [dBm0]\n", stats.noise_rx.min);
    _file.WriteText("  max:%5d [dBm0]\n", stats.noise_rx.max);
    _file.WriteText("  avg:%5d [dBm0]\n", stats.noise_rx.average);

    EchoStatistics echo;
    GetEchoMetricSummary(echo);

    _file.WriteText("\nEcho Metrics\n");
    _file.WriteText("------------\n\n");

    _file.WriteText("erl:\n");
    _file.WriteText("  min:%5d [dB]\n", echo.erl.min);
    _file.WriteText("  max:%5d [dB]\n", echo.erl.max);
    _file.WriteText("  avg:%5d [dB]\n", echo.erl.average);
    _file.WriteText("\nerle:\n");
    _file.WriteText("  min:%5d [dB]\n", echo.erle.min);
    _file.WriteText("  max:%5d [dB]\n", echo.erle.max);
    _file.WriteText("  avg:%5d [dB]\n", echo.erle.average);
    _file.WriteText("rerl:\n");
    _file.WriteText("  min:%5d [dB]\n", echo.rerl.min);
    _file.WriteText("  max:%5d [dB]\n", echo.rerl.max);
    _file.WriteText("  avg:%5d [dB]\n", echo.rerl.average);
    _file.WriteText("a_nlp:\n");
    _file.WriteText("  min:%5d [dB]\n", echo.a_nlp.min);
    _file.WriteText("  max:%5d [dB]\n", echo.a_nlp.max);
    _file.WriteText("  avg:%5d [dB]\n", echo.a_nlp.average);

    _file.WriteText("\n<END>");

    _file.Flush();
    _file.CloseFile();

    return 0;
}

#endif  // WEBRTC_VOICE_ENGINE_CALL_REPORT_API

} // namespace webrtc
