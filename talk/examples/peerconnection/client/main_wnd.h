/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#ifndef PEERCONNECTION_SAMPLES_CLIENT_MAIN_WND_H_
#define PEERCONNECTION_SAMPLES_CLIENT_MAIN_WND_H_
#pragma once

#include <map>
#include <string>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/base/win32.h"
#include "talk/examples/peerconnection/client/peer_connection_client.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/base/videoframe.h"
#include "talk/media/base/videorenderer.h"

class MainWndCallback {
 public:
  virtual void StartLogin(const std::string& server, int port) = 0;
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

  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video) = 0;
  virtual void StopLocalRenderer() = 0;
  virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video) = 0;
  virtual void StopRemoteRenderer() = 0;

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

  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);
  virtual void StopLocalRenderer();
  virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);
  virtual void StopRemoteRenderer();

  virtual void QueueUIThreadCallback(int msg_id, void* data);

  HWND handle() const { return wnd_; }

  class VideoRenderer : public webrtc::VideoRendererInterface {
   public:
    VideoRenderer(HWND wnd, int width, int height,
                  webrtc::VideoTrackInterface* track_to_render);
    virtual ~VideoRenderer();

    void Lock() {
      ::EnterCriticalSection(&buffer_lock_);
    }

    void Unlock() {
      ::LeaveCriticalSection(&buffer_lock_);
    }

    // VideoRendererInterface implementation
    virtual void SetSize(int width, int height);
    virtual void RenderFrame(const cricket::VideoFrame* frame);

    const BITMAPINFO& bmi() const { return bmi_; }
    const uint8* image() const { return image_.get(); }

   protected:
    enum {
      SET_SIZE,
      RENDER_FRAME,
    };

    HWND wnd_;
    BITMAPINFO bmi_;
    talk_base::scoped_ptr<uint8[]> image_;
    CRITICAL_SECTION buffer_lock_;
    talk_base::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
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
  talk_base::scoped_ptr<VideoRenderer> local_renderer_;
  talk_base::scoped_ptr<VideoRenderer> remote_renderer_;
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
