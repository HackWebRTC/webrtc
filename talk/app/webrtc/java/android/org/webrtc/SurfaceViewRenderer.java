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

import java.nio.ByteBuffer;

import android.content.Context;
import android.graphics.Point;
import android.graphics.SurfaceTexture;
import android.opengl.EGLContext;
import android.opengl.GLES20;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * Implements org.webrtc.VideoRenderer.Callbacks by displaying the video stream on a SurfaceView.
 * renderFrame() is asynchronous to avoid blocking the calling thread. Instead, a shallow copy of
 * the frame is posted to a dedicated render thread.
 * This class is thread safe and handles access from potentially four different threads:
 * Interaction from the main app in init, release, setMirror, and setScalingtype.
 * Interaction from C++ webrtc::VideoRendererInterface in renderFrame and canApplyRotation.
 * Interaction from the Activity lifecycle in surfaceCreated, surfaceChanged, and surfaceDestroyed.
 * Interaction with the layout framework in onMeasure and onSizeChanged.
 */
public class SurfaceViewRenderer extends SurfaceView
    implements SurfaceHolder.Callback, VideoRenderer.Callbacks {
  private static final String TAG = "SurfaceViewRenderer";

  // Dedicated render thread. Synchronized on |this|.
  private HandlerThread renderThread;
  // Handler for inter-thread communication. Synchronized on |this|.
  private Handler renderThreadHandler;
  // Pending frame to render. Serves as a queue with size 1. Synchronized on |this|.
  private VideoRenderer.I420Frame pendingFrame;

  // EGL and GL resources for drawing YUV/OES textures. After initilization, these are only accessed
  // from the render thread.
  private EglBase eglBase;
  private GlRectDrawer drawer;
  // Texture ids for YUV frames. Allocated on first arrival of a YUV frame.
  private int[] yuvTextures = null;
  // Intermediate copy buffers in case yuv frames are not packed, i.e. stride > plane width. One for
  // Y, and one for U and V.
  private final ByteBuffer[] copyBuffer = new ByteBuffer[2];

  // These variables are synchronized on |layoutLock|.
  private final Object layoutLock = new Object();
  // Current surface size.
  public int surfaceWidth;
  public int surfaceHeight;
  // Most recent measurement specification from onMeasure().
  private int widthSpec;
  private int heightSpec;
  // Current size on screen in pixels.
  public int layoutWidth;
  public int layoutHeight;
  // Desired layout size, or 0 if no frame has arrived yet. The desired size is updated before
  // rendering a new frame, and is enforced in onMeasure(). Rendering is blocked until layout is
  // updated to the desired size.
  public int desiredLayoutWidth;
  public int desiredLayoutHeight;
  // |scalingType| determines how the video will fill the allowed layout area in onMeasure().
  private RendererCommon.ScalingType scalingType = RendererCommon.ScalingType.SCALE_ASPECT_BALANCED;
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
  }

  /**
   * Standard View constructor. In order to render something, you must first call init().
   */
  public SurfaceViewRenderer(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  /**
   * Initialize this class, sharing resources with |sharedContext|.
   */
  public synchronized void init(EGLContext sharedContext) {
    if (renderThreadHandler != null) {
      throw new IllegalStateException("Already initialized");
    }
    Log.d(TAG, "Initializing");
    renderThread = new HandlerThread(TAG);
    renderThread.start();
    renderThreadHandler = new Handler(renderThread.getLooper());
    eglBase = new EglBase(sharedContext, EglBase.ConfigType.PLAIN);
    drawer = new GlRectDrawer();
    getHolder().addCallback(this);
  }

  /**
   * Release all resources. This needs to be done manually, otherwise the resources are leaked.
   */
  public synchronized void release() {
    if (renderThreadHandler == null) {
      Log.d(TAG, "Already released");
      return;
    }
    // Release EGL and GL resources on render thread.
    renderThreadHandler.post(new Runnable() {
      @Override public void run() {
        drawer.release();
        drawer = null;
        if (yuvTextures != null) {
          GLES20.glDeleteTextures(3, yuvTextures, 0);
          yuvTextures = null;
        }
        eglBase.release();
        eglBase = null;
      }
    });
    // Don't accept any more messages to the render thread.
    renderThreadHandler = null;
    // Quit safely to make sure the EGL/GL cleanup posted above is executed.
    renderThread.quitSafely();
    renderThread = null;

    getHolder().removeCallback(this);
    if (pendingFrame != null) {
      VideoRenderer.renderFrameDone(pendingFrame);
      pendingFrame = null;
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
    synchronized (this) {
      if (renderThreadHandler == null) {
        Log.d(TAG, "Dropping frame - SurfaceViewRenderer not initialized or already released.");
        VideoRenderer.renderFrameDone(frame);
        return;
      }
      if (pendingFrame != null) {
        synchronized (statisticsLock) {
          ++framesDropped;
        }
        Log.d(TAG, "Dropping frame - previous frame has not been rendered yet.");
        VideoRenderer.renderFrameDone(frame);
        return;
      }
      pendingFrame = frame;
      renderThreadHandler.post(renderFrameRunnable);
    }
  }

  @Override
  public boolean canApplyRotation() {
    return true;
  }

  // View layout interface.
  @Override
  protected void onMeasure(int widthSpec, int heightSpec) {
    synchronized (layoutLock) {
      this.widthSpec = widthSpec;
      this.heightSpec = heightSpec;
      if (desiredLayoutWidth == 0 || desiredLayoutHeight == 0) {
        super.onMeasure(widthSpec, heightSpec);
      } else {
        setMeasuredDimension(desiredLayoutWidth, desiredLayoutHeight);
     }
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
    Log.d(TAG, "Surface created");
    runOnRenderThread(new Runnable() {
      @Override public void run() {
        eglBase.createSurface(holder.getSurface());
        eglBase.makeCurrent();
        // Necessary for YUV frames with odd width.
        GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
      }
    });
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder holder) {
    Log.d(TAG, "Surface destroyed");
    synchronized (layoutLock) {
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
    Log.d(TAG, "Surface changed: " + width + "x" + height);
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
  private synchronized void runOnRenderThread(Runnable runnable) {
    if (renderThreadHandler != null) {
      renderThreadHandler.post(runnable);
    }
  }

  private synchronized void runOnRenderThreadDelayed(Runnable runnable, long ms) {
    if (renderThreadHandler != null) {
      renderThreadHandler.postDelayed(runnable, ms);
    }
  }

  /**
   * Renders and releases |pendingFrame|.
   */
  private void renderFrameOnRenderThread() {
    if (eglBase == null || !eglBase.hasSurface()) {
      Log.d(TAG, "No surface to draw on");
      return;
    }
    final float videoAspectRatio;
    synchronized (this) {
      if (pendingFrame == null) {
        return;
      }
      videoAspectRatio = (float) pendingFrame.rotatedWidth() / pendingFrame.rotatedHeight();
    }
    // Request new layout if necessary. Don't continue until layout and surface size are in a good
    // state.
    synchronized (layoutLock) {
      final int maxWidth = getDefaultSize(Integer.MAX_VALUE, widthSpec);
      final int maxHeight = getDefaultSize(Integer.MAX_VALUE, heightSpec);
      final Point suggestedSize =
          RendererCommon.getDisplaySize(scalingType, videoAspectRatio, maxWidth, maxHeight);
      desiredLayoutWidth =
          MeasureSpec.getMode(widthSpec) == MeasureSpec.EXACTLY ? maxWidth : suggestedSize.x;
      desiredLayoutHeight =
          MeasureSpec.getMode(heightSpec) == MeasureSpec.EXACTLY ? maxHeight : suggestedSize.y;
      if (desiredLayoutWidth != layoutWidth || desiredLayoutHeight != layoutHeight) {
        Log.d(TAG, "Requesting new layout with size: "
            + desiredLayoutWidth + "x" + desiredLayoutHeight);
        // Output an intermediate black frame while the layout is updated.
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
        eglBase.swapBuffers();
        // Request layout update on UI thread.
        post(new Runnable() {
          @Override public void run() {
            requestLayout();
          }
        });
        return;
      }
      if (surfaceWidth != layoutWidth || surfaceHeight != layoutHeight) {
        Log.d(TAG, "Postponing rendering until surface size is updated.");
        return;
      }
    }

    // The EGLSurface might have a buffer of the old size in the pipeline, even after
    // surfaceChanged() has been called. Querying the EGLSurface will show if the underlying buffer
    // dimensions haven't yet changed.
    if (eglBase.surfaceWidth() != surfaceWidth || eglBase.surfaceHeight() != surfaceHeight) {
      Log.d(TAG, "Flushing old egl surface buffer with incorrect size.");
      // There is no way to display the old buffer correctly, so just make it black, and immediately
      // render |pendingFrame| on the next buffer.
      GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
      eglBase.swapBuffers();
      // In some rare cases, the next buffer is not updated either. In those cases, wait 1 ms and
      // try again.
      if (eglBase.surfaceWidth() != surfaceWidth || eglBase.surfaceHeight() != surfaceHeight) {
        Log.e(TAG, "Unexpected buffer size even after swapBuffers() has been called.");
        runOnRenderThreadDelayed(renderFrameRunnable, 1);
        return;
      }
    }

    // Finally, layout, surface, and EGLSurface are in a good state. Fetch and render pendingFrame|.
    final VideoRenderer.I420Frame frame;
    synchronized (this) {
      if (pendingFrame == null) {
        return;
      }
      frame = pendingFrame;
      pendingFrame = null;
    }

    final long startTimeNs = System.nanoTime();
    final float[] texMatrix = new float[16];
    synchronized (layoutLock) {
      RendererCommon.getTextureMatrix(texMatrix, frame.rotationDegree, mirror, videoAspectRatio,
          (float) layoutWidth / layoutHeight);
    }

    GLES20.glViewport(0, 0, surfaceWidth, surfaceHeight);
    if (frame.yuvFrame) {
      uploadYuvData(frame.width, frame.height, frame.yuvStrides, frame.yuvPlanes);
      drawer.drawYuv(frame.width, frame.height, yuvTextures, texMatrix);
    } else {
      SurfaceTexture surfaceTexture = (SurfaceTexture) frame.textureObject;
      // TODO(magjed): Move updateTexImage() to the video source instead.
      surfaceTexture.updateTexImage();
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

  private void uploadYuvData(int width, int height, int[] strides, ByteBuffer[] planes) {
    // Make sure YUV textures are allocated.
    if (yuvTextures == null) {
      yuvTextures = new int[3];
      // Generate 3 texture ids for Y/U/V and place them into |yuvTextures|.
      GLES20.glGenTextures(3, yuvTextures, 0);
      for (int i = 0; i < 3; i++)  {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
            GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
            GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
            GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D,
            GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
      }
      GlUtil.checkNoGLES2Error("y/u/v glGenTextures");
    }
    // Upload each plane.
    for (int i = 0; i < 3; ++i) {
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);
      final int planeWidth = (i == 0) ? width : width / 2;
      final int planeHeight = (i == 0) ? height : height / 2;
      final int bufferIndex = (i == 0) ? 0 : 1;
      // GLES only accepts packed data, i.e. stride == planeWidth.
      final ByteBuffer packedByteBuffer;
      if (strides[i] == planeWidth) {
        // Input is packed already.
        packedByteBuffer = planes[i];
      } else {
        // Make an intermediate packed copy.
        final int capacityNeeded = planeWidth * planeHeight;
        if (copyBuffer[bufferIndex] == null
            || copyBuffer[bufferIndex].capacity() != capacityNeeded) {
          copyBuffer[bufferIndex] = ByteBuffer.allocateDirect(capacityNeeded);
        }
        packedByteBuffer = copyBuffer[bufferIndex];
        VideoRenderer.nativeCopyPlane(
            planes[i], planeWidth, planeHeight, strides[i], packedByteBuffer, planeWidth);
      }
      GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE, planeWidth, planeHeight, 0,
          GLES20.GL_LUMINANCE, GLES20.GL_UNSIGNED_BYTE, packedByteBuffer);
    }
  }

  private void logStatistics() {
    synchronized (statisticsLock) {
      Log.d(TAG, "ID: " + getResources().getResourceEntryName(getId()) + ". Frames received: "
          + framesReceived + ". Dropped: " + framesDropped + ". Rendered: " + framesRendered);
      if (framesReceived > 0 && framesRendered > 0) {
        final long timeSinceFirstFrameNs = System.nanoTime() - firstFrameTimeNs;
        Log.d(TAG, "Duration: " + (int) (timeSinceFirstFrameNs / 1e6) +
            " ms. FPS: " + (float) framesRendered * 1e9 / timeSinceFirstFrameNs);
        Log.d(TAG, "Average render time: "
            + (int) (renderTimeNs / (1000 * framesRendered)) + " us.");
      }
    }
  }
}
