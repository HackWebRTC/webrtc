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

import android.test.ActivityTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import android.graphics.Point;

import static org.webrtc.RendererCommon.ScalingType.*;
import static org.webrtc.RendererCommon.getDisplaySize;

public class RendererCommonTest extends ActivityTestCase {
  @SmallTest
  static public void testDisplaySizeNoFrame() {
    assertEquals(getDisplaySize(SCALE_ASPECT_FIT, 0.0f, 0, 0), new Point(0, 0));
    assertEquals(getDisplaySize(SCALE_ASPECT_FILL, 0.0f, 0, 0), new Point(0, 0));
    assertEquals(getDisplaySize(SCALE_ASPECT_BALANCED, 0.0f, 0, 0), new Point(0, 0));
  }

  @SmallTest
  static public void testDisplaySizeDegenerateAspectRatio() {
    assertEquals(getDisplaySize(SCALE_ASPECT_FIT, 0.0f, 1280, 720), new Point(1280, 720));
    assertEquals(getDisplaySize(SCALE_ASPECT_FILL, 0.0f, 1280, 720), new Point(1280, 720));
    assertEquals(getDisplaySize(SCALE_ASPECT_BALANCED, 0.0f, 1280, 720), new Point(1280, 720));
  }

  @SmallTest
  static public void testZeroDisplaySize() {
    assertEquals(getDisplaySize(SCALE_ASPECT_FIT, 16.0f / 9, 0, 0), new Point(0, 0));
    assertEquals(getDisplaySize(SCALE_ASPECT_FILL, 16.0f / 9, 0, 0), new Point(0, 0));
    assertEquals(getDisplaySize(SCALE_ASPECT_BALANCED, 16.0f / 9, 0, 0), new Point(0, 0));
  }

  @SmallTest
  static public void testDisplaySizePerfectFit() {
    assertEquals(getDisplaySize(SCALE_ASPECT_FIT, 16.0f / 9, 1280, 720), new Point(1280, 720));
    assertEquals(getDisplaySize(SCALE_ASPECT_FILL, 16.0f / 9, 1280, 720), new Point(1280, 720));
    assertEquals(getDisplaySize(SCALE_ASPECT_BALANCED, 16.0f / 9, 1280, 720), new Point(1280, 720));
    assertEquals(getDisplaySize(SCALE_ASPECT_FIT, 9.0f / 16, 720, 1280), new Point(720, 1280));
    assertEquals(getDisplaySize(SCALE_ASPECT_FILL, 9.0f / 16, 720, 1280), new Point(720, 1280));
    assertEquals(getDisplaySize(SCALE_ASPECT_BALANCED, 9.0f / 16, 720, 1280), new Point(720, 1280));
  }

  @SmallTest
  static public void testLandscapeVideoInPortraitDisplay() {
    assertEquals(getDisplaySize(SCALE_ASPECT_FIT, 16.0f / 9, 720, 1280), new Point(720, 405));
    assertEquals(getDisplaySize(SCALE_ASPECT_FILL, 16.0f / 9, 720, 1280), new Point(720, 1280));
    assertEquals(getDisplaySize(SCALE_ASPECT_BALANCED, 16.0f / 9, 720, 1280), new Point(720, 720));
  }

  @SmallTest
  static public void testPortraitVideoInLandscapeDisplay() {
    assertEquals(getDisplaySize(SCALE_ASPECT_FIT, 9.0f / 16, 1280, 720), new Point(405, 720));
    assertEquals(getDisplaySize(SCALE_ASPECT_FILL, 9.0f / 16, 1280, 720), new Point(1280, 720));
    assertEquals(getDisplaySize(SCALE_ASPECT_BALANCED, 9.0f / 16, 1280, 720), new Point(720, 720));
  }

  @SmallTest
  static public void testFourToThreeVideoInSixteenToNineDisplay() {
    assertEquals(getDisplaySize(SCALE_ASPECT_FIT, 4.0f / 3, 1280, 720), new Point(960, 720));
    assertEquals(getDisplaySize(SCALE_ASPECT_FILL, 4.0f / 3, 1280, 720), new Point(1280, 720));
    assertEquals(getDisplaySize(SCALE_ASPECT_BALANCED, 4.0f / 3, 1280, 720), new Point(1280, 720));
  }
}
