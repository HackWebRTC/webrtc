#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "api/jsep.h"
#include "api/stats/rtc_stats_report.h"

namespace AvConf {

class PeerConnectionClientCallback {
public:
    virtual ~PeerConnectionClientCallback() {}

    // stay sync with PeerConnectionClient.kt
    static constexpr int ERR_NO_FACTORY = 1000;
    static constexpr int ERR_NO_SENDING_TRACK = 1001;
    static constexpr int ERR_CREATE_PC_FAIL = 1002;
    static constexpr int ERR_ICE_FAIL = 1003;
    static constexpr int ERR_CREATE_MULTIPLE_SDP = 1004;
    static constexpr int ERR_CREATE_SDP_FAIL = 1005;
    static constexpr int ERR_SET_SDP_FAIL = 1006;

    virtual std::string OnPreferCodecs(const std::string& peer_uid, const std::string& sdp) = 0;

    virtual void OnLocalDescription(const std::string& peer_uid, const webrtc::SessionDescriptionInterface& local_sdp) = 0;

    virtual void OnIceCandidate(const std::string& peer_uid, const webrtc::IceCandidateInterface& candidate) = 0;

    virtual void OnIceCandidatesRemoved(const std::string& peer_uid, const std::vector<webrtc::IceCandidateInterface*>& candidates) = 0;

    virtual void OnPeerConnectionstatsReady(const std::string& peer_uid, const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) = 0;

    virtual void OnIceConnected(const std::string& peer_uid) = 0;

    virtual void OnIceDisconnected(const std::string& peer_uid) = 0;

    virtual void OnError(const std::string& peer_uid, int code) = 0;
};

}  // namespace AvConf
