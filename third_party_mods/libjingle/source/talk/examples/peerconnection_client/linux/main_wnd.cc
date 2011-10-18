/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "talk/examples/peerconnection_client/linux/main_wnd.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stddef.h>

#include "talk/examples/peerconnection_client/defaults.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/base/stringutils.h"

using talk_base::sprintfn;

namespace {

//
// Simple static functions that simply forward the callback to the
// GtkMainWnd instance.
//

gboolean OnDestroyedCallback(GtkWidget* widget, GdkEvent* event,
                             gpointer data) {
  reinterpret_cast<GtkMainWnd*>(data)->OnDestroyed(widget, event);
  return FALSE;
}

void OnClickedCallback(GtkWidget* widget, gpointer data) {
  reinterpret_cast<GtkMainWnd*>(data)->OnClicked(widget);
}

gboolean OnKeyPressCallback(GtkWidget* widget, GdkEventKey* key,
                            gpointer data) {
  reinterpret_cast<GtkMainWnd*>(data)->OnKeyPress(widget, key);
  return false;
}

void OnRowActivatedCallback(GtkTreeView* tree_view, GtkTreePath* path,
                            GtkTreeViewColumn* column, gpointer data) {
  reinterpret_cast<GtkMainWnd*>(data)->OnRowActivated(tree_view, path, column);
}

// Creates a tree view, that we use to display the list of peers.
void InitializeList(GtkWidget* list) {
  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(
      "List Items", renderer, "text", 0, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
  GtkListStore* store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
  gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(store));
  g_object_unref(store);
}

// Adds an entry to a tree view.
void AddToList(GtkWidget* list, const gchar* str, int value) {
  GtkListStore* store = GTK_LIST_STORE(
      gtk_tree_view_get_model(GTK_TREE_VIEW(list)));

  GtkTreeIter iter;
  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter, 0, str, 1, value, -1);
}

struct UIThreadCallbackData {
  explicit UIThreadCallbackData(MainWndCallback* cb, int id, void* d)
      : callback(cb), msg_id(id), data(d) {}
  MainWndCallback* callback;
  int msg_id;
  void* data;
};

gboolean HandleUIThreadCallback(gpointer data) {
  UIThreadCallbackData* cb_data = reinterpret_cast<UIThreadCallbackData*>(data);
  cb_data->callback->UIThreadCallback(cb_data->msg_id, cb_data->data);
  delete cb_data;
  return false;
}

gboolean Redraw(gpointer data) {
  GtkMainWnd* wnd = reinterpret_cast<GtkMainWnd*>(data);
  wnd->OnRedraw();
  return false;
}
}  // end anonymous

//
// GtkMainWnd implementation.
//

GtkMainWnd::GtkMainWnd()
    : window_(NULL), draw_area_(NULL), vbox_(NULL), server_edit_(NULL),
      port_edit_(NULL), peer_list_(NULL), callback_(NULL),
      server_("localhost") {
  char buffer[10];
  sprintfn(buffer, sizeof(buffer), "%i", kDefaultServerPort);
  port_ = buffer;
}

GtkMainWnd::~GtkMainWnd() {
  ASSERT(!IsWindow());
}

void GtkMainWnd::RegisterObserver(MainWndCallback* callback) {
  callback_ = callback;
}

bool GtkMainWnd::IsWindow() {
  return window_ != NULL && GTK_IS_WINDOW(window_);
}

void GtkMainWnd::MessageBox(const char* caption, const char* text,
                            bool is_error) {
  GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(window_),
      GTK_DIALOG_DESTROY_WITH_PARENT,
      is_error ? GTK_MESSAGE_ERROR : GTK_MESSAGE_INFO,
      GTK_BUTTONS_CLOSE, "%s", text);
  gtk_window_set_title(GTK_WINDOW(dialog), caption);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

MainWindow::UI GtkMainWnd::current_ui() {
  if (vbox_)
    return CONNECT_TO_SERVER;

  if (peer_list_)
    return LIST_PEERS;

  return STREAMING;
}

webrtc::VideoRendererWrapperInterface* GtkMainWnd::local_renderer() {
  if (!local_renderer_wrapper_.get())
    local_renderer_wrapper_  =
        webrtc::CreateVideoRenderer(new VideoRenderer(this));
  return local_renderer_wrapper_.get();
}

webrtc::VideoRendererWrapperInterface*  GtkMainWnd::remote_renderer() {
  if (!remote_renderer_wrapper_.get())
    remote_renderer_wrapper_ =
        webrtc::CreateVideoRenderer(new VideoRenderer(this));
  return remote_renderer_wrapper_.get();
}

void GtkMainWnd::QueueUIThreadCallback(int msg_id, void* data) {
  g_idle_add(HandleUIThreadCallback,
             new UIThreadCallbackData(callback_, msg_id, data));
}

bool GtkMainWnd::Create() {
  ASSERT(window_ == NULL);

  window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (window_) {
    gtk_window_set_position(GTK_WINDOW(window_), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window_), 640, 480);
    gtk_window_set_title(GTK_WINDOW(window_), "PeerConnection client");
    g_signal_connect(G_OBJECT(window_), "delete-event",
                     G_CALLBACK(&OnDestroyedCallback), this);
    g_signal_connect(window_, "key-press-event", G_CALLBACK(OnKeyPressCallback),
                     this);

    SwitchToConnectUI();
  }

  return window_ != NULL;
}

bool GtkMainWnd::Destroy() {
  if (!IsWindow())
    return false;

  gtk_widget_destroy(window_);
  window_ = NULL;

  return true;
}

void GtkMainWnd::SwitchToConnectUI() {
  LOG(INFO) << __FUNCTION__;

  ASSERT(IsWindow());
  ASSERT(vbox_ == NULL);

  gtk_container_set_border_width(GTK_CONTAINER(window_), 10);

  if (peer_list_) {
    gtk_widget_destroy(peer_list_);
    peer_list_ = NULL;
  }

  vbox_ = gtk_vbox_new(FALSE, 5);
  GtkWidget* valign = gtk_alignment_new(0, 1, 0, 0);
  gtk_container_add(GTK_CONTAINER(vbox_), valign);
  gtk_container_add(GTK_CONTAINER(window_), vbox_);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 5);

  GtkWidget* label = gtk_label_new("Server");
  gtk_container_add(GTK_CONTAINER(hbox), label);

  server_edit_ = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(server_edit_), server_.c_str());
  gtk_widget_set_size_request(server_edit_, 400, 30);
  gtk_container_add(GTK_CONTAINER(hbox), server_edit_);

  port_edit_ = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(port_edit_), port_.c_str());
  gtk_widget_set_size_request(port_edit_, 70, 30);
  gtk_container_add(GTK_CONTAINER(hbox), port_edit_);

  GtkWidget* button = gtk_button_new_with_label("Connect");
  gtk_widget_set_size_request(button, 70, 30);
  g_signal_connect(button, "clicked", G_CALLBACK(OnClickedCallback), this);
  gtk_container_add(GTK_CONTAINER(hbox), button);

  GtkWidget* halign = gtk_alignment_new(1, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(halign), hbox);
  gtk_box_pack_start(GTK_BOX(vbox_), halign, FALSE, FALSE, 0);

  gtk_widget_show_all(window_);
}

void GtkMainWnd::SwitchToPeerList(const Peers& peers) {
  LOG(INFO) << __FUNCTION__;

  // Clean up buffers from a potential previous session.
  local_renderer_wrapper_ = NULL;
  remote_renderer_wrapper_ = NULL;

  if (!peer_list_) {
    gtk_container_set_border_width(GTK_CONTAINER(window_), 0);
    if (vbox_) {
      gtk_widget_destroy(vbox_);
      vbox_ = NULL;
      server_edit_ = NULL;
      port_edit_ = NULL;
    } else if (draw_area_) {
      gtk_widget_destroy(draw_area_);
      draw_area_ = NULL;
      draw_buffer_.reset();
    }

    peer_list_ = gtk_tree_view_new();
    g_signal_connect(peer_list_, "row-activated",
                     G_CALLBACK(OnRowActivatedCallback), this);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(peer_list_), FALSE);
    InitializeList(peer_list_);
    gtk_container_add(GTK_CONTAINER(window_), peer_list_);
    gtk_widget_show_all(window_);
  } else {
    GtkListStore* store =
        GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(peer_list_)));
    gtk_list_store_clear(store);
  }

  AddToList(peer_list_, "List of currently connected peers:", -1);
  for (Peers::const_iterator i = peers.begin(); i != peers.end(); ++i)
    AddToList(peer_list_, i->second.c_str(), i->first);
}

void GtkMainWnd::SwitchToStreamingUI() {
  LOG(INFO) << __FUNCTION__;

  ASSERT(draw_area_ == NULL);

  gtk_container_set_border_width(GTK_CONTAINER(window_), 0);
  if (peer_list_) {
    gtk_widget_destroy(peer_list_);
    peer_list_ = NULL;
  }

  draw_area_ = gtk_drawing_area_new();
  gtk_container_add(GTK_CONTAINER(window_), draw_area_);

  gtk_widget_show_all(window_);
}

void GtkMainWnd::OnDestroyed(GtkWidget* widget, GdkEvent* event) {
  callback_->Close();
  window_ = NULL;
  draw_area_ = NULL;
  vbox_ = NULL;
  server_edit_ = NULL;
  port_edit_ = NULL;
  peer_list_ = NULL;
}

void GtkMainWnd::OnClicked(GtkWidget* widget) {
  server_ = gtk_entry_get_text(GTK_ENTRY(server_edit_));
  port_ = gtk_entry_get_text(GTK_ENTRY(port_edit_));
  int port = port_.length() ? atoi(port_.c_str()) : 0;
  callback_->StartLogin(server_, port);
}

void GtkMainWnd::OnKeyPress(GtkWidget* widget, GdkEventKey* key) {
  if (key->type == GDK_KEY_PRESS) {
    switch (key->keyval) {
     case GDK_Escape:
       if (draw_area_) {
         callback_->DisconnectFromCurrentPeer();
       } else if (peer_list_) {
         callback_->DisconnectFromServer();
       }
       break;

     case GDK_KP_Enter:
     case GDK_Return:
       if (vbox_) {
         OnClicked(NULL);
       } else if (peer_list_) {
         // OnRowActivated will be called automatically when the user
         // presses enter.
       }
       break;

     default:
       break;
    }
  }
}

void GtkMainWnd::OnRowActivated(GtkTreeView* tree_view, GtkTreePath* path,
                                GtkTreeViewColumn* column) {
  ASSERT(peer_list_ != NULL);
  GtkTreeIter iter;
  GtkTreeModel* model;
  GtkTreeSelection* selection =
      gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
  if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
     char* text;
     int id = -1;
     gtk_tree_model_get(model, &iter, 0, &text, 1, &id,  -1);
     if (id != -1)
       callback_->ConnectToPeer(id);
     g_free(text);
  }
}

void GtkMainWnd::OnRedraw() {
  gdk_threads_enter();

  VideoRenderer* remote_renderer =
      static_cast<VideoRenderer*>(remote_renderer_wrapper_->renderer());
  if (remote_renderer && remote_renderer->image() != NULL &&
      draw_area_ != NULL) {
    int width = remote_renderer->width();
    int height = remote_renderer->height();

    if (!draw_buffer_.get()) {
      draw_buffer_size_ = (width * height * 4) * 4;
      draw_buffer_.reset(new uint8[draw_buffer_size_]);
      gtk_widget_set_size_request(draw_area_, width * 2, height * 2);
    }

    const uint32* image = reinterpret_cast<const uint32*>(
        remote_renderer->image());
    uint32* scaled = reinterpret_cast<uint32*>(draw_buffer_.get());
    for (int r = 0; r < height; ++r) {
      for (int c = 0; c < width; ++c) {
        int x = c * 2;
        scaled[x] = scaled[x + 1] = image[c];
      }

      uint32* prev_line = scaled;
      scaled += width * 2;
      memcpy(scaled, prev_line, (width * 2) * 4);

      image += width;
      scaled += width * 2;
    }

    VideoRenderer* local_renderer =
        static_cast<VideoRenderer*>(local_renderer_wrapper_->renderer());
    if (local_renderer && local_renderer->image()) {
      image = reinterpret_cast<const uint32*>(local_renderer->image());
      scaled = reinterpret_cast<uint32*>(draw_buffer_.get());
      // Position the local preview on the right side.
      scaled += (width * 2) - (local_renderer->width() / 2);
      // right margin...
      scaled -= 10;
      // ... towards the bottom.
      scaled += (height * width * 4) -
                ((local_renderer->height() / 2) *
                 (local_renderer->width() / 2) * 4);
      // bottom margin...
      scaled -= (width * 2) * 5;
      for (int r = 0; r < local_renderer->height(); r += 2) {
        for (int c = 0; c < local_renderer->width(); c += 2) {
          scaled[c / 2] = image[c + r * local_renderer->width()];
        }
        scaled += width * 2;
      }
    }

    gdk_draw_rgb_32_image(draw_area_->window,
                          draw_area_->style->fg_gc[GTK_STATE_NORMAL],
                          0,
                          0,
                          width * 2,
                          height * 2,
                          GDK_RGB_DITHER_MAX,
                          draw_buffer_.get(),
                          (width * 2) * 4);
  }

  gdk_threads_leave();
}

GtkMainWnd::VideoRenderer::VideoRenderer(GtkMainWnd* main_wnd)
    : width_(0), height_(0), main_wnd_(main_wnd) {
}

GtkMainWnd::VideoRenderer::~VideoRenderer() {
}

bool GtkMainWnd::VideoRenderer::SetSize(int width, int height, int reserved) {
  gdk_threads_enter();
  width_ = width;
  height_ = height;
  image_.reset(new uint8[width * height * 4]);
  gdk_threads_leave();
  return true;
}

bool GtkMainWnd::VideoRenderer::RenderFrame(const cricket::VideoFrame* frame) {
  gdk_threads_enter();

  int size = width_ * height_ * 4;
  frame->ConvertToRgbBuffer(cricket::FOURCC_ARGB,
                            image_.get(),
                            size,
                            width_ * 4);
  // Convert the B,G,R,A frame to R,G,B,A, which is accepted by GTK.
  // The 'A' is just padding for GTK, so we can use it as temp.
  uint8* pix = image_.get();
  uint8* end = image_.get() + size;
  while (pix < end) {
    pix[3] = pix[0];     // Save B to A.
    pix[0] = pix[2];  // Set Red.
    pix[2] = pix[3];  // Set Blue.
    pix[3] = 0xFF;     // Fixed Alpha.
    pix += 4;
  }

  gdk_threads_leave();

  g_idle_add(Redraw, main_wnd_);

  return true;
}


