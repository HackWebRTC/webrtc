package com.piasy.avconf.utils;

import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Created by Piasy{github.com/Piasy} on 2019-07-18.
 */
public class AudioDeviceModuleError {
    public static final int ERR_INIT = 1;
    public static final int ERR_START = 2;
    public static final int ERR_RUN = 3;

    private static final CopyOnWriteArrayList<Callback> sCallbacks = new CopyOnWriteArrayList<>();

    public static void register(Callback callback) {
        sCallbacks.add(callback);
    }

    public static void unregister(Callback callback) {
        sCallbacks.remove(callback);
    }

    public static void onAudioRecordError(int code) {
        for (Callback callback : sCallbacks) {
            callback.onAudioRecordError(code);
        }
    }

    public static void onAudioTrackError(int code) {
        for (Callback callback : sCallbacks) {
            callback.onAudioTrackError(code);
        }
    }

    public interface Callback {
        void onAudioRecordError(int code);

        void onAudioTrackError(int code);
    }
}
