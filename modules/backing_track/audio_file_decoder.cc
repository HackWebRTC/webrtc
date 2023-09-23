//
// Created by Piasy on 08/11/2017.
//

#include <algorithm>

#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/backing_track/audio_file_decoder.h"
#include "modules/backing_track/audio_mixer_global.h"
#include "rtc_base/logging.h"

namespace webrtc {

AudioFileDecoder::AudioFileDecoder(TaskQueueFactory* task_queue_factory,
                                   const std::string& filepath)
    : packet_consumed_(true),
      frame_consumed_(true),
      eof_(false),
      error_(false),
      seeking_(false),
      last_decoded_frame_pts_(0),
      last_consumed_frame_pts_(0),
      decoder_queue_(task_queue_factory->CreateTaskQueue(
          "music_dec", TaskQueueFactory::Priority::HIGH)) {
    frame_.reset(av_frame_alloc());
    if (!frame_) {
        RTC_LOG(LS_ERROR) << "AudioFileDecoder:: av_frame_alloc fail";
        return;
    }

    packet_.reset(av_packet_alloc());
    if (!packet_) {
        RTC_LOG(LS_ERROR) << "AudioFileDecoder:: av_packet_alloc fail";
        return;
    }

    {
        AVFormatContext* format_context = nullptr;
        int32_t error = avformat_open_input(&format_context, filepath.c_str(),
                                            nullptr, nullptr);
        if (error < 0) {
            RTC_LOG(LS_ERROR) << "AudioFileDecoder:: avformat_open_input fail "
                              << filepath.c_str() << " "
                              << av_err2str(error);
            return;
        }

        format_context_.reset(format_context);
    }

    int32_t error = avformat_find_stream_info(format_context_.get(), nullptr);
    if (error < 0) {
        RTC_LOG(LS_ERROR)
            << "AudioFileDecoder:: avformat_find_stream_info fail "
            << av_err2str(error);
        return;
    }

    const AVCodec* codec = nullptr;
    stream_no_ = av_find_best_stream(format_context_.get(), AVMEDIA_TYPE_AUDIO,
                                     -1, -1, &codec, 0);
    if (stream_no_ < 0 || !codec
        || format_context_->streams[stream_no_]->time_base.den <= 0) {
        RTC_LOG(LS_ERROR) << "AudioFileDecoder:: av_find_best_stream fail "
                          << av_err2str(stream_no_) << ", codec "
                          << static_cast<const void*>(codec);
        return;
    }

    codec_context_.reset(avcodec_alloc_context3(codec));
    if (!codec_context_) {
        RTC_LOG(LS_ERROR) << "AudioFileDecoder:: avcodec_alloc_context3 fail";
        return;
    }
    error = avcodec_parameters_to_context(
        codec_context_.get(), format_context_->streams[stream_no_]->codecpar);
    if (error < 0) {
        RTC_LOG(LS_ERROR)
            << "AudioFileDecoder:: avcodec_parameters_to_context fail "
            << av_err2str(error);
        return;
    }

    error = avcodec_open2(codec_context_.get(), codec, nullptr);
    if (error < 0) {
        RTC_LOG(LS_ERROR) << "AudioFileDecoder:: avcodec_open2 fail "
                          << av_err2str(error);
        return;
    }

    fifo_capacity_ = 10 * codec_context_->sample_rate *
                     webrtc::AudioMixerImpl::kFrameDurationInMs / 1000;
    fifo_.reset(av_audio_fifo_alloc(codec_context_->sample_fmt,
                                    codec_context_->ch_layout.nb_channels, fifo_capacity_));
    if (!fifo_) {
        RTC_LOG(LS_ERROR) << "AudioFileDecoder:: av_audio_fifo_alloc fail";
        return;
    }

    RTC_LOG(LS_INFO)
        << "AudioFileDecoder create: start ts "
        << format_context_->streams[stream_no_]->start_time *
               format_context_->streams[stream_no_]->time_base.num /
               (float)format_context_->streams[stream_no_]->time_base.den
        << " s, duration "
        << format_context_->streams[stream_no_]->duration *
               format_context_->streams[stream_no_]->time_base.num /
               (float)format_context_->streams[stream_no_]->time_base.den
        << " s,"
        << " ch " << codec_context_->ch_layout.nb_channels;

    FillDecoder(false);
    FillFifo(false, nullptr);
    Advance();
}

AVSampleFormat AudioFileDecoder::sample_format() {
    return codec_context_ ? codec_context_->sample_fmt : AV_SAMPLE_FMT_NONE;
}

int32_t AudioFileDecoder::sample_rate() {
    return codec_context_ ? codec_context_->sample_rate : 0;
}

int32_t AudioFileDecoder::channel_num() {
    return codec_context_ ? codec_context_->ch_layout.nb_channels : 0;
}

int32_t AudioFileDecoder::Consume(void** buffer, int32_t samples) {
    if (!fifo_ || codec_context_->sample_rate <= 0) {
        return kMixerErrInit;
    }
    if (error_) {
        return kMixerErrDecode;
    }

    if (++consumed_frames_ % 500 == 1) {
        RTC_LOG(LS_INFO) << "AudioFileDecoder::Consume " << consumed_frames_
                         << " times, last_decoded_frame_pts_ " << last_decoded_frame_pts_
                         << ", last_consumed_frame_pts_ " << last_consumed_frame_pts_;
    }

    Advance();

    MutexLock lock(&fifo_mutex_);

    int32_t target_samples = std::min(av_audio_fifo_size(fifo_.get()), samples);
    int32_t actual_samples =
        av_audio_fifo_read(fifo_.get(), buffer, target_samples);
    last_consumed_frame_pts_ =
        last_decoded_frame_pts_ -
        1000 * av_audio_fifo_size(fifo_.get()) / codec_context_->sample_rate;

    return actual_samples *
           av_get_bytes_per_sample(codec_context_->sample_fmt) *
           codec_context_->ch_layout.nb_channels;
}

void AudioFileDecoder::Seek(int64_t position_ms) {
    seeking_ = true;

    MutexLock lock(&seek_mutex_);

    RTC_LOG(LS_INFO) << "AudioFileDecoder::Seek start, want " << position_ms;

    av_audio_fifo_reset(fifo_.get());
    av_seek_frame(format_context_.get(), stream_no_,
                  static_cast<int64_t>(
                      (position_ms - 100) / 1000.0F *
                      format_context_->streams[stream_no_]->time_base.den /
                      format_context_->streams[stream_no_]->time_base.num),
                  AVSEEK_FLAG_ANY);

    int64_t last_frame_ts = 0;
    do {
        FillDecoder(true);
    } while (!eof_ && !error_ && !FillFifo(true, &last_frame_ts) &&
             last_frame_ts < position_ms);

    seeking_ = false;

    RTC_LOG(LS_INFO) << "AudioFileDecoder::Seek end, actual " << last_frame_ts;
}

void AudioFileDecoder::FillDecoder(bool seeking) {
    while (!eof_ && !error_ && seeking == seeking_) {
        if (packet_consumed_) {
            int error = av_read_frame(format_context_.get(), packet_.get());
            if (error != 0) {
                eof_ = error == AVERROR_EOF;
                error_ = error != AVERROR_EOF;
                break;
            }
            if (packet_->stream_index != stream_no_) {
                av_packet_unref(packet_.get());
                continue;
            }
            packet_consumed_ = false;
        }
        int32_t error =
            avcodec_send_packet(codec_context_.get(), packet_.get());
        if (error == 0) {
            av_packet_unref(packet_.get());
            packet_consumed_ = true;
            continue;
        }
        if (error == AVERROR(EAGAIN)) {
            break;
        }
        RTC_LOG(LS_ERROR) << "FillDecoder error " << av_err2str(error);
        error_ = true;
        break;
    }
}

bool AudioFileDecoder::FillFifo(bool seeking, int64_t* last_frame_ts) {
    bool fifo_full = false;
    while (!eof_ && !error_ && seeking == seeking_) {
        if (frame_consumed_) {
            int error =
                avcodec_receive_frame(codec_context_.get(), frame_.get());
            if (error != 0) {
                error_ = error != AVERROR(EAGAIN);
                break;
            }

            frame_consumed_ = false;
        }

        if (seeking) {
            frame_consumed_ = true;
            if (last_frame_ts) {
                *last_frame_ts =
                    1000 * frame_->pts *
                    format_context_->streams[stream_no_]->time_base.num /
                    format_context_->streams[stream_no_]->time_base.den;
            }
            break;
        }

        MutexLock lock(&fifo_mutex_);

        if (av_audio_fifo_size(fifo_.get()) + frame_->nb_samples <
            fifo_capacity_) {
            if (av_audio_fifo_write(fifo_.get(), reinterpret_cast<void**>(
                                                     frame_->extended_data),
                                    frame_->nb_samples) < 0) {
                error_ = true;
                break;
            }
            last_decoded_frame_pts_ =
                1000 * frame_->pts *
                format_context_->streams[stream_no_]->time_base.num /
                format_context_->streams[stream_no_]->time_base.den;
            av_frame_unref(frame_.get());

            frame_consumed_ = true;
        } else {
            fifo_full = true;
            break;
        }
    }

    return fifo_full;
}

void AudioFileDecoder::Advance() {
    decoder_queue_.PostTask([=]() {
        MutexLock lock(&seek_mutex_);
        do {
            FillDecoder(false);
        } while (!eof_ && !error_ && !seeking_ && !FillFifo(false, nullptr));
    });
}
}
