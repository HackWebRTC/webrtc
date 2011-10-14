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

#ifndef TALK_APP_WEBRTC_MEDIASTREAM_H_
#define TALK_APP_WEBRTC_MEDIASTREAM_H_

#include <string>

#include "talk/app/webrtc_dev/ref_count.h"
#include "talk/app/webrtc_dev/scoped_refptr.h"
#include "talk/base/basictypes.h"

namespace cricket {
class VideoRenderer;
class MediaEngine;
}  // namespace cricket

namespace webrtc {

class AudioDeviceModule;
class VideoCaptureModule;

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
class MediaStreamTrackInterface : public talk_base::RefCount,
                                  public Notifier {
 public:
  enum TrackState {
    kInitializing,  // Track is beeing negotiated.
    kLive = 1,  // Track alive
    kEnded = 2,  // Track have ended
    kFailed = 3,  // Track negotiation failed.
  };

  enum TrackType {
    kAudio = 0, 
    kVideo = 1,
  };

  virtual const char* kind() const = 0;
  virtual const std::string& label()  const = 0;
  virtual TrackType type() const = 0;
  virtual uint32 ssrc() const = 0;
  virtual bool enabled() const = 0;
  virtual TrackState state() const = 0;
  virtual bool set_enabled(bool enable) = 0;
  // Return false (or assert) if the ssrc is already set.
  virtual bool set_ssrc(uint32 ssrc) = 0;
  virtual bool set_state(TrackState new_state) = 0;
};

// Reference counted wrapper for a VideoRenderer.
class VideoRendererInterface : public talk_base::RefCount {
 public:
  virtual cricket::VideoRenderer* renderer() = 0;

 protected:
  virtual ~VideoRendererInterface() {}
};

// Creates a reference counted object of type webrtc::VideoRenderer.
// webrtc::VideoRenderer take ownership of cricket::VideoRenderer.
scoped_refptr<VideoRendererInterface> CreateVideoRenderer(
    cricket::VideoRenderer* renderer);

class VideoTrackInterface : public MediaStreamTrackInterface {
 public:
  // Set the video renderer for a local or remote stream.
  // This call will start decoding the received video stream and render it.
  virtual void SetRenderer(VideoRendererInterface* renderer) = 0;

  // Get the VideoRenderer associated with this track.
  virtual VideoRendererInterface* GetRenderer() = 0;

 protected:
  virtual ~VideoTrackInterface() {}
};

class LocalVideoTrackInterface : public VideoTrackInterface {
 public:
  // Get the VideoCapture device associated with this track.
  virtual VideoCaptureModule* GetVideoCapture() = 0;

 protected:
  virtual ~LocalVideoTrackInterface() {}
};

scoped_refptr<LocalVideoTrackInterface> CreateLocalVideoTrack(
    const std::string& label,
    VideoCaptureModule* video_device);

class AudioTrackInterface : public MediaStreamTrackInterface {
 public:
 protected:
  virtual ~AudioTrackInterface() {}
};

class LocalAudioTrackInterface : public AudioTrackInterface {
 public:
  // Get the AudioDeviceModule associated with this track.
  virtual AudioDeviceModule* GetAudioDevice() =  0;
 protected:
  virtual ~LocalAudioTrackInterface() {}
};

scoped_refptr<LocalAudioTrackInterface> CreateLocalAudioTrack(
    const std::string& label,
    AudioDeviceModule* audio_device);

// List of of tracks.
class MediaStreamTrackListInterface : public talk_base::RefCount,
                                      public Notifier {
 public:
  virtual size_t count() = 0;
  virtual MediaStreamTrackInterface* at(size_t index) = 0;

 protected:
  virtual ~MediaStreamTrackListInterface() {}
};

class MediaStreamInterface : public talk_base::RefCount,
                             public Notifier {
 public:
  virtual const std::string& label() = 0;
  virtual MediaStreamTrackListInterface* tracks() = 0;

  enum ReadyState {
    kInitializing,
    kLive = 1,  // Stream alive
    kEnded = 2,  // Stream have ended
  };

  virtual ReadyState ready_state() = 0;

  // Only to be used by the implementation.
  virtual void set_ready_state(ReadyState state) = 0;

 protected:
  virtual ~MediaStreamInterface() {}
};

class LocalMediaStreamInterface : public MediaStreamInterface {
 public:
  virtual bool AddTrack(MediaStreamTrackInterface* track) = 0;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAM_H_
