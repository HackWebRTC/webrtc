/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_coding_defines.h"
#include "fec_tables_xor.h"
#include "er_tables_xor.h"
#include "nack_fec_tables.h"
#include "qm_select_data.h"
#include "media_opt_util.h"

#include <math.h>
#include <float.h>
#include <limits.h>

namespace webrtc {

void
VCMProtectionMethod::UpdateContentMetrics(
                     const VideoContentMetrics*  contentMetrics)
{
   _qmRobustness->UpdateContent(contentMetrics);
}

bool
VCMProtectionMethod::BetterThan(VCMProtectionMethod *pm)
{
  if (pm == NULL)
  {
      return true;
  }
  return pm->_score > _score;
}

bool
VCMNackFecMethod::ProtectionFactor(const VCMProtectionParameters* /*parameters*/)
{
    // use FEC model with modification with RTT for now
    return true;
}

bool
VCMNackFecMethod::EffectivePacketLoss(const
                                      VCMProtectionParameters* /*parameters*/)
{
    // use FEC model with modification with RTT for now
    return true;
}

bool
VCMNackFecMethod::UpdateParameters(const VCMProtectionParameters* parameters)
{
    // Hybrid Nack FEC has three operational modes:
    // 1. Low RTT - Nack only (Set FEC rates to zero)
    // 2. High RTT - FEC Only
    // 3. Medium RTT values - Hybrid ; in hybrid mode, we will only nack the
    //    residual following the decoding of the FEC (refer to JB logic)

    // Low RTT - NACK only mode
    if (parameters->rtt < kLowRttNackMs)
    {
        // Set the FEC parameters to 0
        _protectionFactorK = 0;
        _protectionFactorD = 0;

        // assume packets will be restored via NACK
        // TODO: relax this assumption?
        _effectivePacketLoss = 0;
        _score = _efficiency;
        return true;
    }
    // otherwise: we count on FEC; if the RTT is below a threshold, then we can
    // nack the residual, based on a decision made in the JB.
    // TODO(mikhal): adapt the FEC rate based on the RTT, i.e. the the level on
    // which we will rely on NACK, e.g. less as we approach upper threshold.
    VCMFecMethod fecMethod;

    const WebRtc_UWord8 plossMax = 129;

    // Compute the protection factor
    fecMethod.ProtectionFactor(parameters);

    // Compute the effective packet loss
    fecMethod.EffectivePacketLoss(parameters);

    WebRtc_UWord8 protFactorK = fecMethod._protectionFactorK;
    WebRtc_UWord8 protFactorD = fecMethod._protectionFactorD;
    WebRtc_UWord8 effPacketLoss = fecMethod._effectivePacketLoss;
    float resPacketLoss = fecMethod._residualPacketLoss;

    // Correct FEC rates based on the RTT ( NACK effectiveness)
    WebRtc_Word16 rttIndex= (WebRtc_UWord16) parameters->rtt;
    float softnessRtt = 1.0;
    if (parameters->rtt < kHighRttNackMs)
    {
        // TODO(mikhal): update table
         softnessRtt = (float)VCMNackFecTable[rttIndex] / (float)4096.0;

        // soften ER with NACK on
        // table depends on RTT relative to rttMax (NACK Threshold)
        _effectivePacketLoss = (WebRtc_UWord8)(effPacketLoss * softnessRtt);

        // soften FEC with NACK on
        // table depends on RTT relative to rttMax (NACK Threshold)
        _protectionFactorK = (WebRtc_UWord8) (protFactorK * softnessRtt);
        _protectionFactorD = (WebRtc_UWord8) (protFactorD * softnessRtt);
    }
    // else - NACK is disabled, rely on FEC only


    // make sure I frame protection is at least larger than P frame protection,
    // and at least as high as received loss
    WebRtc_UWord8 packetLoss = (WebRtc_UWord8) (255 * parameters->lossPr);
    _protectionFactorK = static_cast<WebRtc_UWord8> (VCM_MAX(packetLoss,
        VCM_MAX(_scaleProtKey * protFactorD, protFactorK)));

    // check limit on amount of protection for I frame: 50% is max
    if (_protectionFactorK >= plossMax)
        _protectionFactorK = plossMax - 1;

    // Bit cost for NackFec

    // NACK cost: based on residual packet loss (since we should only NACK
    // packets not recovered by FEC)
    _efficiency = 0.0f;
    if (parameters->rtt < kHighRttNackMs)
    {
        _efficiency = parameters->bitRate * resPacketLoss /
                                  (1.0f + resPacketLoss);
    }
    else
    {
        // efficiency based on FEC only
        // add FEC cost: ignore I frames for now
        float fecRate = static_cast<float> (_protectionFactorD) / 255.0f;
        if (fecRate >= 0.0f)
            _efficiency += parameters->bitRate * fecRate;
    }
    _score = _efficiency;

    // Protection/fec rates obtained above are defined relative to total number
    // of packets (total rate: source + fec) FEC in RTP module assumes
    // protection factor is defined relative to source number of packets so we
    // should convert the factor to reduce mismatch between mediaOpt's rate and
    // the actual one
    WebRtc_UWord8 codeRate = protFactorK;
    _protectionFactorK = fecMethod.ConvertFECRate(codeRate);
    codeRate = protFactorD;
    _protectionFactorD = fecMethod.ConvertFECRate(codeRate);

    return true;
}

bool
VCMNackMethod::EffectivePacketLoss(WebRtc_UWord8 effPacketLoss,
                                   WebRtc_UWord16 rttTime)
{
    WebRtc_UWord16 rttMax = MaxRttNack();

    // For large RTT, we should rely on some Error Resilience, so we set
    // packetLossEnc = 0 for RTT less than the NACK threshold
    if (rttTime < rttMax)
    {
        effPacketLoss = 0; //may want a softer transition here
    }
    _effectivePacketLoss = effPacketLoss;

    return true;
}

bool
VCMNackMethod::UpdateParameters(const VCMProtectionParameters* parameters)
{
    // Compute the effective packet loss for ER
    WebRtc_UWord8 effPacketLoss = (WebRtc_UWord8) (255 * parameters->lossPr);
    WebRtc_UWord16 rttTime = (WebRtc_UWord16) parameters->rtt;
    EffectivePacketLoss(effPacketLoss, rttTime);

    // Compute the NACK bit cost
    _efficiency = parameters->bitRate * parameters->lossPr /
                              (1.0f + parameters->lossPr);
    _score = _efficiency;
    if (parameters->rtt > _NACK_MAX_RTT)
    {
        _score = 0.0f;
        return false;
    }
    return true;
}

WebRtc_UWord8
VCMFecMethod::BoostCodeRateKey(WebRtc_UWord8 packetFrameDelta,
                               WebRtc_UWord8 packetFrameKey) const
{
    WebRtc_UWord8 boostRateKey = 2;
    // default: ratio scales the FEC protection up for I frames
    WebRtc_UWord8 ratio = 1;

    if (packetFrameDelta > 0)
    {
        ratio = (WebRtc_Word8) (packetFrameKey / packetFrameDelta);
    }
    ratio = VCM_MAX(boostRateKey, ratio);

    return ratio;
}

WebRtc_UWord8
VCMFecMethod::ConvertFECRate(WebRtc_UWord8 codeRateRTP) const
{
    return static_cast<WebRtc_UWord8> (VCM_MIN(255,(0.5 + 255.0 * codeRateRTP /
                                      (float)(255 - codeRateRTP))));
}

// AvgRecoveryFEC: average recovery from FEC, assuming random packet loss model
// Computed offline for a range of FEC code parameters and loss rates
float
VCMFecMethod::AvgRecoveryFEC(const VCMProtectionParameters* parameters) const
{
    // Total (avg) bits available per frame: total rate over actual/sent frame
    // rate units are kbits/frame
    const WebRtc_UWord16 bitRatePerFrame = static_cast<WebRtc_UWord16>
                        (parameters->bitRate / (parameters->frameRate));

    // Total (avg) number of packets per frame (source and fec):
    const WebRtc_UWord8 avgTotPackets = 1 +
                        (WebRtc_UWord8) ((float) bitRatePerFrame * 1000.0
                        / (float) (8.0 * _maxPayloadSize) + 0.5);

    // parameters for tables
    const WebRtc_UWord8 codeSize = 24;
    const WebRtc_UWord8 plossMax = 129;
    const WebRtc_UWord16 maxErTableSize = 38700;

    // Get index for table
    const float protectionFactor = (float) _protectionFactorD / (float) 255;
    WebRtc_UWord8 fecPacketsPerFrame = (WebRtc_UWord8) (0.5 + protectionFactor
                                                        * avgTotPackets);
    WebRtc_UWord8 sourcePacketsPerFrame = avgTotPackets - fecPacketsPerFrame;

    if (fecPacketsPerFrame == 0)
    {
        return 0.0; // no protection, so avg. recov from FEC == 0
    }

    // table defined up to codeSizexcodeSize code
    if (sourcePacketsPerFrame > codeSize)
    {
        sourcePacketsPerFrame = codeSize;
    }

    // check: protection factor is maxed at 50%, so this should never happen
    if (sourcePacketsPerFrame < 1)
    {
        assert("average number of source packets below 1\n");
    }

    // index for ER tables: up to codeSizexcodeSize mask
    WebRtc_UWord16 codeIndexTable[codeSize * codeSize];
    WebRtc_UWord16 k = -1;
    for (WebRtc_UWord8 i = 1; i <= codeSize; i++)
    {
        for (WebRtc_UWord8 j = 1; j <= i; j++)
        {
            k += 1;
            codeIndexTable[(j - 1) * codeSize + i - 1] = k;
        }
    }

    const WebRtc_UWord8 lossRate = (WebRtc_UWord8) (255.0 *
                                    parameters->lossPr + 0.5f);

    const WebRtc_UWord16 codeIndex = (fecPacketsPerFrame - 1) * codeSize
                                      + (sourcePacketsPerFrame - 1);
    const WebRtc_UWord16 indexTable = codeIndexTable[codeIndex] * plossMax
                                      + lossRate;

    const WebRtc_UWord16 codeIndex2 = (fecPacketsPerFrame) * codeSize
                                      + (sourcePacketsPerFrame);
    WebRtc_UWord16 indexTable2 = codeIndexTable[codeIndex2] * plossMax
                                 + lossRate;

    // checks on table index
    if (indexTable >= maxErTableSize)
    {
        assert("ER table index too large\n");
    }

    if (indexTable2 >= maxErTableSize)
    {
        indexTable2 = indexTable;
    }

    // Get the average effective packet loss recovery from FEC
    // this is from tables, computed using random loss model
    WebRtc_UWord8 avgFecRecov1 = 0;
    WebRtc_UWord8 avgFecRecov2 = 0;
    float avgFecRecov = 0;

    if (fecPacketsPerFrame > 0)
    {
        avgFecRecov1 = VCMAvgFECRecoveryXOR[indexTable];
        avgFecRecov2 = VCMAvgFECRecoveryXOR[indexTable2];
    }

    // interpolate over two FEC codes
    const float weightRpl = (float) (0.5 + protectionFactor * avgTotPackets)
        - (float) fecPacketsPerFrame;
    avgFecRecov = (float) weightRpl * (float) avgFecRecov2 + (float)
                  (1.0 - weightRpl) * (float) avgFecRecov1;

    return avgFecRecov;
}

bool
VCMFecMethod::ProtectionFactor(const VCMProtectionParameters* parameters)
{
    // FEC PROTECTION SETTINGS: varies with packet loss and bitrate

    WebRtc_UWord8 packetLoss = (WebRtc_UWord8) (255 * parameters->lossPr);

    // No protection if (filtered) packetLoss is 0
    if (packetLoss == 0)
    {
        _protectionFactorK = 0;
        _protectionFactorD = 0;
         return true;
    }

    // Size of tables
    const WebRtc_UWord16 maxFecTableSize = 6450;
    // Parameters for range of rate and packet loss for tables
    const WebRtc_UWord8 ratePar1 = 5;
    const WebRtc_UWord8 ratePar2 = 49;
    const WebRtc_UWord8 plossMax = 129;

    const float bitRate = parameters->bitRate;

    // Total (avg) bits available per frame: total rate over actual/frame_rate.
    // Units are kbits/frame
    const WebRtc_UWord16 bitRatePerFrame = static_cast<WebRtc_UWord16>
                                                     (bitRate /
                                                     (parameters->frameRate));

    // TODO (marpan): Incorporate frame size (bpp) into FEC setting

    // Total (avg) number of packets per frame (source and fec):
    const WebRtc_UWord8 avgTotPackets = 1 + (WebRtc_UWord8)
                                        ((float) bitRatePerFrame * 1000.0
                                       / (float) (8.0 * _maxPayloadSize) + 0.5);


    // First partition protection: ~ 20%
    WebRtc_UWord8 firstPartitionProt = (WebRtc_UWord8) (255 * 0.20);

    // Threshold on packetLoss and bitRrate/frameRate (=average #packets),
    // above which we allocate protection to cover at least roughly
    // first partition size.
    WebRtc_UWord8 lossThr = 0;
    WebRtc_UWord8 packetNumThr = 1;

    // Modulation of protection with available bits/frame (or avgTotpackets)
    float weight1 = 0.5;
    float weight2 = 0.5;
    if (avgTotPackets > 4)
    {
        weight1 = 0.75;
        weight2 = 0.25;
    }
    if (avgTotPackets > 6)
    {
        weight1 = 1.5;
        weight2 = 0.;
    }

    // FEC rate parameters: for P and I frame
    WebRtc_UWord8 codeRateDelta = 0;
    WebRtc_UWord8 codeRateKey = 0;

    // Get index for table: the FEC protection depends on the (average)
    // available bits/frame. The range on the rate index corresponds to rates
    // (bps) from 200k to 8000k, for 30fps
    WebRtc_UWord8 rateIndexTable =
        (WebRtc_UWord8) VCM_MAX(VCM_MIN((bitRatePerFrame - ratePar1) /
                                        ratePar1, ratePar2), 0);

    // Restrict packet loss range to 50:
    // current tables defined only up to 50%
    if (packetLoss >= plossMax)
    {
        packetLoss = plossMax - 1;
    }
    WebRtc_UWord16 indexTable = rateIndexTable * plossMax + packetLoss;

    // Check on table index
    assert(indexTable < maxFecTableSize);

    // Protection factor for P frame
    codeRateDelta = VCMCodeRateXORTable[indexTable];

    if (packetLoss > lossThr && avgTotPackets > packetNumThr)
    {
        // Average with minimum protection level given by (average) total
        // number of packets
        codeRateDelta = static_cast<WebRtc_UWord8>((weight1 *
            (float) codeRateDelta + weight2 * 255.0 / (float) avgTotPackets));

        // Set a minimum based on first partition size.
        if (codeRateDelta < firstPartitionProt)
        {
            codeRateDelta = firstPartitionProt;
        }
    }

    // Check limit on amount of protection for P frame; 50% is max.
    if (codeRateDelta >= plossMax)
    {
        codeRateDelta = plossMax - 1;
    }

    float adjustFec = _qmRobustness->AdjustFecFactor(codeRateDelta, bitRate,
                                                     parameters->frameRate,
                                                     parameters->rtt, packetLoss);

    codeRateDelta = static_cast<WebRtc_UWord8>(codeRateDelta * adjustFec);

    // For Key frame:
    // Effectively at a higher rate, so we scale/boost the rate
    // The boost factor may depend on several factors: ratio of packet
    // number of I to P frames, how much protection placed on P frames, etc.
    const WebRtc_UWord8 packetFrameDelta = (WebRtc_UWord8)
                                           (0.5 + parameters->packetsPerFrame);
    const WebRtc_UWord8 packetFrameKey = (WebRtc_UWord8)
                                         (0.5 + parameters->packetsPerFrameKey);
    const WebRtc_UWord8 boostKey = BoostCodeRateKey(packetFrameDelta,
                                                    packetFrameKey);

    rateIndexTable = (WebRtc_UWord8) VCM_MAX(VCM_MIN(
                      1 + (boostKey * bitRatePerFrame - ratePar1) /
                      ratePar1,ratePar2),0);
    WebRtc_UWord16 indexTableKey = rateIndexTable * plossMax + packetLoss;

    indexTableKey = VCM_MIN(indexTableKey, maxFecTableSize);

    // Check on table index
    assert(indexTableKey < maxFecTableSize);

    // Protection factor for I frame
    codeRateKey = VCMCodeRateXORTable[indexTableKey];

    // Boosting for Key frame.
    WebRtc_UWord32 boostKeyProt = _scaleProtKey * codeRateDelta;
    if ( boostKeyProt >= plossMax)
    {
        boostKeyProt = plossMax - 1;
    }

    // Make sure I frame protection is at least larger than P frame protection,
    // and at least as high as filtered packet loss.
    codeRateKey = static_cast<WebRtc_UWord8> (VCM_MAX(packetLoss,
            VCM_MAX(boostKeyProt, codeRateKey)));

    // Check limit on amount of protection for I frame: 50% is max.
    if (codeRateKey >= plossMax)
    {
        codeRateKey = plossMax - 1;
    }

    _protectionFactorK = codeRateKey;
    _protectionFactorD = codeRateDelta;


     // TODO (marpan): Set the UEP protection on/off for Key and Delta frames
    _useUepProtectionK = _qmRobustness->SetUepProtection(codeRateKey, bitRate,
                                                         packetLoss, 0);

    _useUepProtectionD = _qmRobustness->SetUepProtection(codeRateKey, bitRate,
                                                         packetLoss, 1);

    // DONE WITH FEC PROTECTION SETTINGS
    return true;
}

bool
VCMFecMethod::EffectivePacketLoss(const VCMProtectionParameters* parameters)
{
    // ER SETTINGS:
    // Effective packet loss to encoder is based on RPL (residual packet loss)
    // this is a soft setting based on degree of FEC protection
    // RPL = received/input packet loss - average_FEC_recovery
    // note: received/input packet loss may be filtered based on FilteredLoss

    // The input packet loss:
    WebRtc_UWord8 effPacketLoss = (WebRtc_UWord8) (255 * parameters->lossPr);

    float scaleErRS = 0.5;
    float scaleErXOR = 0.5;
    float minErLevel = (float) 0.025;
    // float scaleErRS = 1.0;
    // float scaleErXOR = 1.0;
    // float minErLevel = (float) 0.0;

    float avgFecRecov = 0.;
    // Effective packet loss for ER:
    float scaleEr = scaleErXOR;
    avgFecRecov = AvgRecoveryFEC(parameters);

    // Residual Packet Loss:
    _residualPacketLoss = (float) (effPacketLoss - avgFecRecov) / (float) 255.0;

    //Effective Packet Loss for encoder:
    _effectivePacketLoss = 0;
    if (effPacketLoss > 0) {
      _effectivePacketLoss = VCM_MAX((effPacketLoss -
              (WebRtc_UWord8)(scaleEr * avgFecRecov)),
          static_cast<WebRtc_UWord8>(minErLevel * 255));
    }

    // DONE WITH ER SETTING
    return true;
}

bool
VCMFecMethod::UpdateParameters(const VCMProtectionParameters* parameters)
{
    // Compute the protection factor
    ProtectionFactor(parameters);

    // Compute the effective packet loss
    EffectivePacketLoss(parameters);

    // Compute the bit cost
    // Ignore key frames for now.
    float fecRate = static_cast<float> (_protectionFactorD) / 255.0f;
    if (fecRate >= 0.0f)
    {
        // use this formula if the fecRate (protection factor) is defined
        // relative to number of source packets
        // this is the case for the previous tables:
        // _efficiency = parameters->bitRate * ( 1.0 - 1.0 / (1.0 + fecRate));

        // in the new tables, the fecRate is defined relative to total number of
        // packets (total rate), so overhead cost is:
        _efficiency = parameters->bitRate * fecRate;
    }
    else
    {
        _efficiency = 0.0f;
    }
    _score = _efficiency;

    // Protection/fec rates obtained above is defined relative to total number
    // of packets (total rate: source+fec) FEC in RTP module assumes protection
    // factor is defined relative to source number of packets so we should
    // convert the factor to reduce mismatch between mediaOpt suggested rate and
    // the actual rate
    _protectionFactorK = ConvertFECRate(_protectionFactorK);
    _protectionFactorD = ConvertFECRate(_protectionFactorD);

    return true;
}

bool
VCMIntraReqMethod::UpdateParameters(const VCMProtectionParameters* parameters)
{
    float packetRate = parameters->packetsPerFrame * parameters->frameRate;
    // Assume that all lost packets cohere to different frames
    float lossRate = parameters->lossPr * packetRate;
    if (parameters->keyFrameSize <= 1e-3)
    {
        _score = FLT_MAX;
        return false;
    }
    _efficiency = lossRate * parameters->keyFrameSize;
    _score = _efficiency;
    if (parameters->lossPr >= 1.0f / parameters->keyFrameSize ||
        parameters->rtt > _IREQ_MAX_RTT)
    {
        return false;
    }
    return true;
}

bool
VCMPeriodicIntraMethod::UpdateParameters(const
                                        VCMProtectionParameters* /*parameters*/)
{
    // Periodic I-frames. The last thing we want to use.
    _efficiency = 0.0f;
    _score = FLT_MAX;
    return true;
}

bool
VCMMbIntraRefreshMethod::UpdateParameters(const
                                          VCMProtectionParameters* parameters)
{
    // Assume optimal for now.
    _efficiency = parameters->bitRate * parameters->lossPr /
                  (1.0f + parameters->lossPr);
    _score = _efficiency;
    if (parameters->bitRate < _MBREF_MIN_BITRATE)
    {
        return false;
    }
    return true;
}

WebRtc_UWord16
VCMNackMethod::MaxRttNack() const
{
    return _NACK_MAX_RTT;
}

VCMLossProtectionLogic::~VCMLossProtectionLogic()
{
    ClearLossProtections();
}

void
VCMLossProtectionLogic::ClearLossProtections()
{
    ListItem *item;
    while ((item = _availableMethods.First()) != 0) {
      VCMProtectionMethod *method = static_cast<VCMProtectionMethod*>
                                    (item->GetItem());
      if (method != NULL)
      {
          delete method;
      }
      _availableMethods.PopFront();
    }
    _selectedMethod = NULL;
}

bool
VCMLossProtectionLogic::AddMethod(VCMProtectionMethod *newMethod)
{
    VCMProtectionMethod *method;
    ListItem *item;
    if (newMethod == NULL)
    {
        return false;
    }
    for (item = _availableMethods.First(); item != NULL;
        item = _availableMethods.Next(item))
    {
        method = static_cast<VCMProtectionMethod *> (item->GetItem());
        if (method != NULL && method->Type() == newMethod->Type())
        {
            return false;
        }
    }
    _availableMethods.PushBack(newMethod);
    return true;
}
bool
VCMLossProtectionLogic::RemoveMethod(VCMProtectionMethodEnum methodType)
{
    VCMProtectionMethod *method;
    ListItem *item;
    bool foundAndRemoved = false;
    for (item = _availableMethods.First(); item != NULL;
         item = _availableMethods.Next(item))
    {
        method = static_cast<VCMProtectionMethod *> (item->GetItem());
        if (method != NULL && method->Type() == methodType)
        {
            if (_selectedMethod != NULL &&
                _selectedMethod->Type() == method->Type())
            {
                _selectedMethod = NULL;
            }
            _availableMethods.Erase(item);
            item = NULL;
            delete method;
            foundAndRemoved = true;
        }
    }
    return foundAndRemoved;
}

VCMProtectionMethod*
VCMLossProtectionLogic::FindMethod(VCMProtectionMethodEnum methodType) const
{
    VCMProtectionMethod *method;
    ListItem *item;
    for (item = _availableMethods.First(); item != NULL;
         item = _availableMethods.Next(item))
    {
        method = static_cast<VCMProtectionMethod *> (item->GetItem());
        if (method != NULL && method->Type() == methodType)
        {
            return method;
        }
    }
    return NULL;
}

float
VCMLossProtectionLogic::HighestOverhead() const
{
    VCMProtectionMethod *method;
    ListItem *item;
    float highestOverhead = 0.0f;
    for (item = _availableMethods.First(); item != NULL;
         item = _availableMethods.Next(item))
    {
        method = static_cast<VCMProtectionMethod *> (item->GetItem());
        if (method != NULL && method->RequiredBitRate() > highestOverhead)
        {
            highestOverhead = method->RequiredBitRate();
        }
    }
    return highestOverhead;
}

void
VCMLossProtectionLogic::UpdateRtt(WebRtc_UWord32 rtt)
{
    _rtt = rtt;
}

void
VCMLossProtectionLogic::UpdateResidualPacketLoss(float residualPacketLoss)
{
    _residualPacketLoss = residualPacketLoss;
}

void
VCMLossProtectionLogic::UpdateFecType(VCMFecTypes fecType)
{
    _fecType = fecType;
}

void
VCMLossProtectionLogic::UpdateLossPr(WebRtc_UWord8 lossPr255)
{
    const WebRtc_Word64 now = VCMTickTime::MillisecondTimestamp();
    UpdateMaxLossHistory(lossPr255, now);
    _lossPr255.Apply(static_cast<float> (now - _lastPrUpdateT),
                     static_cast<float> (lossPr255));
    _lastPrUpdateT = now;
    _lossPr = _lossPr255.Value() / 255.0f;
}

void
VCMLossProtectionLogic::UpdateMaxLossHistory(WebRtc_UWord8 lossPr255,
                                             WebRtc_Word64 now)
{
    if (_lossPrHistory[0].timeMs >= 0 &&
        now - _lossPrHistory[0].timeMs < kLossPrShortFilterWinMs)
    {
        if (lossPr255 > _shortMaxLossPr255)
        {
            _shortMaxLossPr255 = lossPr255;
        }
    }
    else
    {
        // Only add a new value to the history once a second
        if (_lossPrHistory[0].timeMs == -1)
        {
            // First, no shift
            _shortMaxLossPr255 = lossPr255;
        }
        else
        {
            // Shift
            for (WebRtc_Word32 i = (kLossPrHistorySize - 2); i >= 0; i--)
            {
                _lossPrHistory[i + 1].lossPr255 = _lossPrHistory[i].lossPr255;
                _lossPrHistory[i + 1].timeMs = _lossPrHistory[i].timeMs;
            }
        }
        if (_shortMaxLossPr255 == 0)
        {
            _shortMaxLossPr255 = lossPr255;
        }

        _lossPrHistory[0].lossPr255 = _shortMaxLossPr255;
        _lossPrHistory[0].timeMs = now;
        _shortMaxLossPr255 = 0;
    }
}

WebRtc_UWord8
VCMLossProtectionLogic::MaxFilteredLossPr(WebRtc_Word64 nowMs) const
{
    WebRtc_UWord8 maxFound = _shortMaxLossPr255;
    if (_lossPrHistory[0].timeMs == -1)
    {
        return maxFound;
    }
    for (WebRtc_Word32 i = 0; i < kLossPrHistorySize; i++)
    {
        if (_lossPrHistory[i].timeMs == -1)
        {
            break;
        }
        if (nowMs - _lossPrHistory[i].timeMs >
            kLossPrHistorySize * kLossPrShortFilterWinMs)
        {
            // This sample (and all samples after this) is too old
            break;
        }
        if (_lossPrHistory[i].lossPr255 > maxFound)
        {
            // This sample is the largest one this far into the history
            maxFound = _lossPrHistory[i].lossPr255;
        }
    }
    return maxFound;
}

WebRtc_UWord8
VCMLossProtectionLogic::FilteredLoss() const
{
    //take the average received loss
    //return static_cast<WebRtc_UWord8>(_lossPr255.Value() + 0.5f);

    //TODO: Update for hybrid
    //take the windowed max of the received loss
    if (_selectedMethod != NULL && _selectedMethod->Type() == kFEC)
    {
        return MaxFilteredLossPr(VCMTickTime::MillisecondTimestamp());
    }
    else
    {
        return static_cast<WebRtc_UWord8> (_lossPr255.Value() + 0.5);
    }
}

void
VCMLossProtectionLogic::UpdateFilteredLossPr(WebRtc_UWord8 packetLossEnc)
{
    _lossPr = (float) packetLossEnc / (float) 255.0;
}

void
VCMLossProtectionLogic::UpdateBitRate(float bitRate)
{
    _bitRate = bitRate;
}

void
VCMLossProtectionLogic::UpdatePacketsPerFrame(float nPackets)
{
    const WebRtc_Word64 now = VCMTickTime::MillisecondTimestamp();
    _packetsPerFrame.Apply(static_cast<float>(now - _lastPacketPerFrameUpdateT),
                           nPackets);
    _lastPacketPerFrameUpdateT = now;
}

void
VCMLossProtectionLogic::UpdatePacketsPerFrameKey(float nPackets)
{
    const WebRtc_Word64 now = VCMTickTime::MillisecondTimestamp();
    _packetsPerFrameKey.Apply(static_cast<float>(now -
                              _lastPacketPerFrameUpdateTKey), nPackets);
    _lastPacketPerFrameUpdateTKey = now;
}

void
VCMLossProtectionLogic::UpdateKeyFrameSize(float keyFrameSize)
{
    _keyFrameSize = keyFrameSize;
}

void
VCMLossProtectionLogic::UpdateFrameSize(WebRtc_UWord16 width,
                                        WebRtc_UWord16 height)
{
    _codecWidth = width;
    _codecHeight = height;
}

bool
VCMLossProtectionLogic::UpdateMethod(VCMProtectionMethod *newMethod /*=NULL */)
{
    _currentParameters.rtt = _rtt;
    _currentParameters.lossPr = _lossPr;
    _currentParameters.bitRate = _bitRate;
    _currentParameters.frameRate = _frameRate; // rename actual frame rate?
    _currentParameters.keyFrameSize = _keyFrameSize;
    _currentParameters.fecRateDelta = _fecRateDelta;
    _currentParameters.fecRateKey = _fecRateKey;
    _currentParameters.packetsPerFrame = _packetsPerFrame.Value();
    _currentParameters.packetsPerFrameKey = _packetsPerFrameKey.Value();
    _currentParameters.residualPacketLoss = _residualPacketLoss;
    _currentParameters.fecType = _fecType;
    _currentParameters.codecWidth = _codecWidth;
    _currentParameters.codecHeight = _codecHeight;

    if (newMethod == NULL)
    {
        //_selectedMethod = _bestNotOkMethod = NULL;
        VCMProtectionMethod *method;
        ListItem *item;
        for (item = _availableMethods.First(); item != NULL;
             item = _availableMethods.Next(item))
        {
            method = static_cast<VCMProtectionMethod *> (item->GetItem());
            if (method != NULL)
            {
                if (method->Type() == kFEC)
                {
                      _selectedMethod = method;
                }
                if (method->Type() == kNACK)
                {
                    _selectedMethod = method;
                }
                if (method->Type() == kNackFec)
                {
                    _selectedMethod = method;
                }
                method->UpdateParameters(&_currentParameters);
            }
        }
        if (_selectedMethod != NULL && _selectedMethod->Type() != kFEC)
        {
            _selectedMethod = method;
        }
     }
     else
     {
         _selectedMethod = newMethod;
         _selectedMethod->UpdateParameters(&_currentParameters);
     }
    return true;
}

VCMProtectionMethod*
VCMLossProtectionLogic::SelectedMethod() const
{
    return _selectedMethod;
}

void VCMLossProtectionLogic::Reset()
{
    const WebRtc_Word64 now = VCMTickTime::MillisecondTimestamp();
    _lastPrUpdateT = now;
    _lastPacketPerFrameUpdateT = now;
    _lastPacketPerFrameUpdateTKey = now;
    _lossPr255.Reset(0.9999f);
    _packetsPerFrame.Reset(0.9999f);
    _fecRateDelta = _fecRateKey = 0;
    for (WebRtc_Word32 i = 0; i < kLossPrHistorySize; i++)
    {
        _lossPrHistory[i].lossPr255 = 0;
        _lossPrHistory[i].timeMs = -1;
    }
    _shortMaxLossPr255 = 0;
    ClearLossProtections();
}

}
