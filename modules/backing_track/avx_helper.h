//
// Created by Piasy on 08/11/2017.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/audio_fifo.h>

#ifdef __cplusplus
}
#endif

namespace webrtc {

struct AVFormatContextDeleter {
    void operator()(AVFormatContext* context) {
        if (context) {
            avformat_close_input(&context);
        }
    }
};

struct AVCodecContextDeleter {
    void operator()(AVCodecContext* context) {
        if (context) {
            avcodec_free_context(&context);
        }
    }
};

struct AVFrameDeleter {
    void operator()(AVFrame* frame) {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

struct AVPacketDeleter {
    void operator()(AVPacket* packet) {
        if (packet) {
            av_packet_free(&packet);
        }
    }
};

struct SwrContextDeleter {
    void operator()(SwrContext* swrContext) {
        if (swrContext) {
            swr_free(&swrContext);
        }
    }
};

struct AVAudioFifoDeleter {
    void operator()(AVAudioFifo* fifo) {
        if (fifo) {
            av_audio_fifo_free(fifo);
        }
    }
};
}
