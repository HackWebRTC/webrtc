package com.piasy.avconf.utils;

import org.webrtc.DataChannel;
import org.webrtc.IceCandidate;
import org.webrtc.MediaStream;
import org.webrtc.PeerConnection;

/**
 * Created by Piasy{github.com/Piasy} on 2025/04/03.
 */
public class DefaultPeerConnectionObserver implements PeerConnection.Observer {
    @Override
    public void onSignalingChange(PeerConnection.SignalingState newState) {
    }

    @Override
    public void onIceConnectionChange(PeerConnection.IceConnectionState newState) {
    }

    @Override
    public void onIceConnectionReceivingChange(boolean receiving) {
    }

    @Override
    public void onIceGatheringChange(PeerConnection.IceGatheringState newState) {
    }

    @Override
    public void onIceCandidate(IceCandidate candidate) {
    }

    @Override
    public void onIceCandidatesRemoved(IceCandidate[] candidates) {
    }

    @Override
    public void onAddStream(MediaStream stream) {
    }

    @Override
    public void onRemoveStream(MediaStream stream) {
    }

    @Override
    public void onDataChannel(DataChannel dataChannel) {
    }

    @Override
    public void onRenegotiationNeeded() {
    }
}
