/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

extern const NSString * const kRtxCodecMimeType;
extern const NSString * const kRedCodecMimeType;
extern const NSString * const kUlpfecCodecMimeType;
extern const NSString * const kOpusCodecMimeType;
extern const NSString * const kIsacCodecMimeType;
extern const NSString * const kL16CodecMimeType;
extern const NSString * const kG722CodecMimeType;
extern const NSString * const kIlbcCodecMimeType;
extern const NSString * const kPcmuCodecMimeType;
extern const NSString * const kPcmaCodecMimeType;
extern const NSString * const kDtmfCodecMimeType;
extern const NSString * const kComfortNoiseCodecMimeType;
extern const NSString * const kVp8CodecMimeType;
extern const NSString * const kVp9CodecMimeType;
extern const NSString * const kH264CodecMimeType;

/** Defined in http://w3c.github.io/webrtc-pc/#idl-def-RTCRtpCodecParameters */
@interface RTCRtpCodecParameters : NSObject

/** The RTP payload type. */
@property(nonatomic, assign) int payloadType;

/**
 * The codec MIME type. Valid types are listed in:
 * http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml#rtp-parameters-2
 *
 * Several supported types are represented by the constants above.
 */
@property(nonatomic, nonnull) NSString *mimeType;

/** The codec clock rate expressed in Hertz. */
@property(nonatomic, assign) int clockRate;

/** The number of channels (mono=1, stereo=2). */
@property(nonatomic, assign) int channels;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
