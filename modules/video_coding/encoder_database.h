/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_ENCODER_DATABASE_H_
#define MODULES_VIDEO_CODING_ENCODER_DATABASE_H_

#include <memory>

#include "modules/video_coding/generic_encoder.h"

namespace webrtc {

class VCMEncoderDataBase {
 public:
  explicit VCMEncoderDataBase(VCMEncodedFrameCallback* encoded_frame_callback);
  ~VCMEncoderDataBase();

  // Sets the sender side codec and initiates the desired codec given the
  // VideoCodec struct.
  // Returns true if the codec was successfully registered, false otherwise.
  bool SetSendCodec(const VideoCodec* send_codec,
                    int number_of_cores,
                    size_t max_payload_size);

  // Registers and initializes an external encoder object.
  // |internal_source| should be set to true if the codec has an internal
  // video source and doesn't need the user to provide it with frames via
  // the Encode() method.
  void RegisterExternalEncoder(VideoEncoder* external_encoder,
                               uint8_t payload_type,
                               bool internal_source);

  // Deregisters an external encoder. Returns true if the encoder was
  // found and deregistered, false otherwise. |was_send_codec| is set to true
  // if the external encoder was the send codec before being deregistered.
  bool DeregisterExternalEncoder(uint8_t payload_type, bool* was_send_codec);

  VCMGenericEncoder* GetEncoder();

  bool SetPeriodicKeyFrames(bool enable);

  bool MatchesCurrentResolution(int width, int height) const;

 private:
  // Determines whether a new codec has to be created or not.
  // Checks every setting apart from maxFramerate and startBitrate.
  bool RequiresEncoderReset(const VideoCodec& send_codec);

  void DeleteEncoder();

  int number_of_cores_;
  size_t max_payload_size_;
  bool periodic_key_frames_;
  bool pending_encoder_reset_;
  VideoCodec send_codec_;
  uint8_t encoder_payload_type_;
  VideoEncoder* external_encoder_;
  bool internal_source_;
  VCMEncodedFrameCallback* const encoded_frame_callback_;
  std::unique_ptr<VCMGenericEncoder> ptr_encoder_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_ENCODER_DATABASE_H_
