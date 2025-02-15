#import <AVFoundation/AVFoundation.h>

#import "RTCMacros.h"
#import "RTCVideoFrameBuffer.h"

#include "api/scoped_refptr.h"
#include "modules/transit_media/transit_video_frame_buffer.h"

NS_ASSUME_NONNULL_BEGIN

@interface CFTransitBuffer : NSObject<RTCVideoFrameBuffer>

- (instancetype)initWithBuffer:
    (rtc::scoped_refptr<webrtc::TransitVideoFrameBuffer>)buffer;

- (rtc::scoped_refptr<webrtc::TransitVideoFrameBuffer>)buffer;

@end

NS_ASSUME_NONNULL_END
