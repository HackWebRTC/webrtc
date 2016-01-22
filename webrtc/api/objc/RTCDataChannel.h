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

@interface RTCDataBuffer : NSObject

/** NSData representation of the underlying buffer. */
@property(nonatomic, readonly) NSData *data;

/** Indicates whether |data| contains UTF-8 or binary data. */
@property(nonatomic, readonly) BOOL isBinary;

- (instancetype)init NS_UNAVAILABLE;

/**
 * Initialize an RTCDataBuffer from NSData. |isBinary| indicates whether |data|
 * contains UTF-8 or binary data.
 */
- (instancetype)initWithData:(NSData *)data isBinary:(BOOL)isBinary;

@end


@class RTCDataChannel;

@protocol RTCDataChannelDelegate <NSObject>

/** The data channel state changed. */
- (void)dataChannelDidChangeState:(RTCDataChannel *)dataChannel;

/** The data channel successfully received a data buffer. */
- (void)dataChannel:(RTCDataChannel *)dataChannel
    didReceiveMessageWithBuffer:(RTCDataBuffer *)buffer;

@optional

/** The data channel's |bufferedAmount| changed. */
- (void)dataChannel:(RTCDataChannel *)dataChannel
    didChangeBufferedAmount:(NSUInteger)amount;

@end


/** Represents the state of the data channel. */
typedef NS_ENUM(NSInteger, RTCDataChannelState) {
    RTCDataChannelStateConnecting,
    RTCDataChannelStateOpen,
    RTCDataChannelStateClosing,
    RTCDataChannelStateClosed,
};

@interface RTCDataChannel : NSObject

/**
 * A label that can be used to distinguish this data channel from other data
 * channel objects.
 */
@property(nonatomic, readonly) NSString *label;

/** Returns whether this data channel is ordered or not. */
@property(nonatomic, readonly) BOOL isOrdered;

/**
 * The length of the time window (in milliseconds) during which transmissions
 * and retransmissions may occur in unreliable mode.
 */
@property(nonatomic, readonly) uint16_t maxPacketLifeTime;

/**
 * The maximum number of retransmissions that are attempted in unreliable mode.
 */
@property(nonatomic, readonly) uint16_t maxRetransmits;

/**
 * The name of the sub-protocol used with this data channel, if any. Otherwise
 * this returns an empty string.
 */
@property(nonatomic, readonly) NSString *protocol;

/**
 * Returns whether this data channel was negotiated by the application or not.
 */
@property(nonatomic, readonly) BOOL isNegotiated;

/** The identifier for this data channel. */
@property(nonatomic, readonly) int id;

/** The state of the data channel. */
@property(nonatomic, readonly) RTCDataChannelState readyState;

/**
 * The number of bytes of application data that have been queued using
 * |sendData:| but that have not yet been transmitted to the network.
 */
@property(nonatomic, readonly) uint64_t bufferedAmount;

/** The delegate for this data channel. */
@property(nonatomic, weak) id<RTCDataChannelDelegate> delegate;

- (instancetype)init NS_UNAVAILABLE;

/** Closes the data channel. */
- (void)close;

/** Attempt to send |data| on this data channel's underlying data transport. */
- (BOOL)sendData:(RTCDataBuffer *)data;

@end

NS_ASSUME_NONNULL_END
