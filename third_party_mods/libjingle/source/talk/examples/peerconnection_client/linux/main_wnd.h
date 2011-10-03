/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef PEERCONNECTION_SAMPLES_CLIENT_LINUX_MAIN_WND_H_
#define PEERCONNECTION_SAMPLES_CLIENT_LINUX_MAIN_WND_H_

#include "talk/examples/peerconnection_client/main_wnd.h"
#include "talk/examples/peerconnection_client/peer_connection_client.h"

// Forward declarations.
typedef struct _GtkWidget GtkWidget;
typedef union _GdkEvent GdkEvent;
typedef struct _GdkEventKey GdkEventKey;
typedef struct _GtkTreeView GtkTreeView;
typedef struct _GtkTreePath GtkTreePath;
typedef struct _GtkTreeViewColumn GtkTreeViewColumn;

// Implements the main UI of the peer connection client.
// This is functionally equivalent to the MainWnd class in the Windows
// implementation.
class GtkMainWnd : public MainWindow {
 public:
  GtkMainWnd();
  ~GtkMainWnd();

  virtual void RegisterObserver(MainWndCallback* callback);
  virtual bool IsWindow();
  virtual void SwitchToConnectUI();
  virtual void SwitchToPeerList(const Peers& peers);
  virtual void SwitchToStreamingUI();
  virtual void MessageBox(const char* caption, const char* text,
                          bool is_error);
  virtual MainWindow::UI current_ui();
  virtual cricket::VideoRenderer* local_renderer();
  virtual cricket::VideoRenderer* remote_renderer();
  virtual void QueueUIThreadCallback(int msg_id, void* data);

  // Creates and shows the main window with the |Connect UI| enabled.
  bool Create();

  // Destroys the window.  When the window is destroyed, it ends the
  // main message loop.
  bool Destroy();

  // Callback for when the main window is destroyed.
  void OnDestroyed(GtkWidget* widget, GdkEvent* event);

  // Callback for when the user clicks the "Connect" button.
  void OnClicked(GtkWidget* widget);

  // Callback for keystrokes.  Used to capture Esc and Return.
  void OnKeyPress(GtkWidget* widget, GdkEventKey* key);

  // Callback when the user double clicks a peer in order to initiate a
  // connection.
  void OnRowActivated(GtkTreeView* tree_view, GtkTreePath* path,
                      GtkTreeViewColumn* column);
                      
  void OnRedraw();

 protected:
  class VideoRenderer : public cricket::VideoRenderer {
   public:
    VideoRenderer(GtkMainWnd* main_wnd);
    virtual ~VideoRenderer();

    virtual bool SetSize(int width, int height, int reserved);

    virtual bool RenderFrame(const cricket::VideoFrame* frame);

    const uint8* image() const {
      return image_.get();
    }

    int width() const {
      return width_;
    }
    
    int height() const {
      return height_;
    }
    
   protected:
    talk_base::scoped_array<uint8> image_;
    int width_;
    int height_;
    GtkMainWnd* main_wnd_;
  };

 protected:
  GtkWidget* window_;  // Our main window.
  GtkWidget* draw_area_;  // The drawing surface for rendering video streams.
  GtkWidget* vbox_;  // Container for the Connect UI.
  GtkWidget* server_edit_;
  GtkWidget* port_edit_;
  GtkWidget* peer_list_;  // The list of peers.
  MainWndCallback* callback_;
  std::string server_;
  std::string port_;
  talk_base::scoped_ptr<VideoRenderer> local_renderer_;
  talk_base::scoped_ptr<VideoRenderer> remote_renderer_;
  talk_base::scoped_ptr<uint8> draw_buffer_;
  int draw_buffer_size_;
};

#endif  // PEERCONNECTION_SAMPLES_CLIENT_LINUX_MAIN_WND_H_
