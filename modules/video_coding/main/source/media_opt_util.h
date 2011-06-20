/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPT_UTIL_H_
#define WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPT_UTIL_H_

#include "typedefs.h"
#include "list_wrapper.h"
#include "trace.h"
#include "exp_filter.h"
#include "internal_defines.h"
#include "tick_time.h"

#include <cmath>
#include <cstdlib>


namespace webrtc
{
class ListWrapper;

enum { kLossPrHistorySize = 30 }; // 30 time periods
enum { kLossPrShortFilterWinMs = 1000 }; // 1000 ms, total filter length is 30 000 ms

enum VCMFecTypes
{
    kXORFec
};

// Thresholds for hybrid NACK/FEC
// common to media optimization and the jitter buffer.
enum HybridNackTH {
    kHighRttNackMs = 100,
    kLowRttNackMs = 20
};

struct VCMProtectionParameters
{
    VCMProtectionParameters() : rtt(0), lossPr(0), bitRate(0), packetsPerFrame(0),
        frameRate(0), keyFrameSize(0), fecRateDelta(0), fecRateKey(0),
        residualPacketLoss(0.0), fecType(kXORFec) {}

    WebRtc_UWord32      rtt;
    float               lossPr;
    float               bitRate;
    float               packetsPerFrame;
    float               packetsPerFrameKey;
    float               frameRate;
    float               keyFrameSize;
    WebRtc_UWord8       fecRateDelta;
    WebRtc_UWord8       fecRateKey;
    float               residualPacketLoss;
    VCMFecTypes           fecType;
};


/******************************/
/* VCMProtectionMethod class    */
/****************************/

enum VCMProtectionMethodEnum
{
    kNACK,
    kFEC,
    kNackFec,
    kIntraRequest, // I-frame request
    kPeriodicIntra, // I-frame refresh
    kMBIntraRefresh, // Macro block refresh
    kNone
};

class VCMLossProbabilitySample
{
public:
    VCMLossProbabilitySample() : lossPr255(0), timeMs(-1) {};

    WebRtc_UWord8     lossPr255;
    WebRtc_Word64     timeMs;
};



class VCMProtectionMethod
{
public:
    //friend VCMProtectionMethod;
    VCMProtectionMethod(VCMProtectionMethodEnum type) : _protectionFactorK(0),
        _protectionFactorD(0), _residualPacketLoss(0.0), _scaleProtKey(2.0),
        _maxPayloadSize(1460), _efficiency(0), _score(0), _type(type) {}
    virtual ~VCMProtectionMethod() {}

    // Updates the efficiency of the method using the parameters provided
    //
    // Input:
    //         - parameters         : Parameters used to calculate the efficiency
    //
    // Return value                 : True if this method is recommended in
    //                                the given conditions.
    virtual bool UpdateParameters(const VCMProtectionParameters* parameters) = 0;

    // Returns the protection type
    //
    // Return value                 : The protection type
    enum VCMProtectionMethodEnum Type() const { return _type; }

    // Evaluates if this protection method is considered
    // better than the provided method.
    //
    // Input:
    //          - pm                : The protection method to compare with
    bool BetterThan(VCMProtectionMethod *pm);

    // Returns the bit rate required by this protection method
    // during these conditions.
    //
    // Return value                 : Required bit rate
    virtual float RequiredBitRate() { return _efficiency; }

    // Returns the effective packet loss for ER, required by this protection method
    //
    // Return value                 : Required effective packet loss
    virtual WebRtc_UWord8 RequiredPacketLossER() { return _effectivePacketLoss; }

    // Extracts the FEC protection factor for Key frame, required by this protection method
    //
    // Return value                 : Required protectionFactor for Key frame
    virtual WebRtc_UWord8 RequiredProtectionFactorK() { return _protectionFactorK; }

    // Extracts the FEC protection factor for Delta frame, required by this protection method
    //
    // Return value                 : Required protectionFactor for delta frame
    virtual WebRtc_UWord8 RequiredProtectionFactorD() { return _protectionFactorD; }

    WebRtc_UWord8                        _effectivePacketLoss;
    WebRtc_UWord8                        _protectionFactorK;
    WebRtc_UWord8                        _protectionFactorD;
    float                                _residualPacketLoss;
    float                                _scaleProtKey;
    WebRtc_Word32                        _maxPayloadSize;

protected:
    float                                _efficiency;
    float                                _score;

private:
    const enum VCMProtectionMethodEnum   _type;

};

class VCMNackMethod : public VCMProtectionMethod
{
public:
    VCMNackMethod() : VCMProtectionMethod(kNACK), _NACK_MAX_RTT(200) {}
    virtual ~VCMNackMethod() {}
    virtual bool UpdateParameters(const VCMProtectionParameters* parameters);
    //get the effective packet loss for ER
    bool EffectivePacketLoss(WebRtc_UWord8 effPacketLoss, WebRtc_UWord16 rttTime);
    //get the threshold for NACK
    WebRtc_UWord16 MaxRttNack() const;
private:
    const WebRtc_UWord16 _NACK_MAX_RTT;
};

class VCMFecMethod : public VCMProtectionMethod
{
public:
    VCMFecMethod() : VCMProtectionMethod(kFEC) {}
    virtual ~VCMFecMethod() {}
    virtual bool UpdateParameters(const VCMProtectionParameters* parameters);
    //get the effective packet loss for ER
    bool EffectivePacketLoss(const VCMProtectionParameters* parameters);
    //get the FEC protection factors
    bool ProtectionFactor(const VCMProtectionParameters* parameters);
    //get the boost for key frame protection
    WebRtc_UWord8 BoostCodeRateKey(WebRtc_UWord8 packetFrameDelta,
                                   WebRtc_UWord8 packetFrameKey) const;
    //convert the rates: defined relative to total# packets or source# packets
    WebRtc_UWord8 ConvertFECRate(WebRtc_UWord8 codeRate) const;
    //get the average effective recovery from FEC: for random loss model
    float AvgRecoveryFEC(const VCMProtectionParameters* parameters) const;
};


class VCMNackFecMethod : public VCMProtectionMethod
{
public:
    VCMNackFecMethod() : VCMProtectionMethod(kNackFec) {}
    virtual bool UpdateParameters(const VCMProtectionParameters* parameters);
    //get the effective packet loss for ER
    bool EffectivePacketLoss(const VCMProtectionParameters* parameters);
    //get the FEC protection factors
    bool ProtectionFactor(const VCMProtectionParameters* parameters);

};


class VCMIntraReqMethod : public VCMProtectionMethod
{
public:
    VCMIntraReqMethod() : VCMProtectionMethod(kIntraRequest), _IREQ_MAX_RTT(150) {}
    virtual bool UpdateParameters(const VCMProtectionParameters* parameters);
private:
    const WebRtc_UWord32 _IREQ_MAX_RTT;
};

class VCMPeriodicIntraMethod : public VCMProtectionMethod
{
public:
    VCMPeriodicIntraMethod() : VCMProtectionMethod(kPeriodicIntra) {}
    virtual bool UpdateParameters(const VCMProtectionParameters* parameters);
};

class VCMMbIntraRefreshMethod : public VCMProtectionMethod
{
public:
    VCMMbIntraRefreshMethod() :
        VCMProtectionMethod(kMBIntraRefresh), _MBREF_MIN_BITRATE(150) {}
    virtual bool UpdateParameters(const VCMProtectionParameters* parameters);
    virtual float RequiredBitRate() { return 0.0; }
private:
    const WebRtc_UWord32 _MBREF_MIN_BITRATE;
};

class VCMLossProtectionLogic
{
public:
    VCMLossProtectionLogic() : _availableMethods(), _selectedMethod(NULL),
        _bestNotOkMethod(NULL), _rtt(0), _lossPr(0.0f), _bitRate(0.0f), _frameRate(0.0f),
        _keyFrameSize(0.0f), _fecRateKey(0), _fecRateDelta(0), _lastPrUpdateT(0),
        _lossPr255(0.9999f), _lossPrHistory(), _shortMaxLossPr255(0),
        _packetsPerFrame(0.9999f), _packetsPerFrameKey(0.9999f), _residualPacketLoss(0),
        _boostRateKey(2)
    { Reset(); }

    ~VCMLossProtectionLogic();

    void ClearLossProtections();
    bool AddMethod(VCMProtectionMethod *newMethod);
    bool RemoveMethod(VCMProtectionMethodEnum methodType);
    VCMProtectionMethod* FindMethod(VCMProtectionMethodEnum methodType) const;
    float HighestOverhead() const;

    // Update the round-trip time
    //
    // Input:
    //          - rtt           : Round-trip time in seconds.
    void UpdateRtt(WebRtc_UWord32 rtt);

    // Update residual packet loss
    //
    // Input:
    //          - residualPacketLoss  : residual packet loss: effective loss after FEC recovery
    void UpdateResidualPacketLoss(float residualPacketLoss);

    // Update fecType
    //
    // Input:
    //          - fecType           : kXORFec for generic XOR FEC
    void UpdateFecType(VCMFecTypes fecType);

    // Update the loss probability.
    //
    // Input:
    //          - lossPr255        : The packet loss probability in the interval [0, 255],
    //                               reported by RTCP.
    void UpdateLossPr(WebRtc_UWord8 lossPr255);

    // Update the filtered packet loss.
    //
    // Input:
    //          - packetLossEnc :  The reported packet loss filtered (max window or average)
    void UpdateFilteredLossPr(WebRtc_UWord8 packetLossEnc);

    // Update the current target bit rate.
    //
    // Input:
    //          - bitRate          : The current target bit rate in kbits/s
    void UpdateBitRate(float bitRate);

    // Update the number of packets per frame estimate, for delta frames
    //
    // Input:
    //          - nPackets         : Number of packets used to send the latest frame.
    void UpdatePacketsPerFrame(float nPackets);

   // Update the number of packets per frame estimate, for key frames
    //
    // Input:
    //          - nPackets         : Number of packets used to send the latest frame.
    void UpdatePacketsPerFrameKey(float nPackets);

    // Update the keyFrameSize estimate
    //
    // Input:
    //          - keyFrameSize     : The size of the latest sent key frame.
    void UpdateKeyFrameSize(float keyFrameSize);

    // Update the frame rate
    //
    // Input:
    //          - frameRate        : The current target frame rate.
    void UpdateFrameRate(float frameRate) { _frameRate = frameRate; }

    // The amount of packet loss to cover for with FEC.
    //
    // Input:
    //          - fecRateKey      : Packet loss to cover for with FEC when sending key frames.
    //          - fecRateDelta    : Packet loss to cover for with FEC when sending delta frames.
    void UpdateFECRates(WebRtc_UWord8 fecRateKey, WebRtc_UWord8 fecRateDelta)
                       { _fecRateKey = fecRateKey; _fecRateDelta = fecRateDelta; }

    // Update the protection methods with the current VCMProtectionParameters and
    // choose the best method available. The update involves computing the robustness settings
    // for the protection method.
    //
    // Input:
    //          - newMethod         : If not NULL, this is method will be selected by force.
    //
    // Return value     : True if the selected method is recommended using these settings,
    //                    false if it's the best method, but still not recommended to be used.
    //                    E.g. if NACK is the best available, but the RTT is too large, false
    //                    will be returned.
    bool UpdateMethod(VCMProtectionMethod *newMethod = NULL);

    // Returns the method currently selected.
    //
    // Return value                 : The protection method currently selected.
    VCMProtectionMethod* SelectedMethod() const;

    // Returns the filtered loss probability in the interval [0, 255].
    //
    // Return value                 : The filtered loss probability
    WebRtc_UWord8 FilteredLoss() const;

    // Get constraint on NACK
    //
    // return value                : RTT threshold for using NACK
   WebRtc_UWord8  GetNackThreshold() const;

    void Reset();

private:
    // Sets the available loss protection methods.
    void UpdateMaxLossHistory(WebRtc_UWord8 lossPr255, WebRtc_Word64 now);
    WebRtc_UWord8 MaxFilteredLossPr(WebRtc_Word64 nowMs) const;
    ListWrapper               _availableMethods;
    VCMProtectionMethod*      _selectedMethod;
    VCMProtectionMethod*      _bestNotOkMethod;
    VCMProtectionParameters   _currentParameters;
    WebRtc_UWord32            _rtt;
    float                     _lossPr;
    float                     _bitRate;
    float                     _frameRate;
    float                     _keyFrameSize;
    WebRtc_UWord8             _fecRateKey;
    WebRtc_UWord8             _fecRateDelta;
    WebRtc_Word64             _lastPrUpdateT;
    WebRtc_Word64             _lastPacketPerFrameUpdateT;
    WebRtc_Word64             _lastPacketPerFrameUpdateTKey;
    VCMExpFilter              _lossPr255;
    VCMLossProbabilitySample  _lossPrHistory[kLossPrHistorySize];
    WebRtc_UWord8             _shortMaxLossPr255;
    VCMExpFilter              _packetsPerFrame;
    VCMExpFilter              _packetsPerFrameKey;
    float                     _residualPacketLoss;
    WebRtc_UWord8             _boostRateKey;
    VCMFecTypes               _fecType;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPT_UTIL_H_
