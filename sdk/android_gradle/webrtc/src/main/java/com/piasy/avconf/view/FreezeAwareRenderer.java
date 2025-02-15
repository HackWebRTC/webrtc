package com.piasy.avconf.view;

import android.content.Context;
import android.util.AttributeSet;
import org.webrtc.Logging;
import org.webrtc.VideoFrame;

/**
 * Created by Piasy{github.com/Piasy} on 2018/9/12.
 */
public class FreezeAwareRenderer extends AutoScaleRenderer {

    private static final String TAG = "FreezeAwareRenderer";

    private static final long FREEZE_THRESHOLD_MS = 250;
    private static final long LOG_FREEZE_INTERVAL_MS = 3000;

    private long mLastFrameReceivedTime;
    private long mLastLogFreezeTime;

    public FreezeAwareRenderer(Context context) {
        super(context);
    }

    public FreezeAwareRenderer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public FreezeAwareRenderer(final Context context, String uid, final int scaleType) {
        super(context, uid + " ", scaleType);
    }

    @Override
    public void onFrame(final VideoFrame frame) {
        super.onFrame(frame);

        long now = System.currentTimeMillis();
        long lastFrameReceivedTime = mLastFrameReceivedTime;
        mLastFrameReceivedTime = now;
        if (lastFrameReceivedTime == 0) {
            Logging.d(TAG, "video render start: " + resourceName + ", "
                           + frame.getRotatedWidth() + "x" + frame.getRotatedHeight());
            return;
        }

        long interval = now - lastFrameReceivedTime;
        if (interval < FREEZE_THRESHOLD_MS) {
            return;
        }

        if (now - mLastLogFreezeTime < LOG_FREEZE_INTERVAL_MS) {
            return;
        }
        mLastLogFreezeTime = now;

        Logging.e(TAG, "video freeze " + interval + " ms: " + resourceName + ", "
                       + frame.getRotatedWidth() + "x" + frame.getRotatedHeight());
    }
}
