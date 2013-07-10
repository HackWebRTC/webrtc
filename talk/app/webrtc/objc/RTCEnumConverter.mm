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

#import "RTCEnumConverter.h"

#include "talk/app/webrtc/peerconnectioninterface.h"

@implementation RTCEnumConverter

+ (RTCICEConnectionState)convertIceConnectionStateToObjC:
    (webrtc::PeerConnectionInterface::IceConnectionState)nativeState {
  switch (nativeState) {
    case webrtc::PeerConnectionInterface::kIceConnectionNew:
      return RTCICEConnectionNew;
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
      return RTCICEConnectionChecking;
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
      return RTCICEConnectionConnected;
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
      return RTCICEConnectionCompleted;
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
      return RTCICEConnectionFailed;
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
      return RTCICEConnectionDisconnected;
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
      return RTCICEConnectionClosed;
  }
}

+ (RTCICEGatheringState)convertIceGatheringStateToObjC:
    (webrtc::PeerConnectionInterface::IceGatheringState)nativeState {
  switch (nativeState) {
    case webrtc::PeerConnectionInterface::kIceGatheringNew:
      return RTCICEGatheringNew;
    case webrtc::PeerConnectionInterface::kIceGatheringGathering:
      return RTCICEGatheringGathering;
    case webrtc::PeerConnectionInterface::kIceGatheringComplete:
      return RTCICEGatheringComplete;
  }
}

+ (RTCSignalingState)convertSignalingStateToObjC:
    (webrtc::PeerConnectionInterface::SignalingState)nativeState {
  switch (nativeState) {
    case webrtc::PeerConnectionInterface::kStable:
      return RTCSignalingStable;
    case webrtc::PeerConnectionInterface::kHaveLocalOffer:
      return RTCSignalingHaveLocalOffer;
    case webrtc::PeerConnectionInterface::kHaveLocalPrAnswer:
      return RTCSignalingHaveLocalPrAnswer;
    case webrtc::PeerConnectionInterface::kHaveRemoteOffer:
      return RTCSignalingHaveRemoteOffer;
    case webrtc::PeerConnectionInterface::kHaveRemotePrAnswer:
      return RTCSignalingHaveRemotePrAnswer;
    case webrtc::PeerConnectionInterface::kClosed:
      return RTCSignalingClosed;
  }
}

+ (RTCSourceState)convertSourceStateToObjC:
    (webrtc::MediaSourceInterface::SourceState)nativeState {
  switch (nativeState) {
    case webrtc::MediaSourceInterface::kInitializing:
      return RTCSourceStateInitializing;
    case webrtc::MediaSourceInterface::kLive:
      return RTCSourceStateLive;
    case webrtc::MediaSourceInterface::kEnded:
      return RTCSourceStateEnded;
    case webrtc::MediaSourceInterface::kMuted:
      return RTCSourceStateMuted;
  }
}

+ (webrtc::MediaStreamTrackInterface::TrackState)
    convertTrackStateToNative:(RTCTrackState)state {
  switch (state) {
    case RTCTrackStateInitializing:
      return webrtc::MediaStreamTrackInterface::kInitializing;
    case RTCTrackStateLive:
      return webrtc::MediaStreamTrackInterface::kLive;
    case RTCTrackStateEnded:
      return webrtc::MediaStreamTrackInterface::kEnded;
    case RTCTrackStateFailed:
      return webrtc::MediaStreamTrackInterface::kFailed;
  }
}

+ (RTCTrackState)convertTrackStateToObjC:
    (webrtc::MediaStreamTrackInterface::TrackState)nativeState {
  switch (nativeState) {
    case webrtc::MediaStreamTrackInterface::kInitializing:
      return RTCTrackStateInitializing;
    case webrtc::MediaStreamTrackInterface::kLive:
      return RTCTrackStateLive;
    case webrtc::MediaStreamTrackInterface::kEnded:
      return RTCTrackStateEnded;
    case webrtc::MediaStreamTrackInterface::kFailed:
      return RTCTrackStateFailed;
  }
}

@end
