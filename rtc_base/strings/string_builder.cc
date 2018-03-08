/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/strings/string_builder.h"

namespace rtc {

SimpleStringBuilder::SimpleStringBuilder(rtc::ArrayView<char> buffer)
    : buffer_(buffer) {
  buffer_[0] = '\0';
  RTC_DCHECK(IsConsistent());
}

}  // namespace rtc
