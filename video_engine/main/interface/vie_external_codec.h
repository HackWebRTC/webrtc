/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_EXTERNAL_CODEC_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_EXTERNAL_CODEC_H_

#include "common_types.h"

namespace webrtc
{
class VideoEngine;
class VideoEncoder;
class VideoDecoder;

// ----------------------------------------------------------------------------
//	ViEExternalCodec
// ----------------------------------------------------------------------------

class WEBRTC_DLLEXPORT ViEExternalCodec
{
public:
    static ViEExternalCodec* GetInterface(VideoEngine* videoEngine);

    virtual int Release() = 0;

    virtual int RegisterExternalSendCodec(const int videoChannel,
                                          const unsigned char plType,
                                          VideoEncoder* encoder) = 0;

    virtual int DeRegisterExternalSendCodec(const int videoChannel,
                                            const unsigned char plType) = 0;

    virtual int RegisterExternalReceiveCodec(const int videoChannel,
                                             const unsigned int plType,
                                             VideoDecoder* decoder,
                                             bool decoderRender = false,
                                             int renderDelay = 0) = 0;

    virtual int DeRegisterExternalReceiveCodec(const int videoChannel,
                                               const unsigned char plType) = 0;

protected:
    ViEExternalCodec() {};
    virtual ~ViEExternalCodec() {};
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_EXTERNAL_CODEC_H_
