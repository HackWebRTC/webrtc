/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PEERCONNECTION_SAMPLES_CLIENT_MAIN_WND_H_
#define PEERCONNECTION_SAMPLES_CLIENT_MAIN_WND_H_
#pragma once

#include <map>
#include <string>

#include "peerconnection/samples/client/peer_connection_client.h"
#include "talk/base/win32.h"
#include "talk/session/phone/mediachannel.h"
#include "talk/session/phone/videocommon.h"
#include "talk/session/phone/videoframe.h"
#include "talk/session/phone/videorenderer.h"

class MainWndCallback {
 public:
  virtual bool StartLogin(const std::string& server, int port) = 0;
  virtual void DisconnectFromServer() = 0;
  virtual void ConnectToPeer(int peer_id) = 0;
  virtual void DisconnectFromCurrentPeer() = 0;
  virtual void UIThreadCallback(int msg_id, void* data) = 0;
  virtual void Close() = 0;
 protected:
  virtual ~MainWndCallback() {}
};

// Pure virtual interface for the main window.
class MainWindow {
 public:
  virtual ~MainWindow() {}

  enum UI {
    CONNECT_TO_SERVER,
    LIST_PEERS,
    STREAMING,
  };

  virtual void RegisterObserver(MainWndCallback* callback) = 0;

  virtual bool IsWindow() = 0;
  virtual void MessageBox(const char* caption, const char* text,
                          bool is_error) = 0;

  virtual UI current_ui() = 0;

  virtual void SwitchToConnectUI() = 0;
  virtual void SwitchToPeerList(const Peers& peers) = 0;
  virtual void SwitchToStreamingUI() = 0;

  virtual cricket::VideoRenderer* local_renderer() = 0;
  virtual cricket::VideoRenderer* remote_renderer() = 0;

  virtual void QueueUIThreadCallback(int msg_id, void* data) = 0;
};

#ifdef WIN32

class MainWnd : public MainWindow {
 public:
  static const wchar_t kClassName[];

  enum WindowMessages {
    UI_THREAD_CALLBACK = WM_APP + 1,
  };

  MainWnd();
  ~MainWnd();

  bool Create();
  bool Destroy();
  bool PreTranslateMessage(MSG* msg);

  virtual void RegisterObserver(MainWndCallback* callback);
  virtual bool IsWindow();
  virtual void SwitchToConnectUI();
  virtual void SwitchToPeerList(const Peers& peers);
  virtual void SwitchToStreamingUI();
  virtual void MessageBox(const char* caption, const char* text,
                          bool is_error);
  virtual UI current_ui() { return ui_; }

  virtual cricket::VideoRenderer* local_renderer();
  virtual cricket::VideoRenderer* remote_renderer();

  virtual void QueueUIThreadCallback(int msg_id, void* data);

  HWND handle() const { return wnd_; }

  class VideoRenderer : public cricket::VideoRenderer {
   public:
    VideoRenderer(HWND wnd, int width, int height);
    virtual ~VideoRenderer();

    void Lock() {
      ::EnterCriticalSection(&buffer_lock_);
    }

    void Unlock() {
      ::LeaveCriticalSection(&buffer_lock_);
    }

    virtual bool SetSize(int width, int height, int reserved);

    // Called when a new frame is available for display.
    virtual bool RenderFrame(const cricket::VideoFrame* frame);

    const BITMAPINFO& bmi() const { return bmi_; }
    const uint8* image() const { return image_.get(); }

   protected:
    enum {
      SET_SIZE,
      RENDER_FRAME,
    };

    HWND wnd_;
    BITMAPINFO bmi_;
    talk_base::scoped_array<uint8> image_;
    CRITICAL_SECTION buffer_lock_;
  };

  // A little helper class to make sure we always to proper locking and
  // unlocking when working with VideoRenderer buffers.
  template <typename T>
  class AutoLock {
   public:
    explicit AutoLock(T* obj) : obj_(obj) { obj_->Lock(); }
    ~AutoLock() { obj_->Unlock(); }
   protected:
    T* obj_;
  };

 protected:
  enum ChildWindowID {
    EDIT_ID = 1,
    BUTTON_ID,
    LABEL1_ID,
    LABEL2_ID,
    LISTBOX_ID,
  };

  void OnPaint();
  void OnDestroyed();

  void OnDefaultAction();

  bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result);

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static bool RegisterWindowClass();

  void CreateChildWindow(HWND* wnd, ChildWindowID id, const wchar_t* class_name,
                         DWORD control_style, DWORD ex_style);
  void CreateChildWindows();

  void LayoutConnectUI(bool show);
  void LayoutPeerListUI(bool show);

  void HandleTabbing();

 private:
  talk_base::scoped_ptr<VideoRenderer> remote_video_;
  talk_base::scoped_ptr<VideoRenderer> local_video_;
  UI ui_;
  HWND wnd_;
  DWORD ui_thread_id_;
  HWND edit1_;
  HWND edit2_;
  HWND label1_;
  HWND label2_;
  HWND button_;
  HWND listbox_;
  bool destroyed_;
  void* nested_msg_;
  MainWndCallback* callback_;
  static ATOM wnd_class_;
};
#endif  // WIN32

#endif  // PEERCONNECTION_SAMPLES_CLIENT_MAIN_WND_H_
