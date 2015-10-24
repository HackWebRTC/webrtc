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
import android.view.SurfaceHolder;

import org.webrtc.Logging;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

/**
 * Holds EGL state and utility methods for handling an EGLContext, an EGLDisplay, and an EGLSurface.
 */
public final class EglBase {
  private static final String TAG = "EglBase";
  // These constants are taken from EGL14.EGL_OPENGL_ES2_BIT and EGL14.EGL_CONTEXT_CLIENT_VERSION.
  // https://android.googlesource.com/platform/frameworks/base/+/master/opengl/java/android/opengl/EGL14.java
  // This is similar to how GlSurfaceView does:
  // http://grepcode.com/file/repository.grepcode.com/java/ext/com.google.android/android/5.1.1_r1/android/opengl/GLSurfaceView.java#760
  private static final int EGL_OPENGL_ES2_BIT = 4;
  private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
  // Android-specific extension.
  private static final int EGL_RECORDABLE_ANDROID = 0x3142;

  private final EGL10 egl;
  private EGLContext eglContext;
  private ConfigType configType;
  private EGLConfig eglConfig;
  private EGLDisplay eglDisplay;
  private EGLSurface eglSurface = EGL10.EGL_NO_SURFACE;

  // EGLConfig constructor type. Influences eglChooseConfig arguments.
  public static enum ConfigType {
    // No special parameters.
    PLAIN,
    // Configures with EGL_SURFACE_TYPE = EGL_PBUFFER_BIT.
    PIXEL_BUFFER,
    // Configures with EGL_RECORDABLE_ANDROID = 1.
    // Discourages EGL from using pixel formats that cannot efficiently be
    // converted to something usable by the video encoder.
    RECORDABLE
  }

  // Create root context without any EGLSurface or parent EGLContext. This can be used for branching
  // new contexts that share data.
  public EglBase() {
    this(EGL10.EGL_NO_CONTEXT, ConfigType.PLAIN);
  }

  // Create a new context with the specified config type, sharing data with sharedContext.
  public EglBase(EGLContext sharedContext, ConfigType configType) {
    this.egl = (EGL10) EGLContext.getEGL();
    this.configType = configType;
    eglDisplay = getEglDisplay();
    eglConfig = getEglConfig(eglDisplay, configType);
    eglContext = createEglContext(sharedContext, eglDisplay, eglConfig);
  }

  // Create EGLSurface from the Android SurfaceHolder.
  public void createSurface(SurfaceHolder surfaceHolder) {
    createSurfaceInternal(surfaceHolder);
  }

  // Create EGLSurface from the Android SurfaceTexture.
  public void createSurface(SurfaceTexture surfaceTexture) {
    createSurfaceInternal(surfaceTexture);
  }

  // Create EGLSurface from either a SurfaceHolder or a SurfaceTexture.
  private void createSurfaceInternal(Object nativeWindow) {
    if (!(nativeWindow instanceof SurfaceHolder) && !(nativeWindow instanceof SurfaceTexture)) {
      throw new IllegalStateException("Input must be either a SurfaceHolder or SurfaceTexture");
    }
    checkIsNotReleased();
    if (configType == ConfigType.PIXEL_BUFFER) {
      Logging.w(TAG, "This EGL context is configured for PIXEL_BUFFER, but uses regular Surface");
    }
    if (eglSurface != EGL10.EGL_NO_SURFACE) {
      throw new RuntimeException("Already has an EGLSurface");
    }
    int[] surfaceAttribs = {EGL10.EGL_NONE};
    eglSurface = egl.eglCreateWindowSurface(eglDisplay, eglConfig, nativeWindow, surfaceAttribs);
    if (eglSurface == EGL10.EGL_NO_SURFACE) {
      throw new RuntimeException("Failed to create window surface");
    }
  }

  // Create dummy 1x1 pixel buffer surface so the context can be made current.
  public void createDummyPbufferSurface() {
    createPbufferSurface(1, 1);
  }

  public void createPbufferSurface(int width, int height) {
    checkIsNotReleased();
    if (configType != ConfigType.PIXEL_BUFFER) {
      throw new RuntimeException(
          "This EGL context is not configured to use a pixel buffer: " + configType);
    }
    if (eglSurface != EGL10.EGL_NO_SURFACE) {
      throw new RuntimeException("Already has an EGLSurface");
    }
    int[] surfaceAttribs = {EGL10.EGL_WIDTH, width, EGL10.EGL_HEIGHT, height, EGL10.EGL_NONE};
    eglSurface = egl.eglCreatePbufferSurface(eglDisplay, eglConfig, surfaceAttribs);
    if (eglSurface == EGL10.EGL_NO_SURFACE) {
      throw new RuntimeException("Failed to create pixel buffer surface");
    }
  }

  public EGLContext getContext() {
    return eglContext;
  }

  public boolean hasSurface() {
    return eglSurface != EGL10.EGL_NO_SURFACE;
  }

  public int surfaceWidth() {
    final int widthArray[] = new int[1];
    egl.eglQuerySurface(eglDisplay, eglSurface, EGL10.EGL_WIDTH, widthArray);
    return widthArray[0];
  }

  public int surfaceHeight() {
    final int heightArray[] = new int[1];
    egl.eglQuerySurface(eglDisplay, eglSurface, EGL10.EGL_HEIGHT, heightArray);
    return heightArray[0];
  }

  public void releaseSurface() {
    if (eglSurface != EGL10.EGL_NO_SURFACE) {
      egl.eglDestroySurface(eglDisplay, eglSurface);
      eglSurface = EGL10.EGL_NO_SURFACE;
    }
  }

  private void checkIsNotReleased() {
    if (eglDisplay == EGL10.EGL_NO_DISPLAY || eglContext == EGL10.EGL_NO_CONTEXT
        || eglConfig == null) {
      throw new RuntimeException("This object has been released");
    }
  }

  public void release() {
    checkIsNotReleased();
    releaseSurface();
    detachCurrent();
    egl.eglDestroyContext(eglDisplay, eglContext);
    egl.eglTerminate(eglDisplay);
    eglContext = EGL10.EGL_NO_CONTEXT;
    eglDisplay = EGL10.EGL_NO_DISPLAY;
    eglConfig = null;
  }

  public void makeCurrent() {
    checkIsNotReleased();
    if (eglSurface == EGL10.EGL_NO_SURFACE) {
      throw new RuntimeException("No EGLSurface - can't make current");
    }
    if (!egl.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
      throw new RuntimeException("eglMakeCurrent failed");
    }
  }

  // Detach the current EGL context, so that it can be made current on another thread.
  public void detachCurrent() {
    if (!egl.eglMakeCurrent(
        eglDisplay, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT)) {
      throw new RuntimeException("eglMakeCurrent failed");
    }
  }

  public void swapBuffers() {
    checkIsNotReleased();
    if (eglSurface == EGL10.EGL_NO_SURFACE) {
      throw new RuntimeException("No EGLSurface - can't swap buffers");
    }
    egl.eglSwapBuffers(eglDisplay, eglSurface);
  }

  // Return an EGLDisplay, or die trying.
  private EGLDisplay getEglDisplay() {
    EGLDisplay eglDisplay = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL10.EGL_NO_DISPLAY) {
      throw new RuntimeException("Unable to get EGL10 display");
    }
    int[] version = new int[2];
    if (!egl.eglInitialize(eglDisplay, version)) {
      throw new RuntimeException("Unable to initialize EGL10");
    }
    return eglDisplay;
  }

  // Return an EGLConfig, or die trying.
  private EGLConfig getEglConfig(EGLDisplay eglDisplay, ConfigType configType) {
    // Always RGB888, GLES2.
    int[] configAttributes = {
      EGL10.EGL_RED_SIZE, 8,
      EGL10.EGL_GREEN_SIZE, 8,
      EGL10.EGL_BLUE_SIZE, 8,
      EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL10.EGL_NONE, 0,  // Allocate dummy fields for specific options.
      EGL10.EGL_NONE
    };

    // Fill in dummy fields based on configType.
    switch (configType) {
      case PLAIN:
        break;
      case PIXEL_BUFFER:
        configAttributes[configAttributes.length - 3] = EGL10.EGL_SURFACE_TYPE;
        configAttributes[configAttributes.length - 2] = EGL10.EGL_PBUFFER_BIT;
        break;
      case RECORDABLE:
        configAttributes[configAttributes.length - 3] = EGL_RECORDABLE_ANDROID;
        configAttributes[configAttributes.length - 2] = 1;
        break;
      default:
        throw new IllegalArgumentException();
    }

    EGLConfig[] configs = new EGLConfig[1];
    int[] numConfigs = new int[1];
    if (!egl.eglChooseConfig(
        eglDisplay, configAttributes, configs, configs.length, numConfigs)) {
      throw new RuntimeException("Unable to find RGB888 " + configType + " EGL config");
    }
    return configs[0];
  }

  // Return an EGLConfig, or die trying.
  private EGLContext createEglContext(
      EGLContext sharedContext, EGLDisplay eglDisplay, EGLConfig eglConfig) {
    int[] contextAttributes = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE};
    EGLContext eglContext =
        egl.eglCreateContext(eglDisplay, eglConfig, sharedContext, contextAttributes);
    if (eglContext == EGL10.EGL_NO_CONTEXT) {
      throw new RuntimeException("Failed to create EGL context");
    }
    return eglContext;
  }
}
