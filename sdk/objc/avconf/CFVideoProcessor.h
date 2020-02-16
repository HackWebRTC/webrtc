#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoCapturer.h"

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@protocol CFVideoProcessorDelegate <NSObject>

- (void)onProcessedVideoFrame:(RTCVideoFrame *)frame;

@end

RTC_OBJC_EXPORT
@protocol CFVideoProcessor <NSObject>

- (void)setVideoProcessorDelegate:(nullable id<CFVideoProcessorDelegate>)delegate;

- (void)onVideoFrame:(RTCVideoFrame *)frame;

@end

NS_ASSUME_NONNULL_END
