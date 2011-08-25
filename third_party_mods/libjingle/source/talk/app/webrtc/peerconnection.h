/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#ifndef TALK_APP_WEBRTC_PEERCONNECTION_H_
#define TALK_APP_WEBRTC_PEERCONNECTION_H_

// TODO(mallinath) - Add a factory class or some kind of PeerConnection manager
// to support multiple PeerConnection object instantiation. This class will
// create ChannelManager object and pass it to PeerConnection object. Otherwise
// each PeerConnection object will have its own ChannelManager hence MediaEngine
// and VoiceEngine/VideoEngine.

#include <string>

namespace cricket {
class VideoRenderer;
}

namespace talk_base {
class Thread;
}

namespace webrtc {

class PeerConnectionObserver {
 public:
  virtual void OnInitialized() = 0;
  virtual void OnError() = 0;

  // serialized signaling message
  virtual void OnSignalingMessage(const std::string& msg) = 0;

  // Triggered when a local stream has been added and initialized
  virtual void OnLocalStreamInitialized(const std::string& stream_id,
      bool video) = 0;

  // Triggered when a remote peer accepts a media connection.
  virtual void OnAddStream(const std::string& stream_id, bool video) = 0;

  // Triggered when a remote peer closes a media stream.
  virtual void OnRemoveStream(const std::string& stream_id, bool video) = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionObserver() {}
};

class PeerConnection {
 public:
  enum ReadyState {
    NEW = 0,
    NEGOTIATING,
    ACTIVE,
    CLOSED,
  };

  virtual ~PeerConnection() {}

  // Register a listener
  virtual void RegisterObserver(PeerConnectionObserver* observer) = 0;

  // SignalingMessage in json format
  virtual bool SignalingMessage(const std::string& msg) = 0;

  // Asynchronously adds a local stream device to the peer
  // connection. The operation is complete when
  // PeerConnectionObserver::OnLocalStreamInitialized is called.
  virtual bool AddStream(const std::string& stream_id, bool video) = 0;

  // Asynchronously removes a local stream device from the peer
  // connection. The operation is complete when
  // PeerConnectionObserver::OnRemoveStream is called.
  virtual bool RemoveStream(const std::string& stream_id) = 0;

  // Info the peerconnection that it is time to return the signaling
  // information. The operation is complete when
  // PeerConnectionObserver::OnSignalingMessage is called.
  virtual bool Connect() = 0;

  // Remove all the streams and tear down the session.
  // After the Close() is called, the OnSignalingMessage will be invoked
  // asynchronously. And before OnSignalingMessage is called,
  // OnRemoveStream will be called for each stream that was active.
  // TODO(ronghuawu): Add an event such as onclose, or onreadystatechanged
  // when the readystate reaches the closed state (no more streams in the
  // peerconnection object.
  virtual bool Close() = 0;

  // Set the audio input & output devices based on the given device name.
  // An empty device name means to use the default audio device.
  virtual bool SetAudioDevice(const std::string& wave_in_device,
                              const std::string& wave_out_device,
                              int opts) = 0;

  // Set the video renderer for the camera preview.
  virtual bool SetLocalVideoRenderer(cricket::VideoRenderer* renderer) = 0;

  // Set the video renderer for the specified stream.
  virtual bool SetVideoRenderer(const std::string& stream_id,
                                cricket::VideoRenderer* renderer) = 0;

  // Set video capture device
  // For Chromium the cam_device should use the capture session id.
  // For standalone app, cam_device is the camera name. It will try to
  // set the default capture device when cam_device is "".
  virtual bool SetVideoCapture(const std::string& cam_device) = 0;

  // Returns the state of the PeerConnection object.  See the ReadyState
  // enum for valid values.
  virtual ReadyState GetReadyState() = 0;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTION_H_
