/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_X11_SHARED_X_DISPLAY_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_X11_SHARED_X_DISPLAY_H_

#include <assert.h>
#include <X11/Xlib.h>

#include <string>

#include "webrtc/system_wrappers/interface/atomic32.h"
#include "webrtc/system_wrappers/interface/scoped_refptr.h"

namespace webrtc {

// A ref-counted object to store XDisplay connection.
class SharedXDisplay {
 public:
  // Takes ownership of |display|.
  explicit SharedXDisplay(Display* display);

  void AddRef() { ++ref_count_; }
  void Release() {
    if (--ref_count_ == 0)
      delete this;
  }

  Display* display() { return display_; }

  // Creates a new X11 Display for the |display_name|. NULL is returned if X11
  // connection failed. Equivalent to CreateDefault() when |display_name| is
  // empty.
  static scoped_refptr<SharedXDisplay> Create(const std::string& display_name);

  // Creates X11 Display connection for the default display (e.g. specified in
  // DISPLAY). NULL is returned if X11 connection failed.
  static scoped_refptr<SharedXDisplay> CreateDefault();

 private:
  ~SharedXDisplay();

  Atomic32 ref_count_;
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(SharedXDisplay);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_X11_SHARED_X_DISPLAY_H_
