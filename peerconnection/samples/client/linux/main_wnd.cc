/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "peerconnection/samples/client/linux/main_wnd.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stddef.h>

#include "talk/base/common.h"
#include "talk/base/logging.h"

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

}  // end anonymous

//
// GtkMainWnd implementation.
//

GtkMainWnd::GtkMainWnd()
    : window_(NULL), draw_area_(NULL), vbox_(NULL), peer_list_(NULL) {
}

GtkMainWnd::~GtkMainWnd() {
  ASSERT(!IsWindow());
}

bool GtkMainWnd::IsWindow() {
  return window_ != NULL && GTK_IS_WINDOW(window_);
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

  GtkWidget* hbox = gtk_hbox_new(FALSE, 3);

  GtkWidget* edit = gtk_entry_new();
  gtk_widget_set_size_request(edit, 400, 30);
  gtk_container_add(GTK_CONTAINER(hbox), edit);

  GtkWidget* button = gtk_button_new_with_label("Connect");
  gtk_widget_set_size_request(button, 70, 30);
  g_signal_connect(button, "clicked", G_CALLBACK(OnClickedCallback), this);
  gtk_container_add(GTK_CONTAINER(hbox), button);

  GtkWidget* halign = gtk_alignment_new(1, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(halign), hbox);
  gtk_box_pack_start(GTK_BOX(vbox_), halign, FALSE, FALSE, 0);

  gtk_widget_show_all(window_);
}

void GtkMainWnd::SwitchToPeerList(/*const Peers& peers*/) {
  gtk_container_set_border_width(GTK_CONTAINER(window_), 0);
  if (vbox_) {
    gtk_widget_destroy(vbox_);
    vbox_ = NULL;
  } else if (draw_area_) {
    gtk_widget_destroy(draw_area_);
    draw_area_ = NULL;
  }

  peer_list_ = gtk_tree_view_new();
  g_signal_connect(peer_list_, "row-activated",
                   G_CALLBACK(OnRowActivatedCallback), this);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(peer_list_), FALSE);
  InitializeList(peer_list_);
  AddToList(peer_list_, "item 1", 1);
  AddToList(peer_list_, "item 2", 2);
  AddToList(peer_list_, "item 3", 3);
  gtk_container_add(GTK_CONTAINER(window_), peer_list_);

  gtk_widget_show_all(window_);
}

void GtkMainWnd::SwitchToStreamingUI() {
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
  gtk_main_quit();
}

void GtkMainWnd::OnClicked(GtkWidget* widget) {
  g_print("Clicked!\n");
  SwitchToPeerList();
}

void GtkMainWnd::OnKeyPress(GtkWidget* widget, GdkEventKey* key) {
  g_print("KeyPress!\n");
  if (key->type == GDK_KEY_PRESS) {
    g_print("0x%08X\n", key->keyval);
    switch (key->keyval) {
     case GDK_Escape :
       if (draw_area_) {
         SwitchToPeerList();
       } else if (peer_list_) {
         SwitchToConnectUI();
       } else {
         gtk_main_quit();
       }
       break;

     case GDK_KP_Enter:
     case GDK_Return:
       // TODO(tommi): Use g_idle_add() if we need to switch asynchronously.
       if (vbox_) {
         SwitchToPeerList();
       } else if (peer_list_) {
         SwitchToStreamingUI();
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
     int id = 0;
     gtk_tree_model_get(model, &iter, 0, &text, 1, &id,  -1);
     g_print("%s - %i\n", text, id);
     g_free(text);
   }
}
