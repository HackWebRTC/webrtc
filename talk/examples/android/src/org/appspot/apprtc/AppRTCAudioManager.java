/*
 * libjingle
 * Copyright 2014, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package org.appspot.apprtc;

import android.content.Context;
import android.media.AudioManager;
import android.util.Log;

/**
 * AppRTCAudioManager manages all audio related parts of the AppRTC demo.
 * TODO(henrika): add support for device enumeration, device selection etc.
 */
public class AppRTCAudioManager {
  private static final String TAG = "AppRTCAudioManager";

  private boolean initialized = false;
  private AudioManager audioManager;
  private int savedAudioMode = AudioManager.MODE_INVALID;
  private boolean savedIsSpeakerPhoneOn = false;
  private boolean savedIsMicrophoneMute = false;

  /** Construction */
  static AppRTCAudioManager create(Context context) {
    return new AppRTCAudioManager(context);
  }

  private AppRTCAudioManager(Context context) {
    Log.d(TAG, "AppRTCAudioManager");
    audioManager = ((AudioManager) context.getSystemService(
        Context.AUDIO_SERVICE));
  }

  public void init() {
    Log.d(TAG, "init");
    if (initialized) {
      return;
    }

    // Store current audio state so we can restore it when close() is called.
    savedAudioMode = audioManager.getMode();
    savedIsSpeakerPhoneOn = audioManager.isSpeakerphoneOn();
    savedIsMicrophoneMute = audioManager.isMicrophoneMute();

    // The AppRTC demo shall always run in COMMUNICATION mode since it will
    // result in best possible "VoIP settings", like audio routing, volume
    // control etc.
    audioManager.setMode(AudioManager.MODE_IN_COMMUNICATION);

    initialized = true;
  }

  public void close() {
    Log.d(TAG, "close");
    if (!initialized) {
      return;
    }

    // Restore previously stored audio states.
    setSpeakerphoneOn(savedIsSpeakerPhoneOn);
    setMicrophoneMute(savedIsMicrophoneMute);
    audioManager.setMode(savedAudioMode);

    initialized = false;
  }

  /** Sets the speaker phone mode. */
  private void setSpeakerphoneOn(boolean on) {
    boolean wasOn = audioManager.isSpeakerphoneOn();
    if (wasOn == on) {
      return;
    }
    audioManager.setSpeakerphoneOn(on);
  }

  /** Sets the microphone mute state. */
  private void setMicrophoneMute(boolean on) {
    boolean wasMuted = audioManager.isMicrophoneMute();
    if (wasMuted == on) {
      return;
    }
    audioManager.setMicrophoneMute(on);
  }
}
