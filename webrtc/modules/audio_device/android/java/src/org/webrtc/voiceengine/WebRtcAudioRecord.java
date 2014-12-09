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

import java.nio.ByteBuffer;
import java.util.concurrent.locks.ReentrantLock;

import android.content.Context;
import android.media.AudioFormat;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.AudioEffect;
import android.media.audiofx.AudioEffect.Descriptor;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.MediaRecorder.AudioSource;
import android.os.Build;
import android.util.Log;

class WebRtcAudioRecord {
    private AudioRecord _audioRecord = null;

    private Context _context;

    private ByteBuffer _recBuffer;
    private byte[] _tempBufRec;

    private final ReentrantLock _recLock = new ReentrantLock();

    private boolean _doRecInit = true;
    private boolean _isRecording = false;

    private int _bufferedRecSamples = 0;

    private AcousticEchoCanceler _aec = null;
    private boolean _useBuiltInAEC = false;

    private static boolean runningOnJellyBeanOrHigher() {
      return Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN;
    }

    WebRtcAudioRecord() {
        try {
            _recBuffer = ByteBuffer.allocateDirect(2 * 480); // Max 10 ms @ 48
                                                             // kHz
        } catch (Exception e) {
            DoLog(e.getMessage());
        }

        _tempBufRec = new byte[2 * 480];
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

    private int EnableBuiltInAEC(boolean enable) {
      DoLog("EnableBuiltInAEC(" + enable + ')');
      // AcousticEchoCanceler was added in API level 16 (Jelly Bean).
      if (!runningOnJellyBeanOrHigher()) {
        return -1;
      }

      _useBuiltInAEC = enable;

      // Set AEC state if AEC has already been created.
      if (_aec != null) {
        int ret = _aec.setEnabled(enable);
        if (ret != AudioEffect.SUCCESS) {
          DoLogErr("AcousticEchoCanceler.setEnabled failed");
          return -1;
        }
        DoLog("AcousticEchoCanceler.getEnabled: " + _aec.getEnabled());
      }

      return 0;
    }

    @SuppressWarnings("unused")
    private int InitRecording(int audioSource, int sampleRate) {
        DoLog("InitRecording");
        audioSource = AudioSource.VOICE_COMMUNICATION;
        // get the minimum buffer size that can be used
        int minRecBufSize = AudioRecord.getMinBufferSize(
            sampleRate,
            AudioFormat.CHANNEL_IN_MONO,
            AudioFormat.ENCODING_PCM_16BIT);

        // DoLog("min rec buf size is " + minRecBufSize);

        // double size to be more safe
        int recBufSize = minRecBufSize * 2;
        // On average half of the samples have been recorded/buffered and the
        // recording interval is 1/100s.
        _bufferedRecSamples = sampleRate / 200;
        // DoLog("rough rec delay set to " + _bufferedRecSamples);

        if (_aec != null) {
            _aec.release();
            _aec = null;
        }

        // release the object
        if (_audioRecord != null) {
            _audioRecord.release();
            _audioRecord = null;
        }

        try {
            _audioRecord = new AudioRecord(
                            audioSource,
                            sampleRate,
                            AudioFormat.CHANNEL_IN_MONO,
                            AudioFormat.ENCODING_PCM_16BIT,
                            recBufSize);

        } catch (Exception e) {
            DoLog(e.getMessage());
            return -1;
        }

        // check that the audioRecord is ready to be used
        if (_audioRecord.getState() != AudioRecord.STATE_INITIALIZED) {
            // DoLog("rec not initialized " + sampleRate);
            return -1;
        }

        // DoLog("rec sample rate set to " + sampleRate);

        DoLog("AcousticEchoCanceler.isAvailable: " + BuiltInAECIsAvailable());
        if (!BuiltInAECIsAvailable()) {
            return _bufferedRecSamples;
        }

        _aec = AcousticEchoCanceler.create(_audioRecord.getAudioSessionId());
        if (_aec == null) {
            DoLogErr("AcousticEchoCanceler.create failed");
            return -1;
        }

        int ret = _aec.setEnabled(_useBuiltInAEC);
        if (ret != AudioEffect.SUCCESS) {
            DoLogErr("AcousticEchoCanceler.setEnabled failed");
            return -1;
        }

        Descriptor descriptor = _aec.getDescriptor();
        DoLog("AcousticEchoCanceler " +
              "name: " + descriptor.name + ", " +
              "implementor: " + descriptor.implementor + ", " +
              "uuid: " + descriptor.uuid);
        DoLog("AcousticEchoCanceler.getEnabled: " + _aec.getEnabled());

        return _bufferedRecSamples;
    }

    @SuppressWarnings("unused")
    private int StartRecording() {
        DoLog("StartRecording");
        // start recording
        try {
            _audioRecord.startRecording();

        } catch (IllegalStateException e) {
            e.printStackTrace();
            return -1;
        }

        _isRecording = true;
        return 0;
    }

    @SuppressWarnings("unused")
    private int StopRecording() {
        DoLog("StopRecording");
        _recLock.lock();
        try {
            // only stop if we are recording
            if (_audioRecord.getRecordingState() ==
              AudioRecord.RECORDSTATE_RECORDING) {
                // stop recording
                try {
                    _audioRecord.stop();
                } catch (IllegalStateException e) {
                    e.printStackTrace();
                    return -1;
                }
            }

            // Release the AEC object.
            if (_aec != null) {
                _aec.release();
                _aec = null;
            }

            // Release the AudioRecord object.
            _audioRecord.release();
            _audioRecord = null;

        } finally {
            // Ensure we always unlock, both for success, exception or error
            // return.
            _doRecInit = true;
            _recLock.unlock();
        }

        _isRecording = false;
        return 0;
    }

    @SuppressWarnings("unused")
    private int RecordAudio(int lengthInBytes) {
        _recLock.lock();

        try {
            if (_audioRecord == null) {
                return -2; // We have probably closed down while waiting for rec
                           // lock
            }

            // Set priority, only do once
            if (_doRecInit == true) {
                try {
                    android.os.Process.setThreadPriority(
                        android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);
                } catch (Exception e) {
                    DoLog("Set rec thread priority failed: " + e.getMessage());
                }
                _doRecInit = false;
            }

            int readBytes = 0;
            _recBuffer.rewind(); // Reset the position to start of buffer
            readBytes = _audioRecord.read(_tempBufRec, 0, lengthInBytes);
            // DoLog("read " + readBytes + "from SC");
            _recBuffer.put(_tempBufRec);

            if (readBytes != lengthInBytes) {
                // DoLog("Could not read all data from sc (read = " + readBytes
                // + ", length = " + lengthInBytes + ")");
                return -1;
            }

        } catch (Exception e) {
            DoLogErr("RecordAudio try failed: " + e.getMessage());

        } finally {
            // Ensure we always unlock, both for success, exception or error
            // return.
            _recLock.unlock();
        }

        return _bufferedRecSamples;
    }

    final String logTag = "WebRtcAudioRecord-Java";

    private void DoLog(String msg) {
        Log.d(logTag, msg);
    }

    private void DoLogErr(String msg) {
        Log.e(logTag, msg);
    }
}
