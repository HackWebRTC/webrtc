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

#ifndef TALK_APP_WEBRTC_STREAM_H_
#define TALK_APP_WEBRTC_STREAM_H_

#include <string>

#include "talk/app/webrtc_dev/ref_count.h"
#include "talk/app/webrtc_dev/scoped_refptr.h"

namespace cricket {
class VideoRenderer;
class MediaEngine;
}  // namespace cricket

namespace webrtc {

class AudioDeviceModule;
class VideoCaptureModule;

const char kVideoTrackKind[] = "video";
const char kAudioTrackKind[] = "audio";

// Generic observer interface.
class Observer {
 public:
  virtual void OnChanged() = 0;

 protected:
  virtual ~Observer() {}
};

class Notifier {
 public:
  virtual void RegisterObserver(Observer* observer) = 0;
  virtual void UnregisterObserver(Observer* observer) = 0;
};

// Information about a track.
class MediaStreamTrack : public RefCount,
                         public Notifier {
 public:
  virtual const std::string& kind() = 0;
  virtual const std::string& label() = 0;
  virtual bool enabled() = 0;
  // Enable or disables a track.
  // For Remote streams - disable means that the video is not decoded,
  // or audio not decoded.
  // For local streams this means that video is not captured
  // or audio is not captured.
  virtual bool set_enabled(bool enable) = 0;
};

// Reference counted wrapper for an AudioDeviceModule.
class AudioDevice : public RefCount {
 public:
  static scoped_refptr<AudioDevice> Create(const std::string& name,
                                           AudioDeviceModule* adm);

  // Name of this device. Same as label of a MediaStreamTrack.
  const std::string& name();

  AudioDeviceModule* module();

 protected:
  AudioDevice() {}
  virtual ~AudioDevice() {}
  void Initialize(const std::string& name, AudioDeviceModule* adm);

  std::string name_;
  AudioDeviceModule* adm_;
};

// Reference counted wrapper for a VideoCaptureModule.
class VideoDevice : public RefCount {
 public:
  static scoped_refptr<VideoDevice> Create(const std::string& name,
                                           VideoCaptureModule* vcm);
  // Name of this device. Same as label of a MediaStreamTrack.
  const std::string& name();

  VideoCaptureModule* module();

 protected:
  VideoDevice() {}
  ~VideoDevice() {}
  void Initialize(const std::string& name, VideoCaptureModule* vcm);

  std::string name_;
  VideoCaptureModule* vcm_;
};

// Reference counted wrapper for a VideoRenderer.
class VideoRenderer : public RefCount {
 public:
  static scoped_refptr<VideoRenderer> Create(cricket::VideoRenderer* renderer);
  virtual cricket::VideoRenderer* module();

 protected:
  VideoRenderer() {}
  ~VideoRenderer() {}
  void Initialize(cricket::VideoRenderer* renderer);

  cricket::VideoRenderer* renderer_;
};

class VideoTrack : public MediaStreamTrack {
 public:
  // Set the video renderer for a local or remote stream.
  // This call will start decoding the received video stream and render it.
  virtual void SetRenderer(VideoRenderer* renderer) = 0;

  // Get the VideoRenderer associated with this track.
  virtual scoped_refptr<VideoRenderer> GetRenderer() = 0;

 protected:
  virtual ~VideoTrack() {}
};

class LocalVideoTrack : public VideoTrack {
 public:
  static scoped_refptr<LocalVideoTrack> Create(VideoDevice* video_device);

  // Get the VideoCapture device associated with this track.
  virtual scoped_refptr<VideoDevice> GetVideoCapture() = 0;

 protected:
  virtual ~LocalVideoTrack() {}
};

class AudioTrack : public MediaStreamTrack {
 public:
 protected:
  virtual ~AudioTrack() {}
};

class LocalAudioTrack : public AudioTrack {
 public:
  static scoped_refptr<LocalAudioTrack> Create(AudioDevice* audio_device);

  // Get the AudioDevice associated with this track.
  virtual scoped_refptr<AudioDevice> GetAudioDevice() =  0;
 protected:
  virtual ~LocalAudioTrack() {}
};

// List of of tracks.
class MediaStreamTrackList : public RefCount, public Notifier {
 public:
  virtual size_t count() = 0;
  virtual scoped_refptr<MediaStreamTrack> at(size_t index) = 0;

 protected:
  virtual ~MediaStreamTrackList() {}
};

class MediaStream : public RefCount {
 public:
  virtual const std::string& label() = 0;
  virtual scoped_refptr<MediaStreamTrackList> tracks() = 0;

  enum ReadyState {
    kInitializing,
    kLive = 1,  // Stream alive
    kEnded = 2,  // Stream have ended
  };

  virtual ReadyState ready_state() = 0;

 protected:
  virtual ~MediaStream() {}
};

class LocalMediaStream : public MediaStream {
 public:
  static scoped_refptr<LocalMediaStream> Create(const std::string& label);
  virtual bool AddTrack(MediaStreamTrack* track) = 0;
};

// Remote streams are created by the PeerConnection object and provided to the
// client using PeerConnectionObserver::OnAddStream.
// The client can provide the renderer to the PeerConnection object calling
// VideoTrack::SetRenderer.
class RemoteMediaStream : public MediaStream {
 public:
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_STREAM_H_
