#import "CFTransitBuffer.h"

#import "sdk/objc/api/video_frame_buffer/RTCNativeI420Buffer.h"

static RTCI420Buffer* g_dummy_i420;

@implementation CFTransitBuffer {
    rtc::scoped_refptr<webrtc::TransitVideoFrameBuffer> _buffer;
}

- (instancetype)initWithBuffer:
    (rtc::scoped_refptr<webrtc::TransitVideoFrameBuffer>)buffer {
    self = [super init];
    if (self) {
        _buffer = buffer;
    }
    return self;
}

- (rtc::scoped_refptr<webrtc::TransitVideoFrameBuffer>)buffer {
    return _buffer;
}

- (int)width {
    return _buffer->width();
}

- (int)height {
    return _buffer->height();
}

- (id<RTCI420Buffer>)toI420 {
    if (!g_dummy_i420) {
        g_dummy_i420 = [[RTCI420Buffer alloc] initWithWidth:_buffer->width()
                                                     height:_buffer->height()];
    }
    return g_dummy_i420;
}

@end
