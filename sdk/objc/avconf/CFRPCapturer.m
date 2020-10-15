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


#import "CFRPCapturer.h"

#import <ReplayKit/ReplayKit.h>

#import "base/RTCLogging.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"

#define TAG "CFRPCapturer"

@implementation CFRPCapturer {
    void (^_errorHandler)(NSString*);
}

- (instancetype)initWithDelegate:(id<RTCVideoCapturerDelegate>)delegate
                 andErrorHandler:(void (^)(NSString*))handler {
    self = [super initWithDelegate:delegate];
    if (self) {
        _errorHandler = handler;
    }
    return self;
}

- (void)startCapture {
    RPScreenRecorder* recorder = [RPScreenRecorder sharedRecorder];
    if (recorder.recording) {
        RTCLogError(TAG " startCapture already started");
        return;
    }
    RTCLogInfo(TAG " startCapture");
    __weak CFRPCapturer* weakSelf = self;
    if (@available(iOS 11.0, *)) {
        // recorder.microphoneEnabled = YES;
        [recorder startCaptureWithHandler:^(CMSampleBufferRef sampleBuffer,
                                            RPSampleBufferType sampleBufferType,
                                            NSError* error) {
            switch (sampleBufferType) {
                case RPSampleBufferTypeVideo: {
                    if (CMSampleBufferGetNumSamples(sampleBuffer) != 1 ||
                        !CMSampleBufferIsValid(sampleBuffer) ||
                        !CMSampleBufferDataIsReady(sampleBuffer)) {
                        return;
                    }

                    CVPixelBufferRef pixelBuffer =
                        CMSampleBufferGetImageBuffer(sampleBuffer);
                    if (pixelBuffer == nil) {
                        return;
                    }

                    CFRPCapturer* strongSelf = weakSelf;
                    if (strongSelf) {
                        RTCCVPixelBuffer* rtcPixelBuffer =
                            [[RTCCVPixelBuffer alloc]
                                initWithPixelBuffer:pixelBuffer];
                        int64_t timeStampNs =
                            CMTimeGetSeconds(
                                CMSampleBufferGetPresentationTimeStamp(
                                    sampleBuffer)) *
                            NSEC_PER_SEC;
                        RTCVideoFrame* videoFrame = [[RTCVideoFrame alloc]
                            initWithBuffer:rtcPixelBuffer
                                  rotation:RTCVideoRotation_0
                               timeStampNs:timeStampNs];
                        [strongSelf notifyFrame:videoFrame];
                    }
                    break;
                }
                case RPSampleBufferTypeAudioApp:
                    break;
                case RPSampleBufferTypeAudioMic:
                    break;
                default:
                    break;
            }
        }
            completionHandler:^(NSError* error) {
                CFRPCapturer* strongSelf = weakSelf;
                if (error) {
                    RTCLogError(TAG " startCapture fail: %@",
                               error.localizedDescription);
                    if (!strongSelf) {
                        RTCLogError(TAG " startCapture fail, but no strong self");
                        return;
                    }
                    strongSelf->_errorHandler(@"");
                } else {
                    RTCLogInfo(TAG " startCapture success");
                    if (!strongSelf) {
                        RTCLogError(TAG
                                   " startCapture success, but no strong self");
                        return;
                    }
                }
            }];
    } else {
        RTCLogError(TAG " startCapture fail: OS not support");
    }
}

- (void)stopCapture {
    RTCLogInfo(TAG " stopCapture");
    if (@available(iOS 11.0, *)) {
        [[RPScreenRecorder sharedRecorder]
            stopCaptureWithHandler:^(NSError* _Nullable error) {
                if (error) {
                    RTCLogError(TAG " stopCapture fail: %@",
                               error.localizedDescription);
                } else {
                    RTCLogInfo(TAG " stopCapture success");
                }
            }];
    } else {
        RTCLogError(TAG " stopCapture fail: OS not support");
    }
}

- (void)notifyFrame:(RTCVideoFrame*)videoFrame {
    [self.delegate capturer:self didCaptureVideoFrame:videoFrame];
}

@end
