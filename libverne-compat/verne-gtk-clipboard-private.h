#ifndef VERNE_GTK_CLIPBOARD_PRIVATE_H
#define VERNE_GTK_CLIPBOARD_PRIVATE_H

#include "verne-gtk-compat.h"

struct _GtkClipboard {
	GObject parent;
	GdkClipboard *gdk;
	GObject *owner;
	GtkClipboardGetFunc get_func;
	GtkClipboardClearFunc clear_func;
	gpointer user_data;
	GtkTargetEntry *targets;
	guint n_targets;
};

struct _GtkTargetList {
	guint ref;
	GArray *entries;
};

void verne_clipboard_install_content (GtkClipboard *clipboard);

#endif
