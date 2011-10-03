/*
 * libjingle
 * Copyright 2011, Google Inc.
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
// MediaTracks and making sure appropriate action is taken.
// Example: If a user sets a rendererer on a local video track the renderer is
// connected to the appropriate camera.

#ifndef TALK_APP_WEBRTC_DEV_MEDIASTREAMHANDLER_H_
#define TALK_APP_WEBRTC_DEV_MEDIASTREAMHANDLER_H_

#include <list>
#include <vector>

#include "talk/app/webrtc_dev/mediastream.h"
#include "talk/app/webrtc_dev/mediastreamprovider.h"
#include "talk/app/webrtc_dev/peerconnection.h"
#include "talk/base/thread.h"

namespace webrtc {

// VideoTrackHandler listen to events on a VideoTrack instance and
// executes the requested change.
class VideoTrackHandler : public Observer,
                          public talk_base::MessageHandler {
 public:
  VideoTrackHandler(VideoTrack* track,
                    MediaProviderInterface* provider);
  virtual ~VideoTrackHandler();
  virtual void OnChanged();

 protected:
  virtual void OnMessage(talk_base::Message* msg);

  virtual void OnRendererChanged() = 0;
  virtual void OnStateChanged(MediaStreamTrack::TrackState state) = 0;
  virtual void OnEnabledChanged(bool enabled) = 0;

  MediaProviderInterface* provider_;
  scoped_refptr<VideoTrack> video_track_;

 private:
  MediaStreamTrack::TrackState state_;
  bool enabled_;
  scoped_refptr<VideoRenderer> renderer_;
  talk_base::Thread* signaling_thread_;
};

class LocalVideoTrackHandler : public VideoTrackHandler {
 public:
  LocalVideoTrackHandler(VideoTrack* track,
                         MediaProviderInterface* provider);

 protected:
  virtual void OnRendererChanged();
  virtual void OnStateChanged(MediaStreamTrack::TrackState state);
  virtual void OnEnabledChanged(bool enabled);
};

class RemoteVideoTrackHandler : public VideoTrackHandler {
 public:
  RemoteVideoTrackHandler(VideoTrack* track,
                          MediaProviderInterface* provider);

 protected:
  virtual void OnRendererChanged();
  virtual void OnStateChanged(MediaStreamTrack::TrackState state);
  virtual void OnEnabledChanged(bool enabled);
};

class MediaStreamHandler : public Observer {
 public:
  MediaStreamHandler(MediaStream* stream, MediaProviderInterface* provider);
  ~MediaStreamHandler();
  MediaStream* stream();
  virtual void OnChanged();

 protected:
  MediaProviderInterface* provider_;
  typedef std::vector<VideoTrackHandler*> VideoTrackHandlers;
  VideoTrackHandlers video_handlers_;
  scoped_refptr<MediaStream> stream_;
};

class LocalMediaStreamHandler : public MediaStreamHandler {
 public:
  LocalMediaStreamHandler(MediaStream* stream,
                          MediaProviderInterface* provider);
};

class RemoteMediaStreamHandler : public MediaStreamHandler {
 public:
  RemoteMediaStreamHandler(MediaStream* stream,
                           MediaProviderInterface* provider);
};

class MediaStreamHandlers {
 public:
  explicit MediaStreamHandlers(MediaProviderInterface* provider);
  ~MediaStreamHandlers();
  void AddRemoteStream(MediaStream* stream);
  void RemoveRemoteStream(MediaStream* stream);
  void CommitLocalStreams(StreamCollection* streams);

 private:
  typedef std::list<MediaStreamHandler*> StreamHandlerList;
  StreamHandlerList local_streams_handlers_;
  StreamHandlerList remote_streams_handlers_;
  MediaProviderInterface* provider_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAMOBSERVER_H_

