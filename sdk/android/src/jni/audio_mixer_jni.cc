//
// Created by Piasy on 06/06/2019.
//

#include <jni.h>
#include <string.h>

#include "audio/audio_transport_impl.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/backing_track/avx_helper.h"
#include "modules/backing_track/bt_audio_mixer.h"

extern "C" {

struct MixerHolder {
  webrtc::BtAudioMixer* mixer;
  int16_t* buffer;
  volatile bool stop = false;
  jobject callback;
  jmethodID onSsrcFinished;
  jmethodID onSsrcError;
};

static JavaVM* g_vm = nullptr;

static int enterJava(JNIEnv** env) {
    int getEnvStat = g_vm->GetEnv((void**) env, JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
        g_vm->AttachCurrentThread(env, NULL);
    } else if (getEnvStat != JNI_OK) {
        return getEnvStat;
    }

    return getEnvStat;
}

static void leaveJava(int getEnvStat) {
    if (getEnvStat == JNI_EDETACHED) {
        g_vm->DetachCurrentThread();
    }
}

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
    MixerHolder* holder = reinterpret_cast<MixerHolder*>(opaque);
    JNIEnv* env = nullptr;
    int stat = enterJava(&env);

    if (env && holder && holder->callback && holder->onSsrcFinished) {
        env->CallVoidMethod(holder->callback, holder->onSsrcFinished, ssrc);
    }

    leaveJava(stat);
}

static void onSourceError(void* opaque, int32_t ssrc, int32_t code) {
    MixerHolder* holder = reinterpret_cast<MixerHolder*>(opaque);
    JNIEnv* env = nullptr;
    int stat = enterJava(&env);

    if (env && holder && holder->callback && holder->onSsrcError) {
        env->CallVoidMethod(holder->callback, holder->onSsrcError, ssrc, code);
    }

    leaveJava(stat);
}

JNIEXPORT jlong JNICALL Java_com_piasy_avconf_AudioMixer_nativeCreate(
    JNIEnv* env, jclass, jint musicSsrc, jstring backingTrack_, jint recSsrc,
    jint captureSampleRate, jint captureChannelNum, jint frameDurationUs,
    jboolean enableMusicSyncFix, jint waiting_mix_delay_frames,
    jobject callback) {
    env->GetJavaVM(&g_vm);

    const char* backingTrack = env->GetStringUTFChars(backingTrack_, 0);

    MixerHolder* holder = new MixerHolder();
    holder->callback = env->NewGlobalRef(callback);
    jclass clazz = env->FindClass("com/piasy/avconf/AudioMixer$MixerCallback");
    holder->onSsrcFinished =
        env->GetMethodID(clazz, "onMixerSsrcFinished", "(I)V");
    holder->onSsrcError = env->GetMethodID(clazz, "onMixerSsrcError", "(II)V");

    webrtc::MixerConfig config(std::vector<webrtc::MixerSource>(),
                               captureSampleRate, captureChannelNum,
                               frameDurationUs, enableMusicSyncFix,
                               waiting_mix_delay_frames);
    config.sources.emplace_back(webrtc::MixerSource::TYPE_RECORD, recSsrc, 1,
                                1, true, true, false, false, "",
                                captureSampleRate, captureChannelNum);
    config.sources.emplace_back(
        webrtc::MixerSource::TYPE_FILE, musicSsrc, 1, 1, false, false, false,
        false, std::string(backingTrack), captureSampleRate, captureChannelNum);
    webrtc::BtAudioMixer* mixer = new webrtc::BtAudioMixer(
        config, onSourceFinish, onSourceError, holder);

    holder->mixer = mixer;
    holder->buffer = new int16_t[frameDurationUs * captureSampleRate /
                                  1000 * captureChannelNum];

    webrtc::AudioDeviceBuffer* adb = webrtc::AudioDeviceBuffer::Instance();
    if (adb) {
        webrtc::AudioTransportImpl* audio_transport =
            reinterpret_cast<webrtc::AudioTransportImpl*>(
                adb->audio_transport());
        audio_transport->SetPreDeliverRecordedDataCallback(
            preDeliverRecordedData, holder);
    }

    env->ReleaseStringUTFChars(backingTrack_, backingTrack);

    return reinterpret_cast<jlong>(holder);
}

JNIEXPORT void JNICALL Java_com_piasy_avconf_AudioMixer_nativeToggleEnable(
    JNIEnv*, jclass, jlong nativeMixer, jint ssrc, jboolean enable) {
    reinterpret_cast<MixerHolder*>(nativeMixer)->mixer
        ->ToggleEnable(ssrc, enable);
}

JNIEXPORT void JNICALL Java_com_piasy_avconf_AudioMixer_nativeToggleStreaming(
    JNIEnv*, jclass, jlong nativeMixer, jint ssrc, jboolean streaming) {
    reinterpret_cast<MixerHolder*>(nativeMixer)->mixer
        ->ToggleStreaming(ssrc, streaming);
}

JNIEXPORT void JNICALL Java_com_piasy_avconf_AudioMixer_nativeTogglePlayback(
    JNIEnv*, jclass, jlong nativeMixer, jint ssrc, jboolean playback) {
    reinterpret_cast<MixerHolder*>(nativeMixer)->mixer
        ->TogglePlayback(ssrc, playback);
}

JNIEXPORT void JNICALL Java_com_piasy_avconf_AudioMixer_nativeUpdateVolume(
    JNIEnv*, jclass, jlong nativeMixer, jint ssrc, jfloat volume) {
    reinterpret_cast<MixerHolder*>(nativeMixer)->mixer
        ->UpdateVolume(ssrc, volume, volume);
}

JNIEXPORT jlong JNICALL Java_com_piasy_avconf_AudioMixer_nativeGetLengthMs(
    JNIEnv*, jclass, jlong nativeMixer, jint ssrc) {
    return reinterpret_cast<MixerHolder*>(nativeMixer)->mixer
        ->GetLengthMs(ssrc);
}

JNIEXPORT jlong JNICALL Java_com_piasy_avconf_AudioMixer_nativeGetProgressMs(
    JNIEnv*, jclass, jlong nativeMixer, jint ssrc) {
    return reinterpret_cast<MixerHolder*>(nativeMixer)->mixer
        ->GetProgressMs(ssrc);
}

JNIEXPORT void JNICALL Java_com_piasy_avconf_AudioMixer_nativeSeek(
    JNIEnv*, jclass, jlong nativeMixer, jint ssrc, jlong progressMs) {
    reinterpret_cast<MixerHolder*>(nativeMixer)->mixer
        ->Seek(ssrc, progressMs);
}

JNIEXPORT void JNICALL Java_com_piasy_avconf_AudioMixer_nativeDestroy(
    JNIEnv*, jclass, jlong nativeMixer) {
    MixerHolder* holder = reinterpret_cast<MixerHolder*>(nativeMixer);
    holder->stop = true;
}

}
