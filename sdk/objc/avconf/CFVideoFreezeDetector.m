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


#import "CFVideoFreezeDetector.h"
#import "base/RTCLogging.h"

#define TAG "VideoFreezeDetector"

static int64_t kFreezeThresholdMs = 250;
static int64_t kLogFreezeIntervalMs = 3000;

@implementation CFVideoFreezeDetector {
    NSString* _uid;

    int64_t _lastFrameReceivedTime;
    int64_t _lastLogFreezeTime;
}

- (instancetype)initWithUid:(NSString*)uid {
    self = [super init];
    if (self) {
        _uid = uid;
    }
    return self;
}

- (void)onFrame {
    int64_t now = (int64_t)([[NSDate date] timeIntervalSince1970] * 1000);
    int64_t lastFrameReceivedTime = _lastFrameReceivedTime;
    _lastFrameReceivedTime = now;
    if (lastFrameReceivedTime == 0) {
        RTCLogInfo(TAG " video render start: %@", _uid);
        return;
    }

    int64_t interval = now - lastFrameReceivedTime;
    if (interval < kFreezeThresholdMs) {
        return;
    }

    if (now - _lastLogFreezeTime < kLogFreezeIntervalMs) {
        return;
    }
    _lastLogFreezeTime = now;
    RTCLogInfo(TAG " video freeze %lld ms: %@", interval, _uid);
}

@end
