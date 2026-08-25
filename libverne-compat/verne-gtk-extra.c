#include "config.h"
#include "verne-gtk-compat.h"
#include <atk/atk.h>
#include <stdarg.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <graphene.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

/* ---------- GdkColor boxed type (removed in GTK4) ---------- */
static GdkColor *
gdk_color_copy_impl (const GdkColor *color)
{
	GdkColor *c = g_new (GdkColor, 1);
	*c = *color;
	return c;
}

GType
gdk_color_get_type (void)
{
	static GType type = 0;
	if (type == 0)
		type = g_boxed_type_register_static ("GdkColor",
						     (GBoxedCopyFunc) gdk_color_copy_impl,
						     (GBoxedFreeFunc) gdk_color_free);
	return type;
}

GdkColor *
gdk_color_copy (const GdkColor *color)
{
	return color ? gdk_color_copy_impl (color) : NULL;
}

void
gdk_color_free (GdkColor *color)
{
	g_free (color);
}

/* ---------- style properties ---------- */
static GHashTable *style_props;

void
gtk_widget_class_install_style_property (GtkWidgetClass *klass, GParamSpec *pspec)
{
	GList *list;
	GType type = G_TYPE_FROM_CLASS (klass);

	if (style_props == NULL)
		style_props = g_hash_table_new (g_direct_hash, g_direct_equal);
	list = g_hash_table_lookup (style_props, GSIZE_TO_POINTER (type));
	list = g_list_prepend (list, pspec);
	g_hash_table_insert (style_props, GSIZE_TO_POINTER (type), list);
}

static GParamSpec *
lookup_style_pspec (GtkWidget *widget, const gchar *name)
{
	GType type = G_OBJECT_TYPE (widget);
	while (type != 0 && type != GTK_TYPE_WIDGET) {
		GList *l;
		if (style_props) {
			for (l = g_hash_table_lookup (style_props, GSIZE_TO_POINTER (type)); l; l = l->next) {
				GParamSpec *p = l->data;
				if (g_strcmp0 (p->name, name) == 0)
					return p;
			}
		}
		type = g_type_parent (type);
	}
	return NULL;
}

void
gtk_widget_style_get (GtkWidget *widget, const gchar *first_property_name, ...)
{
	va_list args;
	const gchar *name;

	va_start (args, first_property_name);
	for (name = first_property_name; name != NULL; name = va_arg (args, const gchar *)) {
		GParamSpec *pspec = lookup_style_pspec (widget, name);
		gpointer dest = va_arg (args, gpointer);
		if (dest == NULL)
			continue;
		if (pspec == NULL) {
			if (g_str_has_suffix (name, "color"))
				*(gpointer *) dest = NULL;
			else
				*(gint *) dest = 0;
			continue;
		}
		if (G_IS_PARAM_SPEC_INT (pspec))
			*(gint *) dest = G_PARAM_SPEC_INT (pspec)->default_value;
		else if (G_IS_PARAM_SPEC_BOOLEAN (pspec))
			*(gboolean *) dest = G_PARAM_SPEC_BOOLEAN (pspec)->default_value;
		else if (G_IS_PARAM_SPEC_BOXED (pspec))
			*(gpointer *) dest = NULL;
		else
			*(gint *) dest = 0;
	}
	va_end (args);
}

/* ---------- accessible widget glue ---------- */
static GQuark
accessible_widget_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-accessible-widget");
	return q;
}

void
gtk_accessible_set_widget (gpointer accessible, GtkWidget *widget)
{
	if (accessible)
		g_object_set_qdata (G_OBJECT (accessible), accessible_widget_quark (), widget);
}

GtkWidget *
verne_gtk_accessible_get_widget (gpointer accessible)
{
	gpointer obj;

	if (accessible == NULL)
		return NULL;
	if (GTK_IS_WIDGET (accessible))
		return GTK_WIDGET (accessible);
	obj = g_object_get_qdata (G_OBJECT (accessible), accessible_widget_quark ());
	if (GTK_IS_WIDGET (obj))
		return GTK_WIDGET (obj);
	obj = g_object_get_qdata (G_OBJECT (accessible), g_quark_from_string ("object-for-accessible"));
	if (GTK_IS_WIDGET (obj))
		return GTK_WIDGET (obj);
	if (ATK_IS_GOBJECT_ACCESSIBLE (accessible)) {
		obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible));
		if (GTK_IS_WIDGET (obj))
			return GTK_WIDGET (obj);
	}
	return NULL;
}

/* ---------- dialog action area ---------- */
GtkWidget *
gtk_dialog_get_action_area (GtkDialog *dialog)
{
	GtkWidget *area = g_object_get_data (G_OBJECT (dialog), "verne-action-area");
	if (area == NULL) {
		area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_halign (area, GTK_ALIGN_END);
		gtk_widget_set_margin_top (area, 6);
		gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (dialog)), area);
		g_object_set_data (G_OBJECT (dialog), "verne-action-area", area);
	}
	return area;
}

void
gtk_button_box_set_layout (gpointer box, gint layout_style)
{
	(void) box;
	(void) layout_style;
}

/* ---------- grabs / type hints / toplevel helpers ---------- */
void
gtk_grab_add (GtkWidget *widget)
{
	g_object_set_data (G_OBJECT (gdk_display_get_default ()), "verne-grab-widget", widget);
}

void
gtk_grab_remove (GtkWidget *widget)
{
	GdkDisplay *d = gdk_display_get_default ();
	if (g_object_get_data (G_OBJECT (d), "verne-grab-widget") == widget)
		g_object_set_data (G_OBJECT (d), "verne-grab-widget", NULL);
}

static void verne_window_ensure_realize_hook (GtkWindow *window);

void
gtk_window_set_type_hint (GtkWindow *window, GdkWindowTypeHint hint)
{
	g_object_set_data (G_OBJECT (window), "verne-type-hint", GINT_TO_POINTER ((int) hint));
	if (hint == GDK_WINDOW_TYPE_HINT_DESKTOP) {
		gtk_window_set_decorated (window, FALSE);
		gtk_window_set_deletable (window, FALSE);
	}
	verne_window_ensure_realize_hook (window);
}

GdkWindowTypeHint
gtk_window_get_type_hint (GtkWindow *window)
{
	return (GdkWindowTypeHint) GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window), "verne-type-hint"));
}

static void
verne_window_apply_x11 (GtkWindow *window)
{
#ifdef GDK_WINDOWING_X11
	GtkNative *native;
	GdkSurface *surface;
	Display *dpy;
	Window xid;
	GdkWindowTypeHint hint;
	gpointer xptr, yptr;

	native = gtk_widget_get_native (GTK_WIDGET (window));
	surface = native ? gtk_native_get_surface (native) : NULL;
	if (surface == NULL || !GDK_IS_X11_SURFACE (surface))
		return;
	dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (surface));
	xid = gdk_x11_surface_get_xid (surface);
	if (dpy == NULL || xid == 0)
		return;

	hint = gtk_window_get_type_hint (window);
	if (hint == GDK_WINDOW_TYPE_HINT_DESKTOP || hint == GDK_WINDOW_TYPE_HINT_DIALOG) {
		Atom type = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", False);
		Atom value = XInternAtom (dpy,
					  hint == GDK_WINDOW_TYPE_HINT_DESKTOP
					  ? "_NET_WM_WINDOW_TYPE_DESKTOP"
					  : "_NET_WM_WINDOW_TYPE_DIALOG",
					  False);
		XChangeProperty (dpy, xid, type, XA_ATOM, 32, PropModeReplace,
				 (unsigned char *) &value, 1);
		if (hint == GDK_WINDOW_TYPE_HINT_DESKTOP)
			XLowerWindow (dpy, xid);
	}

	xptr = g_object_get_data (G_OBJECT (window), "verne-win-x");
	yptr = g_object_get_data (G_OBJECT (window), "verne-win-y");
	if (xptr || yptr)
		XMoveWindow (dpy, xid,
			     GPOINTER_TO_INT (xptr),
			     GPOINTER_TO_INT (yptr));
#else
	(void) window;
#endif
}

static void
verne_window_on_realize (GtkWidget *widget, gpointer data)
{
	(void) data;
	verne_window_apply_x11 (GTK_WINDOW (widget));
}

static void
verne_window_ensure_realize_hook (GtkWindow *window)
{
	if (g_object_get_data (G_OBJECT (window), "verne-x11-hooked"))
		return;
	g_object_set_data (G_OBJECT (window), "verne-x11-hooked", GINT_TO_POINTER (1));
	g_signal_connect (window, "realize", G_CALLBACK (verne_window_on_realize), NULL);
	if (gtk_widget_get_realized (GTK_WIDGET (window)))
		verne_window_apply_x11 (window);
}

void
gtk_window_move (GtkWindow *window, gint x, gint y)
{
	g_object_set_data (G_OBJECT (window), "verne-win-x", GINT_TO_POINTER (x));
	g_object_set_data (G_OBJECT (window), "verne-win-y", GINT_TO_POINTER (y));
	verne_window_ensure_realize_hook (window);
	if (gtk_widget_get_realized (GTK_WIDGET (window)))
		verne_window_apply_x11 (window);
}

void
gtk_window_get_position (GtkWindow *window, gint *x, gint *y)
{
	if (x)
		*x = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window), "verne-win-x"));
	if (y)
		*y = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window), "verne-win-y"));
}

void
gtk_container_child_set (GtkWidget *container, GtkWidget *child, const gchar *first_property_name, ...)
{
	va_list args;
	const gchar *name;

	va_start (args, first_property_name);
	for (name = first_property_name; name != NULL; name = va_arg (args, const gchar *)) {
		if (GTK_IS_NOTEBOOK (container) && g_strcmp0 (name, "tab-expand") == 0) {
			gboolean expand = va_arg (args, gboolean);
			GtkNotebookPage *page = gtk_notebook_get_page (GTK_NOTEBOOK (container), child);
			if (page)
				g_object_set (page, "tab-expand", expand, NULL);
		} else {
			/* skip one value */
			(void) va_arg (args, gpointer);
		}
	}
	va_end (args);
}

GtkWidget *
gtk_style_context_get_widget_or_null (GtkStyleContext *ctx)
{
	return ctx ? g_object_get_data (G_OBJECT (ctx), "verne-widget") : NULL;
}

void
verne_style_context_bind_widget (GtkWidget *widget)
{
	if (widget)
		g_object_set_data (G_OBJECT (gtk_widget_get_style_context (widget)), "verne-widget", widget);
}

gboolean
gtk_widget_hide_on_delete (GtkWidget *widget)
{
	gtk_widget_set_visible (widget, FALSE);
	return TRUE;
}

void
gtk_window_activate_default (GtkWindow *window)
{
	GtkWidget *focus = gtk_window_get_focus (window);
	if (GTK_IS_BUTTON (focus))
		g_signal_emit_by_name (focus, "clicked");
}

static GHashTable *binding_sets;

GtkBindingSet *
gtk_binding_set_by_class (gpointer class_struct)
{
	GtkBindingSet *set;

	g_return_val_if_fail (class_struct != NULL, NULL);
	if (binding_sets == NULL)
		binding_sets = g_hash_table_new (g_direct_hash, g_direct_equal);
	set = g_hash_table_lookup (binding_sets, class_struct);
	if (set == NULL) {
		set = g_new0 (GtkBindingSet, 1);
		set->klass = class_struct;
		set->name = g_strdup (G_OBJECT_CLASS_NAME (class_struct));
		g_hash_table_insert (binding_sets, class_struct, set);
		if (set->name)
			g_hash_table_insert (binding_sets, set->name, set);
	}
	return set;
}

GtkBindingSet *
gtk_binding_set_find (const gchar *name)
{
	if (binding_sets == NULL || name == NULL)
		return NULL;
	return g_hash_table_lookup (binding_sets, name);
}

void
gtk_binding_entry_add_signal (GtkBindingSet *binding_set, guint keyval, GdkModifierType modifiers,
			      const gchar *signal_name, guint n_args, ...)
{
	va_list args;
	g_return_if_fail (binding_set != NULL && binding_set->klass != NULL);
	va_start (args, n_args);
	if (n_args == 0) {
		gtk_widget_class_add_binding_signal (binding_set->klass, keyval, modifiers, signal_name, NULL);
	} else if (n_args == 1) {
		GType t1 = va_arg (args, GType);
		if (t1 == G_TYPE_BOOLEAN) {
			gboolean v = va_arg (args, gboolean);
			gtk_widget_class_add_binding_signal (binding_set->klass, keyval, modifiers, signal_name, "b", v);
		} else {
			gtk_widget_class_add_binding_signal (binding_set->klass, keyval, modifiers, signal_name, NULL);
			(void) va_arg (args, gpointer);
		}
	} else {
		gtk_widget_class_add_binding_signal (binding_set->klass, keyval, modifiers, signal_name, NULL);
	}
	va_end (args);
}

void
gtk_binding_entry_remove (GtkBindingSet *binding_set, guint keyval, GdkModifierType modifiers)
{
	(void) binding_set;
	(void) keyval;
	(void) modifiers;
}

void
gtk_style_context_get (GtkStyleContext *context, GtkStateFlags state, ...)
{
	va_list args;
	const char *name;
	GtkWidget *widget = gtk_style_context_get_widget_or_null (context);
	(void) state;

	va_start (args, state);
	while ((name = va_arg (args, const char *)) != NULL) {
		gpointer *out = va_arg (args, gpointer *);
		if (out == NULL)
			break;
		if (g_strcmp0 (name, "font") == 0 && widget) {
			PangoFontDescription *desc = pango_font_description_copy (
				pango_context_get_font_description (gtk_widget_get_pango_context (widget)));
			*out = desc;
		} else {
			*out = NULL;
		}
	}
	va_end (args);
}

gboolean
verne_gdk_cairo_get_clip_rectangle (cairo_t *cr, cairo_rectangle_int_t *rect)
{
	double x1, y1, x2, y2;

	cairo_clip_extents (cr, &x1, &y1, &x2, &y2);
	if (x1 >= x2 || y1 >= y2)
		return FALSE;
	if (rect) {
		rect->x = (int) x1;
		rect->y = (int) y1;
		rect->width = (int) (x2 - x1 + 0.5);
		rect->height = (int) (y2 - y1 + 0.5);
	}
	return TRUE;
}

GdkSurface *
gdk_window_new (GdkSurface *parent, GdkWindowAttr *attributes, gint attributes_mask)
{
	(void) attributes;
	(void) attributes_mask;
	return parent;
}

void gdk_window_destroy (GdkSurface *window) { (void) window; }
void gdk_window_show (GdkSurface *window) { (void) window; }
void gdk_window_hide (GdkSurface *window) { (void) window; }
static GQuark
verne_surface_user_data_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-surface-user-data");
	return q;
}

void
gdk_window_set_user_data (GdkSurface *window, gpointer user_data)
{
	if (window)
		g_object_set_qdata (G_OBJECT (window), verne_surface_user_data_quark (), user_data);
}

void
gdk_window_get_user_data (GdkSurface *window, gpointer *data)
{
	gpointer found = NULL;

	if (data == NULL)
		return;
	if (window != NULL) {
		found = g_object_get_qdata (G_OBJECT (window), verne_surface_user_data_quark ());
		if (found == NULL) {
			GtkNative *native = gtk_native_get_for_surface (window);
			if (native)
				found = native;
		}
	}
	*data = found;
}
void gdk_window_invalidate_rect (GdkSurface *window, const GdkRectangle *rect, gboolean invalidate_children)
{
	(void) window; (void) rect; (void) invalidate_children;
}
void
gdk_window_get_geometry (GdkSurface *window, gint *x, gint *y, gint *width, gint *height)
{
	if (x) *x = 0;
	if (y) *y = 0;
	if (width) *width = window ? gdk_surface_get_width (window) : 0;
	if (height) *height = window ? gdk_surface_get_height (window) : 0;
}
void gdk_window_get_position (GdkSurface *window, gint *x, gint *y)
{
	(void) window;
	if (x) *x = 0;
	if (y) *y = 0;
}
void gdk_window_move (GdkSurface *window, gint x, gint y) { (void) window; (void) x; (void) y; }
void gdk_window_resize (GdkSurface *window, gint width, gint height) { (void) window; (void) width; (void) height; }
void gdk_window_raise (GdkSurface *window) { (void) window; }
void gdk_window_lower (GdkSurface *window) { (void) window; }
void gdk_window_focus (GdkSurface *window, guint32 timestamp) { (void) window; (void) timestamp; }
void gdk_window_set_cursor (GdkSurface *window, GdkCursor *cursor) { (void) window; (void) cursor; }

GdkCursor *
gdk_cursor_new (gint cursor_type)
{
	const char *name = "default";
	if (cursor_type == GDK_XTERM)
		name = "text";
	return gdk_cursor_new_from_name (name, NULL);
}

GdkCursor *
gdk_cursor_new_for_display (GdkDisplay *display, gint cursor_type)
{
	(void) display;
	return gdk_cursor_new (cursor_type);
}

void
gtk_window_get_size (GtkWindow *window, gint *width, gint *height)
{
	int w = gtk_widget_get_width (GTK_WIDGET (window));
	int h = gtk_widget_get_height (GTK_WIDGET (window));
	if (w <= 0 || h <= 0)
		gtk_window_get_default_size (window, &w, &h);
	if (width) *width = w;
	if (height) *height = h;
}

GtkWidget *
gtk_info_bar_get_content_area (GtkInfoBar *bar)
{
	GtkWidget *box;
	gpointer w = bar;

	if (w != NULL && VERNE_IS_INFO_BAR (w))
		return VERNE_INFO_BAR (w)->content_area;
	box = g_object_get_data (G_OBJECT (bar), "verne-content");
	if (box == NULL) {
		box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
		(gtk_info_bar_add_child) (bar, box);
		g_object_set_data (G_OBJECT (bar), "verne-content", box);
	}
	return box;
}

GtkWidget *
gtk_info_bar_get_action_area (GtkInfoBar *bar)
{
	GtkWidget *box;
	gpointer w = bar;

	if (w != NULL && VERNE_IS_INFO_BAR (w))
		return VERNE_INFO_BAR (w)->action_area;
	box = g_object_get_data (G_OBJECT (bar), "verne-action");
	if (box == NULL) {
		box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
		(gtk_info_bar_add_action_widget) (bar, box, 0);
		g_object_set_data (G_OBJECT (bar), "verne-action", box);
	}
	return box;
}

void
gtk_window_set_icon (GtkWindow *window, GdkPixbuf *pixbuf)
{
	(void) window;
	(void) pixbuf;
}

static GMainLoop *verne_main_loop;
static guint verne_main_depth;

void
gtk_main (void)
{
	GMainLoop *loop = g_main_loop_new (NULL, FALSE);
	GMainLoop *prev = verne_main_loop;
	verne_main_loop = loop;
	verne_main_depth++;
	g_main_loop_run (loop);
	verne_main_depth--;
	verne_main_loop = prev;
	g_main_loop_unref (loop);
}

void
gtk_main_quit (void)
{
	if (verne_main_loop)
		g_main_loop_quit (verne_main_loop);
}

gboolean
gtk_main_iteration (void)
{
	g_main_context_iteration (NULL, TRUE);
	return g_main_context_pending (NULL);
}

gboolean
gtk_main_iteration_do (gboolean blocking)
{
	return g_main_context_iteration (NULL, blocking);
}

guint
gtk_main_level (void)
{
	return verne_main_depth;
}

void
gtk_widget_get_preferred_width (GtkWidget *widget, gint *minimum, gint *natural)
{
	int min = 0, nat = 0;
	gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1, &min, &nat, NULL, NULL);
	if (minimum) *minimum = min;
	if (natural) *natural = nat;
}

void
gtk_widget_get_preferred_height (GtkWidget *widget, gint *minimum, gint *natural)
{
	int min = 0, nat = 0;
	gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL, -1, &min, &nat, NULL, NULL);
	if (minimum) *minimum = min;
	if (natural) *natural = nat;
}

cairo_surface_t *
gdk_cairo_surface_create_from_pixbuf (const GdkPixbuf *pixbuf, int scale, GdkSurface *for_surface)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	int w, h;
	(void) for_surface;
	if (pixbuf == NULL)
		return NULL;
	if (scale < 1)
		scale = 1;
	w = gdk_pixbuf_get_width (pixbuf);
	h = gdk_pixbuf_get_height (pixbuf);
	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
	cairo_surface_set_device_scale (surface, scale, scale);
	cr = cairo_create (surface);
	gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
	cairo_paint (cr);
	cairo_destroy (cr);
	return surface;
}

void
gdk_screen_get_monitor_geometry (GdkScreen *screen, int monitor, GdkRectangle *dest)
{
	GdkDisplay *d = screen ? screen : gdk_display_get_default ();
	GListModel *list = d ? gdk_display_get_monitors (d) : NULL;
	GdkMonitor *m = NULL;
	(void) monitor;
	if (list && g_list_model_get_n_items (list) > 0)
		m = g_list_model_get_item (list, 0);
	if (m && dest)
		gdk_monitor_get_geometry (m, dest);
	else if (dest) {
		dest->x = dest->y = 0;
		dest->width = verne_screen_width ();
		dest->height = verne_screen_height ();
	}
	if (m)
		g_object_unref (m);
}

int
gdk_screen_get_monitor_scale_factor (GdkScreen *screen, int monitor)
{
	(void) screen; (void) monitor;
	return 1;
}

gchar *
gdk_screen_get_monitor_plug_name (GdkScreen *screen, int monitor)
{
	(void) screen; (void) monitor;
	return g_strdup ("default");
}

void
gdk_monitor_get_workarea (GdkMonitor *monitor, GdkRectangle *workarea)
{
	if (monitor && workarea)
		gdk_monitor_get_geometry (monitor, workarea);
}

GdkEvent *
gdk_event_copy (const GdkEvent *event)
{
	return event ? g_memdup2 (event, sizeof (GdkEvent)) : NULL;
}

void
gdk_event_free (GdkEvent *event)
{
	g_free (event);
}

GdkEvent *
gdk_event_new (GdkEventType type)
{
	GdkEvent *event = g_new0 (GdkEvent, 1);
	event->type = type;
	return event;
}

gboolean
gtk_true (void)
{
	return TRUE;
}

gboolean
gtk_false (void)
{
	return FALSE;
}

void
gtk_widget_destroyed (GtkWidget *widget, GtkWidget **widget_pointer)
{
	(void) widget;
	if (widget_pointer)
		*widget_pointer = NULL;
}

void
gtk_container_check_resize (gpointer container)
{
	(void) container;
}

void
verne_gtk_tree_view_enable_model_drag_source (GtkTreeView *tree_view, GdkModifierType start_button_mask,
					      gconstpointer targets, gint n_targets, GdkDragAction actions)
{
	const GtkTargetEntry *entries = targets;
	const char **mimes;
	GdkContentFormats *formats;
	gint i;

	mimes = g_new0 (const char *, MAX (n_targets, 0) + 1);
	for (i = 0; i < n_targets; i++)
		mimes[i] = entries[i].target;
	formats = gdk_content_formats_new (mimes, (guint) MAX (n_targets, 0));
	(gtk_tree_view_enable_model_drag_source) (tree_view, start_button_mask, formats, actions);
	gdk_content_formats_unref (formats);
	g_free (mimes);
}

static GQuark
verne_cell_render_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-cell-render");
	return q;
}

static void
verne_cell_snapshot (GtkCellRenderer *cell,
		     GtkSnapshot *snapshot,
		     GtkWidget *widget,
		     const GdkRectangle *background_area,
		     const GdkRectangle *cell_area,
		     GtkCellRendererState flags)
{
	VerneCellRenderFunc render = NULL;
	GtkCellRendererClass *parent;
	GType type;
	graphene_rect_t rect;
	cairo_t *cr;

	for (type = G_OBJECT_TYPE (cell); type != 0; type = g_type_parent (type)) {
		render = g_type_get_qdata (type, verne_cell_render_quark ());
		if (render)
			break;
	}
	if (render) {
		graphene_rect_init (&rect,
				    background_area->x, background_area->y,
				    background_area->width, background_area->height);
		cr = gtk_snapshot_append_cairo (snapshot, &rect);
		render (cell, cr, widget, background_area, cell_area, flags);
		cairo_destroy (cr);
	}
	parent = g_type_class_peek_parent (G_OBJECT_GET_CLASS (cell));
	if (parent && parent->snapshot && parent->snapshot != verne_cell_snapshot)
		parent->snapshot (cell, snapshot, widget, background_area, cell_area, flags);
}

void
verne_cell_renderer_class_set_render (GtkCellRendererClass *klass, VerneCellRenderFunc handler)
{
	g_type_set_qdata (G_TYPE_FROM_CLASS (klass), verne_cell_render_quark (), handler);
	klass->snapshot = verne_cell_snapshot;
}

gboolean gtk_targets_include_text (GdkAtom *targets, gint n_targets) { (void) targets; (void) n_targets; return TRUE; }
gboolean gtk_targets_include_uri (GdkAtom *targets, gint n_targets) { (void) targets; (void) n_targets; return TRUE; }

guchar *
gtk_selection_data_get_text (const GtkSelectionData *s)
{
	if (!s || !s->data)
		return NULL;
	return (guchar *) g_strndup ((char *) s->data, s->length);
}

void
gtk_entry_set_icon_from_stock (GtkEntry *entry, GtkEntryIconPosition pos, const gchar *stock_id)
{
	gtk_entry_set_icon_from_icon_name (entry, pos, stock_id);
}

GtkWidget *gtk_tool_item_new (void) { return gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); }
void gtk_tool_item_set_expand (gpointer item, gboolean expand) {
	gtk_widget_set_hexpand (GTK_WIDGET (item), expand);
}

void gtk_activatable_set_related_action (gpointer activatable, GtkAction *action) {
	g_object_set_data (G_OBJECT (activatable), "verne-action", action);
}
GtkAction *gtk_activatable_get_related_action (gpointer activatable) {
	return g_object_get_data (G_OBJECT (activatable), "verne-action");
}
void gtk_activatable_set_use_action_appearance (gpointer activatable, gboolean use) { (void) activatable; (void) use; }
gboolean gtk_activatable_get_use_action_appearance (gpointer activatable) { (void) activatable; return TRUE; }

gboolean gtk_bindings_activate_event (GObject *object, GdkEventKey *event) { (void) object; (void) event; return FALSE; }
void gtk_propagate_event (GtkWidget *widget, GdkEvent *event) { (void) widget; (void) event; }

GtkWidget *
gtk_image_new_from_surface (cairo_surface_t *surface)
{
	(void) surface;
	return gtk_image_new ();
}
void gtk_image_set_from_surface (GtkImage *image, cairo_surface_t *surface) { (void) image; (void) surface; }
void gtk_render_icon_surface (GtkStyleContext *context, cairo_t *cr, cairo_surface_t *surface, gdouble x, gdouble y) {
	(void) context;
	if (surface) {
		cairo_set_source_surface (cr, surface, x, y);
		cairo_paint (cr);
	}
}

gpointer
gtk_icon_theme_lookup_icon_for_scale (GtkIconTheme *theme, const gchar *name, gint size, gint scale, GtkIconLookupFlags flags)
{
	(void) flags;
	return gtk_icon_theme_lookup_icon (theme, name, NULL, size, scale, GTK_TEXT_DIR_NONE, 0);
}
gpointer
gtk_icon_theme_lookup_by_gicon_for_scale (GtkIconTheme *theme, GIcon *icon, gint size, gint scale, GtkIconLookupFlags flags)
{
	(void) flags;
	return gtk_icon_theme_lookup_by_gicon (theme, icon, size, scale, GTK_TEXT_DIR_NONE, 0);
}
GdkPixbuf *gtk_icon_info_load_icon (gpointer info, GError **error) { (void) info; (void) error; return NULL; }
const gchar *gtk_icon_info_get_filename (gpointer info) { (void) info; return NULL; }
GtkIconSize gtk_icon_size_from_name (const gchar *name) { (void) name; return GTK_ICON_SIZE_INHERIT; }

GtkWidget *gtk_menu_shell_get_selected_item (gpointer menu_shell) { (void) menu_shell; return NULL; }

void
gtk_file_filter_add_custom (GtkFileFilter *filter, GtkFileFilterFlags needed, GtkFileFilterFunc func, gpointer data, GDestroyNotify notify)
{
	(void) needed; (void) func;
	if (notify && data)
		notify (data);
	gtk_file_filter_add_pattern (filter, "*");
}

gchar *
gtk_file_chooser_get_filename (GtkFileChooser *chooser)
{
	GFile *file = gtk_file_chooser_get_file (chooser);
	gchar *path = file ? g_file_get_path (file) : NULL;
	if (file)
		g_object_unref (file);
	return path;
}

GdkSurface *
gdk_device_get_window_at_position (GdkDevice *device, gint *x, gint *y)
{
	double dx = 0, dy = 0;
	GdkSurface *s = gdk_device_get_surface_at_position (device, &dx, &dy);
	if (x) *x = (gint) dx;
	if (y) *y = (gint) dy;
	return s;
}

void gdk_window_set_background_rgba (GdkSurface *window, const GdkRGBA *rgba) { (void) window; (void) rgba; }
void gdk_window_set_transient_for (GdkSurface *window, GdkSurface *parent) { (void) window; (void) parent; }
GdkWindowTypeHint gdk_window_get_type_hint (GdkSurface *window) { (void) window; return GDK_WINDOW_TYPE_HINT_NORMAL; }
cairo_surface_t *gdk_window_create_similar_surface (GdkSurface *window, cairo_content_t content, int w, int h) {
	(void) window;
	return cairo_image_surface_create (content == CAIRO_CONTENT_COLOR ? CAIRO_FORMAT_RGB24 : CAIRO_FORMAT_ARGB32, w, h);
}
cairo_surface_t *gdk_window_create_similar_image_surface (GdkSurface *window, cairo_format_t format, int w, int h, int scale) {
	cairo_surface_t *s;
	(void) window;
	s = cairo_image_surface_create (format, w, h);
	if (scale > 1)
		cairo_surface_set_device_scale (s, scale, scale);
	return s;
}
void gdk_window_move_to_rect (GdkSurface *window, const GdkRectangle *rect, GdkGravity rect_anchor, GdkGravity window_anchor, GdkAnchorHints hints, int dx, int dy) {
	(void) window; (void) rect; (void) rect_anchor; (void) window_anchor; (void) hints; (void) dx; (void) dy;
}
void gdk_window_remove_filter (GdkSurface *window, gpointer func, gpointer data) { (void) window; (void) func; (void) data; }
gboolean gdk_property_get (GdkSurface *window, GdkAtom property, GdkAtom type, gulong offset, gulong length, gint pdelete, GdkAtom *actual_type, gint *actual_format, gint *actual_length, guchar **data) {
	(void) window; (void) property; (void) type; (void) offset; (void) length; (void) pdelete;
	if (actual_type) *actual_type = NULL;
	if (actual_format) *actual_format = 0;
	if (actual_length) *actual_length = 0;
	if (data) *data = NULL;
	return FALSE;
}
void gdk_property_change (GdkSurface *window, GdkAtom property, GdkAtom type, gint format, gint mode, const guchar *data, gint nelements) {
	(void) window; (void) property; (void) type; (void) format; (void) mode; (void) data; (void) nelements;
}
GdkSurface *gdk_selection_owner_get (GdkAtom selection) { (void) selection; return NULL; }
gboolean gdk_display_supports_selection_notification (GdkDisplay *display) { (void) display; return FALSE; }
gboolean gdk_screen_get_setting (GdkScreen *screen, const gchar *name, GValue *value) { (void) screen; (void) name; (void) value; return FALSE; }
GList *gdk_screen_get_window_stack (GdkScreen *screen) { (void) screen; return NULL; }
unsigned long gdk_x11_get_xatom_by_name (const gchar *name) { (void) name; return 0; }
struct passwd *
gnome_desktop_get_session_user_pwent (void)
{
	return getpwuid (getuid ());
}

GType
gtk_activatable_get_type (void)
{
	static gsize init = 0;
	static GType type;
	if (g_once_init_enter (&init)) {
		type = g_type_register_static_simple (G_TYPE_INTERFACE, "GtkActivatable",
						      sizeof (GtkActivatableIface), NULL,
						      0, NULL, 0);
		g_once_init_leave (&init, 1);
	}
	return type;
}

void
gtk_builder_add_callback_symbols (GtkBuilder *builder, const char *first, ...)
{
	(void) builder; (void) first;
}

void
gtk_style_context_get_border_color (GtkStyleContext *context, GtkStateFlags state, GdkRGBA *color)
{
	(void) state;
	if (color)
		(gtk_style_context_get_color) (context, color);
}

void
gtk_style_context_get_style (GtkStyleContext *context, ...)
{
	(void) context;
}

unsigned long
verne_gdk_root_xid (void)
{
#ifdef GDK_WINDOWING_X11
	GdkDisplay *display = gdk_display_get_default ();
	Display *xdisplay;

	if (display == NULL || !GDK_IS_X11_DISPLAY (display))
		return 0;
	xdisplay = gdk_x11_display_get_xdisplay (display);
	if (xdisplay == NULL)
		return 0;
	return (unsigned long) RootWindow (xdisplay, DefaultScreen (xdisplay));
#else
	return 0;
#endif
}

