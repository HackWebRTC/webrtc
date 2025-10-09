//
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Piasy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
//


#import "CFDefaultPeerConnectionObserver.h"

@implementation CFDefaultPeerConnectionObserver

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
    didChangeSignalingState:(RTC_OBJC_TYPE(RTCSignalingState))stateChanged {
}

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
          didAddStream:(RTC_OBJC_TYPE(RTCMediaStream)*)stream {
}

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
       didRemoveStream:(RTC_OBJC_TYPE(RTCMediaStream)*)stream {
}

- (void)peerConnectionShouldNegotiate:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection {
}

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
    didChangeIceConnectionState:(RTC_OBJC_TYPE(RTCIceConnectionState))newState {
}

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
    didChangeIceGatheringState:(RTC_OBJC_TYPE(RTCIceGatheringState))newState {
}

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
    didGenerateIceCandidate:(RTC_OBJC_TYPE(RTCIceCandidate)*)candidate {
}

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
    didRemoveIceCandidates:(NSArray<RTC_OBJC_TYPE(RTCIceCandidate)*>*)candidates {
}

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
    didOpenDataChannel:(RTC_OBJC_TYPE(RTCDataChannel)*)dataChannel {
}

@end
