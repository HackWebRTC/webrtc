/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_IVF_FILE_WRITER_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_IVF_FILE_WRITER_H_

#include <memory>
#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/video_frame.h"
#include "webrtc/system_wrappers/include/file_wrapper.h"

namespace webrtc {

class IvfFileWriter {
 public:
  ~IvfFileWriter();

  static std::unique_ptr<IvfFileWriter> Open(const std::string& file_name,
                                             VideoCodecType codec_type);
  bool WriteFrame(const EncodedImage& encoded_image);
  bool Close();

 private:
  IvfFileWriter(const std::string& path_name,
                std::unique_ptr<FileWrapper> file,
                VideoCodecType codec_type);
  bool WriteHeader();
  bool InitFromFirstFrame(const EncodedImage& encoded_image);

  const VideoCodecType codec_type_;
  size_t num_frames_;
  uint16_t width_;
  uint16_t height_;
  int64_t last_timestamp_;
  bool using_capture_timestamps_;
  rtc::TimestampWrapAroundHandler wrap_handler_;
  const std::string file_name_;
  std::unique_ptr<FileWrapper> file_;

  RTC_DISALLOW_COPY_AND_ASSIGN(IvfFileWriter);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_IVF_FILE_WRITER_H_
