package com.piasy.avconf;

import java.util.List;
import org.webrtc.IceCandidate;
import org.webrtc.RTCStatsReport;
import org.webrtc.SessionDescription;

public interface PeerConnectionClientCallback {
    int ERR_NO_FACTORY = 1000;

    int ERR_NO_SENDING_TRACK = 1001;

    int ERR_CREATE_PC_FAIL = 1002;

    int ERR_ICE_FAIL = 1003;

    int ERR_CREATE_MULTIPLE_SDP = 1004;

    int ERR_CREATE_SDP_FAIL = 1005;

    int ERR_SET_SDP_FAIL = 1006;

    String onPreferCodecs(String peerUid, String sdp);

    void onLocalDescription(String peerUid, SessionDescription localSdp);

    void onIceCandidate(String peerUid, IceCandidate candidate);

    void onIceCandidatesRemoved(String peerUid, List<IceCandidate> candidates);

    void onPeerConnectionStatsReady(String peerUid, RTCStatsReport report);

    void onIceConnected(String peerUid);

    void onIceDisconnected(String peerUid);

    void onError(String peerUid, int code);
}
