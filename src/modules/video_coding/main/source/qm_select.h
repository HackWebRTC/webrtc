/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_QM_SELECT_H_
#define WEBRTC_MODULES_VIDEO_CODING_QM_SELECT_H_

#include "common_types.h"
#include "typedefs.h"

/******************************************************/
/* Quality Modes: Resolution and Robustness settings  */
/******************************************************/

namespace webrtc {
struct VideoContentMetrics;

struct VCMResolutionScale {
  VCMResolutionScale()
      : spatialWidthFact(1),
        spatialHeightFact(1),
        temporalFact(1) {
  }
  uint8_t spatialWidthFact;
  uint8_t spatialHeightFact;
  uint8_t temporalFact;
};

enum LevelClass {
  kLow,
  kHigh,
  kDefault
};

struct VCMContFeature {
  VCMContFeature()
      : value(0.0f),
        level(kDefault) {
  }
  void Reset() {
    value = 0.0f;
    level = kDefault;
  }
  float value;
  LevelClass level;
};

enum ResolutionAction {
  kDownResolution,
  kUpResolution,
  kNoChangeResolution
};

enum EncoderState {
  kStableEncoding,    // Low rate mis-match, stable buffer levels.
  kStressedEncoding,  // Significant over-shooting of target rate,
                      // Buffer under-flow, etc.
  kEasyEncoding       // Significant under-shooting of target rate.
};

// QmMethod class: main class for resolution and robustness settings

class VCMQmMethod {
 public:
  VCMQmMethod();
  virtual ~VCMQmMethod();

  // Reset values
  void ResetQM();
  virtual void Reset() = 0;

  // Compute content class.
  uint8_t ComputeContentClass();

  // Update with the content metrics.
  void UpdateContent(const VideoContentMetrics* contentMetrics);

  // Compute spatial texture magnitude and level.
  // Spatial texture is a spatial prediction error measure.
  void ComputeSpatial();

  // Compute motion magnitude and level for NFD metric.
  // NFD is normalized frame difference (normalized by spatial variance).
  void ComputeMotionNFD();

  // Get the imageType (CIF, VGA, HD, etc) for the system width/height.
  uint8_t GetImageType(uint16_t width, uint16_t height);

  // Get the frame rate level.
  LevelClass FrameRateLevel(float frame_rate);

 protected:
  // Content Data.
  const VideoContentMetrics* _contentMetrics;

  // Encoder frame sizes and native frame sizes.
  uint16_t _width;
  uint16_t _height;
  uint16_t _nativeWidth;
  uint16_t _nativeHeight;
  float _aspectRatio;
  // Image type and frame rate leve, for the current encoder resolution.
  uint8_t _imageType;
  LevelClass _frameRateLevel;
  // Content class data.
  VCMContFeature _motion;
  VCMContFeature _spatial;
  uint8_t _contentClass;
  bool _init;
};

// Resolution settings class

class VCMQmResolution : public VCMQmMethod {
 public:
  VCMQmResolution();
  virtual ~VCMQmResolution();

  // Reset all quantities.
  virtual void Reset();

  // Reset rate quantities and counters after every SelectResolution() call.
  void ResetRates();

  // Reset down-sampling state.
  void ResetDownSamplingState();

  // Get the encoder state.
  EncoderState GetEncoderState();

  // Initialize after SetEncodingData in media_opt.
  int Initialize(float bitRate, float userFrameRate,
                 uint16_t width, uint16_t height);

  // Update the encoder frame size.
  void UpdateCodecFrameSize(uint16_t width, uint16_t height);

  // Update with actual bit rate (size of the latest encoded frame)
  // and frame type, after every encoded frame.
  void UpdateEncodedSize(int encodedSize,
                         FrameType encodedFrameType);

  // Update with new target bitrate, actual encoder sent rate, frame_rate,
  // loss rate: every ~1 sec from SetTargetRates in media_opt.
  void UpdateRates(float targetBitRate, float encoderSentRate,
                   float incomingFrameRate, uint8_t packetLoss);

  // Extract ST (spatio-temporal) resolution action.
  // Inputs: qm: Reference to the quality modes pointer.
  // Output: the spatial and/or temporal scale change.
  int SelectResolution(VCMResolutionScale** qm);

  // Compute rates for the selection of down-sampling action.
  void ComputeRatesForSelection();

  // Compute the encoder state.
  void ComputeEncoderState();

  // Return true if the action is to go back up in resolution.
  bool GoingUpResolution();

  // Return true if the action is to go down in resolution.
  bool GoingDownResolution();

  // Check the condition for going up in resolution by the scale factors:
  // |facWidth|, |facHeight|, |facTemp|.
  // |scaleFac| is a scale factor for the transition rate.
  bool ConditionForGoingUp(uint8_t facWidth, uint8_t facHeight,
                           uint8_t facTemp,
                           float scaleFac);

  // Get the bitrate threshold for the resolution action.
  // The case |facWidth|=|facHeight|=|facTemp|==1 is for down-sampling action.
  // |scaleFac| is a scale factor for the transition rate.
  float GetTransitionRate(uint8_t facWidth, uint8_t facHeight,
                          uint8_t facTemp, float scaleFac);

  // Update the downsampling state.
  void UpdateDownsamplingState(ResolutionAction action);

  void AdjustAction();

  // Select the directional (1x2 or 2x1) spatial down-sampling action.
  void SelectSpatialDirectionMode(float transRate);

 private:
  VCMResolutionScale* _qm;
  // Encoder rate control parameters.
  float _targetBitRate;
  float _userFrameRate;
  float _incomingFrameRate;
  float _perFrameBandwidth;
  float _bufferLevel;

  // Data accumulated every ~1sec from MediaOpt.
  float _sumTargetRate;
  float _sumIncomingFrameRate;
  float _sumRateMM;
  float _sumRateMMSgn;
  float  _sumPacketLoss;
  // Counters.
  uint32_t _frameCnt;
  uint32_t _frameCntDelta;
  uint32_t _updateRateCnt;
  uint32_t _lowBufferCnt;

  // Resolution state parameters.
  uint8_t _stateDecFactorSpatial;
  uint8_t _stateDecFactorTemp;

  // Quantities used for selection.
  float _avgTargetRate;
  float _avgIncomingFrameRate;
  float _avgRatioBufferLow;
  float _avgRateMisMatch;
  float _avgRateMisMatchSgn;
  float _avgPacketLoss;
  EncoderState _encoderState;
};

// Robustness settings class.

class VCMQmRobustness : public VCMQmMethod {
 public:
  VCMQmRobustness();
  ~VCMQmRobustness();

  virtual void Reset();

  // Adjust FEC rate based on content: every ~1 sec from SetTargetRates.
  // Returns an adjustment factor.
  float AdjustFecFactor(uint8_t codeRateDelta,
                        float totalRate,
                        float frameRate,
                        uint32_t rttTime,
                        uint8_t packetLoss);

  // Set the UEP protection on/off.
  bool SetUepProtection(uint8_t codeRateDelta,
                        float totalRate,
                        uint8_t packetLoss,
                        bool frameType);

 private:
  // Previous state of network parameters.
  float _prevTotalRate;
  uint32_t _prevRttTime;
  uint8_t _prevPacketLoss;
  uint8_t _prevCodeRateDelta;
};
}   // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CODING_QM_SELECT_H_

