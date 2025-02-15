#import "CFFileVideoCapturer.h"

#import "base/RTCLogging.h"
#import "CFTransitBuffer.h"

#include <memory>

#include "api/task_queue/default_task_queue_factory.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "modules/transit_media/file_capturer.h"

#define TAG "CFFileVideoCapturer"

class FrameSink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
    FrameSink(CFFileVideoCapturer* capturer) : capturer_(capturer) {}

    void OnFrame(const webrtc::VideoFrame& frame) override {
        webrtc::TransitVideoFrameBuffer* buffer =
            reinterpret_cast<webrtc::TransitVideoFrameBuffer*>(
                frame.video_frame_buffer().release());
        CFTransitBuffer* transitBuffer = [[CFTransitBuffer alloc]
            initWithBuffer:rtc::scoped_refptr<webrtc::TransitVideoFrameBuffer>(
                               buffer)];

        RTCVideoFrame* videoFrame =
            [[RTCVideoFrame alloc] initWithBuffer:transitBuffer
                                         rotation:RTCVideoRotation_0
                                      timeStampNs:frame.timestamp_us() * 1000
                                            dummy:false
                                          transit:true];
        [capturer_.delegate capturer:capturer_ didCaptureVideoFrame:videoFrame];
    }

private:
    CFFileVideoCapturer* capturer_;
};

@implementation CFFileVideoCapturer {
    // void (^_errorHandler)(NSString*);
    NSString* _path;
    std::unique_ptr<webrtc::FileCapturer> _file_capturer;
    std::unique_ptr<FrameSink> _frame_sink;
}

- (instancetype)initWithDelegate:(id<RTCVideoCapturerDelegate>)delegate
                            // errorHandler:(void (^)(NSString*))handler
                            path:(NSString*)path
                            dumpPath:(NSString*)dumpPath
                           width:(int)width
                          height:(int)height {
    self = [super initWithDelegate:delegate];
    if (self) {
        //_errorHandler = handler;
        _path = path;
        std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory =
            webrtc::CreateDefaultTaskQueueFactory();
        _file_capturer.reset(new webrtc::FileCapturer(
            [path UTF8String], [dumpPath UTF8String], width, height, task_queue_factory.get()));
    }
    return self;
}

- (void)startCapture {
    RTCLogInfo(TAG " startCapture");

    _frame_sink.reset(new FrameSink(self));
    _file_capturer->RegisterVideoFrameCallback(_frame_sink.get());
    _file_capturer->Start(true);
}

- (void)stopCapture {
    RTCLogInfo(TAG " stopCapture");
    _file_capturer->DeRegisterVideoFrameCallback();
    _file_capturer->Stop();
}

@end
