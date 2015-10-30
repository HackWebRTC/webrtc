/*
 * libjingle
 * Copyright 2013 Google Inc.
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

import org.webrtc.Logging;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;

// Java-side of peerconnection_jni.cc:MediaCodecVideoEncoder.
// This class is an implementation detail of the Java PeerConnection API.
public class MediaCodecVideoEncoder {
  // This class is constructed, operated, and destroyed by its C++ incarnation,
  // so the class and its methods have non-public visibility.  The API this
  // class exposes aims to mimic the webrtc::VideoEncoder API as closely as
  // possibly to minimize the amount of translation work necessary.

  private static final String TAG = "MediaCodecVideoEncoder";

  // Tracks webrtc::VideoCodecType.
  public enum VideoCodecType {
    VIDEO_CODEC_VP8,
    VIDEO_CODEC_VP9,
    VIDEO_CODEC_H264
  }

  private static final int MEDIA_CODEC_RELEASE_TIMEOUT_MS = 5000; // Timeout for codec releasing.
  private static final int DEQUEUE_TIMEOUT = 0;  // Non-blocking, no wait.
  // Active running encoder instance. Set in initDecode() (called from native code)
  // and reset to null in release() call.
  private static MediaCodecVideoEncoder runningInstance = null;
  private static MediaCodecVideoEncoderErrorCallback errorCallback = null;
  private static int codecErrors = 0;

  private Thread mediaCodecThread;
  private MediaCodec mediaCodec;
  private ByteBuffer[] outputBuffers;
  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  private static final String H264_MIME_TYPE = "video/avc";
  // List of supported HW VP8 codecs.
  private static final String[] supportedVp8HwCodecPrefixes =
    {"OMX.qcom.", "OMX.Intel." };
  // List of supported HW H.264 codecs.
  private static final String[] supportedH264HwCodecPrefixes =
    {"OMX.qcom." };
  // List of devices with poor H.264 encoder quality.
  private static final String[] H264_HW_EXCEPTION_MODELS = new String[] {
    // HW H.264 encoder on below devices has poor bitrate control - actual
    // bitrates deviates a lot from the target value.
    "SAMSUNG-SGH-I337",
    "Nexus 7",
    "Nexus 4"
  };

  // Bitrate modes - should be in sync with OMX_VIDEO_CONTROLRATETYPE defined
  // in OMX_Video.h
  private static final int VIDEO_ControlRateVariable = 1;
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
  // Video encoder type.
  private VideoCodecType type;
  // SPS and PPS NALs (Config frame) for H.264.
  private ByteBuffer configData = null;

  private MediaCodecVideoEncoder() {
  }

  // MediaCodec error handler - invoked when critical error happens which may prevent
  // further use of media codec API. Now it means that one of media codec instances
  // is hanging and can no longer be used in the next call.
  public static interface MediaCodecVideoEncoderErrorCallback {
    void onMediaCodecVideoEncoderCriticalError(int codecErrors);
  }

  public static void setErrorCallback(MediaCodecVideoEncoderErrorCallback errorCallback) {
    Logging.d(TAG, "Set error callback");
    MediaCodecVideoEncoder.errorCallback = errorCallback;
  }

  // Helper struct for findHwEncoder() below.
  private static class EncoderProperties {
    public EncoderProperties(String codecName, int colorFormat) {
      this.codecName = codecName;
      this.colorFormat = colorFormat;
    }
    public final String codecName; // OpenMax component name for HW codec.
    public final int colorFormat;  // Color format supported by codec.
  }

  private static EncoderProperties findHwEncoder(
      String mime, String[] supportedHwCodecPrefixes) {
    // MediaCodec.setParameters is missing for JB and below, so bitrate
    // can not be adjusted dynamically.
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
      return null;
    }

    // Check if device is in H.264 exception list.
    if (mime.equals(H264_MIME_TYPE)) {
      List<String> exceptionModels = Arrays.asList(H264_HW_EXCEPTION_MODELS);
      if (exceptionModels.contains(Build.MODEL)) {
        Logging.w(TAG, "Model: " + Build.MODEL + " has black listed H.264 encoder.");
        return null;
      }
    }

    for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
      MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
      if (!info.isEncoder()) {
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
      Logging.v(TAG, "Found candidate encoder " + name);

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

      CodecCapabilities capabilities = info.getCapabilitiesForType(mime);
      for (int colorFormat : capabilities.colorFormats) {
        Logging.v(TAG, "   Color: 0x" + Integer.toHexString(colorFormat));
      }

      // Check if codec supports either yuv420 or nv12.
      for (int supportedColorFormat : supportedColorList) {
        for (int codecColorFormat : capabilities.colorFormats) {
          if (codecColorFormat == supportedColorFormat) {
            // Found supported HW encoder.
            Logging.d(TAG, "Found target encoder for mime " + mime + " : " + name +
                ". Color: 0x" + Integer.toHexString(codecColorFormat));
            return new EncoderProperties(name, codecColorFormat);
          }
        }
      }
    }
    return null;  // No HW VP8 encoder.
  }

  public static boolean isVp8HwSupported() {
    return findHwEncoder(VP8_MIME_TYPE, supportedVp8HwCodecPrefixes) != null;
  }

  public static boolean isH264HwSupported() {
    return findHwEncoder(H264_MIME_TYPE, supportedH264HwCodecPrefixes) != null;
  }

  private void checkOnMediaCodecThread() {
    if (mediaCodecThread.getId() != Thread.currentThread().getId()) {
      throw new RuntimeException(
          "MediaCodecVideoEncoder previously operated on " + mediaCodecThread +
          " but is now called on " + Thread.currentThread());
    }
  }

  public static void printStackTrace() {
    if (runningInstance != null && runningInstance.mediaCodecThread != null) {
      StackTraceElement[] mediaCodecStackTraces = runningInstance.mediaCodecThread.getStackTrace();
      if (mediaCodecStackTraces.length > 0) {
        Logging.d(TAG, "MediaCodecVideoEncoder stacks trace:");
        for (StackTraceElement stackTrace : mediaCodecStackTraces) {
          Logging.d(TAG, stackTrace.toString());
        }
      }
    }
  }

  static MediaCodec createByCodecName(String codecName) {
    try {
      // In the L-SDK this call can throw IOException so in order to work in
      // both cases catch an exception.
      return MediaCodec.createByCodecName(codecName);
    } catch (Exception e) {
      return null;
    }
  }

  // Return the array of input buffers, or null on failure.
  private ByteBuffer[] initEncode(
      VideoCodecType type, int width, int height, int kbps, int fps) {
    Logging.d(TAG, "Java initEncode: " + type + " : " + width + " x " + height +
        ". @ " + kbps + " kbps. Fps: " + fps +
        ". Color: 0x" + Integer.toHexString(colorFormat));
    if (mediaCodecThread != null) {
      throw new RuntimeException("Forgot to release()?");
    }
    this.type = type;
    EncoderProperties properties = null;
    String mime = null;
    int keyFrameIntervalSec = 0;
    if (type == VideoCodecType.VIDEO_CODEC_VP8) {
      mime = VP8_MIME_TYPE;
      properties = findHwEncoder(VP8_MIME_TYPE, supportedVp8HwCodecPrefixes);
      keyFrameIntervalSec = 100;
    } else if (type == VideoCodecType.VIDEO_CODEC_H264) {
      mime = H264_MIME_TYPE;
      properties = findHwEncoder(H264_MIME_TYPE, supportedH264HwCodecPrefixes);
      keyFrameIntervalSec = 20;
    }
    if (properties == null) {
      throw new RuntimeException("Can not find HW encoder for " + type);
    }
    runningInstance = this; // Encoder is now running and can be queried for stack traces.
    mediaCodecThread = Thread.currentThread();
    try {
      MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
      format.setInteger(MediaFormat.KEY_BIT_RATE, 1000 * kbps);
      format.setInteger("bitrate-mode", VIDEO_ControlRateConstant);
      format.setInteger(MediaFormat.KEY_COLOR_FORMAT, properties.colorFormat);
      format.setInteger(MediaFormat.KEY_FRAME_RATE, fps);
      format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, keyFrameIntervalSec);
      Logging.d(TAG, "  Format: " + format);
      mediaCodec = createByCodecName(properties.codecName);
      if (mediaCodec == null) {
        Logging.e(TAG, "Can not create media encoder");
        return null;
      }
      mediaCodec.configure(
          format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
      mediaCodec.start();
      colorFormat = properties.colorFormat;
      outputBuffers = mediaCodec.getOutputBuffers();
      ByteBuffer[] inputBuffers = mediaCodec.getInputBuffers();
      Logging.d(TAG, "Input buffers: " + inputBuffers.length +
          ". Output buffers: " + outputBuffers.length);
      return inputBuffers;
    } catch (IllegalStateException e) {
      Logging.e(TAG, "initEncode failed", e);
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
        Logging.d(TAG, "Sync frame request");
        Bundle b = new Bundle();
        b.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
        mediaCodec.setParameters(b);
      }
      mediaCodec.queueInputBuffer(
          inputBuffer, 0, size, presentationTimestampUs, 0);
      return true;
    }
    catch (IllegalStateException e) {
      Logging.e(TAG, "encode failed", e);
      return false;
    }
  }

  private void release() {
    Logging.d(TAG, "Java releaseEncoder");
    checkOnMediaCodecThread();

    // Run Mediacodec stop() and release() on separate thread since sometime
    // Mediacodec.stop() may hang.
    final CountDownLatch releaseDone = new CountDownLatch(1);

    Runnable runMediaCodecRelease = new Runnable() {
      @Override
      public void run() {
        try {
          Logging.d(TAG, "Java releaseEncoder on release thread");
          mediaCodec.stop();
          mediaCodec.release();
          Logging.d(TAG, "Java releaseEncoder on release thread done");
        } catch (Exception e) {
          Logging.e(TAG, "Media encoder release failed", e);
        }
        releaseDone.countDown();
      }
    };
    new Thread(runMediaCodecRelease).start();

    if (!ThreadUtils.awaitUninterruptibly(releaseDone, MEDIA_CODEC_RELEASE_TIMEOUT_MS)) {
      Logging.e(TAG, "Media encoder release timeout");
      codecErrors++;
      if (errorCallback != null) {
        Logging.e(TAG, "Invoke codec error callback. Errors: " + codecErrors);
        errorCallback.onMediaCodecVideoEncoderCriticalError(codecErrors);
      }
    }

    mediaCodec = null;
    mediaCodecThread = null;
    runningInstance = null;
    Logging.d(TAG, "Java releaseEncoder done");
  }

  private boolean setRates(int kbps, int frameRateIgnored) {
    // frameRate argument is ignored - HW encoder is supposed to use
    // video frame timestamps for bit allocation.
    checkOnMediaCodecThread();
    Logging.v(TAG, "setRates: " + kbps + " kbps. Fps: " + frameRateIgnored);
    try {
      Bundle params = new Bundle();
      params.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, 1000 * kbps);
      mediaCodec.setParameters(params);
      return true;
    } catch (IllegalStateException e) {
      Logging.e(TAG, "setRates failed", e);
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
      Logging.e(TAG, "dequeueIntputBuffer failed", e);
      return -2;
    }
  }

  // Helper struct for dequeueOutputBuffer() below.
  private static class OutputBufferInfo {
    public OutputBufferInfo(
        int index, ByteBuffer buffer,
        boolean isKeyFrame, long presentationTimestampUs) {
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
      // Check if this is config frame and save configuration data.
      if (result >= 0) {
        boolean isConfigFrame =
            (info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0;
        if (isConfigFrame) {
          Logging.d(TAG, "Config frame generated. Offset: " + info.offset +
              ". Size: " + info.size);
          configData = ByteBuffer.allocateDirect(info.size);
          outputBuffers[result].position(info.offset);
          outputBuffers[result].limit(info.offset + info.size);
          configData.put(outputBuffers[result]);
          // Release buffer back.
          mediaCodec.releaseOutputBuffer(result, false);
          // Query next output.
          result = mediaCodec.dequeueOutputBuffer(info, DEQUEUE_TIMEOUT);
        }
      }
      if (result >= 0) {
        // MediaCodec doesn't care about Buffer position/remaining/etc so we can
        // mess with them to get a slice and avoid having to pass extra
        // (BufferInfo-related) parameters back to C++.
        ByteBuffer outputBuffer = outputBuffers[result].duplicate();
        outputBuffer.position(info.offset);
        outputBuffer.limit(info.offset + info.size);
        // Check key frame flag.
        boolean isKeyFrame =
            (info.flags & MediaCodec.BUFFER_FLAG_SYNC_FRAME) != 0;
        if (isKeyFrame) {
          Logging.d(TAG, "Sync frame generated");
        }
        if (isKeyFrame && type == VideoCodecType.VIDEO_CODEC_H264) {
          Logging.d(TAG, "Appending config frame of size " + configData.capacity() +
              " to output buffer with offset " + info.offset + ", size " +
              info.size);
          // For H.264 key frame append SPS and PPS NALs at the start
          ByteBuffer keyFrameBuffer = ByteBuffer.allocateDirect(
              configData.capacity() + info.size);
          configData.rewind();
          keyFrameBuffer.put(configData);
          keyFrameBuffer.put(outputBuffer);
          keyFrameBuffer.position(0);
          return new OutputBufferInfo(result, keyFrameBuffer,
              isKeyFrame, info.presentationTimeUs);
        } else {
          return new OutputBufferInfo(result, outputBuffer.slice(),
              isKeyFrame, info.presentationTimeUs);
        }
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
      Logging.e(TAG, "dequeueOutputBuffer failed", e);
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
      Logging.e(TAG, "releaseOutputBuffer failed", e);
      return false;
    }
  }
}
