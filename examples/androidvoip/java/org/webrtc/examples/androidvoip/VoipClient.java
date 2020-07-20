/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.examples.androidvoip;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import java.util.ArrayList;
import java.util.List;

public class VoipClient {
  private static final String TAG = "VoipClient";

  private final HandlerThread thread;
  private final Handler handler;

  private long nativeClient;
  private OnVoipClientTaskCompleted listener;

  public VoipClient(Context applicationContext, OnVoipClientTaskCompleted listener) {
    this.listener = listener;
    thread = new HandlerThread(TAG + "Thread");
    thread.start();
    handler = new Handler(thread.getLooper());

    handler.post(() -> {
      nativeClient = nativeCreateClient(applicationContext);
      listener.onVoipClientInitializationCompleted(/* isSuccessful */ nativeClient != 0);
    });
  }

  private boolean isInitialized() {
    return nativeClient != 0;
  }

  public void getAndSetUpSupportedCodecs() {
    handler.post(() -> {
      if (isInitialized()) {
        listener.onGetSupportedCodecsCompleted(nativeGetSupportedCodecs(nativeClient));
      } else {
        listener.onUninitializedVoipClient();
      }
    });
  }

  public void getAndSetUpLocalIPAddress() {
    handler.post(() -> {
      if (isInitialized()) {
        listener.onGetLocalIPAddressCompleted(nativeGetLocalIPAddress(nativeClient));
      } else {
        listener.onUninitializedVoipClient();
      }
    });
  }

  public void setEncoder(String encoder) {
    handler.post(() -> {
      if (isInitialized()) {
        nativeSetEncoder(nativeClient, encoder);
      } else {
        listener.onUninitializedVoipClient();
      }
    });
  }

  public void setDecoders(List<String> decoders) {
    handler.post(() -> {
      if (isInitialized()) {
        nativeSetDecoders(nativeClient, decoders);
      } else {
        listener.onUninitializedVoipClient();
      }
    });
  }

  public void setLocalAddress(String ipAddress, int portNumber) {
    handler.post(() -> {
      if (isInitialized()) {
        nativeSetLocalAddress(nativeClient, ipAddress, portNumber);
      } else {
        listener.onUninitializedVoipClient();
      }
    });
  }

  public void setRemoteAddress(String ipAddress, int portNumber) {
    handler.post(() -> {
      if (isInitialized()) {
        nativeSetRemoteAddress(nativeClient, ipAddress, portNumber);
      } else {
        listener.onUninitializedVoipClient();
      }
    });
  }

  public void startSession() {
    handler.post(() -> {
      if (isInitialized()) {
        listener.onStartSessionCompleted(nativeStartSession(nativeClient));
      } else {
        listener.onUninitializedVoipClient();
      }
    });
  }

  public void stopSession() {
    handler.post(() -> {
      if (isInitialized()) {
        listener.onStopSessionCompleted(nativeStopSession(nativeClient));
      } else {
        listener.onUninitializedVoipClient();
      }
    });
  }

  public void startSend() {
    handler.post(() -> {
      if (isInitialized()) {
        listener.onStartSendCompleted(nativeStartSend(nativeClient));
      } else {
        listener.onUninitializedVoipClient();
      }
    });
  }

  public void stopSend() {
    handler.post(() -> {
      if (isInitialized()) {
        listener.onStopSendCompleted(nativeStopSend(nativeClient));
      } else {
        listener.onUninitializedVoipClient();
      }
    });
  }

  public void startPlayout() {
    handler.post(() -> {
      if (isInitialized()) {
        listener.onStartPlayoutCompleted(nativeStartPlayout(nativeClient));
      } else {
        listener.onUninitializedVoipClient();
      }
    });
  }

  public void stopPlayout() {
    handler.post(() -> {
      if (isInitialized()) {
        listener.onStopPlayoutCompleted(nativeStopPlayout(nativeClient));
      } else {
        listener.onUninitializedVoipClient();
      }
    });
  }

  public void close() {
    handler.post(() -> {
      nativeDelete(nativeClient);
      nativeClient = 0;
    });
    thread.quitSafely();
  }

  private static native long nativeCreateClient(Context applicationContext);
  private static native List<String> nativeGetSupportedCodecs(long nativeAndroidVoipClient);
  private static native String nativeGetLocalIPAddress(long nativeAndroidVoipClient);
  private static native void nativeSetEncoder(long nativeAndroidVoipClient, String encoder);
  private static native void nativeSetDecoders(long nativeAndroidVoipClient, List<String> decoders);
  private static native void nativeSetLocalAddress(
      long nativeAndroidVoipClient, String ipAddress, int portNumber);
  private static native void nativeSetRemoteAddress(
      long nativeAndroidVoipClient, String ipAddress, int portNumber);
  private static native boolean nativeStartSession(long nativeAndroidVoipClient);
  private static native boolean nativeStopSession(long nativeAndroidVoipClient);
  private static native boolean nativeStartSend(long nativeAndroidVoipClient);
  private static native boolean nativeStopSend(long nativeAndroidVoipClient);
  private static native boolean nativeStartPlayout(long nativeAndroidVoipClient);
  private static native boolean nativeStopPlayout(long nativeAndroidVoipClient);
  private static native void nativeDelete(long nativeAndroidVoipClient);
}
