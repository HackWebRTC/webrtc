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
 * vie_sync_module.h
 * Responsible for doing Audio/Video sync
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_SYNC_MODULE_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_SYNC_MODULE_H_

#include "module.h"
#include "tick_util.h"

namespace webrtc
{
class CriticalSectionWrapper;
class RtpRtcp;
class VideoCodingModule;
class VoEVideoSync;


class ViESyncModule : public Module
{
public:
    enum { kSyncInterval = 1000};
    enum { kMaxVideoDiffMS = 80 }; // Video sync
    enum { kMaxAudioDiffMS = 80 }; // Video sync
    enum { kMaxDelay = 1500 };     // Video sync

    ViESyncModule(int id, VideoCodingModule& vcm,
                      RtpRtcp& rtcpModule);
    ~ViESyncModule();
    int SetVoiceChannel(int voiceChannelId, VoEVideoSync* voiceSyncInterface);
    int VoiceChannel();
    void SetNetworkDelay(int networkDelay);

    // Implements Module
    virtual WebRtc_Word32 Version(WebRtc_Word8* version,
                        WebRtc_UWord32& remainingBufferInBytes,
                        WebRtc_UWord32& position) const;

    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);
    virtual WebRtc_Word32 TimeUntilNextProcess();
    virtual WebRtc_Word32 Process();

private:
    // Critical sections
    CriticalSectionWrapper& _dataCritsect;
    int _id;
    VideoCodingModule& _vcm;
    RtpRtcp& _rtcpModule;
    int _voiceChannelId;
    VoEVideoSync* _voiceSyncInterface;
    TickTime _lastSyncTime;

    struct ViESyncDelay
    {
        ViESyncDelay()
        {
            extraVideoDelayMS = 0;
            lastVideoDelayMS = 0;
            extraAudioDelayMS = 0;
            lastSyncDelay = 0;
            networkDelay = 120;
        }
        int extraVideoDelayMS;
        int lastVideoDelayMS;
        int extraAudioDelayMS; //audioDelayMS;
        int lastSyncDelay;
        int networkDelay;
    };
    ViESyncDelay _channelDelay;

};
} // namespace webrtc
#endif // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_SYNC_MODULE_H_
