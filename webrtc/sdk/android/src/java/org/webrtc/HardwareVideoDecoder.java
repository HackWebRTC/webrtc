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

import android.annotation.TargetApi;
import android.media.MediaCodec;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaFormat;
import android.os.SystemClock;
import android.view.Surface;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Deque;
import java.util.concurrent.LinkedBlockingDeque;
import org.webrtc.ThreadUtils.ThreadChecker;

/** Android hardware video decoder. */
@TargetApi(16)
@SuppressWarnings("deprecation") // Cannot support API 16 without using deprecated methods.
class HardwareVideoDecoder
    implements VideoDecoder, SurfaceTextureHelper.OnTextureFrameAvailableListener {
  private static final String TAG = "HardwareVideoDecoder";

  // TODO(magjed): Use MediaFormat.KEY_* constants when part of the public API.
  private static final String MEDIA_FORMAT_KEY_STRIDE = "stride";
  private static final String MEDIA_FORMAT_KEY_SLICE_HEIGHT = "slice-height";
  private static final String MEDIA_FORMAT_KEY_CROP_LEFT = "crop-left";
  private static final String MEDIA_FORMAT_KEY_CROP_RIGHT = "crop-right";
  private static final String MEDIA_FORMAT_KEY_CROP_TOP = "crop-top";
  private static final String MEDIA_FORMAT_KEY_CROP_BOTTOM = "crop-bottom";

  // MediaCodec.release() occasionally hangs.  Release stops waiting and reports failure after
  // this timeout.
  private static final int MEDIA_CODEC_RELEASE_TIMEOUT_MS = 5000;

  // WebRTC queues input frames quickly in the beginning on the call. Wait for input buffers with a
  // long timeout (500 ms) to prevent this from causing the codec to return an error.
  private static final int DEQUEUE_INPUT_TIMEOUT_US = 500000;

  // Dequeuing an output buffer will block until a buffer is available (up to 100 milliseconds).
  // If this timeout is exceeded, the output thread will unblock and check if the decoder is still
  // running.  If it is, it will block on dequeue again.  Otherwise, it will stop and release the
  // MediaCodec.
  private static final int DEQUEUE_OUTPUT_BUFFER_TIMEOUT_US = 100000;

  private final String codecName;
  private final VideoCodecType codecType;

  private static class FrameInfo {
    final long decodeStartTimeMs;
    final int rotation;

    FrameInfo(long decodeStartTimeMs, int rotation) {
      this.decodeStartTimeMs = decodeStartTimeMs;
      this.rotation = rotation;
    }
  }

  private final Deque<FrameInfo> frameInfos;
  private int colorFormat;

  // Output thread runs a loop which polls MediaCodec for decoded output buffers.  It reformats
  // those buffers into VideoFrames and delivers them to the callback.
  private Thread outputThread;

  // Checker that ensures work is run on the output thread.
  private ThreadChecker outputThreadChecker;

  // Checker that ensures work is run on the decoder thread.  The decoder thread is owned by the
  // caller and must be used to call initDecode, decode, and release.
  private ThreadChecker decoderThreadChecker;

  private volatile boolean running = false;
  private volatile Exception shutdownException = null;

  // Prevents the decoder from being released before all output buffers have been released.
  private final Object activeOutputBuffersLock = new Object();
  private int activeOutputBuffers = 0; // Guarded by activeOutputBuffersLock

  // Dimensions (width, height, stride, and sliceHeight) may be accessed by either the decode thread
  // or the output thread.  Accesses should be protected with this lock.
  private final Object dimensionLock = new Object();
  private int width;
  private int height;
  private int stride;
  private int sliceHeight;

  // Whether the decoder has finished the first frame.  The codec may not change output dimensions
  // after delivering the first frame.
  private boolean hasDecodedFirstFrame;
  // Whether the decoder has seen a key frame.  The first frame must be a key frame.
  private boolean keyFrameRequired;

  private final EglBase.Context sharedContext;
  private SurfaceTextureHelper surfaceTextureHelper;
  private Surface surface = null;

  private static class DecodedTextureMetadata {
    final int width;
    final int height;
    final int rotation;
    final long presentationTimestampUs;
    final Integer decodeTimeMs;

    DecodedTextureMetadata(
        int width, int height, int rotation, long presentationTimestampUs, Integer decodeTimeMs) {
      this.width = width;
      this.height = height;
      this.rotation = rotation;
      this.presentationTimestampUs = presentationTimestampUs;
      this.decodeTimeMs = decodeTimeMs;
    }
  }

  // Metadata for the last frame rendered to the texture.  Only accessed on the texture helper's
  // thread.
  private DecodedTextureMetadata renderedTextureMetadata;

  // Decoding proceeds asynchronously.  This callback returns decoded frames to the caller.
  private Callback callback;

  private MediaCodec codec = null;

  HardwareVideoDecoder(
      String codecName, VideoCodecType codecType, int colorFormat, EglBase.Context sharedContext) {
    if (!isSupportedColorFormat(colorFormat)) {
      throw new IllegalArgumentException("Unsupported color format: " + colorFormat);
    }
    this.codecName = codecName;
    this.codecType = codecType;
    this.colorFormat = colorFormat;
    this.sharedContext = sharedContext;
    this.frameInfos = new LinkedBlockingDeque<>();
  }

  @Override
  public VideoCodecStatus initDecode(Settings settings, Callback callback) {
    this.decoderThreadChecker = new ThreadChecker();
    return initDecodeInternal(settings.width, settings.height, callback);
  }

  private VideoCodecStatus initDecodeInternal(int width, int height, Callback callback) {
    decoderThreadChecker.checkIsOnValidThread();
    if (outputThread != null) {
      Logging.e(TAG, "initDecodeInternal called while the codec is already running");
      return VideoCodecStatus.ERROR;
    }

    // Note:  it is not necessary to initialize dimensions under the lock, since the output thread
    // is not running.
    this.callback = callback;
    this.width = width;
    this.height = height;

    stride = width;
    sliceHeight = height;
    hasDecodedFirstFrame = false;
    keyFrameRequired = true;

    try {
      codec = MediaCodec.createByCodecName(codecName);
    } catch (IOException | IllegalArgumentException e) {
      Logging.e(TAG, "Cannot create media decoder " + codecName);
      return VideoCodecStatus.ERROR;
    }
    try {
      MediaFormat format = MediaFormat.createVideoFormat(codecType.mimeType(), width, height);
      if (sharedContext == null) {
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, colorFormat);
      } else {
        surfaceTextureHelper = SurfaceTextureHelper.create("decoder-texture-thread", sharedContext);
        surface = new Surface(surfaceTextureHelper.getSurfaceTexture());
        surfaceTextureHelper.startListening(this);
      }
      codec.configure(format, surface, null, 0);
      codec.start();
    } catch (IllegalStateException e) {
      Logging.e(TAG, "initDecode failed", e);
      release();
      return VideoCodecStatus.ERROR;
    }

    running = true;
    outputThread = createOutputThread();
    outputThread.start();

    return VideoCodecStatus.OK;
  }

  @Override
  public VideoCodecStatus decode(EncodedImage frame, DecodeInfo info) {
    decoderThreadChecker.checkIsOnValidThread();
    if (codec == null || callback == null) {
      return VideoCodecStatus.UNINITIALIZED;
    }

    if (frame.buffer == null) {
      Logging.e(TAG, "decode() - no input data");
      return VideoCodecStatus.ERR_PARAMETER;
    }

    int size = frame.buffer.remaining();
    if (size == 0) {
      Logging.e(TAG, "decode() - input buffer empty");
      return VideoCodecStatus.ERR_PARAMETER;
    }

    // Load dimensions from shared memory under the dimension lock.
    int width, height;
    synchronized (dimensionLock) {
      width = this.width;
      height = this.height;
    }

    // Check if the resolution changed and reset the codec if necessary.
    if (frame.encodedWidth * frame.encodedHeight > 0
        && (frame.encodedWidth != width || frame.encodedHeight != height)) {
      VideoCodecStatus status = reinitDecode(frame.encodedWidth, frame.encodedHeight);
      if (status != VideoCodecStatus.OK) {
        return status;
      }
    }

    if (keyFrameRequired) {
      // Need to process a key frame first.
      if (frame.frameType != EncodedImage.FrameType.VideoFrameKey) {
        Logging.e(TAG, "decode() - key frame required first");
        return VideoCodecStatus.ERROR;
      }
      if (!frame.completeFrame) {
        Logging.e(TAG, "decode() - complete frame required first");
        return VideoCodecStatus.ERROR;
      }
    }

    int index;
    try {
      index = codec.dequeueInputBuffer(DEQUEUE_INPUT_TIMEOUT_US);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "dequeueInputBuffer failed", e);
      return VideoCodecStatus.ERROR;
    }
    if (index < 0) {
      // Decoder is falling behind.  No input buffers available.
      // The decoder can't simply drop frames; it might lose a key frame.
      Logging.e(TAG, "decode() - no HW buffers available; decoder falling behind");
      return VideoCodecStatus.ERROR;
    }

    ByteBuffer buffer;
    try {
      buffer = codec.getInputBuffers()[index];
    } catch (IllegalStateException e) {
      Logging.e(TAG, "getInputBuffers failed", e);
      return VideoCodecStatus.ERROR;
    }

    if (buffer.capacity() < size) {
      Logging.e(TAG, "decode() - HW buffer too small");
      return VideoCodecStatus.ERROR;
    }
    buffer.put(frame.buffer);

    frameInfos.offer(new FrameInfo(SystemClock.elapsedRealtime(), frame.rotation));
    try {
      codec.queueInputBuffer(
          index, 0 /* offset */, size, frame.captureTimeMs * 1000, 0 /* flags */);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "queueInputBuffer failed", e);
      frameInfos.pollLast();
      return VideoCodecStatus.ERROR;
    }
    if (keyFrameRequired) {
      keyFrameRequired = false;
    }
    return VideoCodecStatus.OK;
  }

  @Override
  public boolean getPrefersLateDecoding() {
    return true;
  }

  @Override
  public String getImplementationName() {
    return "HardwareVideoDecoder: " + codecName;
  }

  @Override
  public VideoCodecStatus release() {
    // TODO(sakal): This is not called on the correct thread but is still called synchronously.
    // Re-enable the check once this is called on the correct thread.
    // decoderThreadChecker.checkIsOnValidThread();
    if (!running) {
      Logging.d(TAG, "release: Decoder is not running.");
      return VideoCodecStatus.OK;
    }
    try {
      // The outputThread actually stops and releases the codec once running is false.
      running = false;
      if (!ThreadUtils.joinUninterruptibly(outputThread, MEDIA_CODEC_RELEASE_TIMEOUT_MS)) {
        // Log an exception to capture the stack trace and turn it into a TIMEOUT error.
        Logging.e(TAG, "Media decoder release timeout", new RuntimeException());
        return VideoCodecStatus.TIMEOUT;
      }
      if (shutdownException != null) {
        // Log the exception and turn it into an error.  Wrap the exception in a new exception to
        // capture both the output thread's stack trace and this thread's stack trace.
        Logging.e(TAG, "Media decoder release error", new RuntimeException(shutdownException));
        shutdownException = null;
        return VideoCodecStatus.ERROR;
      }
    } finally {
      codec = null;
      callback = null;
      outputThread = null;
      frameInfos.clear();
      if (surface != null) {
        surface.release();
        surface = null;
        surfaceTextureHelper.stopListening();
        surfaceTextureHelper.dispose();
        surfaceTextureHelper = null;
      }
    }
    return VideoCodecStatus.OK;
  }

  private VideoCodecStatus reinitDecode(int newWidth, int newHeight) {
    decoderThreadChecker.checkIsOnValidThread();
    VideoCodecStatus status = release();
    if (status != VideoCodecStatus.OK) {
      return status;
    }
    return initDecodeInternal(newWidth, newHeight, callback);
  }

  private Thread createOutputThread() {
    return new Thread("HardwareVideoDecoder.outputThread") {
      @Override
      public void run() {
        outputThreadChecker = new ThreadChecker();
        while (running) {
          deliverDecodedFrame();
        }
        releaseCodecOnOutputThread();
      }
    };
  }

  private void deliverDecodedFrame() {
    outputThreadChecker.checkIsOnValidThread();
    try {
      MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
      // Block until an output buffer is available (up to 100 milliseconds).  If the timeout is
      // exceeded, deliverDecodedFrame() will be called again on the next iteration of the output
      // thread's loop.  Blocking here prevents the output thread from busy-waiting while the codec
      // is idle.
      int result = codec.dequeueOutputBuffer(info, DEQUEUE_OUTPUT_BUFFER_TIMEOUT_US);
      if (result == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
        reformat(codec.getOutputFormat());
        return;
      }

      if (result < 0) {
        Logging.v(TAG, "dequeueOutputBuffer returned " + result);
        return;
      }

      FrameInfo frameInfo = frameInfos.poll();
      Integer decodeTimeMs = null;
      int rotation = 0;
      if (frameInfo != null) {
        decodeTimeMs = (int) (SystemClock.elapsedRealtime() - frameInfo.decodeStartTimeMs);
        rotation = frameInfo.rotation;
      }

      hasDecodedFirstFrame = true;

      if (surfaceTextureHelper != null) {
        deliverTextureFrame(result, info, rotation, decodeTimeMs);
      } else {
        deliverByteFrame(result, info, rotation, decodeTimeMs);
      }

    } catch (IllegalStateException e) {
      Logging.e(TAG, "deliverDecodedFrame failed", e);
    }
  }

  private void deliverTextureFrame(final int index, final MediaCodec.BufferInfo info,
      final int rotation, final Integer decodeTimeMs) {
    // Load dimensions from shared memory under the dimension lock.
    final int width, height;
    synchronized (dimensionLock) {
      width = this.width;
      height = this.height;
    }

    surfaceTextureHelper.getHandler().post(new Runnable() {
      @Override
      public void run() {
        renderedTextureMetadata = new DecodedTextureMetadata(
            width, height, rotation, info.presentationTimeUs, decodeTimeMs);
        codec.releaseOutputBuffer(index, true);
      }
    });
  }

  @Override
  public void onTextureFrameAvailable(int oesTextureId, float[] transformMatrix, long timestampNs) {
    VideoFrame.TextureBuffer oesBuffer = surfaceTextureHelper.createTextureBuffer(
        renderedTextureMetadata.width, renderedTextureMetadata.height,
        RendererCommon.convertMatrixToAndroidGraphicsMatrix(transformMatrix));

    VideoFrame frame = new VideoFrame(oesBuffer, renderedTextureMetadata.rotation,
        renderedTextureMetadata.presentationTimestampUs * 1000);
    callback.onDecodedFrame(frame, renderedTextureMetadata.decodeTimeMs, null /* qp */);
    frame.release();
  }

  private void deliverByteFrame(
      int result, MediaCodec.BufferInfo info, int rotation, Integer decodeTimeMs) {
    // Load dimensions from shared memory under the dimension lock.
    int width, height, stride, sliceHeight;
    synchronized (dimensionLock) {
      width = this.width;
      height = this.height;
      stride = this.stride;
      sliceHeight = this.sliceHeight;
    }

    // Output must be at least width * height bytes for Y channel, plus (width / 2) * (height / 2)
    // bytes for each of the U and V channels.
    if (info.size < width * height * 3 / 2) {
      Logging.e(TAG, "Insufficient output buffer size: " + info.size);
      return;
    }

    if (info.size < stride * height * 3 / 2 && sliceHeight == height && stride > width) {
      // Some codecs (Exynos) report an incorrect stride.  Correct it here.
      // Expected size == stride * height * 3 / 2.  A bit of algebra gives the correct stride as
      // 2 * size / (3 * height).
      stride = info.size * 2 / (height * 3);
    }

    ByteBuffer buffer = codec.getOutputBuffers()[result];
    buffer.position(info.offset);
    buffer.limit(info.offset + info.size);
    buffer = buffer.slice();
    final VideoFrame.Buffer frameBuffer;

    if (colorFormat == CodecCapabilities.COLOR_FormatYUV420Planar) {
      if (sliceHeight % 2 == 0) {
        frameBuffer = wrapI420Buffer(buffer, result, stride, sliceHeight, width, height);
      } else {
        // WebRTC rounds chroma plane size conversions up so we have to repeat the last row.
        frameBuffer = copyI420Buffer(buffer, result, stride, sliceHeight, width, height);
      }
    } else {
      // All other supported color formats are NV12.
      frameBuffer = wrapNV12Buffer(buffer, result, stride, sliceHeight, width, height);
    }

    long presentationTimeNs = info.presentationTimeUs * 1000;
    VideoFrame frame = new VideoFrame(frameBuffer, rotation, presentationTimeNs);

    // Note that qp is parsed on the C++ side.
    callback.onDecodedFrame(frame, decodeTimeMs, null /* qp */);
    frame.release();
  }

  private VideoFrame.Buffer wrapNV12Buffer(ByteBuffer buffer, int outputBufferIndex, int stride,
      int sliceHeight, int width, int height) {
    synchronized (activeOutputBuffersLock) {
      activeOutputBuffers++;
    }

    return new NV12Buffer(width, height, stride, sliceHeight, buffer, () -> {
      codec.releaseOutputBuffer(outputBufferIndex, false);
      synchronized (activeOutputBuffersLock) {
        activeOutputBuffers--;
        activeOutputBuffersLock.notifyAll();
      }
    });
  }

  private VideoFrame.Buffer copyI420Buffer(ByteBuffer buffer, int outputBufferIndex, int stride,
      int sliceHeight, int width, int height) {
    final int uvStride = stride / 2;

    final int yPos = 0;
    final int uPos = yPos + stride * sliceHeight;
    final int uEnd = uPos + uvStride * (sliceHeight / 2);
    final int vPos = uPos + uvStride * sliceHeight / 2;
    final int vEnd = vPos + uvStride * (sliceHeight / 2);

    VideoFrame.I420Buffer frameBuffer = I420BufferImpl.allocate(width, height);

    ByteBuffer dataY = frameBuffer.getDataY();
    dataY.position(0); // Ensure we are in the beginning.
    buffer.position(yPos);
    buffer.limit(uPos);
    dataY.put(buffer);
    dataY.position(0); // Go back to beginning.

    ByteBuffer dataU = frameBuffer.getDataU();
    dataU.position(0); // Ensure we are in the beginning.
    buffer.position(uPos);
    buffer.limit(uEnd);
    dataU.put(buffer);
    if (sliceHeight % 2 != 0) {
      buffer.position(uEnd - uvStride); // Repeat the last row.
      dataU.put(buffer);
    }
    dataU.position(0); // Go back to beginning.

    ByteBuffer dataV = frameBuffer.getDataU();
    dataV.position(0); // Ensure we are in the beginning.
    buffer.position(vPos);
    buffer.limit(vEnd);
    dataV.put(buffer);
    if (sliceHeight % 2 != 0) {
      buffer.position(vEnd - uvStride); // Repeat the last row.
      dataV.put(buffer);
    }
    dataV.position(0); // Go back to beginning.

    codec.releaseOutputBuffer(outputBufferIndex, false);

    return frameBuffer;
  }

  private VideoFrame.Buffer wrapI420Buffer(ByteBuffer buffer, int outputBufferIndex, int stride,
      int sliceHeight, int width, int height) {
    final int uvStride = stride / 2;

    final int yPos = 0;
    final int uPos = yPos + stride * sliceHeight;
    final int uEnd = uPos + uvStride * (sliceHeight / 2);
    final int vPos = uPos + uvStride * sliceHeight / 2;
    final int vEnd = vPos + uvStride * (sliceHeight / 2);

    synchronized (activeOutputBuffersLock) {
      activeOutputBuffers++;
    }

    Runnable releaseCallback = () -> {
      codec.releaseOutputBuffer(outputBufferIndex, false);
      synchronized (activeOutputBuffersLock) {
        activeOutputBuffers--;
        activeOutputBuffersLock.notifyAll();
      }
    };

    buffer.position(yPos);
    buffer.limit(uPos);
    ByteBuffer dataY = buffer.slice();

    buffer.position(uPos);
    buffer.limit(uEnd);
    ByteBuffer dataU = buffer.slice();

    buffer.position(vPos);
    buffer.limit(vEnd);
    ByteBuffer dataV = buffer.slice();

    return new I420BufferImpl(
        width, height, dataY, stride, dataU, uvStride, dataV, uvStride, releaseCallback);
  }

  private void reformat(MediaFormat format) {
    outputThreadChecker.checkIsOnValidThread();
    Logging.d(TAG, "Decoder format changed: " + format.toString());
    final int newWidth;
    final int newHeight;
    if (format.containsKey(MEDIA_FORMAT_KEY_CROP_LEFT)
        && format.containsKey(MEDIA_FORMAT_KEY_CROP_RIGHT)
        && format.containsKey(MEDIA_FORMAT_KEY_CROP_BOTTOM)
        && format.containsKey(MEDIA_FORMAT_KEY_CROP_TOP)) {
      newWidth = 1 + format.getInteger(MEDIA_FORMAT_KEY_CROP_RIGHT)
          - format.getInteger(MEDIA_FORMAT_KEY_CROP_LEFT);
      newHeight = 1 + format.getInteger(MEDIA_FORMAT_KEY_CROP_BOTTOM)
          - format.getInteger(MEDIA_FORMAT_KEY_CROP_TOP);
    } else {
      newWidth = format.getInteger(MediaFormat.KEY_WIDTH);
      newHeight = format.getInteger(MediaFormat.KEY_HEIGHT);
    }
    // Compare to existing width, height, and save values under the dimension lock.
    synchronized (dimensionLock) {
      if (hasDecodedFirstFrame && (width != newWidth || height != newHeight)) {
        stopOnOutputThread(new RuntimeException("Unexpected size change. Configured " + width + "*"
            + height + ". New " + newWidth + "*" + newHeight));
        return;
      }
      width = newWidth;
      height = newHeight;
    }

    // Note:  texture mode ignores colorFormat.  Hence, if the texture helper is non-null, skip
    // color format updates.
    if (surfaceTextureHelper == null && format.containsKey(MediaFormat.KEY_COLOR_FORMAT)) {
      colorFormat = format.getInteger(MediaFormat.KEY_COLOR_FORMAT);
      Logging.d(TAG, "Color: 0x" + Integer.toHexString(colorFormat));
      if (!isSupportedColorFormat(colorFormat)) {
        stopOnOutputThread(new IllegalStateException("Unsupported color format: " + colorFormat));
        return;
      }
    }

    // Save stride and sliceHeight under the dimension lock.
    synchronized (dimensionLock) {
      if (format.containsKey(MEDIA_FORMAT_KEY_STRIDE)) {
        stride = format.getInteger(MEDIA_FORMAT_KEY_STRIDE);
      }
      if (format.containsKey(MEDIA_FORMAT_KEY_SLICE_HEIGHT)) {
        sliceHeight = format.getInteger(MEDIA_FORMAT_KEY_SLICE_HEIGHT);
      }
      Logging.d(TAG, "Frame stride and slice height: " + stride + " x " + sliceHeight);
      stride = Math.max(width, stride);
      sliceHeight = Math.max(height, sliceHeight);
    }
  }

  private void releaseCodecOnOutputThread() {
    outputThreadChecker.checkIsOnValidThread();
    Logging.d(TAG, "Releasing MediaCodec on output thread");
    waitOutputBuffersReleasedOnOutputThread();
    try {
      codec.stop();
    } catch (Exception e) {
      Logging.e(TAG, "Media decoder stop failed", e);
    }
    try {
      codec.release();
    } catch (Exception e) {
      Logging.e(TAG, "Media decoder release failed", e);
      // Propagate exceptions caught during release back to the main thread.
      shutdownException = e;
    }
    codec = null;
    callback = null;
    outputThread = null;
    frameInfos.clear();
    Logging.d(TAG, "Release on output thread done");
  }

  private void waitOutputBuffersReleasedOnOutputThread() {
    outputThreadChecker.checkIsOnValidThread();
    synchronized (activeOutputBuffersLock) {
      while (activeOutputBuffers > 0) {
        Logging.d(TAG, "Waiting for all frames to be released.");
        try {
          activeOutputBuffersLock.wait();
        } catch (InterruptedException e) {
          Logging.e(TAG, "Interrupted while waiting for output buffers to be released.", e);
          return;
        }
      }
    }
  }

  private void stopOnOutputThread(Exception e) {
    outputThreadChecker.checkIsOnValidThread();
    running = false;
    shutdownException = e;
  }

  private boolean isSupportedColorFormat(int colorFormat) {
    for (int supported : MediaCodecUtils.DECODER_COLOR_FORMATS) {
      if (supported == colorFormat) {
        return true;
      }
    }
    return false;
  }
}
