

//
// Created by Piasy on 08/11/2017.
//

#pragma once

#include <string>
#include <vector>

#include "api/task_queue/task_queue_factory.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_queue.h"

#include "modules/backing_track/avx_helper.h"

namespace webrtc {

class AudioFileDecoder {
public:
    AudioFileDecoder(TaskQueueFactory* task_queue_factory,
                     const std::string& filepath);

    ~AudioFileDecoder() {}

    AVSampleFormat sample_format();

    int32_t sample_rate();

    int32_t channel_num();

    int64_t consume_progress_ms() { return last_consumed_frame_pts_; }

    int64_t length_ms() {
        return format_context_
                   ? 1000 * format_context_->streams[stream_no_]->duration *
                         format_context_->streams[stream_no_]->time_base.num /
                         format_context_->streams[stream_no_]->time_base.den
                   : 0;
    }

    int32_t Consume(void** buffer, int32_t samples);

    void Seek(int64_t position_ms);

    bool eof() { return eof_; }

private:
    void FillDecoder(bool seeking);

    bool FillFifo(bool seeking, int64_t* last_frame_ts);

    void Advance();

    int32_t stream_no_;

    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> format_context_;
    std::unique_ptr<AVCodecContext, AVCodecContextDeleter> codec_context_;

    std::unique_ptr<AVPacket, AVPacketDeleter> packet_;
    bool packet_consumed_;
    std::unique_ptr<AVFrame, AVFrameDeleter> frame_;
    bool frame_consumed_;

    mutable Mutex seek_mutex_;

    mutable Mutex fifo_mutex_;
    int32_t fifo_capacity_;
    std::unique_ptr<AVAudioFifo, AVAudioFifoDeleter> fifo_;

    bool eof_;
    bool error_;
    bool seeking_;

    int64_t last_decoded_frame_pts_;
    int64_t last_consumed_frame_pts_;

    rtc::TaskQueue decoder_queue_;

    int64_t consumed_frames_ = 0;
};
}
