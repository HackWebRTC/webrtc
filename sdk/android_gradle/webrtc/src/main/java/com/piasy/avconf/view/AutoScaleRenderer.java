package com.piasy.avconf.view;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import org.webrtc.Logging;
import org.webrtc.VideoFrame;

/**
 * Created by Piasy{github.com/Piasy} on 2018/9/28.
 */
public class AutoScaleRenderer extends TextureViewRenderer {
    public static final int SCALE_TYPE_CENTER_CROP = 1;
    public static final int SCALE_TYPE_CENTER_INSIDE = 2;

    private static final String TAG = "AutoScaleRenderer";

    private int mScaleType;

    private int mVideoWidth;
    private int mVideoHeight;

    public AutoScaleRenderer(Context context) {
        super(context);
    }

    public AutoScaleRenderer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public AutoScaleRenderer(final Context context, final String resourceName,
            final int scaleType) {
        super(context, resourceName);
        mScaleType = scaleType;
    }

    public void setScaleType(int scaleType) {
        mScaleType = scaleType;
        adjustViewSize();
    }

    @Override
    public void onFrame(final VideoFrame frame) {
        super.onFrame(frame);

        int videoWidth = frame.getRotatedWidth();
        int videoHeight = frame.getRotatedHeight();
        if (mVideoWidth != videoWidth || mVideoHeight != videoHeight) {
            mVideoWidth = videoWidth;
            mVideoHeight = videoHeight;
            post(this::doAdjustViewSize);
        }
    }

    public void adjustViewSize() {
        post(this::doAdjustViewSize);
    }

    private void doAdjustViewSize() {
        if (!(getParent() instanceof FrameLayout)) {
            return;
        }
        if (mScaleType == SCALE_TYPE_CENTER_CROP) {
            Logging.d(TAG, resourceName + " adjustViewSize parent for CENTER_CROP");
            FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) getLayoutParams();
            params.width = ViewGroup.LayoutParams.MATCH_PARENT;
            params.height = ViewGroup.LayoutParams.MATCH_PARENT;
            params.leftMargin = 0;
            params.topMargin = 0;
            setLayoutParams(params);
            return;
        }

        FrameLayout parent = (FrameLayout) getParent();
        int parentWidth = parent.getWidth();
        int parentHeight = parent.getHeight();

        Logging.d(TAG, resourceName + " adjustViewSize parent for CENTER_INSIDE "
                       + parentWidth + "x" + parentHeight
                       + ", video " + mVideoWidth + "x" + mVideoHeight);
        if (parentWidth != 0 && parentHeight != 0 && mVideoWidth != 0 && mVideoHeight != 0) {
            final float frameAspectRatio = mVideoWidth / (float) mVideoHeight;
            final float drawnAspectRatio = parentWidth / (float) parentHeight;

            final float scaleX;
            final float scaleY;
            if (frameAspectRatio > drawnAspectRatio) {
                scaleY = drawnAspectRatio / frameAspectRatio;
                scaleX = 1f;
            } else {
                scaleY = 1f;
                scaleX = frameAspectRatio / drawnAspectRatio;
            }
            Logging.d(TAG, resourceName + " adjustViewSize scaleX " + scaleX
                           + ", scaleY " + scaleY);
            FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) getLayoutParams();
            params.width = (int) (parentWidth * scaleX);
            params.height = (int) (parentHeight * scaleY);
            params.leftMargin = (parentWidth - params.width) / 2;
            params.topMargin = (parentHeight - params.height) / 2;
            setLayoutParams(params);
        }
    }
}
