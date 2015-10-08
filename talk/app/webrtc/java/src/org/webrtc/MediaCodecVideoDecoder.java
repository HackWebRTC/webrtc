/*
 * libjingle
 * Copyright 2014 Google Inc.
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
import android.view.Surface;

import org.webrtc.Logging;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

// Java-side of peerconnection_jni.cc:MediaCodecVideoDecoder.
// This class is an implementation detail of the Java PeerConnection API.
// MediaCodec is thread-hostile so this class must be operated on a single
// thread.
public class MediaCodecVideoDecoder {
  // This class is constructed, operated, and destroyed by its C++ incarnation,
  // so the class and its methods have non-public visibility.  The API this
  // class exposes aims to mimic the webrtc::VideoDecoder API as closely as
  // possibly to minimize the amount of translation work necessary.

  private static final String TAG = "MediaCodecVideoDecoder";

  // Tracks webrtc::VideoCodecType.
  public enum VideoCodecType {
    VIDEO_CODEC_VP8,
    VIDEO_CODEC_VP9,
    VIDEO_CODEC_H264
  }

  private static final int DEQUEUE_INPUT_TIMEOUT = 500000;  // 500 ms timeout.
  private static Thread mediaCodecThread;
  private MediaCodec mediaCodec;
  private ByteBuffer[] inputBuffers;
  private ByteBuffer[] outputBuffers;
  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  private static final String H264_MIME_TYPE = "video/avc";
  // List of supported HW VP8 decoders.
  private static final String[] supportedVp8HwCodecPrefixes =
    {"OMX.qcom.", "OMX.Nvidia.", "OMX.Exynos.", "OMX.Intel." };
  // List of supported HW H.264 decoders.
  private static final String[] supportedH264HwCodecPrefixes =
    {"OMX.qcom.", "OMX.Intel." };
  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  private static final int
    COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;
  // Allowable color formats supported by codec - in order of preference.
  private static final List<Integer> supportedColorList = Arrays.asList(
    CodecCapabilities.COLOR_FormatYUV420Planar,
    CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
    CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar,
    COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m);
  private int colorFormat;
  private int width;
  private int height;
  private int stride;
  private int sliceHeight;
  private boolean useSurface;
  // |isWaitingForTexture| is true when waiting for the transition:
  // MediaCodec.releaseOutputBuffer() -> onTextureFrameAvailable().
  private boolean isWaitingForTexture = false;
  private TextureListener textureListener;
  private Surface surface = null;

  private MediaCodecVideoDecoder() { }

  // Helper struct for findVp8Decoder() below.
  private static class DecoderProperties {
    public DecoderProperties(String codecName, int colorFormat) {
      this.codecName = codecName;
      this.colorFormat = colorFormat;
    }
    public final String codecName; // OpenMax component name for VP8 codec.
    public final int colorFormat;  // Color format supported by codec.
  }

  private static DecoderProperties findDecoder(
      String mime, String[] supportedCodecPrefixes) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
      return null; // MediaCodec.setParameters is missing.
    }
    for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
      MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
      if (info.isEncoder()) {
        continue;
      }
      String name = null;
      for (String mimeType : info.getSupportedTypes()) {
        if (mimeType.equals(mime)) {
          name = info.getName();
          break;
        }
      }
      if (name == null) {
        continue;  // No HW support in this codec; try the next one.
      }
      Logging.v(TAG, "Found candidate decoder " + name);

      // Check if this is supported decoder.
      boolean supportedCodec = false;
      for (String codecPrefix : supportedCodecPrefixes) {
        if (name.startsWith(codecPrefix)) {
          supportedCodec = true;
          break;
        }
      }
      if (!supportedCodec) {
        continue;
      }

      // Check if codec supports either yuv420 or nv12.
      CodecCapabilities capabilities =
          info.getCapabilitiesForType(mime);
      for (int colorFormat : capabilities.colorFormats) {
        Logging.v(TAG, "   Color: 0x" + Integer.toHexString(colorFormat));
      }
      for (int supportedColorFormat : supportedColorList) {
        for (int codecColorFormat : capabilities.colorFormats) {
          if (codecColorFormat == supportedColorFormat) {
            // Found supported HW decoder.
            Logging.d(TAG, "Found target decoder " + name +
                ". Color: 0x" + Integer.toHexString(codecColorFormat));
            return new DecoderProperties(name, codecColorFormat);
          }
        }
      }
    }
    return null;  // No HW decoder.
  }

  public static boolean isVp8HwSupported() {
    return findDecoder(VP8_MIME_TYPE, supportedVp8HwCodecPrefixes) != null;
  }

  public static boolean isH264HwSupported() {
    return findDecoder(H264_MIME_TYPE, supportedH264HwCodecPrefixes) != null;
  }

  public static void printStackTrace() {
    if (mediaCodecThread != null) {
      StackTraceElement[] mediaCodecStackTraces = mediaCodecThread.getStackTrace();
      if (mediaCodecStackTraces.length > 0) {
        Logging.d(TAG, "MediaCodecVideoDecoder stacks trace:");
        for (StackTraceElement stackTrace : mediaCodecStackTraces) {
          Logging.d(TAG, stackTrace.toString());
        }
      }
    }
  }

  private void checkOnMediaCodecThread() throws IllegalStateException {
    if (mediaCodecThread.getId() != Thread.currentThread().getId()) {
      throw new IllegalStateException(
          "MediaCodecVideoDecoder previously operated on " + mediaCodecThread +
          " but is now called on " + Thread.currentThread());
    }
  }

  // Pass null in |surfaceTextureHelper| to configure the codec for ByteBuffer output.
  private boolean initDecode(
      VideoCodecType type, int width, int height, SurfaceTextureHelper surfaceTextureHelper) {
    if (mediaCodecThread != null) {
      throw new RuntimeException("Forgot to release()?");
    }
    useSurface = (surfaceTextureHelper != null);
    String mime = null;
    String[] supportedCodecPrefixes = null;
    if (type == VideoCodecType.VIDEO_CODEC_VP8) {
      mime = VP8_MIME_TYPE;
      supportedCodecPrefixes = supportedVp8HwCodecPrefixes;
    } else if (type == VideoCodecType.VIDEO_CODEC_H264) {
      mime = H264_MIME_TYPE;
      supportedCodecPrefixes = supportedH264HwCodecPrefixes;
    } else {
      throw new RuntimeException("Non supported codec " + type);
    }
    DecoderProperties properties = findDecoder(mime, supportedCodecPrefixes);
    if (properties == null) {
      throw new RuntimeException("Cannot find HW decoder for " + type);
    }
    Logging.d(TAG, "Java initDecode: " + type + " : "+ width + " x " + height +
        ". Color: 0x" + Integer.toHexString(properties.colorFormat) +
        ". Use Surface: " + useSurface);
    mediaCodecThread = Thread.currentThread();
    try {
      this.width = width;
      this.height = height;
      stride = width;
      sliceHeight = height;

      if (useSurface) {
        textureListener = new TextureListener(surfaceTextureHelper);
        surface = new Surface(surfaceTextureHelper.getSurfaceTexture());
      }

      MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
      if (!useSurface) {
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, properties.colorFormat);
      }
      Logging.d(TAG, "  Format: " + format);
      mediaCodec =
          MediaCodecVideoEncoder.createByCodecName(properties.codecName);
      if (mediaCodec == null) {
        return false;
      }
      mediaCodec.configure(format, surface, null, 0);
      mediaCodec.start();
      colorFormat = properties.colorFormat;
      outputBuffers = mediaCodec.getOutputBuffers();
      inputBuffers = mediaCodec.getInputBuffers();
      Logging.d(TAG, "Input buffers: " + inputBuffers.length +
          ". Output buffers: " + outputBuffers.length);
      return true;
    } catch (IllegalStateException e) {
      Logging.e(TAG, "initDecode failed", e);
      return false;
    }
  }

  private void release() {
    Logging.d(TAG, "Java releaseDecoder");
    checkOnMediaCodecThread();
    try {
      mediaCodec.stop();
      mediaCodec.release();
    } catch (IllegalStateException e) {
      Logging.e(TAG, "release failed", e);
    }
    mediaCodec = null;
    mediaCodecThread = null;
    if (useSurface) {
      surface.release();
      surface = null;
      textureListener.release();
    }
  }

  // Dequeue an input buffer and return its index, -1 if no input buffer is
  // available, or -2 if the codec is no longer operative.
  private int dequeueInputBuffer() {
    checkOnMediaCodecThread();
    try {
      return mediaCodec.dequeueInputBuffer(DEQUEUE_INPUT_TIMEOUT);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "dequeueIntputBuffer failed", e);
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
      Logging.e(TAG, "decode failed", e);
      return false;
    }
  }

  // Helper structs for dequeueOutputBuffer() below.
  private static class DecodedByteBuffer {
    public DecodedByteBuffer(int index, int offset, int size, long presentationTimestampUs) {
      this.index = index;
      this.offset = offset;
      this.size = size;
      this.presentationTimestampUs = presentationTimestampUs;
    }

    private final int index;
    private final int offset;
    private final int size;
    private final long presentationTimestampUs;
  }

  private static class DecodedTextureBuffer {
    private final int textureID;
    private final float[] transformMatrix;
    private final long timestampNs;

    public DecodedTextureBuffer(int textureID, float[] transformMatrix, long timestampNs) {
      this.textureID = textureID;
      this.transformMatrix = transformMatrix;
      this.timestampNs = timestampNs;
    }
  }

  // Poll based texture listener.
  private static class TextureListener
      implements SurfaceTextureHelper.OnTextureFrameAvailableListener {
    private final SurfaceTextureHelper surfaceTextureHelper;
    private DecodedTextureBuffer textureBuffer;
    // |newFrameLock| is used to synchronize arrival of new frames with wait()/notifyAll().
    private final Object newFrameLock = new Object();

    public TextureListener(SurfaceTextureHelper surfaceTextureHelper) {
      this.surfaceTextureHelper = surfaceTextureHelper;
      surfaceTextureHelper.setListener(this);
    }

    // Callback from |surfaceTextureHelper|. May be called on an arbitrary thread.
    @Override
    public void onTextureFrameAvailable(
        int oesTextureId, float[] transformMatrix, long timestampNs) {
      synchronized (newFrameLock) {
        if (textureBuffer != null) {
          Logging.e(TAG,
              "Unexpected onTextureFrameAvailable() called while already holding a texture.");
          throw new IllegalStateException("Already holding a texture.");
        }
        textureBuffer = new DecodedTextureBuffer(oesTextureId, transformMatrix, timestampNs);
        newFrameLock.notifyAll();
      }
    }

    // Dequeues and returns a texture buffer if available, or null otherwise.
    public DecodedTextureBuffer dequeueTextureFrame(int timeoutMs) {
      synchronized (newFrameLock) {
        if (textureBuffer == null && timeoutMs > 0) {
          try {
            newFrameLock.wait(timeoutMs);
          } catch(InterruptedException e) {
            // Restore the interrupted status by reinterrupting the thread.
            Thread.currentThread().interrupt();
          }
        }
        final DecodedTextureBuffer textureBuffer = this.textureBuffer;
        this.textureBuffer = null;
        return textureBuffer;
      }
    }

    public void release() {
      // SurfaceTextureHelper.disconnect() will block until any onTextureFrameAvailable() in
      // progress is done. Therefore, the call to disconnect() must be outside any synchronized
      // statement that is also used in the onTextureFrameAvailable() above to avoid deadlocks.
      surfaceTextureHelper.disconnect();
      synchronized (newFrameLock) {
        if (textureBuffer != null) {
          surfaceTextureHelper.returnTextureFrame();
          textureBuffer = null;
        }
      }
    }
  }

  // Returns null if no decoded buffer is available, and otherwise either a DecodedByteBuffer or
  // DecodedTexturebuffer depending on |useSurface| configuration.
  // Throws IllegalStateException if call is made on the wrong thread, if color format changes to an
  // unsupported format, or if |mediaCodec| is not in the Executing state. Throws CodecException
  // upon codec error.
  private Object dequeueOutputBuffer(int dequeueTimeoutMs)
      throws IllegalStateException, MediaCodec.CodecException {
    checkOnMediaCodecThread();
    // Calling multiple MediaCodec.releaseOutputBuffer() with render=true in a row will result in
    // dropped texture frames. Therefore, wait for any pending onTextureFrameAvailable() before
    // proceeding.
    if (isWaitingForTexture) {
      final DecodedTextureBuffer textureBuffer =
          textureListener.dequeueTextureFrame(dequeueTimeoutMs);
      isWaitingForTexture = (textureBuffer == null);
      return textureBuffer;
    }

    // Drain the decoder until receiving a decoded buffer or hitting
    // MediaCodec.INFO_TRY_AGAIN_LATER.
    final MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
    while (true) {
      final int result = mediaCodec.dequeueOutputBuffer(
          info, TimeUnit.MILLISECONDS.toMicros(dequeueTimeoutMs));
      switch (result) {
        case MediaCodec.INFO_TRY_AGAIN_LATER:
          return null;
        case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
          outputBuffers = mediaCodec.getOutputBuffers();
          Logging.d(TAG, "Decoder output buffers changed: " + outputBuffers.length);
          break;
        case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
          MediaFormat format = mediaCodec.getOutputFormat();
          Logging.d(TAG, "Decoder format changed: " + format.toString());
          width = format.getInteger(MediaFormat.KEY_WIDTH);
          height = format.getInteger(MediaFormat.KEY_HEIGHT);
          if (!useSurface && format.containsKey(MediaFormat.KEY_COLOR_FORMAT)) {
            colorFormat = format.getInteger(MediaFormat.KEY_COLOR_FORMAT);
            Logging.d(TAG, "Color: 0x" + Integer.toHexString(colorFormat));
            if (!supportedColorList.contains(colorFormat)) {
              throw new IllegalStateException("Non supported color format: " + colorFormat);
            }
          }
          if (format.containsKey("stride")) {
            stride = format.getInteger("stride");
          }
          if (format.containsKey("slice-height")) {
            sliceHeight = format.getInteger("slice-height");
          }
          Logging.d(TAG, "Frame stride and slice height: " + stride + " x " + sliceHeight);
          stride = Math.max(width, stride);
          sliceHeight = Math.max(height, sliceHeight);
          break;
        default:
          // Output buffer decoded.
          if (useSurface) {
            mediaCodec.releaseOutputBuffer(result, true /* render */);
            final DecodedTextureBuffer textureBuffer =
                textureListener.dequeueTextureFrame(dequeueTimeoutMs);
            isWaitingForTexture = (textureBuffer == null);
            return textureBuffer;
          } else {
            return new DecodedByteBuffer(result, info.offset, info.size, info.presentationTimeUs);
          }
      }
    }
  }

  // Release a dequeued output byte buffer back to the codec for re-use. Should only be called for
  // non-surface decoding.
  // Throws IllegalStateException if the call is made on the wrong thread, if codec is configured
  // for surface decoding, or if |mediaCodec| is not in the Executing state. Throws
  // MediaCodec.CodecException upon codec error.
  private void returnDecodedByteBuffer(int index)
      throws IllegalStateException, MediaCodec.CodecException {
    checkOnMediaCodecThread();
    if (useSurface) {
      throw new IllegalStateException("returnDecodedByteBuffer() called for surface decoding.");
    }
    mediaCodec.releaseOutputBuffer(index, false /* render */);
  }
}
