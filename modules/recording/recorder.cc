#include "modules/recording/recorder.h"

#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "common_video/h264/h264_common.h"
#ifndef DISABLE_H265
#include "common_video/h265/h265_common.h"
#endif
#include "rtc_base/logging.h"

namespace webrtc {

static inline int64_t currentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

Recorder::Frame::Frame(const uint8_t* payload, uint32_t length) {
    this->payload = new uint8_t[length];
    memcpy(this->payload, payload, length);
    this->length = length;
    this->timestamp = currentTimeMs();
    this->duration = 0;
    this->is_video = false;
    this->is_key_frame = false;
}

Recorder::Frame::~Frame() {
    delete[] payload;
}

Recorder::Recorder(TaskQueueFactory* task_queue_factory)
    : got_audio_(false),
      sample_rate_(0),
      channel_num_(0),
      got_video_(false),
      width_(0),
      height_(0),
      stream_opened_(false),
      context_(nullptr),
      audio_stream_(nullptr),
      video_stream_(nullptr),
      record_queue_(task_queue_factory->CreateTaskQueue(
          "recorder", TaskQueueFactory::Priority::NORMAL)),
      timestamp_offset_(0),
      added_audio_frames_(0),
      added_video_frames_(0),
      drained_frames_(0) {
}

Recorder::~Recorder() { Stop(); }

int32_t Recorder::Start(const std::string& path) {
    const char* format_name = "matroska";
    avformat_alloc_output_context2(&context_, nullptr, format_name,
                                   path.c_str());
    if (!context_) {
        RTC_LOG(LS_ERROR) << "Recorder::Start error, alloc context fail";
        return -11;
    }
    int res = avio_open(&context_->pb, context_->url, AVIO_FLAG_WRITE);
    if (res < 0) {
        RTC_LOG(LS_ERROR) << "Recorder::Start error, open fail "
                          << av_err2str(res);
        avformat_free_context(context_);
        context_ = nullptr;
        return -12;
    }

    RTC_LOG(LS_INFO) << "Recorder::Start success";
    return 0;
}

void Recorder::AddVideoFrame(const EncodedImage* frame,
                             VideoCodecType video_codec) {
    if (++added_video_frames_ % 125 == 1) {
        RTC_LOG(LS_INFO) << "Recorder::AddVideoFrame " << added_video_frames_
                         << " times";
    }
    if (!got_video_ && frame->_frameType == VideoFrameType::kVideoFrameKey) {
        got_video_ = true;
        video_codec_ = video_codec;
        width_ = frame->_encodedWidth;
        height_ = frame->_encodedHeight;
    }

    std::shared_ptr<Frame> media_frame(new Frame(frame->data(), frame->size()));
    media_frame->is_video = true;
    media_frame->is_key_frame =
        frame->_frameType == VideoFrameType::kVideoFrameKey;

    if (!last_video_frame_) {
        last_video_frame_ = media_frame;
        return;
    }

    last_video_frame_->duration =
        media_frame->timestamp - last_video_frame_->timestamp;
    if (last_video_frame_->duration <= 0) {
        last_video_frame_->duration = 1;
        media_frame->timestamp = last_video_frame_->timestamp + 1;
    }

    if (last_video_frame_->is_key_frame && !video_key_frame_) {
        video_key_frame_ = last_video_frame_;
    }

    frames_.push(last_video_frame_);
    last_video_frame_ = media_frame;

    record_queue_.PostTask([this]() { drainFrames(); });
}

void Recorder::AddAudioFrame(int32_t sample_rate, int32_t channel_num,
                             const uint8_t* frame, uint32_t size,
                             AudioEncoder::CodecType audio_codec) {
    if (++added_audio_frames_ % 500 == 1) {
        RTC_LOG(LS_INFO) << "Recorder::AddAudioFrame " << added_audio_frames_
                         << " times";
    }
    if (!frame || !size) {
      return;
    }

    if (!got_audio_) {
        got_audio_ = true;
        audio_codec_ = audio_codec;
        sample_rate_ = sample_rate;
        channel_num_ = channel_num;
    }

    std::shared_ptr<Frame> media_frame(new Frame(frame, size));

    if (!last_audio_frame_) {
        last_audio_frame_ = media_frame;
        return;
    }

    last_audio_frame_->duration =
        media_frame->timestamp - last_audio_frame_->timestamp;
    if (last_audio_frame_->duration <= 0) {
        last_audio_frame_->duration = 1;
        media_frame->timestamp = last_audio_frame_->timestamp + 1;
    }

    frames_.push(last_audio_frame_);
    last_audio_frame_ = media_frame;

    record_queue_.PostTask([this]() { drainFrames(); });
}

void Recorder::Stop() {
    if (context_) {
        if (audio_stream_ && video_stream_) {
            av_write_trailer(context_);
        }
        avio_close(context_->pb);
        avformat_free_context(context_);
        context_ = nullptr;
    }
    audio_stream_ = nullptr;
    video_stream_ = nullptr;
}

int Recorder::parseParamSets(int video_codec_id, const uint8_t* payload, uint32_t length) {
    if (video_codec_id == (int) AV_CODEC_ID_H264) {
        std::vector<H264::NaluIndex> nalu_indices = H264::FindNaluIndices(payload, length);
        bool is_param_sets = false;
        for (const H264::NaluIndex& index : nalu_indices) {
            H264::NaluType nalu_type = H264::ParseNaluType(payload[index.payload_start_offset]);
            switch (nalu_type) {
                case H264::NaluType::kSps:
                case H264::NaluType::kPps:
                    is_param_sets = true;
                    break;
                default:
                    if (is_param_sets) {
                        return index.start_offset - 1;
                    }
                    break;
            }
        }
#ifndef DISABLE_H265
    } else {
        std::vector<H265::NaluIndex> nalu_indices = H265::FindNaluIndices(payload, length);
        bool is_param_sets = false;
        for (const H265::NaluIndex& index : nalu_indices) {
            H265::NaluType nalu_type = H265::ParseNaluType(payload[index.payload_start_offset]);
            switch (nalu_type) {
                case H265::NaluType::kVps:
                case H265::NaluType::kSps:
                case H265::NaluType::kPps:
                    is_param_sets = true;
                    break;
                default:
                    if (is_param_sets) {
                        return index.start_offset - 1;
                    }
                    break;
            }
        }
    }
#else
    }
#endif
    return -1;
}

void Recorder::openStreams() {
    if (got_audio_ && got_video_ && video_key_frame_ && !stream_opened_) {
        stream_opened_ = true;

        enum AVCodecID audio_codec_id = AV_CODEC_ID_NONE;
        switch (audio_codec_) {
            case AudioEncoder::CodecType::kOpus:
                audio_codec_id = AV_CODEC_ID_OPUS;
                break;
            default:
                break;
        }
        enum AVCodecID video_codec_id = AV_CODEC_ID_NONE;
        switch (video_codec_) {
            case kVideoCodecVP8:
                video_codec_id = AV_CODEC_ID_VP8;
                break;
            case kVideoCodecVP9:
                video_codec_id = AV_CODEC_ID_VP9;
                break;
            case kVideoCodecH264:
                video_codec_id = AV_CODEC_ID_H264;
                break;
#ifndef DISABLE_H265
            case kVideoCodecH265:
                video_codec_id = AV_CODEC_ID_H265;
                break;
#endif
            default:
                break;
        }
        if (audio_codec_id == AV_CODEC_ID_NONE ||
            video_codec_id == AV_CODEC_ID_NONE) {
            RTC_LOG(LS_ERROR)
                << "Recorder::openStreams error, unsupported codec, audio "
                << audio_codec_ << ", video " << video_codec_;
            return;
        }

        AVStream* audio_stream = avformat_new_stream(context_, nullptr);
        if (!audio_stream) {
            RTC_LOG(LS_ERROR)
                << "Recorder::openStreams error, open audio stream fail";
            return;
        }

        AVCodecParameters* par = audio_stream->codecpar;
        par->codec_type = AVMEDIA_TYPE_AUDIO;
        par->codec_id = audio_codec_id;
        par->sample_rate = sample_rate_;
        if (channel_num_ == 1) {
            par->ch_layout = AV_CHANNEL_LAYOUT_MONO;
        } else if (channel_num_ == 2) {
            par->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        }
        switch (audio_codec_id) {
            case AV_CODEC_ID_AAC:  // AudioSpecificConfig 48000-2
                par->extradata_size = 2;
                par->extradata = (uint8_t*)av_malloc(
                    par->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
                par->extradata[0] = 0x11;
                par->extradata[1] = 0x90;
                break;
            case AV_CODEC_ID_OPUS:  // OpusHead 48000-2
                par->extradata_size = 19;
                par->extradata = (uint8_t*)av_malloc(
                    par->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
                par->extradata[0] = 'O';
                par->extradata[1] = 'p';
                par->extradata[2] = 'u';
                par->extradata[3] = 's';
                par->extradata[4] = 'H';
                par->extradata[5] = 'e';
                par->extradata[6] = 'a';
                par->extradata[7] = 'd';
                // Version
                par->extradata[8] = 1;
                // Channel Count
                par->extradata[9] = 2;
                // Pre-skip
                par->extradata[10] = 0x38;
                par->extradata[11] = 0x1;
                // Input Sample Rate (Hz)
                par->extradata[12] = 0x80;
                par->extradata[13] = 0xbb;
                par->extradata[14] = 0;
                par->extradata[15] = 0;
                // Output Gain (Q7.8 in dB)
                par->extradata[16] = 0;
                par->extradata[17] = 0;
                // Mapping Family
                par->extradata[18] = 0;
                break;
            default:
                break;
        }

        AVStream* video_stream = avformat_new_stream(context_, nullptr);
        if (!video_stream) {
            RTC_LOG(LS_ERROR)
                << "Recorder::openStreams error, open video stream fail";
            return;
        }

        par = video_stream->codecpar;
        par->codec_type = AVMEDIA_TYPE_VIDEO;
        par->codec_id = video_codec_id;
        par->width = width_;
        par->height = height_;
        if (video_codec_id == AV_CODEC_ID_H264 ||
            video_codec_id == AV_CODEC_ID_H265) {
            // extradata
            int size = parseParamSets((int) video_codec_id, video_key_frame_->payload,
                                      video_key_frame_->length);
            if (size > 0) {
                par->extradata_size = size;
                par->extradata = (uint8_t*)av_malloc(
                    par->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
                memcpy(par->extradata, video_key_frame_->payload,
                       par->extradata_size);
            } else {
                RTC_LOG(LS_WARNING) << "Recorder::openStreams error, can't "
                                       "find video extradata";
            }
        }

        if (video_codec_id == AV_CODEC_ID_H265) {
            par->codec_tag = 0x31637668;  // hvc1
        }

        int res = avformat_write_header(context_, nullptr);
        if (res < 0) {
            RTC_LOG(LS_ERROR)
                << "Recorder::openStreams error, avformat_write_header fail "
                << av_err2str(res);
            return;
        }

        audio_stream_ = audio_stream;
        video_stream_ = video_stream;
        timestamp_offset_ = currentTimeMs();

        RTC_LOG(LS_INFO) << "Recorder::openStreams success";
    }
}

void Recorder::drainFrames() {
    openStreams();

    if (!audio_stream_ || !video_stream_) {
        return;
    }

    while (!frames_.empty()) {
        if (++drained_frames_ % 1000 == 1) {
            RTC_LOG(LS_INFO) << "Recorder::drainFrames " << drained_frames_
                             << " times";
        }
        std::shared_ptr<Frame> frame = frames_.front();
        frames_.pop();

        AVStream* stream = frame->is_video ? video_stream_ : audio_stream_;

        AVPacket* pkt = av_packet_alloc();
        pkt->data = frame->payload;
        pkt->size = frame->length;
        pkt->dts = (int64_t)((frame->timestamp - timestamp_offset_) /
                             (av_q2d(stream->time_base) * 1000));
        pkt->pts = pkt->dts;
        pkt->duration =
            (int64_t)(frame->duration / (av_q2d(stream->time_base) * 1000));
        pkt->stream_index = stream->index;

        if (frame->is_key_frame) {
            pkt->flags |= AV_PKT_FLAG_KEY;
        }

        int res = av_interleaved_write_frame(context_, pkt);
        if (res < 0) {
            RTC_LOG(LS_ERROR) << "Recorder::drainFrames error, "
                                 "av_interleaved_write_frame fail "
                              << av_err2str(res);
        }
        av_packet_free(&pkt);
    }
}
}
