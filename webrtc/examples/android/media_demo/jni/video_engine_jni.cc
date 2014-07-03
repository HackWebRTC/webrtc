/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains JNI for the video engine interfaces.
// The native functions are found using jni's auto discovery.

#include "webrtc/examples/android/media_demo/jni/video_engine_jni.h"

#include <map>
#include <string>

#include "webrtc/common_types.h"
#include "webrtc/examples/android/media_demo/jni/jni_helpers.h"
#include "webrtc/examples/android/media_demo/jni/media_codec_video_decoder.h"
#include "webrtc/examples/android/media_demo/jni/voice_engine_jni.h"
#include "webrtc/modules/utility/interface/helpers_android.h"
#include "webrtc/test/channel_transport/include/channel_transport.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_external_codec.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"

// Macro for native functions that can be found by way of jni-auto discovery.
// Note extern "C" is needed for "discovery" of native methods to work.
#define JOWW(rettype, name)                                             \
  extern "C" rettype JNIEXPORT JNICALL Java_org_webrtc_webrtcdemo_##name

namespace {

static JavaVM* g_vm = NULL;
static ClassReferenceHolder* g_class_reference_holder = NULL;

jclass GetClass(const char* name) {
  CHECK(g_class_reference_holder, "Class reference holder NULL");
  return g_class_reference_holder->GetClass(name);
}

// C(++) description of a camera. This class is created by Java native calls
// and associated with the CameraDesc Java class. The Java class is used in the
// Java code but it is just a thin wrapper of the C(++) class that contain the
// actual information. The information is stored in C(++) as it is used to
// call video engine APIs.
struct CameraDesc {
  // The name and id corresponds to ViECapture's |device_nameUTF8| and
  // |unique_idUTF8|.
  char name[64];
  char unique_id[64];
};

// C++ callback class that can be used to register for callbacks from the
// video engine. It further propagates the callbacks to
// VideoDecodeEncodeObserver.java interface. The memory associated with this
// class is managed globally by the VideoEngineData class when registering and
// unregistering VideoDecodeEncodeObserver.java to receive callbacks.
class VideoDecodeEncodeObserver : public webrtc::ViEDecoderObserver,
                                  public webrtc::ViEEncoderObserver {
 public:
  explicit VideoDecodeEncodeObserver(jobject j_observer)
      : j_observer_(j_observer) {
    webrtc::AttachThreadScoped ats(g_vm);
    JNIEnv* jni = ats.env();
    jclass j_observer_class = jni->GetObjectClass(j_observer_);
    incoming_rate_ =
        GetMethodID(jni, j_observer_class, "incomingRate", "(III)V");
    incoming_codec_changed_ =
        GetMethodID(jni, j_observer_class, "incomingCodecChanged",
                    "(ILorg/webrtc/webrtcdemo/VideoCodecInst;)V");
    request_new_keyframe_ =
        GetMethodID(jni, j_observer_class, "requestNewKeyFrame", "(I)V");
    outgoing_rate_ =
        GetMethodID(jni, j_observer_class, "outgoingRate", "(III)V");
    j_observer_ = jni->NewGlobalRef(j_observer_);
  }

  ~VideoDecodeEncodeObserver() {
    webrtc::AttachThreadScoped ats(g_vm);
    JNIEnv* jni = ats.env();
    jni->DeleteGlobalRef(j_observer_);
  }

  virtual void IncomingRate(const int video_channel,
                            const unsigned int framerate,
                            const unsigned int bitrate) {
    webrtc::AttachThreadScoped ats(g_vm);
    JNIEnv* jni = ats.env();
    jni->CallVoidMethod(j_observer_, incoming_rate_, video_channel,
                        static_cast<int>(framerate), static_cast<int>(bitrate));
  }

  virtual void DecoderTiming(int decode_ms, int max_decode_ms,
                             int current_delay_ms, int target_delay_ms,
                             int jitter_buffer_ms, int min_playout_delay_ms,
                             int render_delay_ms) {
    // TODO(fischman): consider plumbing this through to Java.
  }

  virtual void IncomingCodecChanged(const int video_channel,
                                    const webrtc::VideoCodec& video_codec) {
    webrtc::AttachThreadScoped ats(g_vm);
    JNIEnv* jni = ats.env();
    webrtc::VideoCodec* codec = new webrtc::VideoCodec(video_codec);
    jclass j_codec_class =
        GetClass("org/webrtc/webrtcdemo/VideoCodecInst");
    jmethodID j_codec_ctor = GetMethodID(jni, j_codec_class, "<init>", "(J)V");
    jobject j_codec =
        jni->NewObject(j_codec_class, j_codec_ctor, jlongFromPointer(codec));
    CHECK_EXCEPTION(jni, "error during NewObject");
    jni->CallVoidMethod(j_observer_, incoming_codec_changed_, video_channel,
                        j_codec);
  }

  virtual void RequestNewKeyFrame(const int video_channel) {
    webrtc::AttachThreadScoped ats(g_vm);
    JNIEnv* jni = ats.env();
    jni->CallVoidMethod(j_observer_, request_new_keyframe_, video_channel);
  }

  virtual void OutgoingRate(const int video_channel,
                            const unsigned int framerate,
                            const unsigned int bitrate) {
    webrtc::AttachThreadScoped ats(g_vm);
    JNIEnv* jni = ats.env();
    jni->CallVoidMethod(j_observer_, outgoing_rate_, video_channel,
                        static_cast<int>(framerate), static_cast<int>(bitrate));
  }

  virtual void SuspendChange(int video_channel, bool is_suspended) {}

 private:
  jobject j_observer_;
  jmethodID incoming_rate_;
  jmethodID incoming_codec_changed_;
  jmethodID request_new_keyframe_;
  jmethodID outgoing_rate_;
};

template<typename T>
void ReleaseSubApi(T instance) {
  CHECK(instance->Release() == 0, "failed to release instance")
}

class VideoEngineData {
 public:
  VideoEngineData()
      : vie(webrtc::VideoEngine::Create()),
        base(webrtc::ViEBase::GetInterface(vie)),
        codec(webrtc::ViECodec::GetInterface(vie)),
        network(webrtc::ViENetwork::GetInterface(vie)),
        rtp(webrtc::ViERTP_RTCP::GetInterface(vie)),
        render(webrtc::ViERender::GetInterface(vie)),
        capture(webrtc::ViECapture::GetInterface(vie)),
        externalCodec(webrtc::ViEExternalCodec::GetInterface(vie)) {
    CHECK(vie != NULL, "Video engine instance failed to be created");
    CHECK(base != NULL, "Failed to acquire base interface");
    CHECK(codec != NULL, "Failed to acquire codec interface");
    CHECK(network != NULL, "Failed to acquire network interface");
    CHECK(rtp != NULL, "Failed to acquire rtp interface");
    CHECK(render != NULL, "Failed to acquire render interface");
    CHECK(capture != NULL, "Failed to acquire capture interface");
    CHECK(externalCodec != NULL, "Failed to acquire externalCodec interface");
  }

  ~VideoEngineData() {
    CHECK(channel_transports_.empty(),
          "ViE transports must be deleted before terminating");
    CHECK(observers_.empty(),
          "ViE observers must be deleted before terminating");
    CHECK(external_decoders_.empty(),
          "ViE external decoders must be deleted before terminating");
    ReleaseSubApi(externalCodec);
    ReleaseSubApi(capture);
    ReleaseSubApi(render);
    ReleaseSubApi(rtp);
    ReleaseSubApi(network);
    ReleaseSubApi(codec);
    ReleaseSubApi(base);
    webrtc::VideoEngine* vie_pointer = vie;
    CHECK(webrtc::VideoEngine::Delete(vie_pointer), "ViE failed to be deleted");
  }

  int CreateChannel() {
    int channel;
    CHECK(base->CreateChannel(channel) == 0, "Failed to create channel");
    CreateTransport(channel);
    return channel;
  }

  int DeleteChannel(int channel) {
    if (base->DeleteChannel(channel) != 0) {
      return -1;
    }
    DeleteTransport(channel);
    return 0;
  }

  webrtc::test::VideoChannelTransport* GetTransport(int channel) {
    ChannelTransports::iterator found = channel_transports_.find(channel);
    if (found == channel_transports_.end()) {
      return NULL;
    }
    return found->second;
  }

  int RegisterObserver(int channel, jobject j_observer) {
    CHECK(observers_.find(channel) == observers_.end(),
          "Observer already created for channel, inconsistent state");
    observers_[channel] = new VideoDecodeEncodeObserver(j_observer);
    int ret_val = codec->RegisterDecoderObserver(channel, *observers_[channel]);
    ret_val |= codec->RegisterEncoderObserver(channel, *observers_[channel]);
    return ret_val;
  }

  int DeregisterObserver(int channel) {
    Observers::iterator found = observers_.find(channel);
    if (observers_.find(channel) == observers_.end()) {
      return -1;
    }
    int ret_val = codec->DeregisterDecoderObserver(channel);
    ret_val |= codec->DeregisterEncoderObserver(channel);
    delete found->second;
    observers_.erase(found);
    return ret_val;
  }

  int RegisterExternalReceiveCodec(jint channel, jint pl_type, jobject decoder,
                                   bool internal_source) {
    CHECK(external_decoders_.find(channel) == external_decoders_.end(),
          "External decoder already created for channel, inconsistent state");
    external_decoders_[channel] =
        new webrtc::MediaCodecVideoDecoder(g_vm, decoder);
    return externalCodec->RegisterExternalReceiveCodec(
        channel, pl_type, external_decoders_[channel], internal_source);
  }

  int DeRegisterExternalReceiveCodec(jint channel, jint pl_type) {
    ExternalDecoders::iterator found = external_decoders_.find(channel);
    CHECK(found != external_decoders_.end(),
          "ViE channel missing external decoder, inconsistent state");
    CHECK(externalCodec->DeRegisterExternalReceiveCodec(channel, pl_type) == 0,
          "Failed to register external receive decoder");
    delete found->second;
    external_decoders_.erase(found);
    return 0;
  }

  webrtc::VideoEngine* const vie;
  webrtc::ViEBase* const base;
  webrtc::ViECodec* const codec;
  webrtc::ViENetwork* const network;
  webrtc::ViERTP_RTCP* const rtp;
  webrtc::ViERender* const render;
  webrtc::ViECapture* const capture;
  webrtc::ViEExternalCodec* const externalCodec;

 private:
  // Video engine no longer provides a socket implementation. There is,
  // however, a socket implementation in webrtc::test.
  typedef std::map<int, webrtc::test::VideoChannelTransport*>
  ChannelTransports;
  typedef std::map<int, VideoDecodeEncodeObserver*> Observers;
  typedef std::map<int, webrtc::MediaCodecVideoDecoder*> ExternalDecoders;

  void CreateTransport(int channel) {
    CHECK(GetTransport(channel) == NULL,
          "Transport already created for ViE channel, inconsistent state");
    channel_transports_[channel] =
        new webrtc::test::VideoChannelTransport(network, channel);
  }
  void DeleteTransport(int channel) {
    CHECK(GetTransport(channel) != NULL,
          "ViE channel missing transport, inconsistent state");
    delete channel_transports_[channel];
    channel_transports_.erase(channel);
  }

  ChannelTransports channel_transports_;
  Observers observers_;
  ExternalDecoders external_decoders_;
};

webrtc::VideoCodec* GetCodecInst(JNIEnv* jni, jobject j_codec) {
  jclass j_codec_class = jni->GetObjectClass(j_codec);
  jfieldID native_codec_id =
      jni->GetFieldID(j_codec_class, "nativeCodecInst", "J");
  jlong j_p = jni->GetLongField(j_codec, native_codec_id);
  return reinterpret_cast<webrtc::VideoCodec*>(j_p);
}

CameraDesc* GetCameraDesc(JNIEnv* jni, jobject j_camera) {
  jclass j_camera_class = jni->GetObjectClass(j_camera);
  jfieldID native_camera_id =
      jni->GetFieldID(j_camera_class, "nativeCameraDesc", "J");
  jlong j_p = jni->GetLongField(j_camera, native_camera_id);
  return reinterpret_cast<CameraDesc*>(j_p);
}

VideoEngineData* GetVideoEngineData(JNIEnv* jni, jobject j_vie) {
  jclass j_vie_class = jni->GetObjectClass(j_vie);
  jfieldID native_vie_id =
      jni->GetFieldID(j_vie_class, "nativeVideoEngine", "J");
  jlong j_p = jni->GetLongField(j_vie, native_vie_id);
  return reinterpret_cast<VideoEngineData*>(j_p);
}

}  // namespace

namespace webrtc_examples {

static const char* g_classes[] = {
  "org/webrtc/webrtcdemo/CameraDesc",
  "org/webrtc/webrtcdemo/RtcpStatistics",
  "org/webrtc/webrtcdemo/VideoCodecInst",
  "org/webrtc/webrtcdemo/VideoDecodeEncodeObserver",
  "org/webrtc/webrtcdemo/MediaCodecVideoDecoder"};

void SetVieDeviceObjects(JavaVM* vm) {
  CHECK(vm, "Trying to register NULL vm");
  CHECK(!g_vm, "Trying to re-register vm");
  g_vm = vm;
  webrtc::AttachThreadScoped ats(g_vm);
  JNIEnv* jni = ats.env();
  g_class_reference_holder = new ClassReferenceHolder(
      jni, g_classes, ARRAYSIZE(g_classes));
}

void ClearVieDeviceObjects() {
  CHECK(g_vm, "Clearing vm without it being set");
  {
    webrtc::AttachThreadScoped ats(g_vm);
    g_class_reference_holder->FreeReferences(ats.env());
  }
  g_vm = NULL;
  delete g_class_reference_holder;
  g_class_reference_holder = NULL;
}

}  // namespace webrtc_examples

JOWW(jlong, VideoEngine_create)(JNIEnv* jni, jclass) {
  VideoEngineData* vie_data = new VideoEngineData();
  return jlongFromPointer(vie_data);
}

JOWW(jint, VideoEngine_init)(JNIEnv* jni, jobject j_vie) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->base->Init();
}

JOWW(jint, VideoEngine_setVoiceEngine)(JNIEnv* jni, jobject j_vie,
                                       jobject j_voe) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  webrtc::VoiceEngine* voe = GetVoiceEngine(jni, j_voe);
  return vie_data->base->SetVoiceEngine(voe);
}

JOWW(void, VideoEngine_dispose)(JNIEnv* jni, jobject j_vie) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  delete vie_data;
}

JOWW(jint, VideoEngine_startSend)(JNIEnv* jni, jobject j_vie, jint channel) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->base->StartSend(channel);
}

JOWW(jint, VideoEngine_stopRender)(JNIEnv* jni, jobject j_vie, jint channel) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->render->StopRender(channel);
}

JOWW(jint, VideoEngine_stopSend)(JNIEnv* jni, jobject j_vie, jint channel) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->base->StopSend(channel);
}

JOWW(jint, VideoEngine_startReceive)(JNIEnv* jni, jobject j_vie, jint channel) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->base->StartReceive(channel);
}

JOWW(jint, VideoEngine_stopReceive)(JNIEnv* jni, jobject j_vie, jint channel) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->base->StopReceive(channel);
}

JOWW(jint, VideoEngine_createChannel)(JNIEnv* jni, jobject j_vie) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->CreateChannel();
}

JOWW(jint, VideoEngine_deleteChannel)(JNIEnv* jni, jobject j_vie,
                                      jint channel) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->DeleteChannel(channel);
}

JOWW(jint,
     VideoEngine_connectAudioChannel(JNIEnv* jni, jobject j_vie,
                                     jint video_channel, jint audio_channel)) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->base->ConnectAudioChannel(video_channel, audio_channel);
}

JOWW(jint, VideoEngine_setLocalReceiver)(JNIEnv* jni, jobject j_vie,
                                         jint channel, jint port) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->GetTransport(channel)->SetLocalReceiver(port);
}

JOWW(jint, VideoEngine_setSendDestination)(JNIEnv* jni, jobject j_vie,
                                           jint channel, jint port,
                                           jstring j_addr) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  std::string addr = JavaToStdString(jni, j_addr);
  webrtc::test::VideoChannelTransport* transport =
      vie_data->GetTransport(channel);
  return transport->SetSendDestination(addr.c_str(), port);
}

JOWW(jint, VideoEngine_setReceiveCodec)(JNIEnv* jni, jobject j_vie,
                                        jint channel, jobject j_codec) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  webrtc::VideoCodec* codec = GetCodecInst(jni, j_codec);
  return vie_data->codec->SetReceiveCodec(channel, *codec);
}

JOWW(jint, VideoEngine_setSendCodec)(JNIEnv* jni, jobject j_vie, jint channel,
                                     jobject j_codec) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  webrtc::VideoCodec* codec = GetCodecInst(jni, j_codec);
  return vie_data->codec->SetSendCodec(channel, *codec);
}

JOWW(jint, VideoEngine_numberOfCodecs)(JNIEnv* jni, jobject j_vie) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->codec->NumberOfCodecs();
}

JOWW(jobject, VideoEngine_getCodec)(JNIEnv* jni, jobject j_vie, jint index) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  webrtc::VideoCodec* codec = new webrtc::VideoCodec();
  CHECK(vie_data->codec->GetCodec(index, *codec) == 0,
        "getCodec must be called with valid index");
  jclass j_codec_class = GetClass("org/webrtc/webrtcdemo/VideoCodecInst");
  jmethodID j_codec_ctor = GetMethodID(jni, j_codec_class, "<init>", "(J)V");
  jobject j_codec =
      jni->NewObject(j_codec_class, j_codec_ctor, jlongFromPointer(codec));
  CHECK_EXCEPTION(jni, "error during NewObject");
  return j_codec;
}

JOWW(jint, VideoEngine_addRenderer)(JNIEnv* jni, jobject j_vie, jint channel,
                                    jobject gl_surface, jint z_order,
                                    jfloat left, jfloat top, jfloat right,
                                    jfloat bottom) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->render->AddRenderer(channel, gl_surface, z_order, left, top,
                                       right, bottom);
}

JOWW(jint, VideoEngine_removeRenderer)(JNIEnv* jni, jobject j_vie,
                                       jint channel) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->render->RemoveRenderer(channel);
}

JOWW(jint, VideoEngine_registerExternalReceiveCodec)(JNIEnv* jni, jobject j_vie,
                                                     jint channel, jint pl_type,
                                                     jobject decoder,
                                                     bool internal_source) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->RegisterExternalReceiveCodec(channel, pl_type, decoder,
                                                true);
}

JOWW(jint,
     VideoEngine_deRegisterExternalReceiveCodec)(JNIEnv* jni, jobject j_vie,
                                                 jint channel, jint pl_type) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->DeRegisterExternalReceiveCodec(channel, pl_type);
}

JOWW(jint, VideoEngine_startRender)(JNIEnv* jni, jobject j_vie, jint channel) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->render->StartRender(channel);
}

JOWW(jint, VideoEngine_numberOfCaptureDevices)(JNIEnv* jni, jobject j_vie) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->capture->NumberOfCaptureDevices();
}

JOWW(jobject,
     VideoEngine_getCaptureDevice(JNIEnv* jni, jobject j_vie, jint index)) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  CameraDesc* camera_info = new CameraDesc();
  if (vie_data->capture->GetCaptureDevice(
          index, camera_info->name, sizeof(camera_info->name),
          camera_info->unique_id, sizeof(camera_info->unique_id)) != 0) {
    delete camera_info;
    return NULL;
  }
  jclass j_camera_class = GetClass("org/webrtc/webrtcdemo/CameraDesc");
  jmethodID j_camera_ctor = GetMethodID(jni, j_camera_class, "<init>", "(J)V");
  jobject j_camera = jni->NewObject(j_camera_class, j_camera_ctor,
                                    jlongFromPointer(camera_info));
  CHECK_EXCEPTION(jni, "error during NewObject");
  return j_camera;
}

JOWW(jint, VideoEngine_allocateCaptureDevice)(JNIEnv* jni, jobject j_vie,
                                              jobject j_camera) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  CameraDesc* camera_info = GetCameraDesc(jni, j_camera);
  jint capture_id;
  if (vie_data->capture->AllocateCaptureDevice(camera_info->unique_id,
                                               sizeof(camera_info->unique_id),
                                               capture_id) != 0) {
    return -1;
  }
  return capture_id;
}

JOWW(jint, VideoEngine_connectCaptureDevice)(JNIEnv* jni, jobject j_vie,
                                             jint camera_num, jint channel) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->capture->ConnectCaptureDevice(camera_num, channel);
}

JOWW(jint, VideoEngine_startCapture)(JNIEnv* jni, jobject j_vie,
                                     jint camera_num) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->capture->StartCapture(camera_num);
}

JOWW(jint, VideoEngine_stopCapture)(JNIEnv* jni, jobject j_vie,
                                    jint camera_id) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->capture->StopCapture(camera_id);
}

JOWW(jint, VideoEngine_releaseCaptureDevice)(JNIEnv* jni, jobject j_vie,
                                             jint camera_id) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->capture->ReleaseCaptureDevice(camera_id);
}

JOWW(jint, VideoEngine_getOrientation)(JNIEnv* jni, jobject j_vie,
                                       jobject j_camera) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  CameraDesc* camera_info = GetCameraDesc(jni, j_camera);
  webrtc::RotateCapturedFrame orientation;
  if (vie_data->capture->GetOrientation(camera_info->unique_id, orientation) !=
      0) {
    return -1;
  }
  return static_cast<jint>(orientation);
}

JOWW(jint, VideoEngine_setRotateCapturedFrames)(JNIEnv* jni, jobject j_vie,
                                                jint capture_id, jint degrees) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->capture->SetRotateCapturedFrames(
      capture_id, static_cast<webrtc::RotateCapturedFrame>(degrees));
}

JOWW(jint, VideoEngine_setNackStatus)(JNIEnv* jni, jobject j_vie, jint channel,
                                      jboolean enable) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->rtp->SetNACKStatus(channel, enable);
}

JOWW(jint, VideoEngine_setKeyFrameRequestMethod)(JNIEnv* jni, jobject j_vie,
                                                 jint channel,
                                                 jint request_method) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->rtp->SetKeyFrameRequestMethod(
      channel, static_cast<webrtc::ViEKeyFrameRequestMethod>(request_method));
}

JOWW(jobject, VideoEngine_getReceivedRtcpStatistics)(JNIEnv* jni, jobject j_vie,
                                                     jint channel) {
  unsigned short fraction_lost;  // NOLINT
  unsigned int cumulative_lost;  // NOLINT
  unsigned int extended_max;     // NOLINT
  unsigned int jitter;           // NOLINT
  int rtt_ms;
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  if (vie_data->rtp->GetReceivedRTCPStatistics(channel, fraction_lost,
                                               cumulative_lost, extended_max,
                                               jitter, rtt_ms) != 0) {
    return NULL;
  }
  jclass j_rtcp_statistics_class =
      GetClass("org/webrtc/webrtcdemo/RtcpStatistics");
  jmethodID j_rtcp_statistics_ctor =
      GetMethodID(jni, j_rtcp_statistics_class, "<init>", "(IIIII)V");
  jobject j_rtcp_statistics =
      jni->NewObject(j_rtcp_statistics_class, j_rtcp_statistics_ctor,
                     fraction_lost, cumulative_lost, extended_max, jitter,
                     rtt_ms);
  CHECK_EXCEPTION(jni, "error during NewObject");
  return j_rtcp_statistics;
}

JOWW(jint, VideoEngine_registerObserver)(JNIEnv* jni, jobject j_vie,
                                         jint channel, jobject callback) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->RegisterObserver(channel, callback);
}

JOWW(jint, VideoEngine_deregisterObserver)(JNIEnv* jni, jobject j_vie,
                                           jint channel) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->DeregisterObserver(channel);
}

JOWW(jint, VideoEngine_setTraceFile)(JNIEnv* jni, jobject, jstring j_filename,
                                     jboolean file_counter) {
  std::string filename = JavaToStdString(jni, j_filename);
  return webrtc::VideoEngine::SetTraceFile(filename.c_str(), file_counter);
}

JOWW(jint, VideoEngine_nativeSetTraceFilter)(JNIEnv* jni, jobject,
                                             jint filter) {
  return webrtc::VideoEngine::SetTraceFilter(filter);
}

JOWW(jint, VideoEngine_startRtpDump)(JNIEnv* jni, jobject j_vie, jint channel,
                                     jstring j_filename, jint direction) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  std::string filename = JavaToStdString(jni, j_filename);
  return vie_data->rtp->StartRTPDump(
      channel, filename.c_str(), static_cast<webrtc::RTPDirections>(direction));
}

JOWW(jint, VideoEngine_stopRtpDump)(JNIEnv* jni, jobject j_vie, jint channel,
                                    jint direction) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->rtp->StopRTPDump(
      channel, static_cast<webrtc::RTPDirections>(direction));
}

JOWW(void, VideoCodecInst_dispose)(JNIEnv* jni, jobject j_codec) {
  delete GetCodecInst(jni, j_codec);
}

JOWW(jint, VideoCodecInst_plType)(JNIEnv* jni, jobject j_codec) {
  return GetCodecInst(jni, j_codec)->plType;
}

JOWW(jstring, VideoCodecInst_name)(JNIEnv* jni, jobject j_codec) {
  return jni->NewStringUTF(GetCodecInst(jni, j_codec)->plName);
}

JOWW(jint, VideoCodecInst_width)(JNIEnv* jni, jobject j_codec) {
  return GetCodecInst(jni, j_codec)->width;
}

JOWW(void, VideoCodecInst_setWidth)(JNIEnv* jni, jobject j_codec, jint width) {
  GetCodecInst(jni, j_codec)->width = width;
}

JOWW(jint, VideoCodecInst_height)(JNIEnv* jni, jobject j_codec) {
  return GetCodecInst(jni, j_codec)->height;
}

JOWW(void, VideoCodecInst_setHeight)(JNIEnv* jni, jobject j_codec,
                                     jint height) {
  GetCodecInst(jni, j_codec)->height = height;
}

JOWW(jint, VideoCodecInst_startBitRate)(JNIEnv* jni, jobject j_codec) {
  return GetCodecInst(jni, j_codec)->startBitrate;
}

JOWW(void, VideoCodecInst_setStartBitRate)(JNIEnv* jni, jobject j_codec,
                                           jint bitrate) {
  GetCodecInst(jni, j_codec)->startBitrate = bitrate;
}

JOWW(jint, VideoCodecInst_maxBitRate)(JNIEnv* jni, jobject j_codec) {
  return GetCodecInst(jni, j_codec)->maxBitrate;
}

JOWW(void, VideoCodecInst_setMaxBitRate)(JNIEnv* jni, jobject j_codec,
                                         jint bitrate) {
  GetCodecInst(jni, j_codec)->maxBitrate = bitrate;
}

JOWW(jint, VideoCodecInst_maxFrameRate)(JNIEnv* jni, jobject j_codec) {
  return GetCodecInst(jni, j_codec)->maxFramerate;
}

JOWW(void, VideoCodecInst_setMaxFrameRate)(JNIEnv* jni, jobject j_codec,
                                           jint framerate) {
  GetCodecInst(jni, j_codec)->maxFramerate = framerate;
}

JOWW(void, CameraDesc_dispose)(JNIEnv* jni, jobject j_camera) {
  delete GetCameraDesc(jni, j_camera);
}

JOWW(jint, VideoEngine_setLocalSSRC)(JNIEnv* jni, jobject j_vie, jint channel,
                                      jint ssrc) {
  VideoEngineData* vie_data = GetVideoEngineData(jni, j_vie);
  return vie_data->rtp->SetLocalSSRC(channel, ssrc);
}
