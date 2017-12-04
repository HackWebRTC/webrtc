/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/pc/peerconnectionobserver_jni.h"

#include <pc/mediastreamobserver.h>
#include <string>

#include "rtc_base/ptr_util.h"
#include "sdk/android/generated_peerconnection_jni/jni/MediaStream_jni.h"
#include "sdk/android/generated_peerconnection_jni/jni/PeerConnection_jni.h"
#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/datachannel.h"
#include "sdk/android/src/jni/pc/java_native_conversion.h"

namespace webrtc {
namespace jni {

// Convenience, used since callbacks occur on the signaling thread, which may
// be a non-Java thread.
static JNIEnv* jni() {
  return AttachCurrentThreadIfNeeded();
}

PeerConnectionObserverJni::PeerConnectionObserverJni(JNIEnv* jni,
                                                     jobject j_observer)
    : j_observer_global_(jni, j_observer) {}

PeerConnectionObserverJni::~PeerConnectionObserverJni() {
  ScopedLocalRefFrame local_ref_frame(jni());
  while (!remote_streams_.empty())
    DisposeRemoteStream(remote_streams_.begin());
}

void PeerConnectionObserverJni::OnIceCandidate(
    const IceCandidateInterface* candidate) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onIceCandidate(env, *j_observer_global_,
                               NativeToJavaIceCandidate(env, *candidate));
}

void PeerConnectionObserverJni::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onIceCandidatesRemoved(
      env, *j_observer_global_, NativeToJavaCandidateArray(env, candidates));
}

void PeerConnectionObserverJni::OnSignalingChange(
    PeerConnectionInterface::SignalingState new_state) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onSignalingChange(
      env, *j_observer_global_,
      Java_SignalingState_fromNativeIndex(env, new_state));
}

void PeerConnectionObserverJni::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onIceConnectionChange(
      env, *j_observer_global_,
      Java_IceConnectionState_fromNativeIndex(env, new_state));
}

void PeerConnectionObserverJni::OnIceConnectionReceivingChange(bool receiving) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onIceConnectionReceivingChange(env, *j_observer_global_,
                                               receiving);
}

void PeerConnectionObserverJni::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onIceGatheringChange(
      env, *j_observer_global_,
      Java_IceGatheringState_fromNativeIndex(env, new_state));
}

void PeerConnectionObserverJni::OnAddStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  // The stream could be added into the remote_streams_ map when calling
  // OnAddTrack.
  jobject j_stream = GetOrCreateJavaStream(stream);

  for (const auto& track : stream->GetAudioTracks()) {
    AddNativeAudioTrackToJavaStream(track, j_stream);
  }
  for (const auto& track : stream->GetVideoTracks()) {
    AddNativeVideoTrackToJavaStream(track, j_stream);
  }

  Java_Observer_onAddStream(env, *j_observer_global_, j_stream);

  // Create an observer to update the Java stream when the native stream's set
  // of tracks changes.
  auto observer = rtc::MakeUnique<MediaStreamObserver>(stream);
  observer->SignalAudioTrackRemoved.connect(
      this, &PeerConnectionObserverJni::OnAudioTrackRemovedFromStream);
  observer->SignalVideoTrackRemoved.connect(
      this, &PeerConnectionObserverJni::OnVideoTrackRemovedFromStream);
  observer->SignalAudioTrackAdded.connect(
      this, &PeerConnectionObserverJni::OnAudioTrackAddedToStream);
  observer->SignalVideoTrackAdded.connect(
      this, &PeerConnectionObserverJni::OnVideoTrackAddedToStream);
  stream_observers_.push_back(std::move(observer));
}

void PeerConnectionObserverJni::AddNativeAudioTrackToJavaStream(
    rtc::scoped_refptr<AudioTrackInterface> track,
    jobject j_stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_MediaStream_addNativeAudioTrack(env, j_stream,
                                       jlongFromPointer(track.release()));
}

void PeerConnectionObserverJni::AddNativeVideoTrackToJavaStream(
    rtc::scoped_refptr<VideoTrackInterface> track,
    jobject j_stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_MediaStream_addNativeVideoTrack(env, j_stream,
                                       jlongFromPointer(track.release()));
}

void PeerConnectionObserverJni::OnAudioTrackAddedToStream(
    AudioTrackInterface* track,
    MediaStreamInterface* stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject j_stream = GetOrCreateJavaStream(stream);
  AddNativeAudioTrackToJavaStream(track, j_stream);
}

void PeerConnectionObserverJni::OnVideoTrackAddedToStream(
    VideoTrackInterface* track,
    MediaStreamInterface* stream) {
  ScopedLocalRefFrame local_ref_frame(jni());
  jobject j_stream = GetOrCreateJavaStream(stream);
  AddNativeVideoTrackToJavaStream(track, j_stream);
}

void PeerConnectionObserverJni::OnAudioTrackRemovedFromStream(
    AudioTrackInterface* track,
    MediaStreamInterface* stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject j_stream = GetOrCreateJavaStream(stream);
  Java_MediaStream_removeAudioTrack(env, j_stream, jlongFromPointer(track));
}

void PeerConnectionObserverJni::OnVideoTrackRemovedFromStream(
    VideoTrackInterface* track,
    MediaStreamInterface* stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject j_stream = GetOrCreateJavaStream(stream);
  Java_MediaStream_removeVideoTrack(env, j_stream, jlongFromPointer(track));
}

void PeerConnectionObserverJni::OnRemoveStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  NativeToJavaStreamsMap::iterator it = remote_streams_.find(stream);
  RTC_CHECK(it != remote_streams_.end())
      << "unexpected stream: " << std::hex << stream;
  Java_Observer_onRemoveStream(env, *j_observer_global_, it->second);

  // Release the refptr reference so that DisposeRemoteStream can assert
  // it removes the final reference.
  stream = nullptr;
  DisposeRemoteStream(it);
}

void PeerConnectionObserverJni::OnDataChannel(
    rtc::scoped_refptr<DataChannelInterface> channel) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onDataChannel(env, *j_observer_global_,
                              WrapNativeDataChannel(env, channel));
}

void PeerConnectionObserverJni::OnRenegotiationNeeded() {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  Java_Observer_onRenegotiationNeeded(env, *j_observer_global_);
}

void PeerConnectionObserverJni::OnAddTrack(
    rtc::scoped_refptr<RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(env);
  jobject j_rtp_receiver = NativeToJavaRtpReceiver(env, receiver);
  rtp_receivers_.emplace_back(env, j_rtp_receiver);

  Java_Observer_onAddTrack(env, *j_observer_global_, j_rtp_receiver,
                           NativeToJavaMediaStreamArray(env, streams));
}

void PeerConnectionObserverJni::SetConstraints(
    std::unique_ptr<MediaConstraintsInterface> constraints) {
  RTC_CHECK(!constraints_.get()) << "constraints already set!";
  constraints_ = std::move(constraints);
}

void PeerConnectionObserverJni::DisposeRemoteStream(
    const NativeToJavaStreamsMap::iterator& it) {
  MediaStreamInterface* stream = it->first;
  jobject j_stream = it->second;

  // Remove the observer first, so it doesn't react to events during deletion.
  stream_observers_.erase(
      std::remove_if(
          stream_observers_.begin(), stream_observers_.end(),
          [stream](const std::unique_ptr<MediaStreamObserver>& observer) {
            return observer->stream() == stream;
          }),
      stream_observers_.end());

  remote_streams_.erase(it);
  JNIEnv* env = AttachCurrentThreadIfNeeded();
  Java_MediaStream_dispose(env, j_stream);
  DeleteGlobalRef(env, j_stream);
}

// If the NativeToJavaStreamsMap contains the stream, return it.
// Otherwise, create a new Java MediaStream.
jobject PeerConnectionObserverJni::GetOrCreateJavaStream(
    const rtc::scoped_refptr<MediaStreamInterface>& stream) {
  NativeToJavaStreamsMap::iterator it = remote_streams_.find(stream);
  if (it != remote_streams_.end()) {
    return it->second;
  }
  // Java MediaStream holds one reference. Corresponding Release() is in
  // MediaStream_free, triggered by MediaStream.dispose().
  stream->AddRef();
  jobject j_stream =
      Java_MediaStream_Constructor(jni(), jlongFromPointer(stream.get()));

  remote_streams_[stream] = NewGlobalRef(jni(), j_stream);
  return j_stream;
}

jobjectArray PeerConnectionObserverJni::NativeToJavaMediaStreamArray(
    JNIEnv* jni,
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) {
  jobjectArray java_streams = jni->NewObjectArray(
      streams.size(), org_webrtc_MediaStream_clazz(jni), nullptr);
  CHECK_EXCEPTION(jni) << "error during NewObjectArray";
  for (size_t i = 0; i < streams.size(); ++i) {
    jobject j_stream = GetOrCreateJavaStream(streams[i]);
    jni->SetObjectArrayElement(java_streams, i, j_stream);
  }
  return java_streams;
}

}  // namespace jni
}  // namespace webrtc
