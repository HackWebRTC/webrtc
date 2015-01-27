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
#import <OCMock/OCMock.h>

#import "ARDAppClient+Internal.h"
#import "ARDJoinResponse+Internal.h"
#import "ARDMessageResponse+Internal.h"
#import "RTCMediaConstraints.h"
#import "RTCPeerConnectionFactory.h"

#include "webrtc/base/gunit.h"
#include "webrtc/base/ssladapter.h"

// These classes mimic XCTest APIs, to make eventual conversion to XCTest
// easier. Conversion will happen once XCTest is supported well on build bots.
@interface ARDTestExpectation : NSObject

@property(nonatomic, readonly) NSString *description;
@property(nonatomic, readonly) BOOL isFulfilled;

- (instancetype)initWithDescription:(NSString *)description;
- (void)fulfill;

@end

@implementation ARDTestExpectation

@synthesize description = _description;
@synthesize isFulfilled = _isFulfilled;

- (instancetype)initWithDescription:(NSString *)description {
  if (self = [super init]) {
    _description = description;
  }
  return self;
}

- (void)fulfill {
  _isFulfilled = YES;
}

@end

@interface ARDTestCase : NSObject

- (ARDTestExpectation *)expectationWithDescription:(NSString *)description;
- (void)waitForExpectationsWithTimeout:(NSTimeInterval)timeout
                               handler:(void (^)(NSError *error))handler;

@end

@implementation ARDTestCase {
  NSMutableArray *_expectations;
}

- (instancetype)init {
  if (self = [super init]) {
   _expectations = [NSMutableArray array];
  }
  return self;
}

- (ARDTestExpectation *)expectationWithDescription:(NSString *)description {
  ARDTestExpectation *expectation =
      [[ARDTestExpectation alloc] initWithDescription:description];
  [_expectations addObject:expectation];
  return expectation;
}

- (void)waitForExpectationsWithTimeout:(NSTimeInterval)timeout
                               handler:(void (^)(NSError *error))handler {
  NSDate *startDate = [NSDate date];
  while (![self areExpectationsFulfilled]) {
    NSTimeInterval duration = [[NSDate date] timeIntervalSinceDate:startDate];
    if (duration > timeout) {
      NSAssert(NO, @"Expectation timed out.");
      break;
    }
    [[NSRunLoop currentRunLoop]
        runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1]];
  }
  handler(nil);
}

- (BOOL)areExpectationsFulfilled {
  for (ARDTestExpectation *expectation in _expectations) {
    if (!expectation.isFulfilled) {
      return NO;
    }
  }
  return YES;
}

@end

@interface ARDAppClientTest : ARDTestCase
@end

@implementation ARDAppClientTest

#pragma mark - Mock helpers

- (id)mockRoomServerClientForRoomId:(NSString *)roomId
                           clientId:(NSString *)clientId
                        isInitiator:(BOOL)isInitiator
                           messages:(NSArray *)messages
                     messageHandler:
    (void (^)(ARDSignalingMessage *))messageHandler {
  id mockRoomServerClient =
      [OCMockObject mockForProtocol:@protocol(ARDRoomServerClient)];

  // Successful join response.
  ARDJoinResponse *joinResponse = [[ARDJoinResponse alloc] init];
  joinResponse.result = kARDJoinResultTypeSuccess;
  joinResponse.roomId = roomId;
  joinResponse.clientId = clientId;
  joinResponse.isInitiator = isInitiator;
  joinResponse.messages = messages;

  // Successful message response.
  ARDMessageResponse *messageResponse = [[ARDMessageResponse alloc] init];
  messageResponse.result = kARDMessageResultTypeSuccess;

  // Return join response from above on join.
  [[[mockRoomServerClient stub] andDo:^(NSInvocation *invocation) {
    __unsafe_unretained void (^completionHandler)(ARDJoinResponse *response,
                                                  NSError *error);
    [invocation getArgument:&completionHandler atIndex:3];
    completionHandler(joinResponse, nil);
  }] joinRoomWithRoomId:roomId completionHandler:[OCMArg any]];

  // Return message response from above on join.
  [[[mockRoomServerClient stub] andDo:^(NSInvocation *invocation) {
    __unsafe_unretained ARDSignalingMessage *message;
    __unsafe_unretained void (^completionHandler)(ARDMessageResponse *response,
                                                  NSError *error);
    [invocation getArgument:&message atIndex:2];
    [invocation getArgument:&completionHandler atIndex:5];
    messageHandler(message);
    completionHandler(messageResponse, nil);
  }] sendMessage:[OCMArg any]
            forRoomId:roomId
             clientId:clientId
    completionHandler:[OCMArg any]];

  // Do nothing on leave.
  [[[mockRoomServerClient stub] andDo:^(NSInvocation *invocation) {
    __unsafe_unretained void (^completionHandler)(NSError *error);
    [invocation getArgument:&completionHandler atIndex:4];
    if (completionHandler) {
      completionHandler(nil);
    }
  }] leaveRoomWithRoomId:roomId
                clientId:clientId
       completionHandler:[OCMArg any]];

  return mockRoomServerClient;
}

- (id)mockSignalingChannelForRoomId:(NSString *)roomId
                           clientId:(NSString *)clientId
                     messageHandler:
    (void (^)(ARDSignalingMessage *message))messageHandler {
  id mockSignalingChannel =
      [OCMockObject niceMockForProtocol:@protocol(ARDSignalingChannel)];
  [[mockSignalingChannel stub] registerForRoomId:roomId clientId:clientId];
  [[[mockSignalingChannel stub] andDo:^(NSInvocation *invocation) {
    __unsafe_unretained ARDSignalingMessage *message;
    [invocation getArgument:&message atIndex:2];
    messageHandler(message);
  }] sendMessage:[OCMArg any]];
  return mockSignalingChannel;
}

- (id)mockTURNClient {
  id mockTURNClient =
      [OCMockObject mockForProtocol:@protocol(ARDTURNClient)];
  [[[mockTURNClient stub] andDo:^(NSInvocation *invocation) {
    // Don't return anything in TURN response.
    __unsafe_unretained void (^completionHandler)(NSArray *turnServers,
                                                  NSError *error);
    [invocation getArgument:&completionHandler atIndex:2];
    completionHandler([NSArray array], nil);
  }] requestServersWithCompletionHandler:[OCMArg any]];
  return mockTURNClient;
}

- (ARDAppClient *)createAppClientForRoomId:(NSString *)roomId
                                  clientId:(NSString *)clientId
                               isInitiator:(BOOL)isInitiator
                                  messages:(NSArray *)messages
                            messageHandler:
    (void (^)(ARDSignalingMessage *message))messageHandler
                          connectedHandler:(void (^)(void))connectedHandler {
  id turnClient = [self mockTURNClient];
  id signalingChannel = [self mockSignalingChannelForRoomId:roomId
                                                   clientId:clientId
                                             messageHandler:messageHandler];
  id roomServerClient =
      [self mockRoomServerClientForRoomId:roomId
                                 clientId:clientId
                              isInitiator:isInitiator
                                 messages:messages
                           messageHandler:messageHandler];
  id delegate =
      [OCMockObject niceMockForProtocol:@protocol(ARDAppClientDelegate)];
  [[[delegate stub] andDo:^(NSInvocation *invocation) {
    connectedHandler();
  }] appClient:[OCMArg any] didChangeConnectionState:RTCICEConnectionConnected];

  return [[ARDAppClient alloc] initWithRoomServerClient:roomServerClient
                                       signalingChannel:signalingChannel
                                             turnClient:turnClient
                                               delegate:delegate];
}

// Tests that an ICE connection is established between two ARDAppClient objects
// where one is set up as a caller and the other the answerer. Network
// components are mocked out and messages are relayed directly from object to
// object. It's expected that both clients reach the RTCICEConnectionConnected
// state within a reasonable amount of time.
- (void)testSession {
  // Need block arguments here because we're setting up a callbacks before we
  // create the clients.
  ARDAppClient *caller = nil;
  ARDAppClient *answerer = nil;
  __block __weak ARDAppClient *weakCaller = nil;
  __block __weak ARDAppClient *weakAnswerer = nil;
  NSString *roomId = @"testRoom";
  NSString *callerId = @"testCallerId";
  NSString *answererId = @"testAnswererId";

  ARDTestExpectation *callerConnectionExpectation =
      [self expectationWithDescription:@"Caller PC connected."];
  ARDTestExpectation *answererConnectionExpectation =
      [self expectationWithDescription:@"Answerer PC connected."];

  caller = [self createAppClientForRoomId:roomId
                                 clientId:callerId
                              isInitiator:YES
                                 messages:[NSArray array]
                           messageHandler:^(ARDSignalingMessage *message) {
    ARDAppClient *strongAnswerer = weakAnswerer;
    [strongAnswerer channel:strongAnswerer.channel didReceiveMessage:message];
  } connectedHandler:^{
    [callerConnectionExpectation fulfill];
  }];
  // TODO(tkchin): Figure out why DTLS-SRTP constraint causes thread assertion
  // crash in Debug.
  caller.defaultPeerConnectionConstraints = [[RTCMediaConstraints alloc] init];
  weakCaller = caller;

  answerer = [self createAppClientForRoomId:roomId
                                   clientId:answererId
                                isInitiator:NO
                                   messages:[NSArray array]
                             messageHandler:^(ARDSignalingMessage *message) {
    ARDAppClient *strongCaller = weakCaller;
    [strongCaller channel:strongCaller.channel didReceiveMessage:message];
  } connectedHandler:^{
    [answererConnectionExpectation fulfill];
  }];
  // TODO(tkchin): Figure out why DTLS-SRTP constraint causes thread assertion
  // crash in Debug.
  answerer.defaultPeerConnectionConstraints =
      [[RTCMediaConstraints alloc] init];
  weakAnswerer = answerer;

  // Kick off connection.
  [caller connectToRoomWithId:roomId options:nil];
  [answerer connectToRoomWithId:roomId options:nil];
  [self waitForExpectationsWithTimeout:20 handler:^(NSError *error) {
    if (error) {
      NSLog(@"Expectations error: %@", error);
    }
  }];
}

@end

class SignalingTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    rtc::InitializeSSL();
  }
  static void TearDownTestCase() {
    rtc::CleanupSSL();
  }
};

TEST_F(SignalingTest, SessionTest) {
  @autoreleasepool {
    ARDAppClientTest *test = [[ARDAppClientTest alloc] init];
    [test testSession];
  }
}
