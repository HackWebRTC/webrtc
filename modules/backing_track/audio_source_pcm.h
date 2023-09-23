//
// Created by Piasy on 2018/5/28.
//

#pragma once

#include "rtc_base/buffer.h"

#include "modules/backing_track/audio_source.h"

namespace webrtc {

class AudioSourcePcm : public AudioSource {
public:
    AudioSourcePcm(int32_t ssrc, int32_t sample_rate, int32_t channel_num,
                   int32_t frame_duration_us, float volume, bool enabled);

    ~AudioSourcePcm() override;

    int32_t FrameSize() override;

    void OnAudioRecorded(const void* data, int32_t size);

    AudioFrameInfo GetAudioFrameWithInfo(
        int32_t sample_rate_hz, webrtc::AudioFrame* audio_frame) override;

    void UpdateFrameDurationUs(int32_t frame_duration_us) override {
        AudioSource::UpdateFrameDurationUs(frame_duration_us);
        real_buffer_num_elements_ =
            channel_num_ * sample_rate_ * frame_duration_us / 1000 / 1000;
    }

private:
    int32_t report_output_samples_;
    int32_t real_buffer_num_elements_;

    rtc::BufferT<int16_t> buffer_;

    int16_t* mv_buf_;
    size_t mv_buf_size_;
};
}
