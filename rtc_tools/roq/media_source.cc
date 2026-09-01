/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/roq/media_source.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/error.h"
#include "libavutil/frame.h"
#include "libavutil/pixfmt.h"
#include "libavutil/samplefmt.h"
}

namespace webrtc::roq {
namespace {

constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

struct FormatContextDeleter {
  void operator()(AVFormatContext* context) const {
    avformat_close_input(&context);
  }
};

struct CodecContextDeleter {
  void operator()(AVCodecContext* context) const {
    avcodec_free_context(&context);
  }
};

struct FrameDeleter {
  void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};

struct PacketDeleter {
  void operator()(AVPacket* packet) const { av_packet_free(&packet); }
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;

class TimelineLog {
 public:
  TimelineLog(const std::string& path, std::string run_id)
      : output_(path.empty() ? std::ofstream() : std::ofstream(path)),
        run_id_(std::move(run_id)),
        start_(std::chrono::steady_clock::now()) {}

  bool requested() const { return output_.is_open(); }
  bool ok() const { return !requested() || output_.good(); }

  void Write(const char* event, int64_t media_pts_us) {
    if (!requested()) {
      return;
    }
    const int64_t actual_emit_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start_)
            .count();
    const int64_t monotonic_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    const int64_t utc_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    output_ << "{\"schema_version\":1,\"run_id\":\"" << run_id_
            << "\",\"process\":\"media-source\",\"event\":\"" << event
            << "\",\"sequence\":" << sequence_++
            << ",\"media_pts_us\":" << media_pts_us
            << ",\"target_emit_us\":" << media_pts_us
            << ",\"actual_emit_us\":" << actual_emit_us
            << ",\"monotonic_us\":" << monotonic_us << ",\"utc_ms\":" << utc_ms
            << "}\n";
  }

 private:
  std::ofstream output_;
  std::string run_id_;
  std::chrono::steady_clock::time_point start_;
  uint64_t sequence_ = 0;
};

void HashBytes(uint64_t* hash, const uint8_t* data, size_t size) {
  for (size_t index = 0; index < size; ++index) {
    *hash ^= data[index];
    *hash *= kFnvPrime;
  }
}

std::string AvError(int code) {
  std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer = {};
  av_strerror(code, buffer.data(), buffer.size());
  return buffer.data();
}

CodecContextPtr OpenDecoder(AVFormatContext* format,
                            AVMediaType media_type,
                            int* stream_index,
                            std::string* error) {
  const AVCodec* decoder = nullptr;
  const int index =
      av_find_best_stream(format, media_type, -1, -1, &decoder, 0);
  if (index < 0 || decoder == nullptr) {
    *error = "failed to find media stream: " + AvError(index);
    return nullptr;
  }

  CodecContextPtr context(avcodec_alloc_context3(decoder));
  if (!context) {
    *error = "failed to allocate decoder context";
    return nullptr;
  }
  int status = avcodec_parameters_to_context(context.get(),
                                             format->streams[index]->codecpar);
  if (status < 0) {
    *error = "failed to copy decoder parameters: " + AvError(status);
    return nullptr;
  }
  status = avcodec_open2(context.get(), decoder, nullptr);
  if (status < 0) {
    *error = "failed to open decoder: " + AvError(status);
    return nullptr;
  }
  *stream_index = index;
  return context;
}

int64_t FramePts(const AVFrame* frame) {
  return frame->best_effort_timestamp != AV_NOPTS_VALUE
             ? frame->best_effort_timestamp
             : frame->pts;
}

void PaceFrame(MediaClockMode clock_mode,
               int64_t pts_us,
               std::chrono::steady_clock::time_point start,
               int64_t* max_error_us) {
  if (clock_mode != MediaClockMode::kRealtime || pts_us < 0) {
    return;
  }
  const auto target = start + std::chrono::microseconds(pts_us);
  std::this_thread::sleep_until(target);
  const auto now = std::chrono::steady_clock::now();
  const int64_t error_us =
      std::chrono::duration_cast<std::chrono::microseconds>(now - target)
          .count();
  *max_error_us = std::max(*max_error_us, std::max<int64_t>(0, error_us));
}

bool ConsumeVideoFrame(const AVFrame* frame,
                       AVRational time_base,
                       int64_t duration_us,
                       MediaClockMode clock_mode,
                       std::chrono::steady_clock::time_point start,
                       MediaSourceResult* result,
                       TimelineLog* timeline) {
  const int64_t pts = FramePts(frame);
  if (pts == AV_NOPTS_VALUE) {
    result->error = "video frame is missing PTS";
    return false;
  }
  const int64_t pts_us = av_rescale_q(pts, time_base, AVRational{1, 1000000});
  if (pts_us >= duration_us) {
    return true;
  }
  if (frame->format != AV_PIX_FMT_YUV420P) {
    result->error = "video decoder did not produce yuv420p";
    return false;
  }

  PaceFrame(clock_mode, pts_us, start, &result->max_pacing_error_us);
  timeline->Write("video_frame", pts_us);
  HashBytes(&result->video_pts_hash, reinterpret_cast<const uint8_t*>(&pts),
            sizeof(pts));
  result->video_width = frame->width;
  result->video_height = frame->height;
  ++result->video_frames;
  const std::array<int, 3> widths = {frame->width, (frame->width + 1) / 2,
                                     (frame->width + 1) / 2};
  const std::array<int, 3> heights = {frame->height, (frame->height + 1) / 2,
                                      (frame->height + 1) / 2};
  for (size_t plane = 0; plane < widths.size(); ++plane) {
    for (int row = 0; row < heights[plane]; ++row) {
      HashBytes(&result->video_hash,
                frame->data[plane] + row * frame->linesize[plane],
                static_cast<size_t>(widths[plane]));
    }
  }
  return true;
}

bool ConsumeAudioFrame(const AVFrame* frame,
                       AVRational time_base,
                       int64_t duration_us,
                       MediaClockMode clock_mode,
                       std::chrono::steady_clock::time_point start,
                       MediaSourceResult* result,
                       TimelineLog* timeline) {
  if (frame->format != AV_SAMPLE_FMT_FLTP || frame->sample_rate != 48000 ||
      frame->ch_layout.nb_channels != 2) {
    result->error = "audio decoder must produce stereo 48 kHz fltp";
    return false;
  }
  const int64_t pts = FramePts(frame);
  if (pts == AV_NOPTS_VALUE) {
    result->error = "audio frame is missing PTS";
    return false;
  }
  const int64_t frame_start =
      av_rescale_q(pts, time_base, AVRational{1, frame->sample_rate});
  const int64_t duration_samples =
      static_cast<int64_t>(std::llround(duration_us * 0.048));
  if (frame_start >= duration_samples) {
    return true;
  }

  const int64_t pts_us =
      frame_start * 1000000 / static_cast<int64_t>(frame->sample_rate);
  PaceFrame(clock_mode, pts_us, start, &result->max_pacing_error_us);
  const int samples_to_copy = static_cast<int>(
      std::min<int64_t>(frame->nb_samples, duration_samples - frame_start));
  HashBytes(&result->audio_pts_hash, reinterpret_cast<const uint8_t*>(&pts),
            sizeof(pts));
  for (int sample = 0; sample < samples_to_copy; ++sample) {
    for (int channel = 0; channel < 2; ++channel) {
      const float* plane =
          reinterpret_cast<const float*>(frame->extended_data[channel]);
      const float scaled = std::clamp(plane[sample], -1.0f, 1.0f) * 32767.0f;
      const int16_t converted = static_cast<int16_t>(std::lrint(scaled));
      HashBytes(&result->audio_hash,
                reinterpret_cast<const uint8_t*>(&converted),
                sizeof(converted));
    }
    ++result->audio_block_fill;
    if (result->audio_block_fill == 480) {
      const int64_t block_pts_us = result->audio_blocks * 10000;
      timeline->Write("audio_block", block_pts_us);
      ++result->audio_blocks;
      result->audio_block_fill = 0;
    }
  }
  result->audio_samples_per_channel += samples_to_copy;
  return true;
}

bool DrainDecoder(AVCodecContext* decoder,
                  AVStream* stream,
                  bool is_video,
                  int64_t duration_us,
                  MediaClockMode clock_mode,
                  std::chrono::steady_clock::time_point start,
                  AVFrame* frame,
                  MediaSourceResult* result,
                  TimelineLog* timeline) {
  while (true) {
    const int status = avcodec_receive_frame(decoder, frame);
    if (status == AVERROR(EAGAIN) || status == AVERROR_EOF) {
      return true;
    }
    if (status < 0) {
      result->error = "failed to decode frame: " + AvError(status);
      return false;
    }
    const bool ok =
        is_video ? ConsumeVideoFrame(frame, stream->time_base, duration_us,
                                     clock_mode, start, result, timeline)
                 : ConsumeAudioFrame(frame, stream->time_base, duration_us,
                                     clock_mode, start, result, timeline);
    av_frame_unref(frame);
    if (!ok) {
      return false;
    }
  }
}

}  // namespace

MediaSourceResult RunMediaSource(const std::string& input_path,
                                 double duration_seconds,
                                 MediaClockMode clock_mode,
                                 const std::string& run_id,
                                 const std::string& event_log_path) {
  MediaSourceResult result;
  result.video_hash = kFnvOffsetBasis;
  result.audio_hash = kFnvOffsetBasis;
  result.video_pts_hash = kFnvOffsetBasis;
  result.audio_pts_hash = kFnvOffsetBasis;
  TimelineLog timeline(event_log_path, run_id);
  if (!timeline.ok()) {
    result.error = "failed to open media event log";
    return result;
  }
  if (duration_seconds <= 0 ||
      duration_seconds >
          static_cast<double>(std::numeric_limits<int64_t>::max()) / 1000000) {
    result.error = "duration must be positive";
    return result;
  }
  const int64_t duration_us =
      static_cast<int64_t>(std::llround(duration_seconds * 1000000.0));

  AVFormatContext* raw_format = nullptr;
  int status =
      avformat_open_input(&raw_format, input_path.c_str(), nullptr, nullptr);
  FormatContextPtr format(raw_format);
  if (status < 0) {
    result.error = "failed to open input: " + AvError(status);
    return result;
  }
  status = avformat_find_stream_info(format.get(), nullptr);
  if (status < 0) {
    result.error = "failed to read stream information: " + AvError(status);
    return result;
  }

  int video_stream_index = -1;
  int audio_stream_index = -1;
  CodecContextPtr video_decoder = OpenDecoder(
      format.get(), AVMEDIA_TYPE_VIDEO, &video_stream_index, &result.error);
  if (!video_decoder) {
    return result;
  }
  CodecContextPtr audio_decoder = OpenDecoder(
      format.get(), AVMEDIA_TYPE_AUDIO, &audio_stream_index, &result.error);
  if (!audio_decoder) {
    return result;
  }

  FramePtr frame(av_frame_alloc());
  PacketPtr packet(av_packet_alloc());
  if (!frame || !packet) {
    result.error = "failed to allocate FFmpeg frame or packet";
    return result;
  }

  const auto start = std::chrono::steady_clock::now();
  while ((status = av_read_frame(format.get(), packet.get())) >= 0) {
    AVCodecContext* decoder = nullptr;
    AVStream* stream = nullptr;
    bool is_video = false;
    if (packet->stream_index == video_stream_index) {
      decoder = video_decoder.get();
      stream = format->streams[video_stream_index];
      is_video = true;
    } else if (packet->stream_index == audio_stream_index) {
      decoder = audio_decoder.get();
      stream = format->streams[audio_stream_index];
    }
    if (decoder != nullptr) {
      status = avcodec_send_packet(decoder, packet.get());
      if (status < 0) {
        result.error = "failed to submit packet: " + AvError(status);
        return result;
      }
      if (!DrainDecoder(decoder, stream, is_video, duration_us, clock_mode,
                        start, frame.get(), &result, &timeline)) {
        return result;
      }
    }
    if (result.video_frames > 0 &&
        result.audio_samples_per_channel >= duration_us * 48 / 1000 &&
        packet->stream_index == video_stream_index &&
        packet->pts != AV_NOPTS_VALUE &&
        av_rescale_q(packet->pts,
                     format->streams[video_stream_index]->time_base,
                     AVRational{1, 1000000}) >= duration_us) {
      av_packet_unref(packet.get());
      break;
    }
    av_packet_unref(packet.get());
  }

  if (result.video_frames == 0 || result.audio_samples_per_channel == 0) {
    result.error = "input did not produce both audio and video";
  }
  result.audio_sample_rate_hz = audio_decoder->sample_rate;
  result.audio_channels = audio_decoder->ch_layout.nb_channels;
  return result;
}

std::string HexHash(uint64_t value) {
  std::ostringstream output;
  output << std::hex << std::setw(16) << std::setfill('0') << value;
  return output.str();
}

}  // namespace webrtc::roq
