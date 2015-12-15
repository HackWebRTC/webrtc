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
import android.view.Surface;

import javax.microedition.khronos.egl.EGL10;


/**
 * Holds EGL state and utility methods for handling an egl 1.0 EGLContext, an EGLDisplay,
 * and an EGLSurface.
 */
public abstract class EglBase {
  // EGL wrapper for an actual EGLContext.
  public static class Context {
  }

  // These constants are taken from EGL14.EGL_OPENGL_ES2_BIT and EGL14.EGL_CONTEXT_CLIENT_VERSION.
  // https://android.googlesource.com/platform/frameworks/base/+/master/opengl/java/android/opengl/EGL14.java
  // This is similar to how GlSurfaceView does:
  // http://grepcode.com/file/repository.grepcode.com/java/ext/com.google.android/android/5.1.1_r1/android/opengl/GLSurfaceView.java#760
  private static final int EGL_OPENGL_ES2_BIT = 4;
  // Android-specific extension.
  private static final int EGL_RECORDABLE_ANDROID = 0x3142;

  public static final int[] CONFIG_PLAIN = {
    EGL10.EGL_RED_SIZE, 8,
    EGL10.EGL_GREEN_SIZE, 8,
    EGL10.EGL_BLUE_SIZE, 8,
    EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL10.EGL_NONE
  };
  public static final int[] CONFIG_RGBA = {
    EGL10.EGL_RED_SIZE, 8,
    EGL10.EGL_GREEN_SIZE, 8,
    EGL10.EGL_BLUE_SIZE, 8,
    EGL10.EGL_ALPHA_SIZE, 8,
    EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL10.EGL_NONE
  };
  public static final int[] CONFIG_PIXEL_BUFFER = {
    EGL10.EGL_RED_SIZE, 8,
    EGL10.EGL_GREEN_SIZE, 8,
    EGL10.EGL_BLUE_SIZE, 8,
    EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL10.EGL_SURFACE_TYPE, EGL10.EGL_PBUFFER_BIT,
    EGL10.EGL_NONE
  };
  public static final int[] CONFIG_PIXEL_RGBA_BUFFER = {
    EGL10.EGL_RED_SIZE, 8,
    EGL10.EGL_GREEN_SIZE, 8,
    EGL10.EGL_BLUE_SIZE, 8,
    EGL10.EGL_ALPHA_SIZE, 8,
    EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL10.EGL_SURFACE_TYPE, EGL10.EGL_PBUFFER_BIT,
    EGL10.EGL_NONE
  };
  public static final int[] CONFIG_RECORDABLE = {
    EGL10.EGL_RED_SIZE, 8,
    EGL10.EGL_GREEN_SIZE, 8,
    EGL10.EGL_BLUE_SIZE, 8,
    EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RECORDABLE_ANDROID, 1,
    EGL10.EGL_NONE
  };

  // Create a new context with the specified config attributes, sharing data with sharedContext.
  // |sharedContext| can be null.
  public static EglBase create(Context sharedContext, int[] configAttributes) {
    return (EglBase14.isEGL14Supported()
        && (sharedContext == null || sharedContext instanceof EglBase14.Context))
            ? new EglBase14((EglBase14.Context) sharedContext, configAttributes)
            : new EglBase10((EglBase10.Context) sharedContext, configAttributes);
  }

  public static EglBase create() {
    return create(null, CONFIG_PLAIN);
  }

  public abstract void createSurface(Surface surface);

  // Create EGLSurface from the Android SurfaceTexture.
  public abstract void createSurface(SurfaceTexture surfaceTexture);

  // Create dummy 1x1 pixel buffer surface so the context can be made current.
  public abstract void createDummyPbufferSurface();

  public abstract void createPbufferSurface(int width, int height);

  public abstract Context getEglBaseContext();

  public abstract boolean hasSurface();

  public abstract int surfaceWidth();

  public abstract int surfaceHeight();

  public abstract void releaseSurface();

  public abstract void release();

  public abstract void makeCurrent();

  // Detach the current EGL context, so that it can be made current on another thread.
  public abstract void detachCurrent();

  public abstract void swapBuffers();
}
