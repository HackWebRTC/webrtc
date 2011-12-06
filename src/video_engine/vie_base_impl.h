/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_BASE_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_BASE_IMPL_H_

#include "vie_base.h"
#include "vie_defines.h"
#include "vie_ref_count.h"
#include "vie_shared_data.h"

namespace webrtc {

class Module;
class VoiceEngine;

class ViEBaseImpl
    : public virtual ViESharedData,
      public ViEBase,
      public ViERefCount {
 public:
  virtual int Release();

  // Initializes VideoEngine and must be called before any other API is called.
  virtual int Init();

  // Connects ViE to a VoE instance. Pass in NULL to forget about a previously
  // set voice engine and release all resources we allocated from it.
  virtual int SetVoiceEngine(VoiceEngine* voice_engine);

  // Creates a new ViE channel.
  virtual int CreateChannel(int& video_channel);

  // Creates a new ViE channel that will use the same capture device and encoder
  // as |original_channel|.
  virtual int CreateChannel(int& video_channel, int original_channel);

  // Deletes a ViE channel.
  virtual int DeleteChannel(const int video_channel);

  // Connects a ViE channel with a VoE channel.
  virtual int ConnectAudioChannel(const int video_channel,
                                  const int audio_channel);

  // Disconnects a video/voice channel pair.
  virtual int DisconnectAudioChannel(const int video_channel);

  // Starts sending on video_channel and also starts the encoder.
  virtual int StartSend(const int video_channel);

  // Stops sending on the specified channel.
  virtual int StopSend(const int video_channel);

  // Starts receiving on the channel and also start decoding.
  virtual int StartReceive(const int video_channel);

  // Stops receiving on the specified channel.
  virtual int StopReceive(const int video_channel);

  // Registers a customer implemented observer.
  virtual int RegisterObserver(ViEBaseObserver& observer);

  // Deregisters the observer.
  virtual int DeregisterObserver();

  // Prints version information into |verson|.
  virtual int GetVersion(char version[1024]);

  // Returns the error code for the last registered error.
  virtual int LastError();

 protected:
  ViEBaseImpl();
  virtual ~ViEBaseImpl();

 private:
  // Version functions.
  WebRtc_Word32 AddViEVersion(char* str) const;
  WebRtc_Word32 AddBuildInfo(char* str) const;
#ifdef WEBRTC_EXTERNAL_TRANSPORT
  WebRtc_Word32 AddExternalTransportBuild(char* str) const;
#else
  WebRtc_Word32 AddSocketModuleVersion(char* str) const;
#endif
  WebRtc_Word32 AddModuleVersion(webrtc::Module* module, char* str) const;
  WebRtc_Word32 AddVCMVersion(char* str) const;
  WebRtc_Word32 AddVideoCaptureVersion(char* str) const;
  WebRtc_Word32 AddVideoProcessingVersion(char* str) const;
  WebRtc_Word32 AddRenderVersion(char* str) const;
  WebRtc_Word32 AddRtpRtcpModuleVersion(char* str) const;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_BASE_IMPL_H_
