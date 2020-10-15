//
// Created by Piasy on 2018/5/28.
//

#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/backing_track/audio_source_pcm.h"
#include "modules/backing_track/audio_mixer_global.h"

namespace webrtc {

AudioSourcePcm::AudioSourcePcm(int32_t ssrc, int32_t sample_rate,
                               int32_t channel_num, int32_t frame_duration_us,
                               float volume, bool enabled)
    : AudioSource(ssrc, sample_rate, channel_num, frame_duration_us, volume,
                  volume, enabled),
      report_output_samples_(sample_rate *
                             webrtc::AudioMixerImpl::kFrameDurationInMs / 1000),
      real_buffer_num_elements_(channel_num * sample_rate * frame_duration_us /
                                1000 / 1000),
      mv_buf_(nullptr),
      mv_buf_size_(0) {
    buffer_.Clear();
}

AudioSourcePcm::~AudioSourcePcm() {
    if (mv_buf_) {
        delete[] mv_buf_;
        mv_buf_ = nullptr;
        mv_buf_size_ = 0;
    }
}

int32_t AudioSourcePcm::FrameSize() {
    return real_buffer_num_elements_ * sizeof(int16_t);
}

void AudioSourcePcm::OnAudioRecorded(const void* data, int32_t size) {
    buffer_.AppendData(static_cast<const int16_t*>(data),
                       static_cast<size_t>(size / sizeof(int16_t)));
}

webrtc::AudioMixer::Source::AudioFrameInfo
AudioSourcePcm::GetAudioFrameWithInfo(int32_t sample_rate_hz,
                                      webrtc::AudioFrame* audio_frame) {
    if (sample_rate_hz != sample_rate_) {
        return webrtc::AudioMixer::Source::AudioFrameInfo::kError;
    }
    if (!enabled() ||
        static_cast<int32_t>(buffer_.size()) < real_buffer_num_elements_) {
        return webrtc::AudioMixer::Source::AudioFrameInfo::kMuted;
    }

    audio_frame->UpdateFrame(
        0, buffer_.data(), static_cast<size_t>(report_output_samples_),
        sample_rate_, webrtc::AudioFrame::SpeechType::kNormalSpeech,
        webrtc::AudioFrame::VADActivity::kVadActive,
        static_cast<size_t>(channel_num_));

    if (real_buffer_num_elements_ < static_cast<int32_t>(buffer_.size())) {
        if (mv_buf_size_ < buffer_.size() - real_buffer_num_elements_) {
            mv_buf_size_ = (buffer_.size() - real_buffer_num_elements_) * 2;
            if (mv_buf_) {
                delete[] mv_buf_;
            }
            mv_buf_ = new int16_t[mv_buf_size_];
        }
        memset(mv_buf_, 0, mv_buf_size_ * sizeof(int16_t));
        memcpy(mv_buf_, buffer_.data() + real_buffer_num_elements_,
               (buffer_.size() - real_buffer_num_elements_) * sizeof(int16_t));
        memcpy(buffer_.data(), mv_buf_,
               (buffer_.size() - real_buffer_num_elements_) * sizeof(int16_t));
    }
    buffer_.SetSize(buffer_.size() - real_buffer_num_elements_);

    preProduceFrame(audio_frame, false);

    return muted() ? webrtc::AudioMixer::Source::AudioFrameInfo::kMuted
                   : webrtc::AudioMixer::Source::AudioFrameInfo::kNormal;
}
}
