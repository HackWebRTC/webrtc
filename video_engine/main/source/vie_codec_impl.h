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
 * vie_codec_impl.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CODEC_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CODEC_IMPL_H_

#include "vie_defines.h"

#include "typedefs.h"
#include "vie_ref_count.h"
#include "vie_shared_data.h"
#include "vie_codec.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
//	ViECodecImpl
// ----------------------------------------------------------------------------

class ViECodecImpl : public virtual ViESharedData,
                         public ViECodec,
                         public ViERefCount
{
public:
    virtual int Release();

    // Available codecs
    virtual int NumberOfCodecs() const;

    virtual int GetCodec(const unsigned char listNumber,
                         VideoCodec& videoCodec) const;

    // Codec settings
    virtual int SetSendCodec(const int videoChannel,
                             const VideoCodec& videoCodec);

    virtual int GetSendCodec(const int videoChannel,
                             VideoCodec& videoCodec) const;

    virtual int SetReceiveCodec(const int videoChannel,
                                const VideoCodec& videoCodec);

    virtual int GetReceiveCodec(const int videoChannel,
                                VideoCodec& videoCodec) const;

    virtual int GetCodecConfigParameters(
        const int videoChannel,
        unsigned char configParameters[kConfigParameterSize],
        unsigned char& configParametersSize) const;

    // Input image scaling
    virtual int SetImageScaleStatus(const int videoChannel, const bool enable);

    // Codec statistics
    virtual int GetSendCodecStastistics(const int videoChannel,
                                        unsigned int& keyFrames,
                                        unsigned int& deltaFrames) const;

    virtual int GetReceiveCodecStastistics(const int videoChannel,
                                           unsigned int& keyFrames,
                                           unsigned int& deltaFrames) const;

    // Callbacks
    virtual int SetKeyFrameRequestCallbackStatus(const int videoChannel,
                                                 const bool enable);

    virtual int SetSignalKeyPacketLossStatus(const int videoChannel,
                                             const bool enable,
                                             const bool onlyKeyFrames = false);

    virtual int RegisterEncoderObserver(const int videoChannel,
                                        ViEEncoderObserver& observer);

    virtual int DeregisterEncoderObserver(const int videoChannel);

    virtual int RegisterDecoderObserver(const int videoChannel,
                                        ViEDecoderObserver& observer);

    virtual int DeregisterDecoderObserver(const int videoChannel);

    // Key frame settings
    virtual int SendKeyFrame(const int videoChannel);

    virtual int WaitForFirstKeyFrame(const int videoChannel, const bool wait);

    // H263 Specific
    virtual int SetInverseH263Logic(int videoChannel, bool enable);

protected:
    ViECodecImpl();
    virtual ~ViECodecImpl();

private:
    bool CodecValid(const VideoCodec& videoCodec);
};
} // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_CODEC_IMPL_H_
