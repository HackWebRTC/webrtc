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

@interface RTCMediaConstraints : NSObject

- (nonnull instancetype)init NS_UNAVAILABLE;

/** Initialize with mandatory and/or optional constraints. */
- (nonnull instancetype)initWithMandatoryConstraints:
    (nullable NSDictionary *)mandatory
                                 optionalConstraints:
    (nullable NSDictionary *)optional
    NS_DESIGNATED_INITIALIZER;
// - (instancetype)initWithMandatoryConstraints:
//     (nullable NSDictionary<NSString *, NSString *> *)mandatory
//                          optionalConstraints:
//     (nullable NSDictionary<NSString *, NSString *> *)optional
//     NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
