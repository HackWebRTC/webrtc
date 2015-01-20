/*
 * libjingle
 * Copyright 2012 Google Inc.
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

// Author: thorcarpenter@google.com (Thor Carpenter)
//
// Defines variant class ScreencastId that combines WindowId and DesktopId.

#ifndef TALK_MEDIA_BASE_SCREENCASTID_H_
#define TALK_MEDIA_BASE_SCREENCASTID_H_

#include <string>
#include <vector>

#include "webrtc/base/window.h"
#include "webrtc/base/windowpicker.h"

namespace cricket {

class ScreencastId;
typedef std::vector<ScreencastId> ScreencastIdList;

// Used for identifying a window or desktop to be screencast.
class ScreencastId {
 public:
  enum Type { INVALID, WINDOW, DESKTOP };

  // Default constructor indicates invalid ScreencastId.
  ScreencastId() : type_(INVALID) {}
  explicit ScreencastId(const rtc::WindowId& id)
      : type_(WINDOW), window_(id) {
  }
  explicit ScreencastId(const rtc::DesktopId& id)
      : type_(DESKTOP), desktop_(id) {
  }

  Type type() const { return type_; }
  const rtc::WindowId& window() const { return window_; }
  const rtc::DesktopId& desktop() const { return desktop_; }

  // Title is an optional parameter.
  const std::string& title() const { return title_; }
  void set_title(const std::string& desc) { title_ = desc; }

  bool IsValid() const {
    if (type_ == INVALID) {
      return false;
    } else if (type_ == WINDOW) {
      return window_.IsValid();
    } else {
      return desktop_.IsValid();
    }
  }
  bool IsWindow() const { return type_ == WINDOW; }
  bool IsDesktop() const { return type_ == DESKTOP; }
  bool EqualsId(const ScreencastId& other) const {
    if (type_ != other.type_) {
      return false;
    }
    if (type_ == INVALID) {
      return true;
    } else if (type_ == WINDOW) {
      return window_.Equals(other.window());
    }
    return desktop_.Equals(other.desktop());
  }

  // T is assumed to be WindowDescription or DesktopDescription.
  template<class T>
  static cricket::ScreencastIdList Convert(const std::vector<T>& list) {
    ScreencastIdList screencast_list;
    screencast_list.reserve(list.size());
    for (typename std::vector<T>::const_iterator it = list.begin();
         it != list.end(); ++it) {
      ScreencastId id(it->id());
      id.set_title(it->title());
      screencast_list.push_back(id);
    }
    return screencast_list;
  }

 private:
  Type type_;
  rtc::WindowId window_;
  rtc::DesktopId desktop_;
  std::string title_;  // Optional.
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_SCREENCASTID_H_
