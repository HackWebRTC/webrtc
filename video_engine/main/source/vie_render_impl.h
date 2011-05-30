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
 * vie_render_impl.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RENDER_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RENDER_IMPL_H_

#include "vie_defines.h"

#include "typedefs.h"
#include "video_render_defines.h"
#include "vie_ref_count.h"
#include "vie_render.h"
#include "vie_shared_data.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
//	ViERenderImpl
// ----------------------------------------------------------------------------

class ViERenderImpl: public virtual ViESharedData,
                     public ViERender,
                     public ViERefCount
{
public:
    virtual int Release();

    // Registration of render module
    virtual int RegisterVideoRenderModule(VideoRender& renderModule);

    virtual int DeRegisterVideoRenderModule(
        VideoRender& renderModule);

    // Add/remove renderer
    virtual int AddRenderer(const int renderId, void* window,
                            const unsigned int zOrder, const float left,
                            const float top, const float right,
                            const float bottom);

    virtual int RemoveRenderer(const int renderId);

    // Start/stop
    virtual int StartRender(const int renderId);

    virtual int StopRender(const int renderId);

    virtual int ConfigureRender(int renderId, const unsigned int zOrder,
                                const float left, const float top,
                                const float right, const float bottom);

    virtual int MirrorRenderStream(const int renderId, const bool enable,
                                   const bool mirrorXAxis,
                                   const bool mirrorYAxis);

    // External render
    virtual int AddRenderer(const int renderId,
                            webrtc::RawVideoType videoInputFormat,
                            ExternalRenderer* renderer);

protected:
    ViERenderImpl();
    virtual ~ViERenderImpl();
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RENDER_IMPL_H_
