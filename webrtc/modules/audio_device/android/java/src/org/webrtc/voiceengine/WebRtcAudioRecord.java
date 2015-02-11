/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.voiceengine;

import java.lang.System;
import java.lang.Thread;
import java.nio.ByteBuffer;
import java.util.concurrent.TimeUnit;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

import android.content.Context;
import android.media.AudioFormat;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.AudioEffect;
import android.media.audiofx.AudioEffect.Descriptor;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.MediaRecorder.AudioSource;
import android.os.Build;
import android.os.Process;
import android.os.SystemClock;
import android.util.Log;

class  WebRtcAudioRecord {
  private static final boolean DEBUG = false;

  private static final String TAG = "WebRtcAudioRecord";

  // Use 44.1kHz as the default sampling rate.
  private static final int SAMPLE_RATE_HZ = 44100;

  // Mono recording is default.
  private static final int CHANNELS = 1;

  // Default audio data format is PCM 16 bit per sample.
  // Guaranteed to be supported by all devices.
  private static final int BITS_PER_SAMPLE = 16;

  // Number of bytes per audio frame.
  // Example: 16-bit PCM in stereo => 2*(16/8)=4 [bytes/frame]
  private static final int BYTES_PER_FRAME = CHANNELS * (BITS_PER_SAMPLE / 8);

  // Requested size of each recorded buffer provided to the client.
  private static final int CALLBACK_BUFFER_SIZE_MS = 10;

  // Average number of callbacks per second.
  private static final int BUFFERS_PER_SECOND = 1000 / CALLBACK_BUFFER_SIZE_MS;

  private ByteBuffer byteBuffer;
  private final int bytesPerBuffer;
  private final int framesPerBuffer;
  private final int sampleRate;

  private final long nativeAudioRecord;
  private final AudioManager audioManager;
  private final Context context;

  private AudioRecord audioRecord = null;
  private AudioRecordThread audioThread = null;

  private AcousticEchoCanceler aec = null;
  private boolean useBuiltInAEC = false;

  private final Set<Long> threadIds = new HashSet<Long>();

  private static boolean runningOnJellyBeanOrHigher() {
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN;
  }

  private static boolean runningOnJellyBeanMR1OrHigher() {
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1;
  }

  /**
   * Audio thread which keeps calling ByteBuffer.read() waiting for audio
   * to be recorded. Feeds recorded data to the native counterpart as a
   * periodic sequence of callbacks using DataIsRecorded().
   * This thread uses a Process.THREAD_PRIORITY_URGENT_AUDIO priority.
   */
  private class AudioRecordThread extends Thread {
    private volatile boolean keepAlive = true;

    public AudioRecordThread(String name) {
      super(name);
    }

    @Override
    public void run() {
      Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
      DoLog("AudioRecordThread" + getThreadInfo());
      AddThreadId();

      try {
        audioRecord.startRecording();
      } catch (IllegalStateException e) {
          DoLogErr("AudioRecord.startRecording failed: " + e.getMessage());
        return;
      }
      assertIsTrue(audioRecord.getRecordingState()
          == AudioRecord.RECORDSTATE_RECORDING);

      long lastTime = System.nanoTime();
      while (keepAlive) {
        int bytesRead = audioRecord.read(byteBuffer, byteBuffer.capacity());
        if (bytesRead == byteBuffer.capacity()) {
          nativeDataIsRecorded(bytesRead, nativeAudioRecord);
        } else {
          DoLogErr("AudioRecord.read failed: " + bytesRead);
          if (bytesRead == AudioRecord.ERROR_INVALID_OPERATION) {
            keepAlive = false;
          }
        }
        if (DEBUG) {
          long nowTime = System.nanoTime();
          long durationInMs =
              TimeUnit.NANOSECONDS.toMillis((nowTime - lastTime));
          lastTime = nowTime;
          DoLog("bytesRead[" + durationInMs + "] " + bytesRead);
        }
      }

      try {
        audioRecord.stop();
      } catch (IllegalStateException e) {
        DoLogErr("AudioRecord.stop failed: " + e.getMessage());
      }
      RemoveThreadId();
    }

    public void joinThread() {
      keepAlive = false;
      while (isAlive()) {
        try {
          join();
        } catch (InterruptedException e) {
          // Ignore.
        }
      }
    }
  }

  WebRtcAudioRecord(Context context, long nativeAudioRecord) {
    DoLog("ctor" + getThreadInfo());
    this.context = context;
    this.nativeAudioRecord = nativeAudioRecord;
    audioManager = ((AudioManager) context.getSystemService(
        Context.AUDIO_SERVICE));
    sampleRate = GetNativeSampleRate();
    bytesPerBuffer = BYTES_PER_FRAME * (sampleRate / BUFFERS_PER_SECOND);
    framesPerBuffer = sampleRate / BUFFERS_PER_SECOND;
    byteBuffer = byteBuffer.allocateDirect(bytesPerBuffer);
    DoLog("byteBuffer.capacity: " + byteBuffer.capacity());

    // Rather than passing the ByteBuffer with every callback (requiring
    // the potentially expensive GetDirectBufferAddress) we simply have the
    // the native class cache the address to the memory once.
    nativeCacheDirectBufferAddress(byteBuffer, nativeAudioRecord);
    AddThreadId();
  }

  /**
   * Returns the native or optimal input sample rate for this device's
   * primary input stream. Unit is in Hz.
   * Note that we actually query the output device but the same result is
   * also valid for input.
   */
  private int GetNativeSampleRate() {
    if (!runningOnJellyBeanMR1OrHigher()) {
      return SAMPLE_RATE_HZ;
    }
    String sampleRateString = audioManager.getProperty(
        AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
    return (sampleRateString == null) ?
        SAMPLE_RATE_HZ : Integer.parseInt(sampleRateString);
  }

  public static boolean BuiltInAECIsAvailable() {
    // AcousticEchoCanceler was added in API level 16 (Jelly Bean).
    if (!runningOnJellyBeanOrHigher()) {
      return false;
    }
    // TODO(henrika): add black-list based on device name. We could also
    // use uuid to exclude devices but that would require a session ID from
    // an existing AudioRecord object.
    return AcousticEchoCanceler.isAvailable();
  }

  private boolean EnableBuiltInAEC(boolean enable) {
    DoLog("EnableBuiltInAEC(" + enable + ')');
    AddThreadId();
    // AcousticEchoCanceler was added in API level 16 (Jelly Bean).
    if (!runningOnJellyBeanOrHigher()) {
      return false;
    }
    // Store the AEC state.
    useBuiltInAEC = enable;
    // Set AEC state if AEC has already been created.
    if (aec != null) {
      int ret = aec.setEnabled(enable);
      if (ret != AudioEffect.SUCCESS) {
        DoLogErr("AcousticEchoCanceler.setEnabled failed");
        return false;
      }
      DoLog("AcousticEchoCanceler.getEnabled: " + aec.getEnabled());
    }
    return true;
  }

  private int InitRecording(int sampleRate) {
    DoLog("InitRecording(sampleRate=" + sampleRate + ")");
    AddThreadId();
    // Get the minimum buffer size required for the successful creation of
    // an AudioRecord object, in byte units.
    // Note that this size doesn't guarantee a smooth recording under load.
    // TODO(henrika): Do we need to make this larger to avoid underruns?
    int minBufferSize = AudioRecord.getMinBufferSize(
          sampleRate,
          AudioFormat.CHANNEL_IN_MONO,
          AudioFormat.ENCODING_PCM_16BIT);
    DoLog("AudioRecord.getMinBufferSize: " + minBufferSize);

    if (aec != null) {
      aec.release();
      aec = null;
    }
    if (audioRecord != null) {
      audioRecord.release();
      audioRecord = null;
    }

    int bufferSizeInBytes = Math.max(byteBuffer.capacity(), minBufferSize);
    DoLog("bufferSizeInBytes: " + bufferSizeInBytes);
    try {
      audioRecord = new AudioRecord(AudioSource.VOICE_COMMUNICATION,
                                    sampleRate,
                                    AudioFormat.CHANNEL_IN_MONO,
                                    AudioFormat.ENCODING_PCM_16BIT,
                                    bufferSizeInBytes);

    } catch (IllegalArgumentException e) {
      DoLog(e.getMessage());
      return -1;
    }
    assertIsTrue(audioRecord.getState() == AudioRecord.STATE_INITIALIZED);

    DoLog("AudioRecord " +
          "session ID: " + audioRecord.getAudioSessionId() + ", " +
          "audio format: " + audioRecord.getAudioFormat() + ", " +
          "channels: " + audioRecord.getChannelCount() + ", " +
          "sample rate: " + audioRecord.getSampleRate());
    DoLog("AcousticEchoCanceler.isAvailable: " + BuiltInAECIsAvailable());
    if (!BuiltInAECIsAvailable()) {
      return framesPerBuffer;
    }

    aec = AcousticEchoCanceler.create(audioRecord.getAudioSessionId());
    if (aec == null) {
      DoLogErr("AcousticEchoCanceler.create failed");
      return -1;
    }
    int ret = aec.setEnabled(useBuiltInAEC);
    if (ret != AudioEffect.SUCCESS) {
      DoLogErr("AcousticEchoCanceler.setEnabled failed");
      return -1;
    }
    Descriptor descriptor = aec.getDescriptor();
    DoLog("AcousticEchoCanceler " +
          "name: " + descriptor.name + ", " +
          "implementor: " + descriptor.implementor + ", " +
          "uuid: " + descriptor.uuid);
    DoLog("AcousticEchoCanceler.getEnabled: " + aec.getEnabled());
    return framesPerBuffer;
  }

  private boolean StartRecording() {
    DoLog("StartRecording");
    AddThreadId();
    if (audioRecord == null) {
      DoLogErr("start() called before init()");
      return false;
    }
    if (audioThread != null) {
      DoLogErr("start() was already called");
      return false;
    }
    audioThread = new AudioRecordThread("AudioRecordJavaThread");
    audioThread.start();
    return true;
  }

  private boolean StopRecording() {
    DoLog("StopRecording");
    AddThreadId();
    if (audioThread == null) {
      DoLogErr("start() was never called, or stop() was already called");
      return false;
    }
    audioThread.joinThread();
    audioThread = null;
    if (aec != null) {
      aec.release();
      aec = null;
    }
    if (audioRecord != null) {
      audioRecord.release();
      audioRecord = null;
    }
    return true;
  }

  private void DoLog(String msg) {
    Log.d(TAG, msg);
  }

  private void DoLogErr(String msg) {
    Log.e(TAG, msg);
  }

  /** Helper method for building a string of thread information.*/
  private static String getThreadInfo() {
    return "@[name=" + Thread.currentThread().getName()
        + ", id=" + Thread.currentThread().getId() + "]";
  }

  /** Helper method which throws an exception when an assertion has failed. */
  private static void assertIsTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  private void AddThreadId() {
    threadIds.add(Thread.currentThread().getId());
    DoLog("threadIds: " + threadIds + " (#threads=" + threadIds.size() + ")");
  }

  private void RemoveThreadId() {
    threadIds.remove(Thread.currentThread().getId());
    DoLog("threadIds: " + threadIds + " (#threads=" + threadIds.size() + ")");
  }

  private native void nativeCacheDirectBufferAddress(
      ByteBuffer byteBuffer, long nativeAudioRecord);

  private native void nativeDataIsRecorded(int bytes, long nativeAudioRecord);
}
