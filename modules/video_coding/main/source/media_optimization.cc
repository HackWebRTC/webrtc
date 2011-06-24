/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media_optimization.h"
#include "content_metrics_processing.h"
#include "frame_dropper.h"
#include "qm_select.h"

namespace webrtc {

VCMMediaOptimization::VCMMediaOptimization(WebRtc_Word32 id):
_id(id),
_maxBitRate(0),
_sendCodecType(kVideoCodecUnknown),
_codecWidth(0),
_codecHeight(0),
_userFrameRate(0),
_lossProtOverhead(0),
_packetLossEnc(0),
_fractionLost(0),
_sendStatisticsZeroEncode(0),
_maxPayloadSize(1460),
_lastBitRate(0),
_targetBitRate(0),
_incomingFrameRate(0),
_enableQm(false),
_videoProtectionCallback(NULL),
_videoQMSettingsCallback(NULL),
_encodedFrameSamples(),
_avgSentBitRateBps(0.0f),
_keyFrameCnt(0),
_deltaFrameCnt(0),
_lastQMUpdateTime(0),
_lastChangeTime(0)
{
    memset(_sendStatistics, 0, sizeof(_sendStatistics));
    memset(_incomingFrameTimes, -1, sizeof(_incomingFrameTimes));

    _frameDropper  = new VCMFrameDropper(_id);
    _lossProtLogic = new VCMLossProtectionLogic();
    _content = new VCMContentMetricsProcessing();
    _qms = new VCMQmSelect();
}

VCMMediaOptimization::~VCMMediaOptimization(void)
{
    _lossProtLogic->ClearLossProtections();
    delete _lossProtLogic;
    delete _frameDropper;
    delete _content;
    delete _qms;
}

WebRtc_Word32
VCMMediaOptimization::Reset()
{
    memset(_incomingFrameTimes, -1, sizeof(_incomingFrameTimes));
    InputFrameRate(); // Resets _incomingFrameRate
    _frameDropper->Reset();
    _lossProtLogic->Reset();
    _frameDropper->SetRates(0, 0);
    _content->Reset();
    _qms->Reset();
    _lossProtLogic->UpdateFrameRate(_incomingFrameRate);
    _lossProtLogic->Reset();
    _sendStatisticsZeroEncode = 0;
    _lastBitRate = 0;
    _targetBitRate = 0;
    _lossProtOverhead = 0;
    _codecWidth = 0;
    _codecHeight = 0;
    _userFrameRate = 0;
    _keyFrameCnt = 0;
    _deltaFrameCnt = 0;
    _lastQMUpdateTime = 0;
    _lastChangeTime = 0;
    for (WebRtc_Word32 i = 0; i < kBitrateMaxFrameSamples; i++)
    {
        _encodedFrameSamples[i]._sizeBytes = -1;
        _encodedFrameSamples[i]._timeCompleteMs = -1;
    }
    _avgSentBitRateBps = 0.0f;
    return VCM_OK;
}

WebRtc_UWord32
VCMMediaOptimization::SetTargetRates(WebRtc_UWord32 bitRate,
                                     WebRtc_UWord8 &fractionLost,
                                     WebRtc_UWord32 roundTripTimeMs)
{
    VCMProtectionMethod *selectedMethod = _lossProtLogic->SelectedMethod();
    _lossProtLogic->UpdateBitRate(static_cast<float>(bitRate));
    _lossProtLogic->UpdateLossPr(fractionLost);
    _lossProtLogic->UpdateRtt(roundTripTimeMs);
    _lossProtLogic->UpdateResidualPacketLoss(static_cast<float>(fractionLost));

    VCMFecTypes fecType = kXORFec;  // generic FEC
    _lossProtLogic->UpdateFecType(fecType);

    // Get frame rate for encoder: this is the actual/sent frame rate
    float actualFrameRate = SentFrameRate();

    // sanity
    if (actualFrameRate  < 1.0)
    {
        actualFrameRate = 1.0;
    }

    // Update frame rate for the loss protection logic class: frame rate should
    // be the actual/sent rate
    _lossProtLogic->UpdateFrameRate(actualFrameRate);

    _fractionLost = fractionLost;

    // The effective packet loss may be the received loss or filtered, i.e.,
    // average or max filter may be used.
    // We should think about which filter is appropriate for low/high bit rates,
    // low/high loss rates, etc.
    WebRtc_UWord8 packetLossEnc = _lossProtLogic->FilteredLoss();

    //For now use the filtered loss for computing the robustness settings
    _lossProtLogic->UpdateFilteredLossPr(packetLossEnc);

    // Rate cost of the protection methods
    _lossProtOverhead = 0;

    if (selectedMethod && (selectedMethod->Type() == kFEC ||
        selectedMethod->Type() == kNackFec ))
    {

        // Update method will compute the robustness settings for the given
        // protection method and the overhead cost
        // the protection method is set by the user via SetVideoProtection.
        // The robustness settings are: the effective packet loss for ER and the
        // FEC protection settings
        _lossProtLogic->UpdateMethod();

        // Get the code rate for Key frames
        const WebRtc_UWord8 codeRateKeyRTP  = selectedMethod->RequiredProtectionFactorK();

        // Get the code rate for Delta frames
        const WebRtc_UWord8 codeRateDeltaRTP = selectedMethod->RequiredProtectionFactorD();

        // Get the effective packet loss for ER
        packetLossEnc = selectedMethod->RequiredPacketLossER();

        // NACK is on for NACK and NackFec protection method: off for FEC method
        bool nackStatus = (selectedMethod->Type() == kNackFec ||
                           selectedMethod->Type() == kNACK);

        if(_videoProtectionCallback)
        {
            _videoProtectionCallback->ProtectionRequest(codeRateDeltaRTP,
                                                        codeRateKeyRTP,
                                                        nackStatus);
        }
    }

    // Get the bit cost of protection method
    _lossProtOverhead = static_cast<WebRtc_UWord32>(_lossProtLogic->HighestOverhead() + 0.5f);

    // Update effective packet loss for encoder: note: fractionLost was passed as reference
    fractionLost = packetLossEnc;

    WebRtc_UWord32 nackBitRate=0;
    if(selectedMethod && _lossProtLogic->FindMethod(kNACK) != NULL)
    {
        // TODO(mikhal): update frame dropper with bit rate including both nack and fec
        // Make sure we don't over-use the channel momentarily. This is
        // necessary for NACK since it can be very bursty.
        nackBitRate = (_lastBitRate * fractionLost) / 255;
        if (nackBitRate > _targetBitRate)
        {
            nackBitRate = _targetBitRate;
        }
        _frameDropper->SetRates(static_cast<float>(bitRate - nackBitRate), 0);
    }
    else
    {
        _frameDropper->SetRates(static_cast<float>(bitRate - _lossProtOverhead), 0);
    }

    // This may be used for UpdateEncoderBitRate: lastBitRate is total rate,
    // before compensation
    _lastBitRate = _targetBitRate;

    //Source coding rate: total rate - protection overhead
    _targetBitRate = bitRate - _lossProtOverhead;

    if (_enableQm)
    {
        //Update QM with rates
        _qms->UpdateRates((float)_targetBitRate, _avgSentBitRateBps, _incomingFrameRate);
        //Check for QM selection
        bool selectQM = checkStatusForQMchange();
        if (selectQM)
        {
            SelectQuality();
        }
        // Reset the short-term averaged content data.
        _content->ResetShortTermAvgData();
    }

    return _targetBitRate;
}


bool
VCMMediaOptimization::DropFrame()
{
    // leak appropriate number of bytes
    _frameDropper->Leak((WebRtc_UWord32)(InputFrameRate() + 0.5f));
    return _frameDropper->DropFrame();
}

WebRtc_Word32
VCMMediaOptimization::SentFrameCount(VCMFrameCount &frameCount) const
{
    frameCount.numDeltaFrames = _deltaFrameCnt;
    frameCount.numKeyFrames = _keyFrameCnt;
    return VCM_OK;
}

WebRtc_Word32
VCMMediaOptimization::SetEncodingData(VideoCodecType sendCodecType, WebRtc_Word32 maxBitRate,
                                      WebRtc_UWord32 frameRate, WebRtc_UWord32 bitRate,
                                      WebRtc_UWord16 width, WebRtc_UWord16 height)
{
    // Everything codec specific should be reset here since this means the codec has changed.
    // If native dimension values have changed, then either user initiated change, or QM
    // initiated change. Will be able to determine only after the processing of the first frame
    _lastChangeTime = VCMTickTime::MillisecondTimestamp();
    _content->Reset();
    _content->UpdateFrameRate(frameRate);

    _maxBitRate = maxBitRate;
    _sendCodecType = sendCodecType;
    _targetBitRate = bitRate;
    _lossProtLogic->UpdateBitRate(static_cast<float>(bitRate));
    _lossProtLogic->UpdateFrameRate(static_cast<float>(frameRate));
    _frameDropper->Reset();
    _frameDropper->SetRates(static_cast<float>(bitRate), static_cast<float>(frameRate));
    _userFrameRate = (float)frameRate;
    _codecWidth = width;
    _codecHeight = height;
    WebRtc_Word32 ret = VCM_OK;
    ret = _qms->Initialize((float)_targetBitRate, _userFrameRate, _codecWidth, _codecHeight);
    return ret;
}

WebRtc_Word32
VCMMediaOptimization::RegisterProtectionCallback(VCMProtectionCallback* protectionCallback)
{
    _videoProtectionCallback = protectionCallback;
    return VCM_OK;

}


void
VCMMediaOptimization::EnableFrameDropper(bool enable)
{
    _frameDropper->Enable(enable);
}


void
VCMMediaOptimization::EnableNack(bool enable)
{
    // Add NACK to the list of loss protection methods
    bool updated = false;
    if (enable)
    {
        VCMProtectionMethod *nackMethod = new VCMNackMethod();
        updated = _lossProtLogic->AddMethod(nackMethod);
        if (!updated)
        {
            delete nackMethod;
        }
    }
    else
    {
        updated = _lossProtLogic->RemoveMethod(kNACK);
    }
    if (updated)
    {
        _lossProtLogic->UpdateMethod();
    }
}

bool
VCMMediaOptimization::IsNackEnabled()
{
    return (_lossProtLogic->FindMethod(kNACK) != NULL);
}

void
VCMMediaOptimization::EnableFEC(bool enable)
{
    // Add FEC to the list of loss protection methods
    bool updated = false;
    if (enable)
    {
        VCMProtectionMethod *fecMethod = new VCMFecMethod();
        updated = _lossProtLogic->AddMethod(fecMethod);
        if (!updated)
        {
            delete fecMethod;
        }
    }
    else
    {
        updated = _lossProtLogic->RemoveMethod(kFEC);
    }
    if (updated)
    {
        _lossProtLogic->UpdateMethod();
    }
}
void
VCMMediaOptimization::EnableNackFEC(bool enable)
{
    // Add NackFec to the list of loss protection methods
    bool updated = false;
    if (enable)
    {
        VCMProtectionMethod *nackfecMethod = new VCMNackFecMethod();
        updated = _lossProtLogic->AddMethod(nackfecMethod);
        if (!updated)
        {
            delete nackfecMethod;
        }
    }
    else
    {
        updated = _lossProtLogic->RemoveMethod(kNackFec);
    }
    if (updated)
    {
        _lossProtLogic->UpdateMethod();
    }
}

bool
VCMMediaOptimization::IsFecEnabled()
{
    return (_lossProtLogic->FindMethod(kFEC) != NULL);
}

bool
VCMMediaOptimization::IsNackFecEnabled()
{
    return (_lossProtLogic->FindMethod(kNackFec) != NULL);
}

void
VCMMediaOptimization::SetMtu(WebRtc_Word32 mtu)
{
    _maxPayloadSize = mtu;
}

float
VCMMediaOptimization::SentFrameRate()
{
    if(_frameDropper)
    {
        return _frameDropper->ActualFrameRate((WebRtc_UWord32)(InputFrameRate() + 0.5f));
    }

    return VCM_CODEC_ERROR;
}

float
VCMMediaOptimization::SentBitRate()
{
    UpdateBitRateEstimate(-1, VCMTickTime::MillisecondTimestamp());
    return _avgSentBitRateBps / 1000.0f;
}

WebRtc_Word32
VCMMediaOptimization::MaxBitRate()
{
    return _maxBitRate;
}

WebRtc_Word32
VCMMediaOptimization::UpdateWithEncodedData(WebRtc_Word32 encodedLength,
                                            FrameType encodedFrameType)
{
    // look into the ViE version - debug mode - needs also number of layers.
    UpdateBitRateEstimate(encodedLength, VCMTickTime::MillisecondTimestamp());
    if(encodedLength > 0)
    {
        const bool deltaFrame = (encodedFrameType != kVideoFrameKey &&
                                 encodedFrameType != kVideoFrameGolden);

        _frameDropper->Fill(encodedLength, deltaFrame);
        if (_maxPayloadSize > 0 && encodedLength > 0)
        {
            const float minPacketsPerFrame = encodedLength /
                                             static_cast<float>(_maxPayloadSize);
            if (deltaFrame)
            {
                _lossProtLogic->UpdatePacketsPerFrame(minPacketsPerFrame);
            }
            else
            {
                _lossProtLogic->UpdatePacketsPerFrameKey(minPacketsPerFrame);
            }

            if (_enableQm)
            {
                // update quality select with encoded length
                _qms->UpdateEncodedSize(encodedLength, encodedFrameType);
            }
        }
        if (!deltaFrame && encodedLength > 0)
        {
            _lossProtLogic->UpdateKeyFrameSize(static_cast<float>(encodedLength));
        }

        // updating counters
        if (deltaFrame){
            _deltaFrameCnt++;
        } else {
            _keyFrameCnt++;
        }

    }

     return VCM_OK;

}

void VCMMediaOptimization::UpdateBitRateEstimate(WebRtc_Word64 encodedLength,
                                                 WebRtc_Word64 nowMs)
{
    int i = kBitrateMaxFrameSamples - 1;
    WebRtc_UWord32 frameSizeSum = 0;
    WebRtc_Word64 timeOldest = -1;
    // Find an empty slot for storing the new sample and at the same time
    // accumulate the history.
    for (; i >= 0; i--)
    {
        if (_encodedFrameSamples[i]._sizeBytes == -1)
        {
            // Found empty slot
            break;
        }
        if (nowMs - _encodedFrameSamples[i]._timeCompleteMs < kBitrateAverageWinMs)
        {
            frameSizeSum += static_cast<WebRtc_UWord32>(_encodedFrameSamples[i]._sizeBytes);
            if (timeOldest == -1)
            {
                timeOldest = _encodedFrameSamples[i]._timeCompleteMs;
            }
        }
    }
    if (encodedLength > 0)
    {
        if (i < 0)
        {
            // No empty slot, shift
            for (i = kBitrateMaxFrameSamples - 2; i >= 0; i--)
            {
                _encodedFrameSamples[i + 1] = _encodedFrameSamples[i];
            }
            i++;
        }
        // Insert new sample
        _encodedFrameSamples[i]._sizeBytes = encodedLength;
        _encodedFrameSamples[i]._timeCompleteMs = nowMs;
    }
    if (timeOldest > -1)
    {
        // Update average bit rate
        float denom = static_cast<float>(nowMs - timeOldest);
        if (denom < 1.0)
        {
            denom = 1.0;
        }
        _avgSentBitRateBps = (frameSizeSum + encodedLength) * 8 * 1000 / denom;
    }
    else if (encodedLength > 0)
    {
        _avgSentBitRateBps = static_cast<float>(encodedLength * 8);
    }
    else
    {
        _avgSentBitRateBps = 0;
    }
}


WebRtc_Word32
VCMMediaOptimization::RegisterVideoQMCallback(VCMQMSettingsCallback *videoQMSettings)
{
    _videoQMSettingsCallback = videoQMSettings;
    // Callback setting controls QM
    if (_videoQMSettingsCallback != NULL)
    {
        _enableQm = true;
    }
    else
    {
        _enableQm = false;
    }
    return VCM_OK;
}

void
VCMMediaOptimization::updateContentData(const VideoContentMetrics *contentMetrics)
{
    //Updating content metrics
    if (contentMetrics == NULL)
    {
         //No QM if metrics are NULL
         _enableQm = false;
         _qms->Reset();
    }
    else
    {
        _content->UpdateContentData(contentMetrics);
    }
}

WebRtc_Word32
VCMMediaOptimization::SelectQuality()
{
    // Reset quantities for QM select
    _qms->ResetQM();

    // Select quality mode
    VCMQualityMode* qm = NULL;
    WebRtc_Word32 ret = _qms->SelectQuality(_content->LongTermAvgData(), &qm);
    if (ret < 0)
    {
          return ret;
    }

    // Check for updates to spatial/temporal modes
    QMUpdate(qm);

    // Reset all the rate and related frame counters quantities
    _qms->ResetRates();

    // Reset counters
    _lastQMUpdateTime = VCMTickTime::MillisecondTimestamp();

    // Reset content metrics
    _content->Reset();

    return VCM_OK;
}


// Check timing constraints and look for significant change in:
// (1) scene content
// (2) target bit rate

bool
VCMMediaOptimization::checkStatusForQMchange()
{

    bool status  = true;

    // Check that we do not call QMSelect too often, and that we waited some time
    // (to sample the metrics) from the event lastChangeTime
    // lastChangeTime is the time where user changed the size/rate/frame rate
    // (via SetEncodingData)
    WebRtc_Word64 now = VCMTickTime::MillisecondTimestamp();
    if ((now - _lastQMUpdateTime) < kQmMinIntervalMs ||
        (now  - _lastChangeTime) <  kQmMinIntervalMs)
    {
        status = false;
    }

    return status;

}

bool
VCMMediaOptimization::QMUpdate(VCMQualityMode* qm)
{
    // Check for no change
    if (qm->spatialHeightFact == 1 &&
        qm->spatialWidthFact == 1 &&
        qm->temporalFact == 1)
    {
        return false;
    }

    // Content metrics hold native values
    VideoContentMetrics* cm = _content->LongTermAvgData();

    // Temporal
    WebRtc_UWord32 frameRate  = static_cast<WebRtc_UWord32>(_incomingFrameRate + 0.5f);
    // Check if go back up in temporal resolution
    if (qm->temporalFact == 0)
    {
        frameRate = (WebRtc_UWord32) 2 * _incomingFrameRate;
    }
    // go down in temporal resolution
    else
    {
        frameRate = (WebRtc_UWord32)(_incomingFrameRate / qm->temporalFact + 1);
    }

    // Spatial
    WebRtc_UWord32 height = _codecHeight;
    WebRtc_UWord32 width = _codecWidth;
    // Check if go back up in spatial resolution
    if (qm->spatialHeightFact == 0 && qm->spatialWidthFact == 0)
    {
       height = cm->nativeHeight;
       width = cm->nativeWidth;
    }
    else
    {
        height = _codecHeight / qm->spatialHeightFact;
        width = _codecWidth / qm->spatialWidthFact;
    }

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, _id,
               "Quality Mode Update: W = %d, H = %d, FR = %f",
               width, height, frameRate);

    // Update VPM with new target frame rate and size
    _videoQMSettingsCallback->SetVideoQMSettings(frameRate, width, height);

    return true;
}



void
VCMMediaOptimization::UpdateIncomingFrameRate()
{
    WebRtc_Word64 now = VCMTickTime::MillisecondTimestamp();
    if(_incomingFrameTimes[0] == 0)
    {
        // first no shift
    } else
    {
        // shift
        for(WebRtc_Word32 i = (kFrameCountHistorySize - 2); i >= 0 ; i--)
        {
            _incomingFrameTimes[i+1] = _incomingFrameTimes[i];
        }
    }
    _incomingFrameTimes[0] = now;
    ProcessIncomingFrameRate(now);
}

// allowing VCM to keep track of incoming frame rate
void
VCMMediaOptimization::ProcessIncomingFrameRate(WebRtc_Word64 now)
{
    WebRtc_Word32 num = 0;
    WebRtc_Word32 nrOfFrames = 0;
    for (num = 1; num < (kFrameCountHistorySize - 1); num++)
    {
        if (_incomingFrameTimes[num] <= 0 ||
            // don't use data older than 2 s
            now - _incomingFrameTimes[num] > kFrameHistoryWinMs)
        {
            break;
        } else
        {
            nrOfFrames++;
        }
    }
    if (num > 1)
    {
        const WebRtc_Word64 diff = now - _incomingFrameTimes[num-1];
        _incomingFrameRate = 1.0;
        if(diff >0)
        {
            _incomingFrameRate = nrOfFrames * 1000.0f / static_cast<float>(diff);
        }
    }
    else
    {
        _incomingFrameRate = static_cast<float>(nrOfFrames);
    }
}

WebRtc_UWord32
VCMMediaOptimization::InputFrameRate()
{
    ProcessIncomingFrameRate(VCMTickTime::MillisecondTimestamp());
    return WebRtc_UWord32 (_incomingFrameRate + 0.5f);
}

}
