// Copyright 2012 Google Inc.
// Author: thorcarpenter@google.com (Thor Carpenter)
//
// Defines variant class ScreencastId that combines WindowId and DesktopId.

#ifndef TALK_MEDIA_BASE_SCREENCASTID_H_
#define TALK_MEDIA_BASE_SCREENCASTID_H_

#include <string>
#include <vector>

#include "talk/base/window.h"
#include "talk/base/windowpicker.h"

namespace cricket {

class ScreencastId;
typedef std::vector<ScreencastId> ScreencastIdList;

// Used for identifying a window or desktop to be screencast.
class ScreencastId {
 public:
  enum Type { INVALID, WINDOW, DESKTOP };

  // Default constructor indicates invalid ScreencastId.
  ScreencastId() : type_(INVALID) {}
  explicit ScreencastId(const talk_base::WindowId& id)
      : type_(WINDOW), window_(id) {
  }
  explicit ScreencastId(const talk_base::DesktopId& id)
      : type_(DESKTOP), desktop_(id) {
  }

  Type type() const { return type_; }
  const talk_base::WindowId& window() const { return window_; }
  const talk_base::DesktopId& desktop() const { return desktop_; }

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
  talk_base::WindowId window_;
  talk_base::DesktopId desktop_;
  std::string title_;  // Optional.
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_SCREENCASTID_H_
