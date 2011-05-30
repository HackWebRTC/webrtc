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
 * vie_image_process_impl.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_IMAGE_PROCESS_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_IMAGE_PROCESS_IMPL_H_

#include "typedefs.h"
#include "vie_ref_count.h"
#include "vie_image_process.h"
#include "vie_shared_data.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
//	ViEImageProcessImpl
// ----------------------------------------------------------------------------

class ViEImageProcessImpl: public virtual ViESharedData,
                           public ViEImageProcess,
                           public ViERefCount
{
public:
    virtual int Release();

    // Effect filter
    virtual int RegisterCaptureEffectFilter(const int captureId,
                                            ViEEffectFilter& captureFilter);

    virtual int DeregisterCaptureEffectFilter(const int captureId);

    virtual int RegisterSendEffectFilter(const int videoChannel,
                                         ViEEffectFilter& sendFilter);

    virtual int DeregisterSendEffectFilter(const int videoChannel);

    virtual int RegisterRenderEffectFilter(const int videoChannel,
                                           ViEEffectFilter& renderFilter);

    virtual int DeregisterRenderEffectFilter(const int videoChannel);

    // Image enhancement
    virtual int EnableDeflickering(const int captureId, const bool enable);

    virtual int EnableDenoising(const int captureId, const bool enable);

    virtual int EnableColorEnhancement(const int videoChannel,
                                       const bool enable);

protected:
    ViEImageProcessImpl();
    virtual ~ViEImageProcessImpl();
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_IMAGE_PROCESS_IMPL_H_
