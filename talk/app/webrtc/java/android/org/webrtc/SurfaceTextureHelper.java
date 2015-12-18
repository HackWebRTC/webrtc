/*
 * libjingle
 * Copyright 2015 Google Inc.
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
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;

import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Helper class to create and synchronize access to a SurfaceTexture. The caller will get notified
 * of new frames in onTextureFrameAvailable(), and should call returnTextureFrame() when done with
 * the frame. Only one texture frame can be in flight at once, so returnTextureFrame() must be
 * called in order to receive a new frame. Call disconnect() to stop receiveing new frames and
 * release all resources.
 * Note that there is a C++ counter part of this class that optionally can be used. It is used for
 * wrapping texture frames into webrtc::VideoFrames and also handles calling returnTextureFrame()
 * when the webrtc::VideoFrame is no longer used.
 */
class SurfaceTextureHelper {
  private static final String TAG = "SurfaceTextureHelper";
  /**
   * Callback interface for being notified that a new texture frame is available. The calls will be
   * made on a dedicated thread with a bound EGLContext. The thread will be the same throughout the
   * lifetime of the SurfaceTextureHelper instance, but different from the thread calling the
   * SurfaceTextureHelper constructor. The callee is not allowed to make another EGLContext current
   * on the calling thread.
   */
  public interface OnTextureFrameAvailableListener {
    abstract void onTextureFrameAvailable(
        int oesTextureId, float[] transformMatrix, long timestampNs);
  }

  public static SurfaceTextureHelper create(EglBase.Context sharedContext) {
    return create(sharedContext, null);
  }

  /**
   * Construct a new SurfaceTextureHelper sharing OpenGL resources with |sharedContext|. If
   * |handler| is non-null, the callback will be executed on that handler's thread. If |handler| is
   * null, a dedicated private thread is created for the callbacks.
   */
  public static SurfaceTextureHelper create(final EglBase.Context sharedContext,
      final Handler handler) {
    final Handler finalHandler;
    if (handler != null) {
      finalHandler = handler;
    } else {
      final HandlerThread thread = new HandlerThread(TAG);
      thread.start();
      finalHandler = new Handler(thread.getLooper());
    }
    // The onFrameAvailable() callback will be executed on the SurfaceTexture ctor thread. See:
    // http://grepcode.com/file/repository.grepcode.com/java/ext/com.google.android/android/5.1.1_r1/android/graphics/SurfaceTexture.java#195.
    // Therefore, in order to control the callback thread on API lvl < 21, the SurfaceTextureHelper
    // is constructed on the |handler| thread.
    return ThreadUtils.invokeUninterruptibly(finalHandler, new Callable<SurfaceTextureHelper>() {
      @Override public SurfaceTextureHelper call() {
        return new SurfaceTextureHelper(sharedContext, finalHandler, (handler == null));
      }
    });
  }

  // State for YUV conversion, instantiated on demand.
  static private class YuvConverter {
    private final EglBase eglBase;
    private final GlShader shader;
    private boolean released = false;

    // Vertex coordinates in Normalized Device Coordinates, i.e.
    // (-1, -1) is bottom-left and (1, 1) is top-right.
    private static final FloatBuffer DEVICE_RECTANGLE =
        GlUtil.createFloatBuffer(new float[] {
              -1.0f, -1.0f,  // Bottom left.
               1.0f, -1.0f,  // Bottom right.
              -1.0f,  1.0f,  // Top left.
               1.0f,  1.0f,  // Top right.
            });

    // Texture coordinates - (0, 0) is bottom-left and (1, 1) is top-right.
    private static final FloatBuffer TEXTURE_RECTANGLE =
        GlUtil.createFloatBuffer(new float[] {
              0.0f, 0.0f,  // Bottom left.
              1.0f, 0.0f,  // Bottom right.
              0.0f, 1.0f,  // Top left.
              1.0f, 1.0f   // Top right.
            });

    private static final String VERTEX_SHADER =
        "varying vec2 interp_tc;\n"
      + "attribute vec4 in_pos;\n"
      + "attribute vec4 in_tc;\n"
      + "\n"
      + "uniform mat4 texMatrix;\n"
      + "\n"
      + "void main() {\n"
      + "    gl_Position = in_pos;\n"
      + "    interp_tc = (texMatrix * in_tc).xy;\n"
      + "}\n";

    private static final String FRAGMENT_SHADER =
        "#extension GL_OES_EGL_image_external : require\n"
      + "precision mediump float;\n"
      + "varying vec2 interp_tc;\n"
      + "\n"
      + "uniform samplerExternalOES oesTex;\n"
      // Difference in texture coordinate corresponding to one
      // sub-pixel in the x direction.
      + "uniform vec2 xUnit;\n"
      // Color conversion coefficients, including constant term
      + "uniform vec4 coeffs;\n"
      + "\n"
      + "void main() {\n"
      // Since the alpha read from the texture is always 1, this could
      // be written as a mat4 x vec4 multiply. However, that seems to
      // give a worse framerate, possibly because the additional
      // multiplies by 1.0 consume resources. TODO(nisse): Could also
      // try to do it as a vec3 x mat3x4, followed by an add in of a
      // constant vector.
      + "  gl_FragColor.r = coeffs.a + dot(coeffs.rgb,\n"
      + "      texture2D(oesTex, interp_tc - 1.5 * xUnit).rgb);\n"
      + "  gl_FragColor.g = coeffs.a + dot(coeffs.rgb,\n"
      + "      texture2D(oesTex, interp_tc - 0.5 * xUnit).rgb);\n"
      + "  gl_FragColor.b = coeffs.a + dot(coeffs.rgb,\n"
      + "      texture2D(oesTex, interp_tc + 0.5 * xUnit).rgb);\n"
      + "  gl_FragColor.a = coeffs.a + dot(coeffs.rgb,\n"
      + "      texture2D(oesTex, interp_tc + 1.5 * xUnit).rgb);\n"
      + "}\n";

    private int texMatrixLoc;
    private int xUnitLoc;
    private int coeffsLoc;;

    YuvConverter (EglBase.Context sharedContext) {
      eglBase = EglBase.create(sharedContext, EglBase.CONFIG_PIXEL_RGBA_BUFFER);
      eglBase.createDummyPbufferSurface();
      eglBase.makeCurrent();

      shader = new GlShader(VERTEX_SHADER, FRAGMENT_SHADER);
      shader.useProgram();
      texMatrixLoc = shader.getUniformLocation("texMatrix");
      xUnitLoc = shader.getUniformLocation("xUnit");
      coeffsLoc = shader.getUniformLocation("coeffs");
      GLES20.glUniform1i(shader.getUniformLocation("oesTex"), 0);
      GlUtil.checkNoGLES2Error("Initialize fragment shader uniform values.");
      // Initialize vertex shader attributes.
      shader.setVertexAttribArray("in_pos", 2, DEVICE_RECTANGLE);
      // If the width is not a multiple of 4 pixels, the texture
      // will be scaled up slightly and clipped at the right border.
      shader.setVertexAttribArray("in_tc", 2, TEXTURE_RECTANGLE);
      eglBase.detachCurrent();
    }

    synchronized void convert(ByteBuffer buf,
        int width, int height, int stride, int textureId, float [] transformMatrix) {
      if (released) {
        throw new IllegalStateException(
            "YuvConverter.convert called on released object");
      }

      // We draw into a buffer laid out like
      //
      //    +---------+
      //    |         |
      //    |  Y      |
      //    |         |
      //    |         |
      //    +----+----+
      //    | U  | V  |
      //    |    |    |
      //    +----+----+
      //
      // In memory, we use the same stride for all of Y, U and V. The
      // U data starts at offset |height| * |stride| from the Y data,
      // and the V data starts at at offset |stride/2| from the U
      // data, with rows of U and V data alternating.
      //
      // Now, it would have made sense to allocate a pixel buffer with
      // a single byte per pixel (EGL10.EGL_COLOR_BUFFER_TYPE,
      // EGL10.EGL_LUMINANCE_BUFFER,), but that seems to be
      // unsupported by devices. So do the following hack: Allocate an
      // RGBA buffer, of width |stride|/4. To render each of these
      // large pixels, sample the texture at 4 different x coordinates
      // and store the results in the four components.
      //
      // Since the V data needs to start on a boundary of such a
      // larger pixel, it is not sufficient that |stride| is even, it
      // has to be a multiple of 8 pixels.

      if (stride % 8 != 0) {
        throw new IllegalArgumentException(
            "Invalid stride, must be a multiple of 8");
      }
      if (stride < width){
        throw new IllegalArgumentException(
            "Invalid stride, must >= width");
      }

      int y_width = (width+3) / 4;
      int uv_width = (width+7) / 8;
      int uv_height = (height+1)/2;
      int total_height = height + uv_height;
      int size = stride * total_height;

      if (buf.capacity() < size) {
        throw new IllegalArgumentException("YuvConverter.convert called with too small buffer");
      }
      // Produce a frame buffer starting at top-left corner, not
      // bottom-left.
      transformMatrix =
          RendererCommon.multiplyMatrices(transformMatrix,
              RendererCommon.verticalFlipMatrix());

      // Create new pBuffferSurface with the correct size if needed.
      if (eglBase.hasSurface()) {
        if (eglBase.surfaceWidth() != stride/4 ||
            eglBase.surfaceHeight() != total_height){
          eglBase.releaseSurface();
          eglBase.createPbufferSurface(stride/4, total_height);
        }
      } else {
        eglBase.createPbufferSurface(stride/4, total_height);
      }

      eglBase.makeCurrent();

      GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
      GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, textureId);
      GLES20.glUniformMatrix4fv(texMatrixLoc, 1, false, transformMatrix, 0);

      // Draw Y
      GLES20.glViewport(0, 0, y_width, height);
      // Matrix * (1;0;0;0) / width. Note that opengl uses column major order.
      GLES20.glUniform2f(xUnitLoc,
          transformMatrix[0] / width,
          transformMatrix[1] / width);
      // Y'UV444 to RGB888, see
      // https://en.wikipedia.org/wiki/YUV#Y.27UV444_to_RGB888_conversion.
      // We use the ITU-R coefficients for U and V */
      GLES20.glUniform4f(coeffsLoc, 0.299f, 0.587f, 0.114f, 0.0f);
      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

      // Draw U
      GLES20.glViewport(0, height, uv_width, uv_height);
      // Matrix * (1;0;0;0) / (2*width). Note that opengl uses column major order.
      GLES20.glUniform2f(xUnitLoc,
          transformMatrix[0] / (2.0f*width),
          transformMatrix[1] / (2.0f*width));
      GLES20.glUniform4f(coeffsLoc, -0.169f, -0.331f, 0.499f, 0.5f);
      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

      // Draw V
      GLES20.glViewport(stride/8, height, uv_width, uv_height);
      GLES20.glUniform4f(coeffsLoc, 0.499f, -0.418f, -0.0813f, 0.5f);
      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

      GLES20.glReadPixels(0, 0, stride/4, total_height, GLES20.GL_RGBA,
          GLES20.GL_UNSIGNED_BYTE, buf);

      GlUtil.checkNoGLES2Error("YuvConverter.convert");

      // Unbind texture. Reportedly needed on some devices to get
      // the texture updated from the camera.
      GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, 0);
      eglBase.detachCurrent();
    }

    synchronized void release() {
      released = true;
      eglBase.makeCurrent();
      shader.release();
      eglBase.release();
    }
  }

  private final Handler handler;
  private boolean isOwningThread;
  private final EglBase eglBase;
  private final SurfaceTexture surfaceTexture;
  private final int oesTextureId;
  private YuvConverter yuvConverter;

  private OnTextureFrameAvailableListener listener;
  // The possible states of this class.
  private boolean hasPendingTexture = false;
  private volatile boolean isTextureInUse = false;
  private boolean isQuitting = false;

  private SurfaceTextureHelper(EglBase.Context sharedContext,
      Handler handler, boolean isOwningThread) {
    if (handler.getLooper().getThread() != Thread.currentThread()) {
      throw new IllegalStateException("SurfaceTextureHelper must be created on the handler thread");
    }
    this.handler = handler;
    this.isOwningThread = isOwningThread;

    eglBase = EglBase.create(sharedContext, EglBase.CONFIG_PIXEL_BUFFER);
    eglBase.createDummyPbufferSurface();
    eglBase.makeCurrent();

    oesTextureId = GlUtil.generateTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES);
    surfaceTexture = new SurfaceTexture(oesTextureId);
  }

  private YuvConverter getYuvConverter() {
    // yuvConverter is assigned once
    if (yuvConverter != null)
      return yuvConverter;

    synchronized(this) {
      if (yuvConverter == null)
        yuvConverter = new YuvConverter(eglBase.getEglBaseContext());
      return yuvConverter;
    }
  }

  /**
   *  Start to stream textures to the given |listener|.
   *  A Listener can only be set once.
   */
  public void setListener(OnTextureFrameAvailableListener listener) {
    if (this.listener != null) {
      throw new IllegalStateException("SurfaceTextureHelper listener has already been set.");
    }
    this.listener = listener;
    surfaceTexture.setOnFrameAvailableListener(new SurfaceTexture.OnFrameAvailableListener() {
      @Override
      public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        hasPendingTexture = true;
        tryDeliverTextureFrame();
      }
    });
  }

  /**
   * Retrieve the underlying SurfaceTexture. The SurfaceTexture should be passed in to a video
   * producer such as a camera or decoder.
   */
  public SurfaceTexture getSurfaceTexture() {
    return surfaceTexture;
  }

  /**
   * Call this function to signal that you are done with the frame received in
   * onTextureFrameAvailable(). Only one texture frame can be in flight at once, so you must call
   * this function in order to receive a new frame.
   */
  public void returnTextureFrame() {
    handler.post(new Runnable() {
      @Override public void run() {
        isTextureInUse = false;
        if (isQuitting) {
          release();
        } else {
          tryDeliverTextureFrame();
        }
      }
    });
  }

  public boolean isTextureInUse() {
    return isTextureInUse;
  }

  /**
   * Call disconnect() to stop receiving frames. Resources are released when the texture frame has
   * been returned by a call to returnTextureFrame(). You are guaranteed to not receive any more
   * onTextureFrameAvailable() after this function returns.
   */
  public void disconnect() {
    if (!isOwningThread) {
      throw new IllegalStateException("Must call disconnect(handler).");
    }
    if (handler.getLooper().getThread() == Thread.currentThread()) {
      isQuitting = true;
      if (!isTextureInUse) {
        release();
      }
      return;
    }
    final CountDownLatch barrier = new CountDownLatch(1);
    handler.postAtFrontOfQueue(new Runnable() {
      @Override public void run() {
        isQuitting = true;
        barrier.countDown();
        if (!isTextureInUse) {
          release();
        }
      }
    });
    ThreadUtils.awaitUninterruptibly(barrier);
  }

  /**
   * Call disconnect() to stop receiving frames and quit the looper used by |handler|.
   * Resources are released when the texture frame has been returned by a call to
   * returnTextureFrame(). You are guaranteed to not receive any more
   * onTextureFrameAvailable() after this function returns.
   */
  public void disconnect(Handler handler) {
    if (this.handler != handler) {
      throw new IllegalStateException("Wrong handler.");
    }
    isOwningThread = true;
    disconnect();
  }

  public void textureToYUV(ByteBuffer buf,
      int width, int height, int stride, int textureId, float [] transformMatrix) {
    if (textureId != oesTextureId)
      throw new IllegalStateException("textureToByteBuffer called with unexpected textureId");

    getYuvConverter().convert(buf, width, height, stride, textureId, transformMatrix);
  }

  private void tryDeliverTextureFrame() {
    if (handler.getLooper().getThread() != Thread.currentThread()) {
      throw new IllegalStateException("Wrong thread.");
    }
    if (isQuitting || !hasPendingTexture || isTextureInUse) {
      return;
    }
    isTextureInUse = true;
    hasPendingTexture = false;

    eglBase.makeCurrent();
    surfaceTexture.updateTexImage();

    final float[] transformMatrix = new float[16];
    surfaceTexture.getTransformMatrix(transformMatrix);
    final long timestampNs = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH)
        ? surfaceTexture.getTimestamp()
        : TimeUnit.MILLISECONDS.toNanos(SystemClock.elapsedRealtime());
    listener.onTextureFrameAvailable(oesTextureId, transformMatrix, timestampNs);
  }

  private void release() {
    if (handler.getLooper().getThread() != Thread.currentThread()) {
      throw new IllegalStateException("Wrong thread.");
    }
    if (isTextureInUse || !isQuitting) {
      throw new IllegalStateException("Unexpected release.");
    }
    synchronized (this) {
      if (yuvConverter != null)
        yuvConverter.release();
    }
    eglBase.makeCurrent();
    GLES20.glDeleteTextures(1, new int[] {oesTextureId}, 0);
    surfaceTexture.release();
    eglBase.release();
    handler.getLooper().quit();
  }
}
