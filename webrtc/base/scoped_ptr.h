/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This entire file is deprecated, and will be removed in XXXX 2016. Use
// std::unique_ptr instead!

#ifndef WEBRTC_BASE_SCOPED_PTR_H__
#define WEBRTC_BASE_SCOPED_PTR_H__

// All these #includes are left to maximize backwards compatibility.

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include <algorithm>
#include <cstddef>
#include <memory>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/template_util.h"
#include "webrtc/typedefs.h"

namespace rtc {

template <typename T, typename Deleter = std::default_delete<T>>
using scoped_ptr = std::unique_ptr<T, Deleter>;

// These used to convert between std::unique_ptr and std::unique_ptr. Now they
// are no-ops.
template <typename T>
std::unique_ptr<T> ScopedToUnique(std::unique_ptr<T> up) {
  return up;
}
template <typename T>
std::unique_ptr<T> UniqueToScoped(std::unique_ptr<T> up) {
  return up;
}

}  // namespace rtc

#endif  // #ifndef WEBRTC_BASE_SCOPED_PTR_H__
