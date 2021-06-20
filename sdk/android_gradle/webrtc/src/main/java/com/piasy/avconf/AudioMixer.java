package com.piasy.avconf;

/**
 * Created by Piasy{github.com/Piasy} on 2019-06-06.
 */
public class AudioMixer {
    private static final int REC_SSRC = 9999999;
    private static final int MUSIC_SSRC = 9999001;

    private long mNativeMixer;
    private boolean mMusicEnabled;
    private boolean mMusicStreaming;
    private boolean mMicEcho;
    private float mMusicVolume;
    private float mMicVolume;

    public AudioMixer(String backingTrack, int captureSampleRate, int captureChannelNum,
            int frameDurationUs, boolean enableMusicSyncFix, int waitingMixDelayFrame,
            MixerCallback callback) {
        mNativeMixer = nativeCreate(MUSIC_SSRC, backingTrack, REC_SSRC, captureSampleRate,
                captureChannelNum, frameDurationUs, enableMusicSyncFix, waitingMixDelayFrame,
                callback);

        mMusicEnabled = false;
        mMusicStreaming = false;
        mMicEcho = false;
        mMusicVolume = 1.0F;
        mMicVolume = 1.0F;
    }

    private static native long nativeCreate(int musicSsrc, String backingTrack, int recSsrc,
            int captureSampleRate, int captureChannelNum, int frameDurationUs,
            boolean enableMusicSyncFix, int waitingMixDelayFrame, MixerCallback callback);

    private static native void nativeToggleEnable(long nativeMixer, int ssrc, boolean enable);

    private static native void nativeToggleStreaming(long nativeMixer, int ssrc, boolean streaming);

    private static native void nativeTogglePlayback(long nativeMixer, int ssrc, boolean playback);

    private static native void nativeUpdateVolume(long nativeMixer, int ssrc, float volume);

    private static native long nativeGetLengthMs(long nativeMixer, int ssrc);

    private static native long nativeGetProgressMs(long nativeMixer, int ssrc);

    private static native long nativeSeek(long nativeMixer, int ssrc, long progressMs);

    private static native void nativeDestroy(long nativeMixer);

    public void startMixer() {
        mMusicEnabled = true;
        applyMixerSettings();
    }

    public void pauseMixer() {
        mMusicEnabled = false;
        applyMixerSettings();
    }

    public void resumeMixer() {
        startMixer();
    }

    public void toggleMusicStreaming(boolean streaming) {
        mMusicStreaming = streaming;
        applyMixerSettings();
    }

    public void toggleMicEcho(boolean micEcho) {
        mMicEcho = micEcho;
        applyMixerSettings();
    }

    public void setMicVolume(float volume) {
        mMicVolume = volume;
        applyMixerSettings();
    }

    public void setMusicVolume(float volume) {
        mMusicVolume = volume;
        applyMixerSettings();
    }

    public synchronized long getMusicLengthMs() {
        if (mNativeMixer == 0) {
            return -1;
        }

        return nativeGetLengthMs(mNativeMixer, MUSIC_SSRC);
    }

    public synchronized long getMusicProgressMs() {
        if (mNativeMixer == 0) {
            return -1;
        }

        return nativeGetProgressMs(mNativeMixer, MUSIC_SSRC);
    }

    public synchronized void seekMusic(long progressMs) {
        if (mNativeMixer == 0) {
            return;
        }

        nativeSeek(mNativeMixer, MUSIC_SSRC, progressMs);
    }

    public synchronized void stopMixer() {
        if (mNativeMixer == 0) {
            return;
        }

        nativeDestroy(mNativeMixer);
        mNativeMixer = 0;
    }

    private synchronized void applyMixerSettings() {
        if (mNativeMixer == 0) {
            return;
        }

        nativeToggleEnable(mNativeMixer, MUSIC_SSRC, mMusicEnabled);
        nativeTogglePlayback(mNativeMixer, MUSIC_SSRC, true);
        nativeToggleStreaming(mNativeMixer, MUSIC_SSRC, mMusicStreaming);
        nativeUpdateVolume(mNativeMixer, MUSIC_SSRC, mMusicVolume);

        nativeToggleEnable(mNativeMixer, REC_SSRC, true);
        nativeTogglePlayback(mNativeMixer, REC_SSRC, mMicEcho);
        nativeToggleStreaming(mNativeMixer, REC_SSRC, true);
        nativeUpdateVolume(mNativeMixer, REC_SSRC, mMicVolume);
    }

    public interface MixerCallback {
        void onMixerSsrcFinished(int ssrc);

        void onMixerSsrcError(int ssrc, int code);
    }
}
