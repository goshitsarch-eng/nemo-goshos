#ifndef GAIL_TEXT_UTIL_H
#define GAIL_TEXT_UTIL_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <atk/atk.h>

G_BEGIN_DECLS

typedef enum {
	GAIL_BEFORE_OFFSET,
	GAIL_AT_OFFSET,
	GAIL_AFTER_OFFSET
} GailOffsetType;

#define GAIL_TYPE_TEXT_UTIL (gail_text_util_get_type ())
#define GAIL_TEXT_UTIL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_TEXT_UTIL, GailTextUtil))
#define GAIL_IS_TEXT_UTIL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_TEXT_UTIL))

typedef struct _GailTextUtil {
	GObject parent;
	GtkTextBuffer *buffer;
} GailTextUtil;

typedef struct _GailTextUtilClass {
	GObjectClass parent_class;
} GailTextUtilClass;

GType gail_text_util_get_type (void);
GailTextUtil *gail_text_util_new (void);
void gail_text_util_text_setup (GailTextUtil *textutil, const gchar *text);
void gail_text_util_buffer_setup (GailTextUtil *textutil, GtkTextBuffer *buffer);
gchar *gail_text_util_get_substring (GailTextUtil *textutil, gint start_pos, gint end_pos);
gchar *gail_text_util_get_text (GailTextUtil *textutil,
				PangoLayout *layout,
				GailOffsetType function,
				AtkTextBoundary boundary_type,
				gint offset,
				gint *start_offset,
				gint *end_offset);

G_END_DECLS
#endif
