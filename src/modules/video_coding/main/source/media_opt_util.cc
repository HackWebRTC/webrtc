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

VCMProtectionMethod::VCMProtectionMethod():
_effectivePacketLoss(0),
_protectionFactorK(0),
_protectionFactorD(0),
_residualPacketLossFec(0.0f),
_scaleProtKey(2.0f),
_maxPayloadSize(1460),
_qmRobustness(new VCMQmRobustness()),
_useUepProtectionK(false),
_useUepProtectionD(true),
_corrFecCost(1.0),
_type(kNone),
_efficiency(0)
{
    //
}

VCMProtectionMethod::~VCMProtectionMethod()
{
    delete _qmRobustness;
}
void
VCMProtectionMethod::UpdateContentMetrics(const
                                          VideoContentMetrics* contentMetrics)
{
    _qmRobustness->UpdateContent(contentMetrics);
}

VCMNackFecMethod::VCMNackFecMethod():
VCMFecMethod()
{
    _type = kNackFec;
}

VCMNackFecMethod::~VCMNackFecMethod()
{
    //
}
bool
VCMNackFecMethod::ProtectionFactor(const
                                   VCMProtectionParameters* parameters)
{
    // Hybrid Nack FEC has three operational modes:
    // 1. Low RTT (below kLowRttNackMs) - Nack only: Set FEC rate
    //    (_protectionFactorD) to zero.
    // 2. High RTT (above kHighRttNackMs) - FEC Only: Keep FEC factors.
    // 3. Medium RTT values - Hybrid mode: We will only nack the
    //    residual following the decoding of the FEC (refer to JB logic). FEC
    //    delta protection factor will be adjusted based on the RTT.

    // Otherwise: we count on FEC; if the RTT is below a threshold, then we
    // nack the residual, based on a decision made in the JB.

    // Compute the protection factors
    VCMFecMethod::ProtectionFactor(parameters);

    // When in Hybrid mode (RTT range), adjust FEC rates based on the
    // RTT (NACK effectiveness) - adjustment factor is in the range [0,1].
    if (parameters->rtt < kHighRttNackMs)
    {
        WebRtc_UWord16 rttIndex = (WebRtc_UWord16) parameters->rtt;
        float adjustRtt = (float)VCMNackFecTable[rttIndex] / 100.0f;

        // Adjust FEC with NACK on (for delta frame only)
        // table depends on RTT relative to rttMax (NACK Threshold)
        _protectionFactorD = static_cast<WebRtc_UWord8>
                            (adjustRtt *
                             static_cast<float>(_protectionFactorD));
        // update FEC rates after applyingadjustment softness parameter
        VCMFecMethod::UpdateProtectionFactorD(_protectionFactorD);
    }

    return true;
}

bool
VCMNackFecMethod::EffectivePacketLoss(const
                                      VCMProtectionParameters* parameters)
{
    // Set the effective packet loss for encoder (based on FEC code).
    // Compute the effective packet loss and residual packet loss due to FEC.
    VCMFecMethod::EffectivePacketLoss(parameters);
    return true;
}

bool
VCMNackFecMethod::UpdateParameters(const VCMProtectionParameters* parameters)
{
    ProtectionFactor(parameters);
    EffectivePacketLoss(parameters);

    // Efficiency computation is based on FEC and NACK

    // Add FEC cost: ignore I frames for now
    float fecRate = static_cast<float> (_protectionFactorD) / 255.0f;
    _efficiency = parameters->bitRate * fecRate * _corrFecCost;

    // Add NACK cost, when applicable
    if (parameters->rtt < kHighRttNackMs)
    {
        // nackCost  = (bitRate - nackCost) * (lossPr)
        _efficiency += parameters->bitRate * _residualPacketLossFec /
                       (1.0f + _residualPacketLossFec);
    }

    // Protection/fec rates obtained above are defined relative to total number
    // of packets (total rate: source + fec) FEC in RTP module assumes
    // protection factor is defined relative to source number of packets so we
    // should convert the factor to reduce mismatch between mediaOpt's rate and
    // the actual one
    _protectionFactorK = VCMFecMethod::ConvertFECRate(_protectionFactorK);
    _protectionFactorD = VCMFecMethod::ConvertFECRate(_protectionFactorD);

    return true;
}

VCMNackMethod::VCMNackMethod():
VCMProtectionMethod()
{
    _type = kNack;
}

VCMNackMethod::~VCMNackMethod()
{
    //
}

bool
VCMNackMethod::EffectivePacketLoss(const VCMProtectionParameters* parameter)
{
    // Effective Packet Loss, NA in current version.
    _effectivePacketLoss = 0;
    return true;
}

bool
VCMNackMethod::UpdateParameters(const VCMProtectionParameters* parameters)
{
    // Compute the effective packet loss
    EffectivePacketLoss(parameters);

    // nackCost  = (bitRate - nackCost) * (lossPr)
    _efficiency = parameters->bitRate * parameters->lossPr /
                  (1.0f + parameters->lossPr);
    return true;
}

VCMFecMethod::VCMFecMethod():
VCMProtectionMethod()
{
    _type = kFec;
}
VCMFecMethod::~VCMFecMethod()
{
    //
}

WebRtc_UWord8
VCMFecMethod::BoostCodeRateKey(WebRtc_UWord8 packetFrameDelta,
                               WebRtc_UWord8 packetFrameKey) const
{
    WebRtc_UWord8 boostRateKey = 2;
    // Default: ratio scales the FEC protection up for I frames
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

// Update FEC with protectionFactorD
void
VCMFecMethod::UpdateProtectionFactorD(WebRtc_UWord8 protectionFactorD)
{
    _protectionFactorD = protectionFactorD;
}

// AvgRecoveryFEC: computes the residual packet loss function.
// This is the average recovery from the FEC, assuming random packet loss model.
// Computed off-line for a range of FEC code parameters and loss rates.
float
VCMFecMethod::AvgRecoveryFEC(const VCMProtectionParameters* parameters) const
{
    // Total (avg) bits available per frame: total rate over actual/sent frame
    // rate units are kbits/frame
    const WebRtc_UWord16 bitRatePerFrame = static_cast<WebRtc_UWord16>
                        (parameters->bitRate / (parameters->frameRate));

    // Total (average) number of packets per frame (source and fec):
    const WebRtc_UWord8 avgTotPackets = 1 + static_cast<WebRtc_UWord8>
                        (static_cast<float> (bitRatePerFrame * 1000.0) /
                         static_cast<float> (8.0 * _maxPayloadSize) + 0.5);

    // Parameters for tables
    const WebRtc_UWord8 codeSize = 24;
    const WebRtc_UWord8 plossMax = 129;
    const WebRtc_UWord16 maxErTableSize = 38700;

    // Get index for table
    const float protectionFactor = static_cast<float>(_protectionFactorD) /
                                                      255.0;

    WebRtc_UWord8 fecPacketsPerFrame = static_cast<WebRtc_UWord8>
                                      (0.5 + protectionFactor * avgTotPackets);

    WebRtc_UWord8 sourcePacketsPerFrame = avgTotPackets - fecPacketsPerFrame;

    if ( (fecPacketsPerFrame == 0) || (sourcePacketsPerFrame == 0) )
    {
        // No protection, or rate too low: so average recovery from FEC == 0.
        return 0.0;
    }

    // Table defined up to codeSizexcodeSize code
    if (sourcePacketsPerFrame > codeSize)
    {
        sourcePacketsPerFrame = codeSize;
    }

    // Table defined up to codeSizexcodeSize code
    if (fecPacketsPerFrame > codeSize)
    {
        fecPacketsPerFrame = codeSize;
    }

    // Check: protection factor is maxed at 50%, so this should never happen
    assert(sourcePacketsPerFrame >= 1);

    // Index for ER tables: up to codeSizexcodeSize mask
    WebRtc_UWord16 codeIndexTable[codeSize * codeSize];
    WebRtc_UWord16 k = 0;
    for (WebRtc_UWord8 i = 1; i <= codeSize; i++)
    {
        for (WebRtc_UWord8 j = 1; j <= i; j++)
        {
            codeIndexTable[(j - 1) * codeSize + i - 1] = k;
            k += 1;
        }
    }

    WebRtc_UWord8 lossRate = static_cast<WebRtc_UWord8> (255.0 *
                             parameters->lossPr + 0.5f);
    // Constrain lossRate to 129 (50% protection)
    // TODO (marpan): Verify table values

    const WebRtc_UWord16 codeIndex = (fecPacketsPerFrame - 1) * codeSize +
                                     (sourcePacketsPerFrame - 1);

    const WebRtc_UWord16 indexTable = codeIndexTable[codeIndex] * plossMax +
                                      lossRate;

    const WebRtc_UWord16 codeIndex2 = (fecPacketsPerFrame) * codeSize +
                                      (sourcePacketsPerFrame);

    WebRtc_UWord16 indexTable2 = codeIndexTable[codeIndex2] * plossMax +
                                 lossRate;

    // Check on table index
    assert (indexTable < maxErTableSize);
    if (indexTable2 >= maxErTableSize)
    {
        indexTable2 = indexTable;
    }

    // Get the average effective packet loss recovery from FEC
    // this is from tables, computed using random loss model
    WebRtc_UWord8 avgFecRecov1 = 0;
    WebRtc_UWord8 avgFecRecov2 = 0;
    float avgFecRecov = 0.0f;

    if (fecPacketsPerFrame > 0)
    {
        avgFecRecov1 = VCMAvgFECRecoveryXOR[indexTable];
        avgFecRecov2 = VCMAvgFECRecoveryXOR[indexTable2];
    }

    // Interpolate over two FEC codes
    const float weightRpl = static_cast<float>
                           (0.5 + protectionFactor * avgTotPackets) -
                           (float) fecPacketsPerFrame;

    avgFecRecov = weightRpl * static_cast<float> (avgFecRecov2) +
                  (1.0 - weightRpl) * static_cast<float> (avgFecRecov1);

    return avgFecRecov;
}

bool
VCMFecMethod::ProtectionFactor(const VCMProtectionParameters* parameters)
{
    // FEC PROTECTION SETTINGS: varies with packet loss and bitrate

    // No protection if (filtered) packetLoss is 0
    WebRtc_UWord8 packetLoss = (WebRtc_UWord8) (255 * parameters->lossPr);
    if (packetLoss == 0)
    {
        _protectionFactorK = 0;
        _protectionFactorD = 0;
         return true;
    }

    // Parameters for FEC setting:
    // first partition size, thresholds, table pars, spatial resoln fac.

    // First partition protection: ~ 20%
    WebRtc_UWord8 firstPartitionProt = (WebRtc_UWord8) (255 * 0.20);

    // Threshold on packetLoss and bitRrate/frameRate (=average #packets),
    // above which we allocate protection to cover at least first partition.
    WebRtc_UWord8 lossThr = 0;
    WebRtc_UWord8 packetNumThr = 1;

    // Size of table
    const WebRtc_UWord16 maxFecTableSize = 6450;
    // Parameters for range of rate and packet loss for tables
    const WebRtc_UWord8 ratePar1 = 5;
    const WebRtc_UWord8 ratePar2 = 49;
    const WebRtc_UWord8 plossMax = 129;

    // Spatial resolution size, relative to a reference size.
    float spatialSizeToRef = static_cast<float>
                           (parameters->codecWidth * parameters->codecHeight) /
                           (static_cast<float>(704 * 576));
    // resolnFac: This parameter will generally increase/decrease the FEC rate
    // (for fixed bitRate and packetLoss) based on system size.
    // Use a smaller exponent (< 1) to control/soften system size effect.
    const float resolnFac = 1.0 / powf(spatialSizeToRef, 0.3f);

    const float bitRate = parameters->bitRate;
    const float frameRate = parameters->frameRate;

    // Average bits per frame (units of kbits)
    const WebRtc_UWord16 bitRatePerFrame = static_cast<WebRtc_UWord16>
                                           (bitRate / frameRate);

    // Average number of packets per frame (source and fec):
    const WebRtc_UWord8 avgTotPackets = 1 + (WebRtc_UWord8)
                                        ((float) bitRatePerFrame * 1000.0
                                       / (float) (8.0 * _maxPayloadSize) + 0.5);

    // FEC rate parameters: for P and I frame
    WebRtc_UWord8 codeRateDelta = 0;
    WebRtc_UWord8 codeRateKey = 0;

    // Get index for table: the FEC protection depends on an effective rate.
    // The range on the rate index corresponds to rates (bps)
    // from ~200k to ~8000k, for 30fps
    const WebRtc_UWord16 effRateFecTable = static_cast<WebRtc_UWord16>
                                           (resolnFac * bitRatePerFrame);
    WebRtc_UWord8 rateIndexTable =
        (WebRtc_UWord8) VCM_MAX(VCM_MIN((effRateFecTable - ratePar1) /
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

    float adjustFec = _qmRobustness->AdjustFecFactor(codeRateDelta,
                                                     bitRate,
                                                     frameRate,
                                                     parameters->rtt,
                                                     packetLoss);

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
                      1 + (boostKey * effRateFecTable - ratePar1) /
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

    // Generally there is a rate mis-match between the FEC cost estimated
    // in mediaOpt and the actual FEC cost sent out in RTP module.
    // This is more significant at low rates (small # of source packets), where
    // the granularity of the FEC decreases. In this case, non-zero protection
    // in mediaOpt may generate 0 FEC packets in RTP sender (since actual #FEC
    // is based on rounding off protectionFactor on actual source packet number).
    // The correction factor (_corrFecCost) attempts to corrects this, at least
    // for cases of low rates/low # of packets.
    const float estNumFecGen = 0.5f + static_cast<float> (_protectionFactorD *
                                                        avgTotPackets / 255.0f);
    // Note we reduce cost factor (which will reduce overhead for FEC and
    // hybrid method) and not the protectionFactor.
    _corrFecCost = 1.0f;
    if (estNumFecGen < 1.5f)
    {
        _corrFecCost = 0.5f;
    }
    if (estNumFecGen < 1.0f)
    {
        _corrFecCost = 0.0f;
    }

     // TODO (marpan): Set the UEP protection on/off for Key and Delta frames
    _useUepProtectionK = _qmRobustness->SetUepProtection(codeRateKey, bitRate,
                                                         packetLoss, 0);

    _useUepProtectionD = _qmRobustness->SetUepProtection(codeRateDelta, bitRate,
                                                         packetLoss, 1);

    // DONE WITH FEC PROTECTION SETTINGS
    return true;
}

bool
VCMFecMethod::EffectivePacketLoss(const VCMProtectionParameters* parameters)
{
    // Effective packet loss to encoder is based on RPL (residual packet loss)
    // this is a soft setting based on degree of FEC protection
    // RPL = received/input packet loss - average_FEC_recovery
    // note: received/input packet loss may be filtered based on FilteredLoss

    // The packet loss:
    WebRtc_UWord8 packetLoss = (WebRtc_UWord8) (255 * parameters->lossPr);

    float avgFecRecov = AvgRecoveryFEC(parameters);

    // Residual Packet Loss:
    _residualPacketLossFec = (float) (packetLoss - avgFecRecov) / 255.0f;

    // Effective Packet Loss, NA in current version.
    _effectivePacketLoss = 0;

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
        _efficiency = parameters->bitRate * fecRate * _corrFecCost;
    }
    else
    {
        _efficiency = 0.0f;
    }

    // Protection/fec rates obtained above is defined relative to total number
    // of packets (total rate: source+fec) FEC in RTP module assumes protection
    // factor is defined relative to source number of packets so we should
    // convert the factor to reduce mismatch between mediaOpt suggested rate and
    // the actual rate
    _protectionFactorK = ConvertFECRate(_protectionFactorK);
    _protectionFactorD = ConvertFECRate(_protectionFactorD);

    return true;
}
VCMLossProtectionLogic::VCMLossProtectionLogic():
_selectedMethod(NULL),
_currentParameters(),
_rtt(0),
_lossPr(0.0f),
_bitRate(0.0f),
_frameRate(0.0f),
_keyFrameSize(0.0f),
_fecRateKey(0),
_fecRateDelta(0),
_lastPrUpdateT(0),
_lossPr255(0.9999f),
_lossPrHistory(),
_shortMaxLossPr255(0),
_packetsPerFrame(0.9999f),
_packetsPerFrameKey(0.9999f),
_residualPacketLossFec(0),
_boostRateKey(2),
_codecWidth(0),
_codecHeight(0)
{
    Reset();
}

VCMLossProtectionLogic::~VCMLossProtectionLogic()
{
    Release();
}

bool
VCMLossProtectionLogic::SetMethod(enum VCMProtectionMethodEnum newMethodType)
{
    if (_selectedMethod != NULL)
    {
        if (_selectedMethod->Type() == newMethodType)
        {
            // Nothing to update
            return false;
        }
        // New method - delete existing one
        delete _selectedMethod;
    }
    VCMProtectionMethod *newMethod = NULL;
    switch (newMethodType)
    {
        case kNack:
        {
            newMethod = new VCMNackMethod();
            break;
        }
        case kFec:
        {
            newMethod  = new VCMFecMethod();
            break;
        }
        case kNackFec:
        {
            newMethod =  new VCMNackFecMethod();
            break;
        }
        default:
        {
          return false;
          break;
        }

    }
    _selectedMethod = newMethod;
    return true;
}
bool
VCMLossProtectionLogic::RemoveMethod(enum VCMProtectionMethodEnum method)
{
    if (_selectedMethod == NULL)
    {
        return false;
    }
    else if (_selectedMethod->Type() == method)
    {
        delete _selectedMethod;
        _selectedMethod = NULL;
    }
    return true;
}

float
VCMLossProtectionLogic::RequiredBitRate() const
{
    float RequiredBitRate = 0.0f;
    if (_selectedMethod != NULL)
    {
        RequiredBitRate = _selectedMethod->RequiredBitRate();
    }
    return RequiredBitRate;
}

void
VCMLossProtectionLogic::UpdateRtt(WebRtc_UWord32 rtt)
{
    _rtt = rtt;
}

void
VCMLossProtectionLogic::UpdateResidualPacketLoss(float residualPacketLoss)
{
    _residualPacketLossFec = residualPacketLoss;
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
    if (_selectedMethod != NULL && _selectedMethod->Type() == kFec)
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
VCMLossProtectionLogic::UpdateMethod()
{
    if (_selectedMethod == NULL)
    {
        return false;
    }
    _currentParameters.rtt = _rtt;
    _currentParameters.lossPr = _lossPr;
    _currentParameters.bitRate = _bitRate;
    _currentParameters.frameRate = _frameRate; // rename actual frame rate?
    _currentParameters.keyFrameSize = _keyFrameSize;
    _currentParameters.fecRateDelta = _fecRateDelta;
    _currentParameters.fecRateKey = _fecRateKey;
    _currentParameters.packetsPerFrame = _packetsPerFrame.Value();
    _currentParameters.packetsPerFrameKey = _packetsPerFrameKey.Value();
    _currentParameters.residualPacketLossFec = _residualPacketLossFec;
    _currentParameters.codecWidth = _codecWidth;
    _currentParameters.codecHeight = _codecHeight;
    return _selectedMethod->UpdateParameters(&_currentParameters);
}

VCMProtectionMethod*
VCMLossProtectionLogic::SelectedMethod() const
{
    return _selectedMethod;
}

VCMProtectionMethodEnum
VCMLossProtectionLogic::SelectedType() const
{
    return _selectedMethod->Type();
}

void
VCMLossProtectionLogic::Reset()
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
    Release();
}

void
VCMLossProtectionLogic::Release()
{
    delete _selectedMethod;
    _selectedMethod = NULL;
}

}
