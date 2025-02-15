#pragma once

#include <vector>
#include <memory>
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "pc/video_track_source.h"
#include "sdk/desktop/peer_connection_client_callback.h"
#include "sdk/desktop/native_renderer.h"

#define PC_CLIENT_VERSION "1.2.28032"

namespace AvConf {

class PeerConnectionClient : public webrtc::PeerConnectionObserver {
 public:
    static constexpr int DIR_INACTIVE = 0;

    static constexpr int DIR_RECV_ONLY = 1;

    static constexpr int DIR_SEND_ONLY = 2;

    static constexpr int DIR_SEND_RECV = 3;

    static std::string const K_AUDIO_TRACK_ID;

    static std::string const K_VIDEO_TRACK_ID;

    static std::string const K_STREAM_ID;

    static constexpr int K_BPS_IN_KBPS = 1000;

  PeerConnectionClient(
      const std::string& peer_uid, int dir,
      const std::shared_ptr<PeerConnectionClientCallback>& callback,
      int video_max_bitrate, int video_max_frame_rate);
  ~PeerConnectionClient();

  static constexpr int CAPTURER_TYPE_CAMERA = 1;
  static constexpr int CAPTURER_TYPE_SCREEN = 2;

  static int CreatePeerConnectionFactory();
  static int CreateLocalTracks(webrtc::VideoTrackSource* source);
  static int DestroyPeerConnectionFactory();

  void create_peer_connection(
      const std::vector<webrtc::PeerConnectionInterface::IceServer>& ice_servers);

  void enable_stats_events(bool enable, int period_ms);
  void set_audio_sending_enabled(bool enable);
  void set_video_sending_enabled(bool enable);
  void set_audio_receiving_enabled(bool enable);
  void set_video_receiving_enabled(bool enable);

  void create_offer();
  void create_answer();
  void add_ice_candidate(const webrtc::IceCandidateInterface* candidate);
  void remove_ice_candidates(
      const std::vector<webrtc::IceCandidateInterface*>& candidates);
  void set_remote_description(webrtc::SessionDescriptionInterface* desc);

  bool send();
  bool receive();
  bool stats_enabled();

  void close();

  void OnCreateSuccess(webrtc::SessionDescriptionInterface* desc);
  void OnCreateFailure(const webrtc::RTCError& error);

  void OnSetSuccess();
  void OnSetFailure(const webrtc::RTCError& error);

 protected:
  // PeerConnectionObserver implementation.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state);
  void OnAddTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
      const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
          streams);
  void OnRemoveTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver);
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel);
  void OnRenegotiationNeeded();
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state);
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
  void OnIceConnectionReceivingChange(bool receiving);

 private:
  void reportError(int code);
  void tryGetRemoteTracks();
  void drainCandidates();

  std::string uid_;
  int dir_;
  std::shared_ptr<PeerConnectionClientCallback> callback_;
  //NativeRenderer remote_track_renderer_;
  bool remote_renderer_added_;
  int video_max_bitrate_;
  int video_max_frame_rate_;

  bool is_initiator_;
  std::unique_ptr<webrtc::SessionDescriptionInterface> local_sdp_;

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> remote_audio_track_;
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> remote_video_track_;

  std::vector<const webrtc::IceCandidateInterface*> queued_candidates_;
  bool candidates_drained_;
};

}  // namespace AvConf
