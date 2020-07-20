/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_ANDROIDVOIP_JNI_ANDROID_VOIP_CLIENT_H_
#define EXAMPLES_ANDROIDVOIP_JNI_ANDROID_VOIP_CLIENT_H_

#include <jni.h>

#include <memory>
#include <string>
#include <vector>

#include "api/audio_codecs/audio_format.h"
#include "api/call/transport.h"
#include "api/voip/voip_base.h"
#include "api/voip/voip_engine.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

namespace webrtc_examples {

// AndroidVoipClient facilitates the use of the VoIP API defined in
// api/voip/voip_engine.h. One instance of AndroidVoipClient should
// suffice for most VoIP applications. AndroidVoipClient implements
// webrtc::Transport to send RTP/RTCP packets to the remote endpoint.
// It also creates methods (slots) for sockets to connect to in
// order to receive RTP/RTCP packets. AndroidVoipClient does all
// VoipBase related operations with rtc::Thread (voip_thread_), this
// is to comply with consistent thread usage requirement with
// ProcessThread used within VoipEngine. AndroidVoipClient is meant
// to be used by Java through JNI.
class AndroidVoipClient : public webrtc::Transport,
                          public sigslot::has_slots<> {
 public:
  // Returns a pointer to an AndroidVoipClient object. Clients should
  // use this factory method to create AndroidVoipClient objects. The
  // method will return a nullptr in case of initialization errors.
  // It is the client's responsibility to delete the pointer when
  // they are done with it (this class provides a Delete() method).
  static AndroidVoipClient* Create(
      JNIEnv* env,
      const webrtc::JavaParamRef<jobject>& application_context);

  ~AndroidVoipClient() override;

  // Returns a Java List of Strings containing names of the built-in
  // supported codecs.
  webrtc::ScopedJavaLocalRef<jobject> GetSupportedCodecs(JNIEnv* env);

  // Returns a Java String of the default local IPv4 address. If IPv4
  // address is not found, returns the default local IPv6 address. If
  // IPv6 address is not found, returns an empty string.
  webrtc::ScopedJavaLocalRef<jstring> GetLocalIPAddress(JNIEnv* env);

  // Sets the encoder used by the VoIP API.
  void SetEncoder(JNIEnv* env,
                  const webrtc::JavaRef<jstring>& j_encoder_string);

  // Sets the decoders used by the VoIP API.
  void SetDecoders(JNIEnv* env,
                   const webrtc::JavaParamRef<jobject>& j_decoder_strings);

  // Sets two local/remote addresses, one for RTP packets, and another for
  // RTCP packets. The RTP address will have IP address j_ip_address_string
  // and port number j_port_number_int, the RTCP address will have IP address
  // j_ip_address_string and port number j_port_number_int+1.
  void SetLocalAddress(JNIEnv* env,
                       const webrtc::JavaRef<jstring>& j_ip_address_string,
                       jint j_port_number_int);
  void SetRemoteAddress(JNIEnv* env,
                        const webrtc::JavaRef<jstring>& j_ip_address_string,
                        jint j_port_number_int);

  // Starts a VoIP session. The VoIP operations below can only be
  // used after a session has already started. Returns true if session
  // started successfully and false otherwise.
  jboolean StartSession(JNIEnv* env);

  // Stops the current session. Returns true if session stopped
  // successfully and false otherwise.
  jboolean StopSession(JNIEnv* env);

  // Starts sending RTP/RTCP packets to the remote endpoint. Returns
  // the return value of StartSend in api/voip/voip_base.h.
  jboolean StartSend(JNIEnv* env);

  // Stops sending RTP/RTCP packets to the remote endpoint. Returns
  // the return value of StopSend in api/voip/voip_base.h.
  jboolean StopSend(JNIEnv* env);

  // Starts playing out the voice data received from the remote endpoint.
  // Returns the return value of StartPlayout in api/voip/voip_base.h.
  jboolean StartPlayout(JNIEnv* env);

  // Stops playing out the voice data received from the remote endpoint.
  // Returns the return value of StopPlayout in api/voip/voip_base.h.
  jboolean StopPlayout(JNIEnv* env);

  // Deletes this object. Used by client when they are done.
  void Delete(JNIEnv* env);

  // Implementation for Transport.
  bool SendRtp(const uint8_t* packet,
               size_t length,
               const webrtc::PacketOptions& options) override;
  bool SendRtcp(const uint8_t* packet, size_t length) override;

  // Slots for sockets to connect to.
  void OnSignalReadRTPPacket(rtc::AsyncPacketSocket* socket,
                             const char* rtp_packet,
                             size_t size,
                             const rtc::SocketAddress& addr,
                             const int64_t& timestamp);
  void OnSignalReadRTCPPacket(rtc::AsyncPacketSocket* socket,
                              const char* rtcp_packet,
                              size_t size,
                              const rtc::SocketAddress& addr,
                              const int64_t& timestamp);

 private:
  AndroidVoipClient(JNIEnv* env,
                    const webrtc::JavaParamRef<jobject>& application_context);

  // Used to invoke VoipBase operations and send/receive
  // RTP/RTCP packets.
  std::unique_ptr<rtc::Thread> voip_thread_;
  // A list of AudioCodecSpec supported by the built-in
  // encoder/decoder factories.
  std::vector<webrtc::AudioCodecSpec> supported_codecs_;
  // The entry point to all VoIP APIs.
  std::unique_ptr<webrtc::VoipEngine> voip_engine_;
  // Used by the VoIP API to facilitate a VoIP session.
  absl::optional<webrtc::ChannelId> channel_;
  // Members below are used for network related operations.
  std::unique_ptr<rtc::AsyncUDPSocket> rtp_socket_;
  std::unique_ptr<rtc::AsyncUDPSocket> rtcp_socket_;
  rtc::SocketAddress rtp_local_address_;
  rtc::SocketAddress rtcp_local_address_;
  rtc::SocketAddress rtp_remote_address_;
  rtc::SocketAddress rtcp_remote_address_;
};

}  // namespace webrtc_examples

#endif  // EXAMPLES_ANDROIDVOIP_JNI_ANDROID_VOIP_CLIENT_H_
