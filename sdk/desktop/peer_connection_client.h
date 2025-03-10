#pragma once

#include <memory>
#include <vector>

#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "pc/video_track_source.h"
#include "sdk/desktop/peer_connection_client_callback.h"
#if defined(WEBRTC_WIN)
#include "sdk/desktop/win32_video_renderer.h"
#endif

#define PC_CLIENT_VERSION "1.0.43659"

namespace AvConf {

class PeerConnectionClient : public webrtc::PeerConnectionObserver {
 public:
  static std::string const K_AUDIO_TRACK_ID;
  static std::string const K_VIDEO_TRACK_ID;

  static constexpr int CAPTURER_TYPE_CAMERA = 1;
  static constexpr int CAPTURER_TYPE_SCREEN = 2;
  static constexpr int CAPTURER_TYPE_FILE = 3;

  PeerConnectionClient(
      const std::string& peer_uid,
      int dir,
      bool has_video,
      const std::shared_ptr<PeerConnectionClientCallback>& callback,
      int video_max_bitrate_kbps,
      int video_max_frame_rate);
  ~PeerConnectionClient();

  static int CreatePeerConnectionFactory(void* hwnd,
                                         int disable_encryption,
                                         int dummy_audio_device,
                                         int transit_video);
  static int CreateLocalTracks(webrtc::VideoTrackSource* video_source);
  static void DestroyLocalTracks();

  static webrtc::VideoTrackSource* CreateVideoCapturer(int type,
                                                       int width,
                                                       int height,
                                                       int frame_rate,
                                                       const char* extra_param);
  static void StartVideoCapturer(void* video_capturer, int type);
  static void StopVideoCapturer(void* video_capturer, int type);
  static void AdaptVideoCapturerOutputFormat(void* video_capturer,
                                             int type,
                                             int width,
                                             int height,
                                             int fps);
  static void DestroyVideoCapturer(void* video_capturer, int type);

#if defined(WEBRTC_WIN)
  static void AddLocalRenderer(Win32VideoRenderer* renderer);
  static void RemoveLocalRenderer(Win32VideoRenderer* renderer);
#endif

  static void LogLongMessage(std::string* message);

  void CreatePeerConnection(
      const std::vector<webrtc::PeerConnectionInterface::IceServer>&
          ice_servers);

  void GetStats();

  void SetAudioSendingEnabled(bool enable);
  void SetVideoSendingEnabled(bool enable);
  void SetAudioReceivingEnabled(bool enable);
  void SetVideoReceivingEnabled(bool enable);

  void CreateOffer();
  void CreateAnswer();
  void AddIceCandidate(const webrtc::IceCandidateInterface* candidate);
  void RemoveIceCandidates(
      const std::vector<webrtc::IceCandidateInterface*>& candidates);
  void SetRemoteDescription(webrtc::SessionDescriptionInterface* desc);

#if defined(WEBRTC_WIN)
  void AddRemoteRenderer(Win32VideoRenderer* renderer);
  void RemoveRemoteRenderer(Win32VideoRenderer* renderer);
#endif

  void Close();

  void OnCreateSuccess(webrtc::SessionDescriptionInterface* desc);
  void OnCreateFailure(const webrtc::RTCError& error);

  void OnSetSuccess();
  void OnSetFailure(const webrtc::RTCError& error);

  void OnGetStatsSuccess(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report);

 protected:
  // PeerConnectionObserver implementation.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state);
  void OnAddTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
      const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
          streams);
  void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver);
  void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel);
  void OnRenegotiationNeeded();
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state);
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
  void OnIceConnectionReceivingChange(bool receiving);

 private:
  void reportError(int code);
  void getRemoteTracks();
  void drainCandidates();
  bool send();
  bool receive();
  void setVideoMaxBitrate();
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions defaultSdpOptions();

  std::string peer_uid_;
  int dir_;
  bool has_video_;
  std::shared_ptr<PeerConnectionClientCallback> callback_;
#if defined(WEBRTC_WIN)
  std::vector<Win32VideoRenderer*> remote_track_renderers_;
#endif
  int video_max_bitrate_kbps_;
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
