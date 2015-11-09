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

import android.content.Context;
import android.graphics.Point;
import android.graphics.SurfaceTexture;
import android.opengl.GLES20;
import android.opengl.Matrix;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import org.webrtc.Logging;

import java.util.concurrent.CountDownLatch;

import javax.microedition.khronos.egl.EGLContext;

/**
 * Implements org.webrtc.VideoRenderer.Callbacks by displaying the video stream on a SurfaceView.
 * renderFrame() is asynchronous to avoid blocking the calling thread.
 * This class is thread safe and handles access from potentially four different threads:
 * Interaction from the main app in init, release, setMirror, and setScalingtype.
 * Interaction from C++ webrtc::VideoRendererInterface in renderFrame and canApplyRotation.
 * Interaction from the Activity lifecycle in surfaceCreated, surfaceChanged, and surfaceDestroyed.
 * Interaction with the layout framework in onMeasure and onSizeChanged.
 */
public class SurfaceViewRenderer extends SurfaceView
    implements SurfaceHolder.Callback, VideoRenderer.Callbacks {
  private static final String TAG = "SurfaceViewRenderer";

  // Dedicated render thread.
  private HandlerThread renderThread;
  // |renderThreadHandler| is a handler for communicating with |renderThread|, and is synchronized
  // on |handlerLock|.
  private final Object handlerLock = new Object();
  private Handler renderThreadHandler;

  // EGL and GL resources for drawing YUV/OES textures. After initilization, these are only accessed
  // from the render thread.
  private EglBase eglBase;
  private GlRectDrawer drawer;
  // Texture ids for YUV frames. Allocated on first arrival of a YUV frame.
  private int[] yuvTextures = null;

  // Pending frame to render. Serves as a queue with size 1. Synchronized on |frameLock|.
  private final Object frameLock = new Object();
  private VideoRenderer.I420Frame pendingFrame;

  // These variables are synchronized on |layoutLock|.
  private final Object layoutLock = new Object();
  // These three different dimension values are used to keep track of the state in these functions:
  // requestLayout() -> onMeasure() -> onLayout() -> surfaceChanged().
  // requestLayout() is triggered internally by frame size changes, but can also be triggered
  // externally by layout update requests.
  // Most recent measurement specification from onMeasure().
  private int widthSpec;
  private int heightSpec;
  // Current size on screen in pixels. Updated in onLayout(), and should be consistent with
  // |widthSpec|/|heightSpec| after that.
  private int layoutWidth;
  private int layoutHeight;
  // Current surface size of the underlying Surface. Updated in surfaceChanged(), and should be
  // consistent with |layoutWidth|/|layoutHeight| after that.
  // TODO(magjed): Enable hardware scaler with SurfaceHolder.setFixedSize(). This will decouple
  // layout and surface size.
  private int surfaceWidth;
  private int surfaceHeight;
  // |isSurfaceCreated| keeps track of the current status in surfaceCreated()/surfaceDestroyed().
  private boolean isSurfaceCreated;
  // Last rendered frame dimensions, or 0 if no frame has been rendered yet.
  private int frameWidth;
  private int frameHeight;
  private int frameRotation;
  // |scalingType| determines how the video will fill the allowed layout area in onMeasure().
  private RendererCommon.ScalingType scalingType = RendererCommon.ScalingType.SCALE_ASPECT_BALANCED;
  // If true, mirrors the video stream horizontally.
  private boolean mirror;
  // Callback for reporting renderer events.
  private RendererCommon.RendererEvents rendererEvents;

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

  // Runnable for posting frames to render thread..
  private final Runnable renderFrameRunnable = new Runnable() {
    @Override public void run() {
      renderFrameOnRenderThread();
    }
  };

  /**
   * Standard View constructor. In order to render something, you must first call init().
   */
  public SurfaceViewRenderer(Context context) {
    super(context);
    getHolder().addCallback(this);
  }

  /**
   * Standard View constructor. In order to render something, you must first call init().
   */
  public SurfaceViewRenderer(Context context, AttributeSet attrs) {
    super(context, attrs);
    getHolder().addCallback(this);
  }

  /**
   * Initialize this class, sharing resources with |sharedContext|. It is allowed to call init() to
   * reinitialize the renderer after a previous init()/release() cycle.
   */
  public void init(
      EGLContext sharedContext, RendererCommon.RendererEvents rendererEvents) {
    synchronized (handlerLock) {
      if (renderThreadHandler != null) {
        throw new IllegalStateException("Already initialized");
      }
      Logging.d(TAG, "Initializing");
      this.rendererEvents = rendererEvents;
      renderThread = new HandlerThread(TAG);
      renderThread.start();
      drawer = new GlRectDrawer();
      eglBase = new EglBase(sharedContext, EglBase.ConfigType.PLAIN);
      renderThreadHandler = new Handler(renderThread.getLooper());
    }
    tryCreateEglSurface();
  }

  /**
   * Create and make an EGLSurface current if both init() and surfaceCreated() have been called.
   */
  public void tryCreateEglSurface() {
    // |renderThreadHandler| is only created after |eglBase| is created in init(), so the
    // following code will only execute if eglBase != null.
    runOnRenderThread(new Runnable() {
      @Override public void run() {
        synchronized (layoutLock) {
          if (isSurfaceCreated) {
            eglBase.createSurface(getHolder());
            eglBase.makeCurrent();
            // Necessary for YUV frames with odd width.
            GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
          }
        }
      }
    });
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
        Logging.d(TAG, "Already released");
        return;
      }
      // Release EGL and GL resources on render thread.
      // TODO(magjed): This might not be necessary - all OpenGL resources are automatically deleted
      // when the EGL context is lost. It might be dangerous to delete them manually in
      // Activity.onDestroy().
      renderThreadHandler.postAtFrontOfQueue(new Runnable() {
        @Override public void run() {
          drawer.release();
          drawer = null;
          if (yuvTextures != null) {
            GLES20.glDeleteTextures(3, yuvTextures, 0);
            yuvTextures = null;
          }
          if (eglBase.hasSurface()) {
            // Clear last rendered image to black.
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
            eglBase.swapBuffers();
          }
          eglBase.release();
          eglBase = null;
          eglCleanupBarrier.countDown();
        }
      });
      // Don't accept any more frames or messages to the render thread.
      renderThreadHandler = null;
    }
    // Make sure the EGL/GL cleanup posted above is executed.
    ThreadUtils.awaitUninterruptibly(eglCleanupBarrier);
    renderThread.quit();
    synchronized (frameLock) {
      if (pendingFrame != null) {
        VideoRenderer.renderFrameDone(pendingFrame);
        pendingFrame = null;
      }
    }
    // The |renderThread| cleanup is not safe to cancel and we need to wait until it's done.
    ThreadUtils.joinUninterruptibly(renderThread);
    renderThread = null;
    // Reset statistics and event reporting.
    synchronized (layoutLock) {
      frameWidth = 0;
      frameHeight = 0;
      frameRotation = 0;
      rendererEvents = null;
    }
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
    synchronized (layoutLock) {
      this.mirror = mirror;
    }
  }

  /**
   * Set how the video will fill the allowed layout area.
   */
  public void setScalingType(RendererCommon.ScalingType scalingType) {
    synchronized (layoutLock) {
      this.scalingType = scalingType;
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
        Logging.d(TAG, "Dropping frame - SurfaceViewRenderer not initialized or already released.");
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
        updateFrameDimensionsAndReportEvents(frame);
        renderThreadHandler.post(renderFrameRunnable);
      }
    }
  }

  // Returns desired layout size given current measure specification and video aspect ratio.
  private Point getDesiredLayoutSize() {
    synchronized (layoutLock) {
      final int maxWidth = getDefaultSize(Integer.MAX_VALUE, widthSpec);
      final int maxHeight = getDefaultSize(Integer.MAX_VALUE, heightSpec);
      final Point size =
          RendererCommon.getDisplaySize(scalingType, frameAspectRatio(), maxWidth, maxHeight);
      if (MeasureSpec.getMode(widthSpec) == MeasureSpec.EXACTLY) {
        size.x = maxWidth;
      }
      if (MeasureSpec.getMode(heightSpec) == MeasureSpec.EXACTLY) {
        size.y = maxHeight;
      }
      return size;
    }
  }

  // View layout interface.
  @Override
  protected void onMeasure(int widthSpec, int heightSpec) {
    synchronized (layoutLock) {
      this.widthSpec = widthSpec;
      this.heightSpec = heightSpec;
      final Point size = getDesiredLayoutSize();
      setMeasuredDimension(size.x, size.y);
    }
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    synchronized (layoutLock) {
      layoutWidth = right - left;
      layoutHeight = bottom - top;
    }
    // Might have a pending frame waiting for a layout of correct size.
    runOnRenderThread(renderFrameRunnable);
  }

  // SurfaceHolder.Callback interface.
  @Override
  public void surfaceCreated(final SurfaceHolder holder) {
    Logging.d(TAG, "Surface created");
    synchronized (layoutLock) {
      isSurfaceCreated = true;
    }
    tryCreateEglSurface();
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder holder) {
    Logging.d(TAG, "Surface destroyed");
    synchronized (layoutLock) {
      isSurfaceCreated = false;
      surfaceWidth = 0;
      surfaceHeight = 0;
    }
    runOnRenderThread(new Runnable() {
      @Override public void run() {
        eglBase.releaseSurface();
      }
    });
  }

  @Override
  public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    Logging.d(TAG, "Surface changed: " + width + "x" + height);
    synchronized (layoutLock) {
      surfaceWidth = width;
      surfaceHeight = height;
    }
    // Might have a pending frame waiting for a surface of correct size.
    runOnRenderThread(renderFrameRunnable);
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

  /**
   * Requests new layout if necessary. Returns true if layout and surface size are consistent.
   */
  private boolean checkConsistentLayout() {
    synchronized (layoutLock) {
      final Point desiredLayoutSize = getDesiredLayoutSize();
      if (desiredLayoutSize.x != layoutWidth || desiredLayoutSize.y != layoutHeight) {
        Logging.d(TAG, "Requesting new layout with size: "
            + desiredLayoutSize.x + "x" + desiredLayoutSize.y);
        // Request layout update on UI thread.
        post(new Runnable() {
          @Override public void run() {
            requestLayout();
          }
        });
        return false;
      }
      // Wait for requestLayout() to propagate through this sequence before returning true:
      // requestLayout() -> onMeasure() -> onLayout() -> surfaceChanged().
      return surfaceWidth == layoutWidth && surfaceHeight == layoutHeight;
    }
  }

  /**
   * Renders and releases |pendingFrame|.
   */
  private void renderFrameOnRenderThread() {
    if (eglBase == null || !eglBase.hasSurface()) {
      Logging.d(TAG, "No surface to draw on");
      return;
    }
    if (!checkConsistentLayout()) {
      // Output intermediate black frames while the layout is updated.
      GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
      eglBase.swapBuffers();
      return;
    }
    // After a surface size change, the EGLSurface might still have a buffer of the old size in the
    // pipeline. Querying the EGLSurface will show if the underlying buffer dimensions haven't yet
    // changed. Such a buffer will be rendered incorrectly, so flush it with a black frame.
    synchronized (layoutLock) {
      if (eglBase.surfaceWidth() != surfaceWidth || eglBase.surfaceHeight() != surfaceHeight) {
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
        eglBase.swapBuffers();
      }
    }
    // Fetch and render |pendingFrame|.
    final VideoRenderer.I420Frame frame;
    synchronized (frameLock) {
      if (pendingFrame == null) {
        return;
      }
      frame = pendingFrame;
      pendingFrame = null;
    }

    final long startTimeNs = System.nanoTime();
    final float[] samplingMatrix;
    if (frame.yuvFrame) {
      // The convention in WebRTC is that the first element in a ByteBuffer corresponds to the
      // top-left corner of the image, but in glTexImage2D() the first element corresponds to the
      // bottom-left corner. We correct this discrepancy by setting a vertical flip as sampling
      // matrix.
      samplingMatrix = RendererCommon.verticalFlipMatrix();
    } else {
      // TODO(magjed): Move updateTexImage() to the video source instead.
      SurfaceTexture surfaceTexture = (SurfaceTexture) frame.textureObject;
      surfaceTexture.updateTexImage();
      samplingMatrix = new float[16];
      surfaceTexture.getTransformMatrix(samplingMatrix);
    }

    final float[] texMatrix;
    synchronized (layoutLock) {
      final float[] rotatedSamplingMatrix =
          RendererCommon.rotateTextureMatrix(samplingMatrix, frame.rotationDegree);
      final float[] layoutMatrix = RendererCommon.getLayoutMatrix(
          mirror, frameAspectRatio(), (float) layoutWidth / layoutHeight);
      texMatrix = RendererCommon.multiplyMatrices(rotatedSamplingMatrix, layoutMatrix);
    }

    GLES20.glViewport(0, 0, surfaceWidth, surfaceHeight);
    if (frame.yuvFrame) {
      // Make sure YUV textures are allocated.
      if (yuvTextures == null) {
        yuvTextures = new int[3];
        for (int i = 0; i < 3; i++)  {
          yuvTextures[i] = GlUtil.generateTexture(GLES20.GL_TEXTURE_2D);
        }
      }
      drawer.uploadYuvData(
          yuvTextures, frame.width, frame.height, frame.yuvStrides, frame.yuvPlanes);
      drawer.drawYuv(yuvTextures, texMatrix);
    } else {
      drawer.drawOes(frame.textureId, texMatrix);
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

  // Return current frame aspect ratio, taking rotation into account.
  private float frameAspectRatio() {
    synchronized (layoutLock) {
      if (frameWidth == 0 || frameHeight == 0) {
        return 0.0f;
      }
      return (frameRotation % 180 == 0) ? (float) frameWidth / frameHeight
                                        : (float) frameHeight / frameWidth;
    }
  }

  // Update frame dimensions and report any changes to |rendererEvents|.
  private void updateFrameDimensionsAndReportEvents(VideoRenderer.I420Frame frame) {
    synchronized (layoutLock) {
      if (frameWidth != frame.width || frameHeight != frame.height
          || frameRotation != frame.rotationDegree) {
        if (rendererEvents != null) {
          final String id = getResources().getResourceEntryName(getId());
          if (frameWidth == 0 || frameHeight == 0) {
            Logging.d(TAG, "ID: " + id + ". Reporting first rendered frame.");
            rendererEvents.onFirstFrameRendered();
          }
          Logging.d(TAG, "ID: " + id + ". Reporting frame resolution changed to "
              + frame.width + "x" + frame.height + " with rotation " + frame.rotationDegree);
          rendererEvents.onFrameResolutionChanged(frame.width, frame.height, frame.rotationDegree);
        }
        frameWidth = frame.width;
        frameHeight = frame.height;
        frameRotation = frame.rotationDegree;
      }
    }
  }

  private void logStatistics() {
    synchronized (statisticsLock) {
      Logging.d(TAG, "ID: " + getResources().getResourceEntryName(getId()) + ". Frames received: "
          + framesReceived + ". Dropped: " + framesDropped + ". Rendered: " + framesRendered);
      if (framesReceived > 0 && framesRendered > 0) {
        final long timeSinceFirstFrameNs = System.nanoTime() - firstFrameTimeNs;
        Logging.d(TAG, "Duration: " + (int) (timeSinceFirstFrameNs / 1e6) +
            " ms. FPS: " + (float) framesRendered * 1e9 / timeSinceFirstFrameNs);
        Logging.d(TAG, "Average render time: "
            + (int) (renderTimeNs / (1000 * framesRendered)) + " us.");
      }
    }
  }
}
