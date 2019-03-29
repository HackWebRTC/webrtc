/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SEQUENCED_TASK_CHECKER_H_
#define RTC_BASE_SEQUENCED_TASK_CHECKER_H_

#include "rtc_base/synchronization/sequence_checker.h"

namespace rtc {
// TODO(srte): Replace usages of this with SequenceChecker.
class SequencedTaskChecker : public webrtc::SequenceChecker {
 public:
  bool CalledSequentially() const { return IsCurrent(); }
};
}  // namespace rtc

#define RTC_DCHECK_CALLED_SEQUENTIALLY(x) RTC_DCHECK_RUN_ON(x)

#endif  // RTC_BASE_SEQUENCED_TASK_CHECKER_H_
