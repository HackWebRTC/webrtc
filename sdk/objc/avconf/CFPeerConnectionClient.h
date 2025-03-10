//
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Piasy
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


#import <Foundation/Foundation.h>

#import "RTCMacros.h"

@class RTCIceCandidate;
@class RTCIceServer;
@class RTCStatisticsReport;
@class RTCSessionDescription;
@class RTCVideoCapturer;
@class RTCVideoSource;
@class CFHijackCapturerDelegate;
@protocol RTCVideoRenderer;

@class CFPeerConnectionFactoryOption;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, CFPeerConnectionDir) {
    CF_DIR_SEND_RECV = 0,
    CF_DIR_SEND_ONLY = 1,
    CF_DIR_RECV_ONLY = 2,
    CF_DIR_INACTIVE = 3,
};

typedef NS_ENUM(NSInteger, CFPeerConnectionError) {
    ERR_NO_FACTORY = 1000,
    ERR_NO_SENDING_TRACK = 1001,
    ERR_CREATE_PC_FAIL = 1002,
    ERR_ICE_FAIL = 1003,
    ERR_CREATE_MULTIPLE_SDP = 1004,
    ERR_CREATE_SDP_FAIL = 1005,
    ERR_SET_SDP_FAIL = 1006,
};

RTC_OBJC_EXPORT
@protocol CFPeerConnectionClientDelegate<NSObject>

- (NSString*)onPreferCodecs:(NSString*)peerUid sdp:(NSString*)sdp;

- (void)onLocalDescription:(NSString*)peerUid
                  localSdp:(RTCSessionDescription*)localSdp;

- (void)onIceCandidate:(NSString*)peerUid candidate:(RTCIceCandidate*)candidate;

- (void)onIceCandidatesRemoved:(NSString*)peerUid
                    candidates:(NSArray<RTCIceCandidate*>*)candidates;

- (void)onPeerConnectionStatsReady:(NSString*)peerUid
                            report:(RTCStatisticsReport*)report;

- (void)onIceConnected:(NSString*)peerUid;

- (void)onIceDisconnected:(NSString*)peerUid;

- (void)onError:(NSString*)peerUid code:(CFPeerConnectionError)code;
@end

RTC_OBJC_EXPORT
@interface CFPeerConnectionClient : NSObject

+ (NSString*)versionName;

+ (int32_t)initialize:(NSDictionary*)fieldTrails;

+ (bool)send:(CFPeerConnectionDir)dir;
+ (bool)receive:(CFPeerConnectionDir)dir;
+ (int32_t)createPeerConnectionFactory:
    (CFPeerConnectionFactoryOption*)factoryOption;

+ (int32_t)createLocalTracks:(bool)hasVideo isScreencast:(bool)isScreencast;
+ (CFHijackCapturerDelegate*)getHijackCapturerDelegate;

+ (void)adaptVideoOutputFormat:(int)width height:(int)height fps:(int)fps;

+ (void)addLocalTrackRenderer:(id<RTCVideoRenderer>)localTrackRenderer;
+ (void)removeLocalTrackRenderer:(id<RTCVideoRenderer>)localTrackRenderer;

+ (int32_t)destroyPeerConnectionFactory;


- (instancetype)initWithUid:(NSString*)uid
                        dir:(CFPeerConnectionDir)dir
                   hasVideo:(bool)hasVideo
                   delegate:(id<CFPeerConnectionClientDelegate>)delegate
        videoMaxBitrateKbps:(int32_t)videoMaxBitrateKbps
          videoMaxFrameRate:(int32_t)videoMaxFrameRate;

- (void)createPeerConnection:(NSArray<RTCIceServer*>*)iceServers;

- (void)getStats;

- (void)setAudioSendingEnabled:(bool)enable;
- (void)setVideoSendingEnabled:(bool)enable;
- (void)setAudioReceivingEnabled:(bool)enable;
- (void)setVideoReceivingEnabled:(bool)enable;

- (void)createOffer;
- (void)createAnswer;

- (void)addIceCandidate:(RTCIceCandidate*)candidate;
- (void)removeIceCandidates:(NSArray<RTCIceCandidate*>*)candidates;

- (void)setRemoteDescription:(RTCSessionDescription*)sdp;

- (bool)send;
- (bool)receive;

- (int)startRecorder:(int32_t)dir path:(NSString*)path;
- (int)stopRecorder:(int32_t)dir;
- (void)requestFir;

- (void)close;

- (void)addRemoteTrackRenderer:(id<RTCVideoRenderer>)remoteTrackRenderer;
- (void)removeRemoteTrackRenderer:(id<RTCVideoRenderer>)remoteTrackRenderer;

- (void)setVideoMaxBitrateKbps:(int)videoMaxBitrateKbps;

@end

NS_ASSUME_NONNULL_END
