/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * vie_base_impl.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_BASE_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_BASE_IMPL_H_

#include "vie_defines.h"

#include "vie_ref_count.h"
#include "vie_shared_data.h"
#include "vie_base.h"

// Forward declarations
namespace webrtc
{

class VoiceEngine;
class Module;

class ViEBaseImpl: public virtual ViESharedData,
    public ViEBase,
    public ViERefCount
{
public:
    virtual int Release();

    virtual int Init();

    virtual int SetVoiceEngine(VoiceEngine* ptrVoiceEngine);

    // Channel functions
    virtual int CreateChannel(int& videoChannel);

    virtual int CreateChannel(int& videoChannel, int originalChannel);

    virtual int DeleteChannel(const int videoChannel);

    virtual int ConnectAudioChannel(const int videoChannel,
                                    const int audioChannel);

    virtual int DisconnectAudioChannel(const int videoChannel);

    // Start and stop
    virtual int StartSend(const int videoChannel);

    virtual int StopSend(const int videoChannel);

    virtual int StartReceive(const int videoChannel);

    virtual int StopReceive(const int videoChannel);

    // Callbacks
    virtual int RegisterObserver(ViEBaseObserver& observer);

    virtual int DeregisterObserver();

    // Info functions
    virtual int GetVersion(char version[1024]);

    virtual int LastError();

protected:
    ViEBaseImpl();
    virtual ~ViEBaseImpl();
private:

    // Version functions
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
#ifdef WEBRTC_SRTP
    WebRtc_Word32 AddSRTPModuleVersion(char* str) const;
#endif
    WebRtc_Word32 AddRtpRtcpModuleVersion(char* str) const;
};

} // namespace webrtc

#endif  // #define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_BASE_IMPL_H_
