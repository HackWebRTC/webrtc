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
 * vie_input_manager.cc
 */

#include "vie_input_manager.h"
#include "vie_defines.h"

#include "common_types.h"
#include "critical_section_wrapper.h"
#include "video_capture_factory.h"
#include "video_coding.h"
#include "video_coding_defines.h"
#include "rw_lock_wrapper.h"
#include "trace.h"
#include "vie_capturer.h"
#include "vie_file_player.h"
#include "vie_errors.h"

#include <cassert>

namespace webrtc {

//=============================================================================
// ViEInputManager
//=============================================================================

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViEInputManager::ViEInputManager(const int engineId)
    : _engineId(engineId),
      _mapCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
      _vieFrameProviderMap(),
      _ptrCaptureDeviceInfo(NULL),
      _freeCaptureDeviceId(),
      _freeFileId(),
      _moduleProcessThread(NULL)
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, ViEId(_engineId), "%s",
               __FUNCTION__);

    for (int idx = 0; idx < kViEMaxCaptureDevices; idx++)
    {
        _freeCaptureDeviceId[idx] = true;
    }
    _ptrCaptureDeviceInfo =
        VideoCaptureFactory::CreateDeviceInfo(
            ViEModuleId(_engineId));
    for (int idx = 0; idx < kViEMaxFilePlayers; idx++)
    {
        _freeFileId[idx] = true;
    }

}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViEInputManager::~ViEInputManager()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, ViEId(_engineId), "%s",
               __FUNCTION__);
    while (_vieFrameProviderMap.Size() != 0)
    {
        MapItem* item = _vieFrameProviderMap.First();
        assert(item);
        ViEFrameProviderBase* frameProvider = static_cast<ViEFrameProviderBase*>
                                                              (item->GetItem());
        _vieFrameProviderMap.Erase(item);
        delete frameProvider;
    }

    delete &_mapCritsect;
    if (_ptrCaptureDeviceInfo)
    {
        delete _ptrCaptureDeviceInfo;
        _ptrCaptureDeviceInfo = NULL;
    }
}

// ----------------------------------------------------------------------------
// SetModuleProcessThread
// Initialize the thread context used by none time critical tasks in capture modules.
// ----------------------------------------------------------------------------
void ViEInputManager::SetModuleProcessThread(ProcessThread& moduleProcessThread)
{
    assert(!_moduleProcessThread);
    _moduleProcessThread = &moduleProcessThread;
}
// ----------------------------------------------------------------------------
// NumberOfCaptureDevices
//
// Returns the number of available capture devices
// ----------------------------------------------------------------------------

// Capture device information
int ViEInputManager::NumberOfCaptureDevices()
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId), "%s",
               __FUNCTION__);
    assert(_ptrCaptureDeviceInfo);
    return _ptrCaptureDeviceInfo->NumberOfDevices();
}

// ----------------------------------------------------------------------------
// GetDeviceName
// ----------------------------------------------------------------------------

int ViEInputManager::GetDeviceName(WebRtc_UWord32 deviceNumber,
                                   WebRtc_UWord8* deviceNameUTF8,
                                   WebRtc_UWord32 deviceNameLength,
                                   WebRtc_UWord8* deviceUniqueIdUTF8,
                                   WebRtc_UWord32 deviceUniqueIdUTF8Length)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
               "%s(deviceNumber: %d)", __FUNCTION__, deviceNumber);
    assert(_ptrCaptureDeviceInfo);
    return _ptrCaptureDeviceInfo->GetDeviceName(deviceNumber, deviceNameUTF8,
                                                deviceNameLength,
                                                deviceUniqueIdUTF8,
                                                deviceUniqueIdUTF8Length);
}

// ----------------------------------------------------------------------------
// NumberOfCaptureCapabilities
//
// Returns the number of capture capabilities for the specified capture device
// ----------------------------------------------------------------------------

int ViEInputManager::NumberOfCaptureCapabilities(
                                        const WebRtc_UWord8* deviceUniqueIdUTF8)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId), "%s",
               __FUNCTION__);
    assert(_ptrCaptureDeviceInfo);
    return _ptrCaptureDeviceInfo->NumberOfCapabilities(deviceUniqueIdUTF8);
}

// ----------------------------------------------------------------------------
// GetCaptureCapability
// ----------------------------------------------------------------------------

int ViEInputManager::GetCaptureCapability(const WebRtc_UWord8* deviceUniqueIdUTF8,
                                          const WebRtc_UWord32 deviceCapabilityNumber,
                                          CaptureCapability& capability)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
               "%s(deviceUniqueIdUTF8: %s, deviceCapabilityNumber: %d)",
               __FUNCTION__, deviceUniqueIdUTF8, deviceCapabilityNumber);
    assert(_ptrCaptureDeviceInfo);
    VideoCaptureCapability moduleCapability;
    int result = _ptrCaptureDeviceInfo->GetCapability(deviceUniqueIdUTF8,
                                                      deviceCapabilityNumber,
                                                      moduleCapability);
    if (result != 0)
      return result;

    // Copy from module type to public type
    capability.expectedCaptureDelay = moduleCapability.expectedCaptureDelay;
    capability.height = moduleCapability.height;
    capability.width = moduleCapability.width;
    capability.interlaced = moduleCapability.interlaced;
    capability.rawType = moduleCapability.rawType;
    capability.codecType = moduleCapability.codecType;
    capability.maxFPS = moduleCapability.maxFPS;
    return result;
}

int ViEInputManager::GetOrientation(const WebRtc_UWord8* deviceUniqueIdUTF8,
                                    RotateCapturedFrame &orientation)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
               "%s(deviceUniqueIdUTF8: %s,)", __FUNCTION__, deviceUniqueIdUTF8);
    assert(_ptrCaptureDeviceInfo);
    VideoCaptureRotation moduleOrientation;
    int result = _ptrCaptureDeviceInfo->GetOrientation(deviceUniqueIdUTF8,
                                                       moduleOrientation);
    // Copy from module type to public type
    switch (moduleOrientation)
    {
        case kCameraRotate0:
            orientation = RotateCapturedFrame_0;
            break;
        case kCameraRotate90:
            orientation = RotateCapturedFrame_90;
            break;
        case kCameraRotate180:
            orientation = RotateCapturedFrame_180;
            break;
        case kCameraRotate270:
            orientation = RotateCapturedFrame_270;
            break;
        default:
            assert(!"Unknown enum");
    }
    return result;
}

//------------------------------------------------------------------------------
//
// DisplayCaptureSettingsDialogBox
// Show OS specific Capture settings.
// Return 0 on success.
//------------------------------------------------------------------------------
int ViEInputManager::DisplayCaptureSettingsDialogBox(
                                                     const WebRtc_UWord8* deviceUniqueIdUTF8,
                                                     const WebRtc_UWord8* dialogTitleUTF8,
                                                     void* parentWindow,
                                                     WebRtc_UWord32 positionX,
                                                     WebRtc_UWord32 positionY)
{
    assert(_ptrCaptureDeviceInfo);
    return _ptrCaptureDeviceInfo->DisplayCaptureSettingsDialogBox(
                                                                  deviceUniqueIdUTF8,
                                                                  dialogTitleUTF8,
                                                                  parentWindow,
                                                                  positionX,
                                                                  positionY);
}
// ----------------------------------------------------------------------------
// CreateCaptureDevice
//
// Creates a capture module for the specified capture device and assigns
// a capture device id for the device
// ----------------------------------------------------------------------------

int ViEInputManager::CreateCaptureDevice(const WebRtc_UWord8* deviceUniqueIdUTF8,
                                         const WebRtc_UWord32 deviceUniqueIdUTF8Length,
                                         int& captureId)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
               "%s(deviceUniqueId: %s)", __FUNCTION__, deviceUniqueIdUTF8);
    CriticalSectionScoped cs(_mapCritsect);

    // Make sure the device is not already allocated
    for (MapItem* item = _vieFrameProviderMap.First(); item != NULL;
         item = _vieFrameProviderMap.Next(item))
    {
        if (item->GetId() >= kViECaptureIdBase &&
            item->GetId() <= kViECaptureIdMax) // Make sure it is a capture device
        {
            ViECapturer* vieCapture = static_cast<ViECapturer*> (item->GetItem());
            assert(vieCapture);
            if (strncmp((char*) vieCapture->CurrentDeviceName(),
                        (char*) deviceUniqueIdUTF8,
                        strlen((char*) vieCapture->CurrentDeviceName())) == 0)
            {
                return kViECaptureDeviceAlreadyAllocated;
            }
        }
    }

    // Make sure the device name is valid
    bool foundDevice = false;
    for (WebRtc_UWord32 deviceIndex = 0;
         deviceIndex < _ptrCaptureDeviceInfo->NumberOfDevices(); ++deviceIndex)
    {
        if (deviceUniqueIdUTF8Length >kVideoCaptureUniqueNameLength)
        {
            // user's string length is longer than the max
            return -1;
        }

        WebRtc_UWord8 foundName[kVideoCaptureDeviceNameLength] = "";
        WebRtc_UWord8 foundUniqueName[kVideoCaptureUniqueNameLength] = "";
        _ptrCaptureDeviceInfo->GetDeviceName(deviceIndex, foundName,
                                             kVideoCaptureDeviceNameLength,
                                             foundUniqueName,
                                             kVideoCaptureUniqueNameLength);

        if (strncmp((char*) deviceUniqueIdUTF8, (char*) foundUniqueName,
                    strlen((char*) deviceUniqueIdUTF8)) == 0)
        {
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s:%d Capture device was found by unique ID: %s. Returning",
                       __FUNCTION__, __LINE__, deviceUniqueIdUTF8);
            foundDevice = true;
            break;
        }
    }
    if (!foundDevice)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s:%d Capture device NOT found by unique ID: %s. Returning",
                   __FUNCTION__, __LINE__, deviceUniqueIdUTF8);
        return kViECaptureDeviceDoesNotExist;
    }

    int newcaptureId = 0;
    if (GetFreeCaptureId(newcaptureId) == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Maximum supported number of capture devices already in use",
                   __FUNCTION__);
        return kViECaptureDeviceMaxNoDevicesAllocated;
    }
    ViECapturer* vieCapture =ViECapturer::CreateViECapture(newcaptureId,
                                                         _engineId,
                                                         deviceUniqueIdUTF8,
                                                         deviceUniqueIdUTF8Length,
                                                         *_moduleProcessThread);
    if (vieCapture == NULL)
    {
        ReturnCaptureId(newcaptureId);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Could not create capture module for %s", __FUNCTION__,
                   deviceUniqueIdUTF8);
        return kViECaptureDeviceUnknownError;
    }

    if (_vieFrameProviderMap.Insert(newcaptureId, vieCapture) != 0)
    {
        ReturnCaptureId(newcaptureId);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Could not insert capture module for %s", __FUNCTION__,
                   deviceUniqueIdUTF8);
        return kViECaptureDeviceUnknownError;
    }
    captureId = newcaptureId;
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
               "%s(deviceUniqueId: %s, captureId: %d)", __FUNCTION__,
               deviceUniqueIdUTF8, captureId);

    return 0;
}

int ViEInputManager::CreateCaptureDevice(VideoCaptureModule& captureModule,
                                         int& captureId)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId), "%s", __FUNCTION__);

    CriticalSectionScoped cs(_mapCritsect);

    int newcaptureId = 0;
    if (GetFreeCaptureId(newcaptureId) == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Maximum supported number of capture devices already in use",
                   __FUNCTION__);
        return kViECaptureDeviceMaxNoDevicesAllocated;
    }

    ViECapturer* vieCapture = ViECapturer::CreateViECapture(newcaptureId,
                                                          _engineId,
                                                          captureModule,
                                                          *_moduleProcessThread);
    if (vieCapture == NULL)
    {
        ReturnCaptureId(newcaptureId);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Could attach capture module.", __FUNCTION__);
        return kViECaptureDeviceUnknownError;
    }
    if (_vieFrameProviderMap.Insert(newcaptureId, vieCapture) != 0)
    {
        ReturnCaptureId(newcaptureId);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Could not insert capture module", __FUNCTION__);
        return kViECaptureDeviceUnknownError;
    }

    captureId = newcaptureId;
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
               "%s, captureId: %d", __FUNCTION__, captureId);

    return 0;

}

// ----------------------------------------------------------------------------
// DestroyCaptureDevice
//
// Releases the capture device with specified id
// ----------------------------------------------------------------------------

int ViEInputManager::DestroyCaptureDevice(const int captureId)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
               "%s(captureId: %d)", __FUNCTION__, captureId);

    ViECapturer* vieCapture = NULL;
    {
        // We need exclusive access to the object to delete it
        ViEManagerWriteScoped wl(*this); // Take this write lock first since the read lock is taken before _mapCritsect
        CriticalSectionScoped cs(_mapCritsect);

        vieCapture = ViECapturePtr(captureId);
        if (vieCapture == NULL)
        {
            // No capture deveice with that id
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s(captureId: %d) - No such capture device id",
                       __FUNCTION__, captureId);
            return -1;
        }
        WebRtc_UWord32 numCallbacks = vieCapture->NumberOfRegisteredFrameCallbacks();
        if (numCallbacks > 0)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s(captureId: %d) - %u registered callbacks when destroying capture device",
                       __FUNCTION__, captureId, numCallbacks);
        }
        _vieFrameProviderMap.Erase(captureId);
        ReturnCaptureId(captureId);
    } // Leave cs before deleting the capture object. This is because deleting the object might cause deletions of renderers so we prefer to not have a lock at that time.
    delete vieCapture;
    return 0;
}
// ----------------------------------------------------------------------------
// CreateExternalCaptureDevice
//
// Creates a capture module to be used with external captureing.
// ----------------------------------------------------------------------------
int ViEInputManager::CreateExternalCaptureDevice(ViEExternalCapture*& externalCapture,
                                                 int& captureId)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId), "%s",
               __FUNCTION__);
    CriticalSectionScoped cs(_mapCritsect);

    int newcaptureId = 0;
    if (GetFreeCaptureId(newcaptureId) == false)
    {
        WEBRTC_TRACE( webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Maximum supported number of capture devices already in use",
                   __FUNCTION__);
        return kViECaptureDeviceMaxNoDevicesAllocated;
    }

    ViECapturer* vieCapture = ViECapturer::CreateViECapture(newcaptureId,
                                                          _engineId, NULL, 0,
                                                          *_moduleProcessThread);
    if (vieCapture == NULL)
    {
        ReturnCaptureId(newcaptureId);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Could not create capture module for external capture.",
                   __FUNCTION__);
        return kViECaptureDeviceUnknownError;
    }

    if (_vieFrameProviderMap.Insert(newcaptureId, vieCapture) != 0)
    {
        ReturnCaptureId(newcaptureId);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Could not insert capture module for external capture.",
                   __FUNCTION__);
        return kViECaptureDeviceUnknownError;
    }
    captureId = newcaptureId;
    externalCapture = vieCapture;
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
               "%s, captureId: %d)", __FUNCTION__, captureId);
    return 0;
}

int ViEInputManager::CreateFilePlayer(const WebRtc_Word8* fileNameUTF8,
                                      const bool loop,
                                      const webrtc::FileFormats fileFormat,
                                      VoiceEngine* vePtr, int& fileId)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
               "%s(deviceUniqueId: %s)", __FUNCTION__, fileNameUTF8);

    CriticalSectionScoped cs(_mapCritsect);

    int newFileId = 0;
    if (GetFreeFileId(newFileId) == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Maximum supported number of file players already in use",
                   __FUNCTION__);
        return kViEFileMaxNoOfFilesOpened;
    }

    ViEFilePlayer* vieFilePlayer = ViEFilePlayer::CreateViEFilePlayer(
                                                   newFileId, _engineId, fileNameUTF8,
                                                   loop, fileFormat, *this, vePtr);
    if (vieFilePlayer == NULL)
    {
        ReturnFileId(newFileId);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Could not open file %s for playback", __FUNCTION__,
                   fileNameUTF8);
        return kViEFileUnknownError;
    }

    if (_vieFrameProviderMap.Insert(newFileId, vieFilePlayer) != 0)
    {
        ReturnCaptureId(newFileId);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                   "%s: Could not insert file player for %s", __FUNCTION__,
                   fileNameUTF8);
        delete vieFilePlayer;
        return kViEFileUnknownError;
    }

    fileId = newFileId;
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
               "%s(filename: %s, fileId: %d)", __FUNCTION__, fileNameUTF8,
               newFileId);
    return 0;
}

int ViEInputManager::DestroyFilePlayer(int fileId)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
               "%s(fileId: %d)", __FUNCTION__, fileId);

    ViEFilePlayer* vieFilePlayer = NULL;
    {
        // We need exclusive access to the object to delete it
        ViEManagerWriteScoped wl(*this); // Take this write lock first since the read lock is taken before _mapCritsect

        CriticalSectionScoped cs(_mapCritsect);

        vieFilePlayer = ViEFilePlayerPtr(fileId);
        if (vieFilePlayer == NULL)
        {
            // No capture deveice with that id
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s(fileId: %d) - No such file player",
                       __FUNCTION__, fileId);

            return -1;
        }
        int numCallbacks =
            vieFilePlayer->NumberOfRegisteredFrameCallbacks();
        if (numCallbacks > 0)
        {
            WEBRTC_TRACE( webrtc::kTraceWarning, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s(fileId: %d) - %u registered callbacks when destroying file player",
                       __FUNCTION__, fileId, numCallbacks);
        }
        _vieFrameProviderMap.Erase(fileId);
        ReturnFileId(fileId);
    } // Leave cs before deleting the file object. This is because deleting the object might cause deletions of renderers so we prefer to not have a lock at that time.
    delete vieFilePlayer;
    return 0;
}

// ============================================================================
// Private methods
// ============================================================================

// ----------------------------------------------------------------------------
// GetFreeCaptureId
//
// Gets and allocates a free capture device id. Assumed protected by caller
// ----------------------------------------------------------------------------

// Private, asummed protected
bool ViEInputManager::GetFreeCaptureId(int& freecaptureId)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId), "%s",
               __FUNCTION__);

    for (int id = 0; id < kViEMaxCaptureDevices; id++)
    {
        if (_freeCaptureDeviceId[id])
        {
            // We found a free capture device id
            _freeCaptureDeviceId[id] = false;
            freecaptureId = id + kViECaptureIdBase;
            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s: new id: %d", __FUNCTION__, freecaptureId);
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// ReturnCaptureId
//
// Frees a capture id assigned in GetFreeCaptureId
// ----------------------------------------------------------------------------

void ViEInputManager::ReturnCaptureId(int captureId)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId), "%s(%d)",
               __FUNCTION__, captureId);

    CriticalSectionScoped cs(_mapCritsect);
    if (captureId >= kViECaptureIdBase &&
        captureId < kViEMaxCaptureDevices + kViECaptureIdBase)
    {
        _freeCaptureDeviceId[captureId - kViECaptureIdBase] = true;
    }

    return;
}

// ----------------------------------------------------------------------------
// GetFreeFileId
//
// Gets and allocates a free file id. Assumed protected by caller
// ----------------------------------------------------------------------------

// Private, asumed protected
bool ViEInputManager::GetFreeFileId(int& freeFileId)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId), "%s",
               __FUNCTION__);

    for (int id = 0; id < kViEMaxFilePlayers; id++)
    {
        if (_freeFileId[id])
        {
            // We found a free capture device id
            _freeFileId[id] = false;
            freeFileId = id + kViEFileIdBase;
            WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId),
                       "%s: new id: %d", __FUNCTION__, freeFileId);
            return true;
        }
    }
    return false;
}
// ----------------------------------------------------------------------------
// ReturnFileId
//
// Frees a file id assigned in GetFreeFileId
// ----------------------------------------------------------------------------
void ViEInputManager::ReturnFileId(int fileId)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(_engineId), "%s(%d)",
               __FUNCTION__, fileId);

    CriticalSectionScoped cs(_mapCritsect);
    if (fileId >= kViEFileIdBase &&
        fileId < kViEMaxFilePlayers + kViEFileIdBase)
    {
        _freeFileId[fileId - kViEFileIdBase] = true;
    }
    return;
}

// ============================================================================
// Methods used by ViECaptureScoped

// ----------------------------------------------------------------------------
// ViECapturePtr
//
// Gets the ViECapturer for the capture device id
// ----------------------------------------------------------------------------

ViECapturer* ViEInputManager::ViECapturePtr(int captureId) const
{
    if (!(captureId >= kViECaptureIdBase &&
        captureId <= kViECaptureIdBase + kViEMaxCaptureDevices))
        return NULL;

    CriticalSectionScoped cs(_mapCritsect);
    MapItem* mapItem = _vieFrameProviderMap.Find(captureId);
    if (mapItem == NULL)
    {
        // No ViEEncoder for this channel...
        return NULL;
    }
    ViECapturer* vieCapture = static_cast<ViECapturer*> (mapItem->GetItem());
    return vieCapture;
}

// ----------------------------------------------------------------------------
// ViEFrameProvider
//
// Gets the ViEFrameProvider for this capture observer.
// ----------------------------------------------------------------------------

ViEFrameProviderBase* ViEInputManager::ViEFrameProvider(
                                  const ViEFrameCallback* captureObserver) const
{

    assert(captureObserver);
    CriticalSectionScoped cs(_mapCritsect);

    for (MapItem* providerItem = _vieFrameProviderMap.First(); providerItem
        != NULL; providerItem = _vieFrameProviderMap.Next(providerItem))
    {
        ViEFrameProviderBase* vieFrameProvider = static_cast<ViEFrameProviderBase*>
                                                 (providerItem->GetItem());
        assert(vieFrameProvider != NULL);

        if (vieFrameProvider->IsFrameCallbackRegistered(captureObserver))
        {
            // We found it
            return vieFrameProvider;
        }
    }
    // No capture device set for this channel
    return NULL;
}

// ----------------------------------------------------------------------------
// ViEFrameProvider
//
// Gets the ViEFrameProvider for this capture observer.
// ----------------------------------------------------------------------------

ViEFrameProviderBase* ViEInputManager::ViEFrameProvider(int providerId) const
{
    CriticalSectionScoped cs(_mapCritsect);
    MapItem* mapItem = _vieFrameProviderMap.Find(providerId);
    if (mapItem == NULL)
    {
        return NULL;
    }

    ViEFrameProviderBase* vieFrameProvider = static_cast<ViEFrameProviderBase*>
                                             (mapItem->GetItem());
    return vieFrameProvider;
}

// ----------------------------------------------------------------------------
// GetViECaptures
//
// Gets the the entire map with GetViECaptures
// ----------------------------------------------------------------------------
void ViEInputManager::GetViECaptures(MapWrapper& vieCaptureMap)
{
    CriticalSectionScoped cs(_mapCritsect);

    if (_vieFrameProviderMap.Size() == 0)
    {
        // No ViECaptures
        return;
    }
    // Add all items to the map
    for (MapItem* item = _vieFrameProviderMap.First();
        item != NULL;
        item = _vieFrameProviderMap.Next(item))
    {
        vieCaptureMap.Insert(item->GetId(), item->GetItem());
    }
    return;
}

// ----------------------------------------------------------------------------
// ViEFilePlayerPtr
//
// Gets the ViEFilePlayer for this fileId
// ----------------------------------------------------------------------------

ViEFilePlayer* ViEInputManager::ViEFilePlayerPtr(int fileId) const
{
    if (fileId < kViEFileIdBase || fileId > kViEFileIdMax)
        return NULL;

    CriticalSectionScoped cs(_mapCritsect);
    MapItem* mapItem = _vieFrameProviderMap.Find(fileId);
    if (mapItem == NULL)
    {
        // No ViEFilePlayer for this fileId...
        return NULL;
    }
    ViEFilePlayer* vieFilePlayer =
        static_cast<ViEFilePlayer*> (mapItem->GetItem());
    return vieFilePlayer;
}

// ----------------------------------------------------------------------------
// ViEInputManagerScoped
//
// Provides protected access to ViEInputManater
// ----------------------------------------------------------------------------
ViEInputManagerScoped::ViEInputManagerScoped(
                                          const ViEInputManager& vieInputManager)
    : ViEManagerScopedBase(vieInputManager)
{
}
ViECapturer* ViEInputManagerScoped::Capture(int captureId) const
{
    return static_cast<const ViEInputManager*>
           (vie_manager_)->ViECapturePtr(captureId);
}
ViEFrameProviderBase* ViEInputManagerScoped::FrameProvider(
                                      const ViEFrameCallback* captureObserver) const
{
    return static_cast<const ViEInputManager*>
           (vie_manager_)->ViEFrameProvider(captureObserver);
}
ViEFrameProviderBase* ViEInputManagerScoped::FrameProvider(int providerId) const
{
    return static_cast<const ViEInputManager*>
            (vie_manager_)->ViEFrameProvider( providerId);
}

ViEFilePlayer* ViEInputManagerScoped::FilePlayer(int fileId) const
{
    return static_cast<const ViEInputManager*>
           (vie_manager_)->ViEFilePlayerPtr(fileId);
}

} // namespace webrtc
