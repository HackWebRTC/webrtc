/*
 *  Copyright (c) 2009 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Author: lexnikitin@google.com (Alexey Nikitin)
 *
 * V4LLookup provides basic functionality to work with V2L2 devices in Linux
 * The functionality is implemented as a class with virtual methods for
 * the purpose of unit testing.
 */
#ifndef WEBRTC_MEDIA_DEVICES_V4LLOOKUP_H_
#define WEBRTC_MEDIA_DEVICES_V4LLOOKUP_H_

#include <string>

#ifdef WEBRTC_LINUX
namespace cricket {
class V4LLookup {
 public:
  virtual ~V4LLookup() {}

  static bool IsV4L2Device(const std::string& device_path) {
    return GetV4LLookup()->CheckIsV4L2Device(device_path);
  }

  static void SetV4LLookup(V4LLookup* v4l_lookup) {
    v4l_lookup_ = v4l_lookup;
  }

  static V4LLookup* GetV4LLookup() {
    if (!v4l_lookup_) {
      v4l_lookup_ = new V4LLookup();
    }
    return v4l_lookup_;
  }

 protected:
  static V4LLookup* v4l_lookup_;
  // Making virtual so it is easier to mock
  virtual bool CheckIsV4L2Device(const std::string& device_path);
};

}  // namespace cricket

#endif  // WEBRTC_LINUX
#endif  // WEBRTC_MEDIA_DEVICES_V4LLOOKUP_H_
