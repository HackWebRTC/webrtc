/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//  - Specify render destinations for incoming video streams, capture devices
//    and files.
//  - Configuring render streams.


#ifndef WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_RENDER_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_RENDER_H_

#include "common_types.h"

namespace webrtc
{

class VideoRender;
class VideoEngine;

// ----------------------------------------------------------------------------
//	ExternalRenderer
// ----------------------------------------------------------------------------

// This class declares an abstract interface to be used for external renderers.
// The user implemented derived class is registered using AddRenderer().
class WEBRTC_DLLEXPORT ExternalRenderer
{
public:
    // This method will be called when the stream to be rendered changes in
    // resolution or number of streams mixed in the image.
    virtual int FrameSizeChange(unsigned int width, unsigned int height,
                                unsigned int numberOfStreams) = 0;

    // This method is called when a new frame should be rendered.
    virtual int DeliverFrame(unsigned char* buffer, int bufferSize,
                             unsigned int time_stamp) = 0;

protected:
    virtual ~ExternalRenderer() {}
};

// ----------------------------------------------------------------------------
//	ViERender
// ----------------------------------------------------------------------------

class WEBRTC_DLLEXPORT ViERender
{
public:
    // Factory for the ViERender sub‐API and increases an internal reference
    // counter if successful. Returns NULL if the API is not supported or if
    // construction fails.
    static ViERender* GetInterface(VideoEngine* videoEngine);

    // Releases the ViERender sub-API and decreases an internal reference
    // counter. Returns the new reference count. This value should be zero
    // for all sub-API:s before the VideoEngine object can be safely deleted.
    virtual int Release() = 0;

    // Registers render module
    virtual int RegisterVideoRenderModule(VideoRender& renderModule) = 0;

    // Deegisters render module
    virtual int DeRegisterVideoRenderModule(VideoRender& renderModule) = 0;

    // Sets the render destination for a given render ID.
    virtual int AddRenderer(const int renderId, void* window,
                            const unsigned int zOrder, const float left,
                            const float top, const float right,
                            const float bottom) = 0;

    // Removes the renderer for a stream
    virtual int RemoveRenderer(const int renderId) = 0;

    // Starts rendering a render stream.
    virtual int StartRender(const int renderId) = 0;

    // Stops rendering a render stream.
    virtual int StopRender(const int renderId) = 0;

    // Configures an already added render stream.
    virtual int ConfigureRender(int renderId, const unsigned int zOrder,
                                const float left, const float top,
                                const float right, const float bottom) = 0;

    // This function mirrors the rendered stream left and right or up and down.
    virtual int MirrorRenderStream(const int renderId, const bool enable,
                                   const bool mirrorXAxis,
                                   const bool mirrorYAxis) = 0;

    // External render    
    virtual int AddRenderer(const int renderId, RawVideoType videoInputFormat,
                            ExternalRenderer* renderer) = 0;

protected:
    ViERender() {};
    virtual ~ViERender() {};
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_RENDER_H_
