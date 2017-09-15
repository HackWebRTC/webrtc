/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCPeerConnectionFactory.h"

#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

class AudioEncoderFactory;
class AudioDecoderFactory;

}  // namespace webrtc

namespace cricket {

class WebRtcVideoEncoderFactory;
class WebRtcVideoDecoderFactory;

}  // namespace cricket

NS_ASSUME_NONNULL_BEGIN

/**
 * This class extension exposes methods that work directly with injectable C++ components.
 */
@interface RTCPeerConnectionFactory ()

/* Initialize object with injectable native audio/video encoder/decoder factories */
- (instancetype)initWithNativeAudioEncoderFactory:
                    (rtc::scoped_refptr<webrtc::AudioEncoderFactory>)audioEncoderFactory
                        nativeAudioDecoderFactory:
                            (rtc::scoped_refptr<webrtc::AudioDecoderFactory>)audioDecoderFactory
                        nativeVideoEncoderFactory:
                            (nullable cricket::WebRtcVideoEncoderFactory *)videoEncoderFactory
                        nativeVideoDecoderFactory:
                            (nullable cricket::WebRtcVideoDecoderFactory *)videoDecoderFactory
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
