#ifndef XAPP_GTK_WINDOW_H
#define XAPP_GTK_WINDOW_H
#include <gtk/gtk.h>
#define XAPP_TYPE_GTK_WINDOW GTK_TYPE_WINDOW
#define XAPP_GTK_WINDOW(o) GTK_WINDOW(o)
#define xapp_gtk_window_new(type) gtk_window_new ()
#define xapp_gtk_window_set_icon_name(w, n) gtk_window_set_icon_name (GTK_WINDOW (w), n)
#define xapp_gtk_window_set_progress(w, p) ((void)0)
#endif
