package com.piasy.avconf.capturer;

import android.content.Context;
import java.nio.ByteBuffer;
import org.webrtc.CapturerObserver;
import org.webrtc.JavaI420Buffer;
import org.webrtc.NV12Buffer;
import org.webrtc.NV21Buffer;
import org.webrtc.SurfaceTextureHelper;
import org.webrtc.VideoCapturer;
import org.webrtc.VideoFrame;

/**
 * Created by Piasy{github.com/Piasy} on 2020/8/30.
 */
public class YuvCapturer implements VideoCapturer {

  public static final int FORMAT_NV12 = 1;
  public static final int FORMAT_NV21 = 2;
  public static final int FORMAT_I420 = 3;

  private CapturerObserver observer;

  @Override
  public synchronized void initialize(SurfaceTextureHelper surfaceTextureHelper,
      Context applicationContext, CapturerObserver capturerObserver) {
    observer = capturerObserver;
  }

  @Override public void startCapture(int width, int height, int framerate) {
    observer.onCapturerStarted(true);
  }

  @Override public synchronized void stopCapture() throws InterruptedException {
    observer.onCapturerStopped();
  }

  @Override public void changeCaptureFormat(int width, int height, int framerate) {
  }

  @Override synchronized public void dispose() {
    observer = null;
  }

  @Override public boolean isScreencast() {
    return false;
  }

  public synchronized void feedVideoFrame(int format, int width, int height, byte[] data,
      int bytes) {
    if (observer == null) {
      return;
    }
    VideoFrame.Buffer buffer;
    switch (format) {
      case FORMAT_NV12:
        buffer =
            new NV12Buffer(width, height, width, height, ByteBuffer.wrap(data, 0, bytes), null);
        break;
      case FORMAT_NV21:
        buffer = new NV21Buffer(data, width, height, null);
        break;
      case FORMAT_I420:
        JavaI420Buffer i420 = JavaI420Buffer.allocate(width, height);
        i420.getDataY().put(data, 0, width * height);
        i420.getDataU().put(data, width * height, width * height / 4);
        i420.getDataV().put(data, width * height + width * height / 4, width * height / 4);
        buffer = i420;
        break;
      default:
        return;
    }
    observer.onFrameCaptured(new VideoFrame(buffer, 0 /* rotation */,
        System.currentTimeMillis() * 1_000_000));
  }
}
