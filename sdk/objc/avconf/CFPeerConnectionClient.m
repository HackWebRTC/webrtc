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

#import "CFPeerConnectionClient.h"

#import <UIKit/UIKit.h>

#import "api/peerconnection/RTCConfiguration.h"
#import "api/peerconnection/RTCIceCandidate.h"
#import "api/peerconnection/RTCSessionDescription.h"
#import "api/peerconnection/RTCMediaConstraints.h"
#import "api/peerconnection/RTCPeerConnection.h"
#import "api/peerconnection/RTCPeerConnectionFactory.h"
#import "api/peerconnection/RTCPeerConnectionFactoryOptions.h"
#import "api/peerconnection/RTCRtpEncodingParameters.h"
#import "api/peerconnection/RTCRtpParameters.h"
#import "api/peerconnection/RTCRtpSender.h"
#import "api/peerconnection/RTCRtpTransceiver.h"
#import "api/peerconnection/RTCAudioSource.h"
#import "api/peerconnection/RTCVideoSource.h"
#import "api/peerconnection/RTCAudioTrack.h"
#import "api/peerconnection/RTCVideoTrack.h"

#import "api/peerconnection/RTCFieldTrials.h"
#import "api/peerconnection/RTCSSLAdapter.h"
#import "api/peerconnection/RTCTracing.h"

#import "base/RTCLogging.h"
#import "base/RTCVideoRenderer.h"

#import "components/video_codec/RTCDefaultVideoDecoderFactory.h"
#import "components/video_codec/RTCDefaultVideoEncoderFactory.h"

#import "CFTimerProxy.h"
#import "CFAudioMixer.h"
#import "CFHijackCapturerDelegate.h"
#import "CFPeerConnectionFactoryOption.h"

#define TAG "CFPeerConnectionClient"

static NSString* const kCFVideoTrackKind = @"video";

static NSString* const kCFAudioTrackId = @"CFAMSa0";
static NSString* const kCFVideoTrackId = @"CFAMSv0";

static int const kKbpsMultiplier = 1000;

static RTC_OBJC_TYPE(RTCPeerConnectionFactory)* gFactory = nil;

static RTC_OBJC_TYPE(RTCAudioSource)* gLocalAudioSource = nil;
static RTC_OBJC_TYPE(RTCAudioTrack)* gLocalAudioTrack = nil;
static RTC_OBJC_TYPE(RTCVideoSource)* gLocalVideoSource = nil;
static CFHijackCapturerDelegate* gHijackCapturerDelegate = nil;
static RTC_OBJC_TYPE(RTCVideoTrack)* gLocalVideoTrack = nil;

@interface CFPeerConnectionClient ()<RTC_OBJC_TYPE(RTCPeerConnectionDelegate)>
@end

@implementation CFPeerConnectionClient {
    NSString* _uid;
    CFPeerConnectionDir _dir;
    bool _hasVideo;

    id<CFPeerConnectionClientDelegate> _delegate;

    RTC_OBJC_TYPE(RTCPeerConnection)* _peerConnection;
    NSMutableArray<id<RTC_OBJC_TYPE(RTCVideoRenderer)>>* _remoteTrackRenderers;
    bool _remoteRendererAdded;

    bool _isInitiator;
    NSMutableArray* _queuedRemoteCandidates;

    RTC_OBJC_TYPE(RTCAudioTrack)* _remoteAudioTrack;
    RTC_OBJC_TYPE(RTCVideoTrack)* _remoteVideoTrack;

    int32_t _videoMaxBitrate;
    int32_t _videoMaxFrameRate;

    dispatch_queue_t _queue;
    RTC_OBJC_TYPE(RTCMediaConstraints)* _sdpConstraints;
}

+ (NSString*)versionName {
    return @"1.0.43659";
}

+ (int32_t)initialize:(NSDictionary*)fieldTrails {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        RTCLogInfo(TAG " initialize %@", fieldTrails);

        RTCInitFieldTrialDictionary(fieldTrails);
        RTCInitializeSSL();
        RTCSetupInternalTracer();

        RTCLogInfo(TAG " initialize success");
    });

    return 0;
}

+ (bool)send:(CFPeerConnectionDir)dir {
    return dir == CF_DIR_SEND_ONLY || dir == CF_DIR_SEND_RECV;
}

+ (bool)receive:(CFPeerConnectionDir)dir {
    return dir == CF_DIR_RECV_ONLY || dir == CF_DIR_SEND_RECV;
}

+ (int32_t)createPeerConnectionFactory:
    (CFPeerConnectionFactoryOption*)factoryOption {
    RTCLogInfo(TAG " createPeerConnectionFactory, ver %@, options %@",
               [CFPeerConnectionClient versionName], [factoryOption description]);

    if (gFactory) {
        RTCLogError(TAG " createPeerConnectionFactory error: already created");
        return -1;
    }

    RTC_OBJC_TYPE(RTCDefaultVideoDecoderFactory)* decoderFactory =
        [[RTC_OBJC_TYPE(RTCDefaultVideoDecoderFactory) alloc] init];
    RTC_OBJC_TYPE(RTCDefaultVideoEncoderFactory)* encoderFactory =
        [[RTC_OBJC_TYPE(RTCDefaultVideoEncoderFactory) alloc] init];
    encoderFactory.preferredCodec = factoryOption.preferredVideoCodec;
    gFactory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc]
        initWithEncoderFactory:encoderFactory
                decoderFactory:decoderFactory];

    RTC_OBJC_TYPE(RTCPeerConnectionFactoryOptions)* options =
        [[RTC_OBJC_TYPE(RTCPeerConnectionFactoryOptions) alloc] init];
    options.disableEncryption = factoryOption.disableEncryption;
    [gFactory setOptions:options];

    RTCLogInfo(TAG " PeerConnectionFactory created");

    return 0;
}

+ (int32_t)createLocalTracks:(bool)hasVideo isScreencast:(bool)isScreencast {
    RTCLogInfo(TAG " createLocalTracks");

    if (!gFactory) {
        RTCLogError(TAG " createLocalTracks error: no factory");
        return -1;
    }

    if (hasVideo) {
        gLocalVideoSource = [gFactory videoSourceForScreenCast:isScreencast];
        gHijackCapturerDelegate = [[CFHijackCapturerDelegate alloc]
            initWithRealDelegate:gLocalVideoSource];
        gLocalVideoTrack = [gFactory videoTrackWithSource:gLocalVideoSource
                                                  trackId:kCFVideoTrackId];
    }
    NSDictionary* mandatoryConstraints = @{};
    RTC_OBJC_TYPE(RTCMediaConstraints)* constraints =
        [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc]
            initWithMandatoryConstraints:mandatoryConstraints
                     optionalConstraints:nil];

    gLocalAudioSource = [gFactory audioSourceWithConstraints:constraints];
    gLocalAudioTrack = [gFactory audioTrackWithSource:gLocalAudioSource
                                              trackId:kCFAudioTrackId];

    RTCLogInfo(TAG " createLocalTracks success");
    return 0;
}

+ (void)adaptVideoOutputFormat:(int)width height:(int)height fps:(int)fps {
    if (gLocalVideoSource) {
        [gLocalVideoSource adaptOutputFormatToWidth:width height:height fps:fps];
    }
}

+ (CFHijackCapturerDelegate*)getHijackCapturerDelegate {
    return gHijackCapturerDelegate;
}

+ (void)addLocalTrackRenderer:(id<RTC_OBJC_TYPE(RTCVideoRenderer)>)localTrackRenderer {
    RTC_OBJC_TYPE(RTCVideoTrack)* videoTrack = gLocalVideoTrack;
    if (videoTrack != nil) {
        [videoTrack addRenderer:localTrackRenderer];
    }
}

+ (void)removeLocalTrackRenderer:(id<RTC_OBJC_TYPE(RTCVideoRenderer)>)localTrackRenderer {
    RTC_OBJC_TYPE(RTCVideoTrack)* videoTrack = gLocalVideoTrack;
    if (videoTrack != nil) {
        [videoTrack removeRenderer:localTrackRenderer];
    }
}

+ (int32_t)destroyPeerConnectionFactory {
    RTCLogInfo(TAG " destroyPeerConnectionFactory");
    gLocalAudioSource = nil;
    gLocalAudioTrack = nil;
    gLocalVideoSource = nil;
    gLocalVideoTrack = nil;
    gFactory = nil;

    RTCLogInfo(TAG " destroyPeerConnectionFactory success");
    return 0;
}

- (instancetype)initWithUid:(NSString*)uid
                        dir:(CFPeerConnectionDir)dir
                   hasVideo:(bool)hasVideo
                   delegate:(id<CFPeerConnectionClientDelegate>)delegate
            videoMaxBitrate:(int32_t)videoMaxBitrate
          videoMaxFrameRate:(int32_t)videoMaxFrameRate {
    self = [super init];
    if (self) {
        _uid = uid;
        _dir = dir;
        _hasVideo = hasVideo;
        _delegate = delegate;
        _remoteTrackRenderers = [[NSMutableArray alloc] init];
        _videoMaxBitrate = videoMaxBitrate;
        _videoMaxFrameRate = videoMaxFrameRate;

        _queue = dispatch_queue_create("AvConf-PcClient", NULL);
        _sdpConstraints =
            [self defaultSdpConstraints:[CFPeerConnectionClient receive:dir]];
    }
    return self;
}

#pragma mark - Public

- (void)createPeerConnection:(NSArray<RTC_OBJC_TYPE(RTCIceServer)*>*)iceServers {
    [self logInfo:@"createPeerConnection %@", [iceServers description]];
    __block RTC_OBJC_TYPE(RTCPeerConnectionFactory)* factory = gFactory;
    __block RTC_OBJC_TYPE(RTCAudioTrack)* audioTrack = gLocalAudioTrack;
    __block RTC_OBJC_TYPE(RTCVideoTrack)* videoTrack = gLocalVideoTrack;
    if (!factory) {
        [self logError:@"createPeerConnection error: no factory"];
        [_delegate onError:_uid code:ERR_NO_FACTORY];
        return;
    }
    if ([self send] && (!audioTrack || (_hasVideo && !videoTrack))) {
        [self logError:@"createPeerConnection error: no sending track"];
        [_delegate onError:_uid code:ERR_NO_SENDING_TRACK];
        return;
    }

    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }

        strongSelf->_queuedRemoteCandidates = [[NSMutableArray alloc] init];

        RTC_OBJC_TYPE(RTCConfiguration)* config =
            [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
        config.iceServers = iceServers;
        config.tcpCandidatePolicy = RTCTcpCandidatePolicyDisabled;
        config.bundlePolicy = RTCBundlePolicyMaxBundle;
        config.rtcpMuxPolicy = RTCRtcpMuxPolicyRequire;
        config.continualGatheringPolicy = RTCContinualGatheringPolicyGatherContinually;
        config.keyType = RTCEncryptionKeyTypeECDSA;
        config.sdpSemantics = RTCSdpSemanticsUnifiedPlan;
        strongSelf->_peerConnection = [factory
            peerConnectionWithConfiguration:config
                                constraints:[strongSelf defaultMediaAudioConstraints]
                                   delegate:strongSelf];

        // use addTransceiver API on answer end seems only get recvonly answer,
        // so let's stay at addTrack API for now.
        if ([strongSelf send]) {
            [strongSelf->_peerConnection addTrack:audioTrack
                                        streamIds:@[ strongSelf->_uid ]];
            if (videoTrack) {
                [strongSelf->_peerConnection addTrack:videoTrack
                                            streamIds:@[ strongSelf->_uid ]];
            }
        }

        /*if ([strongSelf send]) {
            RTCRtpTransceiverInit* transceiverInit =
                [[RTCRtpTransceiverInit alloc] init];
            transceiverInit.direction =
                [strongSelf receive] ? RTCRtpTransceiverDirectionSendRecv
                                     : RTCRtpTransceiverDirectionSendOnly;
            transceiverInit.streamIds = @[ strongSelf->_uid ];
            [strongSelf->_peerConnection
                addTransceiverWithTrack:audioTrack
                                   init:transceiverInit];
            if (videoTrack) {
                [strongSelf->_peerConnection
                    addTransceiverWithTrack:videoTrack
                                       init:transceiverInit];
            }
        } else {
            RTCRtpTransceiverInit* transceiverInit =
                [[RTCRtpTransceiverInit alloc] init];
            transceiverInit.direction = RTCRtpTransceiverDirectionRecvOnly;
            transceiverInit.streamIds = @[ strongSelf->_uid ];
            [strongSelf->_peerConnection
                addTransceiverOfType:RTCRtpMediaTypeAudio
                                init:transceiverInit];
            if (strongSelf->_hasVideo) {
                [strongSelf->_peerConnection
                    addTransceiverOfType:RTCRtpMediaTypeVideo
                                    init:transceiverInit];
            }
        }*/

        // don't get remote tracks here after migrate to addTransceiver API,
        // because these tracks are not receiving tracks!
        // if ([strongSelf receive]) {
        //    [strongSelf getRemoteTracks];
        //}

        [self logInfo:@"createPeerConnection success"];
    });
}

- (void)getStats {
    __weak CFPeerConnectionClient* weakSelf = self;
    [_peerConnection statisticsWithCompletionHandler:^(
                        RTC_OBJC_TYPE(RTCStatisticsReport)* report) {
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }
        [strongSelf->_delegate
            onPeerConnectionStatsReady:strongSelf->_uid
                                report:report];
    }];
}

- (void)setAudioSendingEnabled:(bool)enable {
    [self logInfo:@"setAudioSendingEnabled %d", enable];
    __block bool enabled = enable;
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }

        RTC_OBJC_TYPE(RTCAudioTrack)* audioTrack = gLocalAudioTrack;
        if (audioTrack != nil) {
            [audioTrack setIsEnabled:enabled ? YES : NO];
        }
        [self logInfo:@"setAudioSendingEnabled %d success", enabled];
    });
}

- (void)setVideoSendingEnabled:(bool)enable {
    [self logInfo:@"setVideoSendingEnabled %d", enable];
    __block bool enabled = enable;
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }

        RTC_OBJC_TYPE(RTCVideoTrack)* videoTrack = gLocalVideoTrack;
        CFHijackCapturerDelegate* hijackCapturerDelegate = gHijackCapturerDelegate;
        if (videoTrack != nil && hijackCapturerDelegate != nil) {
            videoTrack.isEnabled = enabled ? YES : NO;
            [hijackCapturerDelegate toggleMute:!enabled];
        }
        [self logInfo:@"setVideoSendingEnabled %d success", enabled];
    });
}

- (void)setAudioReceivingEnabled:(bool)enable {
    [self logInfo:@"setAudioReceivingEnabled %d", enable];
    __block bool enabled = enable;
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }

        if (strongSelf->_remoteAudioTrack != nil) {
            [strongSelf->_remoteAudioTrack setIsEnabled:enabled ? YES : NO];
        }
        [self logInfo:@"setAudioReceivingEnabled %d success", enabled];
    });
}

- (void)setVideoReceivingEnabled:(bool)enable {
    [self logInfo:@"setVideoReceivingEnabled %d", enable];
    __block bool enabled = enable;
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }

        if (strongSelf->_remoteVideoTrack != nil) {
            [strongSelf->_remoteVideoTrack setIsEnabled:enabled ? YES : NO];
        }
        [self logInfo:@"setVideoReceivingEnabled %d success", enabled];
    });
}

- (void)createOffer {
    [self logInfo:@"createOffer"];
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf1 = weakSelf;
        if (strongSelf1 == nil) {
            return;
        }

        strongSelf1->_isInitiator = true;
        [strongSelf1->_peerConnection
            offerForConstraints:strongSelf1->_sdpConstraints
              completionHandler:^(RTC_OBJC_TYPE(RTCSessionDescription)* sdp, NSError* error) {
                  CFPeerConnectionClient* strongSelf2 = weakSelf;
                  if (strongSelf2 == nil) {
                      return;
                  }

                  [strongSelf2 createLocalSdpSuccess:sdp error:error];
              }];
        [self logInfo:@"createOffer success"];
    });
}

- (void)createAnswer {
    [self logInfo:@"createAnswer"];
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf1 = weakSelf;
        if (strongSelf1 == nil) {
            return;
        }

        strongSelf1->_isInitiator = false;
        [strongSelf1->_peerConnection
            answerForConstraints:strongSelf1->_sdpConstraints
               completionHandler:^(RTC_OBJC_TYPE(RTCSessionDescription)* sdp, NSError* error) {
                   CFPeerConnectionClient* strongSelf2 = weakSelf;
                   if (strongSelf2 == nil) {
                       return;
                   }

                   [strongSelf2 createLocalSdpSuccess:sdp error:error];
               }];

        [self logInfo:@"createAnswer success"];
    });
}

- (void)addIceCandidate:(RTCIceCandidate*)candidate {
    [self logInfo:@"addIceCandidate"];
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }

        if (strongSelf->_queuedRemoteCandidates) {
            [strongSelf->_queuedRemoteCandidates addObject:candidate];
        } else {
            [strongSelf->_peerConnection addIceCandidate:candidate
                            completionHandler:^(NSError *error) {
                                // todo
                            }];
        }
        [self logInfo:@"addIceCandidate success"];
    });
}

- (void)removeIceCandidates:(NSArray<RTC_OBJC_TYPE(RTCIceCandidate)*>*)candidates {
    [self logInfo:@"removeIceCandidates"];
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }

        [self logInfo:@"removeIceCandidates success"];
    });
}

- (void)setRemoteDescription:(RTC_OBJC_TYPE(RTCSessionDescription)*)sdp {
    [self logInfo:@"setRemoteDescription\n%@", [sdp description]];
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf1 = weakSelf;
        if (strongSelf1 == nil) {
            return;
        }

        // already refined by AvConf
        [strongSelf1->_peerConnection
            setRemoteDescription:sdp
               completionHandler:^(NSError* error) {
                   CFPeerConnectionClient* strongSelf2 = weakSelf;
                   if (!strongSelf2) {
                       return;
                   }

                   if (error != nil) {
                       [strongSelf2->_delegate onError:strongSelf2->_uid
                                                  code:ERR_SET_SDP_FAIL];
                       return;
                   }

                   if (strongSelf2->_isInitiator) {
                       [strongSelf2 drainCandidates];
                   }
               }];
        [strongSelf1 getRemoteTracks];

        [self logInfo:@"setRemoteDescription success"];
    });
}

- (bool)send {
    return [CFPeerConnectionClient send:_dir];
}

- (bool)receive {
    return [CFPeerConnectionClient receive:_dir];
}

- (int)startRecorder:(int32_t)dir path:(NSString*)path {
    if (_peerConnection) {
        return [_peerConnection startRecorder:dir path:path];
    }
    return -100;
}

- (int)stopRecorder:(int32_t)dir {
    if (_peerConnection) {
        return [_peerConnection stopRecorder:dir];
    }
    return -100;
}

- (void)requestFir {
    if (_peerConnection) {
        [_peerConnection sendVideoKeyFrame];
    }
}

- (void)close {
    [self logInfo:@"close"];
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }

        [strongSelf->_peerConnection close];
        strongSelf->_peerConnection = nil;
        strongSelf->_queue = nil;

        [self logInfo:@"close success"];
    });
}

- (void)addRemoteTrackRenderer:(id<RTC_OBJC_TYPE(RTCVideoRenderer)>)remoteTrackRenderer {
    [self logInfo:@"addRemoteTrackRenderer %@", remoteTrackRenderer];
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }

        if (strongSelf->_remoteVideoTrack != nil) {
            [strongSelf->_remoteVideoTrack addRenderer:remoteTrackRenderer];
        } else {
            [strongSelf->_remoteTrackRenderers addObject:remoteTrackRenderer];
        }
    });
}

- (void)removeRemoteTrackRenderer:(id<RTC_OBJC_TYPE(RTCVideoRenderer)>)remoteTrackRenderer {
    [self logInfo:@"removeRemoteTrackRenderer %@", remoteTrackRenderer];
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }

        if (strongSelf->_remoteVideoTrack != nil) {
            [strongSelf->_remoteVideoTrack removeRenderer:remoteTrackRenderer];
        } else {
            [strongSelf->_remoteTrackRenderers
                removeObject:remoteTrackRenderer];
        }
    });
}

#pragma mark - PC Delegate

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
    didChangeSignalingState:(RTC_OBJC_TYPE(RTCSignalingState))stateChanged {
    [self logInfo:@"didChangeSignalingState %ld", stateChanged];
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
    [self logInfo:@"didChangeIceConnectionState %ld", newState];
    if (newState == RTCIceConnectionStateConnected) {
        [_delegate onIceConnected:_uid];
    } else if (newState == RTCIceConnectionStateDisconnected) {
        [_delegate onIceDisconnected:_uid];
    } else if (newState == RTCIceConnectionStateFailed) {
        [_delegate onError:_uid code:ERR_ICE_FAIL];
    }
}

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
    didChangeIceGatheringState:(RTC_OBJC_TYPE(RTCIceGatheringState))newState {
    [self logInfo:@"didChangeIceGatheringState %ld", newState];
}

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
    didGenerateIceCandidate:(RTC_OBJC_TYPE(RTCIceCandidate)*)candidate {
    [_delegate onIceCandidate:_uid candidate:candidate];
}

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
    didRemoveIceCandidates:(NSArray<RTC_OBJC_TYPE(RTCIceCandidate)*>*)candidates {
}

- (void)peerConnection:(RTC_OBJC_TYPE(RTCPeerConnection)*)peerConnection
    didOpenDataChannel:(RTC_OBJC_TYPE(RTCDataChannel)*)dataChannel {
}

#pragma mark - Private

- (void)getRemoteTracks {
    if (_peerConnection == nil || ![self receive] || _remoteAudioTrack != nil ||
        _remoteVideoTrack != nil) {
        return;
    }
    for (RTC_OBJC_TYPE(RTCRtpTransceiver)* transceiver in _peerConnection.transceivers) {
        RTC_OBJC_TYPE(RTCMediaStreamTrack)* track = transceiver.receiver.track;
        if (transceiver.mediaType == RTCRtpMediaTypeAudio && track != nil) {
            _remoteAudioTrack = (RTC_OBJC_TYPE(RTCAudioTrack)*)track;
        } else if (transceiver.mediaType == RTCRtpMediaTypeVideo &&
                   track != nil) {
            _remoteVideoTrack = (RTC_OBJC_TYPE(RTCVideoTrack)*)track;
        }
    }
    if (_remoteVideoTrack != nil) {
        [self logInfo:@"addRemoteTrackRenderer at getRemoteTracks %@",
                      _remoteTrackRenderers];
        for (id<RTC_OBJC_TYPE(RTCVideoRenderer)> renderer in _remoteTrackRenderers) {
            [_remoteVideoTrack addRenderer:renderer];
        }
        [_remoteTrackRenderers removeAllObjects];
    }
}

- (RTC_OBJC_TYPE(RTCMediaConstraints)*)defaultMediaAudioConstraints {
    NSDictionary* mandatoryConstraints = @{};
    RTC_OBJC_TYPE(RTCMediaConstraints)* constraints =
        [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc]
            initWithMandatoryConstraints:mandatoryConstraints
                     optionalConstraints:nil];
    return constraints;
}

- (RTC_OBJC_TYPE(RTCMediaConstraints)*)defaultSdpConstraints:(bool)receive {
    NSDictionary* mandatoryConstraints = nil;
    if (receive) {
        // use addTransceiver API on answer end seems only get recvonly answer,
        // so let's stay at addTrack API for now (which needs OfferToReceiveAudio).
        mandatoryConstraints = @{
            @"OfferToReceiveAudio" : @"true",
            @"OfferToReceiveVideo" : @"true"
        };
    }
    RTC_OBJC_TYPE(RTCMediaConstraints)* constraints =
        [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc]
            initWithMandatoryConstraints:mandatoryConstraints
                     optionalConstraints:nil];
    return constraints;
}

- (void)createLocalSdpSuccess:(RTC_OBJC_TYPE(RTCSessionDescription)*)sdp
                        error:(NSError*)error {
    if (error != nil) {
        [_delegate onError:_uid code:ERR_CREATE_SDP_FAIL];
        return;
    }
    [self logInfo:@"onCreateSuccess\n%@", [sdp description]];

    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf1 = weakSelf;
        if (strongSelf1 == nil) {
            return;
        }

        RTC_OBJC_TYPE(RTCSessionDescription)* localSdp =
            [[RTC_OBJC_TYPE(RTCSessionDescription) alloc]
                initWithType:sdp.type
                         sdp:[strongSelf1->_delegate
                                 onPreferCodecs:strongSelf1->_uid
                                            sdp:sdp.sdp]];
        [strongSelf1 logInfo:@"refined sdp\n%@", [localSdp description]];

        [strongSelf1->_peerConnection
        setLocalDescription:localSdp
          completionHandler:^(NSError* _Nullable error) {
              CFPeerConnectionClient* strongSelf2 = weakSelf;
              if (strongSelf2 == nil) {
                  return;
              }

              [strongSelf2 setLocalSdpSuccess:localSdp error:error];
          }];
    });
}

- (void)setLocalSdpSuccess:(RTC_OBJC_TYPE(RTCSessionDescription)*)sdp error:(NSError*)error {
    if (error != nil) {
        [_delegate onError:_uid code:ERR_SET_SDP_FAIL];
        return;
    }

    [_delegate onLocalDescription:_uid localSdp:sdp];
    [self doSetVideoMaxBitrate];
    if (!_isInitiator) {
        [self drainCandidates];
    }
}

- (void)setVideoMaxBitrate:(int)videoMaxBitrate {
    [self logInfo:@"setVideoMaxBitrate %d", videoMaxBitrate];

    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }

        strongSelf->_videoMaxBitrate = videoMaxBitrate;
        [strongSelf doSetVideoMaxBitrate];
    });
}

- (void)doSetVideoMaxBitrate {
    if (_videoMaxBitrate <= 0 || _videoMaxFrameRate <= 0) {
        return;
    }
    for (RTC_OBJC_TYPE(RTCRtpSender)* sender in _peerConnection.senders) {
        if (sender.track != nil) {
            if ([sender.track.kind isEqualToString:kCFVideoTrackKind]) {
                RTC_OBJC_TYPE(RTCRtpParameters)* parametersToModify = sender.parameters;
                for (RTC_OBJC_TYPE(RTCRtpEncodingParameters)* encoding in parametersToModify
                         .encodings) {
                    encoding.maxBitrateBps =
                        @(_videoMaxBitrate * kKbpsMultiplier);
                    encoding.maxFramerate = @(_videoMaxFrameRate);
                }
                [sender setParameters:parametersToModify];
            }
        }
    }
}

- (void)drainCandidates {
    __weak CFPeerConnectionClient* weakSelf = self;
    dispatch_async(_queue, ^{
        CFPeerConnectionClient* strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }

        if (strongSelf->_queuedRemoteCandidates &&
            strongSelf->_peerConnection) {
            for (RTC_OBJC_TYPE(RTCIceCandidate)* candidate in strongSelf
                     ->_queuedRemoteCandidates) {
                [strongSelf->_peerConnection addIceCandidate:candidate
                                completionHandler:^(NSError *error) {
                                    // todo
                                }];
            }
            strongSelf->_queuedRemoteCandidates = nil;
        }
    });
}

- (void)logInfo:(NSString*)format, ... {
    va_list args;
    va_start(args, format);
    NSString* content = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);
    RTCLogInfo(TAG "@%p %@", self, content);
}

- (void)logError:(NSString*)format, ... {
    va_list args;
    va_start(args, format);
    NSString* content = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);
    RTCLogError(TAG "@%p %@", self, content);
}

@end
