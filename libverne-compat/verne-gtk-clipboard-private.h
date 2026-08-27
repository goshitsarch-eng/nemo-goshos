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
	guint32 magic;
	guint ref;
	GArray *entries;
};

#define VERNE_TARGET_LIST_MAGIC 0x56544C31u /* VTL1 */

void verne_clipboard_install_content (GtkClipboard *clipboard);

#endif
