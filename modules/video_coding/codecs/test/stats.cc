/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/stats.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

std::string FrameStatistic::ToString() const {
  std::stringstream ss;
  ss << "frame " << frame_number;
  ss << " " << decoded_width << "x" << decoded_height;
  ss << " sl " << simulcast_svc_idx;
  ss << " tl " << temporal_layer_idx;
  ss << " type " << frame_type;
  ss << " length " << encoded_frame_size_bytes;
  ss << " qp " << qp;
  ss << " psnr " << psnr;
  ss << " ssim " << ssim;
  ss << " enc_time_us " << encode_time_us;
  ss << " dec_time_us " << decode_time_us;
  ss << " rtp_ts " << rtp_timestamp;
  ss << " bitrate_kbps " << target_bitrate_kbps;
  return ss.str();
}

FrameStatistic* Stats::AddFrame(size_t timestamp) {
  RTC_DCHECK(rtp_timestamp_to_frame_num_.find(timestamp) ==
             rtp_timestamp_to_frame_num_.end());
  const size_t frame_number = stats_.size();
  rtp_timestamp_to_frame_num_[timestamp] = frame_number;
  stats_.emplace_back(frame_number, timestamp);
  return &stats_.back();
}

FrameStatistic* Stats::GetFrame(size_t frame_number) {
  RTC_CHECK_LT(frame_number, stats_.size());
  return &stats_[frame_number];
}

FrameStatistic* Stats::GetFrameWithTimestamp(size_t timestamp) {
  RTC_DCHECK(rtp_timestamp_to_frame_num_.find(timestamp) !=
             rtp_timestamp_to_frame_num_.end());
  return GetFrame(rtp_timestamp_to_frame_num_[timestamp]);
}

size_t Stats::size() const {
  return stats_.size();
}

}  // namespace test
}  // namespace webrtc
