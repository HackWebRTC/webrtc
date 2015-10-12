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

import android.test.ActivityTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;

import org.webrtc.MediaCodecVideoEncoder.OutputBufferInfo;

public final class MediaCodecVideoEncoderTest extends ActivityTestCase {
  final static String TAG = "MediaCodecVideoEncoderTest";

  @SmallTest
  public static void testInitReleaseUsingByteBuffer() {
    if (!MediaCodecVideoEncoder.isVp8HwSupported()) {
      Log.i(TAG,
            "Hardware does not support VP8 encoding, skipping testInitReleaseUsingByteBuffer");
      return;
    }
    MediaCodecVideoEncoder encoder = new MediaCodecVideoEncoder();
    assertTrue(encoder.initEncode(
        MediaCodecVideoEncoder.VideoCodecType.VIDEO_CODEC_VP8, 640, 480, 300, 30));
    encoder.release();
  }

  @SmallTest
  public static void testEncoderUsingByteBuffer() throws InterruptedException {
    if (!MediaCodecVideoEncoder.isVp8HwSupported()) {
      Log.i(TAG, "Hardware does not support VP8 encoding, skipping testEncoderUsingByteBuffer");
      return;
    }

    final int width = 640;
    final int height = 480;
    final int min_size = width * height * 3 / 2;
    final long presentationTimestampUs = 2;

    MediaCodecVideoEncoder encoder = new MediaCodecVideoEncoder();

    assertTrue(encoder.initEncode(
        MediaCodecVideoEncoder.VideoCodecType.VIDEO_CODEC_VP8, width, height, 300, 30));
    ByteBuffer[] inputBuffers = encoder.getInputBuffers();
    assertNotNull(inputBuffers);
    assertTrue(min_size <= inputBuffers[0].capacity());

    int bufferIndex;
    do {
      Thread.sleep(10);
      bufferIndex = encoder.dequeueInputBuffer();
    } while (bufferIndex == -1); // |-1| is returned when there is no buffer available yet.

    assertTrue(bufferIndex >= 0);
    assertTrue(bufferIndex < inputBuffers.length);
    assertTrue(encoder.encodeBuffer(true, bufferIndex, min_size, presentationTimestampUs));

    OutputBufferInfo info;
    do {
      info = encoder.dequeueOutputBuffer();
      Thread.sleep(10);
    } while (info == null);
    assertTrue(info.index >= 0);
    assertEquals(presentationTimestampUs, info.presentationTimestampUs);
    assertTrue(info.buffer.capacity() > 0);
    encoder.releaseOutputBuffer(info.index);

    encoder.release();
  }
}
