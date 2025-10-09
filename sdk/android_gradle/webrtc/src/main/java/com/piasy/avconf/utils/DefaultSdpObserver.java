package com.piasy.avconf.utils;

import org.webrtc.SdpObserver;
import org.webrtc.SessionDescription;

/**
 * Created by Piasy{github.com/Piasy} on 2025/04/03.
 */
abstract public class DefaultSdpObserver implements SdpObserver {
    @Override
    public void onCreateSuccess(SessionDescription sdp) {
    }

    @Override
    public void onSetSuccess() {
    }

    @Override
    public void onCreateFailure(String error) {
    }

    @Override
    public void onSetFailure(String error) {
    }
}
