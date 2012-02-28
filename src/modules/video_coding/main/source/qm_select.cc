/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/main/source/qm_select.h"

#include <math.h>

#include "modules/interface/module_common_types.h"
#include "modules/video_coding/main/source/internal_defines.h"
#include "modules/video_coding/main/source/qm_select_data.h"
#include "modules/video_coding/main/interface/video_coding_defines.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

// QM-METHOD class

VCMQmMethod::VCMQmMethod()
    : _contentMetrics(NULL),
      _width(0),
      _height(0),
      _nativeWidth(0),
      _nativeHeight(0),
      _frameRateLevel(kDefault),
      _init(false) {
  ResetQM();
}

VCMQmMethod::~VCMQmMethod() {
}

void VCMQmMethod::ResetQM() {
  _aspectRatio = 1.0f;
  _imageType = 2;
  _motion.Reset();
  _spatial.Reset();
  _contentClass = 0;
}

uint8_t VCMQmMethod::ComputeContentClass() {
  ComputeMotionNFD();
  ComputeSpatial();
  return _contentClass = 3 * _motion.level + _spatial.level;
}

void VCMQmMethod::UpdateContent(const VideoContentMetrics*  contentMetrics) {
  _contentMetrics = contentMetrics;
}

void VCMQmMethod::ComputeMotionNFD() {
  if (_contentMetrics) {
    _motion.value = _contentMetrics->motion_magnitude;
  }
  // Determine motion level.
  if (_motion.value < kLowMotionNfd) {
    _motion.level = kLow;
  } else if (_motion.value > kHighMotionNfd) {
    _motion.level  = kHigh;
  } else {
    _motion.level = kDefault;
  }
}

void VCMQmMethod::ComputeSpatial() {
  float spatialErr = 0.0;
  float spatialErrH = 0.0;
  float spatialErrV = 0.0;
  if (_contentMetrics) {
    spatialErr =  _contentMetrics->spatial_pred_err;
    spatialErrH = _contentMetrics->spatial_pred_err_h;
    spatialErrV = _contentMetrics->spatial_pred_err_v;
  }
  // Spatial measure: take average of 3 prediction errors.
  _spatial.value = (spatialErr + spatialErrH + spatialErrV) / 3.0f;

  // Reduce thresholds for large scenes/higher pixel correlation (~>=WHD).
  float scale2 = _imageType > 3 ? kScaleTexture : 1.0;

  if (_spatial.value > scale2 * kHighTexture) {
    _spatial.level = kHigh;
  } else if (_spatial.value < scale2 * kLowTexture) {
    _spatial.level = kLow;
  } else {
    _spatial.level = kDefault;
  }
}

uint8_t VCMQmMethod::GetImageType(uint16_t width,
                                  uint16_t height) {
  // Get the closest image type for encoder frame size.
  uint32_t imageSize = width * height;
  if (imageSize < kFrameSizeTh[0]) {
    return 0;  // QCIF
  } else if (imageSize < kFrameSizeTh[1]) {
    return 1;  // CIF
  } else if (imageSize < kFrameSizeTh[2]) {
    return 2;  // VGA
  } else if (imageSize < kFrameSizeTh[3]) {
    return 3;  // 4CIF
  } else if (imageSize < kFrameSizeTh[4]) {
    return 4;  // 720,4:3
  } else if (imageSize < kFrameSizeTh[5]) {
    return 5;  // WHD
  } else {
    return 6;  // HD
  }
}

LevelClass VCMQmMethod::FrameRateLevel(float avgFrameRate) {
  if (avgFrameRate < kLowFrameRate) {
    return kLow;
  } else if (avgFrameRate > kHighFrameRate) {
    return kHigh;
  } else {
    return kDefault;
  }
}

// RESOLUTION CLASS

VCMQmResolution::VCMQmResolution()
    :  _qm(new VCMResolutionScale()) {
  Reset();
}

VCMQmResolution::~VCMQmResolution() {
  delete _qm;
}

void VCMQmResolution::ResetRates() {
  _sumTargetRate = 0.0f;
  _sumIncomingFrameRate = 0.0f;
  _sumRateMM = 0.0f;
  _sumRateMMSgn = 0;
  _sumPacketLoss = 0.0f;
  _frameCnt = 0;
  _frameCntDelta = 0;
  _lowBufferCnt = 0;
  _updateRateCnt = 0;
}

void VCMQmResolution::ResetDownSamplingState() {
  _stateDecFactorSpatial = 1;
  _stateDecFactorTemp  = 1;
}

void VCMQmResolution::Reset() {
  _targetBitRate = 0.0f;
  _userFrameRate = 0.0f;
  _incomingFrameRate = 0.0f;
  _perFrameBandwidth =0.0f;
  _bufferLevel = 0.0f;
  _avgTargetRate = 0.0f;
  _avgIncomingFrameRate = 0.0f;
  _avgRatioBufferLow = 0.0f;
  _avgRateMisMatch = 0.0f;
  _avgRateMisMatchSgn = 0.0f;
  _avgPacketLoss = 0.0f;
  _encoderState = kStableEncoding;
  ResetRates();
  ResetDownSamplingState();
  ResetQM();
}

EncoderState VCMQmResolution::GetEncoderState() {
  return _encoderState;
}

// Initialize state after re-initializing the encoder,
// i.e., after SetEncodingData() in mediaOpt.
int VCMQmResolution::Initialize(float bitRate,
                                float userFrameRate,
                                uint16_t width,
                                uint16_t height) {
  if (userFrameRate == 0.0f || width == 0 || height == 0) {
    return VCM_PARAMETER_ERROR;
  }
  Reset();
  _targetBitRate = bitRate;
  _userFrameRate = userFrameRate;
  _incomingFrameRate = userFrameRate;
  UpdateCodecFrameSize(width, height);
  _nativeWidth = width;
  _nativeHeight = height;
  // Initial buffer level.
  _bufferLevel = kInitBufferLevel * _targetBitRate;
  // Per-frame bandwidth.
  _perFrameBandwidth = _targetBitRate / _userFrameRate;
  _init  = true;
  return VCM_OK;
}

void VCMQmResolution::UpdateCodecFrameSize(uint16_t width, uint16_t height) {
  _width = width;
  _height = height;
  // Set the imageType for the encoder width/height.
  _imageType = GetImageType(width, height);
}

// Update rate data after every encoded frame.
void VCMQmResolution::UpdateEncodedSize(int encodedSize,
                                        FrameType encodedFrameType) {
  _frameCnt++;
  // Convert to Kbps.
  float encodedSizeKbits = static_cast<float>((encodedSize * 8.0) / 1000.0);

  // Update the buffer level:
  // Note this is not the actual encoder buffer level.
  // |_bufferLevel| is reset to 0 every time SelectResolution is called, and
  // does not account for frame dropping by encoder or VCM.
  _bufferLevel += _perFrameBandwidth - encodedSizeKbits;
  // Counter for occurrences of low buffer level:
  // low/negative values means encoder is likely dropping frames.
  if (_bufferLevel <= kPercBufferThr * kOptBufferLevel * _targetBitRate) {
    _lowBufferCnt++;
  }
}

// Update various quantities after SetTargetRates in MediaOpt.
void VCMQmResolution::UpdateRates(float targetBitRate,
                                  float encoderSentRate,
                                  float incomingFrameRate,
                                  uint8_t packetLoss) {
  // Sum the target bitrate and incoming frame rate:
  // these values are the encoder rates (from previous update ~1sec),
  // i.e, before the update for next ~1sec.
  _sumTargetRate += _targetBitRate;
  _sumIncomingFrameRate += _incomingFrameRate;
  _updateRateCnt++;

  // Sum the received (from RTCP reports) packet loss rates.
  _sumPacketLoss += static_cast<float>(packetLoss / 255.0);

  // Sum the sequence rate mismatch:
  // Mismatch here is based on the difference between the target rate
  // used (in previous ~1sec) and the average actual encoding rate measured
  // at previous ~1sec.
  float diff = _targetBitRate - encoderSentRate;
  if (_targetBitRate > 0.0)
    _sumRateMM += fabs(diff) / _targetBitRate;
  int sgnDiff = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
  // To check for consistent under(+)/over_shooting(-) of target rate.
  _sumRateMMSgn += sgnDiff;

  // Update with the current new target and frame rate:
  // these values are ones the encoder will use for the current/next ~1sec
  _targetBitRate =  targetBitRate;
  _incomingFrameRate = incomingFrameRate;

  // Update the per_frame_bandwidth:
  // this is the per_frame_bw for the current/next ~1sec
  _perFrameBandwidth  = 0.0f;
  if (_incomingFrameRate > 0.0f) {
    _perFrameBandwidth = _targetBitRate / _incomingFrameRate;
  }
}

// Select the resolution factors: frame size and frame rate change (qm scales).
// Selection is for going down in resolution, or for going back up
// (if a previous down-sampling action was taken).

// In the current version the following constraints are imposed:
// 1) we only allow for one action (either down or back up) at a given time.
// 2) the possible down-sampling actions are: 2x2 spatial and 1/2 temporal.
// 3) the total amount of down-sampling (spatial and/or temporal) from the
//    initial (native) resolution is limited by various factors.

// TODO(marpan): extend to allow options for: 4/3x4/3, 1x2, 2x1 spatial,
// and 2/3 temporal (i.e., skip every third frame).
int VCMQmResolution::SelectResolution(VCMResolutionScale** qm) {
  if (!_init) {
    return VCM_UNINITIALIZED;
  }
  if (_contentMetrics == NULL) {
    Reset();
    *qm =  _qm;
    return VCM_OK;
  }

  // Default settings: no action.
  _qm->spatialWidthFact = 1;
  _qm->spatialHeightFact = 1;
  _qm->temporalFact = 1;
  *qm = _qm;

  // Compute content class for selection.
  _contentClass = ComputeContentClass();

  // Compute various rate quantities for selection.
  ComputeRatesForSelection();

  // Get the encoder state.
  ComputeEncoderState();

  // Check for going back up in resolution, if we have had some down-sampling
  // relative to native state in Initialize (i.e., after SetEncodingData()
  // in mediaOpt.).
  if (_stateDecFactorSpatial > 1 || _stateDecFactorTemp > 1) {
    if (GoingUpResolution()) {
      *qm = _qm;
      return VCM_OK;
    }
  }

  // Check for going down in resolution, only if current total amount of
  // down-sampling state is below threshold.
  if (_stateDecFactorTemp * _stateDecFactorSpatial < kMaxDownSample) {
    if (GoingDownResolution()) {
      *qm = _qm;
      return VCM_OK;
    }
  }
  return VCM_OK;
}

void VCMQmResolution::ComputeRatesForSelection() {
  _avgTargetRate = 0.0f;
  _avgIncomingFrameRate = 0.0f;
  _avgRatioBufferLow = 0.0f;
  _avgRateMisMatch = 0.0f;
  _avgRateMisMatchSgn = 0.0f;
  _avgPacketLoss = 0.0f;
  if (_frameCnt > 0) {
    _avgRatioBufferLow = static_cast<float>(_lowBufferCnt) /
        static_cast<float>(_frameCnt);
  }
  if (_updateRateCnt > 0) {
    _avgRateMisMatch = static_cast<float>(_sumRateMM) /
        static_cast<float>(_updateRateCnt);
    _avgRateMisMatchSgn = static_cast<float>(_sumRateMMSgn) /
        static_cast<float>(_updateRateCnt);
    _avgTargetRate = static_cast<float>(_sumTargetRate) /
        static_cast<float>(_updateRateCnt);
    _avgIncomingFrameRate = static_cast<float>(_sumIncomingFrameRate) /
        static_cast<float>(_updateRateCnt);
    _avgPacketLoss =  static_cast<float>(_sumPacketLoss) /
        static_cast<float>(_updateRateCnt);
  }
  // For selection we may want to weight some quantities more heavily
  // with the current (i.e., next ~1sec) rate values.
  float weight = 0.7f;
  _avgTargetRate = weight * _avgTargetRate + (1.0 - weight) * _targetBitRate;
  _avgIncomingFrameRate = weight * _avgIncomingFrameRate +
      (1.0 - weight) * _incomingFrameRate;
  _frameRateLevel = FrameRateLevel(_avgIncomingFrameRate);
}

void VCMQmResolution::ComputeEncoderState() {
  // Default.
  _encoderState = kStableEncoding;

  // Assign stressed state if:
  // 1) occurrences of low buffer levels is high, or
  // 2) rate mis-match is high, and consistent over-shooting by encoder.
  if ((_avgRatioBufferLow > kMaxBufferLow) ||
      ((_avgRateMisMatch > kMaxRateMisMatch) &&
          (_avgRateMisMatchSgn < -kRateOverShoot))) {
    _encoderState = kStressedEncoding;
  }
  // Assign easy state if:
  // 1) rate mis-match is high, and
  // 2) consistent under-shooting by encoder.
  if ((_avgRateMisMatch > kMaxRateMisMatch) &&
      (_avgRateMisMatchSgn > kRateUnderShoot)) {
    _encoderState = kEasyEncoding;
  }
}

bool VCMQmResolution::GoingUpResolution() {
  // Check if we should go up both spatially and temporally.
  if (_stateDecFactorSpatial > 1 && _stateDecFactorTemp > 1) {
    if (ConditionForGoingUp(2, 2, 2, kTransRateScaleUpSpatialTemp)) {
      _qm->spatialHeightFact = 0;
      _qm->spatialWidthFact = 0;
      _qm->temporalFact = 0;
      UpdateDownsamplingState(kUpResolution);
      return true;
    }
  } else {
    // Check if we should go up either spatially or temporally.
    bool selectedUpS = false;
    bool selectedUpT = false;
    if (_stateDecFactorSpatial > 1) {
      selectedUpS = ConditionForGoingUp(2, 2, 1, kTransRateScaleUpSpatial);
    }
    if (_stateDecFactorTemp > 1) {
      selectedUpT = ConditionForGoingUp(1, 1, 2, kTransRateScaleUpTemp);
    }
    if (selectedUpS && !selectedUpT) {
      _qm->spatialHeightFact = 0;
      _qm->spatialWidthFact = 0;
      UpdateDownsamplingState(kUpResolution);
      return true;
    } else if (!selectedUpS && selectedUpT) {
      _qm->temporalFact = 0;
      UpdateDownsamplingState(kUpResolution);
      return true;
    } else if (selectedUpS && selectedUpT) {
      // TODO(marpan): which one to pick?
      // pickSpatialOrTemporal()
      // For now take spatial over temporal.
      _qm->spatialHeightFact = 0;
      _qm->spatialWidthFact = 0;
      UpdateDownsamplingState(kUpResolution);
      return true;
    }
  }
  return false;
}

bool VCMQmResolution::ConditionForGoingUp(uint8_t facWidth,
                                          uint8_t facHeight,
                                          uint8_t facTemp,
                                          float scaleFac) {
  float estimatedTransitionRateUp = GetTransitionRate(facWidth, facHeight,
                                                    facTemp, scaleFac);
  // Go back up if:
  // 1) target rate is above threshold and current encoder state is stable, or
  // 2) encoder state is easy (encoder is significantly under-shooting target).
  if (((_avgTargetRate > estimatedTransitionRateUp) &&
      (_encoderState == kStableEncoding)) ||
      (_encoderState == kEasyEncoding)) {
    return true;
  } else {
    return false;
  }
}

bool VCMQmResolution::GoingDownResolution() {
  float estimatedTransitionRateDown = GetTransitionRate(1, 1, 1, 1.0);
  float maxRate = kFrameRateFac[_frameRateLevel] * kMaxRateQm[_imageType];

  // TODO(marpan): Bias down-sampling based on packet loss conditions.

  // Resolution reduction if:
  // (1) target rate is below transition rate, or
  // (2) encoder is in stressed state and target rate below a max threshold.
  if ((_avgTargetRate < estimatedTransitionRateDown ) ||
      (_encoderState == kStressedEncoding && _avgTargetRate < maxRate)) {
    // Get the down-sampling action.
    uint8_t spatialFact = kSpatialAction[_contentClass];
    uint8_t tempFact = kTemporalAction[_contentClass];

    switch (spatialFact) {
      case 4: {
        _qm->spatialWidthFact = 2;
        _qm->spatialHeightFact = 2;
        break;
      }
      case 2: {
        assert(false);  // Currently not used.
        // Select 1x2,2x1, or 4/3x4/3.
        // SelectSpatialDirectionMode((float) estimatedTransitionRateDown);
        break;
      }
      case 1: {
        _qm->spatialWidthFact = 1;
        _qm->spatialHeightFact = 1;
        break;
      }
      default: {
        assert(false);
      }
    }
    switch (tempFact) {
      case 2: {
        _qm->temporalFact = 2;
        break;
      }
      case 1: {
        _qm->temporalFact = 1;
        break;
      }
      default: {
        assert(false);
      }
    }
    // Adjust some cases based on frame rate.
    // TODO(marpan): will be modified when we add 1/2 spatial and 2/3 temporal.
    AdjustAction();

    // Sanity checks on down-sampling selection:
    // override the settings for too small image size and/or frame rate.
    // Also check the limit on current down-sampling states.

    // No spatial sampling if current frame size is too small (QCIF),
    // or if amount of spatial down-sampling is already too much.
    if ((_width * _height) <= kMinImageSize ||
        _stateDecFactorSpatial >= kMaxSpatialDown) {
      _qm->spatialWidthFact = 1;
      _qm->spatialHeightFact = 1;
    }
    // No frame rate reduction if average frame rate is below some point,
    // or if the amount of temporal down-sampling is already too much.
    if (_avgIncomingFrameRate <= kMinFrameRate ||
        _stateDecFactorTemp >= kMaxTempDown) {
      _qm->temporalFact = 1;
    }

    // Update down-sampling state.
    if (_qm->spatialWidthFact != 1 || _qm->spatialHeightFact != 1 ||
               _qm->temporalFact != 1) {
      UpdateDownsamplingState(kDownResolution);
      return true;
    }
  }
  return false;
}

float VCMQmResolution::GetTransitionRate(uint8_t facWidth,
                                         uint8_t facHeight,
                                         uint8_t facTemp,
                                         float scaleFac) {
  uint8_t imageType = GetImageType(facWidth * _width,
                                   facHeight * _height);
  LevelClass frameRateLevel = FrameRateLevel(facTemp * _avgIncomingFrameRate);

  // The maximum allowed rate below which down-sampling is allowed:
  // Nominal values based on image format (frame size and frame rate).
  float maxRate = kFrameRateFac[frameRateLevel] * kMaxRateQm[imageType];

  uint8_t imageClass = imageType > 3 ? 1: 0;
  uint8_t tableIndex = imageClass * 9 + _contentClass;
  // Scale factor for down-sampling transition threshold:
  // factor based on the content class and the image size.
  float scaleTransRate = kScaleTransRateQm[tableIndex];

  // Threshold bitrate for resolution action.
  return static_cast<float> (scaleFac * facTemp * _incomingFrameRate *
      scaleTransRate * maxRate / 30);
}

void VCMQmResolution::UpdateDownsamplingState(ResolutionAction action) {
  // Assumes for now only actions are 1/2 frame rate of 2x2 spatial.
  if (action == kUpResolution) {
    if (_qm->spatialHeightFact == 0 && _qm->spatialWidthFact == 0) {
      _stateDecFactorSpatial = _stateDecFactorSpatial / 4;
      assert(_stateDecFactorSpatial >= 1);
    }
    if (_qm->temporalFact == 0) {
      _stateDecFactorTemp = _stateDecFactorTemp / 2;
      assert(_stateDecFactorTemp >= 1);
    }
  } else if (action == kDownResolution) {
    _stateDecFactorSpatial = _stateDecFactorSpatial * _qm->spatialWidthFact
        * _qm->spatialHeightFact;
    _stateDecFactorTemp = _stateDecFactorTemp * _qm->temporalFact;
    assert(_stateDecFactorSpatial >= 1);
    assert(_stateDecFactorTemp >= 1);
  } else {
    assert(false);
  }
}

void VCMQmResolution::AdjustAction() {
  if (_spatial.level == kDefault && _motion.level != kHigh &&
      _frameRateLevel == kHigh) {
      _qm->temporalFact = 2;
      _qm->spatialWidthFact = 1;
      _qm->spatialHeightFact = 1;
  }
}

// TODO(marpan): Update this when we allow for 1/2 spatial down-sampling.
void VCMQmResolution::SelectSpatialDirectionMode(float transRate) {
  // Default is 1x2 (H)
  // For bit rates well below transitional rate, we select 2x2.
  if (_targetBitRate < transRate * kRateRedSpatial2X2) {
    _qm->spatialWidthFact = 2;
    _qm->spatialHeightFact = 2;
  }
  // Otherwise check prediction errors and aspect ratio.
  float spatialErr = 0.0;
  float spatialErrH = 0.0;
  float spatialErrV = 0.0;
  if (_contentMetrics) {
    spatialErr = _contentMetrics->spatial_pred_err;
    spatialErrH = _contentMetrics->spatial_pred_err_h;
    spatialErrV = _contentMetrics->spatial_pred_err_v;
  }

  // Favor 1x2 if aspect_ratio is 16:9.
  if (_aspectRatio >= 16.0f / 9.0f) {
    // Check if 1x2 has lowest prediction error.
    if (spatialErrH < spatialErr && spatialErrH < spatialErrV) {
      _qm->spatialWidthFact = 2;
      _qm->spatialHeightFact = 1;
    }
  }
  // Check for 2x2 selection: favor 2x2 over 1x2 and 2x1.
  if (spatialErr < spatialErrH * (1.0f + kSpatialErr2x2VsHoriz) &&
      spatialErr < spatialErrV * (1.0f + kSpatialErr2X2VsVert)) {
    _qm->spatialWidthFact = 2;
    _qm->spatialHeightFact = 2;
  }
  // Check for 2x1 selection.
  if (spatialErrV < spatialErrH * (1.0f - kSpatialErrVertVsHoriz) &&
      spatialErrV < spatialErr * (1.0f - kSpatialErr2X2VsVert)) {
    _qm->spatialWidthFact = 1;
    _qm->spatialHeightFact = 2;
  }
}

// ROBUSTNESS CLASS

VCMQmRobustness::VCMQmRobustness() {
  Reset();
}

VCMQmRobustness::~VCMQmRobustness() {
}

void VCMQmRobustness::Reset() {
  _prevTotalRate = 0.0f;
  _prevRttTime = 0;
  _prevPacketLoss = 0;
  _prevCodeRateDelta = 0;
  ResetQM();
}

// Adjust the FEC rate based on the content and the network state
// (packet loss rate, total rate/bandwidth, round trip time).
// Note that packetLoss here is the filtered loss value.
float VCMQmRobustness::AdjustFecFactor(uint8_t codeRateDelta,
                                       float totalRate,
                                       float frameRate,
                                       uint32_t rttTime,
                                       uint8_t packetLoss) {
  // Default: no adjustment
  float adjustFec =  1.0f;
  if (_contentMetrics == NULL) {
    return adjustFec;
  }
  // Compute class state of the content.
  ComputeMotionNFD();
  ComputeSpatial();

  // TODO(marpan): Set FEC adjustment factor.

  // Keep track of previous values of network state:
  // adjustment may be also based on pattern of changes in network state.
  _prevTotalRate = totalRate;
  _prevRttTime = rttTime;
  _prevPacketLoss = packetLoss;
  _prevCodeRateDelta = codeRateDelta;
  return adjustFec;
}

// Set the UEP (unequal-protection across packets) on/off for the FEC.
bool VCMQmRobustness::SetUepProtection(uint8_t codeRateDelta,
                                       float totalRate,
                                       uint8_t packetLoss,
                                       bool frameType) {
  // Default.
  return false;
}
}  // end of namespace
