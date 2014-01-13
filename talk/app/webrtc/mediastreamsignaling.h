/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#ifndef TALK_APP_WEBRTC_MEDIASTREAMSIGNALING_H_
#define TALK_APP_WEBRTC_MEDIASTREAMSIGNALING_H_

#include <string>
#include <vector>
#include <map>

#include "talk/app/webrtc/datachannel.h"
#include "talk/app/webrtc/mediastream.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/streamcollection.h"
#include "talk/base/scoped_ref_ptr.h"
#include "talk/session/media/mediasession.h"

namespace talk_base {
class Thread;
}  // namespace talk_base

namespace webrtc {

class RemoteMediaStreamFactory;

// A MediaStreamSignalingObserver is notified when events happen to
// MediaStreams, MediaStreamTracks or DataChannels associated with the observed
// MediaStreamSignaling object. The notifications identify the stream, track or
// channel.
class MediaStreamSignalingObserver {
 public:
  // Triggered when the remote SessionDescription has a new stream.
  virtual void OnAddRemoteStream(MediaStreamInterface* stream) = 0;

  // Triggered when the remote SessionDescription removes a stream.
  virtual void OnRemoveRemoteStream(MediaStreamInterface* stream) = 0;

  // Triggered when the remote SessionDescription has a new data channel.
  virtual void OnAddDataChannel(DataChannelInterface* data_channel) = 0;

  // Triggered when the remote SessionDescription has a new audio track.
  virtual void OnAddRemoteAudioTrack(MediaStreamInterface* stream,
                                     AudioTrackInterface* audio_track,
                                     uint32 ssrc) = 0;

  // Triggered when the remote SessionDescription has a new video track.
  virtual void OnAddRemoteVideoTrack(MediaStreamInterface* stream,
                                     VideoTrackInterface* video_track,
                                     uint32 ssrc) = 0;

  // Triggered when the remote SessionDescription has removed an audio track.
  virtual void OnRemoveRemoteAudioTrack(MediaStreamInterface* stream,
                                        AudioTrackInterface* audio_track)  = 0;

  // Triggered when the remote SessionDescription has removed a video track.
  virtual void OnRemoveRemoteVideoTrack(MediaStreamInterface* stream,
                                        VideoTrackInterface* video_track) = 0;

  // Triggered when the local SessionDescription has a new audio track.
  virtual void OnAddLocalAudioTrack(MediaStreamInterface* stream,
                                    AudioTrackInterface* audio_track,
                                    uint32 ssrc) = 0;

  // Triggered when the local SessionDescription has a new video track.
  virtual void OnAddLocalVideoTrack(MediaStreamInterface* stream,
                                    VideoTrackInterface* video_track,
                                    uint32 ssrc) = 0;

  // Triggered when the local SessionDescription has removed an audio track.
  virtual void OnRemoveLocalAudioTrack(MediaStreamInterface* stream,
                                       AudioTrackInterface* audio_track) = 0;

  // Triggered when the local SessionDescription has removed a video track.
  virtual void OnRemoveLocalVideoTrack(MediaStreamInterface* stream,
                                       VideoTrackInterface* video_track) = 0;

  // Triggered when RemoveLocalStream is called. |stream| is no longer used
  // when negotiating and all tracks in |stream| should stop providing data to
  // this PeerConnection. This doesn't mean that the local session description
  // has changed and OnRemoveLocalAudioTrack and OnRemoveLocalVideoTrack is not
  // called for each individual track.
  virtual void OnRemoveLocalStream(MediaStreamInterface* stream) = 0;

 protected:
  ~MediaStreamSignalingObserver() {}
};

// MediaStreamSignaling works as a glue between MediaStreams and a cricket
// classes for SessionDescriptions.
// It is used for creating cricket::MediaSessionOptions given the local
// MediaStreams and data channels.
//
// It is responsible for creating remote MediaStreams given a remote
// SessionDescription and creating cricket::MediaSessionOptions given
// local MediaStreams.
//
// To signal that a DataChannel should be established:
// 1. Call AddDataChannel with the new DataChannel. Next time
//    GetMediaSessionOptions will include the description of the DataChannel.
// 2. When a local session description is set, call UpdateLocalStreams with the
//    session description. This will set the SSRC used for sending data on
//    this DataChannel.
// 3. When remote session description is set, call UpdateRemoteStream with the
//    session description. If the DataChannel label and a SSRC is included in
//    the description, the DataChannel is updated with SSRC that will be used
//    for receiving data.
// 4. When both the local and remote SSRC of a DataChannel is set the state of
//    the DataChannel change to kOpen.
//
// To setup a DataChannel initialized by the remote end.
// 1. When remote session description is set, call UpdateRemoteStream with the
//    session description. If a label and a SSRC of a new DataChannel is found
//    MediaStreamSignalingObserver::OnAddDataChannel with the label and SSRC is
//    triggered.
// 2. Create a DataChannel instance with the label and set the remote SSRC.
// 3. Call AddDataChannel with this new DataChannel.  GetMediaSessionOptions
//    will include the description of the DataChannel.
// 4. Create a local session description and call UpdateLocalStreams. This will
//    set the local SSRC used by the DataChannel.
// 5. When both the local and remote SSRC of a DataChannel is set the state of
//    the DataChannel change to kOpen.
//
// To close a DataChannel:
// 1. Call DataChannel::Close. This will change the state of the DataChannel to
//    kClosing. GetMediaSessionOptions will not
//    include the description of the DataChannel.
// 2. When a local session description is set, call UpdateLocalStreams with the
//    session description. The description will no longer contain the
//    DataChannel label or SSRC.
// 3. When remote session description is set, call UpdateRemoteStream with the
//    session description. The description will no longer contain the
//    DataChannel label or SSRC. The DataChannel SSRC is updated with SSRC=0.
//    The DataChannel change state to kClosed.

class MediaStreamSignaling {
 public:
  MediaStreamSignaling(talk_base::Thread* signaling_thread,
                       MediaStreamSignalingObserver* stream_observer,
                       cricket::ChannelManager* channel_manager);
  virtual ~MediaStreamSignaling();

  // Notify all referenced objects that MediaStreamSignaling will be teared
  // down. This method must be called prior to the dtor.
  void TearDown();

  // Set a factory for creating data channels that are initiated by the remote
  // peer.
  void SetDataChannelFactory(DataChannelFactory* data_channel_factory) {
    data_channel_factory_ = data_channel_factory;
  }

  // Checks if |id| is available to be assigned to a new SCTP data channel.
  bool IsSctpSidAvailable(int sid) const;

  // Gets the first available SCTP id that is not assigned to any existing
  // data channels.
  bool AllocateSctpSid(talk_base::SSLRole role, int* sid);

  // Adds |local_stream| to the collection of known MediaStreams that will be
  // offered in a SessionDescription.
  bool AddLocalStream(MediaStreamInterface* local_stream);

  // Removes |local_stream| from the collection of known MediaStreams that will
  // be offered in a SessionDescription.
  void RemoveLocalStream(MediaStreamInterface* local_stream);

  // Checks if any data channel has been added.
  bool HasDataChannels() const;
  // Adds |data_channel| to the collection of DataChannels that will be
  // be offered in a SessionDescription.
  bool AddDataChannel(DataChannel* data_channel);
  // After we receive an OPEN message, create a data channel and add it.
  bool AddDataChannelFromOpenMessage(
      const std::string& label, const DataChannelInit& config);

  // Returns a MediaSessionOptions struct with options decided by |constraints|,
  // the local MediaStreams and DataChannels.
  virtual bool GetOptionsForOffer(
      const MediaConstraintsInterface* constraints,
      cricket::MediaSessionOptions* options);

  // Returns a MediaSessionOptions struct with options decided by
  // |constraints|, the local MediaStreams and DataChannels.
  virtual bool GetOptionsForAnswer(
      const MediaConstraintsInterface* constraints,
      cricket::MediaSessionOptions* options);

  // Called when the remote session description has changed. The purpose is to
  // update remote MediaStreams and DataChannels with the current
  // session state.
  // If the remote SessionDescription contain information about a new remote
  // MediaStreams a new remote MediaStream is created and
  // MediaStreamSignalingObserver::OnAddStream is called.
  // If a remote MediaStream is missing from
  // the remote SessionDescription MediaStreamSignalingObserver::OnRemoveStream
  // is called.
  // If the SessionDescription contains information about a new DataChannel,
  // MediaStreamSignalingObserver::OnAddDataChannel is called with the
  // DataChannel.
  void OnRemoteDescriptionChanged(const SessionDescriptionInterface* desc);

  // Called when the local session description has changed. The purpose is to
  // update local and remote MediaStreams and DataChannels with the current
  // session state.
  // If |desc| indicates that the media type should be rejected, the method
  // ends the remote MediaStreamTracks.
  // It also updates local DataChannels with information about its local SSRC.
  void OnLocalDescriptionChanged(const SessionDescriptionInterface* desc);

  // Called when the audio channel closes.
  void OnAudioChannelClose();
  // Called when the video channel closes.
  void OnVideoChannelClose();
  // Called when the data channel closes.
  void OnDataChannelClose();

  // Returns the SSRC for a given track.
  bool GetRemoteAudioTrackSsrc(const std::string& track_id, uint32* ssrc) const;
  bool GetRemoteVideoTrackSsrc(const std::string& track_id, uint32* ssrc) const;

  // Returns all current known local MediaStreams.
  StreamCollectionInterface* local_streams() const { return local_streams_;}

  // Returns all current remote MediaStreams.
  StreamCollectionInterface* remote_streams() const {
    return remote_streams_.get();
  }
  void OnDataTransportCreatedForSctp();
  void OnDtlsRoleReadyForSctp(talk_base::SSLRole role);

 private:
  struct RemotePeerInfo {
    RemotePeerInfo()
        : msid_supported(false),
          default_audio_track_needed(false),
          default_video_track_needed(false) {
    }
    // True if it has been discovered that the remote peer support MSID.
    bool msid_supported;
    // The remote peer indicates in the session description that audio will be
    // sent but no MSID is given.
    bool default_audio_track_needed;
    // The remote peer indicates in the session description that video will be
    // sent but no MSID is given.
    bool default_video_track_needed;

    bool IsDefaultMediaStreamNeeded() {
      return !msid_supported && (default_audio_track_needed ||
          default_video_track_needed);
    }
  };

  struct TrackInfo {
    TrackInfo() : ssrc(0) {}
    TrackInfo(const std::string& stream_label,
              const std::string track_id,
              uint32 ssrc)
        : stream_label(stream_label),
          track_id(track_id),
          ssrc(ssrc) {
    }
    std::string stream_label;
    std::string track_id;
    uint32 ssrc;
  };
  typedef std::map<std::string, TrackInfo> TrackInfos;

  void UpdateSessionOptions();

  // Makes sure a MediaStream Track is created for each StreamParam in
  // |streams|. |media_type| is the type of the |streams| and can be either
  // audio or video.
  // If a new MediaStream is created it is added to |new_streams|.
  void UpdateRemoteStreamsList(
      const std::vector<cricket::StreamParams>& streams,
      cricket::MediaType media_type,
      StreamCollection* new_streams);

  // Triggered when a remote track has been seen for the first time in a remote
  // session description. It creates a remote MediaStreamTrackInterface
  // implementation and triggers MediaStreamSignaling::OnAddRemoteAudioTrack or
  // MediaStreamSignaling::OnAddRemoteVideoTrack.
  void OnRemoteTrackSeen(const std::string& stream_label,
                         const std::string& track_id,
                         uint32 ssrc,
                         cricket::MediaType media_type);

  // Triggered when a remote track has been removed from a remote session
  // description. It removes the remote track with id |track_id| from a remote
  // MediaStream and triggers MediaStreamSignaling::OnRemoveRemoteAudioTrack or
  // MediaStreamSignaling::OnRemoveRemoteVideoTrack.
  void OnRemoteTrackRemoved(const std::string& stream_label,
                            const std::string& track_id,
                            cricket::MediaType media_type);

  // Set the MediaStreamTrackInterface::TrackState to |kEnded| on all remote
  // tracks of type |media_type|.
  void RejectRemoteTracks(cricket::MediaType media_type);

  // Finds remote MediaStreams without any tracks and removes them from
  // |remote_streams_| and notifies the observer that the MediaStream no longer
  // exist.
  void UpdateEndedRemoteMediaStreams();
  void MaybeCreateDefaultStream();
  TrackInfos* GetRemoteTracks(cricket::MediaType type);

  // Returns a map of currently negotiated LocalTrackInfo of type |type|.
  TrackInfos* GetLocalTracks(cricket::MediaType type);
  bool FindLocalTrack(const std::string& track_id, cricket::MediaType type);

  // Loops through the vector of |streams| and finds added and removed
  // StreamParams since last time this method was called.
  // For each new or removed StreamParam NotifyLocalTrackAdded or
  // NotifyLocalTrackRemoved in invoked.
  void UpdateLocalTracks(const std::vector<cricket::StreamParams>& streams,
                         cricket::MediaType media_type);

  // Triggered when a local track has been seen for the first time in a local
  // session description.
  // This method triggers MediaStreamSignaling::OnAddLocalAudioTrack or
  // MediaStreamSignaling::OnAddLocalVideoTrack if the rtp streams in the local
  // SessionDescription can be mapped to a MediaStreamTrack in a MediaStream in
  // |local_streams_|
  void OnLocalTrackSeen(const std::string& stream_label,
                        const std::string& track_id,
                        uint32 ssrc,
                        cricket::MediaType media_type);

  // Triggered when a local track has been removed from a local session
  // description.
  // This method triggers MediaStreamSignaling::OnRemoveLocalAudioTrack or
  // MediaStreamSignaling::OnRemoveLocalVideoTrack if a stream has been removed
  // from the local SessionDescription and the stream can be mapped to a
  // MediaStreamTrack in a MediaStream in |local_streams_|.
  void OnLocalTrackRemoved(const std::string& stream_label,
                           const std::string& track_id,
                           cricket::MediaType media_type);

  void UpdateLocalRtpDataChannels(const cricket::StreamParamsVec& streams);
  void UpdateRemoteRtpDataChannels(const cricket::StreamParamsVec& streams);
  void UpdateClosingDataChannels(
      const std::vector<std::string>& active_channels, bool is_local_update);
  void CreateRemoteDataChannel(const std::string& label, uint32 remote_ssrc);

  RemotePeerInfo remote_info_;
  talk_base::Thread* signaling_thread_;
  DataChannelFactory* data_channel_factory_;
  cricket::MediaSessionOptions options_;
  MediaStreamSignalingObserver* stream_observer_;
  talk_base::scoped_refptr<StreamCollection> local_streams_;
  talk_base::scoped_refptr<StreamCollection> remote_streams_;
  talk_base::scoped_ptr<RemoteMediaStreamFactory> remote_stream_factory_;

  TrackInfos remote_audio_tracks_;
  TrackInfos remote_video_tracks_;
  TrackInfos local_audio_tracks_;
  TrackInfos local_video_tracks_;

  int last_allocated_sctp_even_sid_;
  int last_allocated_sctp_odd_sid_;

  typedef std::map<std::string, talk_base::scoped_refptr<DataChannel> >
      RtpDataChannels;
  typedef std::vector<talk_base::scoped_refptr<DataChannel> > SctpDataChannels;
  RtpDataChannels rtp_data_channels_;
  SctpDataChannels sctp_data_channels_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAMSIGNALING_H_
