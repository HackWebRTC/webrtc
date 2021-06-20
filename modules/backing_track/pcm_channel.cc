//
// Created by Piasy on 29/10/2017.
//

#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/backing_track/pcm_channel.h"
#include "rtc_base/logging.h"

namespace webrtc {

PcmChannel::PcmChannel(int32_t sample_rate, int32_t channel_num,
                       int32_t frame_duration_us)
    : ssrc_(0),
      sample_rate_(sample_rate),
      channel_num_(channel_num),
      report_output_samples_(sample_rate *
                             webrtc::AudioMixerImpl::kFrameDurationInMs / 1000),
      real_buffer_num_elements_(channel_num * sample_rate * frame_duration_us /
                                1000 / 1000),
      enabled_(false),
      mv_buf_(nullptr),
      mv_buf_size_(0) {}

PcmChannel::~PcmChannel() {
    if (mv_buf_) {
        delete[] mv_buf_;
        mv_buf_ = nullptr;
        mv_buf_size_ = 0;
    }
}

void PcmChannel::FeedData(const void* data, int32_t size) {
    if (channel_num_ <= 0 || sample_rate_ <= 0) {
        return;
    }

    MutexLock lock(&mutex_);

    if (enabled_.load()) {
        buffer_.AppendData(static_cast<const int16_t*>(data),
                           size / sizeof(int16_t));
    } else {
        buffer_.Clear();
    }
}

webrtc::AudioMixer::Source::AudioFrameInfo PcmChannel::GetAudioFrameWithInfo(
    int32_t sample_rate_hz, webrtc::AudioFrame* audio_frame) {
    if (channel_num_ <= 0 || sample_rate_ <= 0) {
        return webrtc::AudioMixer::Source::AudioFrameInfo::kError;
    }

    MutexLock lock(&mutex_);

    int32_t real_buffer_num_elements = real_buffer_num_elements_.load();
    if (!enabled_.load() ||
        static_cast<int32_t>(buffer_.size()) < real_buffer_num_elements) {
        return webrtc::AudioMixer::Source::AudioFrameInfo::kMuted;
    }

    audio_frame->UpdateFrame(
        0, nullptr, static_cast<size_t>(report_output_samples_), sample_rate_,
        webrtc::AudioFrame::SpeechType::kNormalSpeech,
        webrtc::AudioFrame::VADActivity::kVadActive,
        static_cast<size_t>(channel_num_));
    memcpy(audio_frame->mutable_data(), buffer_.data(),
           real_buffer_num_elements * sizeof(int16_t));

    if (real_buffer_num_elements < static_cast<int32_t>(buffer_.size())) {
        if (mv_buf_size_ < buffer_.size() - real_buffer_num_elements) {
            // alloc double size
            mv_buf_size_ = (buffer_.size() - real_buffer_num_elements) * 2;
            if (mv_buf_) {
                delete[] mv_buf_;
            }
            mv_buf_ = new int16_t[mv_buf_size_];
        }
        memset(mv_buf_, 0, mv_buf_size_ * sizeof(int16_t));
        memcpy(mv_buf_, buffer_.data() + real_buffer_num_elements,
               (buffer_.size() - real_buffer_num_elements) * sizeof(int16_t));
        memcpy(buffer_.data(), mv_buf_,
               (buffer_.size() - real_buffer_num_elements) * sizeof(int16_t));
    }
    buffer_.SetSize(buffer_.size() - real_buffer_num_elements);

    return webrtc::AudioMixer::Source::AudioFrameInfo::kNormal;
}

int32_t PcmChannel::Ssrc() const { return ssrc_; }

int32_t PcmChannel::PreferredSampleRate() const { return sample_rate_; }

void PcmChannel::ToggleMix(bool enable) {
    RTC_LOG(LS_INFO) << "PcmChannel(" << static_cast<void*>(this)
                     << ") ToggleMix " << enable;
    enabled_ = enable;
}

void PcmChannel::SetFrameDurationUs(int32_t frame_duration_us) {
    if (channel_num_ <= 0 || sample_rate_ <= 0) {
        return;
    }

    RTC_LOG(LS_INFO) << "SetFrameDurationUs(" << static_cast<void*>(this)
                     << ") SetFrameDurationUs " << frame_duration_us;
    real_buffer_num_elements_ =
        channel_num_ * sample_rate_ * frame_duration_us / 1000 / 1000;
}
}
