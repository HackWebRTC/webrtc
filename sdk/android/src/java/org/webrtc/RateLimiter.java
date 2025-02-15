package org.webrtc;

/**
 * Created by Created by Piasy{github.com/Piasy} on 11/04/2017.
 */
public class RateLimiter {
    private volatile long mInterval;

    private volatile long mExpected;
    private volatile long mFirstTs = -1;

    public RateLimiter(final long interval) {
        mInterval = interval;
    }

    public boolean check(final long ts) {
        if (mFirstTs == -1) {
            mFirstTs = ts;
            return true;
        }

        long expected = mExpected;
        long relativeTs = ts - mFirstTs;
        if (relativeTs >= expected) {
            if (relativeTs - expected > mInterval) {
                mFirstTs = -1;
                mExpected = mInterval;
            } else {
                mExpected = expected + mInterval;
            }
            return true;
        }

        return false;
    }

    public void reset() {
        mFirstTs = -1;
        mExpected = 0;
    }

    public void updateInterval(final long interval) {
        mInterval = interval;
        mFirstTs = -1;
        mExpected = 0;
    }

    public long getInterval() {
        return mInterval;
    }
}
