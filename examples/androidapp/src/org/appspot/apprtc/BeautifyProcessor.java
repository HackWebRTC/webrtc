package org.appspot.apprtc;

import android.support.annotation.Nullable;
import org.webrtc.VideoFrame;
import org.webrtc.VideoProcessor;
import org.webrtc.VideoSink;

/**
 * Created by Piasy{github.com/Piasy} on 2020/2/16.
 */
class BeautifyProcessor implements VideoProcessor {
    private VideoSink videoSink;

    @Override
    public void setSink(@Nullable VideoSink sink) {
        videoSink = sink;
    }

    @Override
    public void onCapturerStarted(boolean success) {
    }

    @Override
    public void onCapturerStopped() {
    }

    @Override
    public void onFrameCaptured(VideoFrame frame) {
        if (videoSink != null) {
            videoSink.onFrame(frame);
        }
    }
}
