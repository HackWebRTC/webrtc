package com.piasy.avconf.utils;

import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Created by Piasy{github.com/Piasy} on 2019-05-29.
 */
public class VideoEncoderError {
    public static final int ERR_START = 1;
    public static final int ERR_ENCODE = 2;

    private static final CopyOnWriteArrayList<Callback> sCallbacks = new CopyOnWriteArrayList<>();

    public static void register(Callback callback) {
        sCallbacks.add(callback);
    }

    public static void unregister(Callback callback) {
        sCallbacks.remove(callback);
    }

    public static void onError(int code) {
        for (Callback callback : sCallbacks) {
            callback.onVideoEncoderError(code);
        }
    }

    public interface Callback {
        void onVideoEncoderError(int code);
    }
}
