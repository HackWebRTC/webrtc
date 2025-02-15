package com.piasy.avconf;

import android.graphics.ImageFormat;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.os.Handler;
import android.os.Looper;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import org.webrtc.CapturerObserver;
import org.webrtc.Logging;
import org.webrtc.TextureBufferImpl;
import org.webrtc.TimestampAligner;
import org.webrtc.VideoFrame;
import org.webrtc.YuvConverter;
import org.webrtc.YuvHelper;

/**
 * Created by Piasy{github.com/Piasy} on 2019-12-21.
 */
public class HijackCapturerObserver implements CapturerObserver {
    private static final String TAG = "HijackCapturerObserver";
    private static final long BLACK_FRAME_INTERVAL_MS = 100;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final CapturerObserver realObserver;
    private final VideoFrame.Buffer blackBuffer;

    private volatile TimestampAligner timestampAligner = new TimestampAligner();
    private boolean muted = false;
    private final Runnable blackFrameProducer = this::produceBlackFrame;

    private boolean paused = false;

    private boolean sendLastFrame = false;
    private VideoFrame.Buffer lastBuffer;
    private int lastFrameRotation;
    private File jpegPath;
    private JpegFrameCallback jpegFrameCallback;

    private YuvFrameCallback yuvFrameCallback;
    private ByteBuffer yuvDirectBuffer;
    private byte[] yuvFrameCallbackBuffer;
    private long yuvFrameCallbackIntervalNs;
    private long lastYuvFrameCallbackTimestampNs;

    public HijackCapturerObserver(CapturerObserver realObserver) {
        this.realObserver = realObserver;

        // actually this buffer won't be used, VideoBroadcaster::OnFrame will replace it with
        // a black frame buffer and pass modified frame to track.
        blackBuffer = new TextureBufferImpl(480, 640, VideoFrame.TextureBuffer.Type.RGB, 0,
                new Matrix(), mainHandler, new YuvConverter(), null);
    }

    private static void NV21toJPEG(byte[] nv21, int width, int height, int quality, File file)
            throws IOException {
        FileOutputStream out = new FileOutputStream(file);
        YuvImage yuv = new YuvImage(nv21, ImageFormat.NV21, width, height, null);
        yuv.compressToJpeg(new Rect(0, 0, width, height), quality, out);
        out.close();
    }

    /**
     * Convert I420 (YYYYYYYY:UU:VV) to NV21 (YYYYYYYYY:VUVU)
     * <p>
     * Credit: https://gist.github.com/pnemonic78/2d8411ed6b0bd6c71b651800bc97eb8e
     */
    private static byte[] I420toNV21(final byte[] input, final int width, final int height) {
        byte[] output = new byte[input.length];
        final int size = width * height;
        final int quarter = size / 4;
        final int v0 = size + quarter;

        System.arraycopy(input, 0, output, 0, size); // Y is same

        for (int u = size, v = v0, o = size; u < v0; u++, v++, o += 2) {
            output[o] = input[v]; // For NV21, V first
            output[o + 1] = input[u]; // For NV21, U second
        }

        return output;
    }

    public void toggleMute(boolean muted) {
        Logging.d(TAG, "toggleMute " + muted);
        this.muted = muted;
        if (muted) {
            // frame interval of 25 fps
            mainHandler.postDelayed(blackFrameProducer, 40);
        } else {
            mainHandler.removeCallbacks(blackFrameProducer);
        }
    }

    public void togglePause(boolean pause) {
        Logging.d(TAG, "togglePause " + pause);
        paused = pause;
    }

    public void setYuvFrameCallback(long intervalMS, YuvFrameCallback callback) {
        Logging.d(TAG, "setYuvFrameCallback intervalMS " + intervalMS + ", callback " + callback);
        yuvFrameCallbackIntervalNs = intervalMS * 1000_000;
        yuvFrameCallback = callback;
    }

    public void toggleSendLastFrame(File jpegPath, JpegFrameCallback callback) {
        Logging.d(TAG, "toggleSendLastFrame jpegPath " + jpegPath + ", callback " + callback);
        sendLastFrame = callback != null;
        this.jpegPath = jpegPath;
        jpegFrameCallback = callback;
    }

    public void dispose() {
        Logging.d(TAG, "dispose");
        muted = false;
        timestampAligner.dispose();
        timestampAligner = null;
        mainHandler.removeCallbacks(blackFrameProducer);

        if (lastBuffer != null) {
            lastBuffer.release();
            lastBuffer = null;
        }
    }

    @Override
    public void onCapturerStarted(boolean success) {
        realObserver.onCapturerStarted(success);
    }

    @Override
    public void onCapturerStopped() {
        realObserver.onCapturerStopped();
    }

    @Override
    public void onFrameCaptured(VideoFrame frame) {
        if (!paused && !muted && timestampAligner != null) {
            if (sendLastFrame && lastBuffer == null) {
                VideoFrame.Buffer frameBuffer = frame.getBuffer();
                lastBuffer = frameBuffer.toI420();
                lastFrameRotation = frame.getRotation();
            }
            if (!sendLastFrame && lastBuffer != null) {
                lastBuffer.release();
                lastBuffer = null;
            }

            VideoFrame newFrame = new VideoFrame(
                    sendLastFrame ? lastBuffer : frame.getBuffer(),
                    sendLastFrame ? lastFrameRotation : frame.getRotation(),
                    timestampAligner.translateTimestamp(frame.getTimestampNs()));

            if (jpegFrameCallback != null && jpegPath != null) {
                Logging.d(TAG, "callback last frame");

                int width = newFrame.getRotatedWidth();
                int height = newFrame.getRotatedHeight();
                getI420(newFrame);
                try {
                    NV21toJPEG(I420toNV21(yuvFrameCallbackBuffer, width, height), width, height,
                            100, jpegPath);
                    jpegFrameCallback.onJpegFrame(jpegPath);
                } catch (IOException e) {
                    Logging.e(TAG, "NV21toJPEG", e);
                }

                jpegPath = null;
                jpegFrameCallback = null;
            } else if (yuvFrameCallback != null
                       && newFrame.getTimestampNs() - lastYuvFrameCallbackTimestampNs
                          >= yuvFrameCallbackIntervalNs) {
                lastYuvFrameCallbackTimestampNs = newFrame.getTimestampNs();
                int yuvBufferSize = getI420(newFrame);
                yuvFrameCallback.onYuvFrame(frame.getRotatedWidth(), frame.getRotatedHeight(),
                        yuvFrameCallbackBuffer, yuvBufferSize);
            }

            realObserver.onFrameCaptured(newFrame);
        }
    }

    private int getI420(VideoFrame frame) {
        VideoFrame.Buffer frameBuffer = frame.getBuffer();
        int yuvBufferSize = frameBuffer.getWidth() * frameBuffer.getHeight() * 3 / 2;
        if (yuvFrameCallbackBuffer == null
            || yuvFrameCallbackBuffer.length < yuvBufferSize) {
            yuvFrameCallbackBuffer = new byte[yuvBufferSize];
        }
        if (yuvDirectBuffer == null || yuvDirectBuffer.capacity() < yuvBufferSize) {
            yuvDirectBuffer = ByteBuffer.allocateDirect(yuvBufferSize);
        }
        VideoFrame.I420Buffer i420 = frameBuffer.toI420();
        YuvHelper.I420Rotate(i420.getDataY(), i420.getStrideY(), i420.getDataU(),
                i420.getStrideU(), i420.getDataV(), i420.getStrideV(), yuvDirectBuffer,
                i420.getWidth(), i420.getHeight(), frame.getRotation());
        i420.release();
        yuvDirectBuffer.rewind();
        yuvDirectBuffer.get(yuvFrameCallbackBuffer, 0, yuvBufferSize);

        return yuvBufferSize;
    }

    private void produceBlackFrame() {
        if (!muted) {
            return;
        }

        blackBuffer.retain();
        realObserver.onFrameCaptured(
                new VideoFrame(blackBuffer, 0, TimestampAligner.getRtcTimeNanos(), true));

        mainHandler.postDelayed(blackFrameProducer, BLACK_FRAME_INTERVAL_MS);
    }

    public interface YuvFrameCallback {
        void onYuvFrame(int width, int height, byte[] buffer, int length);
    }

    public interface JpegFrameCallback {
        void onJpegFrame(File jpegPath);
    }
}
