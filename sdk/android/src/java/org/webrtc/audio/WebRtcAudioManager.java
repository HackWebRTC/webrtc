/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.os.Build;
import javax.annotation.Nullable;
import org.webrtc.Logging;
import org.webrtc.CalledByNative;

// WebRtcAudioManager handles tasks that uses android.media.AudioManager.
// At construction, storeAudioParameters() is called and it retrieves
// fundamental audio parameters like native sample rate and number of channels.
// The result is then provided to the caller by nativeCacheAudioParameters().
// It is also possible to call init() to set up the audio environment for best
// possible "VoIP performance". All settings done in init() are reverted by
// dispose(). This class can also be used without calling init() if the user
// prefers to set up the audio environment separately. However, it is
// recommended to always use AudioManager.MODE_IN_COMMUNICATION.
class WebRtcAudioManager {
  private static final boolean DEBUG = false;

  private static final String TAG = "WebRtcAudioManager";

  // Use mono as default for both audio directions.
  private static boolean useStereoOutput = false;
  private static boolean useStereoInput = false;

  // Call these methods to override the default mono audio modes for the specified direction(s)
  // (input and/or output).
  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  public static synchronized void setStereoOutput(boolean enable) {
    Logging.w(TAG, "Overriding default output behavior: setStereoOutput(" + enable + ')');
    useStereoOutput = enable;
  }

  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  public static synchronized void setStereoInput(boolean enable) {
    Logging.w(TAG, "Overriding default input behavior: setStereoInput(" + enable + ')');
    useStereoInput = enable;
  }

  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  @CalledByNative
  public synchronized boolean getStereoOutput() {
    return useStereoOutput;
  }

  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  @CalledByNative
  public synchronized boolean getStereoInput() {
    return useStereoInput;
  }

  // Default audio data format is PCM 16 bit per sample.
  // Guaranteed to be supported by all devices.
  private static final int BITS_PER_SAMPLE = 16;

  private static final int DEFAULT_FRAME_PER_BUFFER = 256;

  private final AudioManager audioManager;
  private final int sampleRate;
  private final int outputBufferSize;
  private final int inputBufferSize;
  private final VolumeLogger volumeLogger;

  private boolean initialized = false;

  @CalledByNative
  WebRtcAudioManager(Context context) {
    Logging.d(TAG, "ctor" + WebRtcAudioUtils.getThreadInfo());
    this.audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    if (DEBUG) {
      WebRtcAudioUtils.logDeviceInfo(TAG);
    }
    this.volumeLogger = new VolumeLogger(audioManager);

    final int outputChannels = getStereoOutput() ? 2 : 1;
    final int inputChannels = getStereoInput() ? 2 : 1;

    this.sampleRate = getNativeOutputSampleRate();
    this.outputBufferSize = isLowLatencyOutputSupported(context)
        ? getLowLatencyOutputFramesPerBuffer()
        : getMinOutputFrameSize(sampleRate, outputChannels);
    this.inputBufferSize = isLowLatencyInputSupported(context)
        ? getLowLatencyInputFramesPerBuffer()
        : getMinInputFrameSize(sampleRate, inputChannels);

    WebRtcAudioUtils.logAudioState(TAG);
  }

  @CalledByNative
  private boolean init() {
    Logging.d(TAG, "init" + WebRtcAudioUtils.getThreadInfo());
    if (initialized) {
      return true;
    }
    Logging.d(TAG, "audio mode is: " + WebRtcAudioUtils.modeToString(audioManager.getMode()));
    initialized = true;
    volumeLogger.start();
    return true;
  }

  @CalledByNative
  private void dispose() {
    Logging.d(TAG, "dispose" + WebRtcAudioUtils.getThreadInfo());
    if (!initialized) {
      return;
    }
    volumeLogger.stop();
  }

  // Returns true if low-latency audio output is supported.
  public static boolean isLowLatencyOutputSupported(Context context) {
    return context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUDIO_LOW_LATENCY);
  }

  // Returns true if low-latency audio input is supported.
  // TODO(henrika): remove the hardcoded false return value when OpenSL ES
  // input performance has been evaluated and tested more.
  public static boolean isLowLatencyInputSupported(Context context) {
    // TODO(henrika): investigate if some sort of device list is needed here
    // as well. The NDK doc states that: "As of API level 21, lower latency
    // audio input is supported on select devices. To take advantage of this
    // feature, first confirm that lower latency output is available".
    return WebRtcAudioUtils.runningOnLollipopOrHigher() && isLowLatencyOutputSupported(context);
  }

  // Returns the native output sample rate for this device's output stream.
  private int getNativeOutputSampleRate() {
    // Override this if we're running on an old emulator image which only
    // supports 8 kHz and doesn't support PROPERTY_OUTPUT_SAMPLE_RATE.
    if (WebRtcAudioUtils.runningOnEmulator()) {
      Logging.d(TAG, "Running emulator, overriding sample rate to 8 kHz.");
      return 8000;
    }
    // Default can be overriden by WebRtcAudioUtils.setDefaultSampleRateHz().
    // If so, use that value and return here.
    if (WebRtcAudioUtils.isDefaultSampleRateOverridden()) {
      Logging.d(TAG,
          "Default sample rate is overriden to " + WebRtcAudioUtils.getDefaultSampleRateHz()
              + " Hz");
      return WebRtcAudioUtils.getDefaultSampleRateHz();
    }
    // No overrides available. Deliver best possible estimate based on default
    // Android AudioManager APIs.
    final int sampleRateHz;
    if (WebRtcAudioUtils.runningOnJellyBeanMR1OrHigher()) {
      sampleRateHz = getSampleRateOnJellyBeanMR10OrHigher();
    } else {
      sampleRateHz = WebRtcAudioUtils.getDefaultSampleRateHz();
    }
    Logging.d(TAG, "Sample rate is set to " + sampleRateHz + " Hz");
    return sampleRateHz;
  }

  @CalledByNative
  int getSampleRate() {
    return sampleRate;
  }

  @TargetApi(17)
  private int getSampleRateOnJellyBeanMR10OrHigher() {
    String sampleRateString = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
    return (sampleRateString == null) ? WebRtcAudioUtils.getDefaultSampleRateHz()
                                      : Integer.parseInt(sampleRateString);
  }

  // Returns the native output buffer size for low-latency output streams.
  @TargetApi(17)
  private int getLowLatencyOutputFramesPerBuffer() {
    if (!WebRtcAudioUtils.runningOnJellyBeanMR1OrHigher()) {
      return DEFAULT_FRAME_PER_BUFFER;
    }
    String framesPerBuffer =
        audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
    return framesPerBuffer == null ? DEFAULT_FRAME_PER_BUFFER : Integer.parseInt(framesPerBuffer);
  }

  // Returns true if the device supports an audio effect (AEC or NS).
  // Four conditions must be fulfilled if functions are to return true:
  // 1) the platform must support the built-in (HW) effect,
  // 2) explicit use (override) of a WebRTC based version must not be set,
  // 3) the device must not be blacklisted for use of the effect, and
  // 4) the UUID of the effect must be approved (some UUIDs can be excluded).
  @CalledByNative
  boolean isAcousticEchoCancelerSupported() {
    return WebRtcAudioEffects.canUseAcousticEchoCanceler();
  }

  @CalledByNative
  boolean isNoiseSuppressorSupported() {
    return WebRtcAudioEffects.canUseNoiseSuppressor();
  }

  @CalledByNative
  int getOutputBufferSize() {
    return outputBufferSize;
  }

  @CalledByNative
  int getInputBufferSize() {
    return inputBufferSize;
  }

  // Returns the minimum output buffer size for Java based audio (AudioTrack).
  // This size can also be used for OpenSL ES implementations on devices that
  // lacks support of low-latency output.
  private static int getMinOutputFrameSize(int sampleRateInHz, int numChannels) {
    final int bytesPerFrame = numChannels * (BITS_PER_SAMPLE / 8);
    final int channelConfig =
        (numChannels == 1 ? AudioFormat.CHANNEL_OUT_MONO : AudioFormat.CHANNEL_OUT_STEREO);
    return AudioTrack.getMinBufferSize(
               sampleRateInHz, channelConfig, AudioFormat.ENCODING_PCM_16BIT)
        / bytesPerFrame;
  }

  // Returns the native input buffer size for input streams.
  private int getLowLatencyInputFramesPerBuffer() {
    return getLowLatencyOutputFramesPerBuffer();
  }

  // Returns the minimum input buffer size for Java based audio (AudioRecord).
  // This size can calso be used for OpenSL ES implementations on devices that
  // lacks support of low-latency input.
  private static int getMinInputFrameSize(int sampleRateInHz, int numChannels) {
    final int bytesPerFrame = numChannels * (BITS_PER_SAMPLE / 8);
    final int channelConfig =
        (numChannels == 1 ? AudioFormat.CHANNEL_IN_MONO : AudioFormat.CHANNEL_IN_STEREO);
    return AudioRecord.getMinBufferSize(
               sampleRateInHz, channelConfig, AudioFormat.ENCODING_PCM_16BIT)
        / bytesPerFrame;
  }
}
