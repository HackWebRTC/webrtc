/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "webrtc/base/objc/RTCFieldTrials.h"

#include <memory>
#include "webrtc/system_wrappers/include/field_trial_default.h"

#import "webrtc/base/objc/RTCLogging.h"

static NSString * const kRTCEnableSendSideBweString =
    @"WebRTC-SendSideBwe/Enabled/";
static std::unique_ptr<char[]> gFieldTrialInitString;

void RTCInitFieldTrials(RTCFieldTrialOptions options) {
  NSMutableString *fieldTrialInitString = [NSMutableString string];
  if (options & RTCFieldTrialOptionsSendSideBwe) {
    [fieldTrialInitString appendString:kRTCEnableSendSideBweString];
  }
  size_t len = fieldTrialInitString.length + 1;
  gFieldTrialInitString.reset(new char[len]);
  if (![fieldTrialInitString getCString:gFieldTrialInitString.get()
                              maxLength:len
                               encoding:NSUTF8StringEncoding]) {
    RTCLogError(@"Failed to convert field trial string.");
    return;
  }
  webrtc::field_trial::InitFieldTrialsFromString(gFieldTrialInitString.get());
}
