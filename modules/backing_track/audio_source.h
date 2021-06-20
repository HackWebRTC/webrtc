//
// Created by Piasy on 2018/5/28.
//

#pragma once

#include <atomic>

#include "api/audio/audio_mixer.h"
#include "rtc_base/synchronization/mutex.h"

#include "modules/backing_track/pcm_channel.h"

namespace webrtc {

class AudioSource : public webrtc::AudioMixer::Source {
public:
    AudioSource(int32_t ssrc, int32_t sample_rate, int32_t channel_num,
                int32_t frame_duration_us, float volume_left,
                float volume_right, bool enabled);

    virtual ~AudioSource() override;

    int32_t Ssrc() const override { return ssrc_; }

    int32_t PreferredSampleRate() const override { return sample_rate_; }

    void UpdateVolume(float volume_left, float volume_right) {
        volume_left_ = volume_left;
        volume_right_ = volume_right;
    }

    virtual void ToggleEnable(bool enabled) { enabled_ = enabled; }

    void ToggleMute(bool mute) { muted_ = mute; }

    virtual bool StereoInput() { return false; }

    virtual int32_t FrameSize() = 0;

    virtual int64_t GetProgressMs() { return 0; }

    virtual int64_t GetLengthMs() { return 0; }

    virtual void Seek(int64_t position_ms) {}

    void SetPcmChannel(PcmChannel* pcm_channel);

    PcmChannel* GetPcmChannel() { return pcm_channel_; }

    virtual void UpdateFrameDurationUs(int32_t frame_duration_us) {
        if (sample_rate_ <= 0) {
            return;
        }

        frame_duration_us_ = frame_duration_us;

        if (pcm_channel_) {
            pcm_channel_->SetFrameDurationUs(frame_duration_us);
        }
    }

    int32_t sample_rate() { return sample_rate_; }

    int32_t channel_num() { return channel_num_; }

    int32_t frame_duration_us() { return frame_duration_us_; }

    bool enabled() { return enabled_.load(); }

    bool muted() { return muted_.load(); }

protected:
    void preProduceFrame(webrtc::AudioFrame* frame, bool remix);

    int64_t GetTimestamp();

    int32_t ssrc_;

    int32_t sample_rate_;
    int32_t channel_num_;
    int32_t frame_duration_us_;

    PcmChannel* pcm_channel_;

private:
    float volume_left_;
    float volume_right_;
    std::atomic_bool enabled_;
    std::atomic_bool muted_;

    mutable Mutex mutex_;
};
}
