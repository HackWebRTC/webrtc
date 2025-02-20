#import "CFAudioDeviceManager.h"

#import <AVFoundation/AVFoundation.h>
#import "base/RTCLogging.h"
#import "components/audio/RTCAudioSession.h"
#import "components/audio/RTCAudioSessionConfiguration.h"

#define TAG "CFAudioDeviceManager"

@implementation CFAudioDeviceManager {
    id<CFAudioDeviceManagerDelegate> _delegate;
    bool _speakerOn;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _speakerOn = true; // default to turn on speaker when no headset is connected
    }
    return self;
}

- (void)setSpeakerphoneOn:(bool)speakerOn {
    RTCLogInfo(TAG " setSpeakerphoneOn %d", speakerOn);
    _speakerOn = speakerOn;
    NSError* error = nil;

    RTCAudioSessionConfiguration* configuration =
        [RTCAudioSessionConfiguration webRTCConfiguration];

    RTCAudioSession* rtcSession = [RTCAudioSession sharedInstance];
    [rtcSession lockForConfiguration];
    [rtcSession setConfiguration:configuration active:YES error:&error];
    [rtcSession unlockForConfiguration];

    AVAudioSession* session = [AVAudioSession sharedInstance];
    if (speakerOn) {
        [session overrideOutputAudioPort:AVAudioSessionPortOverrideSpeaker
                                   error:&error];
    } else {
        [session overrideOutputAudioPort:AVAudioSessionPortOverrideNone
                                   error:&error];
    }
}

- (void)start:(id<CFAudioDeviceManagerDelegate>)delegate {
    _delegate = delegate;

    // This notification is posted on a secondary thread
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(routeChanged:)
               name:AVAudioSessionRouteChangeNotification
             object:[AVAudioSession sharedInstance]];

    AVAudioSession* session = [AVAudioSession sharedInstance];
    if ([self isBuiltIn:session.currentRoute]) {
        RTCLogInfo(TAG " start setSpeakerphoneOn");
        [self setSpeakerphoneOn:true];
    }
}

- (void)stop {
    _delegate = nil;
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:AVAudioSessionRouteChangeNotification
                object:[AVAudioSession sharedInstance]];
}

#pragma mark - private

- (NSString*)audioDeviceName:(CFAudioDevice)device {
    switch (device) {
        case CF_SPEAKER_PHONE:
            return @"SPEAKER_PHONE";
        case CF_WIRED_HEADSET:
            return @"WIRED_HEADSET";
        case CF_EARPIECE:
            return @"EARPIECE";
        case CF_BLUETOOTH:
            return @"BLUETOOTH";
        case CF_NONE:
            return @"NONE";
    }
}

- (void)routeChanged:(NSNotification*)noti {
    AVAudioSession* session = [AVAudioSession sharedInstance];

    CFAudioDevice device = CF_NONE;
    if ([self isHeadset:session.currentRoute]) {
        device = CF_WIRED_HEADSET;
    } else if ([self isBuiltIn:session.currentRoute]) {
        [self setSpeakerphoneOn:_speakerOn];
        device = CF_SPEAKER_PHONE;
    }
    RTCLogInfo(TAG " routeChanged %@", [self audioDeviceName:device]);
    [_delegate onAudioDeviceChanged:device];
    // __weak id<CFAudioDeviceManagerDelegate> weakDelegate = _delegate;
    // dispatch_async(dispatch_get_main_queue(), ^{
    //     id<CFAudioDeviceManagerDelegate> strongDelegate = weakDelegate;
    //     if (strongDelegate) {
    //         [strongDelegate onAudioDeviceChanged:device];
    //     }
    // });
}

- (bool)isHeadset:(AVAudioSessionRouteDescription*)route {
    bool headsetInput = false;
    if (route.inputs.count > 0) {
        if ([route.inputs[0].portType
                isEqualToString:AVAudioSessionPortHeadsetMic]) {
            headsetInput = true;
        }
    }
    bool headsetOutput = false;
    if (route.outputs.count > 0) {
        if ([route.outputs[0].portType
                isEqualToString:AVAudioSessionPortHeadphones]) {
            headsetOutput = true;
        }
    }

    return headsetInput && headsetOutput;
}

- (bool)isBuiltIn:(AVAudioSessionRouteDescription*)route {
    bool builtInInput = false;
    if (route.inputs.count > 0) {
        if ([route.inputs[0].portType
                isEqualToString:AVAudioSessionPortBuiltInMic]) {
            builtInInput = true;
        }
    }
    bool builtInOutput = false;
    if (route.outputs.count > 0) {
        if ([route.outputs[0].portType
                isEqualToString:AVAudioSessionPortBuiltInSpeaker] ||
            [route.outputs[0].portType
                isEqualToString:AVAudioSessionPortBuiltInReceiver]) {
            builtInOutput = true;
        }
    }

    return builtInInput && builtInOutput;
}

@end
