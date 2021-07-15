/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_VP9_CONSTANTS_H_
#define MODULES_VIDEO_CODING_UTILITY_VP9_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace webrtc {
namespace vp9 {

// Number of frames that can be stored for future reference.
static constexpr size_t kNumRefFrames = 8;
// Number of frame contexts that can be store for future reference.
static constexpr size_t kNumFrameContexts = 4;
// Each inter frame can use up to 3 frames for reference.
constexpr size_t kRefsPerFrame = 3;
// Number of values that can be decoded for mv_fr.
constexpr size_t kMvFrSize = 4;
// Number of positions to search in motion vector prediction.
constexpr size_t kMvrefNeighbours = 8;
// Number of contexts when decoding intra_mode .
constexpr size_t kBlockSizeGroups = 4;
// Number of different block sizes used.
constexpr size_t kBlockSizes = 13;
// Sentinel value to mark partition choices that are illegal.
constexpr size_t kBlockInvalid = 14;
// Number of contexts when decoding partition.
constexpr size_t kPartitionContexts = 16;
// Smallest size of a mode info block.
constexpr size_t kMiSize = 8;
// Minimum  width  of a  tile  in  units  of  superblocks  (although tiles on
// the right hand edge can be narrower).
constexpr size_t kMinTileWidth_B64 = 4;
// Maximum width of a tile in units of superblocks.
constexpr size_t kMaxTileWidth_B64 = 64;
// Number of motion vectors returned by find_mv_refs process.
constexpr size_t kMaxMvRefCandidates = 2;
// Number of values that can be derived for ref_frame.
constexpr size_t kMaxRefFrames = 4;
// Number of contexts for is_inter.
constexpr size_t kIsInterContexts = 4;
// Number of contexts for comp_mode.
constexpr size_t kCompModeContexts = 5;
// Number of contexts for single_ref and comp_ref.
constexpr size_t kRefContexts = 5;
// Number of segments allowed in segmentation map.
constexpr size_t kMaxSegments = 8;
// Index for quantizer segment feature.
constexpr size_t kSegLvlAlt_Q = 0;
// Index for loop filter segment feature.
constexpr size_t kSegLvlAlt_L = 1;
// Index for reference frame segment feature.
constexpr size_t kSegLvlRefFrame = 2;
// Index for skip segment feature.
constexpr size_t kSegLvlSkip = 3;
// Number of segment features.
constexpr size_t kSegLvlMax = 4;
// Number of different plane types (Y or UV).
constexpr size_t kBlockTypes = 2;
// Number of different prediction types (intra or inter).
constexpr size_t kRefTypes = 2;
// Number of coefficient bands.
constexpr size_t kCoefBands = 6;
// Number of contexts for decoding coefficients.
constexpr size_t kPrevCoefContexts = 6;
// Number  of  coefficient  probabilities  that  are  directly transmitted.
constexpr size_t kUnconstrainedNodes = 3;
// Number of contexts for transform size.
constexpr size_t kTxSizeContexts = 2;
// Number of values for interp_filter.
constexpr size_t kSwitchableFilters = 3;
// Number of contexts for interp_filter.
constexpr size_t kInterpFilterContexts = 4;
// Number of contexts for decoding skip.
constexpr size_t kSkipContexts = 3;
// Number of values for partition.
constexpr size_t kPartitionTypes = 4;
// Number of values for tx_size.
constexpr size_t kTxSizes = 4;
// Number of values for tx_mode.
constexpr size_t kTxModes = 5;
// Inverse transform rows with DCT and columns with DCT.
constexpr size_t kDctDct = 0;
// Inverse transform rows with DCT and columns with ADST.
constexpr size_t kAdstDct = 1;
// Inverse transform rows with ADST and columns with DCT.
constexpr size_t kDctAdst = 2;
// Inverse transform rows with ADST and columns with ADST.
constexpr size_t kAdstAdst = 3;
// Number of values for y_mode.
constexpr size_t kMbModeCount = 14;
// Number of values for intra_mode.
constexpr size_t kIntraModes = 10;
// Number of values for inter_mode.
constexpr size_t kInterModes = 4;
// Number of contexts for inter_mode.
constexpr size_t kInterModeContexts = 7;
// Number of values for mv_joint.
constexpr size_t kMvJoints = 4;
// Number of values for mv_class.
constexpr size_t kMvClasses = 11;
// Number of values for mv_class0_bit.
constexpr size_t kClass0Size = 2;
// Maximum number of bits for decoding motion vectors.
constexpr size_t kMvOffsetBits = 10;
// Number of values allowed for a probability adjustment.
constexpr size_t kMaxProb = 255;
// Number of different mode types for loop filtering.
constexpr size_t kMaxModeLfDeltas = 2;
// Threshold at which motion vectors are considered large.
constexpr size_t kCompandedMvrefThresh = 8;
// Maximum value used for loop filtering.
constexpr size_t kMaxLoopFilter = 63;
// Number of bits of precision when scaling reference frames.
constexpr size_t kRefScaleShift = 14;
// Number of bits of precision when performing inter prediction.
constexpr size_t kSubpelBits = 4;
// 1 << kSubpelBits.
constexpr size_t kSubpelShifts = 16;
// kSubpelShifts - 1.
constexpr size_t kSubpelMask = 15;
// Value used when clipping motion vectors.
constexpr size_t kMvBorder = 128;
// Value used when clipping motion vectors.
constexpr size_t kInterpExtend = 4;
// Value used when clipping motion vectors.
constexpr size_t kBorderinpixels = 160;
// Value used in adapting probabilities.
constexpr size_t kMaxUpdateFactor = 128;
// Value used in adapting probabilities.
constexpr size_t kCountSat = 20;
// Both candidates use ZEROMV.
constexpr size_t kBothZero = 0;
// One  candidate uses ZEROMV, one uses NEARMV or NEARESTMV.
constexpr size_t kZeroPlusPredicted = 1;
// Both candidates use NEARMV or NEARESTMV.
constexpr size_t kBothPredicted = 2;
// One candidate uses NEWMV, one uses ZEROMV.
constexpr size_t kNewPlusNonIntra = 3;
// Both candidates use NEWMV.
constexpr size_t kBothNew = 4;
// One candidate uses intra prediction, one uses inter prediction.
constexpr size_t kIntraPlusNonIntra = 5;
// Both candidates use intra prediction.
constexpr size_t kBothIntra = 6;
// Sentinel value marking a case that can never occur.
constexpr size_t kInvalidCase = 9;

enum class TxMode : uint8_t {
  kOnly4X4 = 0,
  kAllow8X8 = 1,
  kAllow16x16 = 2,
  kAllow32x32 = 3,
  kTxModeSelect = 4
};

enum BlockSize : uint8_t {
  kBlock4X4 = 0,
  kBlock4X8 = 1,
  kBlock8X4 = 2,
  kBlock8X8 = 3,
  kBlock8X16 = 4,
  kBlock16X8 = 5,
  kBlock16X16 = 6,
  kBlock16X32 = 7,
  kBlock32X16 = 8,
  kBlock32X32 = 9,
  kBlock32X64 = 10,
  kBlock64X32 = 11,
  kBlock64X64 = 12
};

enum Partition : uint8_t {
  kPartitionNone = 0,
  kPartitionHorizontal = 1,
  kPartitionVertical = 2,
  kPartitionSplit = 3
};

enum class ReferenceMode : uint8_t {
  kSingleReference = 0,
  kCompoundReference = 1,
  kReferenceModeSelect = 2,
};

}  // namespace vp9
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_VP9_CONSTANTS_H_
