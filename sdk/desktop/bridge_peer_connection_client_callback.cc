#include "sdk/desktop/bridge_peer_connection_client_callback.h"

#include "json/json.h"

namespace AvConf {

BridgePeerConnectionClientCallback::BridgePeerConnectionClientCallback(PCClientCallback& real_callback) : real_callback_(real_callback) {
}

int BridgePeerConnectionClientCallback::FromWebRTCSdpType(webrtc::SdpType type) {
    switch (type) {
        case webrtc::SdpType::kPrAnswer:
            return 2; // KT: PRANSWER
        case webrtc::SdpType::kAnswer:
            return 3; // KT: ANSWER
        case webrtc::SdpType::kOffer:
        default:
            return 1; // KT: OFFER
    }
}

webrtc::SdpType BridgePeerConnectionClientCallback::ToWebRTCSdpType(int type) {
    switch (type) {
        case 2: // KT: PRANSWER
            return webrtc::SdpType::kPrAnswer;
        case 3: // KT: ANSWER
            return webrtc::SdpType::kAnswer;
        case 1: // KT: OFFER
        default:
            return webrtc::SdpType::kOffer;
    }
}

std::string BridgePeerConnectionClientCallback::OnPreferCodecs(const std::string& peer_uid, const std::string& sdp) {
    const char* refined_sdp = real_callback_.on_prefer_codecs(real_callback_.opaque, peer_uid.c_str(), sdp.c_str());
    std::string refined_sdp_str(refined_sdp);
    real_callback_.free_kstring(refined_sdp);
    return refined_sdp_str;
}

void BridgePeerConnectionClientCallback::OnLocalDescription(const std::string& peer_uid, const webrtc::SessionDescriptionInterface& local_sdp) {
    std::string sdp_string;
    if (!local_sdp.ToString(&sdp_string)) {
        return;
    }
    real_callback_.on_local_description(real_callback_.opaque, peer_uid.c_str(), FromWebRTCSdpType(local_sdp.GetType()), sdp_string.c_str());
}

void BridgePeerConnectionClientCallback::OnIceCandidate(const std::string& peer_uid, const webrtc::IceCandidateInterface& candidate) {
    std::string sdp_string;
    if (!candidate.ToString(&sdp_string)) {
        return;
    }
    RTC_LOG(LS_INFO) << "BridgePeerConnectionClientCallback::OnIceCandidate " << sdp_string;
    real_callback_.on_ice_candidate(real_callback_.opaque, peer_uid.c_str(), candidate.sdp_mid().c_str(), candidate.sdp_mline_index(), sdp_string.c_str());
}

void BridgePeerConnectionClientCallback::OnIceCandidatesRemoved(const std::string& peer_uid, const std::vector<webrtc::IceCandidateInterface*>& candidates) {
    // unsupported yet
}

void BridgePeerConnectionClientCallback::OnPeerConnectionstatsReady(const std::string& peer_uid, const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
    Json::Value report_root;
    report_root["timestamp_us"] = report->timestamp().us();
    Json::Value stats_map;
    for (auto it = report->begin(); it != report->end(); it++) {
        Json::Value stats_root;
        stats_root["id"] = it->id();
        stats_root["type"] = it->type();
        stats_root["timestamp_us"] = it->timestamp().us();
        Json::Value stats_members(Json::objectValue);
        for (auto& attr : it->Attributes()) {
            stats_members[attr.name()] = attr.ToString();
        }
        stats_root["members"] = stats_members;
        stats_map[it->id()] = stats_root;
    }
    report_root["stats"] = stats_map;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string stats_str;
    stats_str.append(Json::writeString(builder, report_root));
    real_callback_.on_stats_ready(real_callback_.opaque, peer_uid.c_str(), stats_str.c_str());
}

void BridgePeerConnectionClientCallback::OnIceConnected(const std::string& peer_uid) {
    real_callback_.on_ice_connected(real_callback_.opaque, peer_uid.c_str());
}

void BridgePeerConnectionClientCallback::OnIceDisconnected(const std::string& peer_uid) {
    real_callback_.on_ice_disconnected(real_callback_.opaque, peer_uid.c_str());
}

void BridgePeerConnectionClientCallback::OnError(const std::string& peer_uid, int code) {
    real_callback_.on_error(real_callback_.opaque, peer_uid.c_str(), code);
}

}
