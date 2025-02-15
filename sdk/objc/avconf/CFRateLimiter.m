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


#import "CFRateLimiter.h"

@implementation CFRateLimiter {
    int64_t _interval;
    int64_t _expected;
    int64_t _firstTs;
}

- (instancetype)initWithInterval:(int64_t)interval {
    self = [super init];
    if (self) {
        _interval = interval;
        _expected = 0;
        _firstTs = -1;
    }
    return self;
}

- (bool)check:(int64_t)ts {
    if (_firstTs == -1) {
        _firstTs = ts;
        return true;
    }

    int64_t relativeTs = ts - _firstTs;
    if (relativeTs >= _expected) {
        if (relativeTs - _expected > _interval) {
            _firstTs = -1;
            _expected = _interval;
        } else {
            _expected += _interval;
        }
        return true;
    }

    return false;
}

- (void)reset {
    _firstTs = -1;
    _expected = 0;
}

@end
