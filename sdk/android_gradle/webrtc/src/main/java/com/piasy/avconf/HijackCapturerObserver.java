package com.piasy.avconf;

import android.graphics.Matrix;
import android.os.Handler;
import android.os.Looper;
import org.webrtc.CapturerObserver;
import org.webrtc.Logging;
import org.webrtc.TextureBufferImpl;
import org.webrtc.TimestampAligner;
import org.webrtc.VideoFrame;
import org.webrtc.YuvConverter;

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

    public HijackCapturerObserver(CapturerObserver realObserver) {
        this.realObserver = realObserver;

        // actually this buffer won't be used, VideoBroadcaster::OnFrame will replace it with
        // a black frame buffer and pass modified frame to track.
        blackBuffer = new TextureBufferImpl(480, 640, VideoFrame.TextureBuffer.Type.RGB, 0,
                new Matrix(), mainHandler, new YuvConverter(), null);
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

    public void dispose() {
        Logging.d(TAG, "dispose");
        muted = false;
        timestampAligner.dispose();
        timestampAligner = null;
        mainHandler.removeCallbacks(blackFrameProducer);
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
        if (!muted && timestampAligner != null) {
            realObserver.onFrameCaptured(new VideoFrame(frame.getBuffer(), frame.getRotation(),
                    timestampAligner.translateTimestamp(frame.getTimestampNs())));
        }
    }

    private void produceBlackFrame() {
        if (!muted) {
            return;
        }

        realObserver.onFrameCaptured(
                new VideoFrame(blackBuffer, 0, TimestampAligner.getRtcTimeNanos()));

        mainHandler.postDelayed(blackFrameProducer, BLACK_FRAME_INTERVAL_MS);
    }
}
