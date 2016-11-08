/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/h264/profile_level_id.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "webrtc/base/arraysize.h"

namespace {

// For level_idc=11 and profile_idc=0x42, 0x4D, or 0x58, the constraint set3
// flag specifies if level 1b or level 1.1 is used.
const uint8_t kConstraintSet3Flag = 0x10;

// Convert a string of 8 characters into a byte where the positions containing
// character c will have their bit set. For example, c = 'x', str = "x1xx0000"
// will return 0b10110000. constexpr is used so that the pattern table in
// kProfilePatterns is statically initialized.
constexpr uint8_t ByteMaskString(char c, const char (&str)[9]) {
  return (str[0] == c) << 7
      | (str[1] == c) << 6
      | (str[2] == c) << 5
      | (str[3] == c) << 4
      | (str[4] == c) << 3
      | (str[5] == c) << 2
      | (str[6] == c) << 1
      | (str[7] == c) << 0;
}

// Class for matching bit patterns such as "x1xx0000" where 'x' is allowed to be
// either 0 or 1.
class BitPattern {
 public:
  constexpr BitPattern(const char (&str)[9])
      : mask_(~ByteMaskString('x', str)),
        masked_value_(ByteMaskString('1', str)) {}

  bool IsMatch(uint8_t value) const { return masked_value_ == (value & mask_); }

 private:
  const uint8_t mask_;
  const uint8_t masked_value_;
};

// Table for converting between profile_idc/profile_iop to H264::Profile.
struct ProfilePattern {
  const uint8_t profile_idc;
  const BitPattern profile_iop;
  const webrtc::H264::Profile profile;
};

// This is from https://tools.ietf.org/html/rfc6184#section-8.1.
constexpr ProfilePattern kProfilePatterns[] = {
    {0x42, BitPattern("x1xx0000"), webrtc::H264::kProfileConstrainedBaseline},
    {0x4D, BitPattern("1xxx0000"), webrtc::H264::kProfileConstrainedBaseline},
    {0x58, BitPattern("11xx0000"), webrtc::H264::kProfileConstrainedBaseline},
    {0x42, BitPattern("x0xx0000"), webrtc::H264::kProfileBaseline},
    {0x58, BitPattern("10xx0000"), webrtc::H264::kProfileBaseline},
    {0x4D, BitPattern("0x0x0000"), webrtc::H264::kProfileMain},
    {0x64, BitPattern("00000000"), webrtc::H264::kProfileHigh},
    {0x64, BitPattern("00001100"), webrtc::H264::kProfileConstrainedHigh}};

struct LevelConstraint {
  const int max_macroblocks_per_second;
  const int max_macroblock_frame_size;
  const webrtc::H264::Level level;
};

// This is from ITU-T H.264 (02/2016) Table A-1 – Level limits.
static constexpr LevelConstraint kLevelConstraints[] = {
    {1485, 99, webrtc::H264::kLevel1},
    {1485, 99, webrtc::H264::kLevel1_b},
    {3000, 396, webrtc::H264::kLevel1_1},
    {6000, 396, webrtc::H264::kLevel1_2},
    {11880, 396, webrtc::H264::kLevel1_3},
    {11880, 396, webrtc::H264::kLevel2},
    {19800, 792, webrtc::H264::kLevel2_1},
    {20250, 1620, webrtc::H264::kLevel2_2},
    {40500, 1620, webrtc::H264::kLevel3},
    {108000, 3600, webrtc::H264::kLevel3_1},
    {216000, 5120, webrtc::H264::kLevel3_2},
    {245760, 8192, webrtc::H264::kLevel4},
    {245760, 8192, webrtc::H264::kLevel4_1},
    {522240, 8704, webrtc::H264::kLevel4_2},
    {589824, 22080, webrtc::H264::kLevel5},
    {983040, 3684, webrtc::H264::kLevel5_1},
    {2073600, 3684, webrtc::H264::kLevel5_2},
};

}  // anonymous namespace

namespace webrtc {
namespace H264 {

rtc::Optional<ProfileLevelId> ParseProfileLevelId(const char* str) {
  // The string should consist of 3 bytes in hexadecimal format.
  if (strlen(str) != 6u)
    return rtc::Optional<ProfileLevelId>();
  const uint32_t profile_level_id_numeric = strtol(str, nullptr, 16);
  if (profile_level_id_numeric == 0)
    return rtc::Optional<ProfileLevelId>();

  // Separate into three bytes.
  const uint8_t level_idc =
      static_cast<uint8_t>(profile_level_id_numeric & 0xFF);
  const uint8_t profile_iop =
      static_cast<uint8_t>((profile_level_id_numeric >> 8) & 0xFF);
  const uint8_t profile_idc =
      static_cast<uint8_t>((profile_level_id_numeric >> 16) & 0xFF);

  // Parse level based on level_idc and constraint set 3 flag.
  Level level;
  switch (level_idc) {
    case kLevel1_1:
      level = (profile_iop & kConstraintSet3Flag) != 0 ? kLevel1_b : kLevel1_1;
      break;
    case kLevel1:
    case kLevel1_2:
    case kLevel1_3:
    case kLevel2:
    case kLevel2_1:
    case kLevel2_2:
    case kLevel3:
    case kLevel3_1:
    case kLevel3_2:
    case kLevel4:
    case kLevel4_1:
    case kLevel4_2:
    case kLevel5:
    case kLevel5_1:
    case kLevel5_2:
      level = static_cast<Level>(level_idc);
      break;
    default:
      // Unrecognized level_idc.
      return rtc::Optional<ProfileLevelId>();
  }

  // Parse profile_idc/profile_iop into a Profile enum.
  for (const ProfilePattern& pattern : kProfilePatterns) {
    if (profile_idc == pattern.profile_idc &&
        pattern.profile_iop.IsMatch(profile_iop)) {
      return rtc::Optional<ProfileLevelId>({pattern.profile, level});
    }
  }

  // Unrecognized profile_idc/profile_iop combination.
  return rtc::Optional<ProfileLevelId>();
}

rtc::Optional<Level> SupportedLevel(int max_frame_pixel_count, float max_fps) {
  static const int kPixelsPerMacroblock = 16 * 16;

  for (int i = arraysize(kLevelConstraints) - 1; i >= 0; --i) {
    const LevelConstraint& level_constraint = kLevelConstraints[i];
    if (level_constraint.max_macroblock_frame_size * kPixelsPerMacroblock <=
            max_frame_pixel_count &&
        level_constraint.max_macroblocks_per_second <=
            max_fps * level_constraint.max_macroblock_frame_size) {
      return rtc::Optional<Level>(level_constraint.level);
    }
  }

  // No level supported.
  return rtc::Optional<Level>();
}

rtc::Optional<std::string> ProfileLevelIdToString(
    const ProfileLevelId& profile_level_id) {
  // Handle special case level == 1b.
  if (profile_level_id.level == kLevel1_b) {
    switch (profile_level_id.profile) {
      case kProfileConstrainedBaseline:
        return rtc::Optional<std::string>("42f00b");
      case kProfileBaseline:
        return rtc::Optional<std::string>("42100b");
      case kProfileMain:
        return rtc::Optional<std::string>("4d100b");
      // Level 1b is not allowed for other profiles.
      default:
        return rtc::Optional<std::string>();
    }
  }

  const char* profile_idc_iop_string;
  switch (profile_level_id.profile) {
    case kProfileConstrainedBaseline:
      profile_idc_iop_string = "42e0";
      break;
    case kProfileBaseline:
      profile_idc_iop_string = "4200";
      break;
    case kProfileMain:
      profile_idc_iop_string = "4d00";
      break;
    case kProfileConstrainedHigh:
      profile_idc_iop_string = "640c";
      break;
    case kProfileHigh:
      profile_idc_iop_string = "6400";
      break;
    // Unrecognized profile.
    default:
      return rtc::Optional<std::string>();
  }

  char str[7];
  snprintf(str, 7u, "%s%02x", profile_idc_iop_string, profile_level_id.level);
  return rtc::Optional<std::string>(str);
}

}  // namespace H264
}  // namespace webrtc
