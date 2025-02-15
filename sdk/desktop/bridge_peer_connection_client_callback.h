#pragma once

#include "sdk/desktop/peer_connection_client_callback.h"

#include "sdk/desktop/lib_pc_client.h"

namespace AvConf {

class BridgePeerConnectionClientCallback : public PeerConnectionClientCallback {
public:
    BridgePeerConnectionClientCallback(PCClientCallback& real_callback);

    static int FromWebRTCSdpType(webrtc::SdpType type);
    static webrtc::SdpType ToWebRTCSdpType(int type);

    std::string OnPreferCodecs(const std::string& peer_uid, const std::string& sdp) override;

    void OnLocalDescription(const std::string& peer_uid, const webrtc::SessionDescriptionInterface& local_sdp) override;

    void OnIceCandidate(const std::string& peer_uid, const webrtc::IceCandidateInterface& candidate) override;

    void OnIceCandidatesRemoved(const std::string& peer_uid, const std::vector<webrtc::IceCandidateInterface*>& candidates) override;

    void OnPeerConnectionstatsReady(const std::string& peer_uid, const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;

    void OnIceConnected(const std::string& peer_uid) override;

    void OnIceDisconnected(const std::string& peer_uid) override;

    void OnError(const std::string& peer_uid, int code) override;

private:
    PCClientCallback real_callback_;
};

}
