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
 * vie_external_codec_impl.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_EXTERNAL_CODEC_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_EXTERNAL_CODEC_IMPL_H_

#include "vie_external_codec.h"
#include "vie_ref_count.h"
#include "vie_shared_data.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
//	ViEExternalCodec
// ----------------------------------------------------------------------------

class ViEExternalCodecImpl : public virtual ViESharedData,
                             public ViEExternalCodec,
                             public ViERefCount
{
public:

    virtual int Release();

    virtual int RegisterExternalSendCodec(const int videoChannel,
                                          const unsigned char plType,
                                          VideoEncoder* encoder);

    virtual int DeRegisterExternalSendCodec(const int videoChannel,
                                            const unsigned char plType);

    virtual int RegisterExternalReceiveCodec(const int videoChannel,
                                             const unsigned int plType,
                                             VideoDecoder* decoder,
                                             bool decoderRender = false,
                                             int renderDelay = 0);

    virtual int DeRegisterExternalReceiveCodec(const int videoChannel,
                                               const unsigned char plType);
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_EXTERNAL_CODEC_IMPL_H_
