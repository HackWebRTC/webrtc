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

package org.webrtc;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import java.nio.ByteBuffer;

// Java-side of peerconnection_jni.cc:MediaCodecVideoDecoder.
// This class is an implementation detail of the Java PeerConnection API.
// MediaCodec is thread-hostile so this class must be operated on a single
// thread.
class MediaCodecVideoDecoder {
  // This class is constructed, operated, and destroyed by its C++ incarnation,
  // so the class and its methods have non-public visibility.  The API this
  // class exposes aims to mimic the webrtc::VideoDecoder API as closely as
  // possibly to minimize the amount of translation work necessary.

  private static final String TAG = "MediaCodecVideoDecoder";

  private static final int DEQUEUE_TIMEOUT = 1000000;  // 1 sec timeout.
  private Thread mediaCodecThread;
  private MediaCodec mediaCodec;
  private ByteBuffer[] inputBuffers;
  private ByteBuffer[] outputBuffers;
  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  // List of supported HW VP8 decoders.
  private static final String[] supportedHwCodecPrefixes =
    {"OMX.Nvidia."};
  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  private static final int
    COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;
  // Allowable color formats supported by codec - in order of preference.
  private static final int[] supportedColorList = {
    CodecCapabilities.COLOR_FormatYUV420Planar,
    CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
    CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar,
    COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m
  };
  private int colorFormat;
  private int width;
  private int height;
  private int stride;
  private int sliceHeight;

  private MediaCodecVideoDecoder() { }

  // Helper struct for findVp8HwDecoder() below.
  private static class DecoderProperties {
    DecoderProperties(String codecName, int colorFormat) {
      this.codecName = codecName;
      this.colorFormat = colorFormat;
    }
    public final String codecName; // OpenMax component name for VP8 codec.
    public final int colorFormat;  // Color format supported by codec.
  }

  private static DecoderProperties findVp8HwDecoder() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT)
      return null; // MediaCodec.setParameters is missing.

    for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
      MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
      if (info.isEncoder()) {
        continue;
      }
      String name = null;
      for (String mimeType : info.getSupportedTypes()) {
        if (mimeType.equals(VP8_MIME_TYPE)) {
          name = info.getName();
          break;
        }
      }
      if (name == null) {
        continue;  // No VP8 support in this codec; try the next one.
      }
      Log.d(TAG, "Found candidate decoder " + name);
      CodecCapabilities capabilities =
          info.getCapabilitiesForType(VP8_MIME_TYPE);
      for (int colorFormat : capabilities.colorFormats) {
        Log.d(TAG, "   Color: 0x" + Integer.toHexString(colorFormat));
      }

      // Check if this is supported HW decoder
      for (String hwCodecPrefix : supportedHwCodecPrefixes) {
        if (!name.startsWith(hwCodecPrefix)) {
          continue;
        }
        // Check if codec supports either yuv420 or nv12
        for (int supportedColorFormat : supportedColorList) {
          for (int codecColorFormat : capabilities.colorFormats) {
            if (codecColorFormat == supportedColorFormat) {
              // Found supported HW VP8 decoder
              Log.d(TAG, "Found target decoder " + name +
                  ". Color: 0x" + Integer.toHexString(codecColorFormat));
              return new DecoderProperties(name, codecColorFormat);
            }
          }
        }
      }
    }
    return null;  // No HW VP8 decoder.
  }

  private static boolean isPlatformSupported() {
    return findVp8HwDecoder() != null;
  }

  private void checkOnMediaCodecThread() {
    if (mediaCodecThread.getId() != Thread.currentThread().getId()) {
      throw new RuntimeException(
          "MediaCodecVideoDecoder previously operated on " + mediaCodecThread +
          " but is now called on " + Thread.currentThread());
    }
  }

  private boolean initDecode(int width, int height) {
    if (mediaCodecThread != null) {
      throw new RuntimeException("Forgot to release()?");
    }
    DecoderProperties properties = findVp8HwDecoder();
    if (properties == null) {
      throw new RuntimeException("Cannot find HW VP8 decoder");
    }
    Log.d(TAG, "Java initDecode: " + width + " x " + height +
        ". Color: 0x" + Integer.toHexString(properties.colorFormat));
    mediaCodecThread = Thread.currentThread();
    try {
      this.width = width;
      this.height = height;
      stride = width;
      sliceHeight = height;
      MediaFormat format =
          MediaFormat.createVideoFormat(VP8_MIME_TYPE, width, height);
      format.setInteger(MediaFormat.KEY_COLOR_FORMAT, properties.colorFormat);
      Log.d(TAG, "  Format: " + format);
      mediaCodec = MediaCodec.createByCodecName(properties.codecName);
      if (mediaCodec == null) {
        return false;
      }
      mediaCodec.configure(format, null, null, 0);
      mediaCodec.start();
      colorFormat = properties.colorFormat;
      outputBuffers = mediaCodec.getOutputBuffers();
      inputBuffers = mediaCodec.getInputBuffers();
      Log.d(TAG, "Input buffers: " + inputBuffers.length +
          ". Output buffers: " + outputBuffers.length);
      return true;
    } catch (IllegalStateException e) {
      Log.e(TAG, "initDecode failed", e);
      return false;
    }
  }

  private void release() {
    Log.d(TAG, "Java releaseDecoder");
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

  private boolean queueInputBuffer(
      int inputBufferIndex, int size, long timestampUs) {
    checkOnMediaCodecThread();
    try {
      inputBuffers[inputBufferIndex].position(0);
      inputBuffers[inputBufferIndex].limit(size);
      mediaCodec.queueInputBuffer(inputBufferIndex, 0, size, timestampUs, 0);
      return true;
    }
    catch (IllegalStateException e) {
      Log.e(TAG, "decode failed", e);
      return false;
    }
  }

  // Dequeue and return an output buffer index, -1 if no output
  // buffer available or -2 if error happened.
  private int dequeueOutputBuffer() {
    checkOnMediaCodecThread();
    try {
      MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
      int result = mediaCodec.dequeueOutputBuffer(info, DEQUEUE_TIMEOUT);
      while (result == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED ||
          result == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
        if (result == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
          outputBuffers = mediaCodec.getOutputBuffers();
        } else if (result == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
          MediaFormat format = mediaCodec.getOutputFormat();
          Log.d(TAG, "Format changed: " + format.toString());
          width = format.getInteger(MediaFormat.KEY_WIDTH);
          height = format.getInteger(MediaFormat.KEY_HEIGHT);
          if (format.containsKey(MediaFormat.KEY_COLOR_FORMAT)) {
            colorFormat = format.getInteger(MediaFormat.KEY_COLOR_FORMAT);
            Log.d(TAG, "Color: 0x" + Integer.toHexString(colorFormat));
            // Check if new color space is supported.
            boolean validColorFormat = false;
            for (int supportedColorFormat : supportedColorList) {
              if (colorFormat == supportedColorFormat) {
                validColorFormat = true;
                break;
              }
            }
            if (!validColorFormat) {
              Log.e(TAG, "Non supported color format");
              return -2;
            }
          }
          if (format.containsKey("stride")) {
            stride = format.getInteger("stride");
          }
          if (format.containsKey("slice-height")) {
            sliceHeight = format.getInteger("slice-height");
          }
          Log.d(TAG, "Frame stride and slice height: "
              + stride + " x " + sliceHeight);
          stride = Math.max(width, stride);
          sliceHeight = Math.max(height, sliceHeight);
        }
        result = mediaCodec.dequeueOutputBuffer(info, DEQUEUE_TIMEOUT);
      }
      return result;
    } catch (IllegalStateException e) {
      Log.e(TAG, "dequeueOutputBuffer failed", e);
      return -2;
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
