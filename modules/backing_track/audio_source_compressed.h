//
// Created by Piasy on 29/10/2017.
//

#pragma once

#include <string>

#include "rtc_base/buffer.h"
#include "rtc_base/buffer_queue.h"

#include "modules/backing_track/audio_source.h"
#include "modules/backing_track/audio_file_decoder.h"
#include "modules/backing_track/audio_resampler.h"

namespace webrtc {

class AudioSourceCompressed : public AudioSource {
public:
    AudioSourceCompressed(int32_t ssrc, const std::string& filepath,
                          int32_t output_sample_rate,
                          int32_t output_channel_num, int32_t frame_duration_us,
                          float volume_left, float volume_right, bool enabled,
                          bool enable_sync_fix, bool remix,
                          int32_t waiting_mix_delay_frames,
                          SourceFinishCallback finish_callback,
                          SourceErrorCallback error_callback,
                          void* callback_opaque);

    ~AudioSourceCompressed() override;

    void ToggleEnable(bool enabled) override;

    bool StereoInput() override { return input_channel_num_ == 2; }

    int32_t FrameSize() override;

    int64_t GetProgressMs() override {
        return decoder_ ? decoder_->consume_progress_ms() : -2;
    }

    int64_t GetLengthMs() override {
        return decoder_ ? decoder_->length_ms() : 0;
    }

    void Seek(int64_t position_ms) override;

    AudioFrameInfo GetAudioFrameWithInfo(
        int32_t sample_rate_hz, webrtc::AudioFrame* audio_frame) override;

    int32_t input_sample_rate();

    int32_t input_channel_num();

    /**
     * @return > 0 for successfully read size
     *         AVERROR_EOF for end of file
     *         other value <= 0 for error
     */
    int32_t Read(void** buffer);

    void UpdateFrameDurationUs(int32_t frame_duration_us) override {
        AudioSource::UpdateFrameDurationUs(frame_duration_us);
        real_output_samples_ = sample_rate_ * frame_duration_us / 1000 / 1000;
    }

private:
    void checkInitWaitingMixQueue();
    void fireErrorCallback(int32_t code);

    int32_t input_sample_rate_;
    int32_t input_channel_num_;
    AVSampleFormat input_format_;
    int32_t once_decode_samples_;

    int32_t report_output_samples_;
    int32_t real_output_samples_;

    bool enable_sync_fix_;
    int32_t sync_fix_threshold_ms_;
    int32_t sync_fix_break_times_;

    bool remix_;

    void** input_buffer_;

    std::unique_ptr<AudioFileDecoder> decoder_;
    std::unique_ptr<AudioResampler> resampler_;

    rtc::BufferT<int16_t> buffer_;
    int32_t buffer_pos_;

    std::unique_ptr<rtc::BufferQueue> waiting_mix_;
    int32_t waiting_mix_delay_frames_;

    int64_t start_time_;
    int64_t samples_mixed_;
    bool first_frame_decoded_;

    SourceFinishCallback finish_callback_;
    SourceErrorCallback error_callback_;
    void* callback_opaque_;
    bool finish_callback_fired_;
    bool error_callback_fired_;
};
}
