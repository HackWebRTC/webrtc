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

#import "CFRateLimiter.h"

#define TAG "CFRPCapturer"

@implementation CFRPCapturer {
    dispatch_queue_t _queue;
    dispatch_source_t _timerSource;

    int32_t _desiredHeight;
    bool _isLandscape;
    CFRateLimiter* _fpsLimiter;

    void (^_errorHandler)(NSString*);

    int64_t _feedOldFrameInterval;
    int64_t _lastCapturedFrameTime;
    RTCCVPixelBuffer* _lastCapturedFrame;
    int64_t _lastCapturedFrameTs;
}

- (instancetype)initWithDelegate:(id<RTCVideoCapturerDelegate>)delegate
                 andErrorHandler:(void (^)(NSString*))handler
                andDesiredHeight:(int32_t)desiredHeight
                  andIsLandscape:(bool)isLandscape
                          andFps:(int32_t)fps {
    self = [super initWithDelegate:delegate];
    if (self) {
        _queue = dispatch_queue_create("CFRPCapturerGuarder", NULL);

        _errorHandler = handler;
        _feedOldFrameInterval = 1000 / fps;

        _desiredHeight = desiredHeight;
        _isLandscape = isLandscape;
        _fpsLimiter = [[CFRateLimiter alloc] initWithInterval:1000 / fps];
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
    [_fpsLimiter reset];
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
                        size_t width = CVPixelBufferGetWidth(pixelBuffer);
                        size_t height = CVPixelBufferGetHeight(pixelBuffer);
                        int desiredHeight;
                        int desiredWidth;
                        if (strongSelf->_isLandscape) {
                            desiredHeight = strongSelf->_desiredHeight;
                            desiredWidth = width * desiredHeight / height;
                        } else {
                            desiredWidth = strongSelf->_desiredHeight;
                            desiredHeight = height * desiredWidth / width;
                        }

                        RTCCVPixelBuffer* rtcPixelBuffer =
                            [[RTCCVPixelBuffer alloc]
                                initWithPixelBuffer:pixelBuffer
                                       adaptedWidth:desiredWidth
                                      adaptedHeight:desiredHeight
                                          cropWidth:width
                                         cropHeight:height
                                              cropX:0
                                              cropY:0];
                        int64_t timeStampNs =
                            CMTimeGetSeconds(
                                CMSampleBufferGetPresentationTimeStamp(
                                    sampleBuffer)) *
                            NSEC_PER_SEC;
                        RTCVideoFrame* videoFrame = [[RTCVideoFrame alloc]
                            initWithBuffer:rtcPixelBuffer
                                  rotation:RTCVideoRotation_0
                               timeStampNs:timeStampNs];
                        @synchronized(strongSelf) {
                            strongSelf->_lastCapturedFrame = rtcPixelBuffer;
                            strongSelf->_lastCapturedFrameTs = timeStampNs;
                            strongSelf->_lastCapturedFrameTime = (int64_t)(
                                [[NSDate date] timeIntervalSince1970] * 1000);
                        }
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
                    [strongSelf onCaptureStarted];
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
    if (_timerSource) {
        dispatch_source_cancel(_timerSource);
        _timerSource = nil;
    }
}

- (void)onCaptureStarted {
    _timerSource =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    dispatch_source_set_timer(_timerSource, dispatch_time(DISPATCH_TIME_NOW, 0),
                              (_feedOldFrameInterval >> 1) * NSEC_PER_MSEC,
                              0 * NSEC_PER_SEC);
    __weak CFRPCapturer* weakSelf = self;
    dispatch_source_set_event_handler(_timerSource, ^{
        CFRPCapturer* strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf checkFeedFrame];
        }
    });
    if (@available(iOS 10.0, *)) {
        dispatch_activate(_timerSource);
    } else {
        dispatch_resume(_timerSource);
    }
}

- (void)checkFeedFrame {
    RTCVideoFrame* videoFrame = nil;
    @synchronized(self) {
        int64_t now = (int64_t)([[NSDate date] timeIntervalSince1970] * 1000);
        int64_t noFrameDuration = now - _lastCapturedFrameTime;
        if (noFrameDuration > _feedOldFrameInterval &&
            _lastCapturedFrameTime > 0 && _lastCapturedFrame) {
            int64_t timestampNs =
                _lastCapturedFrameTs + noFrameDuration * 1000000;
            videoFrame =
                [[RTCVideoFrame alloc] initWithBuffer:_lastCapturedFrame
                                             rotation:RTCVideoRotation_0
                                          timeStampNs:timestampNs];
            _lastCapturedFrameTime = now;
            _lastCapturedFrameTs = timestampNs;
        }
    }
    if (videoFrame) {
        [self notifyFrame:videoFrame];
    }
}

- (void)notifyFrame:(RTCVideoFrame*)videoFrame {
    if ([_fpsLimiter check:videoFrame.timeStampNs / 1000000]) {
        [self.delegate capturer:self didCaptureVideoFrame:videoFrame];
    }
}

@end
