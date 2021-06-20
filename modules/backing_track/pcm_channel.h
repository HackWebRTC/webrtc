//
// Created by Piasy on 08/11/2017.
//

#pragma once

#include "api/audio/audio_mixer.h"
#include "rtc_base/buffer.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

class PcmChannel : public AudioMixer::Source {
public:
    PcmChannel(int32_t sample_rate, int32_t channel_num, int32_t frame_duration_us);

    ~PcmChannel() override;

    void FeedData(const void* data, int32_t size);

    AudioFrameInfo
    GetAudioFrameWithInfo(int32_t sample_rate_hz, webrtc::AudioFrame* audio_frame) override;

    int32_t Ssrc() const override;

    int32_t PreferredSampleRate() const override;

    void ToggleMix(bool enable);

    void SetFrameDurationUs(int32_t frame_duration_us);

private:
    int32_t ssrc_;

    int32_t sample_rate_;
    int32_t channel_num_;

    int32_t report_output_samples_;
    std::atomic_int_least32_t real_buffer_num_elements_;

    mutable Mutex mutex_;
    std::atomic_bool enabled_;
    rtc::BufferT<int16_t> buffer_;

    int16_t* mv_buf_;
    size_t mv_buf_size_;
};

}
