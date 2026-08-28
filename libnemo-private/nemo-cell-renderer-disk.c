/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

*/

#include "nemo-cell-renderer-disk.h"
#include <math.h>

G_DEFINE_TYPE (NemoCellRendererDisk, nemo_cell_renderer_disk,
	       GTK_TYPE_CELL_RENDERER_TEXT);


static void     nemo_cell_renderer_disk_get_property  (GObject                    *object,
                                                       guint                       param_id,
                                                       GValue                     *value,
                                                       GParamSpec                 *pspec);

static void     nemo_cell_renderer_disk_set_property  (GObject                    *object,
                                                       guint                       param_id,
                                                       const GValue               *value,
                                                       GParamSpec                 *pspec);

static void     nemo_cell_renderer_disk_finalize (GObject *gobject);

static void     nemo_cell_renderer_disk_render (GtkCellRenderer       *cell,
                                                cairo_t               *cr,
                                                GtkWidget             *widget,
                                                const GdkRectangle    *background_area,
                                                const GdkRectangle    *cell_area,
                                                GtkCellRendererState   flags);

enum
{
  PROP_DISK_FULL_PERCENTAGE = 1,
  PROP_SHOW_DISK_FULL_PERCENTAGE = 2,
};

static   gpointer parent_class;

static void
nemo_cell_renderer_disk_init (NemoCellRendererDisk *cell)
{
	g_object_set (cell,
		      "disk-full-percent", 0,
		      "show-disk-full-percent", FALSE,
		      NULL);
}

static void
nemo_cell_renderer_disk_class_init (NemoCellRendererDiskClass *klass)
{
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);
    GObjectClass         *object_class = G_OBJECT_CLASS(klass);
    parent_class           = g_type_class_peek_parent (klass);
    object_class->finalize = nemo_cell_renderer_disk_finalize;

    object_class->get_property = nemo_cell_renderer_disk_get_property;
    object_class->set_property = nemo_cell_renderer_disk_set_property;
    cell_class->snapshot = NULL;
    verne_cell_renderer_class_set_render (cell_class, nemo_cell_renderer_disk_render);

    g_object_class_install_property (object_class,
                                     PROP_DISK_FULL_PERCENTAGE,
                                     g_param_spec_int ("disk-full-percent",
                                                       "Percentage",
                                                       "The fractional bar to display",
                                                       -1, 100, 0,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
                                     PROP_SHOW_DISK_FULL_PERCENTAGE,
                                     g_param_spec_boolean ("show-disk-full-percent",
                                                         "Show Percentage Graph",
                                                         "Whether to show the bar",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

GtkCellRenderer *
nemo_cell_renderer_disk_new (void)
{
    return g_object_new (NEMO_TYPE_CELL_RENDERER_DISK, NULL);
}

static void
nemo_cell_renderer_disk_finalize (GObject *object)
{
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nemo_cell_renderer_disk_get_property (GObject    *object,
                                      guint       param_id,
                                      GValue     *value,
                                      GParamSpec *psec)
{
  NemoCellRendererDisk  *celldisk = NEMO_CELL_RENDERER_DISK (object);

  switch (param_id)
  {
    case PROP_DISK_FULL_PERCENTAGE:
        g_value_set_int(value, celldisk->disk_full_percent);
        break;
    case PROP_SHOW_DISK_FULL_PERCENTAGE:
        g_value_set_boolean(value, celldisk->show_disk_full_percent);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, psec);
        break;
  }
}

static void
nemo_cell_renderer_disk_set_property (GObject      *object,
                                      guint         param_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  NemoCellRendererDisk *celldisk = NEMO_CELL_RENDERER_DISK (object);

  switch (param_id)
  {
    case PROP_DISK_FULL_PERCENTAGE:
        celldisk->disk_full_percent = g_value_get_int (value);
        break;
    case PROP_SHOW_DISK_FULL_PERCENTAGE:
        celldisk->show_disk_full_percent = g_value_get_boolean (value);
        break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
      break;
  }
}

static void
use_default_color (GdkRGBA *color)
{
    color->red = .5;
    color->green = .5;
    color->blue = .5;
    color->alpha = 1;
}

/* GTK4 dropped widget style properties, so the -NemoPlacesTreeView-disk-full-*
 * colours a GTK3 theme used to supply are gone. Derive the two colours from
 * the row's own style instead: the accent colour for the used portion, and a
 * washed-out version of the row's text colour for the trough. On a selected
 * row both are taken from the selection foreground so the bar stays legible
 * against the selection background. */
static void
lookup_theme_color (GtkStyleContext *context,
                    const gchar     *name,
                    GdkRGBA         *color)
{
    if (context != NULL && gtk_style_context_lookup_color (context, name, color))
        return;

    color->red = 0.21;
    color->green = 0.52;
    color->blue = 0.89;
    color->alpha = 1.0;
}

static void
get_disk_bar_colors (GtkWidget            *widget,
                     GtkCellRendererState  flags,
                     GdkRGBA              *bg_color,
                     GdkRGBA              *fg_color)
{
    GtkStyleContext *context = widget ? gtk_widget_get_style_context (widget) : NULL;
    GdkRGBA text_color;

    if (context != NULL) {
        gtk_style_context_get_color (context, gtk_widget_get_state_flags (widget), &text_color);
    } else {
        use_default_color (&text_color);
    }

    if (flags & GTK_CELL_RENDERER_SELECTED) {
        /* Against the selection background the accent colour is invisible,
         * so use the row's own foreground for both halves of the bar. */
        *fg_color = text_color;
        *bg_color = text_color;
        bg_color->alpha = text_color.alpha * 0.35;
    } else {
        lookup_theme_color (context, "accent_bg_color", fg_color);
        *bg_color = text_color;
        bg_color->alpha = text_color.alpha * 0.25;
    }
}

#define _270_DEG 270.0 * (M_PI/180.0)
#define _180_DEG 180.0 * (M_PI/180.0)
#define  _90_DEG  90.0 * (M_PI/180.0)
#define   _0_DEG 0.0

static void
cairo_rectangle_with_radius_corners (cairo_t *cr,
                                     gint x,
                                     gint y,
                                     gint w,
                                     gint h,
                                     gint rad,
                                     GtkTextDirection dir)
{
    if (dir == GTK_TEXT_DIR_RTL) {
        cairo_move_to (cr, x - w + rad, y);
        cairo_line_to (cr, x - rad, y);
        cairo_arc (cr, x - rad, y + rad, rad, _270_DEG, _0_DEG);
        cairo_line_to (cr, x, y + h - rad);
        cairo_arc (cr, x - rad, y + h - rad, rad, _0_DEG, _90_DEG);
        cairo_line_to (cr, x - w + rad, y + h);
        cairo_arc (cr, x - w + rad, y + h - rad, rad, _90_DEG, _180_DEG);
        cairo_line_to (cr, x - w, y - rad);
        cairo_arc (cr, x - w + rad, y + rad, rad, _180_DEG, _270_DEG);
    } else {
        cairo_move_to (cr, x + rad, y);
        cairo_line_to (cr, x + w - rad, y);
        cairo_arc (cr, x + w - rad, y + rad, rad, _270_DEG, _0_DEG);
        cairo_line_to (cr, x + w, y + h - rad);
        cairo_arc (cr, x + w - rad, y + h - rad, rad, _0_DEG, _90_DEG);
        cairo_line_to (cr, x + rad, y + h);
        cairo_arc (cr, x + rad, y + h - rad, rad, _90_DEG, _180_DEG);
        cairo_line_to (cr, x, y - rad);
        cairo_arc (cr, x + rad, y + rad, rad, _180_DEG, _270_DEG);
    }
}

static void
nemo_cell_renderer_disk_render (GtkCellRenderer       *cell,
                                cairo_t               *cr,
                                GtkWidget             *widget,
                                const GdkRectangle    *background_area,
                                const GdkRectangle    *cell_area,
                                GtkCellRendererState   flags)
{
    NemoCellRendererDisk *cellprogress = NEMO_CELL_RENDERER_DISK (cell);
    gint                        x, y, w;
    gint                        xpad, ypad;
    gint                        full;
    gboolean                    show = cellprogress->show_disk_full_percent;
    GtkStyleContext *context;

    if (show) {
        GdkRGBA bg_color, fg_color;
        gint bar_width, bar_radius, bottom_padding, max_length;

        context = gtk_widget_get_style_context (widget);

        gtk_widget_style_get (widget,
                              "disk-full-bar-width",      &bar_width,
                              "disk-full-bar-radius",     &bar_radius,
                              "disk-full-bottom-padding", &bottom_padding,
                              "disk-full-max-length",     &max_length,
                              NULL);

        get_disk_bar_colors (widget, flags, &bg_color, &fg_color);

        gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

        if (cellprogress->direction == GTK_TEXT_DIR_RTL) {
            x = cell_area->x + cell_area->width - xpad;
        } else {
            x = cell_area->x + xpad;
        }

        y = cell_area->y + cell_area->height - bar_width - bottom_padding;
        w = cell_area->width - xpad * 2;
        w = w < max_length ? w : max_length;
        full = (int) (((float) cellprogress->disk_full_percent / 100.0) * (float) w);

        gtk_style_context_save (context);

        cairo_save (cr);

        gdk_cairo_set_source_rgba (cr, &bg_color);
        cairo_rectangle_with_radius_corners (cr, x, y, w, bar_width, bar_radius, cellprogress->direction);
        cairo_fill (cr);

        cairo_restore (cr);
        cairo_save (cr);

        gdk_cairo_set_source_rgba (cr, &fg_color);
        cairo_rectangle_with_radius_corners (cr, x, y, full, bar_width, bar_radius, cellprogress->direction);
        cairo_fill (cr);

        cairo_restore (cr);

        gtk_style_context_restore (context);
    }

}
