/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_EXTERNAL_MEDIA_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_EXTERNAL_MEDIA_IMPL_H

#include "voe_external_media.h"

#include "ref_count.h"
#include "shared_data.h"

namespace webrtc {

class VoEExternalMediaImpl : public virtual voe::SharedData,
                             public VoEExternalMedia,
                             public voe::RefCount
{
public:
    virtual int Release();

    virtual int RegisterExternalMediaProcessing(
        int channel,
        ProcessingTypes type,
        VoEMediaProcess& processObject);

    virtual int DeRegisterExternalMediaProcessing(
        int channel,
        ProcessingTypes type);

    virtual int SetExternalRecordingStatus(bool enable);

    virtual int SetExternalPlayoutStatus(bool enable);

    virtual int ExternalRecordingInsertData(
        const WebRtc_Word16 speechData10ms[],
        int lengthSamples,
        int samplingFreqHz,
        int current_delay_ms);

    virtual int ExternalPlayoutGetData(WebRtc_Word16 speechData10ms[],
                                       int samplingFreqHz,
                                       int current_delay_ms,
                                       int& lengthSamples);

protected:
    VoEExternalMediaImpl();
    virtual ~VoEExternalMediaImpl();

private:
    int playout_delay_ms_;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_EXTERNAL_MEDIA_IMPL_H
