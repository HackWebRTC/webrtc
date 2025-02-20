#import <Foundation/Foundation.h>

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, CFAudioDevice) {
    CF_SPEAKER_PHONE,
    CF_WIRED_HEADSET,
    CF_EARPIECE,
    CF_BLUETOOTH,
    CF_NONE,
};

RTC_OBJC_EXPORT
@protocol CFAudioDeviceManagerDelegate <NSObject>

- (void)onAudioDeviceChanged:(CFAudioDevice)audioDevice;

@end

RTC_OBJC_EXPORT
@interface CFAudioDeviceManager : NSObject

- (instancetype)init;

- (void)setSpeakerphoneOn:(bool)speakerOn;

- (void)start:(id<CFAudioDeviceManagerDelegate>)delegate;

- (void)stop;

@end

NS_ASSUME_NONNULL_END
