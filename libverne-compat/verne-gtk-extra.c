#include "config.h"
#include "verne-gtk-compat.h"
#include <atk/atk.h>
#include <gsk/gsk.h>
#include <gsk/gskcairorenderer.h>
#include <gdk/deprecated/gdkpixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
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

#ifdef gtk_toggle_button_get_active
#undef gtk_toggle_button_get_active
#endif
#ifdef gtk_toggle_button_set_active
#undef gtk_toggle_button_set_active
#endif

gboolean
verne_toggle_button_get_active (gpointer button)
{
	if (button == NULL)
		return FALSE;
	if (GTK_IS_CHECK_BUTTON (button))
		return gtk_check_button_get_active (GTK_CHECK_BUTTON (button));
	if (GTK_IS_TOGGLE_BUTTON (button))
		return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	return FALSE;
}

void
verne_toggle_button_set_active (gpointer button, gboolean active)
{
	if (button == NULL)
		return;
	if (GTK_IS_CHECK_BUTTON (button))
		gtk_check_button_set_active (GTK_CHECK_BUTTON (button), active);
	else if (GTK_IS_TOGGLE_BUTTON (button))
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), active);
}

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

static GdkTexture *verne_desktop_wallpaper_texture;
static void verne_window_ensure_realize_hook (GtkWindow *window);
static void verne_window_apply_x11 (GtkWindow *window);

static GdkTexture *
verne_desktop_wallpaper_for_widget (GtkWidget *widget)
{
	GtkWidget *win;
	GdkTexture *tex = NULL;

	if (widget == NULL)
		return NULL;
	if (GTK_IS_WINDOW (widget))
		win = widget;
	else
		win = gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW);
	if (win == NULL)
		return NULL;
	if (gtk_window_get_type_hint (GTK_WINDOW (win)) != GDK_WINDOW_TYPE_HINT_DESKTOP &&
	    !gtk_widget_has_css_class (win, "verne-desktop") &&
	    !gtk_widget_has_css_class (win, "nemo-desktop-window") &&
	    g_object_get_data (G_OBJECT (win), "is_desktop_window") == NULL)
		return NULL;
	tex = g_object_get_data (G_OBJECT (win), "verne-wallpaper");
	if (tex == NULL)
		tex = verne_desktop_wallpaper_texture;
	return tex;
}

void
gtk_window_set_type_hint (GtkWindow *window, GdkWindowTypeHint hint)
{
	g_object_set_data (G_OBJECT (window), "verne-type-hint", GINT_TO_POINTER ((int) hint));
	if (hint == GDK_WINDOW_TYPE_HINT_DESKTOP) {
		static GtkCssProvider *desktop_css;
		gchar *wallpaper = NULL;
		gchar *css;
		gtk_window_set_decorated (window, FALSE);
		gtk_window_set_deletable (window, FALSE);
		gtk_window_set_skip_taskbar_hint (window, TRUE);
		gtk_window_set_skip_pager_hint (window, TRUE);
		gtk_widget_add_css_class (GTK_WIDGET (window), "verne-desktop");
		{
			const char *schemas[] = {
				"org.cinnamon.desktop.background",
				"org.gnome.desktop.background",
				NULL
			};
			int i;
			for (i = 0; schemas[i]; i++) {
				GSettingsSchema *schema = g_settings_schema_source_lookup (
					g_settings_schema_source_get_default (), schemas[i], TRUE);
				if (schema && g_settings_schema_has_key (schema, "picture-uri")) {
					GSettings *settings = g_settings_new (schemas[i]);
					gchar *uri = g_settings_get_string (settings, "picture-uri");
					if (uri && g_str_has_prefix (uri, "file://")) {
						gchar *path = g_uri_unescape_string (uri + 7, NULL);
						if (path && g_file_test (path, G_FILE_TEST_IS_REGULAR))
							wallpaper = path;
						else
							g_free (path);
					}
					g_free (uri);
					g_object_unref (settings);
				}
				if (schema)
					g_settings_schema_unref (schema);
				if (wallpaper)
					break;
			}
		}
		if (wallpaper == NULL) {
			const char *fallbacks[] = {
				"/usr/share/backgrounds/xfce/xfce-blue.jpg",
				"/usr/share/backgrounds/xfce/xfce-shapes.svg",
				"/usr/share/backgrounds/xfce/xfce-x.svg",
				"/usr/share/backgrounds/gnome/adwaita-l.jpg",
				"/usr/share/backgrounds/gnome/adwaita-l.webp",
				NULL
			};
			int i;
			for (i = 0; fallbacks[i]; i++) {
				if (g_file_test (fallbacks[i], G_FILE_TEST_EXISTS)) {
					wallpaper = g_strdup (fallbacks[i]);
					break;
				}
			}
		}
		if (desktop_css == NULL)
			desktop_css = gtk_css_provider_new ();
		if (wallpaper) {
			GdkTexture *tex = gdk_texture_new_from_filename (wallpaper, NULL);
			if (tex == NULL) {
				GdkPixbuf *pb = gdk_pixbuf_new_from_file (wallpaper, NULL);
				if (pb) {
					tex = gdk_texture_new_for_pixbuf (pb);
					g_object_unref (pb);
				}
			}
			if (tex) {
				g_object_set_data_full (G_OBJECT (window), "verne-wallpaper",
							tex, g_object_unref);
				verne_desktop_wallpaper_texture = tex;
			}
			gchar *escaped = g_uri_escape_string (wallpaper, "/:", TRUE);
			css = g_strdup_printf (
				"window.verne-desktop, window.nemo-desktop-window {"
				"  background-color: #2e3436;"
				"  background-image: url(\"file://%s\");"
				"  background-size: cover;"
				"  background-repeat: no-repeat;"
				"  background-position: center;"
				"}", escaped);
			g_free (escaped);
			g_free (wallpaper);
		} else {
			css = g_strdup (
				"window.verne-desktop, window.nemo-desktop-window {"
				"  background-color: #2e3436;"
				"}");
		}
		gtk_css_provider_load_from_string (desktop_css, css);
		g_free (css);
		gtk_style_context_add_provider_for_display (gdk_display_get_default (),
							    GTK_STYLE_PROVIDER (desktop_css),
							    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
	verne_window_ensure_realize_hook (window);
}

void
verne_paint_desktop_wallpaper (GtkWidget *widget, GtkSnapshot *snapshot, int width, int height)
{
	GdkTexture *tex;
	int tw, th;
	float scale, dw, dh, x, y;

	if (widget == NULL || snapshot == NULL || width <= 0 || height <= 0)
		return;
	tex = verne_desktop_wallpaper_for_widget (widget);
	if (tex == NULL)
		return;
	tw = gdk_texture_get_width (tex);
	th = gdk_texture_get_height (tex);
	if (tw <= 0 || th <= 0)
		return;
	scale = MAX ((float) width / (float) tw, (float) height / (float) th);
	dw = (float) tw * scale;
	dh = (float) th * scale;
	x = ((float) width - dw) / 2.0f;
	y = ((float) height - dh) / 2.0f;
	gtk_snapshot_append_scaled_texture (snapshot, tex, GSK_SCALING_FILTER_LINEAR,
					    &GRAPHENE_RECT_INIT (x, y, dw, dh));
}

void
verne_paint_desktop_wallpaper_cairo (GtkWidget *widget, cairo_t *cr, int width, int height)
{
	GdkTexture *tex;
	GdkPixbuf *pb;
	int tw, th;
	double scale, dw, dh, x, y;

	if (widget == NULL || cr == NULL || width <= 0 || height <= 0)
		return;
	tex = verne_desktop_wallpaper_for_widget (widget);
	if (tex == NULL)
		return;
	pb = gdk_pixbuf_get_from_texture (tex);
	if (pb == NULL)
		return;
	tw = gdk_pixbuf_get_width (pb);
	th = gdk_pixbuf_get_height (pb);
	if (tw <= 0 || th <= 0) {
		g_object_unref (pb);
		return;
	}
	scale = MAX ((double) width / (double) tw, (double) height / (double) th);
	dw = tw * scale;
	dh = th * scale;
	x = ((double) width - dw) / 2.0;
	y = ((double) height - dh) / 2.0;
	cairo_save (cr);
	cairo_translate (cr, x, y);
	cairo_scale (cr, scale, scale);
	gdk_cairo_set_source_pixbuf (cr, pb, 0, 0);
	cairo_paint (cr);
	cairo_restore (cr);
	g_object_unref (pb);
}

GdkWindowTypeHint
gtk_window_get_type_hint (GtkWindow *window)
{
	return (GdkWindowTypeHint) GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window), "verne-type-hint"));
}

void
gtk_window_set_skip_taskbar_hint (GtkWindow *window, gboolean setting)
{
	g_object_set_data (G_OBJECT (window), "verne-skip-taskbar", GINT_TO_POINTER (setting ? 1 : 2));
	verne_window_ensure_realize_hook (window);
	if (gtk_widget_get_realized (GTK_WIDGET (window)))
		verne_window_apply_x11 (window);
}

void
gtk_window_set_skip_pager_hint (GtkWindow *window, gboolean setting)
{
	g_object_set_data (G_OBJECT (window), "verne-skip-pager", GINT_TO_POINTER (setting ? 1 : 2));
	verne_window_ensure_realize_hook (window);
	if (gtk_widget_get_realized (GTK_WIDGET (window)))
		verne_window_apply_x11 (window);
}

static void
verne_x11_add_net_wm_state (Display *dpy, Window xid, Atom *atoms, int *n_atoms, Atom atom)
{
	int i;
	for (i = 0; i < *n_atoms; i++) {
		if (atoms[i] == atom)
			return;
	}
	if (*n_atoms < 8)
		atoms[(*n_atoms)++] = atom;
	(void) dpy;
	(void) xid;
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
	Atom state_atoms[8];
	int n_state = 0;
	gboolean skip_taskbar, skip_pager;

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

	skip_taskbar = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window), "verne-skip-taskbar")) == 1
		|| hint == GDK_WINDOW_TYPE_HINT_DESKTOP;
	skip_pager = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window), "verne-skip-pager")) == 1
		|| hint == GDK_WINDOW_TYPE_HINT_DESKTOP;
	if (skip_taskbar)
		verne_x11_add_net_wm_state (dpy, xid, state_atoms, &n_state,
					    XInternAtom (dpy, "_NET_WM_STATE_SKIP_TASKBAR", False));
	if (skip_pager)
		verne_x11_add_net_wm_state (dpy, xid, state_atoms, &n_state,
					    XInternAtom (dpy, "_NET_WM_STATE_SKIP_PAGER", False));
	if (hint == GDK_WINDOW_TYPE_HINT_DESKTOP) {
		verne_x11_add_net_wm_state (dpy, xid, state_atoms, &n_state,
					    XInternAtom (dpy, "_NET_WM_STATE_STICKY", False));
		verne_x11_add_net_wm_state (dpy, xid, state_atoms, &n_state,
					    XInternAtom (dpy, "_NET_WM_STATE_BELOW", False));
	}
	if (n_state > 0) {
		Atom state = XInternAtom (dpy, "_NET_WM_STATE", False);
		XChangeProperty (dpy, xid, state, XA_ATOM, 32, PropModeReplace,
				 (unsigned char *) state_atoms, n_state);
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

typedef struct {
	guint keyval;
	GdkModifierType modifiers;
	gchar *signal_name;
	GVariant *args;
} VerneBindEntry;

static void
verne_bind_entry_free (gpointer data)
{
	VerneBindEntry *e = data;
	if (e == NULL)
		return;
	g_free (e->signal_name);
	if (e->args)
		g_variant_unref (e->args);
	g_free (e);
}

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
		set->entries = g_ptr_array_new_with_free_func (verne_bind_entry_free);
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
	VerneBindEntry *entry;
	GVariantBuilder builder;
	GVariant *params = NULL;
	GtkShortcut *shortcut;
	guint i;

	g_return_if_fail (binding_set != NULL && binding_set->klass != NULL);
	g_return_if_fail (signal_name != NULL);

	va_start (args, n_args);
	if (n_args > 0) {
		g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);
		for (i = 0; i < n_args; i++) {
			GType t = va_arg (args, GType);
			if (t == G_TYPE_BOOLEAN)
				g_variant_builder_add (&builder, "b", (gboolean) va_arg (args, gint));
			else if (t == G_TYPE_INT || t == G_TYPE_ENUM || G_TYPE_IS_ENUM (t))
				g_variant_builder_add (&builder, "i", va_arg (args, gint));
			else if (t == G_TYPE_UINT)
				g_variant_builder_add (&builder, "u", va_arg (args, guint));
			else if (t == G_TYPE_STRING)
				g_variant_builder_add (&builder, "s", va_arg (args, const gchar *));
			else if (t == G_TYPE_DOUBLE)
				g_variant_builder_add (&builder, "d", va_arg (args, gdouble));
			else
				g_variant_builder_add (&builder, "i", va_arg (args, gint));
		}
		params = g_variant_ref_sink (g_variant_builder_end (&builder));
	}
	va_end (args);

	entry = g_new0 (VerneBindEntry, 1);
	entry->keyval = keyval;
	entry->modifiers = modifiers;
	entry->signal_name = g_strdup (signal_name);
	entry->args = params ? g_variant_ref (params) : NULL;
	if (binding_set->entries == NULL)
		binding_set->entries = g_ptr_array_new_with_free_func (verne_bind_entry_free);
	g_ptr_array_add (binding_set->entries, entry);

	shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (keyval, modifiers),
				     gtk_signal_action_new (signal_name));
	if (params)
		gtk_shortcut_set_arguments (shortcut, params);
	gtk_widget_class_add_shortcut (binding_set->klass, shortcut);
	g_object_unref (shortcut);
	if (params)
		g_variant_unref (params);
}

void
gtk_binding_entry_remove (GtkBindingSet *binding_set, guint keyval, GdkModifierType modifiers)
{
	guint i;

	if (binding_set == NULL || binding_set->entries == NULL)
		return;
	for (i = binding_set->entries->len; i-- > 0; ) {
		VerneBindEntry *e = g_ptr_array_index (binding_set->entries, i);
		if (e->keyval == keyval && e->modifiers == modifiers)
			g_ptr_array_remove_index (binding_set->entries, i);
	}
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
void gdk_window_raise (GdkSurface *window)
{
#ifdef GDK_WINDOWING_X11
	if (window && GDK_IS_X11_SURFACE (window))
		XRaiseWindow (gdk_x11_display_get_xdisplay (gdk_surface_get_display (window)),
			      gdk_x11_surface_get_xid (window));
#else
	(void) window;
#endif
}
void gdk_window_lower (GdkSurface *window)
{
#ifdef GDK_WINDOWING_X11
	if (window && GDK_IS_X11_SURFACE (window))
		XLowerWindow (gdk_x11_display_get_xdisplay (gdk_surface_get_display (window)),
			      gdk_x11_surface_get_xid (window));
#else
	(void) window;
#endif
}
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
		(gtk_info_bar_add_child) (bar, box);
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
	GdkEvent *copy;

	if (event == NULL)
		return NULL;
	if (!verne_gdk_event_is_synth (event))
		return (gdk_event_ref) ((GdkEvent *) event);
	copy = g_memdup2 (event, sizeof (GdkEvent));
	if (((guint) event->type == GDK_KEY_PRESS || (guint) event->type == GDK_KEY_RELEASE) &&
	    event->key.string != NULL)
		copy->key.string = g_strdup (event->key.string);
	return copy;
}

void
gdk_event_free (GdkEvent *event)
{
	if (event == NULL)
		return;
	if (!verne_gdk_event_is_synth (event)) {
		gdk_event_unref (event);
		return;
	}
	if ((guint) event->type == GDK_KEY_PRESS || (guint) event->type == GDK_KEY_RELEASE)
		g_free (event->key.string);
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

static void
on_related_action_active (GObject *action, GParamSpec *pspec, gpointer activatable)
{
	(void) pspec;
	if (GTK_IS_TOGGLE_BUTTON (activatable) && GTK_IS_TOGGLE_ACTION (action)) {
		g_object_set_data (G_OBJECT (activatable), "verne-syncing-toggle", GINT_TO_POINTER (1));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (activatable),
					      gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
		g_object_set_data (G_OBJECT (activatable), "verne-syncing-toggle", NULL);
	}
}

static void
on_related_toggle_toggled (GtkToggleButton *button, gpointer action)
{
	if (g_object_get_data (G_OBJECT (button), "verne-syncing-toggle"))
		return;
	if (gtk_toggle_button_get_active (button))
		gtk_action_activate (GTK_ACTION (action));
}

void
gtk_activatable_set_related_action (gpointer activatable, GtkAction *action)
{
	const gchar *icon;
	GtkWidget *image;

	if (activatable == NULL)
		return;
	g_object_set_data (G_OBJECT (activatable), "verne-action", action);
	if (action == NULL || !GTK_IS_WIDGET (activatable))
		return;

	if (GTK_IS_BUTTON (activatable)) {
		icon = gtk_action_get_icon_name (action);
		if (icon == NULL || icon[0] == '\0')
			icon = gtk_action_get_stock_id (action);
		if (icon && icon[0] != '\0') {
			image = gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_LARGE_TOOLBAR);
			gtk_button_set_child (GTK_BUTTON (activatable), image);
		}
		if (gtk_action_get_tooltip (action))
			gtk_widget_set_tooltip_text (GTK_WIDGET (activatable), gtk_action_get_tooltip (action));
		gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_get_sensitive (action));
		if (GTK_IS_TOGGLE_BUTTON (activatable) && GTK_IS_TOGGLE_ACTION (action)) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (activatable),
						      gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
			if (g_object_get_data (G_OBJECT (activatable), "verne-action-toggled") == NULL) {
				g_signal_connect (activatable, "toggled",
						  G_CALLBACK (on_related_toggle_toggled), action);
				g_object_set_data (G_OBJECT (activatable), "verne-action-toggled", GINT_TO_POINTER (1));
			}
			if (g_object_get_data (G_OBJECT (activatable), "verne-action-active") == NULL) {
				g_signal_connect_object (action, "notify::active",
							 G_CALLBACK (on_related_action_active),
							 activatable, 0);
				g_object_set_data (G_OBJECT (activatable), "verne-action-active", GINT_TO_POINTER (1));
			}
		} else if (g_object_get_data (G_OBJECT (activatable), "verne-action-clicked") == NULL) {
			g_signal_connect_swapped (activatable, "clicked", G_CALLBACK (gtk_action_activate), action);
			g_object_set_data (G_OBJECT (activatable), "verne-action-clicked", GINT_TO_POINTER (1));
		}
	}
}
GtkAction *gtk_activatable_get_related_action (gpointer activatable) {
	return g_object_get_data (G_OBJECT (activatable), "verne-action");
}

static void
verne_cell_renderer_apply_image (GtkCellRenderer *cell, GdkPixbuf *pixbuf, GdkTexture *texture)
{
	GObjectClass *klass;

	if (cell == NULL)
		return;
	klass = G_OBJECT_GET_CLASS (cell);
	/* g_object_set() stops at the first unknown property. GTK 4.14's
	 * GtkCellRendererPixbuf has pixbuf/texture, not paintable. */
	if (g_object_class_find_property (klass, "gicon"))
		g_object_set (cell, "gicon", NULL, NULL);
	if (g_object_class_find_property (klass, "icon-name"))
		g_object_set (cell, "icon-name", NULL, NULL);
	if (g_object_class_find_property (klass, "pixbuf"))
		g_object_set (cell, "pixbuf", pixbuf, NULL);
	if (g_object_class_find_property (klass, "texture"))
		g_object_set (cell, "texture", texture, NULL);
	if (g_object_class_find_property (klass, "paintable"))
		g_object_set (cell, "paintable",
			      texture ? GDK_PAINTABLE (texture) : NULL, NULL);
}

void
verne_cell_renderer_set_pixbuf (GtkCellRenderer *cell, GdkPixbuf *pixbuf)
{
	GdkTexture *texture = NULL;

	if (pixbuf)
		texture = gdk_texture_new_for_pixbuf (pixbuf);
	verne_cell_renderer_apply_image (cell, pixbuf, texture);
	g_clear_object (&texture);
}

static GdkTexture *
verne_texture_from_surface (cairo_surface_t *surface)
{
	GdkPixbuf *pixbuf;
	GdkTexture *texture;
	int w, h;

	if (surface == NULL || cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
		return NULL;
	if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_IMAGE)
		return NULL;
	w = cairo_image_surface_get_width (surface);
	h = cairo_image_surface_get_height (surface);
	if (w <= 0 || h <= 0)
		return NULL;
	pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, w, h);
	if (pixbuf == NULL)
		return NULL;
	texture = gdk_texture_new_for_pixbuf (pixbuf);
	g_object_unref (pixbuf);
	return texture;
}

static void
verne_cell_surface_data_func (GtkCellLayout *layout,
			      GtkCellRenderer *cell,
			      GtkTreeModel *model,
			      GtkTreeIter *iter,
			      gpointer data)
{
	cairo_surface_t *surface = NULL;
	GdkTexture *texture;
	(void) layout;
	gtk_tree_model_get (model, iter, GPOINTER_TO_INT (data), &surface, -1);
	texture = verne_texture_from_surface (surface);
	verne_cell_renderer_apply_image (cell, NULL, texture);
	g_clear_object (&texture);
	if (surface)
		cairo_surface_destroy (surface);
}

void
verne_tree_view_column_set_attributes (GtkTreeViewColumn *tree_column,
				       GtkCellRenderer *cell,
				       ...)
{
	va_list args;
	const gchar *attr;

	g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_column));
	g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

	gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (tree_column), cell);
	va_start (args, cell);
	while ((attr = va_arg (args, const gchar *)) != NULL) {
		int col = va_arg (args, int);
		if (g_strcmp0 (attr, "surface") == 0)
			gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (tree_column), cell,
							    verne_cell_surface_data_func,
							    GINT_TO_POINTER (col), NULL);
		else
			gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (tree_column), cell, attr, col);
	}
	va_end (args);
}
void gtk_activatable_set_use_action_appearance (gpointer activatable, gboolean use) { (void) activatable; (void) use; }
gboolean gtk_activatable_get_use_action_appearance (gpointer activatable) { (void) activatable; return TRUE; }

static gboolean
verne_emit_binding (GObject *object, VerneBindEntry *e)
{
	const gchar *name = e->signal_name;
	guint n;
	gchar *alt = NULL;

	if (g_signal_lookup (name, G_OBJECT_TYPE (object)) == 0) {
		alt = g_strdup (name);
		g_strdelimit (alt, "_", '-');
		if (g_signal_lookup (alt, G_OBJECT_TYPE (object)) != 0)
			name = alt;
		else {
			g_strdelimit (alt, "-", '_');
			if (g_signal_lookup (alt, G_OBJECT_TYPE (object)) != 0)
				name = alt;
			else {
				g_free (alt);
				return FALSE;
			}
		}
	}

	n = e->args ? g_variant_n_children (e->args) : 0;
	if (n == 0)
		g_signal_emit_by_name (object, name);
	else if (n == 1) {
		GVariant *c = g_variant_get_child_value (e->args, 0);
		if (g_variant_is_of_type (c, G_VARIANT_TYPE_STRING))
			g_signal_emit_by_name (object, name, g_variant_get_string (c, NULL));
		else if (g_variant_is_of_type (c, G_VARIANT_TYPE_BOOLEAN))
			g_signal_emit_by_name (object, name, g_variant_get_boolean (c));
		else if (g_variant_is_of_type (c, G_VARIANT_TYPE_INT32))
			g_signal_emit_by_name (object, name, g_variant_get_int32 (c));
		else
			g_signal_emit_by_name (object, name);
		g_variant_unref (c);
	} else if (n == 2 && g_variant_is_of_type (e->args, G_VARIANT_TYPE ("(ii)"))) {
		gint a, b;
		g_variant_get (e->args, "(ii)", &a, &b);
		g_signal_emit_by_name (object, name, a, b);
	} else if (n == 3 && g_variant_is_of_type (e->args, G_VARIANT_TYPE ("(iib)"))) {
		gint a, b;
		gboolean c;
		g_variant_get (e->args, "(iib)", &a, &b, &c);
		g_signal_emit_by_name (object, name, a, b, c);
	} else
		g_signal_emit_by_name (object, name);
	g_free (alt);
	return TRUE;
}

gboolean
gtk_bindings_activate_event (GObject *object, GdkEventKey *event)
{
	GType type;
	guint key;
	GdkModifierType mods;

	if (object == NULL || event == NULL || binding_sets == NULL)
		return FALSE;
	key = event->keyval;
	mods = event->state & gtk_accelerator_get_default_mod_mask ();
	for (type = G_OBJECT_TYPE (object); type != 0 && type != G_TYPE_OBJECT; type = g_type_parent (type)) {
		GtkWidgetClass *klass = g_type_class_peek (type);
		GtkBindingSet *set;
		guint i;

		if (klass == NULL)
			continue;
		set = g_hash_table_lookup (binding_sets, klass);
		if (set == NULL || set->entries == NULL)
			continue;
		for (i = 0; i < set->entries->len; i++) {
			VerneBindEntry *e = g_ptr_array_index (set->entries, i);
			if (e->modifiers != mods)
				continue;
			if (e->keyval != key &&
			    gdk_keyval_to_lower (e->keyval) != gdk_keyval_to_lower (key))
				continue;
			if (verne_emit_binding (object, e))
				return TRUE;
		}
	}
	return FALSE;
}

void gtk_propagate_event (GtkWidget *widget, GdkEvent *event)
{
	if (widget && event)
		gtk_widget_event (widget, event);
}

gboolean
gdk_event_get_scroll_deltas (const GdkEvent *event, gdouble *delta_x, gdouble *delta_y)
{
	if (delta_x)
		*delta_x = 0;
	if (delta_y)
		*delta_y = 0;
	if (event == NULL)
		return FALSE;
	if (verne_gdk_event_is_synth (event)) {
		if ((guint) event->type != GDK_SCROLL)
			return FALSE;
		if (delta_x)
			*delta_x = event->scroll.delta_x;
		if (delta_y)
			*delta_y = event->scroll.delta_y;
		return TRUE;
	}
	if ((gdk_event_get_event_type) ((GdkEvent *) event) != GDK_SCROLL)
		return FALSE;
	gdk_scroll_event_get_deltas ((GdkEvent *) event, delta_x, delta_y);
	return TRUE;
}

gboolean
gtk_widget_send_focus_change (GtkWidget *widget, GdkEvent *event)
{
	gboolean handled = FALSE;
	gboolean in;

	if (!GTK_IS_WIDGET (widget) || event == NULL)
		return FALSE;
	in = FALSE;
	if (verne_gdk_event_is_synth (event) &&
	    ((guint) event->type == GDK_FOCUS_CHANGE))
		in = event->focus_change.in;
	else if (!verne_gdk_event_is_synth (event) &&
		 gdk_event_get_event_type (event) == GDK_FOCUS_CHANGE)
		in = TRUE;
	if (in)
		gtk_widget_grab_focus (widget);
	g_signal_emit_by_name (widget, in ? "focus-in-event" : "focus-out-event", event, &handled);
	return handled;
}

gboolean
verne_im_context_filter_keypress (GtkIMContext *context, GdkEvent *event)
{
	GdkEventKey *key;
	gunichar ch;
	gchar buf[8];
	gint n;

	if (context == NULL || event == NULL)
		return FALSE;
	if (!verne_gdk_event_is_synth (event))
		return (gtk_im_context_filter_keypress) (context, event);

	if ((guint) event->type != GDK_KEY_PRESS)
		return FALSE;
	key = &event->key;
	if (key->state & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK | GDK_META_MASK))
		return FALSE;
	ch = gdk_keyval_to_unicode (key->keyval);
	if (ch == 0 || g_unichar_iscntrl (ch))
		return FALSE;
	n = g_unichar_to_utf8 (ch, buf);
	if (n <= 0)
		return FALSE;
	buf[n] = '\0';
	g_signal_emit_by_name (context, "commit", buf);
	return TRUE;
}

GtkWidget *
gtk_image_new_from_surface (cairo_surface_t *surface)
{
	GtkWidget *image = gtk_image_new ();
	gtk_image_set_from_surface (GTK_IMAGE (image), surface);
	return image;
}
void
gtk_image_set_from_surface (GtkImage *image, cairo_surface_t *surface)
{
	GdkTexture *texture;
	if (image == NULL)
		return;
	texture = verne_texture_from_surface (surface);
	gtk_image_set_from_paintable (image, GDK_PAINTABLE (texture));
	g_clear_object (&texture);
}
void gtk_render_icon_surface (GtkStyleContext *context, cairo_t *cr, cairo_surface_t *surface, gdouble x, gdouble y) {
	(void) context;
	if (surface) {
		cairo_set_source_surface (cr, surface, x, y);
		cairo_paint (cr);
	}
}

static GtkIconLookupFlags
verne_icon_lookup_flags (const gchar *name)
{
	if (name != NULL && g_str_has_suffix (name, "-symbolic"))
		return GTK_ICON_LOOKUP_FORCE_SYMBOLIC;
	return 0;
}

static gboolean
verne_icon_is_missing (gpointer paintable)
{
	const char *found;

	if (paintable == NULL)
		return TRUE;
	if (!GTK_IS_ICON_PAINTABLE (paintable))
		return FALSE;
	found = gtk_icon_paintable_get_icon_name (GTK_ICON_PAINTABLE (paintable));
	if (found == NULL || found[0] == '\0')
		return TRUE;
	return g_strcmp0 (found, "image-missing") == 0 ||
	       g_strcmp0 (found, "image-x-generic") == 0 ||
	       g_strcmp0 (found, "text-x-generic") == 0;
}

static const char *verne_drive_fallbacks[] = {
	"computer-symbolic",
	"drive-harddisk",
	"computer",
	"folder-symbolic",
	NULL
};

static gpointer
verne_lookup_named_icon (GtkIconTheme *theme, const gchar *name, gint size, gint scale)
{
	const gchar *mapped;
	gpointer paintable;
	GtkIconLookupFlags flags;
	const char **fallbacks = NULL;

	mapped = verne_map_icon_name (name);
	if (mapped == NULL || mapped[0] == '\0')
		return NULL;
	if (g_strcmp0 (mapped, "drive-harddisk-symbolic") == 0 ||
	    g_strcmp0 (mapped, "drive-harddisk") == 0)
		fallbacks = verne_drive_fallbacks;
	flags = verne_icon_lookup_flags (mapped);
	paintable = gtk_icon_theme_lookup_icon (theme, mapped, fallbacks,
						size, scale, GTK_TEXT_DIR_NONE, flags);
	if (verne_icon_is_missing (paintable) && flags != 0) {
		if (paintable)
			g_object_unref (paintable);
		paintable = gtk_icon_theme_lookup_icon (theme, mapped, fallbacks,
							size, scale, GTK_TEXT_DIR_NONE, 0);
	}
	if (verne_icon_is_missing (paintable)) {
		if (paintable)
			g_object_unref (paintable);
		paintable = NULL;
	}
	return paintable;
}

gpointer
gtk_icon_theme_lookup_icon_for_scale (GtkIconTheme *theme, const gchar *name, gint size, gint scale, GtkIconLookupFlags flags)
{
	gpointer paintable;

	(void) flags;
	paintable = verne_lookup_named_icon (theme, name, size, scale);
	if (paintable)
		return paintable;
	return gtk_icon_theme_lookup_icon (theme, verne_map_icon_name (name), NULL, size, scale,
					   GTK_TEXT_DIR_NONE, 0);
}

static GIcon *
verne_map_gicon (GIcon *icon)
{
	const gchar *const *names;
	gchar **mapped;
	gboolean changed = FALSE;
	int i, n;
	GIcon *result;

	if (!G_IS_THEMED_ICON (icon))
		return g_object_ref (icon);
	names = g_themed_icon_get_names (G_THEMED_ICON (icon));
	if (names == NULL)
		return g_object_ref (icon);
	n = (int) g_strv_length ((gchar **) names);
	mapped = g_new0 (gchar *, n + 4);
	for (i = 0; i < n; i++) {
		const gchar *m = verne_map_icon_name (names[i]);
		if (m != names[i])
			changed = TRUE;
		mapped[i] = g_strdup (m);
	}
	/* Adwaita may ship computer-symbolic but not a usable drive-harddisk pixbuf. */
	if (n > 0 && (g_strcmp0 (mapped[0], "drive-harddisk-symbolic") == 0 ||
		      g_strcmp0 (mapped[0], "drive-harddisk") == 0)) {
		mapped[n++] = g_strdup ("computer-symbolic");
		mapped[n++] = g_strdup ("drive-harddisk");
		mapped[n++] = g_strdup ("computer");
		changed = TRUE;
	}
	if (!changed) {
		g_strfreev (mapped);
		return g_object_ref (icon);
	}
	result = g_themed_icon_new_from_names (mapped, -1);
	g_strfreev (mapped);
	return result;
}

gpointer
gtk_icon_theme_lookup_by_gicon_for_scale (GtkIconTheme *theme, GIcon *icon, gint size, gint scale, GtkIconLookupFlags flags)
{
	GIcon *mapped;
	gpointer paintable = NULL;
	(void) flags;

	mapped = icon ? verne_map_gicon (icon) : NULL;
	if (mapped && G_IS_THEMED_ICON (mapped)) {
		const gchar *const *names = g_themed_icon_get_names (G_THEMED_ICON (mapped));
		int i;
		for (i = 0; names && names[i]; i++) {
			paintable = verne_lookup_named_icon (theme, names[i], size, scale);
			if (paintable)
				break;
		}
	}
	if (paintable == NULL) {
		const gchar *first = NULL;
		if (mapped && G_IS_THEMED_ICON (mapped)) {
			const gchar *const *names = g_themed_icon_get_names (G_THEMED_ICON (mapped));
			if (names)
				first = names[0];
		}
		paintable = gtk_icon_theme_lookup_by_gicon (theme, mapped ? mapped : icon, size, scale,
							    GTK_TEXT_DIR_NONE,
							    verne_icon_lookup_flags (first));
	}
	g_clear_object (&mapped);
	return paintable;
}

static GQuark
verne_icon_filename_quark (void)
{
	static GQuark q;
	if (q == 0)
		q = g_quark_from_static_string ("verne-icon-filename");
	return q;
}

static GdkPixbuf *
verne_pixbuf_from_paintable (GdkPaintable *paintable, int size, GError **error)
{
	GdkPixbuf *pixbuf = NULL;
	int w, h;
	cairo_surface_t *surface;
	cairo_t *cr;
	GtkSnapshot *snapshot;
	GskRenderNode *node;

	if (!GDK_IS_PAINTABLE (paintable))
		return NULL;

	if (GTK_IS_ICON_PAINTABLE (paintable)) {
		GFile *file = gtk_icon_paintable_get_file (GTK_ICON_PAINTABLE (paintable));
		const char *icon_name = gtk_icon_paintable_get_icon_name (GTK_ICON_PAINTABLE (paintable));
		gboolean symbolic = gtk_icon_paintable_is_symbolic (GTK_ICON_PAINTABLE (paintable)) ||
				    (icon_name != NULL && g_str_has_suffix (icon_name, "-symbolic"));
		/* Symbolic SVGs use currentColor; gdk-pixbuf draws them as a
		 * generic filled page. Snapshot recolors through GTK. */
		if (file && !symbolic) {
			char *path = g_file_get_path (file);
			int load_size = size > 0 ? size : 48;
			if (path)
				pixbuf = gdk_pixbuf_new_from_file_at_size (path, load_size, load_size, error);
			g_free (path);
			g_object_unref (file);
			if (pixbuf)
				return pixbuf;
			if (error)
				g_clear_error (error);
		} else if (file) {
			g_object_unref (file);
		}
	}

	w = gdk_paintable_get_intrinsic_width (paintable);
	h = gdk_paintable_get_intrinsic_height (paintable);
	if (w <= 0)
		w = size > 0 ? size : 48;
	if (h <= 0)
		h = size > 0 ? size : 48;

	snapshot = gtk_snapshot_new ();
	gdk_paintable_snapshot (paintable, snapshot, w, h);
	node = gtk_snapshot_free_to_node (snapshot);
	if (node) {
		GskRenderer *renderer = gsk_cairo_renderer_new ();
		GdkDisplay *display = gdk_display_get_default ();
		if (display && gsk_renderer_realize_for_display (renderer, display, NULL)) {
			GdkTexture *texture = gsk_renderer_render_texture (renderer, node,
									   &GRAPHENE_RECT_INIT (0, 0, w, h));
			gsk_renderer_unrealize (renderer);
			if (texture) {
				pixbuf = gdk_pixbuf_get_from_texture (texture);
				g_object_unref (texture);
			}
		}
		g_object_unref (renderer);
		if (pixbuf == NULL) {
			surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
			cr = cairo_create (surface);
			gsk_render_node_draw (node, cr);
			cairo_destroy (cr);
			pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, w, h);
			cairo_surface_destroy (surface);
		}
		gsk_render_node_unref (node);
	}
	return pixbuf;
}

GdkPixbuf *
gtk_icon_info_load_icon (gpointer info, GError **error)
{
	int size = 0;

	if (info == NULL)
		return NULL;
	if (GDK_IS_PAINTABLE (info))
		size = gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (info));
	return verne_pixbuf_from_paintable (GDK_IS_PAINTABLE (info) ? GDK_PAINTABLE (info) : NULL, size, error);
}

const gchar *
gtk_icon_info_get_filename (gpointer info)
{
	char *path;
	GFile *file;

	if (info == NULL || !GTK_IS_ICON_PAINTABLE (info))
		return NULL;
	path = g_object_get_qdata (G_OBJECT (info), verne_icon_filename_quark ());
	if (path)
		return path;
	file = gtk_icon_paintable_get_file (GTK_ICON_PAINTABLE (info));
	if (file == NULL)
		return NULL;
	path = g_file_get_path (file);
	g_object_unref (file);
	if (path)
		g_object_set_qdata_full (G_OBJECT (info), verne_icon_filename_quark (), path, g_free);
	return path;
}
GtkIconSize gtk_icon_size_from_name (const gchar *name) { (void) name; return GTK_ICON_SIZE_NORMAL; }

typedef struct {
	GtkFileFilterFunc func;
	gpointer data;
	GDestroyNotify notify;
	GtkFileFilterFlags needed;
} VerneCustomFilter;

static void
verne_custom_filter_free (gpointer p)
{
	VerneCustomFilter *c = p;
	if (c->notify)
		c->notify (c->data);
	g_free (c);
}

static GQuark
verne_custom_filter_quark (void)
{
	static GQuark q;
	if (q == 0)
		q = g_quark_from_static_string ("verne-custom-file-filter");
	return q;
}

void
gtk_file_filter_add_custom (GtkFileFilter *filter, GtkFileFilterFlags needed, GtkFileFilterFunc func, gpointer data, GDestroyNotify notify)
{
	VerneCustomFilter *c;

	if (filter == NULL)
		return;
	c = g_new0 (VerneCustomFilter, 1);
	c->func = func;
	c->data = data;
	c->notify = notify;
	c->needed = needed;
	g_object_set_qdata_full (G_OBJECT (filter), verne_custom_filter_quark (), c, verne_custom_filter_free);
	/* GTK4 GtkFileFilter has no custom listing hook. Approximate the common
	 * Nemo cases in the chooser, then enforce the real callback on accept. */
	if (needed & GTK_FILE_FILTER_FILENAME) {
		gtk_file_filter_add_mime_type (filter, "application/x-executable");
		gtk_file_filter_add_mime_type (filter, "application/x-pie-executable");
		gtk_file_filter_add_mime_type (filter, "application/x-sharedlib");
		gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
		gtk_file_filter_add_mime_type (filter, "application/x-desktop");
		gtk_file_filter_add_pattern (filter, "*.sh");
		gtk_file_filter_add_pattern (filter, "*.desktop");
		gtk_file_filter_add_pattern (filter, "*.py");
		gtk_file_filter_add_pattern (filter, "*.pl");
		gtk_file_filter_add_pattern (filter, "*.rb");
	} else {
		gtk_file_filter_add_pattern (filter, "*");
	}
}

gboolean
verne_file_filter_has_custom (GtkFileFilter *filter)
{
	VerneCustomFilter *c;

	if (filter == NULL)
		return FALSE;
	c = g_object_get_qdata (G_OBJECT (filter), verne_custom_filter_quark ());
	return c != NULL && c->func != NULL;
}

gboolean
verne_file_filter_accepts_file (GtkFileFilter *filter, GFile *file)
{
	VerneCustomFilter *c;
	GtkFileFilterInfo info = { 0 };
	gchar *filename = NULL;
	gchar *uri = NULL;
	gchar *display = NULL;
	gchar *mime = NULL;
	gboolean ok = TRUE;

	if (filter == NULL || file == NULL)
		return TRUE;
	c = g_object_get_qdata (G_OBJECT (filter), verne_custom_filter_quark ());
	if (c == NULL || c->func == NULL)
		return TRUE;
	if (c->needed & GTK_FILE_FILTER_FILENAME) {
		filename = g_file_get_path (file);
		info.filename = filename;
		info.contains |= GTK_FILE_FILTER_FILENAME;
	}
	if (c->needed & GTK_FILE_FILTER_URI) {
		uri = g_file_get_uri (file);
		info.uri = uri;
		info.contains |= GTK_FILE_FILTER_URI;
	}
	if (c->needed & GTK_FILE_FILTER_DISPLAY_NAME) {
		display = g_file_get_basename (file);
		info.display_name = display;
		info.contains |= GTK_FILE_FILTER_DISPLAY_NAME;
	}
	if (c->needed & GTK_FILE_FILTER_MIME_TYPE) {
		GFileInfo *fi = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
						     G_FILE_QUERY_INFO_NONE, NULL, NULL);
		if (fi) {
			mime = g_strdup (g_file_info_get_content_type (fi));
			g_object_unref (fi);
		}
		info.mime_type = mime;
		info.contains |= GTK_FILE_FILTER_MIME_TYPE;
	}
	ok = c->func (&info, c->data);
	g_free (filename);
	g_free (uri);
	g_free (display);
	g_free (mime);
	return ok;
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

void
gtk_widget_override_background_color (GtkWidget *widget, GtkStateFlags state, const GdkRGBA *color)
{
	GtkCssProvider *provider;
	gchar *css;
	(void) state;
	if (widget == NULL || color == NULL)
		return;
	/* Fully transparent overrides hide wallpaper CSS on desktop windows. */
	if (color->alpha <= 0.01)
		return;
	css = g_strdup_printf ("* { background-color: rgba(%d,%d,%d,%g); }",
			       (int) (color->red * 255.0),
			       (int) (color->green * 255.0),
			       (int) (color->blue * 255.0),
			       color->alpha);
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_string (provider, css);
	gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
					GTK_STYLE_PROVIDER (provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (provider);
	g_free (css);
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

const gchar *
verne_dialog_button_label (const gchar *text)
{
	if (text == NULL || text[0] == '\0' || text[0] == '_')
		return text;
	if (strchr (text, ' ') != NULL)
		return text;
	if (g_strcmp0 (text, "cancel") == 0 || g_strcmp0 (text, "gtk-cancel") == 0)
		return "_Cancel";
	if (g_strcmp0 (text, "ok") == 0 || g_strcmp0 (text, "gtk-ok") == 0)
		return "_OK";
	if (g_strcmp0 (text, "document-open") == 0 || g_strcmp0 (text, "gtk-open") == 0)
		return "_Open";
	if (g_strcmp0 (text, "document-save") == 0 || g_strcmp0 (text, "gtk-save") == 0)
		return "_Save";
	if (g_strcmp0 (text, "document-save-as") == 0)
		return "Save _As";
	if (g_strcmp0 (text, "window-close") == 0 || g_strcmp0 (text, "gtk-close") == 0)
		return "_Close";
	if (g_strcmp0 (text, "apply") == 0 || g_strcmp0 (text, "gtk-apply") == 0)
		return "_Apply";
	if (g_strcmp0 (text, "help-browser") == 0 || g_strcmp0 (text, "gtk-help") == 0)
		return "_Help";
	if (g_strcmp0 (text, "gtk-yes") == 0)
		return "_Yes";
	if (g_strcmp0 (text, "gtk-no") == 0)
		return "_No";
	return text;
}

GtkWidget *
verne_dialog_add_button (GtkDialog *dialog, const gchar *text, gint response)
{
	return (gtk_dialog_add_button) (dialog, verne_dialog_button_label (text), response);
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
	va_list args;
	const gchar *name;
	GtkWidget *widget = gtk_style_context_get_widget_or_null (context);

	va_start (args, context);
	while ((name = va_arg (args, const gchar *)) != NULL) {
		gpointer dest = va_arg (args, gpointer);
		GParamSpec *pspec = NULL;

		if (dest == NULL)
			continue;
		if (widget)
			pspec = lookup_style_pspec (widget, name);
		if (g_str_has_suffix (name, "color") || (pspec && G_IS_PARAM_SPEC_BOXED (pspec)))
			*(gpointer *) dest = NULL;
		else if (pspec && G_IS_PARAM_SPEC_BOOLEAN (pspec))
			*(gboolean *) dest = G_PARAM_SPEC_BOOLEAN (pspec)->default_value;
		else if (pspec && G_IS_PARAM_SPEC_INT (pspec))
			*(gint *) dest = G_PARAM_SPEC_INT (pspec)->default_value;
		else
			*(gint *) dest = 0;
	}
	va_end (args);
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

