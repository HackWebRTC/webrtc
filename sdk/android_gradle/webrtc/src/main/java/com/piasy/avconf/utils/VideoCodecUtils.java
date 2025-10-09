package com.piasy.avconf.utils;

import java.util.Map;

/**
 * Created by Piasy{github.com/Piasy} on 2025/04/03.
 */
public class VideoCodecUtils {
    private VideoCodecUtils() {
    }

    public static boolean isSameH264Profile(Map<String, String> aParams,
                                            Map<String, String> bParams) {
        return nativeIsSameH264Profile(aParams, bParams);
    }

    public static String H264GenerateProfileLevelIdForAnswer(Map<String, String> aParams,
                                                             Map<String, String> bParams) {
        return nativeH264GenerateProfileLevelIdForAnswer(aParams, bParams);
    }

    private static native boolean nativeIsSameH264Profile(
            Map<String, String> aParams,
            Map<String, String> bParams);

    private static native String nativeH264GenerateProfileLevelIdForAnswer(
            Map<String, String> aParams,
            Map<String, String> bParams);
}
