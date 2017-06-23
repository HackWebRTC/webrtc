/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;

import android.annotation.TargetApi;
import android.graphics.Matrix;
import android.support.test.filters.MediumTest;
import android.util.Log;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.CountDownLatch;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Unit tests for {@link HardwareVideoDecoder}. */
@TargetApi(16)
@RunWith(BaseJUnit4ClassRunner.class)
public final class HardwareVideoDecoderTest {
  private static final String TAG = "HardwareVideoDecoderTest";

  private static final boolean ENABLE_INTEL_VP8_ENCODER = true;
  private static final boolean ENABLE_H264_HIGH_PROFILE = true;
  private static final VideoDecoder.Settings SETTINGS =
      new VideoDecoder.Settings(1 /* core */, 640 /* width */, 480 /* height */);

  @Test
  @MediumTest
  public void testInitialize() {
    HardwareVideoEncoderFactory encoderFactory =
        new HardwareVideoEncoderFactory(ENABLE_INTEL_VP8_ENCODER, ENABLE_H264_HIGH_PROFILE);
    VideoCodecInfo[] supportedCodecs = encoderFactory.getSupportedCodecs();
    if (supportedCodecs.length == 0) {
      Log.i(TAG, "No hardware encoding support, skipping testInitialize");
      return;
    }

    HardwareVideoDecoderFactory decoderFactory = new HardwareVideoDecoderFactory();

    VideoDecoder decoder = decoderFactory.createDecoder(supportedCodecs[0].name);
    assertEquals(decoder.initDecode(SETTINGS, null), VideoCodecStatus.OK);
    assertEquals(decoder.release(), VideoCodecStatus.OK);
  }

  @Test
  @MediumTest
  public void testDecode() throws InterruptedException {
    HardwareVideoEncoderFactory encoderFactory =
        new HardwareVideoEncoderFactory(ENABLE_INTEL_VP8_ENCODER, ENABLE_H264_HIGH_PROFILE);
    VideoCodecInfo[] supportedCodecs = encoderFactory.getSupportedCodecs();
    if (supportedCodecs.length == 0) {
      Log.i(TAG, "No hardware encoding support, skipping testEncodeYuvBuffer");
      return;
    }

    // Set up the decoder.
    HardwareVideoDecoderFactory decoderFactory = new HardwareVideoDecoderFactory();
    VideoDecoder decoder = decoderFactory.createDecoder(supportedCodecs[0].name);

    final long presentationTimestampUs = 20000;
    final int rotation = 270;

    final CountDownLatch decodeDone = new CountDownLatch(1);
    final AtomicReference<VideoFrame> decoded = new AtomicReference<>();
    VideoDecoder.Callback decodeCallback = new VideoDecoder.Callback() {
      @Override
      public void onDecodedFrame(VideoFrame frame, Integer decodeTimeMs, Integer qp) {
        decoded.set(frame);
        decodeDone.countDown();
      }
    };
    assertEquals(decoder.initDecode(SETTINGS, decodeCallback), VideoCodecStatus.OK);

    // Set up an encoder to produce a valid encoded frame.
    VideoEncoder encoder = encoderFactory.createEncoder(supportedCodecs[0]);
    final CountDownLatch encodeDone = new CountDownLatch(1);
    final AtomicReference<EncodedImage> encoded = new AtomicReference<>();
    VideoEncoder.Callback encodeCallback = new VideoEncoder.Callback() {
      @Override
      public void onEncodedFrame(EncodedImage image, VideoEncoder.CodecSpecificInfo info) {
        encoded.set(image);
        encodeDone.countDown();
      }
    };
    assertEquals(
        encoder.initEncode(
            new VideoEncoder.Settings(1, SETTINGS.width, SETTINGS.height, 300, 30), encodeCallback),
        VideoCodecStatus.OK);

    // First, encode a frame.
    VideoFrame.I420Buffer buffer = new I420BufferImpl(SETTINGS.width, SETTINGS.height);
    VideoFrame frame =
        new VideoFrame(buffer, rotation, presentationTimestampUs * 1000, new Matrix());
    VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
        new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameKey});

    assertEquals(encoder.encode(frame, info), VideoCodecStatus.OK);

    ThreadUtils.awaitUninterruptibly(encodeDone);

    // Now decode the frame.
    assertEquals(
        decoder.decode(encoded.get(), new VideoDecoder.DecodeInfo(false, 0)), VideoCodecStatus.OK);

    ThreadUtils.awaitUninterruptibly(decodeDone);

    frame = decoded.get();
    assertEquals(frame.getRotation(), rotation);
    assertEquals(frame.getTimestampNs(), presentationTimestampUs * 1000);
    assertEquals(frame.getTransformMatrix(), new Matrix());
    assertEquals(frame.getWidth(), SETTINGS.width);
    assertEquals(frame.getHeight(), SETTINGS.height);

    assertEquals(decoder.release(), VideoCodecStatus.OK);
    assertEquals(encoder.release(), VideoCodecStatus.OK);
  }
}
