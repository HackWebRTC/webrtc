//
// Created by Piasy on 2019/12/16.
//

#pragma once

#include <string>
#include <queue>
#include <memory>

#include "api/audio_codecs/audio_encoder.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/video/encoded_image.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/task_queue.h"

struct AVFormatContext;
struct AVStream;

namespace webrtc {
class Recorder {
public:
    Recorder(TaskQueueFactory* task_queue_factory);
    ~Recorder();

    int32_t Start(const std::string& path);

    void AddVideoFrame(const EncodedImage* frame,
                       VideoCodecType video_codec);
    void AddAudioFrame(int32_t sample_rate, int32_t channel_num,
                       const uint8_t* frame, uint32_t size,
                       AudioEncoder::CodecType audio_codec);

    void Stop();

private:
    class Frame {
    public:
        Frame(const uint8_t* payload, uint32_t length);
        ~Frame();

        uint8_t* payload;
        uint32_t length;
        int64_t timestamp;
        int64_t duration;
        bool is_video;
        bool is_key_frame;
    };

    int parseParamSets(int video_codec_id, const uint8_t* payload, uint32_t length);

    void openStreams();

    void drainFrames();

    std::shared_ptr<Frame> last_audio_frame_;
    std::shared_ptr<Frame> last_video_frame_;
    std::shared_ptr<Frame> video_key_frame_;

    bool got_audio_;
    AudioEncoder::CodecType audio_codec_;
    int32_t sample_rate_;
    int32_t channel_num_;

    bool got_video_;
    VideoCodecType video_codec_;
    int32_t width_;
    int32_t height_;

    bool stream_opened_;

    AVFormatContext* context_;
    AVStream* audio_stream_;
    AVStream* video_stream_;

    rtc::TaskQueue record_queue_;
    std::queue<std::shared_ptr<Frame>> frames_;
    int64_t timestamp_offset_;

    int64_t added_audio_frames_;
    int64_t added_video_frames_;
    int64_t drained_frames_;
};
}
