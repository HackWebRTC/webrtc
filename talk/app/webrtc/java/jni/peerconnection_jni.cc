/*
 * libjingle
 * Copyright 2013 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Hints for future visitors:
// This entire file is an implementation detail of the org.webrtc Java package,
// the most interesting bits of which are org.webrtc.PeerConnection{,Factory}.
// The layout of this file is roughly:
// - various helper C++ functions & classes that wrap Java counterparts and
//   expose a C++ interface that can be passed to the C++ PeerConnection APIs
// - implementations of methods declared "static" in the Java package (named
//   things like Java_org_webrtc_OMG_Can_This_Name_Be_Any_Longer, prescribed by
//   the JNI spec).
//
// Lifecycle notes: objects are owned where they will be called; in other words
// FooObservers are owned by C++-land, and user-callable objects (e.g.
// PeerConnection and VideoTrack) are owned by Java-land.
// When this file allocates C++ RefCountInterfaces it AddRef()s an artificial
// ref simulating the jlong held in Java-land, and then Release()s the ref in
// the respective free call.  Sometimes this AddRef is implicit in the
// construction of a scoped_refptr<> which is then .release()d.
// Any persistent (non-local) references from C++ to Java must be global or weak
// (in which case they must be checked before use)!
//
// Exception notes: pretty much all JNI calls can throw Java exceptions, so each
// call through a JNIEnv* pointer needs to be followed by an ExceptionCheck()
// call.  In this file this is done in CHECK_EXCEPTION, making for much easier
// debugging in case of failure (the alternative is to wait for control to
// return to the Java frame that called code in this file, at which point it's
// impossible to tell which JNI call broke).

#include <jni.h>
#undef JNIEXPORT
#define JNIEXPORT __attribute__((visibility("default")))

#include <limits>

#include "talk/app/webrtc/java/jni/classreferenceholder.h"
#include "talk/app/webrtc/java/jni/jni_helpers.h"
#include "talk/app/webrtc/java/jni/native_handle_impl.h"
#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videorenderer.h"
#include "talk/media/devices/videorendererfactory.h"
#include "talk/media/webrtc/webrtcvideocapturer.h"
#include "talk/media/webrtc/webrtcvideodecoderfactory.h"
#include "talk/media/webrtc/webrtcvideoencoderfactory.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/system_wrappers/interface/field_trial_default.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/voice_engine/include/voe_base.h"

#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
#include "talk/app/webrtc/androidvideocapturer.h"
#include "talk/app/webrtc/java/jni/androidmediadecoder_jni.h"
#include "talk/app/webrtc/java/jni/androidmediaencoder_jni.h"
#include "talk/app/webrtc/java/jni/androidvideocapturer_jni.h"
#include "webrtc/modules/video_render/video_render_internal.h"
#include "webrtc/system_wrappers/interface/logcat_trace_context.h"
using webrtc::LogcatTraceContext;
#endif

using rtc::Bind;
using rtc::Thread;
using rtc::ThreadManager;
using rtc::scoped_ptr;
using webrtc::AudioSourceInterface;
using webrtc::AudioTrackInterface;
using webrtc::AudioTrackVector;
using webrtc::CreateSessionDescriptionObserver;
using webrtc::DataBuffer;
using webrtc::DataChannelInit;
using webrtc::DataChannelInterface;
using webrtc::DataChannelObserver;
using webrtc::IceCandidateInterface;
using webrtc::NativeHandle;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;
using webrtc::MediaStreamInterface;
using webrtc::MediaStreamTrackInterface;
using webrtc::PeerConnectionFactoryInterface;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionObserver;
using webrtc::SessionDescriptionInterface;
using webrtc::SetSessionDescriptionObserver;
using webrtc::StatsObserver;
using webrtc::StatsReport;
using webrtc::StatsReports;
using webrtc::VideoRendererInterface;
using webrtc::VideoSourceInterface;
using webrtc::VideoTrackInterface;
using webrtc::VideoTrackVector;
using webrtc::kVideoCodecVP8;

namespace webrtc_jni {

// Field trials initialization string
static char *field_trials_init_string = NULL;

#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
// Set in PeerConnectionFactory_initializeAndroidGlobals().
static bool factory_static_initialized = false;
static bool vp8_hw_acceleration_enabled = true;
#endif

extern "C" jint JNIEXPORT JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved) {
  jint ret = InitGlobalJniVariables(jvm);
  if (ret < 0)
    return -1;

  CHECK(rtc::InitializeSSL()) << "Failed to InitializeSSL()";
  LoadGlobalClassReferenceHolder();

  return ret;
}

extern "C" void JNIEXPORT JNICALL JNI_OnUnLoad(JavaVM *jvm, void *reserved) {
  FreeGlobalClassReferenceHolder();
  CHECK(rtc::CleanupSSL()) << "Failed to CleanupSSL()";
}

// Return the (singleton) Java Enum object corresponding to |index|;
// |state_class_fragment| is something like "MediaSource$State".
static jobject JavaEnumFromIndex(
    JNIEnv* jni, const std::string& state_class_fragment, int index) {
  const std::string state_class = "org/webrtc/" + state_class_fragment;
  return JavaEnumFromIndex(jni, FindClass(jni, state_class.c_str()),
                           state_class, index);
}

static DataChannelInit JavaDataChannelInitToNative(
    JNIEnv* jni, jobject j_init) {
  DataChannelInit init;

  jclass j_init_class = FindClass(jni, "org/webrtc/DataChannel$Init");
  jfieldID ordered_id = GetFieldID(jni, j_init_class, "ordered", "Z");
  jfieldID max_retransmit_time_id =
      GetFieldID(jni, j_init_class, "maxRetransmitTimeMs", "I");
  jfieldID max_retransmits_id =
      GetFieldID(jni, j_init_class, "maxRetransmits", "I");
  jfieldID protocol_id =
      GetFieldID(jni, j_init_class, "protocol", "Ljava/lang/String;");
  jfieldID negotiated_id = GetFieldID(jni, j_init_class, "negotiated", "Z");
  jfieldID id_id = GetFieldID(jni, j_init_class, "id", "I");

  init.ordered = GetBooleanField(jni, j_init, ordered_id);
  init.maxRetransmitTime = GetIntField(jni, j_init, max_retransmit_time_id);
  init.maxRetransmits = GetIntField(jni, j_init, max_retransmits_id);
  init.protocol = JavaToStdString(
      jni, GetStringField(jni, j_init, protocol_id));
  init.negotiated = GetBooleanField(jni, j_init, negotiated_id);
  init.id = GetIntField(jni, j_init, id_id);

  return init;
}

class ConstraintsWrapper;

// Adapter between the C++ PeerConnectionObserver interface and the Java
// PeerConnection.Observer interface.  Wraps an instance of the Java interface
// and dispatches C++ callbacks to Java.
class PCOJava : public PeerConnectionObserver {
 public:
  PCOJava(JNIEnv* jni, jobject j_observer)
      : j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, *j_observer_global_)),
        j_media_stream_class_(jni, FindClass(jni, "org/webrtc/MediaStream")),
        j_media_stream_ctor_(GetMethodID(
            jni, *j_media_stream_class_, "<init>", "(J)V")),
        j_audio_track_class_(jni, FindClass(jni, "org/webrtc/AudioTrack")),
        j_audio_track_ctor_(GetMethodID(
            jni, *j_audio_track_class_, "<init>", "(J)V")),
        j_video_track_class_(jni, FindClass(jni, "org/webrtc/VideoTrack")),
        j_video_track_ctor_(GetMethodID(
            jni, *j_video_track_class_, "<init>", "(J)V")),
        j_data_channel_class_(jni, FindClass(jni, "org/webrtc/DataChannel")),
        j_data_channel_ctor_(GetMethodID(
            jni, *j_data_channel_class_, "<init>", "(J)V")) {
  }

  virtual ~PCOJava() {}

  void OnIceCandidate(const IceCandidateInterface* candidate) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    std::string sdp;
    CHECK(candidate->ToString(&sdp)) << "got so far: " << sdp;
    jclass candidate_class = FindClass(jni(), "org/webrtc/IceCandidate");
    jmethodID ctor = GetMethodID(jni(), candidate_class,
        "<init>", "(Ljava/lang/String;ILjava/lang/String;)V");
    jstring j_mid = JavaStringFromStdString(jni(), candidate->sdp_mid());
    jstring j_sdp = JavaStringFromStdString(jni(), sdp);
    jobject j_candidate = jni()->NewObject(
        candidate_class, ctor, j_mid, candidate->sdp_mline_index(), j_sdp);
    CHECK_EXCEPTION(jni()) << "error during NewObject";
    jmethodID m = GetMethodID(jni(), *j_observer_class_,
                              "onIceCandidate", "(Lorg/webrtc/IceCandidate;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_candidate);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onSignalingChange",
        "(Lorg/webrtc/PeerConnection$SignalingState;)V");
    jobject new_state_enum =
        JavaEnumFromIndex(jni(), "PeerConnection$SignalingState", new_state);
    jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onIceConnectionChange",
        "(Lorg/webrtc/PeerConnection$IceConnectionState;)V");
    jobject new_state_enum = JavaEnumFromIndex(
        jni(), "PeerConnection$IceConnectionState", new_state);
    jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onIceGatheringChange",
        "(Lorg/webrtc/PeerConnection$IceGatheringState;)V");
    jobject new_state_enum = JavaEnumFromIndex(
        jni(), "PeerConnection$IceGatheringState", new_state);
    jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnAddStream(MediaStreamInterface* stream) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobject j_stream = jni()->NewObject(
        *j_media_stream_class_, j_media_stream_ctor_, (jlong)stream);
    CHECK_EXCEPTION(jni()) << "error during NewObject";

    AudioTrackVector audio_tracks = stream->GetAudioTracks();
    for (size_t i = 0; i < audio_tracks.size(); ++i) {
      AudioTrackInterface* track = audio_tracks[i];
      jstring id = JavaStringFromStdString(jni(), track->id());
      jobject j_track = jni()->NewObject(
          *j_audio_track_class_, j_audio_track_ctor_, (jlong)track, id);
      CHECK_EXCEPTION(jni()) << "error during NewObject";
      jfieldID audio_tracks_id = GetFieldID(jni(),
                                            *j_media_stream_class_,
                                            "audioTracks",
                                            "Ljava/util/LinkedList;");
      jobject audio_tracks = GetObjectField(jni(), j_stream, audio_tracks_id);
      jmethodID add = GetMethodID(jni(),
                                  GetObjectClass(jni(), audio_tracks),
                                  "add",
                                  "(Ljava/lang/Object;)Z");
      jboolean added = jni()->CallBooleanMethod(audio_tracks, add, j_track);
      CHECK_EXCEPTION(jni()) << "error during CallBooleanMethod";
      CHECK(added);
    }

    VideoTrackVector video_tracks = stream->GetVideoTracks();
    for (size_t i = 0; i < video_tracks.size(); ++i) {
      VideoTrackInterface* track = video_tracks[i];
      jstring id = JavaStringFromStdString(jni(), track->id());
      jobject j_track = jni()->NewObject(
          *j_video_track_class_, j_video_track_ctor_, (jlong)track, id);
      CHECK_EXCEPTION(jni()) << "error during NewObject";
      jfieldID video_tracks_id = GetFieldID(jni(),
                                            *j_media_stream_class_,
                                            "videoTracks",
                                            "Ljava/util/LinkedList;");
      jobject video_tracks = GetObjectField(jni(), j_stream, video_tracks_id);
      jmethodID add = GetMethodID(jni(),
                                  GetObjectClass(jni(), video_tracks),
                                  "add",
                                  "(Ljava/lang/Object;)Z");
      jboolean added = jni()->CallBooleanMethod(video_tracks, add, j_track);
      CHECK_EXCEPTION(jni()) << "error during CallBooleanMethod";
      CHECK(added);
    }
    streams_[stream] = jni()->NewWeakGlobalRef(j_stream);
    CHECK_EXCEPTION(jni()) << "error during NewWeakGlobalRef";

    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onAddStream",
                              "(Lorg/webrtc/MediaStream;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_stream);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnRemoveStream(MediaStreamInterface* stream) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    NativeToJavaStreamsMap::iterator it = streams_.find(stream);
    CHECK(it != streams_.end()) << "unexpected stream: " << std::hex << stream;

    WeakRef s(jni(), it->second);
    streams_.erase(it);
    if (!s.obj())
      return;

    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onRemoveStream",
                              "(Lorg/webrtc/MediaStream;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, s.obj());
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnDataChannel(DataChannelInterface* channel) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobject j_channel = jni()->NewObject(
        *j_data_channel_class_, j_data_channel_ctor_, (jlong)channel);
    CHECK_EXCEPTION(jni()) << "error during NewObject";

    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onDataChannel",
                              "(Lorg/webrtc/DataChannel;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_channel);

    // Channel is now owned by Java object, and will be freed from
    // DataChannel.dispose().  Important that this be done _after_ the
    // CallVoidMethod above as Java code might call back into native code and be
    // surprised to see a refcount of 2.
    int bumped_count = channel->AddRef();
    CHECK(bumped_count == 2) << "Unexpected refcount OnDataChannel";

    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnRenegotiationNeeded() override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m =
        GetMethodID(jni(), *j_observer_class_, "onRenegotiationNeeded", "()V");
    jni()->CallVoidMethod(*j_observer_global_, m);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void SetConstraints(ConstraintsWrapper* constraints) {
    CHECK(!constraints_.get()) << "constraints already set!";
    constraints_.reset(constraints);
  }

  const ConstraintsWrapper* constraints() { return constraints_.get(); }

 private:
  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const ScopedGlobalRef<jclass> j_media_stream_class_;
  const jmethodID j_media_stream_ctor_;
  const ScopedGlobalRef<jclass> j_audio_track_class_;
  const jmethodID j_audio_track_ctor_;
  const ScopedGlobalRef<jclass> j_video_track_class_;
  const jmethodID j_video_track_ctor_;
  const ScopedGlobalRef<jclass> j_data_channel_class_;
  const jmethodID j_data_channel_ctor_;
  typedef std::map<void*, jweak> NativeToJavaStreamsMap;
  NativeToJavaStreamsMap streams_;  // C++ -> Java streams.
  scoped_ptr<ConstraintsWrapper> constraints_;
};

// Wrapper for a Java MediaConstraints object.  Copies all needed data so when
// the constructor returns the Java object is no longer needed.
class ConstraintsWrapper : public MediaConstraintsInterface {
 public:
  ConstraintsWrapper(JNIEnv* jni, jobject j_constraints) {
    PopulateConstraintsFromJavaPairList(
        jni, j_constraints, "mandatory", &mandatory_);
    PopulateConstraintsFromJavaPairList(
        jni, j_constraints, "optional", &optional_);
  }

  virtual ~ConstraintsWrapper() {}

  // MediaConstraintsInterface.
  const Constraints& GetMandatory() const override { return mandatory_; }

  const Constraints& GetOptional() const override { return optional_; }

 private:
  // Helper for translating a List<Pair<String, String>> to a Constraints.
  static void PopulateConstraintsFromJavaPairList(
      JNIEnv* jni, jobject j_constraints,
      const char* field_name, Constraints* field) {
    jfieldID j_id = GetFieldID(jni,
        GetObjectClass(jni, j_constraints), field_name, "Ljava/util/List;");
    jobject j_list = GetObjectField(jni, j_constraints, j_id);
    jmethodID j_iterator_id = GetMethodID(jni,
        GetObjectClass(jni, j_list), "iterator", "()Ljava/util/Iterator;");
    jobject j_iterator = jni->CallObjectMethod(j_list, j_iterator_id);
    CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
    jmethodID j_has_next = GetMethodID(jni,
        GetObjectClass(jni, j_iterator), "hasNext", "()Z");
    jmethodID j_next = GetMethodID(jni,
        GetObjectClass(jni, j_iterator), "next", "()Ljava/lang/Object;");
    while (jni->CallBooleanMethod(j_iterator, j_has_next)) {
      CHECK_EXCEPTION(jni) << "error during CallBooleanMethod";
      jobject entry = jni->CallObjectMethod(j_iterator, j_next);
      CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
      jmethodID get_key = GetMethodID(jni,
          GetObjectClass(jni, entry), "getKey", "()Ljava/lang/String;");
      jstring j_key = reinterpret_cast<jstring>(
          jni->CallObjectMethod(entry, get_key));
      CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
      jmethodID get_value = GetMethodID(jni,
          GetObjectClass(jni, entry), "getValue", "()Ljava/lang/String;");
      jstring j_value = reinterpret_cast<jstring>(
          jni->CallObjectMethod(entry, get_value));
      CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
      field->push_back(Constraint(JavaToStdString(jni, j_key),
                                  JavaToStdString(jni, j_value)));
    }
    CHECK_EXCEPTION(jni) << "error during CallBooleanMethod";
  }

  Constraints mandatory_;
  Constraints optional_;
};

static jobject JavaSdpFromNativeSdp(
    JNIEnv* jni, const SessionDescriptionInterface* desc) {
  std::string sdp;
  CHECK(desc->ToString(&sdp)) << "got so far: " << sdp;
  jstring j_description = JavaStringFromStdString(jni, sdp);

  jclass j_type_class = FindClass(
      jni, "org/webrtc/SessionDescription$Type");
  jmethodID j_type_from_canonical = GetStaticMethodID(
      jni, j_type_class, "fromCanonicalForm",
      "(Ljava/lang/String;)Lorg/webrtc/SessionDescription$Type;");
  jstring j_type_string = JavaStringFromStdString(jni, desc->type());
  jobject j_type = jni->CallStaticObjectMethod(
      j_type_class, j_type_from_canonical, j_type_string);
  CHECK_EXCEPTION(jni) << "error during CallObjectMethod";

  jclass j_sdp_class = FindClass(jni, "org/webrtc/SessionDescription");
  jmethodID j_sdp_ctor = GetMethodID(
      jni, j_sdp_class, "<init>",
      "(Lorg/webrtc/SessionDescription$Type;Ljava/lang/String;)V");
  jobject j_sdp = jni->NewObject(
      j_sdp_class, j_sdp_ctor, j_type, j_description);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  return j_sdp;
}

template <class T>  // T is one of {Create,Set}SessionDescriptionObserver.
class SdpObserverWrapper : public T {
 public:
  SdpObserverWrapper(JNIEnv* jni, jobject j_observer,
                     ConstraintsWrapper* constraints)
      : constraints_(constraints),
        j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, j_observer)) {
  }

  virtual ~SdpObserverWrapper() {}

  // Can't mark override because of templating.
  virtual void OnSuccess() {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onSetSuccess", "()V");
    jni()->CallVoidMethod(*j_observer_global_, m);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  // Can't mark override because of templating.
  virtual void OnSuccess(SessionDescriptionInterface* desc) {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onCreateSuccess",
        "(Lorg/webrtc/SessionDescription;)V");
    jobject j_sdp = JavaSdpFromNativeSdp(jni(), desc);
    jni()->CallVoidMethod(*j_observer_global_, m, j_sdp);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

 protected:
  // Common implementation for failure of Set & Create types, distinguished by
  // |op| being "Set" or "Create".
  void OnFailure(const std::string& op, const std::string& error) {
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "on" + op + "Failure",
                              "(Ljava/lang/String;)V");
    jstring j_error_string = JavaStringFromStdString(jni(), error);
    jni()->CallVoidMethod(*j_observer_global_, m, j_error_string);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

 private:
  scoped_ptr<ConstraintsWrapper> constraints_;
  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
};

class CreateSdpObserverWrapper
    : public SdpObserverWrapper<CreateSessionDescriptionObserver> {
 public:
  CreateSdpObserverWrapper(JNIEnv* jni, jobject j_observer,
                           ConstraintsWrapper* constraints)
      : SdpObserverWrapper(jni, j_observer, constraints) {}

  void OnFailure(const std::string& error) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    SdpObserverWrapper::OnFailure(std::string("Create"), error);
  }
};

class SetSdpObserverWrapper
    : public SdpObserverWrapper<SetSessionDescriptionObserver> {
 public:
  SetSdpObserverWrapper(JNIEnv* jni, jobject j_observer,
                        ConstraintsWrapper* constraints)
      : SdpObserverWrapper(jni, j_observer, constraints) {}

  void OnFailure(const std::string& error) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    SdpObserverWrapper::OnFailure(std::string("Set"), error);
  }
};

// Adapter for a Java DataChannel$Observer presenting a C++ DataChannelObserver
// and dispatching the callback from C++ back to Java.
class DataChannelObserverWrapper : public DataChannelObserver {
 public:
  DataChannelObserverWrapper(JNIEnv* jni, jobject j_observer)
      : j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, j_observer)),
        j_buffer_class_(jni, FindClass(jni, "org/webrtc/DataChannel$Buffer")),
        j_on_state_change_mid_(GetMethodID(jni, *j_observer_class_,
                                           "onStateChange", "()V")),
        j_on_message_mid_(GetMethodID(jni, *j_observer_class_, "onMessage",
                                      "(Lorg/webrtc/DataChannel$Buffer;)V")),
        j_buffer_ctor_(GetMethodID(jni, *j_buffer_class_,
                                   "<init>", "(Ljava/nio/ByteBuffer;Z)V")) {
  }

  virtual ~DataChannelObserverWrapper() {}

  void OnStateChange() override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jni()->CallVoidMethod(*j_observer_global_, j_on_state_change_mid_);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

  void OnMessage(const DataBuffer& buffer) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobject byte_buffer =
        jni()->NewDirectByteBuffer(const_cast<char*>(buffer.data.data()),
                                   buffer.data.length());
    jobject j_buffer = jni()->NewObject(*j_buffer_class_, j_buffer_ctor_,
                                        byte_buffer, buffer.binary);
    jni()->CallVoidMethod(*j_observer_global_, j_on_message_mid_, j_buffer);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

 private:
  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const ScopedGlobalRef<jclass> j_buffer_class_;
  const jmethodID j_on_state_change_mid_;
  const jmethodID j_on_message_mid_;
  const jmethodID j_buffer_ctor_;
};

// Adapter for a Java StatsObserver presenting a C++ StatsObserver and
// dispatching the callback from C++ back to Java.
class StatsObserverWrapper : public StatsObserver {
 public:
  StatsObserverWrapper(JNIEnv* jni, jobject j_observer)
      : j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, j_observer)),
        j_stats_report_class_(jni, FindClass(jni, "org/webrtc/StatsReport")),
        j_stats_report_ctor_(GetMethodID(
            jni, *j_stats_report_class_, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;D"
            "[Lorg/webrtc/StatsReport$Value;)V")),
        j_value_class_(jni, FindClass(
            jni, "org/webrtc/StatsReport$Value")),
        j_value_ctor_(GetMethodID(
            jni, *j_value_class_, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;)V")) {
  }

  virtual ~StatsObserverWrapper() {}

  void OnComplete(const StatsReports& reports) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobjectArray j_reports = ReportsToJava(jni(), reports);
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onComplete",
                              "([Lorg/webrtc/StatsReport;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_reports);
    CHECK_EXCEPTION(jni()) << "error during CallVoidMethod";
  }

 private:
  jobjectArray ReportsToJava(
      JNIEnv* jni, const StatsReports& reports) {
    jobjectArray reports_array = jni->NewObjectArray(
        reports.size(), *j_stats_report_class_, NULL);
    int i = 0;
    for (const auto* report : reports) {
      ScopedLocalRefFrame local_ref_frame(jni);
      jstring j_id = JavaStringFromStdString(jni, report->id()->ToString());
      jstring j_type = JavaStringFromStdString(jni, report->TypeToString());
      jobjectArray j_values = ValuesToJava(jni, report->values());
      jobject j_report = jni->NewObject(*j_stats_report_class_,
                                        j_stats_report_ctor_,
                                        j_id,
                                        j_type,
                                        report->timestamp(),
                                        j_values);
      jni->SetObjectArrayElement(reports_array, i++, j_report);
    }
    return reports_array;
  }

  jobjectArray ValuesToJava(JNIEnv* jni, const StatsReport::Values& values) {
    jobjectArray j_values = jni->NewObjectArray(
        values.size(), *j_value_class_, NULL);
    int i = 0;
    for (const auto& it : values) {
      ScopedLocalRefFrame local_ref_frame(jni);
      // Should we use the '.name' enum value here instead of converting the
      // name to a string?
      jstring j_name = JavaStringFromStdString(jni, it.second->display_name());
      jstring j_value = JavaStringFromStdString(jni, it.second->ToString());
      jobject j_element_value =
          jni->NewObject(*j_value_class_, j_value_ctor_, j_name, j_value);
      jni->SetObjectArrayElement(j_values, i++, j_element_value);
    }
    return j_values;
  }

  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const ScopedGlobalRef<jclass> j_stats_report_class_;
  const jmethodID j_stats_report_ctor_;
  const ScopedGlobalRef<jclass> j_value_class_;
  const jmethodID j_value_ctor_;
};

// Adapter presenting a cricket::VideoRenderer as a
// webrtc::VideoRendererInterface.
class VideoRendererWrapper : public VideoRendererInterface {
 public:
  static VideoRendererWrapper* Create(cricket::VideoRenderer* renderer) {
    if (renderer)
      return new VideoRendererWrapper(renderer);
    return NULL;
  }

  virtual ~VideoRendererWrapper() {}

  void SetSize(int width, int height) override {
    ScopedLocalRefFrame local_ref_frame(AttachCurrentThreadIfNeeded());
    const bool kNotReserved = false;  // What does this param mean??
    renderer_->SetSize(width, height, kNotReserved);
  }

  void RenderFrame(const cricket::VideoFrame* frame) override {
    ScopedLocalRefFrame local_ref_frame(AttachCurrentThreadIfNeeded());
    renderer_->RenderFrame(frame);
  }

 private:
  explicit VideoRendererWrapper(cricket::VideoRenderer* renderer)
      : renderer_(renderer) {}

  scoped_ptr<cricket::VideoRenderer> renderer_;
};

// Wrapper dispatching webrtc::VideoRendererInterface to a Java VideoRenderer
// instance.
class JavaVideoRendererWrapper : public VideoRendererInterface {
 public:
  JavaVideoRendererWrapper(JNIEnv* jni, jobject j_callbacks)
      : j_callbacks_(jni, j_callbacks),
        j_set_size_id_(GetMethodID(
            jni, GetObjectClass(jni, j_callbacks), "setSize", "(II)V")),
        j_render_frame_id_(GetMethodID(
            jni, GetObjectClass(jni, j_callbacks), "renderFrame",
            "(Lorg/webrtc/VideoRenderer$I420Frame;)V")),
        j_frame_class_(jni,
                       FindClass(jni, "org/webrtc/VideoRenderer$I420Frame")),
        j_i420_frame_ctor_id_(GetMethodID(
            jni, *j_frame_class_, "<init>", "(II[I[Ljava/nio/ByteBuffer;)V")),
        j_texture_frame_ctor_id_(GetMethodID(
            jni, *j_frame_class_, "<init>",
            "(IILjava/lang/Object;I)V")),
        j_byte_buffer_class_(jni, FindClass(jni, "java/nio/ByteBuffer")) {
    CHECK_EXCEPTION(jni);
  }

  virtual ~JavaVideoRendererWrapper() {}

  void SetSize(int width, int height) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    jni()->CallVoidMethod(*j_callbacks_, j_set_size_id_, width, height);
    CHECK_EXCEPTION(jni());
  }

  void RenderFrame(const cricket::VideoFrame* frame) override {
    ScopedLocalRefFrame local_ref_frame(jni());
    if (frame->GetNativeHandle() != NULL) {
      jobject j_frame = CricketToJavaTextureFrame(frame);
      jni()->CallVoidMethod(*j_callbacks_, j_render_frame_id_, j_frame);
      CHECK_EXCEPTION(jni());
    } else {
      jobject j_frame = CricketToJavaI420Frame(frame);
      jni()->CallVoidMethod(*j_callbacks_, j_render_frame_id_, j_frame);
      CHECK_EXCEPTION(jni());
    }
  }

 private:
  // Return a VideoRenderer.I420Frame referring to the data in |frame|.
  jobject CricketToJavaI420Frame(const cricket::VideoFrame* frame) {
    jintArray strides = jni()->NewIntArray(3);
    jint* strides_array = jni()->GetIntArrayElements(strides, NULL);
    strides_array[0] = frame->GetYPitch();
    strides_array[1] = frame->GetUPitch();
    strides_array[2] = frame->GetVPitch();
    jni()->ReleaseIntArrayElements(strides, strides_array, 0);
    jobjectArray planes = jni()->NewObjectArray(3, *j_byte_buffer_class_, NULL);
    jobject y_buffer = jni()->NewDirectByteBuffer(
        const_cast<uint8*>(frame->GetYPlane()),
        frame->GetYPitch() * frame->GetHeight());
    jobject u_buffer = jni()->NewDirectByteBuffer(
        const_cast<uint8*>(frame->GetUPlane()), frame->GetChromaSize());
    jobject v_buffer = jni()->NewDirectByteBuffer(
        const_cast<uint8*>(frame->GetVPlane()), frame->GetChromaSize());
    jni()->SetObjectArrayElement(planes, 0, y_buffer);
    jni()->SetObjectArrayElement(planes, 1, u_buffer);
    jni()->SetObjectArrayElement(planes, 2, v_buffer);
    return jni()->NewObject(
        *j_frame_class_, j_i420_frame_ctor_id_,
        frame->GetWidth(), frame->GetHeight(), strides, planes);
  }

  // Return a VideoRenderer.I420Frame referring texture object in |frame|.
  jobject CricketToJavaTextureFrame(const cricket::VideoFrame* frame) {
    NativeHandleImpl* handle =
        reinterpret_cast<NativeHandleImpl*>(frame->GetNativeHandle());
    jobject texture_object = reinterpret_cast<jobject>(handle->GetHandle());
    int texture_id = handle->GetTextureId();
    return jni()->NewObject(
        *j_frame_class_, j_texture_frame_ctor_id_,
        frame->GetWidth(), frame->GetHeight(), texture_object, texture_id);
  }

  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

  ScopedGlobalRef<jobject> j_callbacks_;
  jmethodID j_set_size_id_;
  jmethodID j_render_frame_id_;
  ScopedGlobalRef<jclass> j_frame_class_;
  jmethodID j_i420_frame_ctor_id_;
  jmethodID j_texture_frame_ctor_id_;
  ScopedGlobalRef<jclass> j_byte_buffer_class_;
};


static DataChannelInterface* ExtractNativeDC(JNIEnv* jni, jobject j_dc) {
  jfieldID native_dc_id = GetFieldID(jni,
      GetObjectClass(jni, j_dc), "nativeDataChannel", "J");
  jlong j_d = GetLongField(jni, j_dc, native_dc_id);
  return reinterpret_cast<DataChannelInterface*>(j_d);
}

JOW(jlong, DataChannel_registerObserverNative)(
    JNIEnv* jni, jobject j_dc, jobject j_observer) {
  scoped_ptr<DataChannelObserverWrapper> observer(
      new DataChannelObserverWrapper(jni, j_observer));
  ExtractNativeDC(jni, j_dc)->RegisterObserver(observer.get());
  return jlongFromPointer(observer.release());
}

JOW(void, DataChannel_unregisterObserverNative)(
    JNIEnv* jni, jobject j_dc, jlong native_observer) {
  ExtractNativeDC(jni, j_dc)->UnregisterObserver();
  delete reinterpret_cast<DataChannelObserverWrapper*>(native_observer);
}

JOW(jstring, DataChannel_label)(JNIEnv* jni, jobject j_dc) {
  return JavaStringFromStdString(jni, ExtractNativeDC(jni, j_dc)->label());
}

JOW(jobject, DataChannel_state)(JNIEnv* jni, jobject j_dc) {
  return JavaEnumFromIndex(
      jni, "DataChannel$State", ExtractNativeDC(jni, j_dc)->state());
}

JOW(jlong, DataChannel_bufferedAmount)(JNIEnv* jni, jobject j_dc) {
  uint64 buffered_amount = ExtractNativeDC(jni, j_dc)->buffered_amount();
  CHECK_LE(buffered_amount, std::numeric_limits<int64>::max())
      << "buffered_amount overflowed jlong!";
  return static_cast<jlong>(buffered_amount);
}

JOW(void, DataChannel_close)(JNIEnv* jni, jobject j_dc) {
  ExtractNativeDC(jni, j_dc)->Close();
}

JOW(jboolean, DataChannel_sendNative)(JNIEnv* jni, jobject j_dc,
                                      jbyteArray data, jboolean binary) {
  jbyte* bytes = jni->GetByteArrayElements(data, NULL);
  bool ret = ExtractNativeDC(jni, j_dc)->Send(DataBuffer(
      rtc::Buffer(bytes, jni->GetArrayLength(data)),
      binary));
  jni->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
  return ret;
}

JOW(void, DataChannel_dispose)(JNIEnv* jni, jobject j_dc) {
  CHECK_RELEASE(ExtractNativeDC(jni, j_dc));
}

JOW(void, Logging_nativeEnableTracing)(
    JNIEnv* jni, jclass, jstring j_path, jint nativeLevels,
    jint nativeSeverity) {
  std::string path = JavaToStdString(jni, j_path);
  if (nativeLevels != webrtc::kTraceNone) {
    webrtc::Trace::set_level_filter(nativeLevels);
#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
    if (path != "logcat:") {
#endif
      CHECK_EQ(0, webrtc::Trace::SetTraceFile(path.c_str(), false))
          << "SetTraceFile failed";
#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
    } else {
      // Intentionally leak this to avoid needing to reason about its lifecycle.
      // It keeps no state and functions only as a dispatch point.
      static LogcatTraceContext* g_trace_callback = new LogcatTraceContext();
    }
#endif
  }
  rtc::LogMessage::LogToDebug(nativeSeverity);
}

JOW(void, PeerConnection_freePeerConnection)(JNIEnv*, jclass, jlong j_p) {
  CHECK_RELEASE(reinterpret_cast<PeerConnectionInterface*>(j_p));
}

JOW(void, PeerConnection_freeObserver)(JNIEnv*, jclass, jlong j_p) {
  PCOJava* p = reinterpret_cast<PCOJava*>(j_p);
  delete p;
}

JOW(void, MediaSource_free)(JNIEnv*, jclass, jlong j_p) {
  CHECK_RELEASE(reinterpret_cast<MediaSourceInterface*>(j_p));
}

JOW(void, VideoCapturer_free)(JNIEnv*, jclass, jlong j_p) {
  delete reinterpret_cast<cricket::VideoCapturer*>(j_p);
}

JOW(void, VideoRenderer_freeGuiVideoRenderer)(JNIEnv*, jclass, jlong j_p) {
  delete reinterpret_cast<VideoRendererWrapper*>(j_p);
}

JOW(void, VideoRenderer_freeWrappedVideoRenderer)(JNIEnv*, jclass, jlong j_p) {
  delete reinterpret_cast<JavaVideoRendererWrapper*>(j_p);
}

JOW(void, MediaStreamTrack_free)(JNIEnv*, jclass, jlong j_p) {
  CHECK_RELEASE(reinterpret_cast<MediaStreamTrackInterface*>(j_p));
}

JOW(jboolean, MediaStream_nativeAddAudioTrack)(
    JNIEnv* jni, jclass, jlong pointer, jlong j_audio_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->AddTrack(
      reinterpret_cast<AudioTrackInterface*>(j_audio_track_pointer));
}

JOW(jboolean, MediaStream_nativeAddVideoTrack)(
    JNIEnv* jni, jclass, jlong pointer, jlong j_video_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)
      ->AddTrack(reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer));
}

JOW(jboolean, MediaStream_nativeRemoveAudioTrack)(
    JNIEnv* jni, jclass, jlong pointer, jlong j_audio_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->RemoveTrack(
      reinterpret_cast<AudioTrackInterface*>(j_audio_track_pointer));
}

JOW(jboolean, MediaStream_nativeRemoveVideoTrack)(
    JNIEnv* jni, jclass, jlong pointer, jlong j_video_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->RemoveTrack(
      reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer));
}

JOW(jstring, MediaStream_nativeLabel)(JNIEnv* jni, jclass, jlong j_p) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<MediaStreamInterface*>(j_p)->label());
}

JOW(void, MediaStream_free)(JNIEnv*, jclass, jlong j_p) {
  CHECK_RELEASE(reinterpret_cast<MediaStreamInterface*>(j_p));
}

JOW(jlong, PeerConnectionFactory_nativeCreateObserver)(
    JNIEnv * jni, jclass, jobject j_observer) {
  return (jlong)new PCOJava(jni, j_observer);
}

#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
JOW(jboolean, PeerConnectionFactory_initializeAndroidGlobals)(
    JNIEnv* jni, jclass, jobject context,
    jboolean initialize_audio, jboolean initialize_video,
    jboolean vp8_hw_acceleration, jobject render_egl_context) {
  bool failure = false;
  vp8_hw_acceleration_enabled = vp8_hw_acceleration;
  if (!factory_static_initialized) {
    if (initialize_video) {
      failure |= webrtc::SetRenderAndroidVM(GetJVM());
      failure |= AndroidVideoCapturerJni::SetAndroidObjects(jni, context);
    }
    if (initialize_audio)
      failure |= webrtc::VoiceEngine::SetAndroidObjects(GetJVM(), context);
    factory_static_initialized = true;
  }
  if (initialize_video) {
    failure |= MediaCodecVideoDecoderFactory::SetAndroidObjects(jni,
        render_egl_context);
  }
  return !failure;
}
#endif  // defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)

JOW(void, PeerConnectionFactory_initializeFieldTrials)(
    JNIEnv* jni, jclass, jstring j_trials_init_string) {
  field_trials_init_string = NULL;
  if (j_trials_init_string != NULL) {
    const char* init_string =
        jni->GetStringUTFChars(j_trials_init_string, NULL);
    int init_string_length = jni->GetStringUTFLength(j_trials_init_string);
    field_trials_init_string = new char[init_string_length + 1];
    rtc::strcpyn(field_trials_init_string, init_string_length + 1, init_string);
    jni->ReleaseStringUTFChars(j_trials_init_string, init_string);
    LOG(LS_INFO) << "initializeFieldTrials: " << field_trials_init_string;
  }
  webrtc::field_trial::InitFieldTrialsFromString(field_trials_init_string);
}

// Helper struct for working around the fact that CreatePeerConnectionFactory()
// comes in two flavors: either entirely automagical (constructing its own
// threads and deleting them on teardown, but no external codec factory support)
// or entirely manual (requires caller to delete threads after factory
// teardown).  This struct takes ownership of its ctor's arguments to present a
// single thing for Java to hold and eventually free.
class OwnedFactoryAndThreads {
 public:
  OwnedFactoryAndThreads(Thread* worker_thread,
                         Thread* signaling_thread,
                         PeerConnectionFactoryInterface* factory)
      : worker_thread_(worker_thread),
        signaling_thread_(signaling_thread),
        factory_(factory) {}

  ~OwnedFactoryAndThreads() { CHECK_RELEASE(factory_); }

  PeerConnectionFactoryInterface* factory() { return factory_; }

 private:
  const scoped_ptr<Thread> worker_thread_;
  const scoped_ptr<Thread> signaling_thread_;
  PeerConnectionFactoryInterface* factory_;  // Const after ctor except dtor.
};

JOW(jlong, PeerConnectionFactory_nativeCreatePeerConnectionFactory)(
    JNIEnv* jni, jclass) {
  // talk/ assumes pretty widely that the current Thread is ThreadManager'd, but
  // ThreadManager only WrapCurrentThread()s the thread where it is first
  // created.  Since the semantics around when auto-wrapping happens in
  // webrtc/base/ are convoluted, we simply wrap here to avoid having to think
  // about ramifications of auto-wrapping there.
  rtc::ThreadManager::Instance()->WrapCurrentThread();
  webrtc::Trace::CreateTrace();
  Thread* worker_thread = new Thread();
  worker_thread->SetName("worker_thread", NULL);
  Thread* signaling_thread = new Thread();
  signaling_thread->SetName("signaling_thread", NULL);
  CHECK(worker_thread->Start() && signaling_thread->Start())
      << "Failed to start threads";
  scoped_ptr<cricket::WebRtcVideoEncoderFactory> encoder_factory;
  scoped_ptr<cricket::WebRtcVideoDecoderFactory> decoder_factory;
#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
  if (vp8_hw_acceleration_enabled) {
    encoder_factory.reset(new MediaCodecVideoEncoderFactory());
    decoder_factory.reset(new MediaCodecVideoDecoderFactory());
  }
#endif
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      webrtc::CreatePeerConnectionFactory(worker_thread,
                                          signaling_thread,
                                          NULL,
                                          encoder_factory.release(),
                                          decoder_factory.release()));
  OwnedFactoryAndThreads* owned_factory = new OwnedFactoryAndThreads(
      worker_thread, signaling_thread, factory.release());
  return jlongFromPointer(owned_factory);
}

JOW(void, PeerConnectionFactory_freeFactory)(JNIEnv*, jclass, jlong j_p) {
  delete reinterpret_cast<OwnedFactoryAndThreads*>(j_p);
  if (field_trials_init_string) {
    webrtc::field_trial::InitFieldTrialsFromString(NULL);
    delete field_trials_init_string;
    field_trials_init_string = NULL;
  }
  webrtc::Trace::ReturnTrace();
}

static PeerConnectionFactoryInterface* factoryFromJava(jlong j_p) {
  return reinterpret_cast<OwnedFactoryAndThreads*>(j_p)->factory();
}

JOW(jlong, PeerConnectionFactory_nativeCreateLocalMediaStream)(
    JNIEnv* jni, jclass, jlong native_factory, jstring label) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  rtc::scoped_refptr<MediaStreamInterface> stream(
      factory->CreateLocalMediaStream(JavaToStdString(jni, label)));
  return (jlong)stream.release();
}

JOW(jlong, PeerConnectionFactory_nativeCreateVideoSource)(
    JNIEnv* jni, jclass, jlong native_factory, jlong native_capturer,
    jobject j_constraints) {
  scoped_ptr<ConstraintsWrapper> constraints(
      new ConstraintsWrapper(jni, j_constraints));
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  rtc::scoped_refptr<VideoSourceInterface> source(
      factory->CreateVideoSource(
          reinterpret_cast<cricket::VideoCapturer*>(native_capturer),
          constraints.get()));
  return (jlong)source.release();
}

JOW(jlong, PeerConnectionFactory_nativeCreateVideoTrack)(
    JNIEnv* jni, jclass, jlong native_factory, jstring id,
    jlong native_source) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  rtc::scoped_refptr<VideoTrackInterface> track(
      factory->CreateVideoTrack(
          JavaToStdString(jni, id),
          reinterpret_cast<VideoSourceInterface*>(native_source)));
  return (jlong)track.release();
}

JOW(jlong, PeerConnectionFactory_nativeCreateAudioSource)(
    JNIEnv* jni, jclass, jlong native_factory, jobject j_constraints) {
  scoped_ptr<ConstraintsWrapper> constraints(
      new ConstraintsWrapper(jni, j_constraints));
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  rtc::scoped_refptr<AudioSourceInterface> source(
      factory->CreateAudioSource(constraints.get()));
  return (jlong)source.release();
}

JOW(jlong, PeerConnectionFactory_nativeCreateAudioTrack)(
    JNIEnv* jni, jclass, jlong native_factory, jstring id,
    jlong native_source) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  rtc::scoped_refptr<AudioTrackInterface> track(factory->CreateAudioTrack(
      JavaToStdString(jni, id),
      reinterpret_cast<AudioSourceInterface*>(native_source)));
  return (jlong)track.release();
}

JOW(void, PeerConnectionFactory_nativeSetOptions)(
    JNIEnv* jni, jclass, jlong native_factory, jobject options) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  jclass options_class = jni->GetObjectClass(options);
  jfieldID network_ignore_mask_field =
      jni->GetFieldID(options_class, "networkIgnoreMask", "I");
  int network_ignore_mask =
      jni->GetIntField(options, network_ignore_mask_field);
  PeerConnectionFactoryInterface::Options options_to_set;

  // This doesn't necessarily match the c++ version of this struct; feel free
  // to add more parameters as necessary.
  options_to_set.network_ignore_mask = network_ignore_mask;
  factory->SetOptions(options_to_set);
}

static void JavaIceServersToJsepIceServers(
    JNIEnv* jni, jobject j_ice_servers,
    PeerConnectionInterface::IceServers* ice_servers) {
  jclass list_class = GetObjectClass(jni, j_ice_servers);
  jmethodID iterator_id = GetMethodID(
      jni, list_class, "iterator", "()Ljava/util/Iterator;");
  jobject iterator = jni->CallObjectMethod(j_ice_servers, iterator_id);
  CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  jmethodID iterator_has_next = GetMethodID(
      jni, GetObjectClass(jni, iterator), "hasNext", "()Z");
  jmethodID iterator_next = GetMethodID(
      jni, GetObjectClass(jni, iterator), "next", "()Ljava/lang/Object;");
  while (jni->CallBooleanMethod(iterator, iterator_has_next)) {
    CHECK_EXCEPTION(jni) << "error during CallBooleanMethod";
    jobject j_ice_server = jni->CallObjectMethod(iterator, iterator_next);
    CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
    jclass j_ice_server_class = GetObjectClass(jni, j_ice_server);
    jfieldID j_ice_server_uri_id =
        GetFieldID(jni, j_ice_server_class, "uri", "Ljava/lang/String;");
    jfieldID j_ice_server_username_id =
        GetFieldID(jni, j_ice_server_class, "username", "Ljava/lang/String;");
    jfieldID j_ice_server_password_id =
        GetFieldID(jni, j_ice_server_class, "password", "Ljava/lang/String;");
    jstring uri = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_uri_id));
    jstring username = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_username_id));
    jstring password = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_password_id));
    PeerConnectionInterface::IceServer server;
    server.uri = JavaToStdString(jni, uri);
    server.username = JavaToStdString(jni, username);
    server.password = JavaToStdString(jni, password);
    ice_servers->push_back(server);
  }
  CHECK_EXCEPTION(jni) << "error during CallBooleanMethod";
}

JOW(jlong, PeerConnectionFactory_nativeCreatePeerConnection)(
    JNIEnv *jni, jclass, jlong factory, jobject j_ice_servers,
    jobject j_constraints, jlong observer_p) {
  rtc::scoped_refptr<PeerConnectionFactoryInterface> f(
      reinterpret_cast<PeerConnectionFactoryInterface*>(
          factoryFromJava(factory)));
  PeerConnectionInterface::IceServers servers;
  JavaIceServersToJsepIceServers(jni, j_ice_servers, &servers);
  PCOJava* observer = reinterpret_cast<PCOJava*>(observer_p);
  observer->SetConstraints(new ConstraintsWrapper(jni, j_constraints));
  rtc::scoped_refptr<PeerConnectionInterface> pc(f->CreatePeerConnection(
      servers, observer->constraints(), NULL, NULL, observer));
  return (jlong)pc.release();
}

static rtc::scoped_refptr<PeerConnectionInterface> ExtractNativePC(
    JNIEnv* jni, jobject j_pc) {
  jfieldID native_pc_id = GetFieldID(jni,
      GetObjectClass(jni, j_pc), "nativePeerConnection", "J");
  jlong j_p = GetLongField(jni, j_pc, native_pc_id);
  return rtc::scoped_refptr<PeerConnectionInterface>(
      reinterpret_cast<PeerConnectionInterface*>(j_p));
}

JOW(jobject, PeerConnection_getLocalDescription)(JNIEnv* jni, jobject j_pc) {
  const SessionDescriptionInterface* sdp =
      ExtractNativePC(jni, j_pc)->local_description();
  return sdp ? JavaSdpFromNativeSdp(jni, sdp) : NULL;
}

JOW(jobject, PeerConnection_getRemoteDescription)(JNIEnv* jni, jobject j_pc) {
  const SessionDescriptionInterface* sdp =
      ExtractNativePC(jni, j_pc)->remote_description();
  return sdp ? JavaSdpFromNativeSdp(jni, sdp) : NULL;
}

JOW(jobject, PeerConnection_createDataChannel)(
    JNIEnv* jni, jobject j_pc, jstring j_label, jobject j_init) {
  DataChannelInit init = JavaDataChannelInitToNative(jni, j_init);
  rtc::scoped_refptr<DataChannelInterface> channel(
      ExtractNativePC(jni, j_pc)->CreateDataChannel(
          JavaToStdString(jni, j_label), &init));
  // Mustn't pass channel.get() directly through NewObject to avoid reading its
  // vararg parameter as 64-bit and reading memory that doesn't belong to the
  // 32-bit parameter.
  jlong nativeChannelPtr = jlongFromPointer(channel.get());
  CHECK(nativeChannelPtr) << "Failed to create DataChannel";
  jclass j_data_channel_class = FindClass(jni, "org/webrtc/DataChannel");
  jmethodID j_data_channel_ctor = GetMethodID(
      jni, j_data_channel_class, "<init>", "(J)V");
  jobject j_channel = jni->NewObject(
      j_data_channel_class, j_data_channel_ctor, nativeChannelPtr);
  CHECK_EXCEPTION(jni) << "error during NewObject";
  // Channel is now owned by Java object, and will be freed from there.
  int bumped_count = channel->AddRef();
  CHECK(bumped_count == 2) << "Unexpected refcount";
  return j_channel;
}

JOW(void, PeerConnection_createOffer)(
    JNIEnv* jni, jobject j_pc, jobject j_observer, jobject j_constraints) {
  ConstraintsWrapper* constraints =
      new ConstraintsWrapper(jni, j_constraints);
  rtc::scoped_refptr<CreateSdpObserverWrapper> observer(
      new rtc::RefCountedObject<CreateSdpObserverWrapper>(
          jni, j_observer, constraints));
  ExtractNativePC(jni, j_pc)->CreateOffer(observer, constraints);
}

JOW(void, PeerConnection_createAnswer)(
    JNIEnv* jni, jobject j_pc, jobject j_observer, jobject j_constraints) {
  ConstraintsWrapper* constraints =
      new ConstraintsWrapper(jni, j_constraints);
  rtc::scoped_refptr<CreateSdpObserverWrapper> observer(
      new rtc::RefCountedObject<CreateSdpObserverWrapper>(
          jni, j_observer, constraints));
  ExtractNativePC(jni, j_pc)->CreateAnswer(observer, constraints);
}

// Helper to create a SessionDescriptionInterface from a SessionDescription.
static SessionDescriptionInterface* JavaSdpToNativeSdp(
    JNIEnv* jni, jobject j_sdp) {
  jfieldID j_type_id = GetFieldID(
      jni, GetObjectClass(jni, j_sdp), "type",
      "Lorg/webrtc/SessionDescription$Type;");
  jobject j_type = GetObjectField(jni, j_sdp, j_type_id);
  jmethodID j_canonical_form_id = GetMethodID(
      jni, GetObjectClass(jni, j_type), "canonicalForm",
      "()Ljava/lang/String;");
  jstring j_type_string = (jstring)jni->CallObjectMethod(
      j_type, j_canonical_form_id);
  CHECK_EXCEPTION(jni) << "error during CallObjectMethod";
  std::string std_type = JavaToStdString(jni, j_type_string);

  jfieldID j_description_id = GetFieldID(
      jni, GetObjectClass(jni, j_sdp), "description", "Ljava/lang/String;");
  jstring j_description = (jstring)GetObjectField(jni, j_sdp, j_description_id);
  std::string std_description = JavaToStdString(jni, j_description);

  return webrtc::CreateSessionDescription(
      std_type, std_description, NULL);
}

JOW(void, PeerConnection_setLocalDescription)(
    JNIEnv* jni, jobject j_pc,
    jobject j_observer, jobject j_sdp) {
  rtc::scoped_refptr<SetSdpObserverWrapper> observer(
      new rtc::RefCountedObject<SetSdpObserverWrapper>(
          jni, j_observer, reinterpret_cast<ConstraintsWrapper*>(NULL)));
  ExtractNativePC(jni, j_pc)->SetLocalDescription(
      observer, JavaSdpToNativeSdp(jni, j_sdp));
}

JOW(void, PeerConnection_setRemoteDescription)(
    JNIEnv* jni, jobject j_pc,
    jobject j_observer, jobject j_sdp) {
  rtc::scoped_refptr<SetSdpObserverWrapper> observer(
      new rtc::RefCountedObject<SetSdpObserverWrapper>(
          jni, j_observer, reinterpret_cast<ConstraintsWrapper*>(NULL)));
  ExtractNativePC(jni, j_pc)->SetRemoteDescription(
      observer, JavaSdpToNativeSdp(jni, j_sdp));
}

JOW(jboolean, PeerConnection_updateIce)(
    JNIEnv* jni, jobject j_pc, jobject j_ice_servers, jobject j_constraints) {
  PeerConnectionInterface::IceServers ice_servers;
  JavaIceServersToJsepIceServers(jni, j_ice_servers, &ice_servers);
  scoped_ptr<ConstraintsWrapper> constraints(
      new ConstraintsWrapper(jni, j_constraints));
  return ExtractNativePC(jni, j_pc)->UpdateIce(ice_servers, constraints.get());
}

JOW(jboolean, PeerConnection_nativeAddIceCandidate)(
    JNIEnv* jni, jobject j_pc, jstring j_sdp_mid,
    jint j_sdp_mline_index, jstring j_candidate_sdp) {
  std::string sdp_mid = JavaToStdString(jni, j_sdp_mid);
  std::string sdp = JavaToStdString(jni, j_candidate_sdp);
  scoped_ptr<IceCandidateInterface> candidate(
      webrtc::CreateIceCandidate(sdp_mid, j_sdp_mline_index, sdp, NULL));
  return ExtractNativePC(jni, j_pc)->AddIceCandidate(candidate.get());
}

JOW(jboolean, PeerConnection_nativeAddLocalStream)(
    JNIEnv* jni, jobject j_pc, jlong native_stream) {
  return ExtractNativePC(jni, j_pc)->AddStream(
      reinterpret_cast<MediaStreamInterface*>(native_stream));
}

JOW(void, PeerConnection_nativeRemoveLocalStream)(
    JNIEnv* jni, jobject j_pc, jlong native_stream) {
  ExtractNativePC(jni, j_pc)->RemoveStream(
      reinterpret_cast<MediaStreamInterface*>(native_stream));
}

JOW(bool, PeerConnection_nativeGetStats)(
    JNIEnv* jni, jobject j_pc, jobject j_observer, jlong native_track) {
  rtc::scoped_refptr<StatsObserverWrapper> observer(
      new rtc::RefCountedObject<StatsObserverWrapper>(jni, j_observer));
  return ExtractNativePC(jni, j_pc)->GetStats(
      observer,
      reinterpret_cast<MediaStreamTrackInterface*>(native_track),
      PeerConnectionInterface::kStatsOutputLevelStandard);
}

JOW(jobject, PeerConnection_signalingState)(JNIEnv* jni, jobject j_pc) {
  PeerConnectionInterface::SignalingState state =
      ExtractNativePC(jni, j_pc)->signaling_state();
  return JavaEnumFromIndex(jni, "PeerConnection$SignalingState", state);
}

JOW(jobject, PeerConnection_iceConnectionState)(JNIEnv* jni, jobject j_pc) {
  PeerConnectionInterface::IceConnectionState state =
      ExtractNativePC(jni, j_pc)->ice_connection_state();
  return JavaEnumFromIndex(jni, "PeerConnection$IceConnectionState", state);
}

JOW(jobject, PeerConnection_iceGatheringState)(JNIEnv* jni, jobject j_pc) {
  PeerConnectionInterface::IceGatheringState state =
      ExtractNativePC(jni, j_pc)->ice_gathering_state();
  return JavaEnumFromIndex(jni, "PeerConnection$IceGatheringState", state);
}

JOW(void, PeerConnection_close)(JNIEnv* jni, jobject j_pc) {
  ExtractNativePC(jni, j_pc)->Close();
  return;
}

JOW(jobject, MediaSource_nativeState)(JNIEnv* jni, jclass, jlong j_p) {
  rtc::scoped_refptr<MediaSourceInterface> p(
      reinterpret_cast<MediaSourceInterface*>(j_p));
  return JavaEnumFromIndex(jni, "MediaSource$State", p->state());
}

JOW(jobject, VideoCapturer_nativeCreateVideoCapturer)(
    JNIEnv* jni, jclass, jstring j_device_name) {
// Since we can't create platform specific java implementations in Java, we
// defer the creation to C land.
#if defined(ANDROID)
  jclass j_video_capturer_class(
      FindClass(jni, "org/webrtc/VideoCapturerAndroid"));
  const jmethodID j_videocapturer_ctor(GetMethodID(
      jni, j_video_capturer_class, "<init>", "()V"));
  jobject j_video_capturer = jni->NewObject(j_video_capturer_class,
                                            j_videocapturer_ctor);
  CHECK_EXCEPTION(jni) << "error during NewObject";

  rtc::scoped_ptr<AndroidVideoCapturerJni> delegate =
      AndroidVideoCapturerJni::Create(jni, j_video_capturer, j_device_name);
  if (!delegate.get())
    return nullptr;
  rtc::scoped_ptr<webrtc::AndroidVideoCapturer> capturer(
      new webrtc::AndroidVideoCapturer(delegate.Pass()));

#else
  std::string device_name = JavaToStdString(jni, j_device_name);
  scoped_ptr<cricket::DeviceManagerInterface> device_manager(
      cricket::DeviceManagerFactory::Create());
  CHECK(device_manager->Init()) << "DeviceManager::Init() failed";
  cricket::Device device;
  if (!device_manager->GetVideoCaptureDevice(device_name, &device)) {
    LOG(LS_ERROR) << "GetVideoCaptureDevice failed for " << device_name;
    return 0;
  }
  scoped_ptr<cricket::VideoCapturer> capturer(
      device_manager->CreateVideoCapturer(device));

  jclass j_video_capturer_class(
      FindClass(jni, "org/webrtc/VideoCapturer"));
  const jmethodID j_videocapturer_ctor(GetMethodID(
      jni, j_video_capturer_class, "<init>", "()V"));
  jobject j_video_capturer =
      jni->NewObject(j_video_capturer_class,
                     j_videocapturer_ctor);
  CHECK_EXCEPTION(jni) << "error during creation of VideoCapturer";

#endif
  const jmethodID j_videocapturer_set_native_capturer(GetMethodID(
      jni, j_video_capturer_class, "setNativeCapturer", "(J)V"));
  jni->CallVoidMethod(j_video_capturer,
                      j_videocapturer_set_native_capturer,
                      (jlong)capturer.release());
  CHECK_EXCEPTION(jni) << "error during setNativeCapturer";
  return j_video_capturer;
}

JOW(jlong, VideoRenderer_nativeCreateGuiVideoRenderer)(
    JNIEnv* jni, jclass, int x, int y) {
  scoped_ptr<VideoRendererWrapper> renderer(VideoRendererWrapper::Create(
      cricket::VideoRendererFactory::CreateGuiVideoRenderer(x, y)));
  return (jlong)renderer.release();
}

JOW(jlong, VideoRenderer_nativeWrapVideoRenderer)(
    JNIEnv* jni, jclass, jobject j_callbacks) {
  scoped_ptr<JavaVideoRendererWrapper> renderer(
      new JavaVideoRendererWrapper(jni, j_callbacks));
  return (jlong)renderer.release();
}

JOW(void, VideoRenderer_nativeCopyPlane)(
    JNIEnv *jni, jclass, jobject j_src_buffer, jint width, jint height,
    jint src_stride, jobject j_dst_buffer, jint dst_stride) {
  size_t src_size = jni->GetDirectBufferCapacity(j_src_buffer);
  size_t dst_size = jni->GetDirectBufferCapacity(j_dst_buffer);
  CHECK(src_stride >= width) << "Wrong source stride " << src_stride;
  CHECK(dst_stride >= width) << "Wrong destination stride " << dst_stride;
  CHECK(src_size >= src_stride * height)
      << "Insufficient source buffer capacity " << src_size;
  CHECK(dst_size >= dst_stride * height)
      << "Isufficient destination buffer capacity " << dst_size;
  uint8_t *src =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_src_buffer));
  uint8_t *dst =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_dst_buffer));
  if (src_stride == dst_stride) {
    memcpy(dst, src, src_stride * height);
  } else {
    for (int i = 0; i < height; i++) {
      memcpy(dst, src, width);
      src += src_stride;
      dst += dst_stride;
    }
  }
}

JOW(void, VideoSource_stop)(JNIEnv* jni, jclass, jlong j_p) {
  reinterpret_cast<VideoSourceInterface*>(j_p)->Stop();
}

JOW(void, VideoSource_restart)(
    JNIEnv* jni, jclass, jlong j_p_source, jlong j_p_format) {
  reinterpret_cast<VideoSourceInterface*>(j_p_source)->Restart();
}

JOW(jstring, MediaStreamTrack_nativeId)(JNIEnv* jni, jclass, jlong j_p) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<MediaStreamTrackInterface*>(j_p)->id());
}

JOW(jstring, MediaStreamTrack_nativeKind)(JNIEnv* jni, jclass, jlong j_p) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<MediaStreamTrackInterface*>(j_p)->kind());
}

JOW(jboolean, MediaStreamTrack_nativeEnabled)(JNIEnv* jni, jclass, jlong j_p) {
  return reinterpret_cast<MediaStreamTrackInterface*>(j_p)->enabled();
}

JOW(jobject, MediaStreamTrack_nativeState)(JNIEnv* jni, jclass, jlong j_p) {
  return JavaEnumFromIndex(
      jni,
      "MediaStreamTrack$State",
      reinterpret_cast<MediaStreamTrackInterface*>(j_p)->state());
}

JOW(jboolean, MediaStreamTrack_nativeSetState)(
    JNIEnv* jni, jclass, jlong j_p, jint j_new_state) {
  MediaStreamTrackInterface::TrackState new_state =
      (MediaStreamTrackInterface::TrackState)j_new_state;
  return reinterpret_cast<MediaStreamTrackInterface*>(j_p)
      ->set_state(new_state);
}

JOW(jboolean, MediaStreamTrack_nativeSetEnabled)(
    JNIEnv* jni, jclass, jlong j_p, jboolean enabled) {
  return reinterpret_cast<MediaStreamTrackInterface*>(j_p)
      ->set_enabled(enabled);
}

JOW(void, VideoTrack_nativeAddRenderer)(
    JNIEnv* jni, jclass,
    jlong j_video_track_pointer, jlong j_renderer_pointer) {
  reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer)->AddRenderer(
      reinterpret_cast<VideoRendererInterface*>(j_renderer_pointer));
}

JOW(void, VideoTrack_nativeRemoveRenderer)(
    JNIEnv* jni, jclass,
    jlong j_video_track_pointer, jlong j_renderer_pointer) {
  reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer)->RemoveRenderer(
      reinterpret_cast<VideoRendererInterface*>(j_renderer_pointer));
}

}  // namespace webrtc_jni
