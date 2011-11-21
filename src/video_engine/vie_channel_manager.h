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
 * vie_channel_manager.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CHANNEL_MANAGER_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CHANNEL_MANAGER_H_

// Defines
#include "engine_configurations.h"
#include "vie_defines.h"
#include "typedefs.h"
#include "map_wrapper.h"
#include "vie_manager_base.h"

namespace webrtc
{
class CriticalSectionWrapper;
//class VoiceEngine;
class ProcessThread;
class ViEChannel;
class VoEVideoSync;
class ViEPerformanceMonitor;
class ViEEncoder;
class VoiceEngine;

// ------------------------------------------------------------------
// ViEChannelManager
// ------------------------------------------------------------------

class ViEChannelManager: private ViEManagerBase
{
    friend class ViEChannelManagerScoped;

public:
    ViEChannelManager(int engineId, int numberOfCores,
                      ViEPerformanceMonitor& viePerformanceMonitor);
    ~ViEChannelManager();

    void SetModuleProcessThread(ProcessThread& moduleProcessThread);
    int CreateChannel(int& channelId);
    int CreateChannel(int& channelId, int originalChannel);
    int DeleteChannel(int channelId);
    int SetVoiceEngine(VoiceEngine* voiceEngine);
    int ConnectVoiceChannel(int channelId, int audioChannelId);
    int DisconnectVoiceChannel(int channelId);
    VoiceEngine* GetVoiceEngine();

private:
    // Used by ViEChannelScoped, forcing a manager user to use scoped
    ViEChannel* ViEChannelPtr(int channelId) const;
    void GetViEChannels(MapWrapper& channelMap);

    // Methods used by ViECaptureScoped and ViEEncoderScoped
    ViEEncoder* ViEEncoderPtr(int videoChannelId) const;

    bool GetFreeChannelId(int& freeChannelId);
    void ReturnChannelId(int channelId);

    // Returns true if at least one other channels uses the same ViEEncoder as channelId
    bool ChannelUsingViEEncoder(int channelId) const;

    // Members
    CriticalSectionWrapper* _ptrChannelIdCritsect; // protecting _channelMap and _freeChannelIds
    int _engineId;
    int _numberOfCores;
    ViEPerformanceMonitor& _viePerformanceMonitor;
    MapWrapper _channelMap;
    bool* _freeChannelIds;
    int _freeChannelIdsSize;
    // Encoder
    MapWrapper _vieEncoderMap; // Channel id -> ViEEncoder
    VoEVideoSync* _voiceSyncInterface;
    VoiceEngine* _voiceEngine;
    ProcessThread* _moduleProcessThread;
};

// ------------------------------------------------------------------
// ViEChannelManagerScoped
// ------------------------------------------------------------------
class ViEChannelManagerScoped: private ViEManagerScopedBase
{
public:
    ViEChannelManagerScoped(const ViEChannelManager& vieChannelManager);
    ViEChannel* Channel(int vieChannelId) const;
    ViEEncoder* Encoder(int vieChannelId) const;

    // Returns true if at lease one other channels uses the same ViEEncoder as channelId
    bool ChannelUsingViEEncoder(int channelId) const;
};

} //namespace webrtc
#endif    // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CHANNEL_MANAGER_H_
