// Copyright 2010 Google Inc. All Rights Reserved

//         thorcarpenter@google.com (Thor Carpenter)

#ifndef TALK_BASE_WINDOWPICKER_H_
#define TALK_BASE_WINDOWPICKER_H_

#include <string>
#include <vector>

#include "talk/base/window.h"

namespace talk_base {

class WindowDescription {
 public:
  WindowDescription() : id_() {}
  WindowDescription(const WindowId& id, const std::string& title)
      : id_(id), title_(title) {
  }
  const WindowId& id() const { return id_; }
  void set_id(const WindowId& id) { id_ = id; }
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

 private:
  WindowId id_;
  std::string title_;
};

class DesktopDescription {
 public:
  DesktopDescription() : id_() {}
  DesktopDescription(const DesktopId& id, const std::string& title)
      : id_(id), title_(title), primary_(false) {
  }
  const DesktopId& id() const { return id_; }
  void set_id(const DesktopId& id) { id_ = id; }
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }
  // Indicates whether it is the primary desktop in the system.
  bool primary() const { return primary_; }
  void set_primary(bool primary) { primary_ = primary; }

 private:
  DesktopId id_;
  std::string title_;
  bool primary_;
};

typedef std::vector<WindowDescription> WindowDescriptionList;
typedef std::vector<DesktopDescription> DesktopDescriptionList;

class WindowPicker {
 public:
  virtual ~WindowPicker() {}
  virtual bool Init() = 0;

  // TODO: Move this two methods to window.h when we no longer need to load
  // CoreGraphics dynamically.
  virtual bool IsVisible(const WindowId& id) = 0;
  virtual bool MoveToFront(const WindowId& id) = 0;

  // Gets a list of window description and appends to descriptions.
  // Returns true if successful.
  virtual bool GetWindowList(WindowDescriptionList* descriptions) = 0;
  // Gets a list of desktop descriptions and appends to descriptions.
  // Returns true if successful.
  virtual bool GetDesktopList(DesktopDescriptionList* descriptions) = 0;
  // Gets the width and height of a desktop.
  // Returns true if successful.
  virtual bool GetDesktopDimensions(const DesktopId& id, int* width,
                                    int* height) = 0;
};

}  // namespace talk_base

#endif  // TALK_BASE_WINDOWPICKER_H_
