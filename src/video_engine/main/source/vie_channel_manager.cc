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
 * vie_channel_manager.cc
 */

#include "vie_channel_manager.h"
#include "engine_configurations.h"
#include "vie_defines.h"

#include "critical_section_wrapper.h"
#include "trace.h"
#include "vie_channel.h"
#include "vie_encoder.h"
#include "process_thread.h"

// VoiceEngine
#include "voe_video_sync.h"

namespace webrtc
{

ViEChannelManagerScoped::ViEChannelManagerScoped(
                                    const ViEChannelManager& vieChannelManager)
    : ViEManagerScopedBase(vieChannelManager)
{
}

ViEChannel* ViEChannelManagerScoped::Channel(int vieChannelId) const
{
    return static_cast<const ViEChannelManager*>
                                    (_vieManager)->ViEChannelPtr(vieChannelId);
}
ViEEncoder* ViEChannelManagerScoped::Encoder(int vieChannelId) const
{
    return static_cast<const ViEChannelManager*>
                                    (_vieManager)->ViEEncoderPtr(vieChannelId);
}

bool ViEChannelManagerScoped::ChannelUsingViEEncoder(int channelId) const
{
    return (static_cast<const ViEChannelManager*>
                    (_vieManager))->ChannelUsingViEEncoder( channelId);
}

// ============================================================================
// VieChannelManager
// ============================================================================

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViEChannelManager::ViEChannelManager(int engineId,
                                     int numberOfCores,
                                     ViEPerformanceMonitor& viePerformanceMonitor)
    :   _ptrChannelIdCritsect(CriticalSectionWrapper::CreateCriticalSection()),
        _engineId(engineId), _numberOfCores(numberOfCores),
        _viePerformanceMonitor(viePerformanceMonitor), _channelMap(),
        _freeChannelIds(new bool[kViEMaxNumberOfChannels]),
        _freeChannelIdsSize(kViEMaxNumberOfChannels), _vieEncoderMap(),
        _voiceSyncInterface(NULL), _voiceEngine(NULL),
        _moduleProcessThread(NULL)
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, ViEId(engineId),
               "ViEChannelManager::ViEChannelManager(engineId: %d) - Constructor",
               engineId);

    for (int idx = 0; idx < _freeChannelIdsSize; idx++)
    {
        _freeChannelIds[idx] = true;
    }
}
// ----------------------------------------------------------------------------
// SetModuleProcessThread
// Initialize the thread context used by none time critical tasks in video channels.
// ----------------------------------------------------------------------------
void ViEChannelManager::SetModuleProcessThread( ProcessThread& moduleProcessThread)
{
    assert(!_moduleProcessThread);
    _moduleProcessThread = &moduleProcessThread;
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViEChannelManager::~ViEChannelManager()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, ViEId(_engineId),
               "ViEChannelManager Destructor, engineId: %d", _engineId);

    while (_channelMap.Size() != 0)
    {
        MapItem* item = _channelMap.First();
        const int channelId = item->GetId();
        item = NULL;
        DeleteChannel(channelId);
    }

    if (_voiceSyncInterface)
        _voiceSyncInterface->Release();
    if (_ptrChannelIdCritsect)
    {
        delete _ptrChannelIdCritsect;
        _ptrChannelIdCritsect = NULL;
    }
    if (_freeChannelIds)
    {
        delete[] _freeChannelIds;
        _freeChannelIds = NULL;
        _freeChannelIdsSize = 0;
    }
}

// ----------------------------------------------------------------------------
// CreateChannel
//
// Creates a new channel. 'channelId' will be the id of the created channel
// ----------------------------------------------------------------------------
int ViEChannelManager::CreateChannel(int& channelId)
{
    CriticalSectionScoped cs(*_ptrChannelIdCritsect);

    // Get a free id for the new channel
    if (GetFreeChannelId(channelId) == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "Max number of channels reached: %d", _channelMap.Size());
        return -1;
    }

    ViEChannel* vieChannel = new ViEChannel(channelId, _engineId,
                                            _numberOfCores,
                                            *_moduleProcessThread);
    if (vieChannel == NULL)
    {
        ReturnChannelId(channelId);
        return -1;
    }
    if (vieChannel->Init() != 0)
    {
        // Could not init channel
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s could not init channel", __FUNCTION__, channelId);
        ReturnChannelId(channelId);
        delete vieChannel;
        vieChannel = NULL;
        return -1;

    }
    // There is no ViEEncoder for this channel, create one with default settings
    ViEEncoder* vieEncoder = new ViEEncoder(_engineId, channelId,
                                            _numberOfCores,
                                            *_moduleProcessThread);
    if (vieEncoder == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s(videoChannelId: %d) - Could not create a new encoder",
                   __FUNCTION__, channelId);
        delete vieChannel;
        return -1;
    }

    // Add to the map
    if (_vieEncoderMap.Insert(channelId, vieEncoder) != 0)
    {
        // Could not add to the map
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Could not add new encoder for video channel %d",
                   __FUNCTION__, channelId);
        delete vieChannel;
        delete vieEncoder;
        return -1;
    }
    _channelMap.Insert(channelId, vieChannel);
    // Register the channel at the encoder
    RtpRtcp* ptrSendRtpRtcpModule = vieEncoder->SendRtpRtcpModule();
    if (vieChannel->RegisterSendRtpRtcpModule(*ptrSendRtpRtcpModule) != 0)
    {
        assert(false);
        _vieEncoderMap.Erase(channelId);
        _channelMap.Erase(channelId);
        ReturnChannelId(channelId);
        delete vieChannel;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, channelId),
                   "%s: Could not register rtp module %d", __FUNCTION__,
                   channelId);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// CreateChannel
//
// Creates a channel and attaches to an already existing ViEEncoder
// ----------------------------------------------------------------------------

int ViEChannelManager::CreateChannel(int& channelId, int originalChannel)
{
    CriticalSectionScoped cs(*_ptrChannelIdCritsect);

    // Check that originalChannel already exists
    ViEEncoder* vieEncoder = ViEEncoderPtr(originalChannel);
    if (vieEncoder == NULL)
    {
        // The original channel doesn't exist
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Original channel doesn't exist", __FUNCTION__,
                   originalChannel);
        return -1;
    }
    // Get a free id for the new channel
    if (GetFreeChannelId(channelId) == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "Max number of channels reached: %d", _channelMap.Size());
        return -1;
    }
    ViEChannel* vieChannel = new ViEChannel(channelId, _engineId,
                                            _numberOfCores,
                                            *_moduleProcessThread);
    if (vieChannel == NULL)
    {
        ReturnChannelId(channelId);
        return -1;
    }
    if (vieChannel->Init() != 0)
    {
        // Could not init channel
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s could not init channel", __FUNCTION__, channelId);
        ReturnChannelId(channelId);
        delete vieChannel;
        vieChannel = NULL;
        return -1;
    }
    if (_vieEncoderMap.Insert(channelId, vieEncoder) != 0)
    {
        // Could not add to the map
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Could not add new encoder for video channel %d",
                   __FUNCTION__, channelId);
        ReturnChannelId(channelId);
        delete vieChannel;
        return -1;
    }

    // Set the same encoder settings for the channel as used by the master channel.
    // Do this before attaching rtp module to ensure all rtp cihldren has the same codec type
    VideoCodec encoder;
    if (vieEncoder->GetEncoder(encoder) == 0)
    {
        vieChannel->SetSendCodec(encoder);
    }
    _channelMap.Insert(channelId, vieChannel);

    // Register the channel at the encoder
    RtpRtcp* ptrSendRtpRtcpModule = vieEncoder->SendRtpRtcpModule();
    if (vieChannel->RegisterSendRtpRtcpModule(*ptrSendRtpRtcpModule) != 0)
    {
        assert(false);
        _vieEncoderMap.Erase(channelId);
        _channelMap.Erase(channelId);
        ReturnChannelId(channelId);
        delete vieChannel;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, channelId),
                   "%s: Could not register rtp module %d", __FUNCTION__,
                   channelId);
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// DeleteChannel
// ----------------------------------------------------------------------------

int ViEChannelManager::DeleteChannel(int channelId)
{
    ViEChannel* vieChannel = NULL;
    ViEEncoder* vieEncoder = NULL;
    {
        // Write lock to make sure no one is using the channel
        ViEManagerWriteScoped wl(*this);

        // Protect the map
        CriticalSectionScoped cs(*_ptrChannelIdCritsect);

        MapItem* mapItem = _channelMap.Find(channelId);
        if (mapItem == NULL)
        {
            // No such channel
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s Channel doesn't exist: %d", __FUNCTION__, channelId);
            return -1;
        }
        vieChannel = reinterpret_cast<ViEChannel*> (mapItem->GetItem());
        _channelMap.Erase(mapItem);
        // Deregister the channel from the ViEEncoder to stop the media flow
        vieChannel->DeregisterSendRtpRtcpModule();
        ReturnChannelId(channelId);

        // Find the encoder object
        mapItem = _vieEncoderMap.Find(channelId);
        if (mapItem == NULL)
        {
            assert(false);
            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s ViEEncoder not found for channel %d", __FUNCTION__,
                       channelId);
            return -1;
        }
        // Get the ViEEncoder item
        vieEncoder = reinterpret_cast<ViEEncoder*> (mapItem->GetItem());

        // Check if other channels are using the same encoder
        if (ChannelUsingViEEncoder(channelId))
        {
            // Don't delete the ViEEncoder, at least on other channel is using it.
            WEBRTC_TRACE(
                       webrtc::kTraceInfo,
                       webrtc::kTraceVideo,
                       ViEId(_engineId),
                       "%s ViEEncoder removed from map for channel %d, not deleted",
                       __FUNCTION__, channelId);
            vieEncoder = NULL;
        } else
        {
            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s ViEEncoder deleted for channel %d", __FUNCTION__,
                       channelId);
            // Delete later when we've released the critsect
        }
        // We can't erase the item before we've checked for other channels using same ViEEncoder
        _vieEncoderMap.Erase(mapItem);

    }
    // Leave the write critsect before deleting the objects.
    // Deleting a channel can cause other objects, such as renderers, to be deleted and might take time
    if (vieEncoder)
    {
        delete vieEncoder;
    }
    delete vieChannel;

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
               "%s Channel %d deleted", __FUNCTION__, channelId);
    return 0;
}

// ----------------------------------------------------------------------------
// Channel
//
// Returns a pointer to the channel with id 'channelId'
// ----------------------------------------------------------------------------

ViEChannel* ViEChannelManager::ViEChannelPtr(int channelId) const
{
    CriticalSectionScoped cs(*_ptrChannelIdCritsect);
    MapItem* mapItem = _channelMap.Find(channelId);
    if (mapItem == NULL)
    {
        // No such channel
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s Channel doesn't exist: %d", __FUNCTION__, channelId);
        return NULL;
    }
    ViEChannel* vieChannel = reinterpret_cast<ViEChannel*> (mapItem->GetItem());
    return vieChannel;
}

// ----------------------------------------------------------------------------
// GetChannels
//
// Adds all channels to channelMap
// ----------------------------------------------------------------------------

void ViEChannelManager::GetViEChannels(MapWrapper& channelMap)
{
    CriticalSectionScoped cs(*_ptrChannelIdCritsect);
    if (channelMap.Size() == 0)
    {
        // No channels
        return;
    }
    // Add all items to 'channelMap'
    for (MapItem* item = _channelMap.First(); item != NULL; item
        = _channelMap.Next(item))
    {
        channelMap.Insert(item->GetId(), item->GetItem());
    }
    return;
}

// ----------------------------------------------------------------------------
// ViEEncoderPtr
//
// Gets the ViEEncoder used as input for videoChannelId
// ----------------------------------------------------------------------------

ViEEncoder* ViEChannelManager::ViEEncoderPtr(int videoChannelId) const
{
    CriticalSectionScoped cs(*_ptrChannelIdCritsect);
    MapItem* mapItem = _vieEncoderMap.Find(videoChannelId);
    if (mapItem == NULL)
    {
        // No ViEEncoder for this channel...
        return NULL;
    }
    ViEEncoder* vieEncoder = static_cast<ViEEncoder*> (mapItem->GetItem());
    return vieEncoder;
}

// ----------------------------------------------------------------------------
// GetFreeChannelId
//
// Returns true if we found a new channel id, freeChannelId, false otherwise
// ----------------------------------------------------------------------------
bool ViEChannelManager::GetFreeChannelId(int& freeChannelId)
{
    CriticalSectionScoped cs(*_ptrChannelIdCritsect);
    int idx = 0;
    while (idx < _freeChannelIdsSize)
    {
        if (_freeChannelIds[idx] == true)
        {
            // We've found a free id, allocate it and return
            _freeChannelIds[idx] = false;
            freeChannelId = idx + kViEChannelIdBase;
            return true;
        }
        idx++;
    }
    // No free channel id
    freeChannelId = -1;
    return false;
}

// ----------------------------------------------------------------------------
// ReturnChannelID
//
// Returns a previously allocated channel id
// ----------------------------------------------------------------------------
void ViEChannelManager::ReturnChannelId(int channelId)
{
    CriticalSectionScoped cs(*_ptrChannelIdCritsect);
    assert(channelId < kViEMaxNumberOfChannels+kViEChannelIdBase && channelId>=kViEChannelIdBase);
    _freeChannelIds[channelId - kViEChannelIdBase] = true;
}

// ----------------------------------------------------------------------------
// ChannelUsingViEEncoder
//
// Returns true if at least one nother channel is using the same encoder
// ----------------------------------------------------------------------------

bool ViEChannelManager::ChannelUsingViEEncoder(int channelId) const
{
    CriticalSectionScoped cs(*_ptrChannelIdCritsect);
    MapItem* channelItem = _vieEncoderMap.Find(channelId);
    if (channelItem == NULL)
    {
        // No ViEEncoder for this channel...
        return false;
    }
    ViEEncoder* channelEncoder =
        static_cast<ViEEncoder*> (channelItem->GetItem());

    // Loop through all other channels to see if anyone points at the same ViEEncoder
    MapItem* mapItem = _vieEncoderMap.First();
    while (mapItem)
    {
        if (mapItem->GetId() != channelId)
        {
            if (channelEncoder == static_cast<ViEEncoder*> (mapItem->GetItem()))
            {
                // We've found another channel using the same ViEEncoder
                return true;
            }
        }
        mapItem = _vieEncoderMap.Next(mapItem);
    }
    return false;
}

// ----------------------------------------------------------------------------
// SetVoiceEngine
//
// Set the voice engine instance to be used by all video channels. We are interested in the voice engine sync interfaces
// ----------------------------------------------------------------------------
int ViEChannelManager::SetVoiceEngine(VoiceEngine* voiceEngine)
{

    // Write lock to make sure no one is using the channel
    ViEManagerWriteScoped wl(*this);

    CriticalSectionScoped cs(*_ptrChannelIdCritsect);

    VoEVideoSync* syncInterface = NULL;
    if (voiceEngine)
    {
        // Get new sync interface;
        syncInterface = VoEVideoSync::GetInterface(voiceEngine);
        if (!syncInterface)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s Can't get audio sync interface from VoiceEngine.",
                       __FUNCTION__);

            if (syncInterface)
            {
                syncInterface->Release();
            }
            return -1;
        }
    }

    for (MapItem* item = _channelMap.First(); item != NULL; item
        = _channelMap.Next(item))
    {
        ViEChannel* channel = static_cast<ViEChannel*> (item->GetItem());
        assert(channel);
        channel->SetVoiceChannel(-1, syncInterface);
    }
    if (_voiceSyncInterface)
    {
        _voiceSyncInterface->Release();
    }
    _voiceEngine = voiceEngine;
    _voiceSyncInterface = syncInterface;
    return 0;

}
VoiceEngine* ViEChannelManager::GetVoiceEngine()
{
    CriticalSectionScoped cs(*_ptrChannelIdCritsect);
    return _voiceEngine;

}

// ----------------------------------------------------------------------------
// ConnectVoiceChannel
//
// Enables lip sync of the channel.
// ----------------------------------------------------------------------------
int ViEChannelManager::ConnectVoiceChannel(int channelId, int audioChannelId)
{
    CriticalSectionScoped cs(*_ptrChannelIdCritsect);

    if (_voiceSyncInterface == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId, channelId),
                   "No VoE set");
        return -1;
    }
    ViEChannel* channel = ViEChannelPtr(channelId);
    if (!channel)
    {
        return -1;
    }
    return channel->SetVoiceChannel(audioChannelId, _voiceSyncInterface);

}

// ----------------------------------------------------------------------------
// DisconnectVoiceChannel
//
// Disables lip sync of the channel.
// ----------------------------------------------------------------------------
int ViEChannelManager::DisconnectVoiceChannel(int channelId)
{
    CriticalSectionScoped cs(*_ptrChannelIdCritsect);
    ViEChannel* channel = ViEChannelPtr(channelId);
    if (channel)
    {
        channel->SetVoiceChannel(-1, NULL);
        return 0;
    } else
    {
        return -1;
    }
}
} // namespace webrtc
