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

#include <stdio.h>

#include <list>

#include "gtest/gtest.h"
#include "modules/video_capture/main/interface/video_capture_factory.h"
#include "talk/app/webrtc_dev/mediastream.h"
#include "talk/app/webrtc_dev/peerconnection.h"
#include "talk/base/thread.h"
#include "talk/session/phone/videoframe.h"
#include "talk/session/phone/videorenderer.h"

void GetAllVideoTracks(webrtc::MediaStreamInterface* media_stream,
                       std::list<webrtc::VideoTrackInterface*>* video_tracks) {
  webrtc::VideoTracks* track_list = media_stream->video_tracks();
  for (size_t i = 0; i < track_list->count(); ++i) {
    webrtc::VideoTrackInterface* track = track_list->at(i);
    video_tracks->push_back(
          static_cast<webrtc::VideoTrackInterface*>(track));
  }
}

// TODO(henrike): replace with a capture device that reads from a file/buffer.
scoped_refptr<webrtc::VideoCaptureModule> OpenVideoCaptureDevice() {
  webrtc::VideoCaptureModule::DeviceInfo* device_info(
      webrtc::VideoCaptureFactory::CreateDeviceInfo(0));
  scoped_refptr<webrtc::VideoCaptureModule> video_device;

  const size_t kMaxDeviceNameLength = 128;
  const size_t kMaxUniqueIdLength = 256;
  uint8 device_name[kMaxDeviceNameLength];
  uint8 unique_id[kMaxUniqueIdLength];

  const size_t device_count = device_info->NumberOfDevices();
  for (size_t i = 0; i < device_count; ++i) {
    // Get the name of the video capture device.
    device_info->GetDeviceName(i, device_name, kMaxDeviceNameLength, unique_id,
        kMaxUniqueIdLength);
    // Try to open this device.
    video_device =
        webrtc::VideoCaptureFactory::Create(0, unique_id);
    if (video_device.get())
      break;
  }
  delete device_info;
  return video_device;
}

class VideoRecorder : public cricket::VideoRenderer {
 public:
  static VideoRecorder* CreateVideoRecorder(
      const char* file_name) {
    VideoRecorder* renderer = new VideoRecorder();
    if (!renderer->Init(file_name)) {
      delete renderer;
      return NULL;
    }
    return renderer;
  }
  virtual ~VideoRecorder() {
    if (output_file_ != NULL) {
      fclose(output_file_);
    }
  }

  // Set up files so that recording can start immediately.
  bool Init(const char* file_name) {
    output_file_ = fopen(file_name, "wb");
    if (output_file_ == NULL) {
      return false;
    }
    return true;
  }

  virtual bool SetSize(int width, int height, int /*reserved*/) {
    width_ = width;
    height_ = height;
    image_.reset(new uint8[buffersize()]);
  }

  // |frame| is in I420
  virtual bool RenderFrame(const cricket::VideoFrame* frame) {
    const int actual_size = frame->CopyToBuffer(image_.get(),
                                                buffersize());
    if (actual_size > buffersize()) {
      ASSERT(false);
      // Skip frame.
      return true;
    }
    // Write to file.
    fwrite(image_.get(), sizeof(uint8), actual_size, output_file_);
    return true;
  }

  const uint8* image() const {
    return image_.get();
  }

  int buffersize() const {
    // I420 buffer size
    return (width_ * height_ * 3) >> 1;
  }

  int width() const {
    return width_;
  }

  int height() const {
    return height_;
  }

 protected:
  VideoRecorder()
      : width_(0),
        height_(0),
        output_file_(NULL) {}

  talk_base::scoped_array<uint8> image_;
  int width_;
  int height_;

  // File to record to.
  FILE* output_file_;
};

class SignalingMessageReceiver {
 public:
  virtual void ReceiveMessage(const std::string& msg) = 0;

 protected:
  SignalingMessageReceiver() {}
  virtual ~SignalingMessageReceiver() {}
};

class PeerConnectionP2PTestClient
    : public webrtc::PeerConnectionObserver,
      public SignalingMessageReceiver {
 public:
  static PeerConnectionP2PTestClient* CreateClient(int id) {
    PeerConnectionP2PTestClient* client = new PeerConnectionP2PTestClient(id);
    if (!client->Init()) {
      delete client;
      return NULL;
    }
    return client;
  }

  ~PeerConnectionP2PTestClient() {
    // Ensure that webrtc::PeerConnection is deleted before
    // webrtc::PeerConnectionManager or crash will occur
    webrtc::PeerConnection* temp = peer_connection_.release();
    temp->Release();
  }

  void StartSession() {
    // Audio track doesn't seem to be implemented yet. No need to pass a device
    // to it.
    scoped_refptr<webrtc::LocalAudioTrackInterface> audio_track(
        peer_connection_factory_->CreateLocalAudioTrack("audio_track", NULL));

    scoped_refptr<webrtc::LocalVideoTrackInterface> video_track(
        peer_connection_factory_->CreateLocalVideoTrack(
            "video_track",
            OpenVideoCaptureDevice()));

    scoped_refptr<webrtc::LocalMediaStreamInterface> stream =
        peer_connection_factory_->CreateLocalMediaStream("stream_label");

    stream->AddTrack(audio_track);
    stream->AddTrack(video_track);

    peer_connection_->AddStream(stream);
    peer_connection_->CommitStreamChanges();
  }

  void set_signaling_message_receiver(
      SignalingMessageReceiver* signaling_message_receiver) {
    signaling_message_receiver_ = signaling_message_receiver;
  }

  // SignalingMessageReceiver callback.
  virtual void ReceiveMessage(const std::string& msg) {
    peer_connection_->ProcessSignalingMessage(msg);
  }

  // PeerConnectionObserver callbacks.
  virtual void OnError() {}
  virtual void OnMessage(const std::string&) {}
  virtual void OnSignalingMessage(const std::string& msg)  {
    if (signaling_message_receiver_ == NULL) {
      ADD_FAILURE();
      return;
    }
    signaling_message_receiver_->ReceiveMessage(msg);
  }
  virtual void OnStateChange(Readiness) {}
  virtual void OnAddStream(webrtc::MediaStreamInterface* media_stream) {
    std::list<webrtc::VideoTrackInterface*> video_tracks;
    GetAllVideoTracks(media_stream, &video_tracks);
    int track_id = 0;
    for (std::list<webrtc::VideoTrackInterface*>::iterator iter =
             video_tracks.begin();
         iter != video_tracks.end();
         ++iter) {
      char file_name[256];
      GenerateRecordingFileName(track_id, file_name);
      scoped_refptr<webrtc::VideoRendererWrapperInterface> video_renderer =
          webrtc::CreateVideoRenderer(
              VideoRecorder::CreateVideoRecorder(file_name));
      if (video_renderer == NULL) {
        ADD_FAILURE();
        continue;
      }
      (*iter)->SetRenderer(video_renderer);
      track_id++;
    }
  }
  virtual void OnRemoveStream(webrtc::MediaStreamInterface*) {
  }

 private:
  explicit PeerConnectionP2PTestClient(int id)
      : id_(id),
        peer_connection_(),
        peer_connection_factory_(),
        signaling_message_receiver_(NULL) {
  }

  bool Init() {
    EXPECT_TRUE(peer_connection_.get() == NULL);
    EXPECT_TRUE(peer_connection_factory_.get() == NULL);
    peer_connection_factory_ = webrtc::PeerConnectionManager::Create();
    if (peer_connection_factory_.get() == NULL) {
      ADD_FAILURE();
      return false;
    }

    const char server_configuration[] = "STUN stun.l.google.com:19302";
    peer_connection_ = peer_connection_factory_->CreatePeerConnection(
        server_configuration, this);
    return peer_connection_.get() != NULL;
  }

  void GenerateRecordingFileName(int track, char file_name[256]) {
    if (file_name == NULL) {
      return;
    }
    snprintf(file_name, sizeof(file_name),
             "p2p_test_client_%d_videotrack_%d.yuv", id_, track);
  }

  int id_;
  scoped_refptr<webrtc::PeerConnection> peer_connection_;
  scoped_refptr<webrtc::PeerConnectionManager> peer_connection_factory_;

  // Remote peer communication.
  SignalingMessageReceiver* signaling_message_receiver_;
};

class P2PTestConductor {
 public:
  static P2PTestConductor* CreateConductor() {
    P2PTestConductor* conductor = new P2PTestConductor();
    if (!conductor->Init()) {
      delete conductor;
      return NULL;
    }
    return conductor;
  }
  ~P2PTestConductor() {
    for (int i = 0; i < kClients; ++i) {
      if (clients[i] != NULL) {
        // TODO(hellner): currently deleting the clients will trigger an assert
        // in cricket::BaseChannel::DisableMedia_w (not due to the unit test).
        // Fix that problem and remove the below comment.
        delete clients[i];
      }
    }
  }

  void StartSession() {
    PeerConnectionP2PTestClient* initiating_client = clients[0];
    initiating_client->StartSession();
  }

 private:
  static const int kClients = 2;
  P2PTestConductor() {
    clients[0] = NULL;
    clients[1] = NULL;
  }

  bool Init() {
    for (int i = 0; i < kClients; ++i) {
      clients[i] = PeerConnectionP2PTestClient::CreateClient(i);
      if (clients[i] == NULL) {
        return false;
      }
    }
    clients[0]->set_signaling_message_receiver(clients[1]);
    clients[1]->set_signaling_message_receiver(clients[0]);
    return true;
  }

  PeerConnectionP2PTestClient* clients[kClients];
};

TEST(PeerConnection2, LocalP2PTest) {
  P2PTestConductor* test = P2PTestConductor::CreateConductor();
  ASSERT_TRUE(test != NULL);
  test->StartSession();
  talk_base::Thread::Current()->ProcessMessages(10000);
  delete test;
}
