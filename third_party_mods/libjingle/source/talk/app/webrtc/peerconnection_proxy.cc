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

#include "talk/app/webrtc/peerconnection_proxy.h"

#include "talk/app/webrtc/peerconnection_impl.h"
#include "talk/base/logging.h"

namespace webrtc {

enum {
  MSG_WEBRTC_ADDSTREAM = 1,
  MSG_WEBRTC_CLOSE,
  MSG_WEBRTC_CONNECT,
  MSG_WEBRTC_INIT,
  MSG_WEBRTC_REGISTEROBSERVER,
  MSG_WEBRTC_RELEASE,
  MSG_WEBRTC_REMOVESTREAM,
  MSG_WEBRTC_SETAUDIODEVICE,
  MSG_WEBRTC_SETLOCALRENDERER,
  MSG_WEBRTC_SETVIDEOCAPTURE,
  MSG_WEBRTC_SETVIDEORENDERER,
  MSG_WEBRTC_SIGNALINGMESSAGE,
};

struct AddStreamParams : public talk_base::MessageData {
  AddStreamParams(const std::string& stream_id, bool video)
      : stream_id(stream_id),
        video(video),
        result(false) {}

  std::string stream_id;
  bool video;
  bool result;
};

struct RemoveStreamParams : public talk_base::MessageData {
  explicit RemoveStreamParams(const std::string& stream_id)
      : stream_id(stream_id),
        result(false) {}

  std::string stream_id;
  bool result;
};

struct SignalingMsgParams : public talk_base::MessageData {
  explicit SignalingMsgParams(const std::string& signaling_message)
      : signaling_message(signaling_message),
        result(false) {}

  std::string signaling_message;
  bool result;
};

struct SetAudioDeviceParams : public talk_base::MessageData {
  SetAudioDeviceParams(const std::string& wave_in_device,
                       const std::string& wave_out_device,
                       int opts)
      : wave_in_device(wave_in_device), wave_out_device(wave_out_device),
        opts(opts), result(false) {}

  std::string wave_in_device;
  std::string wave_out_device;
  int opts;
  bool result;
};

struct SetLocalRendererParams : public talk_base::MessageData {
  explicit SetLocalRendererParams(cricket::VideoRenderer* renderer)
      : renderer(renderer), result(false) {}

  cricket::VideoRenderer* renderer;
  bool result;
};

struct SetVideoRendererParams : public talk_base::MessageData {
  SetVideoRendererParams(const std::string& stream_id,
                         cricket::VideoRenderer* renderer)
      : stream_id(stream_id), renderer(renderer), result(false) {}

  std::string stream_id;
  cricket::VideoRenderer* renderer;
  bool result;
};

struct SetVideoCaptureParams : public talk_base::MessageData {
  explicit SetVideoCaptureParams(const std::string& cam_device)
      : cam_device(cam_device), result(false) {}

  std::string cam_device;
  bool result;
};

struct RegisterObserverParams : public talk_base::MessageData {
  explicit RegisterObserverParams(PeerConnectionObserver* observer)
      : observer(observer), result(false) {}

  PeerConnectionObserver* observer;
  bool result;
};

struct ResultParams : public talk_base::MessageData {
  ResultParams()
      : result(false) {}

  bool result;
};

PeerConnectionProxy::PeerConnectionProxy(const std::string& config,
      cricket::PortAllocator* port_allocator,
      cricket::MediaEngine* media_engine,
      talk_base::Thread* worker_thread,
      talk_base::Thread* signaling_thread,
      cricket::DeviceManager* device_manager)
  : peerconnection_impl_(new PeerConnectionImpl(config, port_allocator,
                             media_engine, worker_thread, signaling_thread,
                             device_manager)),
    signaling_thread_(signaling_thread) {
}

PeerConnectionProxy::PeerConnectionProxy(const std::string& config,
                                       cricket::PortAllocator* port_allocator,
                                       talk_base::Thread* worker_thread)
  : peerconnection_impl_(new PeerConnectionImpl(config, port_allocator,
                             worker_thread)),
    signaling_thread_(NULL) {
}

PeerConnectionProxy::~PeerConnectionProxy() {
  ResultParams params;
  Send(MSG_WEBRTC_RELEASE, &params);
}

bool PeerConnectionProxy::Init() {
  // TODO(mallinath) - Changes are required to modify the stand alone
  // constructor to get signaling thread as input. It should not be created
  // here.
  if (!signaling_thread_) {
    signaling_thread_ = new talk_base::Thread();
    if (!signaling_thread_->SetName("signaling thread", this) ||
        !signaling_thread_->Start()) {
      LOG(WARNING) << "Failed to start libjingle signaling thread";
      return false;
    }
  }

  ResultParams params;
  return (Send(MSG_WEBRTC_INIT, &params) && params.result);
}

void PeerConnectionProxy::RegisterObserver(PeerConnectionObserver* observer) {
  RegisterObserverParams params(observer);
  Send(MSG_WEBRTC_REGISTEROBSERVER, &params);
}

bool PeerConnectionProxy::SignalingMessage(
    const std::string& signaling_message) {
  SignalingMsgParams params(signaling_message);
  return (Send(MSG_WEBRTC_SIGNALINGMESSAGE, &params) && params.result);
}

bool PeerConnectionProxy::AddStream(const std::string& stream_id, bool video) {
  AddStreamParams params(stream_id, video);
  return (Send(MSG_WEBRTC_ADDSTREAM, &params) && params.result);
}

bool PeerConnectionProxy::RemoveStream(const std::string& stream_id) {
  RemoveStreamParams params(stream_id);
  return (Send(MSG_WEBRTC_REMOVESTREAM, &params) && params.result);
}

bool PeerConnectionProxy::SetAudioDevice(const std::string& wave_in_device,
                                        const std::string& wave_out_device,
                                        int opts) {
  SetAudioDeviceParams params(wave_in_device, wave_out_device, opts);
  return (Send(MSG_WEBRTC_SETAUDIODEVICE, &params) && params.result);
}

bool PeerConnectionProxy::SetLocalVideoRenderer(
    cricket::VideoRenderer* renderer) {
  SetLocalRendererParams params(renderer);
  return (Send(MSG_WEBRTC_SETLOCALRENDERER, &params) && params.result);
}

bool PeerConnectionProxy::SetVideoRenderer(const std::string& stream_id,
                                          cricket::VideoRenderer* renderer) {
  SetVideoRendererParams params(stream_id, renderer);
  return (Send(MSG_WEBRTC_SETVIDEORENDERER, &params) && params.result);
}

bool PeerConnectionProxy::SetVideoCapture(const std::string& cam_device) {
  SetVideoCaptureParams params(cam_device);
  return (Send(MSG_WEBRTC_SETVIDEOCAPTURE, &params) && params.result);
}

bool PeerConnectionProxy::Connect() {
  ResultParams params;
  return (Send(MSG_WEBRTC_CONNECT, &params) && params.result);
}

bool PeerConnectionProxy::Close() {
  ResultParams params;
  return (Send(MSG_WEBRTC_CLOSE, &params) && params.result);
}

bool PeerConnectionProxy::Send(uint32 id, talk_base::MessageData* data) {
  if (!signaling_thread_)
    return false;
  signaling_thread_->Send(this, id, data);
  return true;
}

void PeerConnectionProxy::OnMessage(talk_base::Message* message) {
  talk_base::MessageData* data = message->pdata;
  switch (message->message_id) {
    case MSG_WEBRTC_ADDSTREAM: {
      AddStreamParams* params = reinterpret_cast<AddStreamParams*>(data);
      params->result = peerconnection_impl_->AddStream(
          params->stream_id, params->video);
      break;
    }
    case MSG_WEBRTC_SIGNALINGMESSAGE: {
      SignalingMsgParams* params =
          reinterpret_cast<SignalingMsgParams*>(data);
      params->result = peerconnection_impl_->SignalingMessage(
          params->signaling_message);
      break;
    }
    case MSG_WEBRTC_REMOVESTREAM: {
      RemoveStreamParams* params = reinterpret_cast<RemoveStreamParams*>(data);
      params->result = peerconnection_impl_->RemoveStream(
          params->stream_id);
      break;
    }
    case MSG_WEBRTC_SETAUDIODEVICE: {
      SetAudioDeviceParams* params =
          reinterpret_cast<SetAudioDeviceParams*>(data);
      params->result = peerconnection_impl_->SetAudioDevice(
          params->wave_in_device, params->wave_out_device, params->opts);
      break;
    }
    case MSG_WEBRTC_SETLOCALRENDERER: {
      SetLocalRendererParams* params =
          reinterpret_cast<SetLocalRendererParams*>(data);
      params->result = peerconnection_impl_->SetLocalVideoRenderer(
          params->renderer);
      break;
    }
    case MSG_WEBRTC_SETVIDEOCAPTURE: {
      SetVideoCaptureParams* params =
          reinterpret_cast<SetVideoCaptureParams*>(data);
      params->result = peerconnection_impl_->SetVideoCapture(
          params->cam_device);
      break;
    }
    case MSG_WEBRTC_SETVIDEORENDERER: {
      SetVideoRendererParams* params =
          reinterpret_cast<SetVideoRendererParams*>(data);
      params->result = peerconnection_impl_->SetVideoRenderer(
          params->stream_id, params->renderer);
      break;
    }
    case MSG_WEBRTC_CONNECT: {
      ResultParams* params =
          reinterpret_cast<ResultParams*>(data);
      params->result = peerconnection_impl_->Connect();
      break;
    }
    case MSG_WEBRTC_CLOSE: {
      ResultParams* params =
          reinterpret_cast<ResultParams*>(data);
      params->result = peerconnection_impl_->Close();
      break;
    }
    case MSG_WEBRTC_INIT: {
      ResultParams* params =
          reinterpret_cast<ResultParams*>(data);
      params->result = peerconnection_impl_->Init();
      break;
    }
    case MSG_WEBRTC_REGISTEROBSERVER: {
      RegisterObserverParams* params =
          reinterpret_cast<RegisterObserverParams*>(data);
      peerconnection_impl_->RegisterObserver(params->observer);
      break;
    }
    case MSG_WEBRTC_RELEASE: {
      peerconnection_impl_.reset();
      break;
    }
    default: {
      ASSERT(false);
      break;
    }
  }
}

}  // namespace webrtc
