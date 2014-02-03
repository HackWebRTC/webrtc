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

// This file contains interfaces for MediaStream, MediaTrack and MediaSource.
// These interfaces are used for implementing MediaStream and MediaTrack as
// defined in http://dev.w3.org/2011/webrtc/editor/webrtc.html#stream-api. These
// interfaces must be used only with PeerConnection. PeerConnectionManager
// interface provides the factory methods to create MediaStream and MediaTracks.

#ifndef TALK_APP_WEBRTC_MEDIASTREAMINTERFACE_H_
#define TALK_APP_WEBRTC_MEDIASTREAMINTERFACE_H_

#include <string>
#include <vector>

#include "talk/base/basictypes.h"
#include "talk/base/refcount.h"
#include "talk/base/scoped_ref_ptr.h"

namespace cricket {

class AudioRenderer;
class VideoCapturer;
class VideoRenderer;
class VideoFrame;

}  // namespace cricket

namespace webrtc {

// Generic observer interface.
class ObserverInterface {
 public:
  virtual void OnChanged() = 0;

 protected:
  virtual ~ObserverInterface() {}
};

class NotifierInterface {
 public:
  virtual void RegisterObserver(ObserverInterface* observer) = 0;
  virtual void UnregisterObserver(ObserverInterface* observer) = 0;

  virtual ~NotifierInterface() {}
};

// Base class for sources. A MediaStreamTrack have an underlying source that
// provide media. A source can be shared with multiple tracks.
// TODO(perkj): Implement sources for local and remote audio tracks and
// remote video tracks.
class MediaSourceInterface : public talk_base::RefCountInterface,
                             public NotifierInterface {
 public:
  enum SourceState {
    kInitializing,
    kLive,
    kEnded,
    kMuted
  };

  virtual SourceState state() const = 0;

 protected:
  virtual ~MediaSourceInterface() {}
};

// Information about a track.
class MediaStreamTrackInterface : public talk_base::RefCountInterface,
                                  public NotifierInterface {
 public:
  enum TrackState {
    kInitializing,  // Track is beeing negotiated.
    kLive = 1,  // Track alive
    kEnded = 2,  // Track have ended
    kFailed = 3,  // Track negotiation failed.
  };

  virtual std::string kind() const = 0;
  virtual std::string id() const = 0;
  virtual bool enabled() const = 0;
  virtual TrackState state() const = 0;
  virtual bool set_enabled(bool enable) = 0;
  // These methods should be called by implementation only.
  virtual bool set_state(TrackState new_state) = 0;

 protected:
  virtual ~MediaStreamTrackInterface() {}
};

// Interface for rendering VideoFrames from a VideoTrack
class VideoRendererInterface {
 public:
  virtual void SetSize(int width, int height) = 0;
  virtual void RenderFrame(const cricket::VideoFrame* frame) = 0;

 protected:
  // The destructor is protected to prevent deletion via the interface.
  // This is so that we allow reference counted classes, where the destructor
  // should never be public, to implement the interface.
  virtual ~VideoRendererInterface() {}
};

class VideoSourceInterface;

class VideoTrackInterface : public MediaStreamTrackInterface {
 public:
  // Register a renderer that will render all frames received on this track.
  virtual void AddRenderer(VideoRendererInterface* renderer) = 0;
  // Deregister a renderer.
  virtual void RemoveRenderer(VideoRendererInterface* renderer) = 0;

  virtual VideoSourceInterface* GetSource() const = 0;

 protected:
  virtual ~VideoTrackInterface() {}
};

// AudioSourceInterface is a reference counted source used for AudioTracks.
// The same source can be used in multiple AudioTracks.
// TODO(perkj): Extend this class with necessary methods to allow separate
// sources for each audio track.
class AudioSourceInterface : public MediaSourceInterface {
};

// Interface for receiving audio data from a AudioTrack.
class AudioTrackSinkInterface {
 public:
  virtual void OnData(const void* audio_data,
                      int bits_per_sample,
                      int sample_rate,
                      int number_of_channels,
                      int number_of_frames) = 0;
 protected:
  virtual ~AudioTrackSinkInterface() {}
};

class AudioTrackInterface : public MediaStreamTrackInterface {
 public:
  // TODO(xians): Figure out if the following interface should be const or not.
  virtual AudioSourceInterface* GetSource() const =  0;

  // Adds/Removes a sink that will receive the audio data from the track.
  // TODO(xians): Make them pure virtual after Chrome implements these
  // interfaces.
  virtual void AddSink(AudioTrackSinkInterface* sink) {}
  virtual void RemoveSink(AudioTrackSinkInterface* sink) {}

  // Gets a pointer to the audio renderer of this AudioTrack.
  // The pointer is valid for the lifetime of this AudioTrack.
  // TODO(xians): Remove the following interface after Chrome switches to
  // AddSink() and RemoveSink() interfaces.
  virtual cricket::AudioRenderer* GetRenderer() { return NULL; }

 protected:
  virtual ~AudioTrackInterface() {}
};

typedef std::vector<talk_base::scoped_refptr<AudioTrackInterface> >
    AudioTrackVector;
typedef std::vector<talk_base::scoped_refptr<VideoTrackInterface> >
    VideoTrackVector;

class MediaStreamInterface : public talk_base::RefCountInterface,
                             public NotifierInterface {
 public:
  virtual std::string label() const = 0;

  virtual AudioTrackVector GetAudioTracks() = 0;
  virtual VideoTrackVector GetVideoTracks() = 0;
  virtual talk_base::scoped_refptr<AudioTrackInterface>
      FindAudioTrack(const std::string& track_id) = 0;
  virtual talk_base::scoped_refptr<VideoTrackInterface>
      FindVideoTrack(const std::string& track_id) = 0;

  virtual bool AddTrack(AudioTrackInterface* track) = 0;
  virtual bool AddTrack(VideoTrackInterface* track) = 0;
  virtual bool RemoveTrack(AudioTrackInterface* track) = 0;
  virtual bool RemoveTrack(VideoTrackInterface* track) = 0;

 protected:
  virtual ~MediaStreamInterface() {}
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAMINTERFACE_H_
