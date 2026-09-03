/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "api/file_capturer/file_media_capturer.h"
#include "api/make_ref_counted.h"
#include "api/video/i420_buffer.h"
#include "modules/file_capturer/file_media_capturer_internal.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/error.h"
#include "libavutil/frame.h"
#include "libavutil/mathematics.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
}

namespace webrtc {
namespace {

constexpr int kOutputSampleRate = 48000;
constexpr size_t kOutputChannels = 2;
constexpr int kSamplesPerBlock = 480;
constexpr int64_t kAudioBlockUs = 10000;
constexpr int64_t kLookaheadUs = 2000000;
constexpr int64_t kMaximumLatenessUs = 2000000;

struct FormatContextDeleter {
  void operator()(AVFormatContext* value) const {
    avformat_close_input(&value);
  }
};
struct CodecContextDeleter {
  void operator()(AVCodecContext* value) const { avcodec_free_context(&value); }
};
struct FrameDeleter {
  void operator()(AVFrame* value) const { av_frame_free(&value); }
};
struct PacketDeleter {
  void operator()(AVPacket* value) const { av_packet_free(&value); }
};
struct SwrContextDeleter {
  void operator()(SwrContext* value) const { swr_free(&value); }
};
struct SwsContextDeleter {
  void operator()(SwsContext* value) const { sws_freeContext(value); }
};
using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;

struct QueuedPacket {
  PacketPtr packet;
  int64_t pts_us = 0;
  size_t size = 0;
};

struct AudioState {
  SwrContextPtr resampler;
  std::vector<int16_t> pending_samples;
  int64_t pending_start_sample = 0;
  int64_t next_input_end_us = std::numeric_limits<int64_t>::min();
};

struct CycleState {
  std::deque<QueuedPacket> audio_packets;
  std::deque<QueuedPacket> video_packets;
  size_t packet_bytes = 0;
  int64_t scan_frontier_us = 0;
  bool input_eof = false;
  bool audio_drain_sent = false;
  bool video_drain_sent = false;
  bool audio_eof = false;
  bool video_eof = false;
  int64_t last_video_pts_us = -1;
  AudioState audio;
  std::optional<FileMediaAudioBlock> next_audio;
  std::optional<VideoFrame> next_video;
};

std::string AvError(int code) {
  std::array<char, AV_ERROR_MAX_STRING_SIZE> text = {};
  av_strerror(code, text.data(), text.size());
  return text.data();
}

FileMediaError Error(FileMediaErrorCode code, std::string message) {
  return {.code = code, .message = std::move(message)};
}

int64_t ToMicros(int64_t value, AVRational time_base) {
  return av_rescale_q(value, time_base, AVRational{1, 1000000});
}

bool IsSupportedAudioFormat(AVSampleFormat format) {
  return format == AV_SAMPLE_FMT_FLT || format == AV_SAMPLE_FMT_FLTP ||
         format == AV_SAMPLE_FMT_S16 || format == AV_SAMPLE_FMT_S16P ||
         format == AV_SAMPLE_FMT_S32 || format == AV_SAMPLE_FMT_S32P;
}

bool IsSupportedVideoFormat(AVPixelFormat format) {
  return format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUVJ420P ||
         format == AV_PIX_FMT_NV12;
}

int SelectStream(AVFormatContext* format,
                 AVMediaType type,
                 const std::optional<int>& requested,
                 FileMediaError* error) {
  int index = -1;
  if (requested) {
    if (*requested < 0 ||
        static_cast<unsigned>(*requested) >= format->nb_streams ||
        format->streams[*requested]->codecpar->codec_type != type) {
      *error = Error(FileMediaErrorCode::kUnsupportedCodec,
                     "requested stream has the wrong media type");
      return -2;
    }
    index = *requested;
  } else {
    index = av_find_best_stream(format, type, -1, -1, nullptr, 0);
    if (index == AVERROR_STREAM_NOT_FOUND)
      return -1;
    if (index < 0) {
      *error = Error(FileMediaErrorCode::kUnsupportedCodec,
                     "failed to select media stream: " + AvError(index));
      return -2;
    }
  }
  const AVCodecID expected =
      type == AVMEDIA_TYPE_AUDIO ? AV_CODEC_ID_AAC : AV_CODEC_ID_H264;
  if (format->streams[index]->codecpar->codec_id != expected) {
    *error = Error(FileMediaErrorCode::kUnsupportedCodec,
                   "selected stream has an unsupported codec");
    return -2;
  }
  return index;
}

FileMediaTrackInfo TrackInfo(AVStream* stream) {
  const int64_t start =
      stream->start_time == AV_NOPTS_VALUE ? 0 : stream->start_time;
  return {
      .stream_index = stream->index,
      .start_time_us = ToMicros(start, stream->time_base),
      .duration_us = stream->duration == AV_NOPTS_VALUE
                         ? 0
                         : ToMicros(stream->duration, stream->time_base),
      .width = stream->codecpar->width,
      .height = stream->codecpar->height,
      .sample_rate_hz = stream->codecpar->sample_rate,
      .channels = static_cast<size_t>(stream->codecpar->ch_layout.nb_channels)};
}

CodecContextPtr OpenDecoder(AVFormatContext* format,
                            int index,
                            FileMediaError* error) {
  if (index < 0)
    return nullptr;
  const AVCodec* decoder =
      avcodec_find_decoder(format->streams[index]->codecpar->codec_id);
  CodecContextPtr context(decoder ? avcodec_alloc_context3(decoder) : nullptr);
  if (!context ||
      avcodec_parameters_to_context(context.get(),
                                    format->streams[index]->codecpar) < 0 ||
      avcodec_open2(context.get(), decoder, nullptr) < 0) {
    *error = Error(FileMediaErrorCode::kUnsupportedCodec,
                   "failed to open selected decoder");
    return nullptr;
  }
  return context;
}

bool Rewind(AVFormatContext* format, int64_t common_start_us) {
  return avformat_seek_file(format, -1, std::numeric_limits<int64_t>::min(),
                            common_start_us, common_start_us,
                            AVSEEK_FLAG_BACKWARD) >= 0;
}

bool ProbeFirstFrame(AVFormatContext* format,
                     AVCodecContext* decoder,
                     int stream_index,
                     bool video,
                     FileMediaError* error) {
  PacketPtr packet(av_packet_alloc());
  FramePtr frame(av_frame_alloc());
  if (!packet || !frame) {
    *error =
        Error(FileMediaErrorCode::kInternal, "failed to allocate format probe");
    return false;
  }
  int status = 0;
  while ((status = av_read_frame(format, packet.get())) >= 0) {
    if (packet->stream_index != stream_index) {
      av_packet_unref(packet.get());
      continue;
    }
    status = avcodec_send_packet(decoder, packet.get());
    av_packet_unref(packet.get());
    if (status < 0)
      break;
    status = avcodec_receive_frame(decoder, frame.get());
    if (status == AVERROR(EAGAIN))
      continue;
    if (status < 0)
      break;
    const bool supported =
        video
            ? IsSupportedVideoFormat(static_cast<AVPixelFormat>(frame->format))
            : frame->sample_rate > 0 &&
                  (frame->ch_layout.nb_channels == 1 ||
                   frame->ch_layout.nb_channels == 2) &&
                  IsSupportedAudioFormat(
                      static_cast<AVSampleFormat>(frame->format));
    if (!supported) {
      *error = Error(FileMediaErrorCode::kUnsupportedFormat,
                     "selected stream decodes to an unsupported format");
    }
    return supported;
  }
  *error = Error(FileMediaErrorCode::kDecodeFailure,
                 "failed to decode first selected frame");
  return false;
}

class FfmpegFileMediaCapturer final : public FileMediaCapturer {
 public:
  FfmpegFileMediaCapturer(FileMediaCapturerConfig config,
                          FileMediaCapturerObserver* observer,
                          FileMediaInfo info,
                          int audio_index,
                          int video_index,
                          int64_t common_start_us)
      : config_(std::move(config)),
        observer_(observer),
        info_(std::move(info)),
        audio_index_(audio_index),
        video_index_(video_index),
        common_start_us_(common_start_us) {}

  ~FfmpegFileMediaCapturer() override { Stop(); }

  FileMediaError Start() override {
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true)) {
      return Error(FileMediaErrorCode::kInvalidState,
                   "file media capturer already started");
    }
    worker_ = std::thread([this] { Run(); });
    return {};
  }

  void Stop() override {
    stop_.store(true);
    wait_condition_.notify_all();
    if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id()) {
      std::lock_guard lock(join_mutex_);
      if (worker_.joinable())
        worker_.join();
    }
  }

  const FileMediaInfo& media_info() const override { return info_; }

 private:
  bool Pace(int64_t timestamp_us,
            std::chrono::steady_clock::time_point playback_start,
            FileMediaError* error) {
    const auto target =
        playback_start + std::chrono::microseconds(timestamp_us);
    std::unique_lock lock(wait_mutex_);
    wait_condition_.wait_until(lock, target, [this] { return stop_.load(); });
    if (stop_.load())
      return false;
    const int64_t lateness_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - target)
            .count();
    if (lateness_us > kMaximumLatenessUs) {
      *error = Error(FileMediaErrorCode::kConsumerTooSlow,
                     "media observer is more than two seconds behind");
      return false;
    }
    return true;
  }

  void Finish(FileMediaTerminal terminal) {
    bool expected = false;
    if (terminal_sent_.compare_exchange_strong(expected, true))
      observer_->OnTerminal(std::move(terminal));
  }

  bool FillLookahead(AVFormatContext* format,
                     CycleState* cycle,
                     int64_t playback_position_us,
                     FileMediaError* error) {
    while (!cycle->input_eof &&
           cycle->scan_frontier_us < playback_position_us + kLookaheadUs) {
      PacketPtr packet(av_packet_alloc());
      if (!packet) {
        *error = Error(FileMediaErrorCode::kInternal,
                       "failed to allocate compressed packet");
        return false;
      }
      const int status = av_read_frame(format, packet.get());
      if (status == AVERROR_EOF) {
        cycle->input_eof = true;
        break;
      }
      if (status < 0) {
        *error = Error(FileMediaErrorCode::kDecodeFailure,
                       "failed to read compressed packet: " + AvError(status));
        return false;
      }
      if (packet->stream_index != audio_index_ &&
          packet->stream_index != video_index_)
        continue;
      if (packet->pts == AV_NOPTS_VALUE && packet->dts == AV_NOPTS_VALUE) {
        *error = Error(FileMediaErrorCode::kInvalidTiming,
                       "selected packet is missing PTS and DTS");
        return false;
      }
      const AVStream* stream = format->streams[packet->stream_index];
      const int64_t packet_time =
          packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
      const int64_t packet_pts_us =
          ToMicros(packet_time, stream->time_base) - common_start_us_;
      cycle->scan_frontier_us =
          std::max(cycle->scan_frontier_us, packet_pts_us);
      const size_t packet_size = static_cast<size_t>(std::max(packet->size, 0));
      if (file_capturer_internal::ExceedsPacketBudget(cycle->packet_bytes,
                                                      packet_size)) {
        *error = Error(FileMediaErrorCode::kResourceExhausted,
                       "compressed packet lookahead exceeds 64 MiB");
        return false;
      }
      cycle->packet_bytes += packet_size;
      QueuedPacket queued = {.packet = std::move(packet),
                             .pts_us = packet_pts_us,
                             .size = packet_size};
      if (queued.packet->stream_index == audio_index_)
        cycle->audio_packets.push_back(std::move(queued));
      else
        cycle->video_packets.push_back(std::move(queued));
    }
    return true;
  }

  bool ConvertVideoFrame(const AVFrame* frame,
                         const AVStream* stream,
                         int64_t cycle_base_us,
                         CycleState* cycle,
                         SwsContextPtr* scaler,
                         FileMediaError* error) {
    const int64_t raw_pts = frame->best_effort_timestamp != AV_NOPTS_VALUE
                                ? frame->best_effort_timestamp
                                : frame->pts;
    if (raw_pts == AV_NOPTS_VALUE) {
      *error = Error(FileMediaErrorCode::kInvalidTiming,
                     "decoded video frame is missing PTS");
      return false;
    }
    const int64_t cycle_pts_us =
        ToMicros(raw_pts, stream->time_base) - common_start_us_;
    if (cycle_pts_us < 0 || cycle_pts_us >= info_.loop_duration_us)
      return true;
    if (cycle->last_video_pts_us >= cycle_pts_us) {
      *error = Error(FileMediaErrorCode::kInvalidTiming,
                     "decoded video PTS is not strictly increasing");
      return false;
    }
    cycle->last_video_pts_us = cycle_pts_us;
    const AVPixelFormat format = static_cast<AVPixelFormat>(frame->format);
    if (!IsSupportedVideoFormat(format)) {
      *error = Error(FileMediaErrorCode::kUnsupportedFormat,
                     "video pixel format changed during capture");
      return false;
    }
    scoped_refptr<I420Buffer> buffer =
        I420Buffer::Create(frame->width, frame->height);
    SwsContext* raw_scaler = sws_getCachedContext(
        scaler->release(), frame->width, frame->height, format, frame->width,
        frame->height, AV_PIX_FMT_YUV420P, SWS_POINT, nullptr, nullptr,
        nullptr);
    if (!raw_scaler) {
      *error = Error(FileMediaErrorCode::kInternal,
                     "failed to initialize video conversion");
      return false;
    }
    scaler->reset(raw_scaler);
    uint8_t* output[] = {buffer->MutableDataY(), buffer->MutableDataU(),
                         buffer->MutableDataV(), nullptr};
    const int strides[] = {buffer->StrideY(), buffer->StrideU(),
                           buffer->StrideV(), 0};
    if (sws_scale(scaler->get(), frame->data, frame->linesize, 0, frame->height,
                  output, strides) != frame->height) {
      *error = Error(FileMediaErrorCode::kInternal,
                     "failed to convert video frame to I420");
      return false;
    }
    cycle->next_video = VideoFrame::Builder()
                            .set_video_frame_buffer(std::move(buffer))
                            .set_timestamp_us(cycle_base_us + cycle_pts_us)
                            .build();
    return true;
  }

  bool ConvertAudioFrame(const AVFrame* frame,
                         const AVStream* stream,
                         CycleState* cycle,
                         FileMediaError* error) {
    const AVSampleFormat format = static_cast<AVSampleFormat>(frame->format);
    if (frame->sample_rate <= 0 ||
        (frame->ch_layout.nb_channels != 1 &&
         frame->ch_layout.nb_channels != 2) ||
        !IsSupportedAudioFormat(format)) {
      *error = Error(FileMediaErrorCode::kUnsupportedFormat,
                     "audio format changed during capture");
      return false;
    }
    const int64_t raw_pts = frame->best_effort_timestamp != AV_NOPTS_VALUE
                                ? frame->best_effort_timestamp
                                : frame->pts;
    if (raw_pts == AV_NOPTS_VALUE) {
      *error = Error(FileMediaErrorCode::kInvalidTiming,
                     "decoded audio frame is missing PTS");
      return false;
    }
    const int64_t input_start_us = ToMicros(raw_pts, stream->time_base);
    const int64_t input_duration_us =
        av_rescale_q(frame->nb_samples, AVRational{1, frame->sample_rate},
                     AVRational{1, 1000000});
    const int64_t tolerance_us = 1000000 / frame->sample_rate + 1;
    if (cycle->audio.next_input_end_us != std::numeric_limits<int64_t>::min() &&
        input_start_us + tolerance_us < cycle->audio.next_input_end_us) {
      *error = Error(FileMediaErrorCode::kInvalidTiming,
                     "decoded audio PTS overlaps or moves backwards");
      return false;
    }
    if (cycle->audio.next_input_end_us != std::numeric_limits<int64_t>::min() &&
        input_start_us > cycle->audio.next_input_end_us + tolerance_us) {
      cycle->audio.pending_samples.clear();
      cycle->audio.resampler.reset();
    }
    cycle->audio.next_input_end_us = input_start_us + input_duration_us;
    if (!cycle->audio.resampler) {
      SwrContext* raw_resampler = nullptr;
      AVChannelLayout output_layout = AV_CHANNEL_LAYOUT_STEREO;
      if (swr_alloc_set_opts2(&raw_resampler, &output_layout, AV_SAMPLE_FMT_S16,
                              kOutputSampleRate, &frame->ch_layout, format,
                              frame->sample_rate, 0, nullptr) < 0 ||
          swr_init(raw_resampler) < 0) {
        swr_free(&raw_resampler);
        *error = Error(FileMediaErrorCode::kUnsupportedFormat,
                       "failed to initialize audio conversion");
        return false;
      }
      cycle->audio.resampler.reset(raw_resampler);
    }
    const int maximum_samples = static_cast<int>(av_rescale_rnd(
        swr_get_delay(cycle->audio.resampler.get(), frame->sample_rate) +
            frame->nb_samples,
        kOutputSampleRate, frame->sample_rate, AV_ROUND_UP));
    std::vector<int16_t> converted(static_cast<size_t>(maximum_samples) *
                                   kOutputChannels);
    uint8_t* output[] = {reinterpret_cast<uint8_t*>(converted.data())};
    const int count = swr_convert(
        cycle->audio.resampler.get(), output, maximum_samples,
        const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples);
    if (count < 0) {
      *error = Error(FileMediaErrorCode::kDecodeFailure,
                     "failed to convert decoded audio");
      return false;
    }
    converted.resize(static_cast<size_t>(count) * kOutputChannels);
    int64_t start_sample =
        av_rescale_q(input_start_us - common_start_us_, AVRational{1, 1000000},
                     AVRational{1, kOutputSampleRate});
    if (start_sample < 0) {
      const size_t skip =
          static_cast<size_t>(std::min<int64_t>(-start_sample, count));
      converted.erase(converted.begin(),
                      converted.begin() + skip * kOutputChannels);
      start_sample += skip;
    }
    if (cycle->audio.pending_samples.empty())
      cycle->audio.pending_start_sample = start_sample;
    cycle->audio.pending_samples.insert(cycle->audio.pending_samples.end(),
                                        converted.begin(), converted.end());
    return true;
  }

  bool FillNextVideo(AVFormatContext* format,
                     AVCodecContext* decoder,
                     int64_t cycle_base_us,
                     int64_t playback_position_us,
                     CycleState* cycle,
                     SwsContextPtr* scaler,
                     AVFrame* frame,
                     FileMediaError* error) {
    if (!decoder || cycle->next_video || cycle->video_eof)
      return true;
    while (!cycle->next_video && !cycle->video_eof) {
      const int receive = avcodec_receive_frame(decoder, frame);
      if (receive >= 0) {
        const bool ok = ConvertVideoFrame(frame, format->streams[video_index_],
                                          cycle_base_us, cycle, scaler, error);
        av_frame_unref(frame);
        if (!ok)
          return false;
        continue;
      }
      if (receive == AVERROR_EOF) {
        cycle->video_eof = true;
        break;
      }
      if (receive != AVERROR(EAGAIN)) {
        *error = Error(FileMediaErrorCode::kDecodeFailure,
                       "failed to receive video frame: " + AvError(receive));
        return false;
      }
      if (cycle->video_packets.empty() &&
          !FillLookahead(format, cycle, playback_position_us, error))
        return false;
      if (cycle->video_packets.empty()) {
        if (cycle->input_eof && !cycle->video_drain_sent) {
          if (avcodec_send_packet(decoder, nullptr) < 0)
            return false;
          cycle->video_drain_sent = true;
          continue;
        }
        break;
      }
      QueuedPacket queued = std::move(cycle->video_packets.front());
      cycle->video_packets.pop_front();
      cycle->packet_bytes -= queued.size;
      if (queued.pts_us >= info_.loop_duration_us) {
        cycle->video_eof = true;
        break;
      }
      if (avcodec_send_packet(decoder, queued.packet.get()) < 0) {
        *error = Error(FileMediaErrorCode::kDecodeFailure,
                       "failed to submit video packet");
        return false;
      }
    }
    return true;
  }

  bool MakeNextAudioBlock(int64_t cycle_base_us, CycleState* cycle) {
    if (cycle->next_audio || cycle->audio.pending_samples.size() <
                                 kSamplesPerBlock * kOutputChannels)
      return false;
    const int64_t block_pts_us =
        av_rescale_q(cycle->audio.pending_start_sample,
                     AVRational{1, kOutputSampleRate}, AVRational{1, 1000000});
    if (block_pts_us < 0) {
      cycle->audio.pending_samples.erase(
          cycle->audio.pending_samples.begin(),
          cycle->audio.pending_samples.begin() +
              kSamplesPerBlock * kOutputChannels);
      cycle->audio.pending_start_sample += kSamplesPerBlock;
      return false;
    }
    if (block_pts_us + kAudioBlockUs > info_.loop_duration_us) {
      cycle->audio.pending_samples.clear();
      cycle->audio_eof = true;
      return false;
    }
    std::vector<int16_t> samples(cycle->audio.pending_samples.begin(),
                                 cycle->audio.pending_samples.begin() +
                                     kSamplesPerBlock * kOutputChannels);
    cycle->audio.pending_samples.erase(cycle->audio.pending_samples.begin(),
                                       cycle->audio.pending_samples.begin() +
                                           kSamplesPerBlock * kOutputChannels);
    cycle->audio.pending_start_sample += kSamplesPerBlock;
    cycle->next_audio = {.samples = std::move(samples),
                         .sample_rate_hz = kOutputSampleRate,
                         .channels = kOutputChannels,
                         .timestamp_us = cycle_base_us + block_pts_us};
    return true;
  }

  bool FillNextAudio(AVFormatContext* format,
                     AVCodecContext* decoder,
                     int64_t cycle_base_us,
                     int64_t playback_position_us,
                     CycleState* cycle,
                     AVFrame* frame,
                     FileMediaError* error) {
    if (!decoder || cycle->next_audio || cycle->audio_eof)
      return true;
    while (!cycle->next_audio && !cycle->audio_eof) {
      if (MakeNextAudioBlock(cycle_base_us, cycle))
        break;
      const int receive = avcodec_receive_frame(decoder, frame);
      if (receive >= 0) {
        const bool ok = ConvertAudioFrame(frame, format->streams[audio_index_],
                                          cycle, error);
        av_frame_unref(frame);
        if (!ok)
          return false;
        continue;
      }
      if (receive == AVERROR_EOF) {
        cycle->audio.pending_samples.clear();
        cycle->audio_eof = true;
        break;
      }
      if (receive != AVERROR(EAGAIN)) {
        *error = Error(FileMediaErrorCode::kDecodeFailure,
                       "failed to receive audio frame: " + AvError(receive));
        return false;
      }
      if (cycle->audio_packets.empty() &&
          !FillLookahead(format, cycle, playback_position_us, error))
        return false;
      if (cycle->audio_packets.empty()) {
        if (cycle->input_eof && !cycle->audio_drain_sent) {
          if (avcodec_send_packet(decoder, nullptr) < 0)
            return false;
          cycle->audio_drain_sent = true;
          continue;
        }
        break;
      }
      QueuedPacket queued = std::move(cycle->audio_packets.front());
      cycle->audio_packets.pop_front();
      cycle->packet_bytes -= queued.size;
      if (queued.pts_us >= info_.loop_duration_us) {
        cycle->audio.pending_samples.clear();
        cycle->audio_eof = true;
        break;
      }
      if (avcodec_send_packet(decoder, queued.packet.get()) < 0) {
        *error = Error(FileMediaErrorCode::kDecodeFailure,
                       "failed to submit audio packet");
        return false;
      }
    }
    return true;
  }

  bool RunCycle(AVFormatContext* format,
                AVCodecContext* audio,
                AVCodecContext* video,
                size_t cycle_index,
                std::chrono::steady_clock::time_point* playback_start,
                FileMediaError* error) {
    CycleState cycle;
    FramePtr audio_frame(av_frame_alloc());
    FramePtr video_frame(av_frame_alloc());
    SwsContextPtr scaler;
    if (!audio_frame || !video_frame) {
      *error = Error(FileMediaErrorCode::kInternal,
                     "failed to allocate decoded frames");
      return false;
    }
    if (!FillLookahead(format, &cycle, 0, error))
      return false;
    if (cycle_index == 0)
      *playback_start = std::chrono::steady_clock::now();
    const int64_t cycle_base_us =
        static_cast<int64_t>(cycle_index) * info_.loop_duration_us;
    int64_t playback_position_us = 0;
    while (!stop_.load()) {
      if (!FillNextAudio(format, audio, cycle_base_us, playback_position_us,
                         &cycle, audio_frame.get(), error) ||
          !FillNextVideo(format, video, cycle_base_us, playback_position_us,
                         &cycle, &scaler, video_frame.get(), error))
        return false;
      if (!cycle.next_audio && !cycle.next_video) {
        if ((audio_index_ < 0 || cycle.audio_eof) &&
            (video_index_ < 0 || cycle.video_eof))
          break;
        if (!FillLookahead(format, &cycle, playback_position_us, error))
          return false;
        continue;
      }
      const bool emit_audio =
          cycle.next_audio &&
          (!cycle.next_video ||
           cycle.next_audio->timestamp_us <= cycle.next_video->timestamp_us());
      const int64_t timestamp_us = emit_audio
                                       ? cycle.next_audio->timestamp_us
                                       : cycle.next_video->timestamp_us();
      if (!Pace(timestamp_us, *playback_start, error))
        return stop_.load();
      playback_position_us = timestamp_us - cycle_base_us;
      if (emit_audio) {
        observer_->OnAudioBlock(std::move(*cycle.next_audio));
        cycle.next_audio.reset();
      } else {
        observer_->OnVideoFrame(std::move(*cycle.next_video));
        cycle.next_video.reset();
      }
    }
    return true;
  }

  void Run() {
    AVFormatContext* raw_format = nullptr;
    int status = avformat_open_input(&raw_format, config_.input_path.c_str(),
                                     nullptr, nullptr);
    FormatContextPtr format(raw_format);
    if (status < 0 || avformat_find_stream_info(format.get(), nullptr) < 0) {
      Finish({.reason = FileMediaTerminalReason::kError,
              .error = Error(FileMediaErrorCode::kDecodeFailure,
                             "failed to reopen input")});
      return;
    }
    FileMediaError error;
    CodecContextPtr audio = OpenDecoder(format.get(), audio_index_, &error);
    CodecContextPtr video = OpenDecoder(format.get(), video_index_, &error);
    if ((audio_index_ >= 0 && !audio) || (video_index_ >= 0 && !video)) {
      Finish({.reason = FileMediaTerminalReason::kError, .error = error});
      return;
    }
    size_t cycle_index = 0;
    auto playback_start = std::chrono::steady_clock::now();
    while (!stop_.load()) {
      if (!RunCycle(format.get(), audio.get(), video.get(), cycle_index,
                    &playback_start, &error)) {
        Finish({.reason = FileMediaTerminalReason::kError, .error = error});
        return;
      }
      if (stop_.load() || config_.eof_mode == FileMediaEofMode::kStop)
        break;
      if (!Rewind(format.get(), common_start_us_)) {
        Finish({.reason = FileMediaTerminalReason::kError,
                .error = Error(FileMediaErrorCode::kInvalidTiming,
                               "failed to seek to loop start")});
        return;
      }
      if (audio)
        avcodec_flush_buffers(audio.get());
      if (video)
        avcodec_flush_buffers(video.get());
      ++cycle_index;
      observer_->OnLoopBoundary(
          {.cycle_index = cycle_index,
           .timestamp_us =
               static_cast<int64_t>(cycle_index) * info_.loop_duration_us});
    }
    Finish({.reason = stop_.load() ? FileMediaTerminalReason::kCallerStopped
                                   : FileMediaTerminalReason::kEndOfFile});
  }

  FileMediaCapturerConfig config_;
  FileMediaCapturerObserver* const observer_;
  const FileMediaInfo info_;
  const int audio_index_;
  const int video_index_;
  const int64_t common_start_us_;
  std::atomic<bool> started_ = false;
  std::atomic<bool> stop_ = false;
  std::atomic<bool> terminal_sent_ = false;
  std::mutex wait_mutex_;
  std::condition_variable wait_condition_;
  std::mutex join_mutex_;
  std::thread worker_;
};

}  // namespace

FileMediaCapturerCreateResult CreateFileMediaCapturer(
    FileMediaCapturerConfig config,
    FileMediaCapturerObserver* observer) {
  if (!observer || config.input_path.empty()) {
    return {.error = Error(FileMediaErrorCode::kInternal,
                           "observer and input path are required")};
  }
  AVFormatContext* raw_format = nullptr;
  int status = avformat_open_input(&raw_format, config.input_path.c_str(),
                                   nullptr, nullptr);
  FormatContextPtr format(raw_format);
  if (status < 0) {
    return {.error = Error(FileMediaErrorCode::kUnsupportedContainer,
                           "failed to open MP4 input: " + AvError(status))};
  }
  if (format->iformat == nullptr ||
      std::string(format->iformat->name).find("mov") == std::string::npos) {
    return {.error = Error(FileMediaErrorCode::kUnsupportedContainer,
                           "input is not an MP4 container")};
  }
  status = avformat_find_stream_info(format.get(), nullptr);
  if (status < 0) {
    return {.error = Error(FileMediaErrorCode::kInvalidTiming,
                           "failed to read MP4 stream metadata")};
  }
  FileMediaError error;
  const int audio_index = SelectStream(format.get(), AVMEDIA_TYPE_AUDIO,
                                       config.audio_stream_index, &error);
  if (audio_index == -2)
    return {.error = std::move(error)};
  const int video_index = SelectStream(format.get(), AVMEDIA_TYPE_VIDEO,
                                       config.video_stream_index, &error);
  if (video_index == -2)
    return {.error = std::move(error)};
  if (audio_index < 0 && video_index < 0) {
    return {.error = Error(FileMediaErrorCode::kUnsupportedCodec,
                           "input has no supported audio or video track")};
  }
  CodecContextPtr audio = OpenDecoder(format.get(), audio_index, &error);
  CodecContextPtr video = OpenDecoder(format.get(), video_index, &error);
  if ((audio_index >= 0 && !audio) || (video_index >= 0 && !video))
    return {.error = std::move(error)};
  if (audio &&
      !ProbeFirstFrame(format.get(), audio.get(), audio_index, false, &error))
    return {.error = std::move(error)};
  if (!Rewind(format.get(), 0)) {
    return {.error = Error(FileMediaErrorCode::kInvalidTiming,
                           "failed to rewind after audio format probe")};
  }
  if (video &&
      !ProbeFirstFrame(format.get(), video.get(), video_index, true, &error))
    return {.error = std::move(error)};

  FileMediaInfo info;
  if (audio_index >= 0)
    info.audio = TrackInfo(format->streams[audio_index]);
  if (video_index >= 0)
    info.video = TrackInfo(format->streams[video_index]);
  int64_t start_us =
      info.audio ? info.audio->start_time_us : info.video->start_time_us;
  int64_t end_us = info.audio
                       ? info.audio->start_time_us + info.audio->duration_us
                       : info.video->start_time_us + info.video->duration_us;
  if (info.video) {
    start_us = std::max(start_us, info.video->start_time_us);
    end_us =
        std::min(end_us, info.video->start_time_us + info.video->duration_us);
  }
  info.loop_duration_us = end_us - start_us;
  if (info.audio)
    info.loop_duration_us =
        info.loop_duration_us / kAudioBlockUs * kAudioBlockUs;
  if (info.loop_duration_us <= 0) {
    return {.error = Error(FileMediaErrorCode::kInvalidTiming,
                           "selected tracks have no usable interval")};
  }
  return {.capturer = std::make_unique<FfmpegFileMediaCapturer>(
              std::move(config), observer, std::move(info), audio_index,
              video_index, start_us)};
}

}  // namespace webrtc
