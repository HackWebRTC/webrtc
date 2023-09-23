//
// Created by Piasy on 29/10/2017.
//

#pragma once

#include <map>

#include "api/audio/audio_mixer.h"
#include "api/scoped_refptr.h"

#include "modules/backing_track/audio_mixer_global.h"
#include "modules/backing_track/audio_source.h"
#include "modules/backing_track/audio_source_pcm.h"
#include "modules/backing_track/mixer_config.h"

namespace webrtc {

class BtAudioMixer {
public:
    BtAudioMixer(const MixerConfig& config,
                 SourceFinishCallback finish_callback,
                 SourceErrorCallback error_callback, void* callback_opaque);

    ~BtAudioMixer();

    void UpdateVolume(int32_t ssrc, float volume_left, float volume_right);

    void ToggleEnable(int32_t ssrc, bool enable);

    void ToggleStreaming(int32_t ssrc, bool streaming);

    void TogglePlayback(int32_t ssrc, bool playback);

    int64_t GetProgressMs(int32_t ssrc);

    int64_t GetLengthMs(int32_t ssrc);

    void Seek(int32_t ssrc, int64_t position_ms);

    void UpdateFrameDuration(int32_t frame_duration_us);

    void AddRawSource(AudioSource* source);

    std::shared_ptr<AudioSource> GetSource(int32_t ssrc);

    int32_t Mix(void* output_buffer);

    int32_t AddRecordedDataAndMix(const void* data, int32_t size,
                                  void* output_buffer);

    int32_t frame_duration_us() { return frame_duration_us_; }

    bool enable_music_sync_fix() { return enable_music_sync_fix_; }

private:
    std::shared_ptr<AudioSource> DoAddSource(const MixerSource& source);

    rtc::scoped_refptr<webrtc::AudioMixer> mixer_;
    std::map<int32_t, std::shared_ptr<AudioSource>> sources_;
    std::shared_ptr<AudioSourcePcm> record_source_;
    std::unique_ptr<webrtc::AudioFrame> mixed_frame_;
    int32_t output_sample_rate_;
    int32_t output_channel_num_;

    bool enable_music_sync_fix_;
    int32_t frame_duration_us_;
    int32_t report_output_samples_;
    int32_t real_output_samples_;
    int32_t waiting_mix_delay_frames_;

    SourceFinishCallback finish_callback_;
    SourceErrorCallback error_callback_;
    void* callback_opaque_;

    int64_t mixed_frames_ = 0;
};
}
