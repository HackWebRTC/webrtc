#include "sdk/desktop/lib_pc_client.h"

#include <memory>

#include "rtc_base/logging.h"
#include "rtc_base/ssl_adapter.h"
#include "sdk/desktop/bridge_peer_connection_client_callback.h"
#include "sdk/desktop/peer_connection_client.h"
#include "sdk/desktop/peer_connection_client_callback.h"
#if defined(WEBRTC_WIN)
#include "sdk/desktop/win32_video_renderer.h"
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif
#include "system_wrappers/include/field_trial.h"

class PCClientLogSink : public rtc::LogSink {
 public:
  PCClientLogSink(PCClientLogCallback callback) : callback_(callback) {}
  ~PCClientLogSink() override {}

  void OnLogMessage(const std::string& message) override {}
  void OnLogMessage(const std::string& message,
                    rtc::LoggingSeverity severity) override {
    callback_((int)severity, message.c_str());
  }

 private:
  PCClientLogCallback callback_;
};

const char* PCClientVersion() {
  return PC_CLIENT_VERSION;
}

int PCClientInitialize(const char* field_trials) {
  webrtc::field_trial::InitFieldTrialsFromString(field_trials);
  rtc::InitializeSSL();
#if defined(WEBRTC_WIN)
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    RTC_LOG(LS_INFO) << "PCClientInitialize WSAStartup failed " << result;
    return result;
  }
  if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
    RTC_LOG(LS_INFO) << "Winsock version not supported " << LOBYTE(wsaData.wVersion) << " " << HIBYTE(wsaData.wVersion);
    WSACleanup();
    return -404;
  }
#endif
  return 0;
}

void PCClientSetLogCallback(PCClientLogCallback callback, int severity) {
  rtc::LogMessage::AddLogToStream(new PCClientLogSink(callback),
                                  rtc::LoggingSeverity::LS_INFO);
}

void* PCClientVideoCapturerCreate(int type,
                                  int width,
                                  int height,
                                  int frame_rate,
                                  const char* extra_param) {
  return AvConf::PeerConnectionClient::CreateVideoCapturer(
      type, width, height, frame_rate, extra_param);
}

void PCClientVideoCapturerStart(void* capturer, int type) {
  AvConf::PeerConnectionClient::StartVideoCapturer(capturer, type);
}

void PCClientVideoCapturerStop(void* capturer, int type) {
  AvConf::PeerConnectionClient::StopVideoCapturer(capturer, type);
}

void PCClientAdaptVideoOutputFormat(void* capturer,
                                    int type,
                                    int width,
                                    int height,
                                    int fps) {
  AvConf::PeerConnectionClient::AdaptVideoCapturerOutputFormat(
      capturer, type, width, height, fps);
}

void PCClientVideoCapturerDestroy(void* capturer, int type) {
  AvConf::PeerConnectionClient::DestroyVideoCapturer(capturer, type);
}

#if defined(WEBRTC_WIN)
void* PCClientVideoRendererCreate(int top,
                                  int left,
                                  int width,
                                  int height,
                                  int z_index,
                                  int scale_type) {
  return new AvConf::Win32VideoRenderer(top, left, width, height, z_index,
                                        scale_type);
}

void PCClientVideoRendererDestroy(void* renderer) {
  delete reinterpret_cast<AvConf::Win32VideoRenderer*>(renderer);
}
#endif

int PCClientCreatePeerConnectionFactory(void* hwnd,
                                        int disable_encryption,
                                        int dummy_audio_device,
                                        int transit_video) {
  return AvConf::PeerConnectionClient::CreatePeerConnectionFactory(
      hwnd, disable_encryption, dummy_audio_device, transit_video);
}

int PCClientCreateLocalTracks(void* video_source) {
  return AvConf::PeerConnectionClient::CreateLocalTracks(
      reinterpret_cast<webrtc::VideoTrackSource*>(video_source));
}

void PCClientDestroyLocalTracks() {
  return AvConf::PeerConnectionClient::DestroyLocalTracks();
}

#if defined(WEBRTC_WIN)
void PCClientAddLocalRenderer(void* renderer) {
  AvConf::PeerConnectionClient::AddLocalRenderer(
      reinterpret_cast<AvConf::Win32VideoRenderer*>(renderer));
}

void PCClientRemoveLocalRenderer(void* renderer) {
  AvConf::PeerConnectionClient::RemoveLocalRenderer(
      reinterpret_cast<AvConf::Win32VideoRenderer*>(renderer));
}
#endif

void* PCClientCreate(const char* peer_uid,
                     int dir,
                     int has_video,
                     struct PCClientCallback callback,
                     int video_max_bitrate_kbps,
                     int video_max_frame_rate) {
  return new AvConf::PeerConnectionClient(
      peer_uid, dir, has_video,
      std::make_shared<AvConf::BridgePeerConnectionClientCallback>(callback),
      video_max_bitrate_kbps, video_max_frame_rate);
}

void PCClientCreatePeerConnection(void* client) {
  reinterpret_cast<AvConf::PeerConnectionClient*>(client)->CreatePeerConnection(
      std::vector<webrtc::PeerConnectionInterface::IceServer>());
}

void PCClientCreateOffer(void* client) {
  reinterpret_cast<AvConf::PeerConnectionClient*>(client)->CreateOffer();
}

void PCClientCreateAnswer(void* client) {
  reinterpret_cast<AvConf::PeerConnectionClient*>(client)->CreateAnswer();
}

void PCClientSetRemoteDescription(void* client,
                                  int type,
                                  const char* description) {
  std::string log("setRemoteDescription\n");
  log.append(description);
  AvConf::PeerConnectionClient::LogLongMessage(&log);
  std::unique_ptr<webrtc::SessionDescriptionInterface> remote_sdp =
      webrtc::CreateSessionDescription(
          AvConf::BridgePeerConnectionClientCallback::ToWebRTCSdpType(type),
          description);
  reinterpret_cast<AvConf::PeerConnectionClient*>(client)->SetRemoteDescription(
      remote_sdp.release());
}

#if defined(WEBRTC_WIN)
void PCClientAddRemoteRenderer(void* client, void* renderer) {
  reinterpret_cast<AvConf::PeerConnectionClient*>(client)->AddRemoteRenderer(
      reinterpret_cast<AvConf::Win32VideoRenderer*>(renderer));
}

void PCClientRemoveRemoteRenderer(void* client, void* renderer) {
  reinterpret_cast<AvConf::PeerConnectionClient*>(client)->RemoveRemoteRenderer(
      reinterpret_cast<AvConf::Win32VideoRenderer*>(renderer));
}
#endif

void PCClientAddIceCandidate(void* client,
                             const char* sdp_mid,
                             int sdp_mline_index,
                             const char* sdp) {
  webrtc::IceCandidateInterface* candidate =
      webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, sdp, nullptr);
  reinterpret_cast<AvConf::PeerConnectionClient*>(client)->AddIceCandidate(
      candidate);
}

void PCClientGetStats(void* client) {
  reinterpret_cast<AvConf::PeerConnectionClient*>(client)->GetStats();
}

void PCClientSetAudioSendingEnabled(void* client, int enable) {
  reinterpret_cast<AvConf::PeerConnectionClient*>(client)
      ->SetAudioSendingEnabled(enable);
}

void PCClientSetVideoSendingEnabled(void* client, int enable) {
  reinterpret_cast<AvConf::PeerConnectionClient*>(client)
      ->SetVideoSendingEnabled(enable);
}

void PCClientSetAudioReceivingEnabled(void* client, int enable) {
  reinterpret_cast<AvConf::PeerConnectionClient*>(client)
      ->SetAudioReceivingEnabled(enable);
}

void PCClientSetVideoReceivingEnabled(void* client, int enable) {
  reinterpret_cast<AvConf::PeerConnectionClient*>(client)
      ->SetVideoReceivingEnabled(enable);
}

void PCClientClose(void* client) {
  AvConf::PeerConnectionClient* pc_client =
      reinterpret_cast<AvConf::PeerConnectionClient*>(client);
  pc_client->Close();
  delete pc_client;
}
