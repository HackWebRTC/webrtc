//
// Created by Piasy on 29/10/2017.
//

#include <chrono>
#include <numeric>

#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/backing_track/audio_mixer_global.h"
#include "modules/backing_track/audio_source_compressed.h"
#include "rtc_base/logging.h"

namespace webrtc {

static constexpr int32_t kOnceDecodeDurationMs = 10;

int gcd(int a, int b) {
    for (;;) {
        if (a == 0) return b;
        b %= a;
        if (b == 0) return a;
        a %= b;
    }
}

int lcm(int a, int b) {
    int temp = gcd(a, b);

    return temp ? (a / temp * b) : 0;
}

AudioSourceCompressed::AudioSourceCompressed(
    int32_t ssrc, const std::string& filepath, int32_t output_sample_rate,
    int32_t output_channel_num, int32_t frame_duration_us, float volume_left,
    float volume_right, bool enabled, bool enable_sync_fix, bool remix,
    int32_t waiting_mix_delay_frames, SourceFinishCallback finish_callback,
    SourceErrorCallback error_callback, void* callback_opaque)
    : AudioSource(ssrc, output_sample_rate, output_channel_num,
                  frame_duration_us, volume_left, volume_right, enabled),
      report_output_samples_(output_sample_rate *
                             webrtc::AudioMixerImpl::kFrameDurationInMs / 1000),
      real_output_samples_(output_sample_rate * frame_duration_us / 1000 /
                           1000),
      enable_sync_fix_(enable_sync_fix),
      sync_fix_threshold_ms_(20),
      sync_fix_break_times_(0),
      remix_(remix),
      input_buffer_(nullptr),
      waiting_mix_delay_frames_(waiting_mix_delay_frames),
      start_time_(0),
      samples_mixed_(0),
      first_frame_decoded_(false),
      finish_callback_(finish_callback),
      error_callback_(error_callback),
      callback_opaque_(callback_opaque),
      finish_callback_fired_(false),
      error_callback_fired_(false) {
    AudioDeviceBuffer* adb = AudioDeviceBuffer::Instance();
    if (!adb) {
        return;
    }
    decoder_.reset(new AudioFileDecoder(adb->task_queue_factory(), filepath));

    input_sample_rate_ = decoder_->sample_rate();
    input_channel_num_ = decoder_->channel_num();
    if (input_sample_rate_ <= 0 || input_channel_num_ <= 0
        || sample_rate_ <= 0) {
       return;
    }
    input_format_ = decoder_->sample_format();

    int32_t once_decode_us =
        lcm(kOnceDecodeDurationMs * 1000, frame_duration_us);
    once_decode_samples_ = input_sample_rate_ * once_decode_us / 1000 / 1000;
    buffer_.SetSize(static_cast<size_t>(input_channel_num_ * sample_rate_ *
                                        once_decode_us / 1000 / 1000));
    buffer_pos_ = static_cast<int32_t>(buffer_.size());

    // to support adjust volume of channels separately, resampler shouldn't
    // remix, but let mixer to remix
    resampler_.reset(new AudioResampler(input_format_, input_sample_rate_,
                                        input_channel_num_, kOutputSampleFormat,
                                        sample_rate_, input_channel_num_));

    int32_t error = av_samples_alloc_array_and_samples(
        reinterpret_cast<uint8_t***>(&input_buffer_), nullptr,
        input_channel_num_, once_decode_samples_, input_format_, 0);
    if (error < 0) {
        input_buffer_ = nullptr;
        RTC_LOG(LS_ERROR)
            << "AudioSourceCompressed:: alloc decode buffer fail: "
            << av_err2str(error);
    }
}

AudioSourceCompressed::~AudioSourceCompressed() {
    if (input_buffer_) {
        av_freep(&input_buffer_[0]);
    }
    av_freep(&input_buffer_);
}

void AudioSourceCompressed::ToggleEnable(bool enabled) {
    AudioSource::ToggleEnable(enabled);

    start_time_ = 0;
    samples_mixed_ = 0;
}

int32_t AudioSourceCompressed::FrameSize() {
    return real_output_samples_ * input_channel_num_ * sizeof(int16_t);
}

void AudioSourceCompressed::Seek(int64_t position_ms) {
    if (decoder_) {
        decoder_->Seek(position_ms);
    }
}

webrtc::AudioMixer::Source::AudioFrameInfo
AudioSourceCompressed::GetAudioFrameWithInfo(int32_t sample_rate_hz,
                                             webrtc::AudioFrame* audio_frame) {
    if (sample_rate_hz != sample_rate_ || finish_callback_fired_ ||
        error_callback_fired_ || frame_duration_us_ <= 0 || sample_rate_ <= 0
        || input_channel_num_ <= 0) {
        RTC_LOG(LS_INFO)
            << "AudioSourceCompressed::GetAudioFrameWithInfo wrong state "
            << sample_rate_hz << " !=? " << sample_rate_
            << ", frame_duration_us_ " << frame_duration_us_
            << ", input_channel_num_ " << input_channel_num_;
        fireErrorCallback(-999);
        return webrtc::AudioMixer::Source::AudioFrameInfo::kError;
    }

    if (!enabled()) {
        return webrtc::AudioMixer::Source::AudioFrameInfo::kMuted;
    }

    int64_t now = GetTimestamp();
    if (start_time_ == 0) {
        start_time_ = now;
    }
    int64_t time_elapsed = now - start_time_;
    int64_t data_duration = 1000 * samples_mixed_ / sample_rate_;

    if (enable_sync_fix_ &&
        data_duration - time_elapsed > sync_fix_threshold_ms_) {
        sync_fix_break_times_ = static_cast<int32_t>(
            (data_duration - time_elapsed) * 1000 / frame_duration_us_);
        RTC_LOG(LS_INFO)
            << "AudioSourceCompressed::GetAudioFrameWithInfo consume too fast, "
               "take "
            << sync_fix_break_times_ << " break";
    } else if (time_elapsed >= data_duration) {
        if (sync_fix_break_times_ > 0) {
            RTC_LOG(LS_INFO)
                << "AudioSourceCompressed::GetAudioFrameWithInfo consume stop "
                   "break early, "
                << sync_fix_break_times_ << " left";
        }
        sync_fix_break_times_ = 0;
    }
    if (sync_fix_break_times_ > 0) {
        sync_fix_break_times_--;
        return webrtc::AudioMixer::Source::AudioFrameInfo::kMuted;
    }

    audio_frame->UpdateFrame(
        0, nullptr, static_cast<size_t>(report_output_samples_), sample_rate_,
        webrtc::AudioFrame::SpeechType::kNormalSpeech,
        webrtc::AudioFrame::VADActivity::kVadActive,
        static_cast<size_t>(input_channel_num_));

    int16_t* output_buffer = audio_frame->mutable_data();
    bool sync_fix_hurry = false;
    if (enable_sync_fix_ &&
        time_elapsed - data_duration > sync_fix_threshold_ms_) {
        sync_fix_hurry = true;
    }
    int32_t read_count = 0;
    do {
        int32_t read = Read(reinterpret_cast<void**>(&output_buffer));
        read_count++;
        if (read < 0) {
            if (read == kMixerErrEof) {
                if (!finish_callback_fired_ && finish_callback_) {
                    RTC_LOG(LS_INFO)
                        << "AudioSourceCompressed::GetAudioFrameWithInfo music "
                           "finished "
                        << ssrc_;
                    finish_callback_fired_ = true;
                    finish_callback_(callback_opaque_, ssrc_);
                }
            } else {
                RTC_LOG(LS_INFO)
                    << "AudioSourceCompressed::GetAudioFrameWithInfo music "
                       "error "
                    << ssrc_ << ", code " << read;
                fireErrorCallback(read);
            }
            return webrtc::AudioMixer::Source::AudioFrameInfo::kError;
        } else if (read == 0) {
            break;
        }
        samples_mixed_ += read / input_channel_num_ / sizeof(int16_t);
        data_duration = 1000 * samples_mixed_ / sample_rate_;
    } while (sync_fix_hurry && GetTimestamp() - start_time_ > data_duration);

    if (read_count > 1) {
        RTC_LOG(LS_INFO)
            << "AudioSourceCompressed::GetAudioFrameWithInfo consume too slow, "
               "hurry up "
            << read_count;
    }

    preProduceFrame(audio_frame, remix_);

    if (waiting_mix_delay_frames_ > 0) {
        checkInitWaitingMixQueue();

        size_t frame_size =
            report_output_samples_ * input_channel_num_ * sizeof(int16_t);
        waiting_mix_->WriteBack(output_buffer, frame_size, nullptr);
        waiting_mix_->ReadFront(output_buffer, frame_size, nullptr);
    }

    return muted() ? webrtc::AudioMixer::Source::AudioFrameInfo::kMuted
                   : webrtc::AudioMixer::Source::AudioFrameInfo::kNormal;
}

int32_t AudioSourceCompressed::input_sample_rate() {
    return input_sample_rate_;
}

int32_t AudioSourceCompressed::input_channel_num() {
    return input_channel_num_;
}

int32_t AudioSourceCompressed::Read(void** buffer) {
    if (!input_buffer_) {
        return kMixerErrInit;
    }

    if (static_cast<int32_t>(buffer_.size() - buffer_pos_) >=
        input_channel_num_ * real_output_samples_) {
        int32_t read_size =
            input_channel_num_ * real_output_samples_ * sizeof(int16_t);
        memcpy(*buffer, buffer_.data() + buffer_pos_,
               static_cast<size_t>(read_size));
        buffer_pos_ += input_channel_num_ * real_output_samples_;
        return read_size;
    }

    int32_t consumed = decoder_->Consume(input_buffer_, once_decode_samples_);
    if (consumed !=
        once_decode_samples_ * av_get_bytes_per_sample(input_format_) *
            input_channel_num_) {
        memset(*buffer, 0,
               input_channel_num_ * real_output_samples_ * sizeof(int16_t));
        if (decoder_->eof()) {
            return kMixerErrEof;
        } else if (consumed < 0) {
            return consumed;
        } else {
            return resampler_->CalcOutputSize(consumed);
        }
    }

    void* buf = buffer_.data();
    int32_t resampled = resampler_->Resample(input_buffer_, consumed, &buf);
    if (resampled < 0) {
        return resampled;
    }
    if (!first_frame_decoded_) {
        first_frame_decoded_ = true;
        memset(buf, 0, buffer_.size() * sizeof(int16_t));
    }

    buffer_pos_ = 0;
    return Read(buffer);
}

void AudioSourceCompressed::checkInitWaitingMixQueue() {
    if (waiting_mix_) {
        return;
    }
    waiting_mix_.reset(new rtc::BufferQueue(waiting_mix_delay_frames_ * 2,
                                            report_output_samples_ * sizeof(int16_t)));

    size_t frame_size =
        report_output_samples_ * input_channel_num_ * sizeof(int16_t);
    int8_t* delay_buffer = new int8_t[frame_size];
    memset(delay_buffer, 0, frame_size);
    for (int32_t i = 0; i < waiting_mix_delay_frames_; i++) {
        waiting_mix_->WriteBack(delay_buffer, frame_size, nullptr);
    }
    delete[] delay_buffer;
}

void AudioSourceCompressed::fireErrorCallback(int32_t code) {
    if (!error_callback_fired_ && error_callback_) {
        error_callback_fired_ = true;
        error_callback_(callback_opaque_, ssrc_, code);
    }
}
}
