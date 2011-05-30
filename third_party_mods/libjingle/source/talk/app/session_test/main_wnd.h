// Copyright 2011 Google Inc. All Rights Reserved.
// Author: tommi@google.com (Tomas Gunnarsson)


#ifndef TALK_APP_SESSION_TEST_MAIN_WND_H_
#define TALK_APP_SESSION_TEST_MAIN_WND_H_
#pragma once

#include "talk/base/win32.h"

#include <map>

// TODO(tommi): Move to same header as PeerConnectionClient.
typedef std::map<int, std::string> Peers;


class MainWndCallback {
 public:
  virtual void StartLogin(const std::string& server, int port) = 0;
  virtual void DisconnectFromServer() = 0;
  virtual void ConnectToPeer(int peer_id) = 0;
  virtual void DisconnectFromCurrentPeer() = 0;
};

class MainWnd {
 public:
  static const wchar_t kClassName[];

  enum UI {
    CONNECT_TO_SERVER,
    LIST_PEERS,
    STREAMING,
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

#endif  // TALK_APP_SESSION_TEST_MAIN_WND_H_
