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
 * vie_impl.cc
 */

#include "vie_impl.h"
#include "trace.h"

#if (defined(_WIN32) || defined(_WIN64))
#include <Windows.h>    // For LoadLibrary
#include <tchar.h>    // For _T
#endif

#ifdef WEBRTC_ANDROID
#include "video_capture.h"
#include "video_render.h"
#endif

// Global counter to get an id for each new ViE instance
static WebRtc_Word32 gViEActiveInstanceCounter = 0;

namespace webrtc
{

// -------------------------------------------------------------------------
//	GetVideoEngine (C-function)
//
//	extern "C" ensures that GetProcAddress() can find the function address
// -------------------------------------------------------------------------

extern "C"
{
VideoEngine* GetVideoEngine();

VideoEngine* GetVideoEngine()
{
    VideoEngineImpl* self = new VideoEngineImpl();
    if (self == NULL)
    {
        return NULL;
    }
    gViEActiveInstanceCounter++;
    VideoEngine* vie = reinterpret_cast<VideoEngine*> (self);
    return vie;
}
}

// -------------------------------------------------------------------------
// Create
// -------------------------------------------------------------------------

VideoEngine* VideoEngine::Create()
{
#if (defined(_WIN32) || defined(_WIN64))
    // Load a debug dll, if there is one...
    HMODULE hmod_ = LoadLibrary(TEXT("VideoEngineTestingDLL.dll"));
    if (hmod_)
    {
        typedef VideoEngine* (*PFNGetVideoEngineLib)(void);
        PFNGetVideoEngineLib pfn =
            (PFNGetVideoEngineLib)GetProcAddress(hmod_,"GetVideoEngine");
        if (pfn)
        {
            VideoEngine* self = pfn();
            return self;
        }
        else
        {
            assert(!"Failed to open test dll VideoEngineTestingDLL.dll");
            return NULL;
        }
    }
#endif

    return GetVideoEngine();
}

// -------------------------------------------------------------------------
// Delete
//
// Deletes the VideoEngineImpl instance if all reference counters are 
// down to zero.
// -------------------------------------------------------------------------

bool VideoEngine::Delete(VideoEngine*& videoEngine)
{
    if (videoEngine == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "VideoEngine::Delete - No argument");
        return false;
    }
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, gViEActiveInstanceCounter,
               "VideoEngine::Delete( vie = 0x%p)", videoEngine);

    VideoEngineImpl* vieImpl = reinterpret_cast<VideoEngineImpl*> (videoEngine);

    // Check all reference counters
    ViEBaseImpl* vieBase = vieImpl;
    if (vieBase->GetCount() > 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "ViEBase ref count: %d", vieBase->GetCount());
        return false;
    }
#ifdef WEBRTC_VIDEO_ENGINE_CAPTURE_API
    ViECaptureImpl* vieCapture = vieImpl;
    if (vieCapture->GetCount() > 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "ViECapture ref count: %d", vieCapture->GetCount());
        return false;
    }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_CODEC_API
    ViECodecImpl* vieCodec = vieImpl;
    if (vieCodec->GetCount() > 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "ViECodec ref count: %d", vieCodec->GetCount());
        return false;
    }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_ENCRYPTION_API
    ViEEncryptionImpl* vieEncryption = vieImpl;
    if (vieEncryption->GetCount() > 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "ViEEncryption ref count: %d", vieEncryption->GetCount());
        return false;
    }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
    ViEExternalCodecImpl* vieExternalCodec = vieImpl;
    if (vieExternalCodec->GetCount() > 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "ViEEncryption ref count: %d", vieEncryption->GetCount());
        return false;
    }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
    ViEFileImpl* vieFile = vieImpl;
    if (vieFile->GetCount() > 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "ViEFile ref count: %d", vieFile->GetCount());
        return false;
    }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_IMAGE_PROCESS_API
    ViEImageProcessImpl* vieImageProcess = vieImpl;
    if (vieImageProcess->GetCount() > 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "ViEImageProcess ref count: %d", vieImageProcess->GetCount());
        return false;
    }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_NETWORK_API
    ViENetworkImpl* vieNetwork = vieImpl;
    if (vieNetwork->GetCount() > 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "ViENetwork ref count: %d", vieNetwork->GetCount());
        return false;
    }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RENDER_API
    ViERenderImpl* vieRender = vieImpl;
    if (vieRender->GetCount() > 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "ViERender ref count: %d", vieRender->GetCount());
        return false;
    }
#endif
#ifdef WEBRTC_VIDEO_ENGINE_RTP_RTCP_API
    ViERTP_RTCPImpl* vieRtpRtcp = vieImpl;
    if (vieRtpRtcp->GetCount() > 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "ViERTP_RTCP ref count: %d", vieRtpRtcp->GetCount());
        return false;
    }
#endif

    // Delete VieImpl
    delete vieImpl;
    vieImpl = NULL;
    videoEngine = NULL;

    // Decrease the number of instances
    gViEActiveInstanceCounter--;

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, gViEActiveInstanceCounter,
               "%s: instance deleted. Remaining instances: %d", __FUNCTION__,
               gViEActiveInstanceCounter);

    return true;
}

// -------------------------------------------------------------------------
// [static] SetTraceFile
// -------------------------------------------------------------------------

int VideoEngine::SetTraceFile(const char* fileNameUTF8,
                              const bool addFileCounter)
{
    if (fileNameUTF8 == NULL)
    {
        return -1;
    }
    if (Trace::SetTraceFile(fileNameUTF8, addFileCounter) == -1)
    {
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, gViEActiveInstanceCounter,
               "SetTraceFileName(fileNameUTF8 = %s, addFileCounter = %d",
               fileNameUTF8, addFileCounter);
    return 0;
}

// -------------------------------------------------------------------------
// [static] SetTraceFilter
// -------------------------------------------------------------------------

int VideoEngine::SetTraceFilter(const unsigned int filter)
{
    WebRtc_UWord32 oldFilter = 0;
    Trace::LevelFilter(oldFilter);

    if (filter == webrtc::kTraceNone && oldFilter != webrtc::kTraceNone)
    {
        // Do the logging before turning it off
        WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "SetTraceFilter(filter = 0x%x)", filter);
    }

    WebRtc_Word32 error = Trace::SetLevelFilter(filter);

    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, gViEActiveInstanceCounter,
               "SetTraceFilter(filter = 0x%x)", filter);
    if (error != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "SetTraceFilter error: %d", error);
        return -1;
    }

    return 0;
}

// -------------------------------------------------------------------------
// [static] SetTraceFilter
// -------------------------------------------------------------------------

int VideoEngine::SetTraceCallback(webrtc::TraceCallback* callback)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, gViEActiveInstanceCounter,
               "SetTraceCallback(webrtc::TraceCallback = 0x%p)", callback);
    return Trace::SetTraceCallback(callback);
}

// -------------------------------------------------------------------------
// [static] SetAndroidObjects
// -------------------------------------------------------------------------

int VideoEngine::SetAndroidObjects(void* javaVM, void* javaContext)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, gViEActiveInstanceCounter,
               "SetAndroidObjects()");

#ifdef WEBRTC_ANDROID
    if (VideoCaptureModule::SetAndroidObjects(javaVM,javaContext) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "Could not set capture module Android objects");
        return -1;
    }
    if (VideoRender::SetAndroidObjects(javaVM) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, gViEActiveInstanceCounter,
                   "Could not set render module Android objects");
        return -1;
    }
    return 0;
#else
    return -1;
#endif
}
} // namespace webrtc
