//
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Piasy
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


#import <Foundation/Foundation.h>

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@protocol CFAudioMixerDelegate <NSObject>
- (void)onSsrcFinished:(int32_t)ssrc;
- (void)onSsrcError:(int32_t)ssrc code:(int32_t)code;
@end

RTC_OBJC_EXPORT
@interface CFAudioMixer : NSObject

- (instancetype)initWithBackingTrack:(NSString*)backingTrack
                   captureSampleRate:(int32_t)captureSampleRate
                   captureChannelNum:(int32_t)captureChannelNum
                     frameDurationUs:(int32_t)frameDurationUs
                  enableMusicSyncFix:(bool)enableMusicSyncFix
                waitingMixDelayFrame:(int32_t)waitingMixDelayFrame
                            delegate:(id<CFAudioMixerDelegate>)delegate;

- (void)startMixer;

- (void)pauseMixer;

- (void)resumeMixer;

- (void)toggleMusicStreaming:(bool)streaming;

- (void)toggleMicEcho:(bool)micEcho;

- (void)setMicVolume:(float)volume;

- (void)setMusicVolume:(float)volume;

- (int64_t)getMusicLengthMs;

- (int64_t)getMusicProgressMs;

- (void)seekMusic:(int64_t)progressMs;

- (void)stopMixer;


- (void)onSsrcFinished:(int32_t)ssrc;
- (void)onSsrcError:(int32_t)ssrc code:(int32_t)code;
@end

NS_ASSUME_NONNULL_END
