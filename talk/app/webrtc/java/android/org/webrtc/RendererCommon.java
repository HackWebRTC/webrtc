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

import android.graphics.Point;
import android.opengl.Matrix;

/**
 * Static helper functions for renderer implementations.
 */
public class RendererCommon {
  /** Interface for reporting rendering events. */
  public static interface RendererEvents {
    /**
     * Callback fired once first frame is rendered.
     */
    public void onFirstFrameRendered();

    /**
     * Callback fired when rendered frame resolution or rotation has changed.
     */
    public void onFrameResolutionChanged(int videoWidth, int videoHeight, int rotation);
  }

  // Types of video scaling:
  // SCALE_ASPECT_FIT - video frame is scaled to fit the size of the view by
  //    maintaining the aspect ratio (black borders may be displayed).
  // SCALE_ASPECT_FILL - video frame is scaled to fill the size of the view by
  //    maintaining the aspect ratio. Some portion of the video frame may be
  //    clipped.
  // SCALE_ASPECT_BALANCED - Compromise between FIT and FILL. Video frame will fill as much as
  // possible of the view while maintaining aspect ratio, under the constraint that at least
  // |BALANCED_VISIBLE_FRACTION| of the frame content will be shown.
  public static enum ScalingType { SCALE_ASPECT_FIT, SCALE_ASPECT_FILL, SCALE_ASPECT_BALANCED }
  // The minimum fraction of the frame content that will be shown for |SCALE_ASPECT_BALANCED|.
  // This limits excessive cropping when adjusting display size.
  private static float BALANCED_VISIBLE_FRACTION = 0.5625f;
  public static final float[] identityMatrix() {
    return new float[] {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1};
  }
  // Matrix with transform y' = 1 - y.
  public static final float[] verticalFlipMatrix() {
    return new float[] {
        1,  0, 0, 0,
        0, -1, 0, 0,
        0,  0, 1, 0,
        0,  1, 0, 1};
  }

  // Matrix with transform x' = 1 - x.
  public static final float[] horizontalFlipMatrix() {
    return new float[] {
        -1, 0, 0, 0,
         0, 1, 0, 0,
         0, 0, 1, 0,
         1, 0, 0, 1};
  }

  /**
   * Returns texture matrix that will have the effect of rotating the frame |rotationDegree|
   * clockwise when rendered.
   */
  public static float[] rotateTextureMatrix(float[] textureMatrix, float rotationDegree) {
    final float[] rotationMatrix = new float[16];
    Matrix.setRotateM(rotationMatrix, 0, rotationDegree, 0, 0, 1);
    adjustOrigin(rotationMatrix);
    return multiplyMatrices(textureMatrix, rotationMatrix);
  }

  /**
   * Returns new matrix with the result of a * b.
   */
  public static float[] multiplyMatrices(float[] a, float[] b) {
    final float[] resultMatrix = new float[16];
    Matrix.multiplyMM(resultMatrix, 0, a, 0, b, 0);
    return resultMatrix;
  }

  /**
   * Returns layout transformation matrix that applies an optional mirror effect and compensates
   * for video vs display aspect ratio.
   */
  public static float[] getLayoutMatrix(
      boolean mirror, float videoAspectRatio, float displayAspectRatio) {
    float scaleX = 1;
    float scaleY = 1;
    // Scale X or Y dimension so that video and display size have same aspect ratio.
    if (displayAspectRatio > videoAspectRatio) {
      scaleY = videoAspectRatio / displayAspectRatio;
    } else {
      scaleX = displayAspectRatio / videoAspectRatio;
    }
    // Apply optional horizontal flip.
    if (mirror) {
      scaleX *= -1;
    }
    final float matrix[] = new float[16];
    Matrix.setIdentityM(matrix, 0);
    Matrix.scaleM(matrix, 0, scaleX, scaleY, 1);
    adjustOrigin(matrix);
    return matrix;
  }

  /**
   * Calculate display size based on scaling type, video aspect ratio, and maximum display size.
   */
  public static Point getDisplaySize(ScalingType scalingType, float videoAspectRatio,
      int maxDisplayWidth, int maxDisplayHeight) {
    return getDisplaySize(convertScalingTypeToVisibleFraction(scalingType), videoAspectRatio,
        maxDisplayWidth, maxDisplayHeight);
  }

  /**
   * Move |matrix| transformation origin to (0.5, 0.5). This is the origin for texture coordinates
   * that are in the range 0 to 1.
   */
  private static void adjustOrigin(float[] matrix) {
    // Note that OpenGL is using column-major order.
    // Pre translate with -0.5 to move coordinates to range [-0.5, 0.5].
    matrix[12] -= 0.5f * (matrix[0] + matrix[4]);
    matrix[13] -= 0.5f * (matrix[1] + matrix[5]);
    // Post translate with 0.5 to move coordinates to range [0, 1].
    matrix[12] += 0.5f;
    matrix[13] += 0.5f;
  }

  /**
   * Each scaling type has a one-to-one correspondence to a numeric minimum fraction of the video
   * that must remain visible.
   */
  private static float convertScalingTypeToVisibleFraction(ScalingType scalingType) {
    switch (scalingType) {
      case SCALE_ASPECT_FIT:
        return 1.0f;
      case SCALE_ASPECT_FILL:
        return 0.0f;
      case SCALE_ASPECT_BALANCED:
        return BALANCED_VISIBLE_FRACTION;
      default:
        throw new IllegalArgumentException();
    }
  }

  /**
   * Calculate display size based on minimum fraction of the video that must remain visible,
   * video aspect ratio, and maximum display size.
   */
  private static Point getDisplaySize(float minVisibleFraction, float videoAspectRatio,
      int maxDisplayWidth, int maxDisplayHeight) {
    // If there is no constraint on the amount of cropping, fill the allowed display area.
    if (minVisibleFraction == 0 || videoAspectRatio == 0) {
      return new Point(maxDisplayWidth, maxDisplayHeight);
    }
    // Each dimension is constrained on max display size and how much we are allowed to crop.
    final int width = Math.min(maxDisplayWidth,
        (int) (maxDisplayHeight / minVisibleFraction * videoAspectRatio));
    final int height = Math.min(maxDisplayHeight,
        (int) (maxDisplayWidth / minVisibleFraction / videoAspectRatio));
    return new Point(width, height);
  }
}
