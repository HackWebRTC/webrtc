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

import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.util.Log;
import android.view.Surface;

/**
 * Holds EGL state and utility methods for handling an EGLContext, an EGLDisplay, and an EGLSurface.
 */
public final class EglBase {
  private static final String TAG = "EglBase";
  private static final int EGL14_SDK_VERSION = android.os.Build.VERSION_CODES.JELLY_BEAN_MR1;
  private static final int CURRENT_SDK_VERSION = android.os.Build.VERSION.SDK_INT;
  // Android-specific extension.
  private static final int EGL_RECORDABLE_ANDROID = 0x3142;

  private EGLContext eglContext;
  private ConfigType configType;
  private EGLConfig eglConfig;
  private EGLDisplay eglDisplay;
  private EGLSurface eglSurface = EGL14.EGL_NO_SURFACE;

  public static boolean isEGL14Supported() {
    Log.d(TAG, "SDK version: " + CURRENT_SDK_VERSION);
    return (CURRENT_SDK_VERSION >= EGL14_SDK_VERSION);
  }

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
    this(EGL14.EGL_NO_CONTEXT, ConfigType.PLAIN);
  }

  // Create a new context with the specified config type, sharing data with sharedContext.
  public EglBase(EGLContext sharedContext, ConfigType configType) {
    this.configType = configType;
    eglDisplay = getEglDisplay();
    eglConfig = getEglConfig(eglDisplay, configType);
    eglContext = createEglContext(sharedContext, eglDisplay, eglConfig);
  }

  // Create EGLSurface from the Android Surface.
  public void createSurface(Surface surface) {
    checkIsNotReleased();
    if (configType == ConfigType.PIXEL_BUFFER) {
      Log.w(TAG, "This EGL context is configured for PIXEL_BUFFER, but uses regular Surface");
    }
    if (eglSurface != EGL14.EGL_NO_SURFACE) {
      throw new RuntimeException("Already has an EGLSurface");
    }
    int[] surfaceAttribs = {EGL14.EGL_NONE};
    eglSurface = EGL14.eglCreateWindowSurface(eglDisplay, eglConfig, surface, surfaceAttribs, 0);
    if (eglSurface == EGL14.EGL_NO_SURFACE) {
      throw new RuntimeException("Failed to create window surface");
    }
  }

  // Create dummy 1x1 pixel buffer surface so the context can be made current.
  public void createDummyPbufferSurface() {
    checkIsNotReleased();
    if (configType != ConfigType.PIXEL_BUFFER) {
      throw new RuntimeException(
          "This EGL context is not configured to use a pixel buffer: " + configType);
    }
    if (eglSurface != EGL14.EGL_NO_SURFACE) {
      throw new RuntimeException("Already has an EGLSurface");
    }
    int[] surfaceAttribs = {EGL14.EGL_WIDTH, 1, EGL14.EGL_HEIGHT, 1, EGL14.EGL_NONE};
    eglSurface = EGL14.eglCreatePbufferSurface(eglDisplay, eglConfig, surfaceAttribs, 0);
    if (eglSurface == EGL14.EGL_NO_SURFACE) {
      throw new RuntimeException("Failed to create pixel buffer surface");
    }
  }

  public EGLContext getContext() {
    return eglContext;
  }

  public boolean hasSurface() {
    return eglSurface != EGL14.EGL_NO_SURFACE;
  }

  public void releaseSurface() {
    if (eglSurface != EGL14.EGL_NO_SURFACE) {
      EGL14.eglDestroySurface(eglDisplay, eglSurface);
      eglSurface = EGL14.EGL_NO_SURFACE;
    }
  }

  private void checkIsNotReleased() {
    if (eglDisplay == EGL14.EGL_NO_DISPLAY || eglContext == EGL14.EGL_NO_CONTEXT
        || eglConfig == null) {
      throw new RuntimeException("This object has been released");
    }
  }

  public void release() {
    checkIsNotReleased();
    releaseSurface();
    // Release our context.
    EGL14.eglMakeCurrent(
        eglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT);
    EGL14.eglDestroyContext(eglDisplay, eglContext);
    EGL14.eglReleaseThread();
    EGL14.eglTerminate(eglDisplay);
    eglContext = EGL14.EGL_NO_CONTEXT;
    eglDisplay = EGL14.EGL_NO_DISPLAY;
    eglConfig = null;
  }

  public void makeCurrent() {
    checkIsNotReleased();
    if (eglSurface == EGL14.EGL_NO_SURFACE) {
      throw new RuntimeException("No EGLSurface - can't make current");
    }
    if (!EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
      throw new RuntimeException("eglMakeCurrent failed");
    }
  }

  public void swapBuffers() {
    checkIsNotReleased();
    if (eglSurface == EGL14.EGL_NO_SURFACE) {
      throw new RuntimeException("No EGLSurface - can't swap buffers");
    }
    EGL14.eglSwapBuffers(eglDisplay, eglSurface);
  }

  // Return an EGLDisplay, or die trying.
  private static EGLDisplay getEglDisplay() {
    EGLDisplay eglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL14.EGL_NO_DISPLAY) {
      throw new RuntimeException("Unable to get EGL14 display");
    }
    int[] version = new int[2];
    if (!EGL14.eglInitialize(eglDisplay, version, 0, version, 1)) {
      throw new RuntimeException("Unable to initialize EGL14");
    }
    return eglDisplay;
  }

  // Return an EGLConfig, or die trying.
  private static EGLConfig getEglConfig(EGLDisplay eglDisplay, ConfigType configType) {
    // Always RGB888, GLES2.
    int[] configAttributes = {
      EGL14.EGL_RED_SIZE, 8,
      EGL14.EGL_GREEN_SIZE, 8,
      EGL14.EGL_BLUE_SIZE, 8,
      EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
      EGL14.EGL_NONE, 0,  // Allocate dummy fields for specific options.
      EGL14.EGL_NONE
    };

    // Fill in dummy fields based on configType.
    switch (configType) {
      case PLAIN:
        break;
      case PIXEL_BUFFER:
        configAttributes[configAttributes.length - 3] = EGL14.EGL_SURFACE_TYPE;
        configAttributes[configAttributes.length - 2] = EGL14.EGL_PBUFFER_BIT;
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
    if (!EGL14.eglChooseConfig(
        eglDisplay, configAttributes, 0, configs, 0, configs.length, numConfigs, 0)) {
      throw new RuntimeException("Unable to find RGB888 " + configType + " EGL config");
    }
    return configs[0];
  }

  // Return an EGLConfig, or die trying.
  private static EGLContext createEglContext(
      EGLContext sharedContext, EGLDisplay eglDisplay, EGLConfig eglConfig) {
    int[] contextAttributes = {EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE};
    EGLContext eglContext =
        EGL14.eglCreateContext(eglDisplay, eglConfig, sharedContext, contextAttributes, 0);
    if (eglContext == EGL14.EGL_NO_CONTEXT) {
      throw new RuntimeException("Failed to create EGL context");
    }
    return eglContext;
  }
}
