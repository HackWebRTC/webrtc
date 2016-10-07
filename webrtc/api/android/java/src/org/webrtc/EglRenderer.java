/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.opengl.GLES20;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.view.Surface;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Implements org.webrtc.VideoRenderer.Callbacks by displaying the video stream on an EGL Surface.
 * This class is intended to be used as a helper class for rendering on SurfaceViews and
 * TextureViews.
 */
public class EglRenderer implements VideoRenderer.Callbacks {
  private static final String TAG = "EglRenderer";
  private static final int MAX_SURFACE_CLEAR_COUNT = 3;

  private class EglSurfaceCreation implements Runnable {
    private Surface surface;

    public synchronized void setSurface(Surface surface) {
      this.surface = surface;
    }

    @Override
    public synchronized void run() {
      if (surface != null && eglBase != null && !eglBase.hasSurface()) {
        eglBase.createSurface((Surface) surface);
        eglBase.makeCurrent();
        // Necessary for YUV frames with odd width.
        GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
      }
    }
  }

  private final String name;

  // |renderThreadHandler| is a handler for communicating with |renderThread|, and is synchronized
  // on |handlerLock|.
  private final Object handlerLock = new Object();
  private Handler renderThreadHandler;

  // EGL and GL resources for drawing YUV/OES textures. After initilization, these are only accessed
  // from the render thread.
  private EglBase eglBase;
  private final RendererCommon.YuvUploader yuvUploader = new RendererCommon.YuvUploader();
  private RendererCommon.GlDrawer drawer;
  // Texture ids for YUV frames. Allocated on first arrival of a YUV frame.
  private int[] yuvTextures = null;

  // Pending frame to render. Serves as a queue with size 1. Synchronized on |frameLock|.
  private final Object frameLock = new Object();
  private VideoRenderer.I420Frame pendingFrame;

  // These variables are synchronized on |layoutLock|.
  private final Object layoutLock = new Object();
  private int surfaceWidth;
  private int surfaceHeight;
  private float layoutAspectRatio;
  // If true, mirrors the video stream horizontally.
  private boolean mirror;

  // These variables are synchronized on |statisticsLock|.
  private final Object statisticsLock = new Object();
  // Total number of video frames received in renderFrame() call.
  private int framesReceived;
  // Number of video frames dropped by renderFrame() because previous frame has not been rendered
  // yet.
  private int framesDropped;
  // Number of rendered video frames.
  private int framesRendered;
  // Time in ns when the first video frame was rendered.
  private long firstFrameTimeNs;
  // Time in ns spent in renderFrameOnRenderThread() function.
  private long renderTimeNs;

  // Runnable for posting frames to render thread.
  private final Runnable renderFrameRunnable = new Runnable() {
    @Override
    public void run() {
      renderFrameOnRenderThread();
    }
  };

  private final EglSurfaceCreation eglSurfaceCreationRunnable = new EglSurfaceCreation();

  /**
   * Standard constructor. The name will be used for the render thread name and included when
   * logging. In order to render something, you must first call init() and createEglSurface.
   */
  public EglRenderer(String name) {
    this.name = name;
  }

  /**
   * Initialize this class, sharing resources with |sharedContext|. The custom |drawer| will be used
   * for drawing frames on the EGLSurface. This class is responsible for calling release() on
   * |drawer|. It is allowed to call init() to reinitialize the renderer after a previous
   * init()/release() cycle.
   */
  public void init(final EglBase.Context sharedContext, final int[] configAttributes,
      RendererCommon.GlDrawer drawer) {
    synchronized (handlerLock) {
      if (renderThreadHandler != null) {
        throw new IllegalStateException(name + "Already initialized");
      }
      logD("Initializing EglRenderer");
      this.drawer = drawer;

      final HandlerThread renderThread = new HandlerThread(name + "EglRenderer");
      renderThread.start();
      renderThreadHandler = new Handler(renderThread.getLooper());
      // Create EGL context on the newly created render thread. It should be possibly to create the
      // context on this thread and make it current on the render thread, but this causes failure on
      // some Marvel based JB devices. https://bugs.chromium.org/p/webrtc/issues/detail?id=6350.
      ThreadUtils.invokeAtFrontUninterruptibly(renderThreadHandler, new Runnable() {
        @Override
        public void run() {
          eglBase = EglBase.create(sharedContext, configAttributes);
        }
      });
    }
  }

  public void createEglSurface(Surface surface) {
    eglSurfaceCreationRunnable.setSurface(surface);
    runOnRenderThread(eglSurfaceCreationRunnable);
  }

  /**
   * Block until any pending frame is returned and all GL resources released, even if an interrupt
   * occurs. If an interrupt occurs during release(), the interrupt flag will be set. This function
   * should be called before the Activity is destroyed and the EGLContext is still valid. If you
   * don't call this function, the GL resources might leak.
   */
  public void release() {
    final CountDownLatch eglCleanupBarrier = new CountDownLatch(1);
    synchronized (handlerLock) {
      if (renderThreadHandler == null) {
        logD("Already released");
        return;
      }
      // Release EGL and GL resources on render thread.
      renderThreadHandler.postAtFrontOfQueue(new Runnable() {
        @Override
        public void run() {
          if (drawer != null) {
            drawer.release();
            drawer = null;
          }
          if (yuvTextures != null) {
            GLES20.glDeleteTextures(3, yuvTextures, 0);
            yuvTextures = null;
          }
          if (eglBase != null) {
            logD("eglBase detach and release.");
            eglBase.detachCurrent();
            eglBase.release();
            eglBase = null;
          }
          eglCleanupBarrier.countDown();
        }
      });
      final Looper renderLooper = renderThreadHandler.getLooper();
      // TODO(magjed): Replace this post() with renderLooper.quitSafely() when API support >= 18.
      renderThreadHandler.post(new Runnable() {
        @Override
        public void run() {
          logD("Quitting render thread.");
          renderLooper.quit();
        }
      });
      // Don't accept any more frames or messages to the render thread.
      renderThreadHandler = null;
    }
    // Make sure the EGL/GL cleanup posted above is executed.
    ThreadUtils.awaitUninterruptibly(eglCleanupBarrier);
    synchronized (frameLock) {
      if (pendingFrame != null) {
        VideoRenderer.renderFrameDone(pendingFrame);
        pendingFrame = null;
      }
    }
    resetStatistics();
    logD("Releasing done.");
  }

  /**
   * Reset statistics. This will reset the logged statistics in logStatistics(), and
   * RendererEvents.onFirstFrameRendered() will be called for the next frame.
   */
  public void resetStatistics() {
    synchronized (statisticsLock) {
      framesReceived = 0;
      framesDropped = 0;
      framesRendered = 0;
      firstFrameTimeNs = 0;
      renderTimeNs = 0;
    }
  }

  /**
   * Set if the video stream should be mirrored or not.
   */
  public void setMirror(final boolean mirror) {
    logD("setMirror: " + mirror);
    synchronized (layoutLock) {
      this.mirror = mirror;
    }
  }

  /**
   * Set layout aspect ratio. This is used to crop frames when rendering to avoid stretched video.
   * Set this to 0 to disable cropping.
   */
  public void setLayoutAspectRatio(float layoutAspectRatio) {
    logD("setLayoutAspectRatio: " + layoutAspectRatio);
    synchronized (layoutLock) {
      this.layoutAspectRatio = layoutAspectRatio;
    }
  }

  // VideoRenderer.Callbacks interface.
  @Override
  public void renderFrame(VideoRenderer.I420Frame frame) {
    synchronized (statisticsLock) {
      ++framesReceived;
    }
    synchronized (handlerLock) {
      if (renderThreadHandler == null) {
        logD("Dropping frame - Not initialized or already released.");
        VideoRenderer.renderFrameDone(frame);
        return;
      }
      synchronized (frameLock) {
        if (pendingFrame != null) {
          // Drop old frame.
          synchronized (statisticsLock) {
            ++framesDropped;
          }
          VideoRenderer.renderFrameDone(pendingFrame);
        }
        pendingFrame = frame;
        renderThreadHandler.post(renderFrameRunnable);
      }
    }
  }

  /**
   * Release EGL surface. This function will block until the EGL surface is released.
   */
  public void releaseEglSurface() {
    // Ensure that the render thread is no longer touching the Surface before returning from this
    // function.
    eglSurfaceCreationRunnable.setSurface(null /* surface */);
    synchronized (handlerLock) {
      if (renderThreadHandler != null) {
        renderThreadHandler.removeCallbacks(eglSurfaceCreationRunnable);
        ThreadUtils.invokeAtFrontUninterruptibly(renderThreadHandler, new Runnable() {
          @Override
          public void run() {
            if (eglBase != null) {
              eglBase.detachCurrent();
              eglBase.releaseSurface();
            }
          }
        });
      }
    }
  }

  /**
   * Notify that the surface size has changed.
   */
  public void surfaceSizeChanged(int surfaceWidth, int surfaceHeight) {
    logD("Surface size changed: " + surfaceWidth + "x" + surfaceHeight);
    synchronized (layoutLock) {
      this.surfaceWidth = surfaceWidth;
      this.surfaceHeight = surfaceHeight;
    }
  }

  /**
   * Private helper function to post tasks safely.
   */
  private void runOnRenderThread(Runnable runnable) {
    synchronized (handlerLock) {
      if (renderThreadHandler != null) {
        renderThreadHandler.post(runnable);
      }
    }
  }

  private void makeBlack() {
    if (eglBase != null && eglBase.hasSurface()) {
      logD("clearSurface");
      GLES20.glClearColor(0 /* red */, 0 /* green */, 0 /* blue */, 0 /* alpha */);
      GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
      eglBase.swapBuffers();
    }
  }

  /**
   * Renders and releases |pendingFrame|.
   */
  private void renderFrameOnRenderThread() {
    // Fetch and render |pendingFrame|.
    final VideoRenderer.I420Frame frame;
    synchronized (frameLock) {
      if (pendingFrame == null) {
        return;
      }
      frame = pendingFrame;
      pendingFrame = null;
    }
    if (eglBase == null || !eglBase.hasSurface()) {
      logD("Dropping frame - No surface");
      VideoRenderer.renderFrameDone(frame);
      return;
    }

    final long startTimeNs = System.nanoTime();
    float[] texMatrix =
        RendererCommon.rotateTextureMatrix(frame.samplingMatrix, frame.rotationDegree);

    // After a surface size change, the EGLSurface might still have a buffer of the old size in the
    // pipeline. Querying the EGLSurface will show if the underlying buffer dimensions haven't yet
    // changed. Such a buffer will be rendered incorrectly, so flush it with a black frame.
    synchronized (layoutLock) {
      int surfaceClearCount = 0;
      while (eglBase.surfaceWidth() != surfaceWidth || eglBase.surfaceHeight() != surfaceHeight) {
        ++surfaceClearCount;
        if (surfaceClearCount > MAX_SURFACE_CLEAR_COUNT) {
          logD("Failed to get surface of expected size - dropping frame.");
          VideoRenderer.renderFrameDone(frame);
          return;
        }
        logD("Surface size mismatch - clearing surface.");
        makeBlack();
      }
      final float[] layoutMatrix;
      if (layoutAspectRatio > 0) {
        layoutMatrix = RendererCommon.getLayoutMatrix(
            mirror, frame.rotatedWidth() / (float) frame.rotatedHeight(), layoutAspectRatio);
      } else {
        layoutMatrix =
            mirror ? RendererCommon.horizontalFlipMatrix() : RendererCommon.identityMatrix();
      }
      texMatrix = RendererCommon.multiplyMatrices(texMatrix, layoutMatrix);
    }

    GLES20.glClearColor(0 /* red */, 0 /* green */, 0 /* blue */, 0 /* alpha */);
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    if (frame.yuvFrame) {
      // Make sure YUV textures are allocated.
      if (yuvTextures == null) {
        yuvTextures = new int[3];
        for (int i = 0; i < 3; i++) {
          yuvTextures[i] = GlUtil.generateTexture(GLES20.GL_TEXTURE_2D);
        }
      }
      yuvUploader.uploadYuvData(
          yuvTextures, frame.width, frame.height, frame.yuvStrides, frame.yuvPlanes);
      drawer.drawYuv(yuvTextures, texMatrix, frame.rotatedWidth(), frame.rotatedHeight(), 0, 0,
          surfaceWidth, surfaceHeight);
    } else {
      drawer.drawOes(frame.textureId, texMatrix, frame.rotatedWidth(), frame.rotatedHeight(), 0, 0,
          surfaceWidth, surfaceHeight);
    }

    eglBase.swapBuffers();
    VideoRenderer.renderFrameDone(frame);
    synchronized (statisticsLock) {
      if (framesRendered == 0) {
        firstFrameTimeNs = startTimeNs;
      }
      ++framesRendered;
      renderTimeNs += (System.nanoTime() - startTimeNs);
      if (framesRendered % 300 == 0) {
        logStatistics();
      }
    }
  }

  private void logStatistics() {
    synchronized (statisticsLock) {
      logD("Frames received: " + framesReceived + ". Dropped: " + framesDropped + ". Rendered: "
          + framesRendered);
      if (framesReceived > 0 && framesRendered > 0) {
        final long timeSinceFirstFrameNs = System.nanoTime() - firstFrameTimeNs;
        logD("Duration: " + (int) (timeSinceFirstFrameNs / 1e6) + " ms. FPS: "
            + framesRendered * 1e9 / timeSinceFirstFrameNs);
        logD("Average render time: " + (int) (renderTimeNs / (1000 * framesRendered)) + " us.");
      }
    }
  }

  private void logD(String string) {
    Logging.d(TAG, name + string);
  }
}
