/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_PEERCONNECTIONOBSERVER_JNI_H_
#define SDK_ANDROID_SRC_JNI_PC_PEERCONNECTIONOBSERVER_JNI_H_

#include <map>
#include <memory>
#include <vector>

#include "api/peerconnectioninterface.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/mediaconstraints_jni.h"

namespace webrtc {
namespace jni {

// Adapter between the C++ PeerConnectionObserver interface and the Java
// PeerConnection.Observer interface.  Wraps an instance of the Java interface
// and dispatches C++ callbacks to Java.
class PeerConnectionObserverJni : public PeerConnectionObserver {
 public:
  PeerConnectionObserverJni(JNIEnv* jni, jobject j_observer);
  virtual ~PeerConnectionObserverJni();

  // Implementation of PeerConnectionObserver interface, which propagates
  // the callbacks to the Java observer.
  void OnIceCandidate(const IceCandidateInterface* candidate) override;
  void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates) override;
  void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) override;
  void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceConnectionReceivingChange(bool receiving) override;
  void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) override;
  void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) override;
  void OnRemoveStream(rtc::scoped_refptr<MediaStreamInterface> stream) override;
  void OnDataChannel(rtc::scoped_refptr<DataChannelInterface> channel) override;
  void OnRenegotiationNeeded() override;
  void OnAddTrack(rtc::scoped_refptr<RtpReceiverInterface> receiver,
                  const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
                      streams) override;

  void SetConstraints(MediaConstraintsJni* constraints);
  const MediaConstraintsJni* constraints() { return constraints_.get(); }

 private:
  typedef std::map<MediaStreamInterface*, jobject> NativeToJavaStreamsMap;
  typedef std::map<RtpReceiverInterface*, jobject> NativeToJavaRtpReceiverMap;

  void DisposeRemoteStream(const NativeToJavaStreamsMap::iterator& it);
  void DisposeRtpReceiver(const NativeToJavaRtpReceiverMap::iterator& it);

  // If the NativeToJavaStreamsMap contains the stream, return it.
  // Otherwise, create a new Java MediaStream.
  jobject GetOrCreateJavaStream(
      const rtc::scoped_refptr<MediaStreamInterface>& stream);

  // Converts array of streams, creating or re-using Java streams as necessary.
  jobjectArray NativeToJavaMediaStreamArray(
      JNIEnv* jni,
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams);

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
  const ScopedGlobalRef<jclass> j_rtp_receiver_class_;
  const jmethodID j_rtp_receiver_ctor_;
  // C++ -> Java remote streams. The stored jobects are global refs and must be
  // manually deleted upon removal. Use DisposeRemoteStream().
  NativeToJavaStreamsMap remote_streams_;
  NativeToJavaRtpReceiverMap rtp_receivers_;
  std::unique_ptr<MediaConstraintsJni> constraints_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_PEERCONNECTIONOBSERVER_JNI_H_
