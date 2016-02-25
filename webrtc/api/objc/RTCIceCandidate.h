/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN
// TODO(hjon): Update nullability types. See http://crbug/webrtc/5592

@interface RTCIceCandidate : NSObject

/**
 * If present, the identifier of the "media stream identification" for the media
 * component this candidate is associated with.
 */
@property(nonatomic, readonly, nullable) NSString *sdpMid;

/**
 * The index (starting at zero) of the media description this candidate is
 * associated with in the SDP.
 */
@property(nonatomic, readonly) NSInteger sdpMLineIndex;

/** The SDP string for this candidate. */
@property(nonatomic, readonly, nonnull) NSString *sdp;

- (nonnull instancetype)init NS_UNAVAILABLE;

/**
 * Initialize an RTCIceCandidate from SDP.
 */
- (nonnull instancetype)initWithSdp:(nonnull NSString *)sdp
                      sdpMLineIndex:(NSInteger)sdpMLineIndex
                             sdpMid:(nullable NSString *)sdpMid
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
