/*
 * libjingle
 * Copyright 2013, Google Inc.
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

#import "GAEChannelClient.h"

// Called when there are RTCICEServers.
@protocol ICEServerDelegate<NSObject>

- (void)onICEServers:(NSArray*)servers;

@end

// Negotiates signaling for chatting with apprtc.appspot.com "rooms".
// Uses the client<->server specifics of the apprtc AppEngine webapp.
//
// To use: create an instance of this object (registering a message handler) and
// call connectToRoom().  apprtc.appspot.com will signal that is successful via
// onOpen through the browser channel.  Then you should call sendData() and wait
// for the registered handler to be called with received messages.
@interface APPRTCAppClient : NSObject<NSURLConnectionDataDelegate>

@property(nonatomic, assign) id<ICEServerDelegate> ICEServerDelegate;
@property(nonatomic, assign) id<GAEMessageHandler> messageHandler;

- (void)connectToRoom:(NSURL *)room;
- (void)sendData:(NSData *)data;

@end
