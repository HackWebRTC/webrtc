/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_RECEIVE_STREAM_H_
#define WEBRTC_AUDIO_RECEIVE_STREAM_H_

#include <map>
#include <string>
#include <vector>

#include "webrtc/config.h"
#include "webrtc/stream.h"
#include "webrtc/transport.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class AudioDecoder;

class AudioReceiveStream : public ReceiveStream {
 public:
  struct Stats {
    uint32_t remote_ssrc = 0;
    int64_t bytes_rcvd = 0;
    uint32_t packets_rcvd = 0;
    uint32_t packets_lost = 0;
    float fraction_lost = 0.0f;
    std::string codec_name;
    uint32_t ext_seqnum = 0;
    uint32_t jitter_ms = 0;
    uint32_t jitter_buffer_ms = 0;
    uint32_t jitter_buffer_preferred_ms = 0;
    uint32_t delay_estimate_ms = 0;
    int32_t audio_level = -1;
    float expand_rate = 0.0f;
    float speech_expand_rate = 0.0f;
    float secondary_decoded_rate = 0.0f;
    float accelerate_rate = 0.0f;
    float preemptive_expand_rate = 0.0f;
    int32_t decoding_calls_to_silence_generator = 0;
    int32_t decoding_calls_to_neteq = 0;
    int32_t decoding_normal = 0;
    int32_t decoding_plc = 0;
    int32_t decoding_cng = 0;
    int32_t decoding_plc_cng = 0;
    int64_t capture_start_ntp_time_ms = 0;
  };

  struct Config {
    std::string ToString() const;

    // Receive-stream specific RTP settings.
    struct Rtp {
      std::string ToString() const;

      // Synchronization source (stream identifier) to be received.
      uint32_t remote_ssrc = 0;

      // Sender SSRC used for sending RTCP (such as receiver reports).
      uint32_t local_ssrc = 0;

      // RTP header extensions used for the received stream.
      std::vector<RtpExtension> extensions;
    } rtp;

    Transport* receive_transport = nullptr;
    Transport* rtcp_send_transport = nullptr;

    // Underlying VoiceEngine handle, used to map AudioReceiveStream to lower-
    // level components.
    // TODO(solenberg): Remove when VoiceEngine channels are created outside
    // of Call.
    int voe_channel_id = -1;

    // Identifier for an A/V synchronization group. Empty string to disable.
    // TODO(pbos): Synchronize streams in a sync group, not just one video
    // stream to one audio stream. Tracked by issue webrtc:4762.
    std::string sync_group;

    // Decoders for every payload that we can receive. Call owns the
    // AudioDecoder instances once the Config is submitted to
    // Call::CreateReceiveStream().
    // TODO(solenberg): Use unique_ptr<> once our std lib fully supports C++11.
    std::map<uint8_t, AudioDecoder*> decoder_map;

    // TODO(pbos): Remove config option once combined A/V BWE is always on.
    bool combined_audio_video_bwe = false;
  };

  virtual Stats GetStats() const = 0;
};
}  // namespace webrtc

#endif  // WEBRTC_AUDIO_RECEIVE_STREAM_H_
