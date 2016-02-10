/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.opengl.GLES20;

import org.webrtc.Logging;

import java.nio.FloatBuffer;

// Helper class for handling OpenGL shaders and shader programs.
public class GlShader {
  private static final String TAG = "GlShader";

  private static int compileShader(int shaderType, String source) {
    int[] result = new int[] {
        GLES20.GL_FALSE
    };
    int shader = GLES20.glCreateShader(shaderType);
    GLES20.glShaderSource(shader, source);
    GLES20.glCompileShader(shader);
    GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, result, 0);
    if (result[0] != GLES20.GL_TRUE) {
      Logging.e(TAG, "Could not compile shader " + shaderType + ":" +
          GLES20.glGetShaderInfoLog(shader));
      throw new RuntimeException(GLES20.glGetShaderInfoLog(shader));
    }
    GlUtil.checkNoGLES2Error("compileShader");
    return shader;
  }

  private int vertexShader;
  private int fragmentShader;
  private int program;

  public GlShader(String vertexSource, String fragmentSource) {
    vertexShader = compileShader(GLES20.GL_VERTEX_SHADER, vertexSource);
    fragmentShader = compileShader(GLES20.GL_FRAGMENT_SHADER, fragmentSource);
    program = GLES20.glCreateProgram();
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
      Logging.e(TAG, "Could not link program: " +
          GLES20.glGetProgramInfoLog(program));
      throw new RuntimeException(GLES20.glGetProgramInfoLog(program));
    }
    GlUtil.checkNoGLES2Error("Creating GlShader");
  }

  public int getAttribLocation(String label) {
    if (program == -1) {
      throw new RuntimeException("The program has been released");
    }
    int location = GLES20.glGetAttribLocation(program, label);
    if (location < 0) {
      throw new RuntimeException("Could not locate '" + label + "' in program");
    }
    return location;
  }

  /**
   * Enable and upload a vertex array for attribute |label|. The vertex data is specified in
   * |buffer| with |dimension| number of components per vertex.
   */
  public void setVertexAttribArray(String label, int dimension, FloatBuffer buffer) {
    if (program == -1) {
      throw new RuntimeException("The program has been released");
    }
    int location = getAttribLocation(label);
    GLES20.glEnableVertexAttribArray(location);
    GLES20.glVertexAttribPointer(location, dimension, GLES20.GL_FLOAT, false, 0, buffer);
    GlUtil.checkNoGLES2Error("setVertexAttribArray");
  }

  public int getUniformLocation(String label) {
    if (program == -1) {
      throw new RuntimeException("The program has been released");
    }
    int location = GLES20.glGetUniformLocation(program, label);
    if (location < 0) {
      throw new RuntimeException("Could not locate uniform '" + label + "' in program");
    }
    return location;
  }

  public void useProgram() {
    if (program == -1) {
      throw new RuntimeException("The program has been released");
    }
    GLES20.glUseProgram(program);
    GlUtil.checkNoGLES2Error("glUseProgram");
  }

  public void release() {
    Logging.d(TAG, "Deleting shader.");
    // Flag shaders for deletion (does not delete until no longer attached to a program).
    if (vertexShader != -1) {
      GLES20.glDeleteShader(vertexShader);
      vertexShader = -1;
    }
    if (fragmentShader != -1) {
      GLES20.glDeleteShader(fragmentShader);
      fragmentShader = -1;
    }
    // Delete program, automatically detaching any shaders from it.
    if (program != -1) {
      GLES20.glDeleteProgram(program);
      program = -1;
    }
  }
}
