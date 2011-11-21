/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_sync_module.h"
#include "critical_section_wrapper.h"
#include "voe_video_sync.h"
#include "rtp_rtcp.h"
#include "trace.h"
#include "video_coding.h"

namespace webrtc {

ViESyncModule::ViESyncModule(int id, VideoCodingModule& vcm,
                             RtpRtcp& rtcpModule)
    : _dataCritsect(*CriticalSectionWrapper::CreateCriticalSection()), _id(id),
      _vcm(vcm), _rtcpModule(rtcpModule), _voiceChannelId(-1),
      _voiceSyncInterface(NULL), _lastSyncTime(TickTime::Now())
{
}

ViESyncModule::~ViESyncModule()
{
    delete &_dataCritsect;
}

int ViESyncModule::SetVoiceChannel(int voiceChannelId,
                                   VoEVideoSync* veSyncInterface)
{
    CriticalSectionScoped cs(_dataCritsect);
    _voiceChannelId = voiceChannelId;
    _voiceSyncInterface = veSyncInterface;
    _rtcpModule.DeRegisterSyncModule();

    if (!veSyncInterface)
    {
        _voiceChannelId = -1;
        if (voiceChannelId >= 0) // trying to set a voice channel but  no interface exist
        {
            return -1;
        }
        return 0;
    }
    RtpRtcp* voiceRTPRTCP = NULL;
    veSyncInterface->GetRtpRtcp(_voiceChannelId, voiceRTPRTCP);
    return _rtcpModule.RegisterSyncModule(voiceRTPRTCP);
}

int ViESyncModule::VoiceChannel()
{
    return _voiceChannelId;
}

// ----------------------------------------------------------------------------
// SetNetworkDelay
//
// Set how long time in ms voice is ahead of video when received on the network.
// Positive means audio is ahead of video.
// ----------------------------------------------------------------------------
void ViESyncModule::SetNetworkDelay(int networkDelay)
{
    _channelDelay.networkDelay = networkDelay;
}

// Implements Module
WebRtc_Word32 ViESyncModule::Version(WebRtc_Word8* version,
                    WebRtc_UWord32& remainingBufferInBytes,
                    WebRtc_UWord32& position) const
{
    if (version == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, -1,
                   "Invalid in argument to ViESyncModule Version()");
        return -1;
    }
    WebRtc_Word8 ourVersion[] = "ViESyncModule 1.1.0";
    WebRtc_UWord32 ourLength = (WebRtc_UWord32) strlen(ourVersion);
    if (remainingBufferInBytes < ourLength + 1)
    {
        return -1;
    }
    memcpy(version, ourVersion, ourLength);
    version[ourLength] = '\0'; // null terminaion
    remainingBufferInBytes -= (ourLength + 1);
    position += (ourLength + 1);
    return 0;
}

WebRtc_Word32 ViESyncModule::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
    return 0;
}

WebRtc_Word32 ViESyncModule::TimeUntilNextProcess()
{
    return (WebRtc_Word32) (kSyncInterval - (TickTime::Now()
        - _lastSyncTime).Milliseconds());
}

// Do the lip sync. 
WebRtc_Word32 ViESyncModule::Process()
{
    CriticalSectionScoped cs(_dataCritsect);
    _lastSyncTime = TickTime::Now();

    int totalVideoDelayTargetMS = _vcm.Delay();
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, _id,
               "Video delay (JB + decoder) is %d ms", totalVideoDelayTargetMS);

    if (_voiceChannelId != -1)
    {
        // Get     //Sync start
        int currentAudioDelayMS = 0;
        if (_voiceSyncInterface->GetDelayEstimate(_voiceChannelId,
                                                  currentAudioDelayMS) != 0)
        {
            // Could not get VoE delay value, probably not a valid channel Id.
            WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, _id,
                       "%s: VE_GetDelayEstimate error for voiceChannel %d",
                       __FUNCTION__, totalVideoDelayTargetMS, _voiceChannelId);
            return 0;
        }
        int currentDiffMS = 0;
        int videoDelayMS = 0; // Total video delay
        if (currentAudioDelayMS > 40) // Voice Engine report delay estimates even when not started. Ignore
        {

            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, _id,
                       "Audio delay is: %d for voice channel: %d",
                       currentAudioDelayMS, _voiceChannelId);
            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, _id,
                       "Network delay diff is: %d for voice channel: %d",
                       _channelDelay.networkDelay, _voiceChannelId);
            // Calculate the diff between the lowest possible 
            // video delay and the current audio delay
            currentDiffMS = totalVideoDelayTargetMS - currentAudioDelayMS
                + _channelDelay.networkDelay;
            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, _id,
                       "Current diff is: %d for audio channel: %d",
                       currentDiffMS, _voiceChannelId);

            if (currentDiffMS > 0)
            {
                // The minimum video delay is longer than the current audio delay.
                // We need to decrease extra video delay, if we have added extra delay 
                // earlier, or add extra audio delay.
                if (_channelDelay.extraVideoDelayMS > 0)
                {
                    // We have extra delay added to ViE.
                    // Reduce this delay before adding delay to VE.

                    // This is the desired delay, we can't reduce more than this.
                    videoDelayMS = totalVideoDelayTargetMS;

                    // Check we don't reduce the delay too much
                    if (videoDelayMS < _channelDelay.lastVideoDelayMS
                                       - kMaxVideoDiffMS)
                    {
                        // Too large step...
                        videoDelayMS = _channelDelay.lastVideoDelayMS
                                       - kMaxVideoDiffMS;
                        _channelDelay.extraVideoDelayMS = videoDelayMS
                                                     - totalVideoDelayTargetMS;
                    } else
                    {
                        _channelDelay.extraVideoDelayMS = 0;
                    }
                    _channelDelay.lastVideoDelayMS = videoDelayMS;
                    _channelDelay.lastSyncDelay = -1;
                    _channelDelay.extraAudioDelayMS = 0;
                } else
                {
                    // We have no extra video delay to remove.
                    // Increase the audio delay
                    if (_channelDelay.lastSyncDelay >= 0)
                    {
                        // We have increased the audio delay earlier,
                        // increase it even more.
                        int audioDiffMS = currentDiffMS / 2;
                        if (audioDiffMS > kMaxAudioDiffMS)
                        {
                            // We only allow a maximum change of KMaxAudioDiffMS for audio
                            // due to NetEQ maximum changes.
                            audioDiffMS = kMaxAudioDiffMS;
                        }
                        // Increase the audio delay
                        _channelDelay.extraAudioDelayMS += audioDiffMS;

                        // Don't set a too high delay.
                        if (_channelDelay.extraAudioDelayMS > kMaxDelay)
                        {
                            _channelDelay.extraAudioDelayMS = kMaxDelay;
                        }

                        // Don't add any extra video delay.
                        videoDelayMS = totalVideoDelayTargetMS;
                        _channelDelay.extraVideoDelayMS = 0;
                        _channelDelay.lastVideoDelayMS = videoDelayMS;

                        _channelDelay.lastSyncDelay = 1;
                    } else // lastSyncDelay < 0
                    {
                        // First time after a delay change, don't add any extra delay.
                        // This is to not toggle back and forth too much.
                        _channelDelay.extraAudioDelayMS = 0;
                        // Set minimum video delay
                        videoDelayMS = totalVideoDelayTargetMS;
                        _channelDelay.extraVideoDelayMS = 0;
                        _channelDelay.lastVideoDelayMS = videoDelayMS;
                        _channelDelay.lastSyncDelay = 0;
                    }
                }
            } else // if (currentDiffMS > 0)
            {
                // The minimum video delay is lower than the current audio delay.
                // We need to decrease possible extra audio delay, or
                // add extra video delay.

                if (_channelDelay.extraAudioDelayMS > 0)
                {
                    // We have extra delay in VoiceEngine
                    // Start with decreasing the voice delay
                    int audioDiffMS = currentDiffMS / 2; // This is a negative value
                    if (audioDiffMS < -1 * kMaxAudioDiffMS)
                    {
                        // Don't change the delay too much at once.
                        audioDiffMS = -1 * kMaxAudioDiffMS;
                    }
                    _channelDelay.extraAudioDelayMS += audioDiffMS; // Add the negative change...

                    if (_channelDelay.extraAudioDelayMS < 0)
                    {
                        // Negative values not allowed
                        _channelDelay.extraAudioDelayMS = 0;
                        _channelDelay.lastSyncDelay = 0;
                    } else
                    {
                        // There is more audio delay to use for the next round.
                        _channelDelay.lastSyncDelay = 1;
                    }

                    // Keep the video delay at the minimum values.
                    videoDelayMS = totalVideoDelayTargetMS;
                    _channelDelay.extraVideoDelayMS = 0;
                    _channelDelay.lastVideoDelayMS = videoDelayMS;
                } else
                {
                    // We have no extra delay in VoiceEngine
                    // Increase the video delay
                    _channelDelay.extraAudioDelayMS = 0;

                    // Make the diff positive
                    int videoDiffMS = -1 * currentDiffMS;

                    // This is the desired delay we want
                    videoDelayMS = totalVideoDelayTargetMS + videoDiffMS;
                    if (videoDelayMS > _channelDelay.lastVideoDelayMS)
                    {
                        if (videoDelayMS > _channelDelay.lastVideoDelayMS
                                           + kMaxVideoDiffMS)
                        {
                            // Don't increase the delay too much at once
                            videoDelayMS = _channelDelay.lastVideoDelayMS
                                           + kMaxVideoDiffMS;
                        }
                        // Verify we don't go above the maximum allowed delay
                        if (videoDelayMS > kMaxDelay)
                        {
                            videoDelayMS = kMaxDelay;
                        }
                    } else
                    {
                        if (videoDelayMS < _channelDelay.lastVideoDelayMS
                            - kMaxVideoDiffMS)
                        {
                            // Don't decrease the delay too much at once
                            videoDelayMS = _channelDelay.lastVideoDelayMS
                                - kMaxVideoDiffMS;
                        }
                        // Verify we don't go below the minimum delay
                        if (videoDelayMS < totalVideoDelayTargetMS)
                        {
                            videoDelayMS = totalVideoDelayTargetMS;
                        }
                    }
                    // Store the values
                    _channelDelay.extraVideoDelayMS = videoDelayMS
                        - totalVideoDelayTargetMS;
                    _channelDelay.lastVideoDelayMS = videoDelayMS;
                    _channelDelay.lastSyncDelay = -1;
                }
            }
        }

        WEBRTC_TRACE(
                   webrtc::kTraceInfo,
                   webrtc::kTraceVideo,
                   _id,
                   "Sync video delay %d ms for video channel and audio delay %d for audio channel %d",
                   videoDelayMS, _channelDelay.extraAudioDelayMS,
                   _voiceChannelId);

        // Set the extra audio delay
        if (_voiceSyncInterface->SetMinimumPlayoutDelay(_voiceChannelId,
                                        _channelDelay.extraAudioDelayMS) == -1)
        {
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideo, _id,
                       "Error setting voice delay");
        }

        // sanity 
        // negative not valid
        if (videoDelayMS < 0)
        {
            videoDelayMS = 0;
        }
        totalVideoDelayTargetMS = (totalVideoDelayTargetMS  >  videoDelayMS) ?
                                   totalVideoDelayTargetMS : videoDelayMS;
        _vcm.SetMinimumPlayoutDelay(totalVideoDelayTargetMS);
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, _id,
                   "New Video delay target is: %d", totalVideoDelayTargetMS);
    }
    return 0;
}
} // namespace webrtc
