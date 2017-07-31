/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodec.h"

#import "WebRTC/RTCVideoCodecH264.h"

#include "webrtc/common_video/include/video_frame.h"
#include "webrtc/media/base/codec.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"

NS_ASSUME_NONNULL_BEGIN

/* Interfaces for converting to/from internal C++ formats. */
@interface RTCEncodedImage ()

- (instancetype)initWithNativeEncodedImage:(webrtc::EncodedImage)encodedImage;
- (webrtc::EncodedImage)nativeEncodedImage;

@end

@interface RTCVideoEncoderSettings ()

- (instancetype)initWithNativeVideoCodec:(const webrtc::VideoCodec *__nullable)videoCodec;
- (std::unique_ptr<webrtc::VideoCodec>)createNativeVideoEncoderSettings;

@end

@interface RTCCodecSpecificInfoH264 ()

- (webrtc::CodecSpecificInfo)nativeCodecSpecificInfo;

@end

@interface RTCRtpFragmentationHeader ()

- (instancetype)initWithNativeFragmentationHeader:
        (const webrtc::RTPFragmentationHeader *__nullable)fragmentationHeader;
- (std::unique_ptr<webrtc::RTPFragmentationHeader>)createNativeFragmentationHeader;

@end

@interface RTCVideoCodecInfo ()

- (instancetype)initWithNativeVideoCodec:(cricket::VideoCodec)videoCodec;
- (cricket::VideoCodec)nativeVideoCodec;

@end

NS_ASSUME_NONNULL_END
