/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains JNI for the voice engine interfaces.
// The native functions are found using jni's auto discovery.

#include "webrtc/examples/android/media_demo/jni/voice_engine_jni.h"

#include <map>
#include <string>

#include "webrtc/base/arraysize.h"
#include "webrtc/examples/android/media_demo/jni/jni_helpers.h"
#include "webrtc/modules/utility/include/helpers_android.h"
#include "webrtc/test/channel_transport/include/channel_transport.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_file.h"
#include "webrtc/voice_engine/include/voe_hardware.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"

// Macro for native functions that can be found by way of jni-auto discovery.
// Note extern "C" is needed for "discovery" of native methods to work.
#define JOWW(rettype, name)                                             \
  extern "C" rettype JNIEXPORT JNICALL Java_org_webrtc_webrtcdemo_##name

namespace {

static JavaVM* g_vm = NULL;
static ClassReferenceHolder* g_class_reference_holder = NULL;

jclass GetClass(JNIEnv* jni, const char* name) {
  CHECK(g_class_reference_holder, "Class reference holder NULL");
  return g_class_reference_holder->GetClass(name);
}

static const char* g_classes[] = {"org/webrtc/webrtcdemo/CodecInst"};

template<typename T>
void ReleaseSubApi(T instance) {
  CHECK(instance->Release() >= 0, "failed to release instance")
}

class VoiceEngineData {
 public:
  VoiceEngineData()
      : ve(webrtc::VoiceEngine::Create()),
        base(webrtc::VoEBase::GetInterface(ve)),
        codec(webrtc::VoECodec::GetInterface(ve)),
        file(webrtc::VoEFile::GetInterface(ve)),
        netw(webrtc::VoENetwork::GetInterface(ve)),
        apm(webrtc::VoEAudioProcessing::GetInterface(ve)),
        volume(webrtc::VoEVolumeControl::GetInterface(ve)),
        hardware(webrtc::VoEHardware::GetInterface(ve)),
        rtp(webrtc::VoERTP_RTCP::GetInterface(ve)) {
    CHECK(ve != NULL, "Voice engine instance failed to be created");
    CHECK(base != NULL, "Failed to acquire base interface");
    CHECK(codec != NULL, "Failed to acquire codec interface");
    CHECK(file != NULL, "Failed to acquire file interface");
    CHECK(netw != NULL, "Failed to acquire netw interface");
    CHECK(apm != NULL, "Failed to acquire apm interface");
    CHECK(volume != NULL, "Failed to acquire volume interface");
    CHECK(hardware != NULL, "Failed to acquire hardware interface");
    CHECK(rtp != NULL, "Failed to acquire rtp interface");
  }

  ~VoiceEngineData() {
    CHECK(channel_transports_.empty(),
          "VoE transports must be deleted before terminating");
    CHECK(base->Terminate() == 0, "VoE failed to terminate");
    ReleaseSubApi(base);
    ReleaseSubApi(codec);
    ReleaseSubApi(file);
    ReleaseSubApi(netw);
    ReleaseSubApi(apm);
    ReleaseSubApi(volume);
    ReleaseSubApi(hardware);
    ReleaseSubApi(rtp);
    webrtc::VoiceEngine* ve_instance = ve;
    CHECK(webrtc::VoiceEngine::Delete(ve_instance), "VoE failed to be deleted");
  }

  int CreateChannel() {
    int channel = base->CreateChannel();
    if (channel == -1) {
      return -1;
    }
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

  webrtc::test::VoiceChannelTransport* GetTransport(int channel) {
    ChannelTransports::iterator found = channel_transports_.find(channel);
    if (found == channel_transports_.end()) {
      return NULL;
    }
    return found->second;
  }

  webrtc::VoiceEngine* const ve;
  webrtc::VoEBase* const base;
  webrtc::VoECodec* const codec;
  webrtc::VoEFile* const file;
  webrtc::VoENetwork* const netw;
  webrtc::VoEAudioProcessing* const apm;
  webrtc::VoEVolumeControl* const volume;
  webrtc::VoEHardware* const hardware;
  webrtc::VoERTP_RTCP* const rtp;

 private:
  // Voice engine no longer provides a socket implementation. There is,
  // however, a socket implementation in webrtc::test.
  typedef std::map<int, webrtc::test::VoiceChannelTransport*>
  ChannelTransports;

  void CreateTransport(int channel) {
    CHECK(GetTransport(channel) == NULL,
          "Transport already created for VoE channel, inconsistent state");
    channel_transports_[channel] =
        new webrtc::test::VoiceChannelTransport(netw, channel);
  }
  void DeleteTransport(int channel) {
    CHECK(GetTransport(channel) != NULL,
          "VoE channel missing transport, inconsistent state");
    delete channel_transports_[channel];
    channel_transports_.erase(channel);
  }

  ChannelTransports channel_transports_;
};

webrtc::CodecInst* GetCodecInst(JNIEnv* jni, jobject j_codec) {
  jclass j_codec_class = jni->GetObjectClass(j_codec);
  jfieldID native_codec_id =
      jni->GetFieldID(j_codec_class, "nativeCodecInst", "J");
  jlong j_p = jni->GetLongField(j_codec, native_codec_id);
  return reinterpret_cast<webrtc::CodecInst*>(j_p);
}

}  // namespace

namespace webrtc_examples {

void SetVoeDeviceObjects(JavaVM* vm) {
  CHECK(vm, "Trying to register NULL vm");
  g_vm = vm;
  webrtc::AttachThreadScoped ats(g_vm);
  JNIEnv* jni = ats.env();
  g_class_reference_holder = new ClassReferenceHolder(
      jni, g_classes, arraysize(g_classes));
}

void ClearVoeDeviceObjects() {
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

VoiceEngineData* GetVoiceEngineData(JNIEnv* jni, jobject j_voe) {
  jclass j_voe_class = jni->GetObjectClass(j_voe);
  jfieldID native_voe_id =
      jni->GetFieldID(j_voe_class, "nativeVoiceEngine", "J");
  jlong j_p = jni->GetLongField(j_voe, native_voe_id);
  return reinterpret_cast<VoiceEngineData*>(j_p);
}

webrtc::VoiceEngine* GetVoiceEngine(JNIEnv* jni, jobject j_voe) {
  return GetVoiceEngineData(jni, j_voe)->ve;
}

JOWW(jlong, VoiceEngine_create)(JNIEnv* jni, jclass) {
  VoiceEngineData* voe_data = new VoiceEngineData();
  return jlongFromPointer(voe_data);
}

JOWW(void, VoiceEngine_dispose)(JNIEnv* jni, jobject j_voe) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  delete voe_data;
}

JOWW(jint, VoiceEngine_init)(JNIEnv* jni, jobject j_voe) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->base->Init();
}

JOWW(jint, VoiceEngine_createChannel)(JNIEnv* jni, jobject j_voe) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->CreateChannel();
}

JOWW(jint, VoiceEngine_deleteChannel)(JNIEnv* jni, jobject j_voe,
                                      jint channel) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->DeleteChannel(channel);
}

JOWW(jint, VoiceEngine_setLocalReceiver)(JNIEnv* jni, jobject j_voe,
                                         jint channel, jint port) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  webrtc::test::VoiceChannelTransport* transport =
      voe_data->GetTransport(channel);
  return transport->SetLocalReceiver(port);
}

JOWW(jint, VoiceEngine_setSendDestination)(JNIEnv* jni, jobject j_voe,
                                           jint channel, jint port,
                                           jstring j_addr) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  std::string addr = JavaToStdString(jni, j_addr);
  webrtc::test::VoiceChannelTransport* transport =
      voe_data->GetTransport(channel);
  return transport->SetSendDestination(addr.c_str(), port);
}

JOWW(jint, VoiceEngine_startListen)(JNIEnv* jni, jobject j_voe, jint channel) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->base->StartReceive(channel);
}

JOWW(jint, VoiceEngine_startPlayout)(JNIEnv* jni, jobject j_voe, jint channel) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->base->StartPlayout(channel);
}

JOWW(jint, VoiceEngine_startSend)(JNIEnv* jni, jobject j_voe, jint channel) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->base->StartSend(channel);
}

JOWW(jint, VoiceEngine_stopListen)(JNIEnv* jni, jobject j_voe, jint channel) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->base->StartReceive(channel);
}

JOWW(jint, VoiceEngine_stopPlayout)(JNIEnv* jni, jobject j_voe, jint channel) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->base->StopPlayout(channel);
}

JOWW(jint, VoiceEngine_stopSend)(JNIEnv* jni, jobject j_voe, jint channel) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->base->StopSend(channel);
}

JOWW(jint, VoiceEngine_setSpeakerVolume)(JNIEnv* jni, jobject j_voe,
                                         jint level) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->volume->SetSpeakerVolume(level);
}

JOWW(jint, VoiceEngine_startPlayingFileLocally)(JNIEnv* jni, jobject j_voe,
                                                jint channel,
                                                jstring j_filename,
                                                jboolean loop) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  std::string filename = JavaToStdString(jni, j_filename);
  return voe_data->file->StartPlayingFileLocally(channel,
                                                 filename.c_str(),
                                                 loop);
}

JOWW(jint, VoiceEngine_stopPlayingFileLocally)(JNIEnv* jni, jobject j_voe,
                                               jint channel) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->file->StopPlayingFileLocally(channel);
}

JOWW(jint, VoiceEngine_startPlayingFileAsMicrophone)(JNIEnv* jni, jobject j_voe,
                                                     jint channel,
                                                     jstring j_filename,
                                                     jboolean loop) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  std::string filename = JavaToStdString(jni, j_filename);
  return voe_data->file->StartPlayingFileAsMicrophone(channel,
                                                      filename.c_str(),
                                                      loop);
}

JOWW(jint, VoiceEngine_stopPlayingFileAsMicrophone)(JNIEnv* jni, jobject j_voe,
                                                    jint channel) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->file->StopPlayingFileAsMicrophone(channel);
}

JOWW(jint, VoiceEngine_numOfCodecs)(JNIEnv* jni, jobject j_voe) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->codec->NumOfCodecs();
}

JOWW(jobject, VoiceEngine_getCodec)(JNIEnv* jni, jobject j_voe, jint index) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  webrtc::CodecInst* codec = new webrtc::CodecInst();
  CHECK(voe_data->codec->GetCodec(index, *codec) == 0,
        "getCodec must be called with valid index");
  jclass j_codec_class = GetClass(jni, "org/webrtc/webrtcdemo/CodecInst");
  jmethodID j_codec_ctor = GetMethodID(jni, j_codec_class, "<init>", "(J)V");
  jobject j_codec =
      jni->NewObject(j_codec_class, j_codec_ctor, jlongFromPointer(codec));
  CHECK_JNI_EXCEPTION(jni, "error during NewObject");
  return j_codec;
}

JOWW(jint, VoiceEngine_setSendCodec)(JNIEnv* jni, jobject j_voe, jint channel,
                                     jobject j_codec) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  webrtc::CodecInst* inst = GetCodecInst(jni, j_codec);
  return voe_data->codec->SetSendCodec(channel, *inst);
}

JOWW(jint, VoiceEngine_setEcStatus)(JNIEnv* jni, jobject j_voe, jboolean enable,
                                    jint ec_mode) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->apm->SetEcStatus(enable,
                                    static_cast<webrtc::EcModes>(ec_mode));
}

JOWW(jint, VoiceEngine_setAecmMode)(JNIEnv* jni, jobject j_voe, jint aecm_mode,
                                    jboolean cng) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->apm->SetAecmMode(static_cast<webrtc::AecmModes>(aecm_mode),
                                    cng);
}

JOWW(jint, VoiceEngine_setAgcStatus)(JNIEnv* jni, jobject j_voe,
                                     jboolean enable, jint agc_mode) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->apm->SetAgcStatus(enable,
                                     static_cast<webrtc::AgcModes>(agc_mode));
}

// Returns the native AgcConfig object associated with the Java object
// |j_codec|.
void GetNativeAgcConfig(JNIEnv* jni, jobject j_codec,
                        webrtc::AgcConfig* agc_config) {
  jclass j_codec_class = jni->GetObjectClass(j_codec);
  jfieldID dBOv_id = jni->GetFieldID(j_codec_class, "targetLevelDbOv", "I");
  agc_config->targetLeveldBOv = jni->GetIntField(j_codec, dBOv_id);
  jfieldID gain_id =
      jni->GetFieldID(j_codec_class, "digitalCompressionGaindB", "I");
  agc_config->digitalCompressionGaindB = jni->GetIntField(j_codec, gain_id);
  jfieldID limiter_id = jni->GetFieldID(j_codec_class, "limiterEnable", "Z");
  agc_config->limiterEnable = jni->GetBooleanField(j_codec, limiter_id);
}

JOWW(jint, VoiceEngine_setAgcConfig)(JNIEnv* jni, jobject j_voe,
                                     jobject j_config) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  webrtc::AgcConfig config;
  GetNativeAgcConfig(jni, j_config, &config);
  return voe_data->apm->SetAgcConfig(config);
}

JOWW(jint, VoiceEngine_setNsStatus)(JNIEnv* jni, jobject j_voe, jboolean enable,
                                    jint ns_mode) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->apm->SetNsStatus(enable,
                                    static_cast<webrtc::NsModes>(ns_mode));
}

JOWW(jint, VoiceEngine_startDebugRecording)(JNIEnv* jni, jobject j_voe,
                                            jstring j_filename) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  std::string filename = JavaToStdString(jni, j_filename);
  return voe_data->apm->StartDebugRecording(filename.c_str());
}

JOWW(jint, VoiceEngine_stopDebugRecording)(JNIEnv* jni, jobject j_voe) {
  VoiceEngineData* voe_data = GetVoiceEngineData(jni, j_voe);
  return voe_data->apm->StopDebugRecording();
}

JOWW(void, CodecInst_dispose)(JNIEnv* jni, jobject j_codec) {
  delete GetCodecInst(jni, j_codec);
}

JOWW(jint, CodecInst_plType)(JNIEnv* jni, jobject j_codec) {
  return GetCodecInst(jni, j_codec)->pltype;
}

JOWW(jstring, CodecInst_name)(JNIEnv* jni, jobject j_codec) {
  return jni->NewStringUTF(GetCodecInst(jni, j_codec)->plname);
}

JOWW(jint, CodecInst_plFrequency)(JNIEnv* jni, jobject j_codec) {
  return GetCodecInst(jni, j_codec)->plfreq;
}

JOWW(jint, CodecInst_pacSize)(JNIEnv* jni, jobject j_codec) {
  return GetCodecInst(jni, j_codec)->pacsize;
}

JOWW(jint, CodecInst_channels)(JNIEnv* jni, jobject j_codec) {
  return GetCodecInst(jni, j_codec)->channels;
}

JOWW(jint, CodecInst_rate)(JNIEnv* jni, jobject j_codec) {
  return GetCodecInst(jni, j_codec)->rate;
}
