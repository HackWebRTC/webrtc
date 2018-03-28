/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import org.webrtc.audio.WebRtcAudioManager;
import org.webrtc.audio.WebRtcAudioRecord;
import org.webrtc.audio.WebRtcAudioTrack;
import org.webrtc.audio.WebRtcAudioUtils;

/**
 * Public API for Java audio methods.
 *
 * <p>Note: This class is still under development and may change without notice.
 */
public class AudioDeviceModule {
  public AudioDeviceModule() {}

  /* AudioManager */
  public static void setStereoInput(boolean enable) {
    WebRtcAudioManager.setStereoInput(enable);
  }

  /* AudioRecord */
  // Audio recording error handler functions.
  public enum AudioRecordStartErrorCode {
    AUDIO_RECORD_START_EXCEPTION,
    AUDIO_RECORD_START_STATE_MISMATCH,
  }

  public static interface AudioRecordErrorCallback {
    void onWebRtcAudioRecordInitError(String errorMessage);
    void onWebRtcAudioRecordStartError(AudioRecordStartErrorCode errorCode, String errorMessage);
    void onWebRtcAudioRecordError(String errorMessage);
  }

  /**
   * Contains audio sample information.
   */
  public static class AudioSamples {
    /** See {@link AudioRecord#getAudioFormat()} */
    private final int audioFormat;
    /** See {@link AudioRecord#getChannelCount()} */
    private final int channelCount;
    /** See {@link AudioRecord#getSampleRate()} */
    private final int sampleRate;

    private final byte[] data;

    public AudioSamples(int audioFormat, int channelCount, int sampleRate, byte[] data) {
      this.audioFormat = audioFormat;
      this.channelCount = channelCount;
      this.sampleRate = sampleRate;
      this.data = data;
    }

    public int getAudioFormat() {
      return audioFormat;
    }

    public int getChannelCount() {
      return channelCount;
    }

    public int getSampleRate() {
      return sampleRate;
    }

    public byte[] getData() {
      return data;
    }
  }

  /** Called when new audio samples are ready. This should only be set for debug purposes */
  public static interface SamplesReadyCallback {
    void onWebRtcAudioRecordSamplesReady(AudioSamples samples);
  }

  public static void setErrorCallback(AudioRecordErrorCallback errorCallback) {
    WebRtcAudioRecord.setErrorCallback(errorCallback);
  }

  public static void setOnAudioSamplesReady(SamplesReadyCallback callback) {
    WebRtcAudioRecord.setOnAudioSamplesReady(callback);
  }

  /* AudioTrack */
  // Audio playout/track error handler functions.
  public enum AudioTrackStartErrorCode {
    AUDIO_TRACK_START_EXCEPTION,
    AUDIO_TRACK_START_STATE_MISMATCH,
  }
  public static interface AudioTrackErrorCallback {
    void onWebRtcAudioTrackInitError(String errorMessage);
    void onWebRtcAudioTrackStartError(AudioTrackStartErrorCode errorCode, String errorMessage);
    void onWebRtcAudioTrackError(String errorMessage);
  }

  public static void setErrorCallback(AudioTrackErrorCallback errorCallback) {
    WebRtcAudioTrack.setErrorCallback(errorCallback);
  }

  /* AudioUtils */
  public static void setWebRtcBasedAcousticEchoCanceler(boolean enable) {
    WebRtcAudioUtils.setWebRtcBasedAcousticEchoCanceler(enable);
  }

  public static void setWebRtcBasedNoiseSuppressor(boolean enable) {
    WebRtcAudioUtils.setWebRtcBasedNoiseSuppressor(enable);
  }

  // Returns true if the device supports an audio effect (AEC or NS).
  // Four conditions must be fulfilled if functions are to return true:
  // 1) the platform must support the built-in (HW) effect,
  // 2) explicit use (override) of a WebRTC based version must not be set,
  // 3) the device must not be blacklisted for use of the effect, and
  // 4) the UUID of the effect must be approved (some UUIDs can be excluded).
  public static boolean isAcousticEchoCancelerSupported() {
    return WebRtcAudioEffects.canUseAcousticEchoCanceler();
  }
  public static boolean isNoiseSuppressorSupported() {
    return WebRtcAudioEffects.canUseNoiseSuppressor();
  }

  // Call this method if the default handling of querying the native sample
  // rate shall be overridden. Can be useful on some devices where the
  // available Android APIs are known to return invalid results.
  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  public static void setDefaultSampleRateHz(int sampleRateHz) {
    WebRtcAudioUtils.setDefaultSampleRateHz(sampleRateHz);
  }
}
