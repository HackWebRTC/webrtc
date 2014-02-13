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

// This file contains classes for listening on changes on MediaStreams and
// MediaTracks that are connected to a certain PeerConnection.
// Example: If a user sets a rendererer on a remote video track the renderer is
// connected to the appropriate remote video stream.

#ifndef TALK_APP_WEBRTC_MEDIASTREAMHANDLER_H_
#define TALK_APP_WEBRTC_MEDIASTREAMHANDLER_H_

#include <list>
#include <vector>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/mediastreamprovider.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/base/thread.h"
#include "talk/media/base/audiorenderer.h"

namespace webrtc {

// TrackHandler listen to events on a MediaStreamTrackInterface that is
// connected to a certain PeerConnection.
class TrackHandler : public ObserverInterface {
 public:
  TrackHandler(MediaStreamTrackInterface* track, uint32 ssrc);
  virtual ~TrackHandler();
  virtual void OnChanged();
  // Stop using |track_| on this PeerConnection.
  virtual void Stop() = 0;

  MediaStreamTrackInterface*  track() { return track_; }
  uint32 ssrc() const { return ssrc_; }

 protected:
  virtual void OnStateChanged() = 0;
  virtual void OnEnabledChanged() = 0;

 private:
  talk_base::scoped_refptr<MediaStreamTrackInterface> track_;
  uint32 ssrc_;
  MediaStreamTrackInterface::TrackState state_;
  bool enabled_;
};

// LocalAudioSinkAdapter receives data callback as a sink to the local
// AudioTrack, and passes the data to the sink of AudioRenderer.
class LocalAudioSinkAdapter : public AudioTrackSinkInterface,
                              public cricket::AudioRenderer {
 public:
  LocalAudioSinkAdapter();
  virtual ~LocalAudioSinkAdapter();

 private:
  // AudioSinkInterface implementation.
  virtual void OnData(const void* audio_data, int bits_per_sample,
                      int sample_rate, int number_of_channels,
                      int number_of_frames) OVERRIDE;

  // cricket::AudioRenderer implementation.
  virtual void SetSink(cricket::AudioRenderer::Sink* sink) OVERRIDE;

  cricket::AudioRenderer::Sink* sink_;
  // Critical section protecting |sink_|.
  talk_base::CriticalSection lock_;
};

// LocalAudioTrackHandler listen to events on a local AudioTrack instance
// connected to a PeerConnection and orders the |provider| to executes the
// requested change.
class LocalAudioTrackHandler : public TrackHandler {
 public:
  LocalAudioTrackHandler(AudioTrackInterface* track,
                         uint32 ssrc,
                         AudioProviderInterface* provider);
  virtual ~LocalAudioTrackHandler();

  virtual void Stop() OVERRIDE;

 protected:
  virtual void OnStateChanged() OVERRIDE;
  virtual void OnEnabledChanged() OVERRIDE;

 private:
  AudioTrackInterface* audio_track_;
  AudioProviderInterface* provider_;

  // Used to pass the data callback from the |audio_track_| to the other
  // end of cricket::AudioRenderer.
  talk_base::scoped_ptr<LocalAudioSinkAdapter> sink_adapter_;
};

// RemoteAudioTrackHandler listen to events on a remote AudioTrack instance
// connected to a PeerConnection and orders the |provider| to executes the
// requested change.
class RemoteAudioTrackHandler : public AudioSourceInterface::AudioObserver,
                                public TrackHandler {
 public:
  RemoteAudioTrackHandler(AudioTrackInterface* track,
                          uint32 ssrc,
                          AudioProviderInterface* provider);
  virtual ~RemoteAudioTrackHandler();
  virtual void Stop() OVERRIDE;

 protected:
  virtual void OnStateChanged() OVERRIDE;
  virtual void OnEnabledChanged() OVERRIDE;

 private:
  // AudioSourceInterface::AudioObserver implementation.
  virtual void OnSetVolume(double volume) OVERRIDE;

  AudioTrackInterface* audio_track_;
  AudioProviderInterface* provider_;
};

// LocalVideoTrackHandler listen to events on a local VideoTrack instance
// connected to a PeerConnection and orders the |provider| to executes the
// requested change.
class LocalVideoTrackHandler : public TrackHandler {
 public:
  LocalVideoTrackHandler(VideoTrackInterface* track,
                         uint32 ssrc,
                         VideoProviderInterface* provider);
  virtual ~LocalVideoTrackHandler();
  virtual void Stop() OVERRIDE;

 protected:
  virtual void OnStateChanged() OVERRIDE;
  virtual void OnEnabledChanged() OVERRIDE;

 private:
  VideoTrackInterface* local_video_track_;
  VideoProviderInterface* provider_;
};

// RemoteVideoTrackHandler listen to events on a remote VideoTrack instance
// connected to a PeerConnection and orders the |provider| to execute
// requested changes.
class RemoteVideoTrackHandler : public TrackHandler {
 public:
  RemoteVideoTrackHandler(VideoTrackInterface* track,
                          uint32 ssrc,
                          VideoProviderInterface* provider);
  virtual ~RemoteVideoTrackHandler();
  virtual void Stop() OVERRIDE;

 protected:
  virtual void OnStateChanged() OVERRIDE;
  virtual void OnEnabledChanged() OVERRIDE;

 private:
  VideoTrackInterface* remote_video_track_;
  VideoProviderInterface* provider_;
};

class MediaStreamHandler : public ObserverInterface {
 public:
  MediaStreamHandler(MediaStreamInterface* stream,
                     AudioProviderInterface* audio_provider,
                     VideoProviderInterface* video_provider);
  ~MediaStreamHandler();
  MediaStreamInterface* stream();
  void Stop();

  virtual void AddAudioTrack(AudioTrackInterface* audio_track, uint32 ssrc) = 0;
  virtual void AddVideoTrack(VideoTrackInterface* video_track, uint32 ssrc) = 0;

  virtual void RemoveTrack(MediaStreamTrackInterface* track);
  virtual void OnChanged() OVERRIDE;

 protected:
  TrackHandler* FindTrackHandler(MediaStreamTrackInterface* track);
  talk_base::scoped_refptr<MediaStreamInterface> stream_;
  AudioProviderInterface* audio_provider_;
  VideoProviderInterface* video_provider_;
  typedef std::vector<TrackHandler*> TrackHandlers;
  TrackHandlers track_handlers_;
};

class LocalMediaStreamHandler : public MediaStreamHandler {
 public:
  LocalMediaStreamHandler(MediaStreamInterface* stream,
                          AudioProviderInterface* audio_provider,
                          VideoProviderInterface* video_provider);
  ~LocalMediaStreamHandler();

  virtual void AddAudioTrack(AudioTrackInterface* audio_track,
                             uint32 ssrc) OVERRIDE;
  virtual void AddVideoTrack(VideoTrackInterface* video_track,
                             uint32 ssrc) OVERRIDE;
};

class RemoteMediaStreamHandler : public MediaStreamHandler {
 public:
  RemoteMediaStreamHandler(MediaStreamInterface* stream,
                           AudioProviderInterface* audio_provider,
                           VideoProviderInterface* video_provider);
  ~RemoteMediaStreamHandler();
  virtual void AddAudioTrack(AudioTrackInterface* audio_track,
                             uint32 ssrc) OVERRIDE;
  virtual void AddVideoTrack(VideoTrackInterface* video_track,
                             uint32 ssrc) OVERRIDE;
};

// Container for MediaStreamHandlers of currently known local and remote
// MediaStreams.
class MediaStreamHandlerContainer {
 public:
  MediaStreamHandlerContainer(AudioProviderInterface* audio_provider,
                              VideoProviderInterface* video_provider);
  ~MediaStreamHandlerContainer();

  // Notify all referenced objects that MediaStreamHandlerContainer will be
  // destroyed. This method must be called prior to the dtor and prior to the
  // |audio_provider| and |video_provider| is destroyed.
  void TearDown();

  // Remove all TrackHandlers for tracks in |stream| and make sure
  // the audio_provider and video_provider is notified that the tracks has been
  // removed.
  void RemoveRemoteStream(MediaStreamInterface* stream);

  // Create a RemoteAudioTrackHandler and associate |audio_track| with |ssrc|.
  void AddRemoteAudioTrack(MediaStreamInterface* stream,
                           AudioTrackInterface* audio_track,
                           uint32 ssrc);
  // Create a RemoteVideoTrackHandler and associate |video_track| with |ssrc|.
  void AddRemoteVideoTrack(MediaStreamInterface* stream,
                           VideoTrackInterface* video_track,
                           uint32 ssrc);
  // Remove the TrackHandler for |track|.
  void RemoveRemoteTrack(MediaStreamInterface* stream,
                         MediaStreamTrackInterface* track);

  // Remove all TrackHandlers for tracks in |stream| and make sure
  // the audio_provider and video_provider is notified that the tracks has been
  // removed.
  void RemoveLocalStream(MediaStreamInterface* stream);

  // Create a LocalAudioTrackHandler and associate |audio_track| with |ssrc|.
  void AddLocalAudioTrack(MediaStreamInterface* stream,
                          AudioTrackInterface* audio_track,
                          uint32 ssrc);
  // Create a LocalVideoTrackHandler and associate |video_track| with |ssrc|.
  void AddLocalVideoTrack(MediaStreamInterface* stream,
                          VideoTrackInterface* video_track,
                          uint32 ssrc);
  // Remove the TrackHandler for |track|.
  void RemoveLocalTrack(MediaStreamInterface* stream,
                        MediaStreamTrackInterface* track);

 private:
  typedef std::list<MediaStreamHandler*> StreamHandlerList;
  MediaStreamHandler* FindStreamHandler(const StreamHandlerList& handlers,
                                        MediaStreamInterface* stream);
  MediaStreamHandler* CreateRemoteStreamHandler(MediaStreamInterface* stream);
  MediaStreamHandler* CreateLocalStreamHandler(MediaStreamInterface* stream);
  void DeleteStreamHandler(StreamHandlerList* streamhandlers,
                           MediaStreamInterface* stream);

  StreamHandlerList local_streams_handlers_;
  StreamHandlerList remote_streams_handlers_;
  AudioProviderInterface* audio_provider_;
  VideoProviderInterface* video_provider_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAMHANDLER_H_
