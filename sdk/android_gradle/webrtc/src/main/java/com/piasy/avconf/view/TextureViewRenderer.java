/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package com.piasy.avconf.view;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.SurfaceTexture;
import android.os.Looper;
import android.util.AttributeSet;
import android.view.TextureView;
import java.util.concurrent.CountDownLatch;
import org.webrtc.EglBase;
import org.webrtc.EglRenderer;
import org.webrtc.GlRectDrawer;
import org.webrtc.Logging;
import org.webrtc.RendererCommon;
import org.webrtc.SurfaceEglRenderer;
import org.webrtc.ThreadUtils;
import org.webrtc.VideoFrame;
import org.webrtc.VideoSink;

/**
 * Display the video stream on a SurfaceView.
 */
public class TextureViewRenderer extends TextureView
    implements TextureView.SurfaceTextureListener, VideoSink, RendererCommon.RendererEvents {
  private static final String TAG = "TextureViewRenderer";

  protected final String resourceName;
  private final RendererCommon.VideoLayoutMeasure videoLayoutMeasure =
      new RendererCommon.VideoLayoutMeasure();
  private final SurfaceEglRenderer eglRenderer;

  // Callback for reporting renderer events. Read-only after initilization so no lock required.
  private RendererCommon.RendererEvents rendererEvents;

  // Accessed only on the main thread.
  private int rotatedFrameWidth;
  private int rotatedFrameHeight;
  private boolean enableFixedSize;
  private int surfaceWidth;
  private int surfaceHeight;

  public TextureViewRenderer(Context context) {
    this(context, (AttributeSet) null);
  }

  public TextureViewRenderer(Context context, AttributeSet attrs) {
    super(context, attrs);
    this.resourceName = getResourceName();
    eglRenderer = new SurfaceEglRenderer(resourceName);
    setSurfaceTextureListener(this);
  }

  public TextureViewRenderer(Context context, String resourceName) {
    super(context);
    this.resourceName = resourceName;
    eglRenderer = new SurfaceEglRenderer(resourceName);
    setSurfaceTextureListener(this);
  }

  /**
   * Initialize this class, sharing resources with |sharedContext|. It is allowed to call init() to
   * reinitialize the renderer after a previous init()/release() cycle.
   */
  public void init(EglBase.Context sharedContext, RendererCommon.RendererEvents rendererEvents) {
    init(sharedContext, rendererEvents, EglBase.CONFIG_PLAIN, new GlRectDrawer());
  }

  /**
   * Initialize this class, sharing resources with |sharedContext|. The custom |drawer| will be used
   * for drawing frames on the EGLSurface. This class is responsible for calling release() on
   * |drawer|. It is allowed to call init() to reinitialize the renderer after a previous
   * init()/release() cycle.
   */
  public void init(final EglBase.Context sharedContext,
      RendererCommon.RendererEvents rendererEvents, final int[] configAttributes,
      RendererCommon.GlDrawer drawer) {
    ThreadUtils.checkIsOnMainThread();
    this.rendererEvents = rendererEvents;
    rotatedFrameWidth = 0;
    rotatedFrameHeight = 0;
    eglRenderer.init(sharedContext, this /* rendererEvents */, configAttributes, drawer);
  }

  /**
   * Block until any pending frame is returned and all GL resources released, even if an interrupt
   * occurs. If an interrupt occurs during release(), the interrupt flag will be set. This function
   * should be called before the Activity is destroyed and the EGLContext is still valid. If you
   * don't call this function, the GL resources might leak.
   */
  public void release() {
    eglRenderer.release();
  }

  /**
   * Register a callback to be invoked when a new video frame has been received.
   *
   * @param listener The callback to be invoked. The callback will be invoked on the render thread.
   *                 It should be lightweight and must not call removeFrameListener.
   * @param scale    The scale of the Bitmap passed to the callback, or 0 if no Bitmap is
   *                 required.
   * @param drawer   Custom drawer to use for this frame listener.
   */
  public void addFrameListener(
      EglRenderer.FrameListener listener, float scale, RendererCommon.GlDrawer drawerParam) {
    eglRenderer.addFrameListener(listener, scale, drawerParam);
  }

  /**
   * Register a callback to be invoked when a new video frame has been received. This version uses
   * the drawer of the EglRenderer that was passed in init.
   *
   * @param listener The callback to be invoked. The callback will be invoked on the render thread.
   *                 It should be lightweight and must not call removeFrameListener.
   * @param scale    The scale of the Bitmap passed to the callback, or 0 if no Bitmap is
   *                 required.
   */
  public void addFrameListener(EglRenderer.FrameListener listener, float scale) {
    eglRenderer.addFrameListener(listener, scale);
  }

  public void removeFrameListener(EglRenderer.FrameListener listener) {
    eglRenderer.removeFrameListener(listener);
  }

  /**
   * Set if the video stream should be mirrored or not.
   */
  public void setMirror(final boolean mirror) {
    eglRenderer.setMirror(mirror);
  }

  /**
   * Set how the video will fill the allowed layout area.
   */
  public void setScalingType(RendererCommon.ScalingType scalingType) {
    ThreadUtils.checkIsOnMainThread();
    videoLayoutMeasure.setScalingType(scalingType);
    requestLayout();
  }

  public void setScalingType(RendererCommon.ScalingType scalingTypeMatchOrientation,
      RendererCommon.ScalingType scalingTypeMismatchOrientation) {
    ThreadUtils.checkIsOnMainThread();
    videoLayoutMeasure.setScalingType(scalingTypeMatchOrientation, scalingTypeMismatchOrientation);
    requestLayout();
  }

  /**
   * Limit render framerate.
   *
   * @param fps Limit render framerate to this value, or use Float.POSITIVE_INFINITY to disable fps
   *            reduction.
   */
  public void setFpsReduction(float fps) {
    eglRenderer.setFpsReduction(fps);
  }

  public void disableFpsReduction() {
    eglRenderer.disableFpsReduction();
  }

  public void pauseVideo() {
    eglRenderer.pauseVideo();
  }

  // VideoSink interface.
  @Override
  public void onFrame(VideoFrame frame) {
    eglRenderer.onFrame(frame);
  }

  // View layout interface.
  @Override
  protected void onMeasure(int widthSpec, int heightSpec) {
    ThreadUtils.checkIsOnMainThread();
    Point size =
        videoLayoutMeasure.measure(widthSpec, heightSpec, rotatedFrameWidth, rotatedFrameHeight);
    setMeasuredDimension(size.x, size.y);
    logD("onMeasure(). New size: " + size.x + "x" + size.y);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    ThreadUtils.checkIsOnMainThread();
    eglRenderer.setLayoutAspectRatio((right - left) / (float) (bottom - top));
  }

  @Override
  public void onSurfaceTextureAvailable(final SurfaceTexture surface, final int width,
      final int height) {
    logD("onSurfaceTextureAvailable: " + surface + " size: " + width + "x" + height);
    ThreadUtils.checkIsOnMainThread();
    eglRenderer.createEglSurface(surface);
  }

  @Override
  public void onSurfaceTextureSizeChanged(final SurfaceTexture surface, final int width,
      final int height) {
    logD("onSurfaceTextureSizeChanged: " + surface + " size: " + width + "x" + height);
  }

  @Override
  public boolean onSurfaceTextureDestroyed(final SurfaceTexture surface) {
    logD("onSurfaceTextureDestroyed: " + surface);
    ThreadUtils.checkIsOnMainThread();
    final CountDownLatch completionLatch = new CountDownLatch(1);
    eglRenderer.releaseEglSurface(completionLatch::countDown);
    ThreadUtils.awaitUninterruptibly(completionLatch);
    return true;
  }

  @Override
  public void onSurfaceTextureUpdated(final SurfaceTexture surface) {
  }

  private String getResourceName() {
    try {
      return getResources().getResourceEntryName(getId());
    } catch (Resources.NotFoundException e) {
      return "";
    }
  }

  /**
   * Post a task to clear the SurfaceView to a transparent uniform color.
   */
  public void clearImage() {
    eglRenderer.clearImage();
  }

  @Override
  public void onFirstFrameRendered() {
    if (rendererEvents != null) {
      rendererEvents.onFirstFrameRendered();
    }
  }

  @Override
  public void onFrameResolutionChanged(int videoWidth, int videoHeight, int rotation) {
    if (rendererEvents != null) {
      rendererEvents.onFrameResolutionChanged(videoWidth, videoHeight, rotation);
    }
    int rotatedWidth = rotation == 0 || rotation == 180 ? videoWidth : videoHeight;
    int rotatedHeight = rotation == 0 || rotation == 180 ? videoHeight : videoWidth;
    // run immediately if possible for ui thread tests
    postOrRun(() -> {
      rotatedFrameWidth = rotatedWidth;
      rotatedFrameHeight = rotatedHeight;
      requestLayout();
    });
  }

  private void postOrRun(Runnable r) {
    if (Thread.currentThread() == Looper.getMainLooper().getThread()) {
      r.run();
    } else {
      post(r);
    }
  }

  private void logD(String string) {
    Logging.d(TAG, resourceName + ": " + string);
  }
}
