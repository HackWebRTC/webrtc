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
import android.opengl.GLES20;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.test.ActivityTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import java.nio.ByteBuffer;

import javax.microedition.khronos.egl.EGL10;

public final class SurfaceTextureHelperTest extends ActivityTestCase {
  /**
   * Mock texture listener with blocking wait functionality.
   */
  public static final class MockTextureListener
      implements SurfaceTextureHelper.OnTextureFrameAvailableListener {
    public int oesTextureId;
    public float[] transformMatrix;
    private boolean hasNewFrame = false;
    // Thread where frames are expected to be received on.
    private final Thread expectedThread;

    MockTextureListener() {
      this.expectedThread = null;
    }

    MockTextureListener(Thread expectedThread) {
      this.expectedThread = expectedThread;
    }

    @Override
    public synchronized void onTextureFrameAvailable(
        int oesTextureId, float[] transformMatrix, long timestampNs) {
      if (expectedThread != null && Thread.currentThread() != expectedThread) {
        throw new IllegalStateException("onTextureFrameAvailable called on wrong thread.");
      }
      this.oesTextureId = oesTextureId;
      this.transformMatrix = transformMatrix;
      hasNewFrame = true;
      notifyAll();
    }

    /**
     * Wait indefinitely for a new frame.
     */
    public synchronized void waitForNewFrame() throws InterruptedException {
      while (!hasNewFrame) {
        wait();
      }
      hasNewFrame = false;
    }

    /**
     * Wait for a new frame, or until the specified timeout elapses. Returns true if a new frame was
     * received before the timeout.
     */
    public synchronized boolean waitForNewFrame(final long timeoutMs) throws InterruptedException {
      final long startTimeMs = SystemClock.elapsedRealtime();
      long timeRemainingMs = timeoutMs;
      while (!hasNewFrame && timeRemainingMs > 0) {
        wait(timeRemainingMs);
        final long elapsedTimeMs = SystemClock.elapsedRealtime() - startTimeMs;
        timeRemainingMs = timeoutMs - elapsedTimeMs;
      }
      final boolean didReceiveFrame = hasNewFrame;
      hasNewFrame = false;
      return didReceiveFrame;
    }
  }

  /**
   * Test normal use by receiving three uniform texture frames. Texture frames are returned as early
   * as possible. The texture pixel values are inspected by drawing the texture frame to a pixel
   * buffer and reading it back with glReadPixels().
   */
  @MediumTest
  public static void testThreeConstantColorFrames() throws InterruptedException {
    final int width = 16;
    final int height = 16;
    // Create EGL base with a pixel buffer as display output.
    final EglBase eglBase = new EglBase(EGL10.EGL_NO_CONTEXT, EglBase.ConfigType.PIXEL_BUFFER);
    eglBase.createPbufferSurface(width, height);
    final GlRectDrawer drawer = new GlRectDrawer();

    // Create SurfaceTextureHelper and listener.
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create(eglBase.getContext());
    final MockTextureListener listener = new MockTextureListener();
    surfaceTextureHelper.setListener(listener);
    surfaceTextureHelper.getSurfaceTexture().setDefaultBufferSize(width, height);

    // Create resources for stubbing an OES texture producer. |eglOesBase| has the SurfaceTexture in
    // |surfaceTextureHelper| as the target EGLSurface.
    final EglBase eglOesBase = new EglBase(eglBase.getContext(), EglBase.ConfigType.PLAIN);
    eglOesBase.createSurface(surfaceTextureHelper.getSurfaceTexture());
    assertEquals(eglOesBase.surfaceWidth(), width);
    assertEquals(eglOesBase.surfaceHeight(), height);

    final int red[] = new int[] {79, 144, 185};
    final int green[] = new int[] {66, 210, 162};
    final int blue[] = new int[] {161, 117, 158};
    // Draw three frames.
    for (int i = 0; i < 3; ++i) {
      // Draw a constant color frame onto the SurfaceTexture.
      eglOesBase.makeCurrent();
      GLES20.glClearColor(red[i] / 255.0f, green[i] / 255.0f, blue[i] / 255.0f, 1.0f);
      GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
      // swapBuffers() will ultimately trigger onTextureFrameAvailable().
      eglOesBase.swapBuffers();

      // Wait for an OES texture to arrive and draw it onto the pixel buffer.
      listener.waitForNewFrame();
      eglBase.makeCurrent();
      drawer.drawOes(listener.oesTextureId, listener.transformMatrix);

      surfaceTextureHelper.returnTextureFrame();

      // Download the pixels in the pixel buffer as RGBA. Not all platforms support RGB, e.g.
      // Nexus 9.
      final ByteBuffer rgbaData = ByteBuffer.allocateDirect(width * height * 4);
      GLES20.glReadPixels(0, 0, width, height, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, rgbaData);
      GlUtil.checkNoGLES2Error("glReadPixels");

      // Assert rendered image is expected constant color.
      while (rgbaData.hasRemaining()) {
        assertEquals(rgbaData.get() & 0xFF, red[i]);
        assertEquals(rgbaData.get() & 0xFF, green[i]);
        assertEquals(rgbaData.get() & 0xFF, blue[i]);
        assertEquals(rgbaData.get() & 0xFF, 255);
      }
    }

    drawer.release();
    surfaceTextureHelper.disconnect();
    eglBase.release();
  }

  /**
   * Test disconnecting the SurfaceTextureHelper while holding a pending texture frame. The pending
   * texture frame should still be valid, and this is tested by drawing the texture frame to a pixel
   * buffer and reading it back with glReadPixels().
   */
  @MediumTest
  public static void testLateReturnFrame() throws InterruptedException {
    final int width = 16;
    final int height = 16;
    // Create EGL base with a pixel buffer as display output.
    final EglBase eglBase = new EglBase(EGL10.EGL_NO_CONTEXT, EglBase.ConfigType.PIXEL_BUFFER);
    eglBase.createPbufferSurface(width, height);

    // Create SurfaceTextureHelper and listener.
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create(eglBase.getContext());
    final MockTextureListener listener = new MockTextureListener();
    surfaceTextureHelper.setListener(listener);
    surfaceTextureHelper.getSurfaceTexture().setDefaultBufferSize(width, height);

    // Create resources for stubbing an OES texture producer. |eglOesBase| has the SurfaceTexture in
    // |surfaceTextureHelper| as the target EGLSurface.
    final EglBase eglOesBase = new EglBase(eglBase.getContext(), EglBase.ConfigType.PLAIN);
    eglOesBase.createSurface(surfaceTextureHelper.getSurfaceTexture());
    assertEquals(eglOesBase.surfaceWidth(), width);
    assertEquals(eglOesBase.surfaceHeight(), height);

    final int red = 79;
    final int green = 66;
    final int blue = 161;
    // Draw a constant color frame onto the SurfaceTexture.
    eglOesBase.makeCurrent();
    GLES20.glClearColor(red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f);
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    // swapBuffers() will ultimately trigger onTextureFrameAvailable().
    eglOesBase.swapBuffers();
    eglOesBase.release();

    // Wait for OES texture frame.
    listener.waitForNewFrame();
    // Diconnect while holding the frame.
    surfaceTextureHelper.disconnect();

    // Draw the pending texture frame onto the pixel buffer.
    eglBase.makeCurrent();
    final GlRectDrawer drawer = new GlRectDrawer();
    drawer.drawOes(listener.oesTextureId, listener.transformMatrix);
    drawer.release();

    // Download the pixels in the pixel buffer as RGBA. Not all platforms support RGB, e.g. Nexus 9.
    final ByteBuffer rgbaData = ByteBuffer.allocateDirect(width * height * 4);
    GLES20.glReadPixels(0, 0, width, height, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, rgbaData);
    GlUtil.checkNoGLES2Error("glReadPixels");
    eglBase.release();

    // Assert rendered image is expected constant color.
    while (rgbaData.hasRemaining()) {
      assertEquals(rgbaData.get() & 0xFF, red);
      assertEquals(rgbaData.get() & 0xFF, green);
      assertEquals(rgbaData.get() & 0xFF, blue);
      assertEquals(rgbaData.get() & 0xFF, 255);
    }
    // Late frame return after everything has been disconnected and released.
    surfaceTextureHelper.returnTextureFrame();
  }

  /**
   * Test disconnecting the SurfaceTextureHelper, but keep trying to produce more texture frames. No
   * frames should be delivered to the listener.
   */
  @MediumTest
  public static void testDisconnect() throws InterruptedException {
    // Create SurfaceTextureHelper and listener.
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create(EGL10.EGL_NO_CONTEXT);
    final MockTextureListener listener = new MockTextureListener();
    surfaceTextureHelper.setListener(listener);
    // Create EglBase with the SurfaceTexture as target EGLSurface.
    final EglBase eglBase = new EglBase(EGL10.EGL_NO_CONTEXT, EglBase.ConfigType.PLAIN);
    eglBase.createSurface(surfaceTextureHelper.getSurfaceTexture());
    eglBase.makeCurrent();
    // Assert no frame has been received yet.
    assertFalse(listener.waitForNewFrame(1));
    // Draw and wait for one frame.
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    // swapBuffers() will ultimately trigger onTextureFrameAvailable().
    eglBase.swapBuffers();
    listener.waitForNewFrame();
    surfaceTextureHelper.returnTextureFrame();

    // Disconnect - we should not receive any textures after this.
    surfaceTextureHelper.disconnect();

    // Draw one frame.
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    eglBase.swapBuffers();
    // swapBuffers() should not trigger onTextureFrameAvailable() because we are disconnected.
    // Assert that no OES texture was delivered.
    assertFalse(listener.waitForNewFrame(500));

    eglBase.release();
  }

  /**
   * Test disconnecting the SurfaceTextureHelper immediately after is has been setup to use a
   * shared context. No frames should be delivered to the listener.
   */
  @SmallTest
  public static void testDisconnectImmediately() {
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create(EGL10.EGL_NO_CONTEXT);
    surfaceTextureHelper.disconnect();
  }

  /**
   * Test use SurfaceTextureHelper on a separate thread. A uniform texture frame is created and
   * received on a thread separate from the test thread.
   */
  @MediumTest
  public static void testFrameOnSeparateThread() throws InterruptedException {
    final HandlerThread thread = new HandlerThread("SurfaceTextureHelperTestThread");
    thread.start();
    final Handler handler = new Handler(thread.getLooper());

    // Create SurfaceTextureHelper and listener.
    final SurfaceTextureHelper surfaceTextureHelper =
        SurfaceTextureHelper.create(EGL10.EGL_NO_CONTEXT, handler);
    // Create a mock listener and expect frames to be delivered on |thread|.
    final MockTextureListener listener = new MockTextureListener(thread);
    surfaceTextureHelper.setListener(listener);

    // Create resources for stubbing an OES texture producer. |eglOesBase| has the
    // SurfaceTexture in |surfaceTextureHelper| as the target EGLSurface.
    final EglBase eglOesBase = new EglBase(EGL10.EGL_NO_CONTEXT, EglBase.ConfigType.PLAIN);
    eglOesBase.createSurface(surfaceTextureHelper.getSurfaceTexture());
    eglOesBase.makeCurrent();
    // Draw a frame onto the SurfaceTexture.
    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
    // swapBuffers() will ultimately trigger onTextureFrameAvailable().
    eglOesBase.swapBuffers();
    eglOesBase.release();

    // Wait for an OES texture to arrive.
    listener.waitForNewFrame();

    // Return the frame from this thread.
    surfaceTextureHelper.returnTextureFrame();
    surfaceTextureHelper.disconnect();
    thread.quitSafely();
  }
}
