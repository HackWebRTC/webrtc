/*
 * libjingle
 * Copyright 2014 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>

#import "ARDSignalingMessage.h"

typedef NS_ENUM(NSInteger, ARDSignalingChannelState) {
  // State when disconnected.
  kARDSignalingChannelStateClosed,
  // State when connection is established but not ready for use.
  kARDSignalingChannelStateOpen,
  // State when connection is established and registered.
  kARDSignalingChannelStateRegistered,
  // State when connection encounters a fatal error.
  kARDSignalingChannelStateError
};

@protocol ARDSignalingChannel;
@protocol ARDSignalingChannelDelegate <NSObject>

- (void)channel:(id<ARDSignalingChannel>)channel
    didChangeState:(ARDSignalingChannelState)state;

- (void)channel:(id<ARDSignalingChannel>)channel
    didReceiveMessage:(ARDSignalingMessage *)message;

@end

@protocol ARDSignalingChannel <NSObject>

@property(nonatomic, readonly) NSString *roomId;
@property(nonatomic, readonly) NSString *clientId;
@property(nonatomic, readonly) ARDSignalingChannelState state;
@property(nonatomic, weak) id<ARDSignalingChannelDelegate> delegate;

// Registers the channel for the given room and client id.
- (void)registerForRoomId:(NSString *)roomId
                 clientId:(NSString *)clientId;

// Sends signaling message over the channel.
- (void)sendMessage:(ARDSignalingMessage *)message;

@end

