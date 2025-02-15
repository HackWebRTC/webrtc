#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCVideoCapturer.h"

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@interface CFFileVideoCapturer : RTCVideoCapturer

- (instancetype)initWithDelegate:(id<RTCVideoCapturerDelegate>)delegate
                            // errorHandler:(void (^)(NSString*))handler
                            path:(NSString*)path
                            dumpPath:(NSString*)dumpPath
                           width:(int)width
                          height:(int)height;

- (void)startCapture;

- (void)stopCapture;

@end

NS_ASSUME_NONNULL_END
