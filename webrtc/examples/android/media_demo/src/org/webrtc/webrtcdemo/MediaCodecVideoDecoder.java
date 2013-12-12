/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.webrtcdemo;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.media.MediaCodec;
import android.media.MediaCrypto;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.LinkedList;

class MediaCodecVideoDecoder {
  public static final int DECODE = 0;
  private enum CodecName { ON2_VP8, GOOGLE_VPX, EXYNOX_VP8 }

  private void check(boolean value, String message) {
    if (value) {
      return;
    }
    Log.e("WEBRTC-CHECK", message);
    AlertDialog alertDialog = new AlertDialog.Builder(context).create();
    alertDialog.setTitle("WebRTC Error");
    alertDialog.setMessage(message);
    alertDialog.setButton(DialogInterface.BUTTON_POSITIVE,
        "OK",
        new DialogInterface.OnClickListener() {
          public void onClick(DialogInterface dialog, int which) {
            return;
          }
        }
                          );
    alertDialog.show();
  }

  class Frame {
    public ByteBuffer buffer;
    public long timestampUs;

    Frame(ByteBuffer buffer, long timestampUs) {
      this.buffer = buffer;
      this.timestampUs = timestampUs;
    }
  }

  // This class enables decoding being run on a separate thread.
  class DecodeHandler extends Handler {
    @Override
    public void handleMessage(Message msg) {
      // TODO(dwkang): figure out exceptions just make this thread finish.
      try {
        switch (msg.what) {
          case DECODE:
            decodePendingBuffers();
            long delayMillis = 5;  // Don't busy wait.
            handler.sendMessageDelayed(
                handler.obtainMessage(DECODE), delayMillis);
            break;
          default:
            break;
        }
      } catch (Exception e) {
        e.printStackTrace();
      }
    }
  }

  private static String TAG;
  private Context context;
  private SurfaceView surfaceView;

  private DecodeHandler handler;
  private Thread looperThread;

  MediaCodec codec;
  MediaFormat format;

  // Buffers supplied by MediaCodec for pushing encoded data to and pulling
  // decoded data from.
  private ByteBuffer[] codecInputBuffers;
  private ByteBuffer[] codecOutputBuffers;

  // Frames from the native layer.
  private LinkedList<Frame> frameQueue;
  // Indexes to MediaCodec buffers
  private LinkedList<Integer> availableInputBufferIndices;
  private LinkedList<Integer> availableOutputBufferIndices;
  private LinkedList<MediaCodec.BufferInfo> availableOutputBufferInfos;

  // Offset between system time and media time.
  private long deltaTimeUs;

  public MediaCodecVideoDecoder(Context context) {
    TAG = context.getString(R.string.tag);
    this.context = context;
    surfaceView = new SurfaceView(context);
    frameQueue = new LinkedList<Frame>();
    availableInputBufferIndices = new LinkedList<Integer>();
    availableOutputBufferIndices = new LinkedList<Integer>();
    availableOutputBufferInfos = new LinkedList<MediaCodec.BufferInfo>();
  }

  public void dispose() {
    codec.stop();
    codec.release();
  }

  // Return view that is written to by MediaCodec.
  public SurfaceView getView() { return surfaceView; }

  // Entry point from the native layer. Called when the class should be ready
  // to start receiving raw frames.
  private boolean start(int width, int height) {
    deltaTimeUs = -1;
    if (!setCodecState(width, height, CodecName.ON2_VP8)) {
      return false;
    }
    startLooperThread();
    // The decoding must happen on |looperThread| thread.
    handler.sendMessage(handler.obtainMessage(DECODE));
    return true;
  }

  private boolean setCodecState(int width, int height, CodecName codecName) {
    // TODO(henrike): enable more than ON2_VP8 codec.
    format = new MediaFormat();
    format.setInteger(MediaFormat.KEY_WIDTH, width);
    format.setInteger(MediaFormat.KEY_HEIGHT, height);
    try {
      switch (codecName) {
        case ON2_VP8:
          format.setString(MediaFormat.KEY_MIME, "video/x-vnd.on2.vp8");
          codec = MediaCodec.createDecoderByType("video/x-vnd.on2.vp8");
          break;
        case GOOGLE_VPX:
          // SW VP8 decoder
          codec = MediaCodec.createByCodecName("OMX.google.vpx.decoder");
          break;
        case EXYNOX_VP8:
          // Nexus10 HW VP8 decoder
          codec = MediaCodec.createByCodecName("OMX.Exynos.VP8.Decoder");
          break;
        default:
          return false;
      }
    } catch  (Exception e) {
      // TODO(dwkang): replace this instanceof/throw with a narrower catch
      // clause once the SDK advances.
      if (e instanceof IOException) {
        Log.e(TAG, "Failed to create MediaCodec for VP8.", e);
        return false;
      }
      throw new RuntimeException(e);
    }
    Surface surface = surfaceView.getHolder().getSurface();
    MediaCrypto crypto = null;  // No crypto.
    int flags = 0;  // Decoder (1 for encoder)
    codec.configure(format, surface, crypto, flags);
    codec.start();
    codecInputBuffers = codec.getInputBuffers();
    codecOutputBuffers = codec.getOutputBuffers();
    return true;
  }

  private void startLooperThread() {
    looperThread = new Thread() {
        @Override
        public void run() {
          Looper.prepare();
          // Handler that is run by this thread.
          handler = new DecodeHandler();
          // Notify that the thread has created a handler.
          synchronized(MediaCodecVideoDecoder.this) {
            MediaCodecVideoDecoder.this.notify();
          }
          Looper.loop();
        }
      };
    looperThread.start();
    // Wait for thread to notify that Handler has been set up.
    synchronized(this) {
      try {
        wait();
      } catch (InterruptedException e) {
        e.printStackTrace();
      }
    }
  }

  // Entry point from the native layer. It pushes the raw buffer to this class.
  private void pushBuffer(ByteBuffer buffer, long renderTimeMs) {
    // TODO(dwkang): figure out why exceptions just make this thread finish.
    try {
      final long renderTimeUs = renderTimeMs * 1000;
      synchronized(frameQueue) {
        frameQueue.add(new Frame(buffer, renderTimeUs));
      }
    } catch (Exception e) {
      e.printStackTrace();
    }
  }

  private boolean hasFrame() {
    synchronized(frameQueue) {
      return !frameQueue.isEmpty();
    }
  }

  private Frame dequeueFrame() {
    synchronized(frameQueue) {
      return frameQueue.removeFirst();
    }
  }

  private void flush() {
    availableInputBufferIndices.clear();
    availableOutputBufferIndices.clear();
    availableOutputBufferInfos.clear();

    codec.flush();
  }

  // Media time is relative to previous frame.
  private long mediaTimeToSystemTime(long mediaTimeUs) {
    if (deltaTimeUs == -1) {
      long nowUs = System.currentTimeMillis() * 1000;
      deltaTimeUs = nowUs - mediaTimeUs;
    }
    return deltaTimeUs + mediaTimeUs;
  }

  private void decodePendingBuffers() {
    int timeoutUs = 0;  // Don't block on dequeuing input buffer.

    int index = codec.dequeueInputBuffer(timeoutUs);
    if (index != MediaCodec.INFO_TRY_AGAIN_LATER) {
      availableInputBufferIndices.add(index);
    }
    while (feedInputBuffer()) {}

    MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
    index = codec.dequeueOutputBuffer(info, timeoutUs);
    if (index > 0) {
      availableOutputBufferIndices.add(index);
      availableOutputBufferInfos.add(info);
    }
    if (index == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
      codecOutputBuffers = codec.getOutputBuffers();
    }

    while (drainOutputBuffer()) {}
  }

  // Returns true if MediaCodec is ready for more data and there was data
  // available from the native layer.
  private boolean feedInputBuffer() {
    if (availableInputBufferIndices.isEmpty()) {
      return false;
    }
    if (!hasFrame()) {
      return false;
    }
    Frame frame = dequeueFrame();
    ByteBuffer buffer = frame.buffer;

    int index = availableInputBufferIndices.pollFirst();
    ByteBuffer codecData = codecInputBuffers[index];
    check(codecData.capacity() >= buffer.capacity(),
        "Buffer is too small to copy a frame.");
    buffer.rewind();
    codecData.rewind();
    codecData.put(buffer);

    try {
      int offset = 0;
      int flags = 0;
      codec.queueInputBuffer(index, offset, buffer.capacity(),
          frame.timestampUs, flags);
    } catch (MediaCodec.CryptoException e) {
      check(false, "CryptoException w/ errorCode " + e.getErrorCode() +
          ", '" + e.getMessage() + "'");
    }
    return true;
  }

  // Returns true if more output data could be drained.MediaCodec has more data
  // to deliver.
  private boolean drainOutputBuffer() {
    if (availableOutputBufferIndices.isEmpty()) {
      return false;
    }

    int index = availableOutputBufferIndices.peekFirst();
    MediaCodec.BufferInfo info = availableOutputBufferInfos.peekFirst();
    if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
      // End of stream is unexpected with streamed video.
      check(false, "Saw output end of stream.");
      return false;
    }
    long realTimeUs = mediaTimeToSystemTime(info.presentationTimeUs);
    long nowUs = System.currentTimeMillis() * 1000;
    long lateUs = nowUs - realTimeUs;
    if (lateUs < -10000) {
      // Frame should not be presented yet.
      return false;
    }

    // TODO(dwkang): For some extreme cases, just not doing rendering is not
    // enough. Need to seek to the next key frame.
    boolean render = lateUs <= 30000;
    if (!render) {
      Log.d(TAG, "video late by " + lateUs + " us. Skipping...");
    }
    // Decode and render to surface if desired.
    codec.releaseOutputBuffer(index, render);
    availableOutputBufferIndices.removeFirst();
    availableOutputBufferInfos.removeFirst();
    return true;
  }
}
