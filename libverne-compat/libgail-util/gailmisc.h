#ifndef GAIL_MISC_H
#define GAIL_MISC_H

#include <gtk/gtk.h>
#include <pango/pango.h>
#include <atk/atk.h>

G_BEGIN_DECLS

AtkAttributeSet *gail_misc_add_attribute (AtkAttributeSet *list,
					  AtkTextAttribute attr,
					  gchar *value);
AtkAttributeSet *gail_misc_layout_get_run_attributes (AtkAttributeSet *attrib_set,
						      PangoLayout *layout,
						      const gchar *text,
						      gint offset,
						      gint *start_offset,
						      gint *end_offset);
AtkAttributeSet *gail_misc_get_default_attributes (AtkAttributeSet *attrib_set,
						   PangoLayout *layout,
						   GtkWidget *widget);
void gail_misc_get_extents_from_pango_rectangle (GtkWidget *widget,
						 PangoRectangle *char_rect,
						 gint x_layout,
						 gint y_layout,
						 gint *x,
						 gint *y,
						 gint *width,
						 gint *height,
						 AtkCoordType coords);
gint gail_misc_get_index_at_point_in_layout (GtkWidget *widget,
					     PangoLayout *layout,
					     gint x_layout,
					     gint y_layout,
					     gint x,
					     gint y,
					     AtkCoordType coords);

G_END_DECLS
#endif
