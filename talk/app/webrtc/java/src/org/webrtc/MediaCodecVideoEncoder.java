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
import android.media.MediaCodecInfo.CodecCapabilities;
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
  private Thread mediaCodecThread;
  private MediaCodec mediaCodec;
  private ByteBuffer[] outputBuffers;
  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  // List of supported HW VP8 codecs.
  private static final String[] supportedHwCodecPrefixes =
    {"OMX.qcom.", "OMX.Nvidia." };
  // Bitrate mode
  private static final int VIDEO_ControlRateConstant = 2;
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

  private MediaCodecVideoEncoder() {}

  // Helper struct for findVp8HwEncoder() below.
  private static class EncoderProperties {
    public EncoderProperties(String codecName, int colorFormat) {
      this.codecName = codecName;
      this.colorFormat = colorFormat;
    }
    public final String codecName; // OpenMax component name for VP8 codec.
    public final int colorFormat;  // Color format supported by codec.
  }

  private static EncoderProperties findVp8HwEncoder() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT)
      return null; // MediaCodec.setParameters is missing.

    for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
      MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
      if (!info.isEncoder()) {
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
      Log.d(TAG, "Found candidate encoder " + name);

      // Check if this is supported HW encoder.
      boolean supportedCodec = false;
      for (String hwCodecPrefix : supportedHwCodecPrefixes) {
        if (name.startsWith(hwCodecPrefix)) {
          supportedCodec = true;
          break;
        }
      }
      if (!supportedCodec) {
        continue;
      }

      CodecCapabilities capabilities =
          info.getCapabilitiesForType(VP8_MIME_TYPE);
      for (int colorFormat : capabilities.colorFormats) {
        Log.d(TAG, "   Color: 0x" + Integer.toHexString(colorFormat));
      }

      // Check if codec supports either yuv420 or nv12.
      for (int supportedColorFormat : supportedColorList) {
        for (int codecColorFormat : capabilities.colorFormats) {
          if (codecColorFormat == supportedColorFormat) {
            // Found supported HW VP8 encoder.
            Log.d(TAG, "Found target encoder " + name +
                ". Color: 0x" + Integer.toHexString(codecColorFormat));
            return new EncoderProperties(name, codecColorFormat);
          }
        }
      }
    }
    return null;  // No HW VP8 encoder.
  }

  private static boolean isPlatformSupported() {
    return findVp8HwEncoder() != null;
  }

  private static int bitRate(int kbps) {
    // webrtc "kilo" means 1000, not 1024.  Apparently.
    // (and the price for overshooting is frame-dropping as webrtc enforces its
    // bandwidth estimation, which is unpleasant).
    // Since the HW encoder in the N5 overshoots, we clamp to a bit less than
    // the requested rate.  Sad but true.  Bug 3194.
    return kbps * 950;
  }

  private void checkOnMediaCodecThread() {
    if (mediaCodecThread.getId() != Thread.currentThread().getId()) {
      throw new RuntimeException(
          "MediaCodecVideoEncoder previously operated on " + mediaCodecThread +
          " but is now called on " + Thread.currentThread());
    }
  }

  // Return the array of input buffers, or null on failure.
  private ByteBuffer[] initEncode(int width, int height, int kbps, int fps) {
    Log.d(TAG, "Java initEncode: " + width + " x " + height +
        ". @ " + kbps + " kbps. Fps: " + fps +
        ". Color: 0x" + Integer.toHexString(colorFormat));
    if (mediaCodecThread != null) {
      throw new RuntimeException("Forgot to release()?");
    }
    EncoderProperties properties = findVp8HwEncoder();
    if (properties == null) {
      throw new RuntimeException("Can not find HW VP8 encoder");
    }
    mediaCodecThread = Thread.currentThread();
    try {
      MediaFormat format =
          MediaFormat.createVideoFormat(VP8_MIME_TYPE, width, height);
      format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate(kbps));
      format.setInteger("bitrate-mode", VIDEO_ControlRateConstant);
      format.setInteger(MediaFormat.KEY_COLOR_FORMAT, properties.colorFormat);
      // Default WebRTC settings
      format.setInteger(MediaFormat.KEY_FRAME_RATE, fps);
      format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 100);
      Log.d(TAG, "  Format: " + format);
      mediaCodec = MediaCodec.createByCodecName(properties.codecName);
      if (mediaCodec == null) {
        return null;
      }
      mediaCodec.configure(
          format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
      mediaCodec.start();
      colorFormat = properties.colorFormat;
      outputBuffers = mediaCodec.getOutputBuffers();
      ByteBuffer[] inputBuffers = mediaCodec.getInputBuffers();
      Log.d(TAG, "Input buffers: " + inputBuffers.length +
          ". Output buffers: " + outputBuffers.length);
      return inputBuffers;
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
        Log.d(TAG, "Sync frame request");
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
    Log.d(TAG, "Java releaseEncoder");
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
    // frameRate argument is ignored - HW encoder is supposed to use
    // video frame timestamps for bit allocation.
    checkOnMediaCodecThread();
    Log.v(TAG, "setRates: " + kbps + " kbps. Fps: " + frameRateIgnored);
    try {
      Bundle params = new Bundle();
      params.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, bitRate(kbps));
      mediaCodec.setParameters(params);
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
        if (isKeyFrame) {
          Log.d(TAG, "Sync frame generated");
        }
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
