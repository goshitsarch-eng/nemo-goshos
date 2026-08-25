#include "config.h"
#include "verne-gtk-compat.h"
#include <libgail-util/gailtextutil.h>
#include <libgail-util/gailmisc.h>
#include <atk/atk.h>
#include <graphene.h>
#include <string.h>

G_DEFINE_TYPE (GailTextUtil, gail_text_util, G_TYPE_OBJECT)

static void
gail_text_util_finalize (GObject *object)
{
	GailTextUtil *util = GAIL_TEXT_UTIL (object);
	g_clear_object (&util->buffer);
	G_OBJECT_CLASS (gail_text_util_parent_class)->finalize (object);
}

static void
gail_text_util_class_init (GailTextUtilClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = gail_text_util_finalize;
}

static void
gail_text_util_init (GailTextUtil *util)
{
	util->buffer = gtk_text_buffer_new (NULL);
}

GailTextUtil *
gail_text_util_new (void)
{
	return g_object_new (GAIL_TYPE_TEXT_UTIL, NULL);
}

void
gail_text_util_text_setup (GailTextUtil *textutil, const gchar *text)
{
	g_return_if_fail (GAIL_IS_TEXT_UTIL (textutil) || textutil != NULL);
	if (textutil->buffer == NULL)
		textutil->buffer = gtk_text_buffer_new (NULL);
	gtk_text_buffer_set_text (textutil->buffer, text ? text : "", -1);
}

void
gail_text_util_buffer_setup (GailTextUtil *textutil, GtkTextBuffer *buffer)
{
	g_return_if_fail (textutil != NULL);
	g_set_object (&textutil->buffer, buffer);
}

gchar *
gail_text_util_get_substring (GailTextUtil *textutil, gint start_pos, gint end_pos)
{
	GtkTextIter start, end;

	g_return_val_if_fail (textutil != NULL && textutil->buffer != NULL, g_strdup (""));
	gtk_text_buffer_get_iter_at_offset (textutil->buffer, &start, start_pos);
	if (end_pos < 0)
		gtk_text_buffer_get_end_iter (textutil->buffer, &end);
	else
		gtk_text_buffer_get_iter_at_offset (textutil->buffer, &end, end_pos);
	return gtk_text_buffer_get_text (textutil->buffer, &start, &end, FALSE);
}

static void
move_iter_boundary (GtkTextIter *iter, AtkTextBoundary boundary, gboolean forward)
{
	switch (boundary) {
	case ATK_TEXT_BOUNDARY_CHAR:
		if (forward)
			gtk_text_iter_forward_char (iter);
		else
			gtk_text_iter_backward_char (iter);
		break;
	case ATK_TEXT_BOUNDARY_WORD_START:
	case ATK_TEXT_BOUNDARY_WORD_END:
		if (forward)
			gtk_text_iter_forward_word_end (iter);
		else
			gtk_text_iter_backward_word_start (iter);
		break;
	case ATK_TEXT_BOUNDARY_LINE_START:
	case ATK_TEXT_BOUNDARY_LINE_END:
		if (forward)
			gtk_text_iter_forward_line (iter);
		else
			gtk_text_iter_backward_line (iter);
		break;
	case ATK_TEXT_BOUNDARY_SENTENCE_START:
	case ATK_TEXT_BOUNDARY_SENTENCE_END:
		if (forward)
			gtk_text_iter_forward_sentence_end (iter);
		else
			gtk_text_iter_backward_sentence_start (iter);
		break;
	default:
		break;
	}
}

gchar *
gail_text_util_get_text (GailTextUtil *textutil,
			 PangoLayout *layout,
			 GailOffsetType function,
			 AtkTextBoundary boundary_type,
			 gint offset,
			 gint *start_offset,
			 gint *end_offset)
{
	GtkTextIter start, end;
	(void) layout;

	g_return_val_if_fail (textutil != NULL && textutil->buffer != NULL, g_strdup (""));
	gtk_text_buffer_get_iter_at_offset (textutil->buffer, &start, offset);
	end = start;

	switch (function) {
	case GAIL_BEFORE_OFFSET:
		move_iter_boundary (&start, boundary_type, FALSE);
		break;
	case GAIL_AFTER_OFFSET:
		move_iter_boundary (&end, boundary_type, TRUE);
		break;
	case GAIL_AT_OFFSET:
	default:
		move_iter_boundary (&start, boundary_type, FALSE);
		move_iter_boundary (&end, boundary_type, TRUE);
		break;
	}

	if (start_offset)
		*start_offset = gtk_text_iter_get_offset (&start);
	if (end_offset)
		*end_offset = gtk_text_iter_get_offset (&end);
	return gtk_text_buffer_get_text (textutil->buffer, &start, &end, FALSE);
}

AtkAttributeSet *
gail_misc_add_attribute (AtkAttributeSet *list, AtkTextAttribute attr, gchar *value)
{
	AtkAttribute *at = g_new (AtkAttribute, 1);
	at->name = g_strdup (atk_text_attribute_get_name (attr));
	at->value = value;
	return g_slist_prepend (list, at);
}

AtkAttributeSet *
gail_misc_layout_get_run_attributes (AtkAttributeSet *attrib_set,
				     PangoLayout *layout,
				     const gchar *text,
				     gint offset,
				     gint *start_offset,
				     gint *end_offset)
{
	(void) layout;
	(void) text;
	if (start_offset)
		*start_offset = offset;
	if (end_offset)
		*end_offset = offset;
	return attrib_set;
}

AtkAttributeSet *
gail_misc_get_default_attributes (AtkAttributeSet *attrib_set,
				  PangoLayout *layout,
				  GtkWidget *widget)
{
	(void) layout;
	(void) widget;
	return attrib_set;
}

void
gail_misc_get_extents_from_pango_rectangle (GtkWidget *widget,
					    PangoRectangle *char_rect,
					    gint x_layout,
					    gint y_layout,
					    gint *x,
					    gint *y,
					    gint *width,
					    gint *height,
					    AtkCoordType coords)
{
	graphene_point_t origin = GRAPHENE_POINT_INIT (0, 0);
	(void) coords;
	if (widget && gtk_widget_get_native (widget)) {
		GtkNative *native = gtk_widget_get_native (widget);
		gtk_widget_compute_point (widget, GTK_WIDGET (native), &GRAPHENE_POINT_INIT (0, 0), &origin);
	}
	if (x)
		*x = x_layout + PANGO_PIXELS (char_rect->x) + (int) origin.x;
	if (y)
		*y = y_layout + PANGO_PIXELS (char_rect->y) + (int) origin.y;
	if (width)
		*width = PANGO_PIXELS (char_rect->width);
	if (height)
		*height = PANGO_PIXELS (char_rect->height);
}

gint
gail_misc_get_index_at_point_in_layout (GtkWidget *widget,
					PangoLayout *layout,
					gint x_layout,
					gint y_layout,
					gint x,
					gint y,
					AtkCoordType coords)
{
	int index = 0, trailing = 0;
	(void) widget;
	(void) coords;
	if (layout == NULL)
		return -1;
	if (!pango_layout_xy_to_index (layout,
				       (x - x_layout) * PANGO_SCALE,
				       (y - y_layout) * PANGO_SCALE,
				       &index, &trailing))
		return -1;
	return g_utf8_pointer_to_offset (pango_layout_get_text (layout),
					 pango_layout_get_text (layout) + index);
}
