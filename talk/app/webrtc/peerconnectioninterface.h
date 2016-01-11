/*
 * libjingle
 * Copyright 2012 Google Inc.
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

// This file contains the PeerConnection interface as defined in
// http://dev.w3.org/2011/webrtc/editor/webrtc.html#peer-to-peer-connections.
// Applications must use this interface to implement peerconnection.
// PeerConnectionFactory class provides factory methods to create
// peerconnection, mediastream and media tracks objects.
//
// The Following steps are needed to setup a typical call using Jsep.
// 1. Create a PeerConnectionFactoryInterface. Check constructors for more
// information about input parameters.
// 2. Create a PeerConnection object. Provide a configuration string which
// points either to stun or turn server to generate ICE candidates and provide
// an object that implements the PeerConnectionObserver interface.
// 3. Create local MediaStream and MediaTracks using the PeerConnectionFactory
// and add it to PeerConnection by calling AddStream.
// 4. Create an offer and serialize it and send it to the remote peer.
// 5. Once an ice candidate have been found PeerConnection will call the
// observer function OnIceCandidate. The candidates must also be serialized and
// sent to the remote peer.
// 6. Once an answer is received from the remote peer, call
// SetLocalSessionDescription with the offer and SetRemoteSessionDescription
// with the remote answer.
// 7. Once a remote candidate is received from the remote peer, provide it to
// the peerconnection by calling AddIceCandidate.


// The Receiver of a call can decide to accept or reject the call.
// This decision will be taken by the application not peerconnection.
// If application decides to accept the call
// 1. Create PeerConnectionFactoryInterface if it doesn't exist.
// 2. Create a new PeerConnection.
// 3. Provide the remote offer to the new PeerConnection object by calling
// SetRemoteSessionDescription.
// 4. Generate an answer to the remote offer by calling CreateAnswer and send it
// back to the remote peer.
// 5. Provide the local answer to the new PeerConnection by calling
// SetLocalSessionDescription with the answer.
// 6. Provide the remote ice candidates by calling AddIceCandidate.
// 7. Once a candidate have been found PeerConnection will call the observer
// function OnIceCandidate. Send these candidates to the remote peer.

#ifndef TALK_APP_WEBRTC_PEERCONNECTIONINTERFACE_H_
#define TALK_APP_WEBRTC_PEERCONNECTIONINTERFACE_H_

#include <string>
#include <utility>
#include <vector>

#include "talk/app/webrtc/datachannelinterface.h"
#include "talk/app/webrtc/dtlsidentitystore.h"
#include "talk/app/webrtc/dtmfsenderinterface.h"
#include "talk/app/webrtc/dtlsidentitystore.h"
#include "talk/app/webrtc/jsep.h"
#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/rtpreceiverinterface.h"
#include "talk/app/webrtc/rtpsenderinterface.h"
#include "talk/app/webrtc/statstypes.h"
#include "talk/app/webrtc/umametrics.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/network.h"
#include "webrtc/base/rtccertificate.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/p2p/base/portallocator.h"

namespace rtc {
class SSLIdentity;
class Thread;
}

namespace cricket {
class WebRtcVideoDecoderFactory;
class WebRtcVideoEncoderFactory;
}

namespace webrtc {
class AudioDeviceModule;
class MediaConstraintsInterface;

// MediaStream container interface.
class StreamCollectionInterface : public rtc::RefCountInterface {
 public:
  // TODO(ronghuawu): Update the function names to c++ style, e.g. find -> Find.
  virtual size_t count() = 0;
  virtual MediaStreamInterface* at(size_t index) = 0;
  virtual MediaStreamInterface* find(const std::string& label) = 0;
  virtual MediaStreamTrackInterface* FindAudioTrack(
      const std::string& id) = 0;
  virtual MediaStreamTrackInterface* FindVideoTrack(
      const std::string& id) = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~StreamCollectionInterface() {}
};

class StatsObserver : public rtc::RefCountInterface {
 public:
  virtual void OnComplete(const StatsReports& reports) = 0;

 protected:
  virtual ~StatsObserver() {}
};

class MetricsObserverInterface : public rtc::RefCountInterface {
 public:

  // |type| is the type of the enum counter to be incremented. |counter|
  // is the particular counter in that type. |counter_max| is the next sequence
  // number after the highest counter.
  virtual void IncrementEnumCounter(PeerConnectionEnumCounterType type,
                                    int counter,
                                    int counter_max) {}

  // This is used to handle sparse counters like SSL cipher suites.
  // TODO(guoweis): Remove the implementation once the dependency's interface
  // definition is updated.
  virtual void IncrementSparseEnumCounter(PeerConnectionEnumCounterType type,
                                          int counter) {
    IncrementEnumCounter(type, counter, 0 /* Ignored */);
  }

  virtual void AddHistogramSample(PeerConnectionMetricsName type,
                                  int value) = 0;

 protected:
  virtual ~MetricsObserverInterface() {}
};

typedef MetricsObserverInterface UMAObserver;

class PeerConnectionInterface : public rtc::RefCountInterface {
 public:
  // See http://dev.w3.org/2011/webrtc/editor/webrtc.html#state-definitions .
  enum SignalingState {
    kStable,
    kHaveLocalOffer,
    kHaveLocalPrAnswer,
    kHaveRemoteOffer,
    kHaveRemotePrAnswer,
    kClosed,
  };

  // TODO(bemasc): Remove IceState when callers are changed to
  // IceConnection/GatheringState.
  enum IceState {
    kIceNew,
    kIceGathering,
    kIceWaiting,
    kIceChecking,
    kIceConnected,
    kIceCompleted,
    kIceFailed,
    kIceClosed,
  };

  enum IceGatheringState {
    kIceGatheringNew,
    kIceGatheringGathering,
    kIceGatheringComplete
  };

  enum IceConnectionState {
    kIceConnectionNew,
    kIceConnectionChecking,
    kIceConnectionConnected,
    kIceConnectionCompleted,
    kIceConnectionFailed,
    kIceConnectionDisconnected,
    kIceConnectionClosed,
    kIceConnectionMax,
  };

  struct IceServer {
    // TODO(jbauch): Remove uri when all code using it has switched to urls.
    std::string uri;
    std::vector<std::string> urls;
    std::string username;
    std::string password;
  };
  typedef std::vector<IceServer> IceServers;

  enum IceTransportsType {
    // TODO(pthatcher): Rename these kTransporTypeXXX, but update
    // Chromium at the same time.
    kNone,
    kRelay,
    kNoHost,
    kAll
  };

  // https://tools.ietf.org/html/draft-ietf-rtcweb-jsep-08#section-4.1.1
  enum BundlePolicy {
    kBundlePolicyBalanced,
    kBundlePolicyMaxBundle,
    kBundlePolicyMaxCompat
  };

  // https://tools.ietf.org/html/draft-ietf-rtcweb-jsep-09#section-4.1.1
  enum RtcpMuxPolicy {
    kRtcpMuxPolicyNegotiate,
    kRtcpMuxPolicyRequire,
  };

  enum TcpCandidatePolicy {
    kTcpCandidatePolicyEnabled,
    kTcpCandidatePolicyDisabled
  };

  enum ContinualGatheringPolicy {
    GATHER_ONCE,
    GATHER_CONTINUALLY
  };

  // TODO(hbos): Change into class with private data and public getters.
  struct RTCConfiguration {
    static const int kUndefined = -1;
    // Default maximum number of packets in the audio jitter buffer.
    static const int kAudioJitterBufferMaxPackets = 50;
    // TODO(pthatcher): Rename this ice_transport_type, but update
    // Chromium at the same time.
    IceTransportsType type;
    // TODO(pthatcher): Rename this ice_servers, but update Chromium
    // at the same time.
    IceServers servers;
    BundlePolicy bundle_policy;
    RtcpMuxPolicy rtcp_mux_policy;
    TcpCandidatePolicy tcp_candidate_policy;
    int audio_jitter_buffer_max_packets;
    bool audio_jitter_buffer_fast_accelerate;
    int ice_connection_receiving_timeout;         // ms
    int ice_backup_candidate_pair_ping_interval;  // ms
    ContinualGatheringPolicy continual_gathering_policy;
    std::vector<rtc::scoped_refptr<rtc::RTCCertificate>> certificates;
    bool disable_prerenderer_smoothing;
    RTCConfiguration()
        : type(kAll),
          bundle_policy(kBundlePolicyBalanced),
          rtcp_mux_policy(kRtcpMuxPolicyNegotiate),
          tcp_candidate_policy(kTcpCandidatePolicyEnabled),
          audio_jitter_buffer_max_packets(kAudioJitterBufferMaxPackets),
          audio_jitter_buffer_fast_accelerate(false),
          ice_connection_receiving_timeout(kUndefined),
          ice_backup_candidate_pair_ping_interval(kUndefined),
          continual_gathering_policy(GATHER_ONCE),
          disable_prerenderer_smoothing(false) {}
  };

  struct RTCOfferAnswerOptions {
    static const int kUndefined = -1;
    static const int kMaxOfferToReceiveMedia = 1;

    // The default value for constraint offerToReceiveX:true.
    static const int kOfferToReceiveMediaTrue = 1;

    int offer_to_receive_video;
    int offer_to_receive_audio;
    bool voice_activity_detection;
    bool ice_restart;
    bool use_rtp_mux;

    RTCOfferAnswerOptions()
        : offer_to_receive_video(kUndefined),
          offer_to_receive_audio(kUndefined),
          voice_activity_detection(true),
          ice_restart(false),
          use_rtp_mux(true) {}

    RTCOfferAnswerOptions(int offer_to_receive_video,
                          int offer_to_receive_audio,
                          bool voice_activity_detection,
                          bool ice_restart,
                          bool use_rtp_mux)
        : offer_to_receive_video(offer_to_receive_video),
          offer_to_receive_audio(offer_to_receive_audio),
          voice_activity_detection(voice_activity_detection),
          ice_restart(ice_restart),
          use_rtp_mux(use_rtp_mux) {}
  };

  // Used by GetStats to decide which stats to include in the stats reports.
  // |kStatsOutputLevelStandard| includes the standard stats for Javascript API;
  // |kStatsOutputLevelDebug| includes both the standard stats and additional
  // stats for debugging purposes.
  enum StatsOutputLevel {
    kStatsOutputLevelStandard,
    kStatsOutputLevelDebug,
  };

  // Accessor methods to active local streams.
  virtual rtc::scoped_refptr<StreamCollectionInterface>
      local_streams() = 0;

  // Accessor methods to remote streams.
  virtual rtc::scoped_refptr<StreamCollectionInterface>
      remote_streams() = 0;

  // Add a new MediaStream to be sent on this PeerConnection.
  // Note that a SessionDescription negotiation is needed before the
  // remote peer can receive the stream.
  virtual bool AddStream(MediaStreamInterface* stream) = 0;

  // Remove a MediaStream from this PeerConnection.
  // Note that a SessionDescription negotiation is need before the
  // remote peer is notified.
  virtual void RemoveStream(MediaStreamInterface* stream) = 0;

  // Returns pointer to the created DtmfSender on success.
  // Otherwise returns NULL.
  virtual rtc::scoped_refptr<DtmfSenderInterface> CreateDtmfSender(
      AudioTrackInterface* track) = 0;

  // TODO(deadbeef): Make these pure virtual once all subclasses implement them.
  // |kind| must be "audio" or "video".
  // |stream_id| is used to populate the msid attribute; if empty, one will
  // be generated automatically.
  virtual rtc::scoped_refptr<RtpSenderInterface> CreateSender(
      const std::string& kind,
      const std::string& stream_id) {
    return rtc::scoped_refptr<RtpSenderInterface>();
  }

  virtual std::vector<rtc::scoped_refptr<RtpSenderInterface>> GetSenders()
      const {
    return std::vector<rtc::scoped_refptr<RtpSenderInterface>>();
  }

  virtual std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceivers()
      const {
    return std::vector<rtc::scoped_refptr<RtpReceiverInterface>>();
  }

  virtual bool GetStats(StatsObserver* observer,
                        MediaStreamTrackInterface* track,
                        StatsOutputLevel level) = 0;

  virtual rtc::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const DataChannelInit* config) = 0;

  virtual const SessionDescriptionInterface* local_description() const = 0;
  virtual const SessionDescriptionInterface* remote_description() const = 0;

  // Create a new offer.
  // The CreateSessionDescriptionObserver callback will be called when done.
  virtual void CreateOffer(CreateSessionDescriptionObserver* observer,
                           const MediaConstraintsInterface* constraints) {}

  // TODO(jiayl): remove the default impl and the old interface when chromium
  // code is updated.
  virtual void CreateOffer(CreateSessionDescriptionObserver* observer,
                           const RTCOfferAnswerOptions& options) {}

  // Create an answer to an offer.
  // The CreateSessionDescriptionObserver callback will be called when done.
  virtual void CreateAnswer(CreateSessionDescriptionObserver* observer,
                            const MediaConstraintsInterface* constraints) = 0;
  // Sets the local session description.
  // JsepInterface takes the ownership of |desc| even if it fails.
  // The |observer| callback will be called when done.
  virtual void SetLocalDescription(SetSessionDescriptionObserver* observer,
                                   SessionDescriptionInterface* desc) = 0;
  // Sets the remote session description.
  // JsepInterface takes the ownership of |desc| even if it fails.
  // The |observer| callback will be called when done.
  virtual void SetRemoteDescription(SetSessionDescriptionObserver* observer,
                                    SessionDescriptionInterface* desc) = 0;
  // Restarts or updates the ICE Agent process of gathering local candidates
  // and pinging remote candidates.
  // TODO(deadbeef): Remove once Chrome is moved over to SetConfiguration.
  virtual bool UpdateIce(const IceServers& configuration,
                         const MediaConstraintsInterface* constraints) {
    return false;
  }
  // Sets the PeerConnection's global configuration to |config|.
  // Any changes to STUN/TURN servers or ICE candidate policy will affect the
  // next gathering phase, and cause the next call to createOffer to generate
  // new ICE credentials. Note that the BUNDLE and RTCP-multiplexing policies
  // cannot be changed with this method.
  // TODO(deadbeef): Make this pure virtual once all Chrome subclasses of
  // PeerConnectionInterface implement it.
  virtual bool SetConfiguration(
      const PeerConnectionInterface::RTCConfiguration& config) {
    return false;
  }
  // Provides a remote candidate to the ICE Agent.
  // A copy of the |candidate| will be created and added to the remote
  // description. So the caller of this method still has the ownership of the
  // |candidate|.
  // TODO(ronghuawu): Consider to change this so that the AddIceCandidate will
  // take the ownership of the |candidate|.
  virtual bool AddIceCandidate(const IceCandidateInterface* candidate) = 0;

  virtual void RegisterUMAObserver(UMAObserver* observer) = 0;

  // Returns the current SignalingState.
  virtual SignalingState signaling_state() = 0;

  // TODO(bemasc): Remove ice_state when callers are changed to
  // IceConnection/GatheringState.
  // Returns the current IceState.
  virtual IceState ice_state() = 0;
  virtual IceConnectionState ice_connection_state() = 0;
  virtual IceGatheringState ice_gathering_state() = 0;

  // Terminates all media and closes the transport.
  virtual void Close() = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionInterface() {}
};

// PeerConnection callback interface. Application should implement these
// methods.
class PeerConnectionObserver {
 public:
  enum StateType {
    kSignalingState,
    kIceState,
  };

  // Triggered when the SignalingState changed.
  virtual void OnSignalingChange(
     PeerConnectionInterface::SignalingState new_state) {}

  // Triggered when SignalingState or IceState have changed.
  // TODO(bemasc): Remove once callers transition to OnSignalingChange.
  virtual void OnStateChange(StateType state_changed) {}

  // Triggered when media is received on a new stream from remote peer.
  virtual void OnAddStream(MediaStreamInterface* stream) = 0;

  // Triggered when a remote peer close a stream.
  virtual void OnRemoveStream(MediaStreamInterface* stream) = 0;

  // Triggered when a remote peer open a data channel.
  virtual void OnDataChannel(DataChannelInterface* data_channel) = 0;

  // Triggered when renegotiation is needed, for example the ICE has restarted.
  virtual void OnRenegotiationNeeded() = 0;

  // Called any time the IceConnectionState changes
  virtual void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) {}

  // Called any time the IceGatheringState changes
  virtual void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) {}

  // New Ice candidate have been found.
  virtual void OnIceCandidate(const IceCandidateInterface* candidate) = 0;

  // TODO(bemasc): Remove this once callers transition to OnIceGatheringChange.
  // All Ice candidates have been found.
  virtual void OnIceComplete() {}

  // Called when the ICE connection receiving status changes.
  virtual void OnIceConnectionReceivingChange(bool receiving) {}

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionObserver() {}
};

// PeerConnectionFactoryInterface is the factory interface use for creating
// PeerConnection, MediaStream and media tracks.
// PeerConnectionFactoryInterface will create required libjingle threads,
// socket and network manager factory classes for networking.
// If an application decides to provide its own threads and network
// implementation of these classes it should use the alternate
// CreatePeerConnectionFactory method which accepts threads as input and use the
// CreatePeerConnection version that takes a PortAllocator as an
// argument.
class PeerConnectionFactoryInterface : public rtc::RefCountInterface {
 public:
  class Options {
   public:
    Options()
        : disable_encryption(false),
          disable_sctp_data_channels(false),
          disable_network_monitor(false),
          network_ignore_mask(rtc::kDefaultNetworkIgnoreMask),
          ssl_max_version(rtc::SSL_PROTOCOL_DTLS_12) {}
    bool disable_encryption;
    bool disable_sctp_data_channels;
    bool disable_network_monitor;

    // Sets the network types to ignore. For instance, calling this with
    // ADAPTER_TYPE_ETHERNET | ADAPTER_TYPE_LOOPBACK will ignore Ethernet and
    // loopback interfaces.
    int network_ignore_mask;

    // Sets the maximum supported protocol version. The highest version
    // supported by both ends will be used for the connection, i.e. if one
    // party supports DTLS 1.0 and the other DTLS 1.2, DTLS 1.0 will be used.
    rtc::SSLProtocolVersion ssl_max_version;
  };

  virtual void SetOptions(const Options& options) = 0;

  virtual rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration& configuration,
      const MediaConstraintsInterface* constraints,
      rtc::scoped_ptr<cricket::PortAllocator> allocator,
      rtc::scoped_ptr<DtlsIdentityStoreInterface> dtls_identity_store,
      PeerConnectionObserver* observer) = 0;

  virtual rtc::scoped_refptr<MediaStreamInterface>
      CreateLocalMediaStream(const std::string& label) = 0;

  // Creates a AudioSourceInterface.
  // |constraints| decides audio processing settings but can be NULL.
  virtual rtc::scoped_refptr<AudioSourceInterface> CreateAudioSource(
      const MediaConstraintsInterface* constraints) = 0;

  // Creates a VideoSourceInterface. The new source take ownership of
  // |capturer|. |constraints| decides video resolution and frame rate but can
  // be NULL.
  virtual rtc::scoped_refptr<VideoSourceInterface> CreateVideoSource(
      cricket::VideoCapturer* capturer,
      const MediaConstraintsInterface* constraints) = 0;

  // Creates a new local VideoTrack. The same |source| can be used in several
  // tracks.
  virtual rtc::scoped_refptr<VideoTrackInterface>
      CreateVideoTrack(const std::string& label,
                       VideoSourceInterface* source) = 0;

  // Creates an new AudioTrack. At the moment |source| can be NULL.
  virtual rtc::scoped_refptr<AudioTrackInterface>
      CreateAudioTrack(const std::string& label,
                       AudioSourceInterface* source) = 0;

  // Starts AEC dump using existing file. Takes ownership of |file| and passes
  // it on to VoiceEngine (via other objects) immediately, which will take
  // the ownerhip. If the operation fails, the file will be closed.
  // TODO(grunell): Remove when Chromium has started to use AEC in each source.
  // http://crbug.com/264611.
  virtual bool StartAecDump(rtc::PlatformFile file) = 0;

  // Stops logging the AEC dump.
  virtual void StopAecDump() = 0;

  // Starts RtcEventLog using existing file. Takes ownership of |file| and
  // passes it on to VoiceEngine, which will take the ownership. If the
  // operation fails the file will be closed. The logging will stop
  // automatically after 10 minutes have passed, or when the StopRtcEventLog
  // function is called.
  // This function as well as the StopRtcEventLog don't really belong on this
  // interface, this is a temporary solution until we move the logging object
  // from inside voice engine to webrtc::Call, which will happen when the VoE
  // restructuring effort is further along.
  // TODO(ivoc): Move this into being:
  //             PeerConnection => MediaController => webrtc::Call.
  virtual bool StartRtcEventLog(rtc::PlatformFile file) = 0;

  // Stops logging the RtcEventLog.
  virtual void StopRtcEventLog() = 0;

 protected:
  // Dtor and ctor protected as objects shouldn't be created or deleted via
  // this interface.
  PeerConnectionFactoryInterface() {}
  ~PeerConnectionFactoryInterface() {} // NOLINT
};

// Create a new instance of PeerConnectionFactoryInterface.
rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory();

// Create a new instance of PeerConnectionFactoryInterface.
// Ownership of |factory|, |default_adm|, and optionally |encoder_factory| and
// |decoder_factory| transferred to the returned factory.
rtc::scoped_refptr<PeerConnectionFactoryInterface>
CreatePeerConnectionFactory(
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    AudioDeviceModule* default_adm,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory);

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTIONINTERFACE_H_
