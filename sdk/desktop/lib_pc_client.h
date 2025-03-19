#pragma once

#if defined(WEBRTC_WIN)
#if defined(PC_CLIENT_SELF_DLL)
#define PC_CLIENT_API __declspec(dllexport)
#else
#define PC_CLIENT_API __declspec(dllimport)
#endif
#else
#define PC_CLIENT_API __attribute__((visibility("default")))
#endif

#if __cplusplus
extern "C" {
#endif

typedef void (*PCClientFreeKString)(const char*);

typedef const char* (*PCClientOnPreferCodecs)(void*, const char*, const char*);
typedef void (*PCClientOnLocalDescription)(void*,
                                           const char*,
                                           int,
                                           const char*);
typedef void (
    *PCClientOnIceCandidate)(void*, const char*, const char*, int, const char*);
typedef void (*PCClientOnPeerConnectionStatsReady)(void*,
                                                   const char*,
                                                   const char*);
typedef void (*PCClientOnIceConnected)(void*, const char*);
typedef void (*PCClientOnIceDisconnected)(void*, const char*);
typedef void (*PCClientOnError)(void*, const char*, int);

struct PCClientCallback {
  PCClientFreeKString free_kstring;
  PCClientOnPreferCodecs on_prefer_codecs;
  PCClientOnLocalDescription on_local_description;
  PCClientOnIceCandidate on_ice_candidate;
  PCClientOnPeerConnectionStatsReady on_stats_ready;
  PCClientOnIceConnected on_ice_connected;
  PCClientOnIceDisconnected on_ice_disconnected;
  PCClientOnError on_error;
  void* opaque;
};

// work around for create kotlin lambda (function2) in cpp user code
typedef void (*KmpWebRTCErrorHandler)(void*, int, const char*);

PC_CLIENT_API const char* PCClientVersion();

PC_CLIENT_API int PCClientInitialize(const char* field_trials);

typedef void (*PCClientLogCallback)(int, const char*);
PC_CLIENT_API void PCClientSetLogCallback(PCClientLogCallback callback, int severity);

PC_CLIENT_API void* PCClientVideoCapturerCreate(int type,
                                                int width,
                                                int height,
                                                int frame_rate,
                                                const char* extra_param1,
                                                const char* extra_param2);
PC_CLIENT_API void PCClientVideoCapturerStart(void* capturer, int type);
PC_CLIENT_API void PCClientVideoCapturerStop(void* capturer, int type);
PC_CLIENT_API void PCClientAdaptVideoOutputFormat(void* capturer,
                                                  int type,
                                                  int width,
                                                  int height,
                                                  int fps);
PC_CLIENT_API void PCClientVideoCapturerDestroy(void* capturer, int type);

#if defined(WEBRTC_WIN)
PC_CLIENT_API void* PCClientVideoRendererCreate(int top,
                                                int left,
                                                int width,
                                                int height,
                                                int z_index,
                                                int scale_type);
PC_CLIENT_API void PCClientVideoRendererDestroy(void* renderer);
#endif

PC_CLIENT_API int PCClientCreatePeerConnectionFactory(void* hwnd,
                                                      int disable_encryption,
                                                      int dummy_audio_device,
                                                      int transit_video);
PC_CLIENT_API int PCClientCreateLocalTracks(void* video_source);
PC_CLIENT_API void PCClientDestroyLocalTracks();
#if defined(WEBRTC_WIN)
PC_CLIENT_API void PCClientAddLocalRenderer(void* renderer);
PC_CLIENT_API void PCClientRemoveLocalRenderer(void* renderer);
#endif

PC_CLIENT_API void* PCClientCreate(const char* peer_uid,
                                   int dir,
                                   int has_video,
                                   struct PCClientCallback callback,
                                   int video_max_bitrate_kbps,
                                   int video_max_frame_rate);
PC_CLIENT_API void PCClientCreatePeerConnection(void* client);
PC_CLIENT_API void PCClientCreateOffer(void* client);
PC_CLIENT_API void PCClientCreateAnswer(void* client);
PC_CLIENT_API void PCClientSetRemoteDescription(void* client,
                                                int type,
                                                const char* description);
#if defined(WEBRTC_WIN)
PC_CLIENT_API void PCClientAddRemoteRenderer(void* client, void* renderer);
PC_CLIENT_API void PCClientRemoveRemoteRenderer(void* client, void* renderer);
#endif
PC_CLIENT_API void PCClientAddIceCandidate(void* client,
                                           const char* sdp_mid,
                                           int sdp_mline_index,
                                           const char* sdp);
PC_CLIENT_API void PCClientGetStats(void* client);
PC_CLIENT_API void PCClientSetAudioSendingEnabled(void* client, int enable);
PC_CLIENT_API void PCClientSetVideoSendingEnabled(void* client, int enable);
PC_CLIENT_API void PCClientSetAudioReceivingEnabled(void* client, int enable);
PC_CLIENT_API void PCClientSetVideoReceivingEnabled(void* client, int enable);
PC_CLIENT_API void PCClientClose(void* client);

PC_CLIENT_API int PCClientStartRecorder(void* client, int dir, const char* path);
PC_CLIENT_API int PCClientStopRecorder(void* client, int dir);

#if __cplusplus
}
#endif
