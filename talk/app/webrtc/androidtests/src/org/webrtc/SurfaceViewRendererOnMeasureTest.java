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
import android.test.ActivityTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import android.view.View.MeasureSpec;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;

import javax.microedition.khronos.egl.EGL10;

public final class SurfaceViewRendererOnMeasureTest extends ActivityTestCase {
  /**
   * List with all possible scaling types.
   */
  private static final List<RendererCommon.ScalingType> scalingTypes = Arrays.asList(
      RendererCommon.ScalingType.SCALE_ASPECT_FIT,
      RendererCommon.ScalingType.SCALE_ASPECT_FILL,
      RendererCommon.ScalingType.SCALE_ASPECT_BALANCED);

  /**
   * List with MeasureSpec modes.
   */
  private static final List<Integer> measureSpecModes =
      Arrays.asList(MeasureSpec.EXACTLY, MeasureSpec.AT_MOST);

  /**
   * Returns a dummy YUV frame.
   */
  static VideoRenderer.I420Frame createFrame(int width, int height, int rotationDegree) {
    final int[] yuvStrides = new int[] {width, (width + 1) / 2, (width + 1) / 2};
    final int[] yuvHeights = new int[] {height, (height + 1) / 2, (height + 1) / 2};
    final ByteBuffer[] yuvPlanes = new ByteBuffer[3];
    for (int i = 0; i < 3; ++i) {
      yuvPlanes[i] = ByteBuffer.allocateDirect(yuvStrides[i] * yuvHeights[i]);
    }
    return new VideoRenderer.I420Frame(width, height, rotationDegree, yuvStrides, yuvPlanes, 0);
  }

  /**
   * Assert onMeasure() with given parameters will result in expected measured size.
   */
  private static void assertMeasuredSize(
      SurfaceViewRenderer surfaceViewRenderer, RendererCommon.ScalingType scalingType,
      String frameDimensions,
      int expectedWidth, int expectedHeight,
      int widthSpec, int heightSpec) {
    surfaceViewRenderer.setScalingType(scalingType);
    surfaceViewRenderer.onMeasure(widthSpec, heightSpec);
    final int measuredWidth = surfaceViewRenderer.getMeasuredWidth();
    final int measuredHeight = surfaceViewRenderer.getMeasuredHeight();
    if (measuredWidth != expectedWidth || measuredHeight != expectedHeight) {
      fail("onMeasure("
          + MeasureSpec.toString(widthSpec) + ", " + MeasureSpec.toString(heightSpec) + ")"
          + " with scaling type " + scalingType
          + " and frame: " + frameDimensions
          + " expected measured size " + expectedWidth + "x" + expectedHeight
          + ", but was " + measuredWidth + "x" + measuredHeight);
    }
  }

  /**
   * Test how SurfaceViewRenderer.onMeasure() behaves when no frame has been delivered.
   */
  @MediumTest
  public void testNoFrame() {
    final SurfaceViewRenderer surfaceViewRenderer =
        new SurfaceViewRenderer(getInstrumentation().getContext());
    final String frameDimensions = "null";

    // Test behaviour before SurfaceViewRenderer.init() is called.
    for (RendererCommon.ScalingType scalingType : scalingTypes) {
      for (int measureSpecMode : measureSpecModes) {
        final int zeroMeasureSize = MeasureSpec.makeMeasureSpec(0, measureSpecMode);
        assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions,
            0, 0, zeroMeasureSize, zeroMeasureSize);
        assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions,
            1280, 720,
            MeasureSpec.makeMeasureSpec(1280, measureSpecMode),
            MeasureSpec.makeMeasureSpec(720, measureSpecMode));
      }
    }

   // Test behaviour after SurfaceViewRenderer.init() is called, but still no frame.
    surfaceViewRenderer.init(EGL10.EGL_NO_CONTEXT, null);
    for (RendererCommon.ScalingType scalingType : scalingTypes) {
      for (int measureSpecMode : measureSpecModes) {
        final int zeroMeasureSize = MeasureSpec.makeMeasureSpec(0, measureSpecMode);
        assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions,
            0, 0, zeroMeasureSize, zeroMeasureSize);
        assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions,
            1280, 720,
            MeasureSpec.makeMeasureSpec(1280, measureSpecMode),
            MeasureSpec.makeMeasureSpec(720, measureSpecMode));
      }
    }

    surfaceViewRenderer.release();
  }

  /**
   * Test how SurfaceViewRenderer.onMeasure() behaves with a 1280x720 frame.
   */
  @MediumTest
  public void testFrame1280x720() {
    final SurfaceViewRenderer surfaceViewRenderer =
        new SurfaceViewRenderer(getInstrumentation().getContext());
    surfaceViewRenderer.init(EGL10.EGL_NO_CONTEXT, null);

    // Test different rotation degress, but same rotated size.
    for (int rotationDegree : new int[] {0, 90, 180, 270}) {
      final int rotatedWidth = 1280;
      final int rotatedHeight = 720;
      final int unrotatedWidth = (rotationDegree % 180 == 0 ? rotatedWidth : rotatedHeight);
      final int unrotatedHeight = (rotationDegree % 180 == 0 ? rotatedHeight : rotatedWidth);
      final VideoRenderer.I420Frame frame =
          createFrame(unrotatedWidth, unrotatedHeight, rotationDegree);
      assertEquals(rotatedWidth, frame.rotatedWidth());
      assertEquals(rotatedHeight, frame.rotatedHeight());
      final String frameDimensions =
          unrotatedWidth + "x" + unrotatedHeight + " with rotation " + rotationDegree;
      surfaceViewRenderer.renderFrame(frame);

      // Test forcing to zero size.
      for (RendererCommon.ScalingType scalingType : scalingTypes) {
        for (int measureSpecMode : measureSpecModes) {
          final int zeroMeasureSize = MeasureSpec.makeMeasureSpec(0, measureSpecMode);
          assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions,
              0, 0, zeroMeasureSize, zeroMeasureSize);
        }
      }

      // Test perfect fit.
      for (RendererCommon.ScalingType scalingType : scalingTypes) {
        for (int measureSpecMode : measureSpecModes) {
          assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions,
              rotatedWidth, rotatedHeight,
              MeasureSpec.makeMeasureSpec(rotatedWidth, measureSpecMode),
              MeasureSpec.makeMeasureSpec(rotatedHeight, measureSpecMode));
        }
      }

      // Force spec size with different aspect ratio than frame aspect ratio.
      for (RendererCommon.ScalingType scalingType : scalingTypes) {
        assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions,
            720, 1280,
            MeasureSpec.makeMeasureSpec(720, MeasureSpec.EXACTLY),
            MeasureSpec.makeMeasureSpec(1280, MeasureSpec.EXACTLY));
      }

      final float videoAspectRatio = (float) rotatedWidth / rotatedHeight;
      {
        // Relax both width and height constraints.
        final int widthSpec = MeasureSpec.makeMeasureSpec(720, MeasureSpec.AT_MOST);
        final int heightSpec = MeasureSpec.makeMeasureSpec(1280, MeasureSpec.AT_MOST);
        for (RendererCommon.ScalingType scalingType : scalingTypes) {
          final Point expectedSize =
              RendererCommon.getDisplaySize(scalingType, videoAspectRatio, 720, 1280);
          assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions,
                  expectedSize.x, expectedSize.y, widthSpec, heightSpec);
        }
      }
      {
        // Force width to 720, but relax height constraint. This will give the same result as
        // above, because width is already the limiting factor and will be maxed out.
        final int widthSpec = MeasureSpec.makeMeasureSpec(720, MeasureSpec.EXACTLY);
        final int heightSpec = MeasureSpec.makeMeasureSpec(1280, MeasureSpec.AT_MOST);
        for (RendererCommon.ScalingType scalingType : scalingTypes) {
          final Point expectedSize =
              RendererCommon.getDisplaySize(scalingType, videoAspectRatio, 720, 1280);
          assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions,
                  expectedSize.x, expectedSize.y, widthSpec, heightSpec);
        }
      }
      {
        // Force height, but relax width constraint. This will force a bad layout size.
        final int widthSpec = MeasureSpec.makeMeasureSpec(720, MeasureSpec.AT_MOST);
        final int heightSpec = MeasureSpec.makeMeasureSpec(1280, MeasureSpec.EXACTLY);
        for (RendererCommon.ScalingType scalingType : scalingTypes) {
          assertMeasuredSize(surfaceViewRenderer, scalingType, frameDimensions,
                  720, 1280, widthSpec, heightSpec);
        }
      }
    }

    surfaceViewRenderer.release();
  }
}
