
//
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Piasy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
//


#import "CFAudioMixer.h"

#import "base/RTCLogging.h"

#include <string.h>
#include "audio/audio_transport_impl.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/backing_track/avx_helper.h"
#include "modules/backing_track/bt_audio_mixer.h"

#define TAG "CFAudioMixer"

static constexpr int32_t kRecSsrc = 9999999;
static constexpr int32_t kMusicSsrc = 9999001;

struct MixerHolder {
  webrtc::BtAudioMixer* mixer;
  int16_t* buffer;
  volatile bool stop = false;
};

static void preDeliverRecordedData(void* opaque, void* audioSamples,
                            const size_t nSamples, const size_t nBytesPerSample,
                            const size_t nChannels,
                            const uint32_t samplesPerSec) {
    MixerHolder* holder = reinterpret_cast<MixerHolder*>(opaque);
    if (holder->stop) {
        webrtc::AudioDeviceBuffer* adb = webrtc::AudioDeviceBuffer::Instance();
        if (adb) {
            webrtc::AudioTransportImpl* audio_transport =
                reinterpret_cast<webrtc::AudioTransportImpl*>(
                    adb->audio_transport());
            audio_transport->SetPreDeliverRecordedDataCallback(nullptr, nullptr);
        }

        delete holder->mixer;
        delete[] holder->buffer;
        delete holder;
    } else {
        size_t size = nSamples * nBytesPerSample * nChannels;
        holder->mixer->AddRecordedDataAndMix(audioSamples, size, holder->buffer);
        memcpy(audioSamples, holder->buffer, size);
    }
}

static void onSourceFinish(void* opaque, int32_t ssrc) {
    CFAudioMixer* mixer = CFBridgingRelease(opaque);
    [mixer onSsrcFinished:ssrc];
}

static void onSourceError(void* opaque, int32_t ssrc, int32_t code) {
    CFAudioMixer* mixer = CFBridgingRelease(opaque);
    [mixer onSsrcError:ssrc code:code];
}

@implementation CFAudioMixer {
    webrtc::BtAudioMixer* _mixer;
    MixerHolder* _holder;
    id<CFAudioMixerDelegate> _delegate;

    bool _musicEnabled;
    bool _musicStreaming;
    bool _micEcho;

    float _micVolume;
    float _musicVolume;
}

- (instancetype)initWithBackingTrack:(NSString*)backingTrack
                   captureSampleRate:(int32_t)captureSampleRate
                   captureChannelNum:(int32_t)captureChannelNum
                     frameDurationUs:(int32_t)frameDurationUs
                  enableMusicSyncFix:(bool)enableMusicSyncFix
                waitingMixDelayFrame:(int32_t)waitingMixDelayFrame
                            delegate:(id<CFAudioMixerDelegate>)delegate {
    self = [super init];
    if (self) {
        _delegate = delegate;

        webrtc::MixerConfig config(std::vector<webrtc::MixerSource>(),
                                   captureSampleRate, captureChannelNum,
                                   frameDurationUs, enableMusicSyncFix,
                                   waitingMixDelayFrame);
        config.sources.emplace_back(webrtc::MixerSource::TYPE_RECORD, kRecSsrc,
                                    1, 1, true, true, false, false, "",
                                    captureSampleRate, captureChannelNum);
        config.sources.emplace_back(webrtc::MixerSource::TYPE_FILE, kMusicSsrc,
                                    1, 1, false, false, false, false,
                                    std::string([backingTrack UTF8String]),
                                    captureSampleRate, captureChannelNum);
        _mixer = new webrtc::BtAudioMixer(config, onSourceFinish, onSourceError,
                                          (void*)CFBridgingRetain(self));

        _holder = new MixerHolder();
        _holder->mixer = _mixer;
        _holder->buffer = new int16_t[frameDurationUs * captureSampleRate /
                                      1000 * captureChannelNum];

        webrtc::AudioDeviceBuffer* adb = webrtc::AudioDeviceBuffer::Instance();
        if (adb) {
            webrtc::AudioTransportImpl* audio_transport =
                reinterpret_cast<webrtc::AudioTransportImpl*>(
                    adb->audio_transport());
            audio_transport->SetPreDeliverRecordedDataCallback(
                preDeliverRecordedData, _holder);
        }

        _musicEnabled = false;
        _musicStreaming = false;
        _micEcho = false;
        _micVolume = 1.0F;
        _musicVolume = 1.0F;
    }
    return self;
}

- (void)startMixer {
    _musicEnabled = true;
    [self applyMixerSettings];
}

- (void)pauseMixer {
    _musicEnabled = false;
    [self applyMixerSettings];
}

- (void)resumeMixer {
    [self startMixer];
}

- (void)toggleMusicStreaming:(bool)streaming {
    _musicStreaming = streaming;
    [self applyMixerSettings];
}

- (void)toggleMicEcho:(bool)micEcho {
    _micEcho = micEcho;
    [self applyMixerSettings];
}

- (void)setMicVolume:(float)volume {
    _micVolume = volume;
    [self applyMixerSettings];
}

- (void)setMusicVolume:(float)volume {
    _musicVolume = volume;
    [self applyMixerSettings];
}

- (int64_t)getMusicLengthMs {
    @synchronized(self) {
        if (!_mixer) {
            return -1;
        }
        return _mixer->GetLengthMs(kMusicSsrc);
    }
}

- (int64_t)getMusicProgressMs {
    @synchronized(self) {
        if (!_mixer) {
            return -1;
        }
        return _mixer->GetProgressMs(kMusicSsrc);
    }
}

- (void)seekMusic:(int64_t)progressMs {
    @synchronized(self) {
        if (!_mixer) {
            return;
        }
        _mixer->Seek(kMusicSsrc, progressMs);
    }
}

- (void)stopMixer {
    @synchronized(self) {
        if (_holder) {
            _holder->stop = true;
            _holder = nullptr;
        }
        _mixer = nullptr;
    }
}

- (void)onSsrcFinished:(int32_t)ssrc {
    [_delegate onSsrcFinished:ssrc];
}

- (void)onSsrcError:(int32_t)ssrc code:(int32_t)code {
    [_delegate onSsrcError:ssrc code:code];
}

- (void)dealloc {
    [self stopMixer];
}

#pragma mark - private

- (void)applyMixerSettings {
    @synchronized(self) {
        if (!_mixer) {
            return;
        }
        _mixer->ToggleEnable(kMusicSsrc, _musicEnabled);
        _mixer->TogglePlayback(kMusicSsrc, true);
        _mixer->ToggleStreaming(kMusicSsrc, _musicStreaming);
        _mixer->UpdateVolume(kMusicSsrc, _musicVolume, _musicVolume);

        _mixer->ToggleEnable(kRecSsrc, true);
        _mixer->TogglePlayback(kRecSsrc, _micEcho);
        _mixer->ToggleStreaming(kRecSsrc, true);
        _mixer->UpdateVolume(kRecSsrc, _micVolume, _micVolume);
    }
}

@end
