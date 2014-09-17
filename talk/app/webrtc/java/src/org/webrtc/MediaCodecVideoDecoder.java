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

import android.graphics.SurfaceTexture;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;

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

  private static final int DEQUEUE_INPUT_TIMEOUT = 500000;  // 500 ms timeout.
  private Thread mediaCodecThread;
  private MediaCodec mediaCodec;
  private ByteBuffer[] inputBuffers;
  private ByteBuffer[] outputBuffers;
  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  // List of supported HW VP8 decoders.
  private static final String[] supportedHwCodecPrefixes =
    {"OMX.qcom.", "OMX.Nvidia." };
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
  private boolean useSurface;
  private int textureID = -1;
  private SurfaceTexture surfaceTexture = null;
  private Surface surface = null;
  private float[] stMatrix = new float[16];
  private EGLDisplay eglDisplay = EGL14.EGL_NO_DISPLAY;
  private EGLContext eglContext = EGL14.EGL_NO_CONTEXT;
  private EGLSurface eglSurface = EGL14.EGL_NO_SURFACE;


  private MediaCodecVideoDecoder() { }

  // Helper struct for findVp8HwDecoder() below.
  private static class DecoderProperties {
    public DecoderProperties(String codecName, int colorFormat) {
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

      // Check if this is supported HW decoder.
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

      // Check if codec supports either yuv420 or nv12.
      CodecCapabilities capabilities =
          info.getCapabilitiesForType(VP8_MIME_TYPE);
      for (int colorFormat : capabilities.colorFormats) {
        Log.d(TAG, "   Color: 0x" + Integer.toHexString(colorFormat));
      }
      for (int supportedColorFormat : supportedColorList) {
        for (int codecColorFormat : capabilities.colorFormats) {
          if (codecColorFormat == supportedColorFormat) {
            // Found supported HW VP8 decoder.
            Log.d(TAG, "Found target decoder " + name +
                ". Color: 0x" + Integer.toHexString(codecColorFormat));
            return new DecoderProperties(name, codecColorFormat);
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

  private void checkEglError(String msg) {
    int error;
    if ((error = EGL14.eglGetError()) != EGL14.EGL_SUCCESS) {
      Log.e(TAG, msg + ": EGL Error: 0x" + Integer.toHexString(error));
      throw new RuntimeException(
          msg + ": EGL error: 0x" + Integer.toHexString(error));
    }
  }

  private void checkGlError(String msg) {
    int error;
    if ((error = GLES20.glGetError()) != GLES20.GL_NO_ERROR) {
      Log.e(TAG, msg + ": GL Error: 0x" + Integer.toHexString(error));
      throw new RuntimeException(
          msg + ": GL Error: 0x " + Integer.toHexString(error));
    }
  }

  private void eglSetup(EGLContext sharedContext, int width, int height) {
    Log.d(TAG, "EGL setup");
    if (sharedContext == null) {
      sharedContext = EGL14.EGL_NO_CONTEXT;
    }
    eglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL14.EGL_NO_DISPLAY) {
      throw new RuntimeException("Unable to get EGL14 display");
    }
    int[] version = new int[2];
    if (!EGL14.eglInitialize(eglDisplay, version, 0, version, 1)) {
      throw new RuntimeException("Unable to initialize EGL14");
    }

    // Configure EGL for pbuffer and OpenGL ES 2.0.
    int[] attribList = {
      EGL14.EGL_RED_SIZE, 8,
      EGL14.EGL_GREEN_SIZE, 8,
      EGL14.EGL_BLUE_SIZE, 8,
      EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
      EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
      EGL14.EGL_NONE
    };
    EGLConfig[] configs = new EGLConfig[1];
    int[] numConfigs = new int[1];
    if (!EGL14.eglChooseConfig(eglDisplay, attribList, 0, configs, 0,
        configs.length, numConfigs, 0)) {
      throw new RuntimeException("Unable to find RGB888 EGL config");
    }

    // Configure context for OpenGL ES 2.0.
    int[] attrib_list = {
      EGL14.EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL14.EGL_NONE
    };
    eglContext = EGL14.eglCreateContext(eglDisplay, configs[0], sharedContext,
        attrib_list, 0);
    checkEglError("eglCreateContext");
    if (eglContext == null) {
      throw new RuntimeException("Null EGL context");
    }

    // Create a pbuffer surface.
    int[] surfaceAttribs = {
      EGL14.EGL_WIDTH, width,
      EGL14.EGL_HEIGHT, height,
      EGL14.EGL_NONE
    };
    eglSurface = EGL14.eglCreatePbufferSurface(eglDisplay, configs[0],
        surfaceAttribs, 0);
    checkEglError("eglCreatePbufferSurface");
    if (eglSurface == null) {
      throw new RuntimeException("EGL surface was null");
    }
  }

  private void eglRelease() {
    Log.d(TAG, "EGL release");
    if (eglDisplay != EGL14.EGL_NO_DISPLAY) {
      EGL14.eglDestroySurface(eglDisplay, eglSurface);
      EGL14.eglDestroyContext(eglDisplay, eglContext);
      EGL14.eglReleaseThread();
      EGL14.eglTerminate(eglDisplay);
    }
    eglDisplay = EGL14.EGL_NO_DISPLAY;
    eglContext = EGL14.EGL_NO_CONTEXT;
    eglSurface = EGL14.EGL_NO_SURFACE;
  }


  private void makeCurrent() {
    if (!EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
      throw new RuntimeException("eglMakeCurrent failed");
    }
  }

  private boolean initDecode(int width, int height, boolean useSurface,
      EGLContext sharedContext) {
    if (mediaCodecThread != null) {
      throw new RuntimeException("Forgot to release()?");
    }
    if (useSurface && sharedContext == null) {
      throw new RuntimeException("No shared EGL context.");
    }
    DecoderProperties properties = findVp8HwDecoder();
    if (properties == null) {
      throw new RuntimeException("Cannot find HW VP8 decoder");
    }
    Log.d(TAG, "Java initDecode: " + width + " x " + height +
        ". Color: 0x" + Integer.toHexString(properties.colorFormat) +
        ". Use Surface: " + useSurface );
    if (sharedContext != null) {
      Log.d(TAG, "Decoder shared EGL Context: " + sharedContext);
    }
    mediaCodecThread = Thread.currentThread();
    try {
      Surface decodeSurface = null;
      this.width = width;
      this.height = height;
      this.useSurface = useSurface;
      stride = width;
      sliceHeight = height;

      if (useSurface) {
        // Create shared EGL context.
        eglSetup(sharedContext, width, height);
        makeCurrent();

        // Create output surface
        int[] textures = new int[1];
        GLES20.glGenTextures(1, textures, 0);
        checkGlError("glGenTextures");
        textureID = textures[0];
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, textureID);
        checkGlError("glBindTexture mTextureID");

        GLES20.glTexParameterf(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
        GLES20.glTexParameterf(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
        checkGlError("glTexParameter");
        Log.d(TAG, "Video decoder TextureID = " + textureID);
        surfaceTexture = new SurfaceTexture(textureID);
        surface = new Surface(surfaceTexture);
        decodeSurface = surface;
     }

      MediaFormat format =
          MediaFormat.createVideoFormat(VP8_MIME_TYPE, width, height);
      if (!useSurface) {
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, properties.colorFormat);
      }
      Log.d(TAG, "  Format: " + format);
      mediaCodec = MediaCodec.createByCodecName(properties.codecName);
      if (mediaCodec == null) {
        return false;
      }
      mediaCodec.configure(format, decodeSurface, null, 0);
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
    if (useSurface) {
      surface.release();
      surface = null;
      surfaceTexture = null;
      if (textureID >= 0) {
        int[] textures = new int[1];
        textures[0] = textureID;
        Log.d(TAG, "Delete video decoder TextureID " + textureID);
        GLES20.glDeleteTextures(1, textures, 0);
        checkGlError("glDeleteTextures");
      }
      eglRelease();
    }
  }

  // Dequeue an input buffer and return its index, -1 if no input buffer is
  // available, or -2 if the codec is no longer operative.
  private int dequeueInputBuffer() {
    checkOnMediaCodecThread();
    try {
      return mediaCodec.dequeueInputBuffer(DEQUEUE_INPUT_TIMEOUT);
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

  // Helper struct for dequeueOutputBuffer() below.
  private static class DecoderOutputBufferInfo {
    public DecoderOutputBufferInfo(
        int index, int offset, int size, long presentationTimestampUs) {
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

  // Dequeue and return an output buffer index, -1 if no output
  // buffer available or -2 if error happened.
  private DecoderOutputBufferInfo dequeueOutputBuffer(int dequeueTimeoutUs) {
    checkOnMediaCodecThread();
    try {
      MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
      int result = mediaCodec.dequeueOutputBuffer(info, dequeueTimeoutUs);
      while (result == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED ||
          result == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
        if (result == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
          outputBuffers = mediaCodec.getOutputBuffers();
          Log.d(TAG, "Output buffers changed: " + outputBuffers.length);
        } else if (result == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
          MediaFormat format = mediaCodec.getOutputFormat();
          Log.d(TAG, "Format changed: " + format.toString());
          width = format.getInteger(MediaFormat.KEY_WIDTH);
          height = format.getInteger(MediaFormat.KEY_HEIGHT);
          if (!useSurface && format.containsKey(MediaFormat.KEY_COLOR_FORMAT)) {
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
              return new DecoderOutputBufferInfo(-1, 0, 0, -1);
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
        result = mediaCodec.dequeueOutputBuffer(info, dequeueTimeoutUs);
      }
      if (result >= 0) {
        return new DecoderOutputBufferInfo(result, info.offset, info.size,
            info.presentationTimeUs);
      }
      return null;
    } catch (IllegalStateException e) {
      Log.e(TAG, "dequeueOutputBuffer failed", e);
      return new DecoderOutputBufferInfo(-1, 0, 0, -1);
    }
  }

  // Release a dequeued output buffer back to the codec for re-use.  Return
  // false if the codec is no longer operable.
  private boolean releaseOutputBuffer(int index, boolean render) {
    checkOnMediaCodecThread();
    try {
      if (!useSurface) {
        render = false;
      }
      mediaCodec.releaseOutputBuffer(index, render);
      return true;
    } catch (IllegalStateException e) {
      Log.e(TAG, "releaseOutputBuffer failed", e);
      return false;
    }
  }
}
