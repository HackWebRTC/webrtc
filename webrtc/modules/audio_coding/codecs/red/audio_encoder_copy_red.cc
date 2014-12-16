/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/red/audio_encoder_copy_red.h"

#include <string.h>

namespace webrtc {

AudioEncoderCopyRed::AudioEncoderCopyRed(const Config& config)
    : speech_encoder_(config.speech_encoder),
      red_payload_type_(config.payload_type),
      secondary_allocated_(0) {
  CHECK(speech_encoder_) << "Speech encoder not provided.";
}

AudioEncoderCopyRed::~AudioEncoderCopyRed() {
}

int AudioEncoderCopyRed::sample_rate_hz() const {
  return speech_encoder_->sample_rate_hz();
}

int AudioEncoderCopyRed::num_channels() const {
  return speech_encoder_->num_channels();
}

int AudioEncoderCopyRed::Num10MsFramesInNextPacket() const {
  return speech_encoder_->Num10MsFramesInNextPacket();
}

int AudioEncoderCopyRed::Max10MsFramesInAPacket() const {
  return speech_encoder_->Max10MsFramesInAPacket();
}

bool AudioEncoderCopyRed::EncodeInternal(uint32_t timestamp,
                                         const int16_t* audio,
                                         size_t max_encoded_bytes,
                                         uint8_t* encoded,
                                         EncodedInfo* info) {
  if (!speech_encoder_->Encode(timestamp, audio,
                               static_cast<size_t>(sample_rate_hz() / 100),
                               max_encoded_bytes, encoded, info))
    return false;
  if (max_encoded_bytes < info->encoded_bytes + secondary_info_.encoded_bytes)
    return false;
  CHECK(info->redundant.empty()) << "Cannot use nested redundant encoders.";

  if (info->encoded_bytes > 0) {
    // |info| will be implicitly cast to an EncodedInfoLeaf struct, effectively
    // discarding the (empty) vector of redundant information. This is
    // intentional.
    info->redundant.push_back(*info);
    DCHECK_EQ(info->redundant.size(), 1u);
    if (secondary_info_.encoded_bytes > 0) {
      memcpy(&encoded[info->encoded_bytes], secondary_encoded_.get(),
             secondary_info_.encoded_bytes);
      info->redundant.push_back(secondary_info_);
      DCHECK_EQ(info->redundant.size(), 2u);
    }
    // Save primary to secondary.
    if (secondary_allocated_ < info->encoded_bytes) {
      secondary_encoded_.reset(new uint8_t[info->encoded_bytes]);
      secondary_allocated_ = info->encoded_bytes;
    }
    CHECK(secondary_encoded_);
    memcpy(secondary_encoded_.get(), encoded, info->encoded_bytes);
    secondary_info_ = *info;
  }
  // Update main EncodedInfo.
  info->payload_type = red_payload_type_;
  info->encoded_bytes = 0;
  for (std::vector<EncodedInfoLeaf>::const_iterator it =
           info->redundant.begin();
       it != info->redundant.end(); ++it) {
    info->encoded_bytes += it->encoded_bytes;
  }
  return true;
}

}  // namespace webrtc
