/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SDK_ANDROID_SRC_JNI_MEDIA_JNI_H_
#define WEBRTC_SDK_ANDROID_SRC_JNI_MEDIA_JNI_H_

#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/thread.h"

namespace webrtc_jni {

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
CreateNativePeerConnectionFactory(
    rtc::Thread* network_thread,
    rtc::Thread* worker_thread,
    rtc::Thread* signaling_thread,
    webrtc::AudioDeviceModule* default_adm,
    rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory,
    rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory,
    cricket::WebRtcVideoEncoderFactory* video_encoder_factory,
    cricket::WebRtcVideoDecoderFactory* video_decoder_factory);

}  // namespace webrtc_jni

#endif  // WEBRTC_SDK_ANDROID_SRC_JNI_MEDIA_JNI_H_
