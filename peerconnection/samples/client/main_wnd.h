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

class MainWndCallback {
 public:
  virtual void StartLogin(const std::string& server, int port) = 0;
  virtual void DisconnectFromServer() = 0;
  virtual void ConnectToPeer(int peer_id) = 0;
  virtual void DisconnectFromCurrentPeer() = 0;
 protected:
  virtual ~MainWndCallback() {}
};

class MainWnd {
 public:
  static const wchar_t kClassName[];

  enum UI {
    CONNECT_TO_SERVER,
    LIST_PEERS,
    STREAMING,
  };

  enum WindowMessages {
    VIDEO_RENDERER_MESSAGE = WM_APP + 1,
  };

  MainWnd();
  ~MainWnd();

  bool Create();
  bool Destroy();
  bool IsWindow() const;

  void RegisterObserver(MainWndCallback* callback);

  bool PreTranslateMessage(MSG* msg);

  void SwitchToConnectUI();
  void SwitchToPeerList(const Peers& peers);
  void SwitchToStreamingUI();

  HWND handle() const { return wnd_; }
  UI current_ui() const { return ui_; }

  cricket::VideoRenderer* local_renderer() const {
    return local_video_.get();
  }

  cricket::VideoRenderer* remote_renderer() const {
    return remote_video_.get();
  }

  class VideoRenderer : public cricket::VideoRenderer {
   public:
    VideoRenderer(HWND wnd, int width, int height);
    virtual ~VideoRenderer();

    virtual bool SetSize(int width, int height, int reserved);

    // Called when a new frame is available for display.
    virtual bool RenderFrame(const cricket::VideoFrame* frame);

    void OnMessage(const MSG& msg);

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

#endif  // PEERCONNECTION_SAMPLES_CLIENT_MAIN_WND_H_
