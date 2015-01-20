/*
 * libjingle
 * Copyright 2009 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Author: lexnikitin@google.com (Alexey Nikitin)
 *
 * V4LLookup provides basic functionality to work with V2L2 devices in Linux
 * The functionality is implemented as a class with virtual methods for
 * the purpose of unit testing.
 */
#include "talk/media/devices/v4llookup.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "webrtc/base/logging.h"

namespace cricket {

V4LLookup *V4LLookup::v4l_lookup_ = NULL;

bool V4LLookup::CheckIsV4L2Device(const std::string& device_path) {
  // check device major/minor numbers are in the range for video devices.
  struct stat s;

  if (lstat(device_path.c_str(), &s) != 0 || !S_ISCHR(s.st_mode)) return false;

  int video_fd = -1;
  bool is_v4l2 = false;

  // check major/minur device numbers are in range for video device
  if (major(s.st_rdev) == 81) {
    dev_t num = minor(s.st_rdev);
    if (num <= 63) {
      video_fd = ::open(device_path.c_str(), O_RDONLY | O_NONBLOCK);
      if ((video_fd >= 0) || (errno == EBUSY)) {
        ::v4l2_capability video_caps;
        memset(&video_caps, 0, sizeof(video_caps));

        if ((errno == EBUSY) ||
            (::ioctl(video_fd, VIDIOC_QUERYCAP, &video_caps) >= 0 &&
            (video_caps.capabilities & V4L2_CAP_VIDEO_CAPTURE))) {
          LOG(LS_INFO) << "Found V4L2 capture device " << device_path;

          is_v4l2 = true;
        } else {
          LOG_ERRNO(LS_ERROR) << "VIDIOC_QUERYCAP failed for " << device_path;
        }
      } else {
        LOG_ERRNO(LS_ERROR) << "Failed to open " << device_path;
      }
    }
  }

  if (video_fd >= 0)
    ::close(video_fd);

  return is_v4l2;
}

};  // namespace cricket
