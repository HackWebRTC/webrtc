//
// Created by Piasy on 29/10/2017.
//

#include "audio/audio_transport_impl.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/backing_track/audio_mixer_global.h"
#include "modules/backing_track/audio_source_compressed.h"
#include "modules/backing_track/bt_audio_mixer.h"
#include "modules/backing_track/mixer_source.h"
#include "rtc_base/logging.h"

namespace webrtc {

BtAudioMixer::BtAudioMixer(const MixerConfig& config,
                           SourceFinishCallback finish_callback,
                           SourceErrorCallback error_callback,
                           void* callback_opaque)
    : mixer_(webrtc::AudioMixerImpl::Create()),
      record_source_(nullptr),
      mixed_frame_(absl::make_unique<webrtc::AudioFrame>()),
      output_sample_rate_(config.output_sample_rate),
      output_channel_num_(config.output_channel_num),
      enable_music_sync_fix_(config.enable_music_sync_fix),
      frame_duration_us_(config.frame_duration_us),
      report_output_samples_(output_sample_rate_ *
                             webrtc::AudioMixerImpl::kFrameDurationInMs / 1000),
      real_output_samples_(output_sample_rate_ * frame_duration_us_ / 1000 /
                           1000),
      waiting_mix_delay_frames_(config.waiting_mix_delay_frames),
      finish_callback_(finish_callback),
      error_callback_(error_callback),
      callback_opaque_(callback_opaque) {
    RTC_LOG(LS_INFO) << "AudioMixer create: frame_duration_us "
                     << config.frame_duration_us;

    for (auto& source : config.sources) {
        DoAddSource(source);
    }
    for (const auto& item : sources_) {
        mixer_->AddSource(item.second.get());
    }

    mixed_frame_->UpdateFrame(
        0, nullptr, static_cast<size_t>(report_output_samples_),
        output_sample_rate_, webrtc::AudioFrame::SpeechType::kUndefined,
        webrtc::AudioFrame::VADActivity::kVadUnknown,
        static_cast<size_t>(output_channel_num_));
}

BtAudioMixer::~BtAudioMixer() {
    for (const auto& item : sources_) {
        mixer_->RemoveSource(item.second.get());
    }

    sources_.clear();
}

void BtAudioMixer::UpdateVolume(int32_t ssrc, float volume_left,
                                float volume_right) {
    RTC_LOG(LS_INFO) << "BtAudioMixer::UpdateVolume " << ssrc << " "
                     << volume_left << " " << volume_right;
    auto source = sources_.find(ssrc);
    if (source != sources_.end()) {
        source->second->UpdateVolume(volume_left, volume_right);
    }
}

void BtAudioMixer::ToggleEnable(int32_t ssrc, bool enable) {
    RTC_LOG(LS_INFO) << "BtAudioMixer::ToggleEnable " << ssrc << " " << enable;
    auto source = sources_.find(ssrc);
    if (source != sources_.end()) {
        source->second->ToggleEnable(enable);
    }
}

void BtAudioMixer::ToggleStreaming(int32_t ssrc, bool streaming) {
    RTC_LOG(LS_INFO) << "BtAudioMixer::ToggleStreaming " << ssrc << " "
                     << streaming;
    std::shared_ptr<AudioSource> music = GetSource(ssrc);
    if (music) {
        // mute controls streaming, not playback (send to pcm channel, thus adb)
        music->ToggleMute(!streaming);
    }
}

void BtAudioMixer::TogglePlayback(int32_t ssrc, bool playback) {
    RTC_LOG(LS_INFO) << "BtAudioMixer::TogglePlayback " << ssrc << " "
                     << playback;
    std::shared_ptr<AudioSource> source = GetSource(ssrc);
    if (source) {
        PcmChannel* pcm_channel = source->GetPcmChannel();
        if (pcm_channel) {
            // mix controls playback, not streaming
            pcm_channel->ToggleMix(playback);
        }
    }
}

int64_t BtAudioMixer::GetProgressMs(int32_t ssrc) {
    std::shared_ptr<AudioSource> source = GetSource(ssrc);

    if (source) {
        return source->GetProgressMs();
    }

    return -2;
}

int64_t BtAudioMixer::GetLengthMs(int32_t ssrc) {
    std::shared_ptr<AudioSource> source = GetSource(ssrc);

    if (source) {
        return source->GetLengthMs();
    }

    return 0;
}

void BtAudioMixer::Seek(int32_t ssrc, int64_t position_ms) {
    std::shared_ptr<AudioSource> source = GetSource(ssrc);
    if (source) {
        source->ToggleMute(true);
        source->Seek(position_ms);
        source->ToggleMute(false);
    }
}

void BtAudioMixer::UpdateFrameDuration(int32_t frame_duration_us) {
    RTC_LOG(LS_INFO) << "BtAudioMixer::UpdateFrameDuration "
                     << frame_duration_us;

    frame_duration_us_ = frame_duration_us;
    real_output_samples_ =
        output_sample_rate_ * frame_duration_us_ / 1000 / 1000;

    for (const auto& item : sources_) {
        item.second->UpdateFrameDurationUs(frame_duration_us);
    }
}

void BtAudioMixer::AddRawSource(AudioSource* source) {
    if (source) {
        std::shared_ptr<AudioSource> ptr(source);
        sources_.emplace(std::make_pair(source->Ssrc(), ptr));
        mixer_->AddSource(source);
    }
}

std::shared_ptr<AudioSource> BtAudioMixer::GetSource(int32_t ssrc) {
    auto source = sources_.find(ssrc);
    return source == sources_.end() ? nullptr : source->second;
}

int32_t BtAudioMixer::Mix(void* output_buffer) {
    if (++mixed_frames_ % 500 == 1) {
        RTC_LOG(LS_INFO) << "BtAudioMixer::Mix " << mixed_frames_ << " times";
    }
    mixer_->Mix(static_cast<size_t>(output_channel_num_), mixed_frame_.get());

    int32_t size = real_output_samples_ *
                   av_get_bytes_per_sample(kOutputSampleFormat) *
                   output_channel_num_;
    memcpy(output_buffer, reinterpret_cast<const void*>(mixed_frame_->data()),
           static_cast<size_t>(size));
    return size;
}

int32_t BtAudioMixer::AddRecordedDataAndMix(const void* data, int32_t size,
                                            void* output_buffer) {
    if (record_source_) {
        record_source_->OnAudioRecorded(data, size);
    }

    return Mix(output_buffer);
}

std::shared_ptr<AudioSource> BtAudioMixer::DoAddSource(
    const MixerSource& source) {
    AudioTransportImpl* audio_transport = nullptr;
    AudioDeviceBuffer* adb = AudioDeviceBuffer::Instance();
    if (adb) {
        audio_transport =
            reinterpret_cast<AudioTransportImpl*>(adb->audio_transport());
    }

    if (source.type == MixerSource::TYPE_RECORD) {
        if (record_source_) {
            RTC_LOG(LS_ERROR) << "BtAudioMixer::DoAddSource error: only one "
                                 "record source is supported";
            return nullptr;
        }
        if (source.sample_rate != output_sample_rate_ ||
            source.channel_num != output_channel_num_) {
            RTC_LOG(LS_ERROR) << "BtAudioMixer::DoAddSource error: "
                                 "bad setting, sr "
                              << source.sample_rate << " ac "
                              << source.channel_num << ", output sr "
                              << output_sample_rate_ << " ac "
                              << output_channel_num_;
            return nullptr;
        }

        record_source_.reset(new AudioSourcePcm(
            source.ssrc, output_sample_rate_, output_channel_num_,
            frame_duration_us_, source.volume_left,
            true /* mic should be enabled when create */
            ));

        if (audio_transport) {
            PcmChannel* pcm_channel = new PcmChannel(
                record_source_->sample_rate(), record_source_->channel_num(),
                record_source_->frame_duration_us());
            RTC_LOG(LS_INFO) << "AudioMixerCreate rec_src "
                             << record_source_->Ssrc() << ", channel "
                             << static_cast<void*>(pcm_channel);
            audio_transport->AddPlaybackSource(pcm_channel);
            record_source_->SetPcmChannel(pcm_channel);
        }

        sources_.emplace(std::make_pair(source.ssrc, record_source_));

        return record_source_;
    } else {
        std::shared_ptr<AudioSourceCompressed> file_source =
            std::make_shared<AudioSourceCompressed>(
                source.ssrc, source.path, output_sample_rate_,
                output_channel_num_, frame_duration_us_, source.volume_left,
                source.volume_right, false /* disable when create */,
                enable_music_sync_fix_, source.remix, waiting_mix_delay_frames_,
                finish_callback_, error_callback_, callback_opaque_);

        if (audio_transport) {
            PcmChannel* pcm_channel = new PcmChannel(
                file_source->sample_rate(), file_source->input_channel_num(),
                file_source->frame_duration_us());
            RTC_LOG(LS_INFO) << "AudioMixerCreate music_src "
                             << file_source->Ssrc() << ", channel "
                             << static_cast<void*>(pcm_channel);
            audio_transport->AddPlaybackSource(pcm_channel);
            file_source->SetPcmChannel(pcm_channel);
        }

        sources_.emplace(std::make_pair(source.ssrc, file_source));

        return file_source;
    }
}

}
