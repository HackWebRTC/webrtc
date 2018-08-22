/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_TOOLS_Y4M_FILE_READER_H_
#define RTC_TOOLS_Y4M_FILE_READER_H_

#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#include "api/video/video_frame.h"
#include "rtc_base/refcount.h"
#include "rtc_base/sequenced_task_checker.h"

namespace webrtc {
namespace test {

// Iterable class representing a sequence of I420 buffers. This class is not
// thread safe because it is expected to be backed by a file.
class Video : public rtc::RefCountInterface {
 public:
  class Iterator {
   public:
    typedef int value_type;
    typedef std::ptrdiff_t difference_type;
    typedef int* pointer;
    typedef int& reference;
    typedef std::input_iterator_tag iterator_category;

    Iterator(const rtc::scoped_refptr<const Video>& video, size_t index);
    Iterator(const Iterator& other);
    Iterator(Iterator&& other);
    Iterator& operator=(Iterator&&);
    Iterator& operator=(const Iterator&);
    ~Iterator();

    rtc::scoped_refptr<I420BufferInterface> operator*() const;
    bool operator==(const Iterator& other) const;
    bool operator!=(const Iterator& other) const;

    Iterator operator++(int);
    Iterator& operator++();

   private:
    rtc::scoped_refptr<const Video> video_;
    size_t index_;
  };

  Iterator begin() const;
  Iterator end() const;

  virtual size_t number_of_frames() const = 0;
  virtual rtc::scoped_refptr<I420BufferInterface> GetFrame(
      size_t index) const = 0;
};

class Y4mFile : public Video {
 public:
  // This function opens the file and reads it as an .y4m file. It returns null
  // on failure. The file will be closed when the returned object is destroyed.
  static rtc::scoped_refptr<Y4mFile> Open(const std::string& file_name);

  size_t number_of_frames() const override;

  rtc::scoped_refptr<I420BufferInterface> GetFrame(
      size_t frame_index) const override;

  int width() const;
  int height() const;
  float fps() const;

 protected:
  Y4mFile(int width,
          int height,
          float fps,
          const std::vector<fpos_t>& frame_positions,
          FILE* file);
  ~Y4mFile() override;

 private:
  const int width_;
  const int height_;
  const float fps_;
  const std::vector<fpos_t> frame_positions_;
  const rtc::SequencedTaskChecker thread_checker_;
  FILE* const file_;
};

}  // namespace test
}  // namespace webrtc

#endif  // RTC_TOOLS_Y4M_FILE_READER_H_
