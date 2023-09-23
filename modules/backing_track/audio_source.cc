//
// Created by Piasy on 2018/5/28.
//

#include <chrono>

#include "audio/audio_transport_impl.h"
#include "audio/utility/audio_frame_operations.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/backing_track/audio_source.h"

namespace webrtc {

AudioSource::AudioSource(int32_t ssrc, int32_t sample_rate, int32_t channel_num,
                         int32_t frame_duration_us, float volume_left,
                         float volume_right, bool enabled)
    : ssrc_(ssrc),
      sample_rate_(sample_rate),
      channel_num_(channel_num),
      frame_duration_us_(frame_duration_us),
      pcm_channel_(nullptr),
      volume_left_(volume_left),
      volume_right_(volume_right),
      enabled_(enabled),
      muted_(false) {}

AudioSource::~AudioSource() {
    MutexLock lock(&mutex_);
    AudioTransportImpl* audio_transport = nullptr;
    AudioDeviceBuffer* adb = AudioDeviceBuffer::Instance();
    if (adb) {
        audio_transport =
            reinterpret_cast<AudioTransportImpl*>(adb->audio_transport());
    }
    if (pcm_channel_) {
        if (audio_transport) {
            audio_transport->RemovePlaybackSource(pcm_channel_);
        } else {
            delete pcm_channel_;
        }

        pcm_channel_ = nullptr;
    }
}

void AudioSource::SetPcmChannel(PcmChannel* pcm_channel) {
    MutexLock lock(&mutex_);

    pcm_channel_ = pcm_channel;
}

void AudioSource::preProduceFrame(webrtc::AudioFrame* frame, bool remix) {
    if (StereoInput()) {
        if ((volume_left_ < 0.99f || volume_left_ > 1.01f) ||
            (volume_right_ < 0.99f || volume_right_ > 1.01f)) {
            webrtc::AudioFrameOperations::Scale(volume_left_, volume_right_,
                                                frame);
        }
    } else {
        if ((volume_left_ < 0.99f || volume_left_ > 1.01f)) {
            webrtc::AudioFrameOperations::ScaleWithSat(volume_left_, frame);
        }
    }

    if (remix && frame->num_channels_ == 2) {
        webrtc::AudioFrameOperations::DownmixChannels(1, frame);
        webrtc::AudioFrameOperations::UpmixChannels(2, frame);
    }

    MutexLock lock(&mutex_);
    if (pcm_channel_) {
        pcm_channel_->FeedData(frame->data(), FrameSize());
    }
}

int64_t AudioSource::GetTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

}
