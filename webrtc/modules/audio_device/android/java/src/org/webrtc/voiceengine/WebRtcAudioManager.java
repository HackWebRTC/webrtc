/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.voiceengine;

import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.os.Build;
import android.util.Log;

import java.lang.Math;

// WebRtcAudioManager handles tasks that uses android.media.AudioManager.
// At construction, storeAudioParameters() is called and it retrieves
// fundamental audio parameters like native sample rate and number of channels.
// The result is then provided to the caller by nativeCacheAudioParameters().
// It is also possible to call init() to set up the audio environment for best
// possible "VoIP performance". All settings done in init() are reverted by
// dispose(). This class can also be used without calling init() if the user
// prefers to set up the audio environment separately. However, it is
// recommended to always use AudioManager.MODE_IN_COMMUNICATION.
// This class also adds support for output volume control of the
// STREAM_VOICE_CALL-type stream.
class WebRtcAudioManager {
  private static final boolean DEBUG = false;

  private static final String TAG = "WebRtcAudioManager";

  // Default audio data format is PCM 16 bit per sample.
  // Guaranteed to be supported by all devices.
  private static final int BITS_PER_SAMPLE = 16;

   // Use 44.1kHz as the default sampling rate.
  private static final int SAMPLE_RATE_HZ = 44100;

  // TODO(henrika): add stereo support for playout.
  private static final int CHANNELS = 1;

  // List of possible audio modes.
  private static final String[] AUDIO_MODES = new String[] {
      "MODE_NORMAL",
      "MODE_RINGTONE",
      "MODE_IN_CALL",
      "MODE_IN_COMMUNICATION",
  };

  private static final int DEFAULT_FRAME_PER_BUFFER = 256;

  private final long nativeAudioManager;
  private final Context context;
  private final AudioManager audioManager;

  private boolean initialized = false;
  private int nativeSampleRate;
  private int nativeChannels;

  private boolean hardwareAEC;
  private boolean lowLatencyOutput;
  private int sampleRate;
  private int channels;
  private int outputBufferSize;
  private int inputBufferSize;

  WebRtcAudioManager(Context context, long nativeAudioManager) {
    Logd("ctor" + WebRtcAudioUtils.getThreadInfo());
    this.context = context;
    this.nativeAudioManager = nativeAudioManager;
    audioManager = (AudioManager) context.getSystemService(
        Context.AUDIO_SERVICE);
    if (DEBUG) {
      WebRtcAudioUtils.logDeviceInfo(TAG);
    }
    storeAudioParameters();
    nativeCacheAudioParameters(
        sampleRate, channels, hardwareAEC, lowLatencyOutput, outputBufferSize,
        inputBufferSize, nativeAudioManager);
  }

  private boolean init() {
    Logd("init" + WebRtcAudioUtils.getThreadInfo());
    if (initialized) {
      return true;
    }
    Logd("audio mode is: " + AUDIO_MODES[audioManager.getMode()]);
    initialized = true;
    return true;
  }

  private void dispose() {
    Logd("dispose" + WebRtcAudioUtils.getThreadInfo());
    if (!initialized) {
      return;
    }
  }

  private boolean isCommunicationModeEnabled() {
    return (audioManager.getMode() == AudioManager.MODE_IN_COMMUNICATION);
  }

  private void storeAudioParameters() {
    // Only mono is supported currently (in both directions).
    // TODO(henrika): add support for stereo playout.
    channels = CHANNELS;
    sampleRate = getNativeOutputSampleRate();
    hardwareAEC = isAcousticEchoCancelerSupported();
    lowLatencyOutput = isLowLatencyOutputSupported();
    outputBufferSize = lowLatencyOutput ?
        getLowLatencyOutputFramesPerBuffer() :
        getMinOutputFrameSize(sampleRate, channels);
    // TODO(henrika): add support for low-latency input.
    inputBufferSize = getMinInputFrameSize(sampleRate, channels);
  }

  // Gets the current earpiece state.
  private boolean hasEarpiece() {
    return context.getPackageManager().hasSystemFeature(
        PackageManager.FEATURE_TELEPHONY);
  }

  // Returns true if low-latency audio output is supported.
  private boolean isLowLatencyOutputSupported() {
    return isOpenSLESSupported() &&
        context.getPackageManager().hasSystemFeature(
            PackageManager.FEATURE_AUDIO_LOW_LATENCY);
  }

  // Returns true if low-latency audio input is supported.
  public boolean isLowLatencyInputSupported() {
    // TODO(henrika): investigate if some sort of device list is needed here
    // as well. The NDK doc states that: "As of API level 21, lower latency
    // audio input is supported on select devices. To take advantage of this
    // feature, first confirm that lower latency output is available".
    return WebRtcAudioUtils.runningOnLollipopOrHigher() &&
        isLowLatencyOutputSupported();
  }

  // Returns the native output sample rate for this device's output stream.
  private int getNativeOutputSampleRate() {
    // Override this if we're running on an old emulator image which only
    // supports 8 kHz and doesn't support PROPERTY_OUTPUT_SAMPLE_RATE.
    if (WebRtcAudioUtils.runningOnEmulator()) {
      Logd("Running on old emulator, overriding sampling rate to 8 kHz.");
      return 8000;
    }
    if (!WebRtcAudioUtils.runningOnJellyBeanMR1OrHigher()) {
      return SAMPLE_RATE_HZ;
    }
    String sampleRateString = audioManager.getProperty(
        AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
    return (sampleRateString == null) ?
        SAMPLE_RATE_HZ : Integer.parseInt(sampleRateString);
  }

  // Returns the native output buffer size for low-latency output streams.
  private int getLowLatencyOutputFramesPerBuffer() {
    assertTrue(isLowLatencyOutputSupported());
    if (!WebRtcAudioUtils.runningOnJellyBeanMR1OrHigher()) {
      return DEFAULT_FRAME_PER_BUFFER;
    }
    String framesPerBuffer = audioManager.getProperty(
        AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
    return framesPerBuffer == null ?
        DEFAULT_FRAME_PER_BUFFER : Integer.parseInt(framesPerBuffer);
  }

  // Returns true if the device supports Acoustic Echo Canceler (AEC).
  // Also takes blacklisting into account.
  private static boolean isAcousticEchoCancelerSupported() {
    if (WebRtcAudioUtils.deviceIsBlacklistedForHwAecUsage()) {
      Logd(Build.MODEL + " is blacklisted for HW AEC usage!");
      return false;
    }
    return WebRtcAudioUtils.isAcousticEchoCancelerSupported();
  }

  // Returns the minimum output buffer size for Java based audio (AudioTrack).
  // This size can also be used for OpenSL ES implementations on devices that
  // lacks support of low-latency output.
  private static int getMinOutputFrameSize(int sampleRateInHz, int numChannels) {
    final int bytesPerFrame = numChannels * (BITS_PER_SAMPLE / 8);
    final int channelConfig;
    if (numChannels == 1) {
      channelConfig = AudioFormat.CHANNEL_OUT_MONO;
    } else if (numChannels == 2) {
      channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
    } else {
      return -1;
    }
    return AudioTrack.getMinBufferSize(
        sampleRateInHz, channelConfig, AudioFormat.ENCODING_PCM_16BIT) /
        bytesPerFrame;
  }

  // Returns the native input buffer size for input streams.
  private int getLowLatencyInputFramesPerBuffer() {
    assertTrue(isLowLatencyInputSupported());
    return getLowLatencyOutputFramesPerBuffer();
  }

  // Returns the minimum input buffer size for Java based audio (AudioRecord).
  // This size can calso be used for OpenSL ES implementations on devices that
  // lacks support of low-latency input.
  private static int getMinInputFrameSize(int sampleRateInHz, int numChannels) {
    final int bytesPerFrame = numChannels * (BITS_PER_SAMPLE / 8);
    assertTrue(numChannels == CHANNELS);
    return AudioRecord.getMinBufferSize(sampleRateInHz,
        AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT) /
        bytesPerFrame;
  }

  // Returns true if OpenSL ES audio is supported.
  private static boolean isOpenSLESSupported() {
    // Check for API level 9 or higher, to confirm use of OpenSL ES.
    return WebRtcAudioUtils.runningOnGingerBreadOrHigher();
  }

  // Helper method which throws an exception  when an assertion has failed.
  private static void assertTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  private static void Logd(String msg) {
    Log.d(TAG, msg);
  }

  private static void Loge(String msg) {
    Log.e(TAG, msg);
  }

  private native void nativeCacheAudioParameters(
    int sampleRate, int channels, boolean hardwareAEC, boolean lowLatencyOutput,
    int outputBufferSize, int inputBufferSize,
    long nativeAudioManager);
}
