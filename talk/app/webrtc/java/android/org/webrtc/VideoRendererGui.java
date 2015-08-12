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

import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.annotation.SuppressLint;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.opengl.EGL14;
import android.opengl.EGLContext;
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
  private static Runnable eglContextReady = null;
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
  private GlRectDrawer drawer;
  private static final int EGL14_SDK_VERSION =
      android.os.Build.VERSION_CODES.JELLY_BEAN_MR1;
  // Current SDK version.
  private static final int CURRENT_SDK_VERSION =
      android.os.Build.VERSION.SDK_INT;

  private VideoRendererGui(GLSurfaceView surface) {
    this.surface = surface;
    // Create an OpenGL ES 2.0 context.
    surface.setPreserveEGLContextOnPause(true);
    surface.setEGLContextClientVersion(2);
    surface.setRenderer(this);
    surface.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);

    yuvImageRenderers = new ArrayList<YuvImageRenderer>();
  }

  /**
   * Class used to display stream of YUV420 frames at particular location
   * on a screen. New video frames are sent to display using renderFrame()
   * call.
   */
  private static class YuvImageRenderer implements VideoRenderer.Callbacks {
    private GLSurfaceView surface;
    private int id;
    private int[] yuvTextures = { -1, -1, -1 };
    private int oesTexture = -1;

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
    private RendererCommon.ScalingType scalingType;
    private boolean mirror;
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
    // The allowed view area in percentage of screen size.
    private final Rect layoutInPercentage;
    // The actual view area in pixels. It is a centered subrectangle of the rectangle defined by
    // |layoutInPercentage|.
    private final Rect displayLayout = new Rect();
    // Cached texture transformation matrix, calculated from current layout parameters.
    private final float[] texMatrix = new float[16];
    // Flag if texture vertices or coordinates update is needed.
    private boolean updateTextureProperties;
    // Texture properties update lock.
    private final Object updateTextureLock = new Object();
    // Viewport dimensions.
    private int screenWidth;
    private int screenHeight;
    // Video dimension.
    private int videoWidth;
    private int videoHeight;

    // This is the degree that the frame should be rotated clockwisely to have
    // it rendered up right.
    private int rotationDegree;

    private YuvImageRenderer(
        GLSurfaceView surface, int id,
        int x, int y, int width, int height,
        RendererCommon.ScalingType scalingType, boolean mirror) {
      Log.d(TAG, "YuvImageRenderer.Create id: " + id);
      this.surface = surface;
      this.id = id;
      this.scalingType = scalingType;
      this.mirror = mirror;
      frameToRenderQueue = new LinkedBlockingQueue<I420Frame>(1);
      layoutInPercentage = new Rect(x, y, Math.min(100, x + width), Math.min(100, y + height));
      updateTextureProperties = false;
      rotationDegree = 0;
    }

    private void createTextures() {
      Log.d(TAG, "  YuvImageRenderer.createTextures " + id + " on GL thread:" +
          Thread.currentThread().getId());

      // Generate 3 texture ids for Y/U/V and place them into |yuvTextures|.
      GLES20.glGenTextures(3, yuvTextures, 0);
      for (int i = 0; i < 3; i++)  {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
            GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
            GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
            GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
            GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
      }
      GlUtil.checkNoGLES2Error("y/u/v glGenTextures");
    }

    private void checkAdjustTextureCoords() {
      synchronized(updateTextureLock) {
        if (!updateTextureProperties) {
          return;
        }
        // Initialize to maximum allowed area. Round to integer coordinates inwards the layout
        // bounding box (ceil left/top and floor right/bottom) to not break constraints.
        displayLayout.set(
            (screenWidth * layoutInPercentage.left + 99) / 100,
            (screenHeight * layoutInPercentage.top + 99) / 100,
            (screenWidth * layoutInPercentage.right) / 100,
            (screenHeight * layoutInPercentage.bottom) / 100);
        Log.d(TAG, "ID: "  + id + ". AdjustTextureCoords. Allowed display size: "
            + displayLayout.width() + " x " + displayLayout.height() + ". Video: " + videoWidth
            + " x " + videoHeight + ". Rotation: " + rotationDegree + ". Mirror: " + mirror);
        final float videoAspectRatio = (rotationDegree % 180 == 0)
            ? (float) videoWidth / videoHeight
            : (float) videoHeight / videoWidth;
        // Adjust display size based on |scalingType|.
        final Point displaySize = RendererCommon.getDisplaySize(scalingType,
            videoAspectRatio, displayLayout.width(), displayLayout.height());
        displayLayout.inset((displayLayout.width() - displaySize.x) / 2,
                            (displayLayout.height() - displaySize.y) / 2);
        Log.d(TAG, "  Adjusted display size: " + displayLayout.width() + " x "
            + displayLayout.height());
        RendererCommon.getTextureMatrix(texMatrix, rotationDegree, mirror, videoAspectRatio,
            (float) displayLayout.width() / displayLayout.height());
        updateTextureProperties = false;
        Log.d(TAG, "  AdjustTextureCoords done");
      }
    }

    private void draw(GlRectDrawer drawer) {
      if (!seenFrame) {
        // No frame received yet - nothing to render.
        return;
      }
      long now = System.nanoTime();

      // OpenGL defaults to lower left origin.
      GLES20.glViewport(displayLayout.left, screenHeight - displayLayout.bottom,
                        displayLayout.width(), displayLayout.height());

      I420Frame frameFromQueue;
      synchronized (frameToRenderQueue) {
        // Check if texture vertices/coordinates adjustment is required when
        // screen orientation changes or video frame size changes.
        checkAdjustTextureCoords();

        frameFromQueue = frameToRenderQueue.peek();
        if (frameFromQueue != null && startTimeNs == -1) {
          startTimeNs = now;
        }

        if (frameFromQueue != null) {
          if (frameFromQueue.yuvFrame) {
            // YUV textures rendering. Upload YUV data as textures.
            for (int i = 0; i < 3; ++i) {
              GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
              GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);
              int w = (i == 0) ? frameFromQueue.width : frameFromQueue.width / 2;
              int h = (i == 0) ? frameFromQueue.height : frameFromQueue.height / 2;
              GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE,
                  w, h, 0, GLES20.GL_LUMINANCE, GLES20.GL_UNSIGNED_BYTE,
                  frameFromQueue.yuvPlanes[i]);
            }
          } else {
            // External texture rendering. Copy texture id and update texture image to latest.
            // TODO(magjed): We should not make an unmanaged copy of texture id. Also, this is not
            // the best place to call updateTexImage.
            oesTexture = frameFromQueue.textureId;
            if (frameFromQueue.textureObject instanceof SurfaceTexture) {
              SurfaceTexture surfaceTexture =
                  (SurfaceTexture) frameFromQueue.textureObject;
              surfaceTexture.updateTexImage();
            }
          }

          frameToRenderQueue.poll();
        }
      }

      if (rendererType == RendererType.RENDERER_YUV) {
        drawer.drawYuv(videoWidth, videoHeight, yuvTextures, texMatrix);
      } else {
        drawer.drawOes(oesTexture, texMatrix);
      }

      if (frameFromQueue != null) {
        framesRendered++;
        drawTimeNs += (System.nanoTime() - now);
        if ((framesRendered % 300) == 0) {
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
      synchronized(updateTextureLock) {
        if (screenWidth == this.screenWidth && screenHeight == this.screenHeight) {
          return;
        }
        Log.d(TAG, "ID: " + id + ". YuvImageRenderer.setScreenSize: " +
            screenWidth + " x " + screenHeight);
        this.screenWidth = screenWidth;
        this.screenHeight = screenHeight;
        updateTextureProperties = true;
      }
    }

    public void setPosition(int x, int y, int width, int height,
        RendererCommon.ScalingType scalingType, boolean mirror) {
      final Rect layoutInPercentage =
          new Rect(x, y, Math.min(100, x + width), Math.min(100, y + height));
      synchronized(updateTextureLock) {
        if (layoutInPercentage.equals(this.layoutInPercentage) && scalingType == this.scalingType
            && mirror == this.mirror) {
          return;
        }
        Log.d(TAG, "ID: " + id + ". YuvImageRenderer.setPosition: (" + x + ", " + y +
            ") " +  width + " x " + height + ". Scaling: " + scalingType +
            ". Mirror: " + mirror);
        this.layoutInPercentage.set(layoutInPercentage);
        this.scalingType = scalingType;
        this.mirror = mirror;
        updateTextureProperties = true;
      }
    }

    private void setSize(final int videoWidth, final int videoHeight, final int rotation) {
      if (videoWidth == this.videoWidth && videoHeight == this.videoHeight
          && rotation == rotationDegree) {
        return;
      }

      // Frame re-allocation need to be synchronized with copying
      // frame to textures in draw() function to avoid re-allocating
      // the frame while it is being copied.
      synchronized (frameToRenderQueue) {
        Log.d(TAG, "ID: " + id + ". YuvImageRenderer.setSize: " +
            videoWidth + " x " + videoHeight + " rotation " + rotation);

        this.videoWidth = videoWidth;
        this.videoHeight = videoHeight;
        rotationDegree = rotation;
        int[] strides = { videoWidth, videoWidth / 2, videoWidth / 2  };

        // Clear rendering queue.
        frameToRenderQueue.poll();
        // Re-allocate / allocate the frame.
        yuvFrameToRender = new I420Frame(videoWidth, videoHeight, rotationDegree,
                                         strides, null, 0);
        textureFrameToRender = new I420Frame(videoWidth, videoHeight, rotationDegree,
                                             null, -1, 0);
        updateTextureProperties = true;
        Log.d(TAG, "  YuvImageRenderer.setSize done.");
      }
    }

    @Override
    public synchronized void renderFrame(I420Frame frame) {
      setSize(frame.width, frame.height, frame.rotationDegree);
      long now = System.nanoTime();
      framesReceived++;
      // Skip rendering of this frame if setSize() was not called.
      if (yuvFrameToRender == null || textureFrameToRender == null) {
        framesDropped++;
        VideoRenderer.renderFrameDone(frame);
        return;
      }
      // Check input frame parameters.
      if (frame.yuvFrame) {
        if (frame.yuvStrides[0] < frame.width ||
            frame.yuvStrides[1] < frame.width / 2 ||
            frame.yuvStrides[2] < frame.width / 2) {
          Log.e(TAG, "Incorrect strides " + frame.yuvStrides[0] + ", " +
              frame.yuvStrides[1] + ", " + frame.yuvStrides[2]);
          VideoRenderer.renderFrameDone(frame);
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
        VideoRenderer.renderFrameDone(frame);
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
      VideoRenderer.renderFrameDone(frame);

      // Request rendering.
      surface.requestRender();
    }

    // TODO(guoweis): Remove this once chrome code base is updated.
    @Override
    public boolean canApplyRotation() {
      return true;
    }
  }

  /** Passes GLSurfaceView to video renderer. */
  public static void setView(GLSurfaceView surface,
      Runnable eglContextReadyCallback) {
    Log.d(TAG, "VideoRendererGui.setView");
    instance = new VideoRendererGui(surface);
    eglContextReady = eglContextReadyCallback;
  }

  public static EGLContext getEGLContext() {
    return eglContext;
  }

  /**
   * Creates VideoRenderer with top left corner at (x, y) and resolution
   * (width, height). All parameters are in percentage of screen resolution.
   */
  public static VideoRenderer createGui(int x, int y, int width, int height,
      RendererCommon.ScalingType scalingType, boolean mirror) throws Exception {
    YuvImageRenderer javaGuiRenderer = create(
        x, y, width, height, scalingType, mirror);
    return new VideoRenderer(javaGuiRenderer);
  }

  public static VideoRenderer.Callbacks createGuiRenderer(
      int x, int y, int width, int height,
      RendererCommon.ScalingType scalingType, boolean mirror) {
    return create(x, y, width, height, scalingType, mirror);
  }

  /**
   * Creates VideoRenderer.Callbacks with top left corner at (x, y) and
   * resolution (width, height). All parameters are in percentage of
   * screen resolution.
   */
  public static YuvImageRenderer create(int x, int y, int width, int height,
      RendererCommon.ScalingType scalingType, boolean mirror) {
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
        x, y, width, height, scalingType, mirror);
    synchronized (instance.yuvImageRenderers) {
      if (instance.onSurfaceCreatedCalled) {
        // onSurfaceCreated has already been called for VideoRendererGui -
        // need to create texture for new image and add image to the
        // rendering list.
        final CountDownLatch countDownLatch = new CountDownLatch(1);
        instance.surface.queueEvent(new Runnable() {
          public void run() {
            yuvImageRenderer.createTextures();
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

  public static void update(
      VideoRenderer.Callbacks renderer,
      int x, int y, int width, int height, RendererCommon.ScalingType scalingType, boolean mirror) {
    Log.d(TAG, "VideoRendererGui.update");
    if (instance == null) {
      throw new RuntimeException(
          "Attempt to update yuv renderer before setting GLSurfaceView");
    }
    synchronized (instance.yuvImageRenderers) {
      for (YuvImageRenderer yuvImageRenderer : instance.yuvImageRenderers) {
        if (yuvImageRenderer == renderer) {
          yuvImageRenderer.setPosition(x, y, width, height, scalingType, mirror);
        }
      }
    }
  }

  public static void remove(VideoRenderer.Callbacks renderer) {
    Log.d(TAG, "VideoRendererGui.remove");
    if (instance == null) {
      throw new RuntimeException(
          "Attempt to remove yuv renderer before setting GLSurfaceView");
    }
    synchronized (instance.yuvImageRenderers) {
      if (!instance.yuvImageRenderers.remove(renderer)) {
        Log.w(TAG, "Couldn't remove renderer (not present in current list)");
      }
    }
  }

  @SuppressLint("NewApi")
  @Override
  public void onSurfaceCreated(GL10 unused, EGLConfig config) {
    Log.d(TAG, "VideoRendererGui.onSurfaceCreated");
    // Store render EGL context.
    if (CURRENT_SDK_VERSION >= EGL14_SDK_VERSION) {
      eglContext = EGL14.eglGetCurrentContext();
      Log.d(TAG, "VideoRendererGui EGL Context: " + eglContext);
    }

    // Create drawer for YUV/OES frames.
    drawer = new GlRectDrawer();

    synchronized (yuvImageRenderers) {
      // Create textures for all images.
      for (YuvImageRenderer yuvImageRenderer : yuvImageRenderers) {
        yuvImageRenderer.createTextures();
      }
      onSurfaceCreatedCalled = true;
    }
    GlUtil.checkNoGLES2Error("onSurfaceCreated done");
    GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
    GLES20.glClearColor(0.15f, 0.15f, 0.15f, 1.0f);

    // Fire EGL context ready event.
    if (eglContextReady != null) {
      eglContextReady.run();
    }
  }

  @Override
  public void onSurfaceChanged(GL10 unused, int width, int height) {
    Log.d(TAG, "VideoRendererGui.onSurfaceChanged: " +
        width + " x " + height + "  ");
    screenWidth = width;
    screenHeight = height;
    synchronized (yuvImageRenderers) {
      for (YuvImageRenderer yuvImageRenderer : yuvImageRenderers) {
        yuvImageRenderer.setScreenSize(screenWidth, screenHeight);
      }
    }
  }

  @Override
  public void onDrawFrame(GL10 unused) {
    GLES20.glViewport(0, 0, screenWidth, screenHeight);
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    synchronized (yuvImageRenderers) {
      for (YuvImageRenderer yuvImageRenderer : yuvImageRenderers) {
        yuvImageRenderer.draw(drawer);
      }
    }
  }

}
