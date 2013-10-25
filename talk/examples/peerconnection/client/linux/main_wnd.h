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


#ifndef PEERCONNECTION_SAMPLES_CLIENT_LINUX_MAIN_WND_H_
#define PEERCONNECTION_SAMPLES_CLIENT_LINUX_MAIN_WND_H_

#include "talk/examples/peerconnection/client/main_wnd.h"
#include "talk/examples/peerconnection/client/peer_connection_client.h"

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
  GtkMainWnd(const char* server, int port, bool autoconnect, bool autocall);
  ~GtkMainWnd();

  virtual void RegisterObserver(MainWndCallback* callback);
  virtual bool IsWindow();
  virtual void SwitchToConnectUI();
  virtual void SwitchToPeerList(const Peers& peers);
  virtual void SwitchToStreamingUI();
  virtual void MessageBox(const char* caption, const char* text,
                          bool is_error);
  virtual MainWindow::UI current_ui();
  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);
  virtual void StopLocalRenderer();
  virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);
  virtual void StopRemoteRenderer();

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
  class VideoRenderer : public webrtc::VideoRendererInterface {
   public:
    VideoRenderer(GtkMainWnd* main_wnd,
                  webrtc::VideoTrackInterface* track_to_render);
    virtual ~VideoRenderer();

    // VideoRendererInterface implementation
    virtual void SetSize(int width, int height);
    virtual void RenderFrame(const cricket::VideoFrame* frame);

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
    talk_base::scoped_ptr<uint8[]> image_;
    int width_;
    int height_;
    GtkMainWnd* main_wnd_;
    talk_base::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
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
  bool autoconnect_;
  bool autocall_;
  talk_base::scoped_ptr<VideoRenderer> local_renderer_;
  talk_base::scoped_ptr<VideoRenderer> remote_renderer_;
  talk_base::scoped_ptr<uint8> draw_buffer_;
  int draw_buffer_size_;
};

#endif  // PEERCONNECTION_SAMPLES_CLIENT_LINUX_MAIN_WND_H_
