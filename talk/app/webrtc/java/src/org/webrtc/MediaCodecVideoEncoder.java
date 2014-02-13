/*
 * libjingle
 * Copyright 2013, Google Inc.
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


package org.webrtc;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;

import java.nio.ByteBuffer;

// Java-side of peerconnection_jni.cc:MediaCodecVideoEncoder.
// This class is an implementation detail of the Java PeerConnection API.
// MediaCodec is thread-hostile so this class must be operated on a single
// thread.
class MediaCodecVideoEncoder {
  // This class is constructed, operated, and destroyed by its C++ incarnation,
  // so the class and its methods have non-public visibility.  The API this
  // class exposes aims to mimic the webrtc::VideoEncoder API as closely as
  // possibly to minimize the amount of translation work necessary.

  private static final String TAG = "MediaCodecVideoEncoder";

  private static final int DEQUEUE_TIMEOUT = 0;  // Non-blocking, no wait.
  private MediaCodec mediaCodec;
  private ByteBuffer[] outputBuffers;
  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  private Thread mediaCodecThread;

  private MediaCodecVideoEncoder() {}

  private static boolean isPlatformSupported() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT)
      return false; // MediaCodec.setParameters is missing.

    if (!Build.MODEL.equals("Nexus 5")) {
      // TODO(fischman): Currently the N5 is the only >=KK device containing a
      // HW VP8 encoder, so don't bother with any others.  When this list grows,
      // update the KEY_COLOR_FORMAT logic below.
      return false;
    }

    for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
      MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
      if (!info.isEncoder())
        continue;
      String name = null;
      for (String mimeType : info.getSupportedTypes()) {
        if (mimeType.equals(VP8_MIME_TYPE)) {
          name = info.getName();
          break;
        }
      }
      if (name == null)
        continue;  // No VP8 support in this codec; try the next one.
      if (name.startsWith("OMX.google.") || name.startsWith("OMX.SEC.")) {
        // SW encoder is highest priority VP8 codec; unlikely we can get HW.
        // "OMX.google." is known-software, while "OMX.SEC." is sometimes SW &
        // sometimes HW, although not VP8 HW in any known device, so treat as SW
        // here (b/9735008 #20).
        return false;
      }
      return true;  // Yay, passed the gauntlet of pre-requisites!
    }
    return false;  // No VP8 encoder.
  }

  private static int bitRate(int kbps) {
    // webrtc "kilo" means 1000, not 1024.  Apparently.
    // (and the price for overshooting is frame-dropping as webrtc enforces its
    // bandwidth estimation, which is unpleasant).
    return kbps * 1000;
  }

  private void checkOnMediaCodecThread() {
    if (mediaCodecThread.getId() != Thread.currentThread().getId()) {
      throw new RuntimeException(
          "MediaCodecVideoEncoder previously operated on " + mediaCodecThread +
          " but is now called on " + Thread.currentThread());
    }
  }

  // Return the array of input buffers, or null on failure.
  private ByteBuffer[] initEncode(int width, int height, int kbps) {
    if (mediaCodecThread != null) {
      throw new RuntimeException("Forgot to release()?");
    }
    mediaCodecThread = Thread.currentThread();
    try {
      MediaFormat format =
          MediaFormat.createVideoFormat(VP8_MIME_TYPE, width, height);
      format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate(kbps));
      // Arbitrary choices.
      format.setInteger(MediaFormat.KEY_FRAME_RATE, 30);
      format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 450);
      // TODO(fischman): when there is more than just the N5 with a VP8 HW
      // encoder, negotiate input colorformats with the codec.  For now
      // hard-code qcom's supported value.  See isPlatformSupported above.
      format.setInteger(
          MediaFormat.KEY_COLOR_FORMAT,
          MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar);
      mediaCodec = MediaCodec.createEncoderByType(VP8_MIME_TYPE);
      if (mediaCodec == null) {
        return null;
      }
      mediaCodec.configure(
          format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
      mediaCodec.start();
      outputBuffers = mediaCodec.getOutputBuffers();
      return mediaCodec.getInputBuffers();
    } catch (IllegalStateException e) {
      Log.e(TAG, "initEncode failed", e);
      return null;
    }
  }

  private boolean encode(
      boolean isKeyframe, int inputBuffer, int size,
      long presentationTimestampUs) {
    checkOnMediaCodecThread();
    try {
      if (isKeyframe) {
        // Ideally MediaCodec would honor BUFFER_FLAG_SYNC_FRAME so we could
        // indicate this in queueInputBuffer() below and guarantee _this_ frame
        // be encoded as a key frame, but sadly that flag is ignored.  Instead,
        // we request a key frame "soon".
        Bundle b = new Bundle();
        b.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
        mediaCodec.setParameters(b);
      }
      mediaCodec.queueInputBuffer(
          inputBuffer, 0, size, presentationTimestampUs, 0);
      return true;
    }
    catch (IllegalStateException e) {
      Log.e(TAG, "encode failed", e);
      return false;
    }
  }

  private void release() {
    checkOnMediaCodecThread();
    try {
      mediaCodec.stop();
      mediaCodec.release();
    } catch (IllegalStateException e) {
      Log.e(TAG, "release failed", e);
    }
    mediaCodec = null;
    mediaCodecThread = null;
  }

  private boolean setRates(int kbps, int frameRateIgnored) {
    checkOnMediaCodecThread();
    try {
      Bundle params = new Bundle();
      params.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, bitRate(kbps));
      mediaCodec.setParameters(params);
      // Sure would be nice to honor the frameRate argument to this function,
      // but MediaCodec doesn't expose that particular knob.  b/12977358
      return true;
    } catch (IllegalStateException e) {
      Log.e(TAG, "setRates failed", e);
      return false;
    }
  }

  // Dequeue an input buffer and return its index, -1 if no input buffer is
  // available, or -2 if the codec is no longer operative.
  private int dequeueInputBuffer() {
    checkOnMediaCodecThread();
    try {
      return mediaCodec.dequeueInputBuffer(DEQUEUE_TIMEOUT);
    } catch (IllegalStateException e) {
      Log.e(TAG, "dequeueIntputBuffer failed", e);
      return -2;
    }
  }

  // Helper struct for dequeueOutputBuffer() below.
  private static class OutputBufferInfo {
    public OutputBufferInfo(
        int index, ByteBuffer buffer, boolean isKeyFrame,
        long presentationTimestampUs) {
      this.index = index;
      this.buffer = buffer;
      this.isKeyFrame = isKeyFrame;
      this.presentationTimestampUs = presentationTimestampUs;
    }

    private final int index;
    private final ByteBuffer buffer;
    private final boolean isKeyFrame;
    private final long presentationTimestampUs;
  }

  // Dequeue and return an output buffer, or null if no output is ready.  Return
  // a fake OutputBufferInfo with index -1 if the codec is no longer operable.
  private OutputBufferInfo dequeueOutputBuffer() {
    checkOnMediaCodecThread();
    try {
      MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
      int result = mediaCodec.dequeueOutputBuffer(info, DEQUEUE_TIMEOUT);
      if (result >= 0) {
        // MediaCodec doesn't care about Buffer position/remaining/etc so we can
        // mess with them to get a slice and avoid having to pass extra
        // (BufferInfo-related) parameters back to C++.
        ByteBuffer outputBuffer = outputBuffers[result].duplicate();
        outputBuffer.position(info.offset);
        outputBuffer.limit(info.offset + info.size);
        boolean isKeyFrame =
            (info.flags & MediaCodec.BUFFER_FLAG_SYNC_FRAME) != 0;
        return new OutputBufferInfo(
            result, outputBuffer.slice(), isKeyFrame, info.presentationTimeUs);
      } else if (result == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
        outputBuffers = mediaCodec.getOutputBuffers();
        return dequeueOutputBuffer();
      } else if (result == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
        return dequeueOutputBuffer();
      } else if (result == MediaCodec.INFO_TRY_AGAIN_LATER) {
        return null;
      }
      throw new RuntimeException("dequeueOutputBuffer: " + result);
    } catch (IllegalStateException e) {
      Log.e(TAG, "dequeueOutputBuffer failed", e);
      return new OutputBufferInfo(-1, null, false, -1);
    }
  }

  // Release a dequeued output buffer back to the codec for re-use.  Return
  // false if the codec is no longer operable.
  private boolean releaseOutputBuffer(int index) {
    checkOnMediaCodecThread();
    try {
      mediaCodec.releaseOutputBuffer(index, false);
      return true;
    } catch (IllegalStateException e) {
      Log.e(TAG, "releaseOutputBuffer failed", e);
      return false;
    }
  }
}
