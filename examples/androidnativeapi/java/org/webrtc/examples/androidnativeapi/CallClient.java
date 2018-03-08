/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.examples.androidnativeapi;

import android.os.Handler;
import android.os.HandlerThread;
import org.webrtc.NativeClassQualifiedName;
import org.webrtc.VideoSink;

public class CallClient {
  private static final String TAG = "CallClient";

  private final HandlerThread thread;
  private final Handler handler;

  private long nativeClient;

  public CallClient() {
    thread = new HandlerThread(TAG + "Thread");
    thread.start();
    handler = new Handler(thread.getLooper());
    handler.post(() -> { nativeClient = nativeCreateClient(); });
  }

  public void call(VideoSink localSink, VideoSink remoteSink) {
    handler.post(() -> { nativeCall(nativeClient, localSink, remoteSink); });
  }

  public void hangup() {
    handler.post(() -> { nativeHangup(nativeClient); });
  }

  public void close() {
    handler.post(() -> {
      nativeDelete(nativeClient);
      nativeClient = 0;
    });
    thread.quitSafely();
  }

  private static native long nativeCreateClient();
  @NativeClassQualifiedName("webrtc_examples::AndroidCallClient")
  private static native void nativeCall(long nativePtr, VideoSink localSink, VideoSink remoteSink);
  @NativeClassQualifiedName("webrtc_examples::AndroidCallClient")
  private static native void nativeHangup(long nativePtr);
  @NativeClassQualifiedName("webrtc_examples::AndroidCallClient")
  private static native void nativeDelete(long nativePtr);
}
