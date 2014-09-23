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

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.graphics.SurfaceTexture;
import android.opengl.EGL14;
import android.opengl.EGLContext;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.util.Log;

import org.webrtc.VideoRenderer.I420Frame;

/**
 * Efficiently renders YUV frames using the GPU for CSC.
 * Clients will want first to call setView() to pass GLSurfaceView
 * and then for each video stream either create instance of VideoRenderer using
 * createGui() call or VideoRenderer.Callbacks interface using create() call.
 * Only one instance of the class can be created.
 */
public class VideoRendererGui implements GLSurfaceView.Renderer {
  private static VideoRendererGui instance = null;
  private static final String TAG = "VideoRendererGui";
  private GLSurfaceView surface;
  private static EGLContext eglContext = null;
  // Indicates if SurfaceView.Renderer.onSurfaceCreated was called.
  // If true then for every newly created yuv image renderer createTexture()
  // should be called. The variable is accessed on multiple threads and
  // all accesses are synchronized on yuvImageRenderers' object lock.
  private boolean onSurfaceCreatedCalled;
  private int screenWidth;
  private int screenHeight;
  // List of yuv renderers.
  private ArrayList<YuvImageRenderer> yuvImageRenderers;
  private int yuvProgram;
  private int oesProgram;
  // Types of video scaling:
  // SCALE_ASPECT_FIT - video frame is scaled to fit the size of the view by
  //    maintaining the aspect ratio (black borders may be displayed).
  // SCALE_ASPECT_FILL - video frame is scaled to fill the size of the view by
  //    maintaining the aspect ratio. Some portion of the video frame may be
  //    clipped.
  // SCALE_FILL - video frame is scaled to to fill the size of the view. Video
  //    aspect ratio is changed if necessary.
  private static enum ScalingType
      { SCALE_ASPECT_FIT, SCALE_ASPECT_FILL, SCALE_FILL };

  private final String VERTEX_SHADER_STRING =
      "varying vec2 interp_tc;\n" +
      "attribute vec4 in_pos;\n" +
      "attribute vec2 in_tc;\n" +
      "\n" +
      "void main() {\n" +
      "  gl_Position = in_pos;\n" +
      "  interp_tc = in_tc;\n" +
      "}\n";

  private final String YUV_FRAGMENT_SHADER_STRING =
      "precision mediump float;\n" +
      "varying vec2 interp_tc;\n" +
      "\n" +
      "uniform sampler2D y_tex;\n" +
      "uniform sampler2D u_tex;\n" +
      "uniform sampler2D v_tex;\n" +
      "\n" +
      "void main() {\n" +
      // CSC according to http://www.fourcc.org/fccyvrgb.php
      "  float y = texture2D(y_tex, interp_tc).r;\n" +
      "  float u = texture2D(u_tex, interp_tc).r - 0.5;\n" +
      "  float v = texture2D(v_tex, interp_tc).r - 0.5;\n" +
      "  gl_FragColor = vec4(y + 1.403 * v, " +
      "                      y - 0.344 * u - 0.714 * v, " +
      "                      y + 1.77 * u, 1);\n" +
      "}\n";


  private static final String OES_FRAGMENT_SHADER_STRING =
      "#extension GL_OES_EGL_image_external : require\n" +
      "precision mediump float;\n" +
      "varying vec2 interp_tc;\n" +
      "\n" +
      "uniform samplerExternalOES oes_tex;\n" +
      "\n" +
      "void main() {\n" +
      "  gl_FragColor = texture2D(oes_tex, interp_tc);\n" +
      "}\n";


  private VideoRendererGui(GLSurfaceView surface) {
    this.surface = surface;
    // Create an OpenGL ES 2.0 context.
    surface.setPreserveEGLContextOnPause(true);
    surface.setEGLContextClientVersion(2);
    surface.setRenderer(this);
    surface.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);

    yuvImageRenderers = new ArrayList<YuvImageRenderer>();
  }

  // Poor-man's assert(): die with |msg| unless |condition| is true.
  private static void abortUnless(boolean condition, String msg) {
    if (!condition) {
      throw new RuntimeException(msg);
    }
  }

  // Assert that no OpenGL ES 2.0 error has been raised.
  private static void checkNoGLES2Error() {
    int error = GLES20.glGetError();
    abortUnless(error == GLES20.GL_NO_ERROR, "GLES20 error: " + error);
  }

  // Wrap a float[] in a direct FloatBuffer using native byte order.
  private static FloatBuffer directNativeFloatBuffer(float[] array) {
    FloatBuffer buffer = ByteBuffer.allocateDirect(array.length * 4).order(
        ByteOrder.nativeOrder()).asFloatBuffer();
    buffer.put(array);
    buffer.flip();
    return buffer;
  }

  private int loadShader(int shaderType, String source) {
    int[] result = new int[] {
        GLES20.GL_FALSE
    };
    int shader = GLES20.glCreateShader(shaderType);
    GLES20.glShaderSource(shader, source);
    GLES20.glCompileShader(shader);
    GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, result, 0);
    if (result[0] != GLES20.GL_TRUE) {
      Log.e(TAG, "Could not compile shader " + shaderType + ":" +
          GLES20.glGetShaderInfoLog(shader));
      throw new RuntimeException(GLES20.glGetShaderInfoLog(shader));
    }
    checkNoGLES2Error();
    return shader;
}


  private int createProgram(String vertexSource, String fragmentSource) {
    int vertexShader = loadShader(GLES20.GL_VERTEX_SHADER, vertexSource);
    int fragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER, fragmentSource);
    int program = GLES20.glCreateProgram();
    if (program == 0) {
      throw new RuntimeException("Could not create program");
    }
    GLES20.glAttachShader(program, vertexShader);
    GLES20.glAttachShader(program, fragmentShader);
    GLES20.glLinkProgram(program);
    int[] linkStatus = new int[] {
        GLES20.GL_FALSE
    };
    GLES20.glGetProgramiv(program, GLES20.GL_LINK_STATUS, linkStatus, 0);
    if (linkStatus[0] != GLES20.GL_TRUE) {
      Log.e(TAG, "Could not link program: " +
          GLES20.glGetProgramInfoLog(program));
      throw new RuntimeException(GLES20.glGetProgramInfoLog(program));
    }
    checkNoGLES2Error();
    return program;
}

  /**
   * Class used to display stream of YUV420 frames at particular location
   * on a screen. New video frames are sent to display using renderFrame()
   * call.
   */
  private static class YuvImageRenderer implements VideoRenderer.Callbacks {
    private GLSurfaceView surface;
    private int id;
    private int yuvProgram;
    private int oesProgram;
    private int[] yuvTextures = { -1, -1, -1 };
    private int oesTexture = -1;
    private float[] stMatrix = new float[16];

    // Render frame queue - accessed by two threads. renderFrame() call does
    // an offer (writing I420Frame to render) and early-returns (recording
    // a dropped frame) if that queue is full. draw() call does a peek(),
    // copies frame to texture and then removes it from a queue using poll().
    LinkedBlockingQueue<I420Frame> frameToRenderQueue;
    // Local copy of incoming video frame.
    private I420Frame yuvFrameToRender;
    private I420Frame textureFrameToRender;
    // Type of video frame used for recent frame rendering.
    private static enum RendererType { RENDERER_YUV, RENDERER_TEXTURE };
    private RendererType rendererType;
    private ScalingType scalingType;
    // Flag if renderFrame() was ever called.
    boolean seenFrame;
    // Total number of video frames received in renderFrame() call.
    private int framesReceived;
    // Number of video frames dropped by renderFrame() because previous
    // frame has not been rendered yet.
    private int framesDropped;
    // Number of rendered video frames.
    private int framesRendered;
    // Time in ns when the first video frame was rendered.
    private long startTimeNs = -1;
    // Time in ns spent in draw() function.
    private long drawTimeNs;
    // Time in ns spent in renderFrame() function - including copying frame
    // data to rendering planes.
    private long copyTimeNs;
    // Texture vertices.
    private float texLeft;
    private float texRight;
    private float texTop;
    private float texBottom;
    private FloatBuffer textureVertices;
    // Texture UV coordinates offsets.
    private float texOffsetU;
    private float texOffsetV;
    private FloatBuffer textureCoords;
    // Flag if texture vertices or coordinates update is needed.
    private boolean updateTextureProperties;
    // Viewport dimensions.
    private int screenWidth;
    private int screenHeight;
    // Video dimension.
    private int videoWidth;
    private int videoHeight;

    private YuvImageRenderer(
        GLSurfaceView surface, int id,
        int x, int y, int width, int height,
        ScalingType scalingType) {
      Log.d(TAG, "YuvImageRenderer.Create id: " + id);
      this.surface = surface;
      this.id = id;
      this.scalingType = scalingType;
      frameToRenderQueue = new LinkedBlockingQueue<I420Frame>(1);
      // Create texture vertices.
      texLeft = (x - 50) / 50.0f;
      texTop = (50 - y) / 50.0f;
      texRight = Math.min(1.0f, (x + width - 50) / 50.0f);
      texBottom = Math.max(-1.0f, (50 - y - height) / 50.0f);
      float textureVeticesFloat[] = new float[] {
          texLeft, texTop,
          texLeft, texBottom,
          texRight, texTop,
          texRight, texBottom
      };
      textureVertices = directNativeFloatBuffer(textureVeticesFloat);
      // Create texture UV coordinates.
      texOffsetU = 0;
      texOffsetV = 0;
      float textureCoordinatesFloat[] = new float[] {
          texOffsetU, texOffsetV,               // left top
          texOffsetU, 1.0f - texOffsetV,        // left bottom
          1.0f - texOffsetU, texOffsetV,        // right top
          1.0f - texOffsetU, 1.0f - texOffsetV  // right bottom
      };
      textureCoords = directNativeFloatBuffer(textureCoordinatesFloat);
      updateTextureProperties = false;
    }

    private void createTextures(int yuvProgram, int oesProgram) {
      Log.d(TAG, "  YuvImageRenderer.createTextures " + id + " on GL thread:" +
          Thread.currentThread().getId());
      this.yuvProgram = yuvProgram;
      this.oesProgram = oesProgram;

      // Generate 3 texture ids for Y/U/V and place them into |yuvTextures|.
      GLES20.glGenTextures(3, yuvTextures, 0);
      for (int i = 0; i < 3; i++)  {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);
        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE,
            128, 128, 0, GLES20.GL_LUMINANCE, GLES20.GL_UNSIGNED_BYTE, null);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
            GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
            GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
            GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
            GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
      }
      checkNoGLES2Error();
    }

    private void checkAdjustTextureCoords() {
      if (!updateTextureProperties ||
          scalingType == ScalingType.SCALE_FILL) {
        return;
      }
      // Re - calculate texture vertices to preserve video aspect ratio.
      float texRight = this.texRight;
      float texLeft = this.texLeft;
      float texTop = this.texTop;
      float texBottom = this.texBottom;
      float displayWidth = (texRight - texLeft) * screenWidth / 2;
      float displayHeight = (texTop - texBottom) * screenHeight / 2;
      if (displayWidth > 1 && displayHeight > 1 &&
          videoWidth > 1 && videoHeight > 1) {
        float displayAspectRatio = displayWidth / displayHeight;
        float videoAspectRatio = (float)videoWidth / videoHeight;
        if (scalingType == ScalingType.SCALE_ASPECT_FIT) {
          // Need to re-adjust vertices width or height to match video AR.
          if (displayAspectRatio > videoAspectRatio) {
            float deltaX = (displayWidth - videoAspectRatio * displayHeight) /
                    instance.screenWidth;
            texRight -= deltaX;
            texLeft += deltaX;
          } else {
            float deltaY = (displayHeight - displayWidth / videoAspectRatio) /
                    instance.screenHeight;
            texTop -= deltaY;
            texBottom += deltaY;
          }
          // Re-allocate vertices buffer to adjust to video aspect ratio.
          float textureVeticesFloat[] = new float[] {
              texLeft, texTop,
              texLeft, texBottom,
              texRight, texTop,
              texRight, texBottom
          };
          textureVertices = directNativeFloatBuffer(textureVeticesFloat);
        }
        if (scalingType == ScalingType.SCALE_ASPECT_FILL) {
          // Need to re-adjust UV coordinates to match display AR.
          if (displayAspectRatio > videoAspectRatio) {
            texOffsetV = (1.0f - videoAspectRatio / displayAspectRatio) / 2.0f;
          } else {
            texOffsetU = (1.0f - displayAspectRatio / videoAspectRatio) / 2.0f;
          }
          // Re-allocate coordinates buffer to adjust to display aspect ratio.
          float textureCoordinatesFloat[] = new float[] {
              texOffsetU, texOffsetV,               // left top
              texOffsetU, 1.0f - texOffsetV,        // left bottom
              1.0f - texOffsetU, texOffsetV,        // right top
              1.0f - texOffsetU, 1.0f - texOffsetV  // right bottom
          };
          textureCoords = directNativeFloatBuffer(textureCoordinatesFloat);
        }
      }
      updateTextureProperties = false;
    }

    private void draw() {
      if (!seenFrame) {
        // No frame received yet - nothing to render.
        return;
      }
      // Check if texture vertices/coordinates adjustment is required when
      // screen orientation changes or video frame size changes.
      checkAdjustTextureCoords();

      long now = System.nanoTime();

      I420Frame frameFromQueue;
      synchronized (frameToRenderQueue) {
        frameFromQueue = frameToRenderQueue.peek();
        if (frameFromQueue != null && startTimeNs == -1) {
          startTimeNs = now;
        }

        if (rendererType == RendererType.RENDERER_YUV) {
          // YUV textures rendering.
          GLES20.glUseProgram(yuvProgram);

          for (int i = 0; i < 3; ++i) {
            GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);
            if (frameFromQueue != null) {
              int w = (i == 0) ?
                  frameFromQueue.width : frameFromQueue.width / 2;
              int h = (i == 0) ?
                  frameFromQueue.height : frameFromQueue.height / 2;
              GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE,
                  w, h, 0, GLES20.GL_LUMINANCE, GLES20.GL_UNSIGNED_BYTE,
                  frameFromQueue.yuvPlanes[i]);
            }
          }
        } else {
          // External texture rendering.
          GLES20.glUseProgram(oesProgram);

          if (frameFromQueue != null) {
            oesTexture = frameFromQueue.textureId;
            if (frameFromQueue.textureObject instanceof SurfaceTexture) {
              SurfaceTexture surfaceTexture =
                  (SurfaceTexture) frameFromQueue.textureObject;
              surfaceTexture.updateTexImage();
              surfaceTexture.getTransformMatrix(stMatrix);
            }
          }
          GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
          GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, oesTexture);
        }

        if (frameFromQueue != null) {
          frameToRenderQueue.poll();
        }
      }

      if (rendererType == RendererType.RENDERER_YUV) {
        GLES20.glUniform1i(GLES20.glGetUniformLocation(yuvProgram, "y_tex"), 0);
        GLES20.glUniform1i(GLES20.glGetUniformLocation(yuvProgram, "u_tex"), 1);
        GLES20.glUniform1i(GLES20.glGetUniformLocation(yuvProgram, "v_tex"), 2);
      }

      int posLocation = GLES20.glGetAttribLocation(yuvProgram, "in_pos");
      if (posLocation == -1) {
        throw new RuntimeException("Could not get attrib location for in_pos");
      }
      GLES20.glEnableVertexAttribArray(posLocation);
      GLES20.glVertexAttribPointer(
          posLocation, 2, GLES20.GL_FLOAT, false, 0, textureVertices);

      int texLocation = GLES20.glGetAttribLocation(yuvProgram, "in_tc");
      if (texLocation == -1) {
        throw new RuntimeException("Could not get attrib location for in_tc");
      }
      GLES20.glEnableVertexAttribArray(texLocation);
      GLES20.glVertexAttribPointer(
          texLocation, 2, GLES20.GL_FLOAT, false, 0, textureCoords);

      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

      GLES20.glDisableVertexAttribArray(posLocation);
      GLES20.glDisableVertexAttribArray(texLocation);

      checkNoGLES2Error();

      if (frameFromQueue != null) {
        framesRendered++;
        drawTimeNs += (System.nanoTime() - now);
        if ((framesRendered % 150) == 0) {
          logStatistics();
        }
      }
    }

    private void logStatistics() {
      long timeSinceFirstFrameNs = System.nanoTime() - startTimeNs;
      Log.d(TAG, "ID: " + id + ". Type: " + rendererType +
          ". Frames received: " + framesReceived +
          ". Dropped: " + framesDropped + ". Rendered: " + framesRendered);
      if (framesReceived > 0 && framesRendered > 0) {
        Log.d(TAG, "Duration: " + (int)(timeSinceFirstFrameNs / 1e6) +
            " ms. FPS: " + (float)framesRendered * 1e9 / timeSinceFirstFrameNs);
        Log.d(TAG, "Draw time: " +
            (int) (drawTimeNs / (1000 * framesRendered)) + " us. Copy time: " +
            (int) (copyTimeNs / (1000 * framesReceived)) + " us");
      }
    }

    public void setScreenSize(final int screenWidth, final int screenHeight) {
      this.screenWidth = screenWidth;
      this.screenHeight = screenHeight;
      updateTextureProperties = true;
    }

    @Override
    public void setSize(final int width, final int height) {
      Log.d(TAG, "ID: " + id + ". YuvImageRenderer.setSize: " +
          width + " x " + height);
      videoWidth = width;
      videoHeight = height;
      int[] strides = { width, width / 2, width / 2  };
      // Frame re-allocation need to be synchronized with copying
      // frame to textures in draw() function to avoid re-allocating
      // the frame while it is being copied.
      synchronized (frameToRenderQueue) {
        // Clear rendering queue.
        frameToRenderQueue.poll();
        // Re-allocate / allocate the frame.
        yuvFrameToRender = new I420Frame(width, height, strides, null);
        textureFrameToRender = new I420Frame(width, height, null, -1);
        updateTextureProperties = true;
      }
    }

    @Override
    public synchronized void renderFrame(I420Frame frame) {
      long now = System.nanoTime();
      framesReceived++;
      // Skip rendering of this frame if setSize() was not called.
      if (yuvFrameToRender == null || textureFrameToRender == null) {
        framesDropped++;
        return;
      }
      // Check input frame parameters.
      if (frame.yuvFrame) {
        if (!(frame.yuvStrides[0] == frame.width &&
            frame.yuvStrides[1] == frame.width / 2 &&
            frame.yuvStrides[2] == frame.width / 2)) {
          Log.e(TAG, "Incorrect strides " + frame.yuvStrides[0] + ", " +
              frame.yuvStrides[1] + ", " + frame.yuvStrides[2]);
          return;
        }
        // Check incoming frame dimensions.
        if (frame.width != yuvFrameToRender.width ||
            frame.height != yuvFrameToRender.height) {
          throw new RuntimeException("Wrong frame size " +
              frame.width + " x " + frame.height);
        }
      }

      if (frameToRenderQueue.size() > 0) {
        // Skip rendering of this frame if previous frame was not rendered yet.
        framesDropped++;
        return;
      }

      // Create a local copy of the frame.
      if (frame.yuvFrame) {
        yuvFrameToRender.copyFrom(frame);
        rendererType = RendererType.RENDERER_YUV;
        frameToRenderQueue.offer(yuvFrameToRender);
      } else {
        textureFrameToRender.copyFrom(frame);
        rendererType = RendererType.RENDERER_TEXTURE;
        frameToRenderQueue.offer(textureFrameToRender);
      }
      copyTimeNs += (System.nanoTime() - now);
      seenFrame = true;

      // Request rendering.
      surface.requestRender();
    }

  }

  /** Passes GLSurfaceView to video renderer. */
  public static void setView(GLSurfaceView surface) {
    Log.d(TAG, "VideoRendererGui.setView");
    instance = new VideoRendererGui(surface);
  }

  public static EGLContext getEGLContext() {
    return eglContext;
  }

  /**
   * Creates VideoRenderer with top left corner at (x, y) and resolution
   * (width, height). All parameters are in percentage of screen resolution.
   */
  public static VideoRenderer createGui(
      int x, int y, int width, int height) throws Exception {
    YuvImageRenderer javaGuiRenderer = create(x, y, width, height);
    return new VideoRenderer(javaGuiRenderer);
  }

  public static VideoRenderer.Callbacks createGuiRenderer(
      int x, int y, int width, int height) {
    return create(x, y, width, height);
  }

  /**
   * Creates VideoRenderer.Callbacks with top left corner at (x, y) and
   * resolution (width, height). All parameters are in percentage of
   * screen resolution.
   */
  public static YuvImageRenderer create(
      int x, int y, int width, int height) {
    // Check display region parameters.
    if (x < 0 || x > 100 || y < 0 || y > 100 ||
        width < 0 || width > 100 || height < 0 || height > 100 ||
        x + width > 100 || y + height > 100) {
      throw new RuntimeException("Incorrect window parameters.");
    }

    if (instance == null) {
      throw new RuntimeException(
          "Attempt to create yuv renderer before setting GLSurfaceView");
    }
    final YuvImageRenderer yuvImageRenderer = new YuvImageRenderer(
        instance.surface, instance.yuvImageRenderers.size(),
        x, y, width, height, ScalingType.SCALE_ASPECT_FIT);
    synchronized (instance.yuvImageRenderers) {
      if (instance.onSurfaceCreatedCalled) {
        // onSurfaceCreated has already been called for VideoRendererGui -
        // need to create texture for new image and add image to the
        // rendering list.
        final CountDownLatch countDownLatch = new CountDownLatch(1);
        instance.surface.queueEvent(new Runnable() {
          public void run() {
            yuvImageRenderer.createTextures(
                instance.yuvProgram, instance.oesProgram);
            yuvImageRenderer.setScreenSize(
                instance.screenWidth, instance.screenHeight);
            countDownLatch.countDown();
          }
        });
        // Wait for task completion.
        try {
          countDownLatch.await();
        } catch (InterruptedException e) {
          throw new RuntimeException(e);
        }
      }
      // Add yuv renderer to rendering list.
      instance.yuvImageRenderers.add(yuvImageRenderer);
    }
    return yuvImageRenderer;
  }

  @Override
  public void onSurfaceCreated(GL10 unused, EGLConfig config) {
    Log.d(TAG, "VideoRendererGui.onSurfaceCreated");
    // Store render EGL context
    eglContext = EGL14.eglGetCurrentContext();
    Log.d(TAG, "VideoRendererGui EGL Context: " + eglContext);

    // Create YUV and OES programs.
    yuvProgram = createProgram(VERTEX_SHADER_STRING,
        YUV_FRAGMENT_SHADER_STRING);
    oesProgram = createProgram(VERTEX_SHADER_STRING,
        OES_FRAGMENT_SHADER_STRING);

    synchronized (yuvImageRenderers) {
      // Create textures for all images.
      for (YuvImageRenderer yuvImageRenderer : yuvImageRenderers) {
        yuvImageRenderer.createTextures(yuvProgram, oesProgram);
      }
      onSurfaceCreatedCalled = true;
    }
    checkNoGLES2Error();
    GLES20.glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
  }

  @Override
  public void onSurfaceChanged(GL10 unused, int width, int height) {
    Log.d(TAG, "VideoRendererGui.onSurfaceChanged: " +
        width + " x " + height + "  ");
    screenWidth = width;
    screenHeight = height;
    GLES20.glViewport(0, 0, width, height);
    synchronized (yuvImageRenderers) {
      for (YuvImageRenderer yuvImageRenderer : yuvImageRenderers) {
        yuvImageRenderer.setScreenSize(screenWidth, screenHeight);
      }
    }
  }

  @Override
  public void onDrawFrame(GL10 unused) {
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    synchronized (yuvImageRenderers) {
      for (YuvImageRenderer yuvImageRenderer : yuvImageRenderers) {
        yuvImageRenderer.draw();
      }
    }
  }

}
