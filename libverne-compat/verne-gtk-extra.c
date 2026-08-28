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
#include <sys/types.h>
#include <pango/pangocairo.h>
#include <graphene.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
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
	static GdkTexture *cached_tex;
	static GdkPixbuf *cached_pb;
	static cairo_surface_t *cached_surface;
	static int cached_w, cached_h;
	int tw, th;
	double scale, dw, dh, x, y;

	if (widget == NULL || cr == NULL || width <= 0 || height <= 0)
		return;
	tex = verne_desktop_wallpaper_for_widget (widget);
	if (tex == NULL)
		return;

	if (cached_tex != tex) {
		g_clear_object (&cached_pb);
		g_clear_pointer (&cached_surface, cairo_surface_destroy);
		cached_tex = tex;
		cached_pb = gdk_pixbuf_get_from_texture (tex);
		cached_w = 0;
		cached_h = 0;
	}
	if (cached_pb == NULL)
		return;

	if (cached_surface == NULL || cached_w != width || cached_h != height) {
		cairo_t *scr;
		cairo_pattern_t *pat;

		g_clear_pointer (&cached_surface, cairo_surface_destroy);
		cached_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
		if (cairo_surface_status (cached_surface) != CAIRO_STATUS_SUCCESS) {
			g_clear_pointer (&cached_surface, cairo_surface_destroy);
			return;
		}
		scr = cairo_create (cached_surface);
		tw = gdk_pixbuf_get_width (cached_pb);
		th = gdk_pixbuf_get_height (cached_pb);
		if (tw > 0 && th > 0) {
			scale = MAX ((double) width / (double) tw, (double) height / (double) th);
			dw = tw * scale;
			dh = th * scale;
			x = ((double) width - dw) / 2.0;
			y = ((double) height - dh) / 2.0;
			cairo_translate (scr, x, y);
			cairo_scale (scr, scale, scale);
			gdk_cairo_set_source_pixbuf (scr, cached_pb, 0, 0);
			pat = cairo_get_source (scr);
			cairo_pattern_set_filter (pat, CAIRO_FILTER_BILINEAR);
			cairo_paint (scr);
		}
		cairo_destroy (scr);
		cached_w = width;
		cached_h = height;
	}

	cairo_save (cr);
	cairo_set_source_surface (cr, cached_surface, 0, 0);
	cairo_paint (cr);
	cairo_restore (cr);
}

gboolean
verne_desktop_canvas_snapshot (GtkWidget *widget, GtkSnapshot *snapshot, int width, int height,
			       VerneDrawEvent draw,
			       void (*emit_draw) (GtkWidget *, cairo_t *))
{
	cairo_t *cr;

	if (widget == NULL || snapshot == NULL || width <= 0 || height <= 0)
		return FALSE;
	if (verne_desktop_wallpaper_for_widget (widget) == NULL)
		return FALSE;

	/* Wallpaper is a stable GdkTexture. Icon labels must be painted every
	 * snapshot: a combined dest-tex cache kept F2/rename text stale even
	 * after dest-dirty, because GTK4 can snapshot dest without going
	 * through queue_draw. */
	verne_paint_desktop_wallpaper (widget, snapshot, width, height);
	cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));
	if (draw)
		draw (widget, cr);
	if (emit_draw)
		emit_draw (widget, cr);
	cairo_destroy (cr);
	return TRUE;
}

#undef gtk_widget_queue_draw
void
verne_gtk_widget_queue_draw (GtkWidget *widget)
{
	GtkWidget *w;

	if (widget != NULL && verne_desktop_wallpaper_for_widget (widget) != NULL) {
		/* EelCanvas draw/update often queues another expose during
		 * dest snapshot. Mark dirty and replay after snapshot so F2
		 * label updates are not stuck on the cached texture. */
		if (g_object_get_data (G_OBJECT (widget), "verne-in-snapshot") != NULL) {
			g_object_set_data (G_OBJECT (widget), "verne-dest-dirty", GINT_TO_POINTER (1));
			g_object_set_data (G_OBJECT (widget), "verne-dest-redraw-after", GINT_TO_POINTER (1));
			return;
		}
		/* Snapshot reads verne-dest-dirty on the canvas, not on a
		 * child label / icon item. Mark every dest ancestor dirty. */
		for (w = widget; GTK_IS_WIDGET (w); w = gtk_widget_get_parent (w)) {
			if (g_object_get_data (G_OBJECT (w), "verne-in-snapshot") != NULL) {
				g_object_set_data (G_OBJECT (w), "verne-dest-dirty", GINT_TO_POINTER (1));
				g_object_set_data (G_OBJECT (w), "verne-dest-redraw-after", GINT_TO_POINTER (1));
				continue;
			}
			g_object_set_data (G_OBJECT (w), "verne-dest-dirty", GINT_TO_POINTER (1));
		}
	}
	gtk_widget_queue_draw (widget);
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

static gboolean verne_window_is_keep_native (GtkWidget *w);

static void
verne_x11_hide_dummy_natives (GtkWindow *keep)
{
#ifdef GDK_WINDOWING_X11
	GdkDisplay *gdpy;
	Display *dpy;
	Window root, parent = None, *children = NULL, keep_xid = 0;
	unsigned int n = 0, i;
	pid_t pid = getpid ();
	Atom pid_atom;
	GHashTable *live;
	GList *toplevels, *l;

	gdpy = gdk_display_get_default ();
	if (gdpy == NULL || !GDK_IS_X11_DISPLAY (gdpy))
		return;
	dpy = gdk_x11_display_get_xdisplay (gdpy);
	live = g_hash_table_new (g_direct_hash, g_direct_equal);
	toplevels = gtk_window_list_toplevels ();
	for (l = toplevels; l; l = l->next) {
		GtkWidget *w = l->data;
		GtkNative *native;
		GdkSurface *surf;
		Window xid = 0;
		int ww = 0, hh = 0;

		if (!GTK_IS_WINDOW (w))
			continue;
		native = gtk_widget_get_native (w);
		surf = native ? gtk_native_get_surface (native) : NULL;
		if (surf && GDK_IS_X11_SURFACE (surf))
			xid = gdk_x11_surface_get_xid (GDK_X11_SURFACE (surf));
		ww = gtk_widget_get_width (w);
		hh = gtk_widget_get_height (w);
		/* Abandoned 1×1 natives stay in gtk_window_list_toplevels.
		 * Protect mapped File/dest windows, dialogs, and shortcuts.
		 * Never hide a window that has a real default size — GtkShortcutsWindow
		 * and Adw dialogs are 1×1 until the first allocate. */
		if (xid && (w == GTK_WIDGET (keep) || verne_window_is_keep_native (w) || ww > 8 || hh > 8))
			g_hash_table_add (live, GUINT_TO_POINTER (xid));
		if (w != GTK_WIDGET (keep) && !verne_window_is_keep_native (w) &&
		    !GTK_IS_MENU (w) && ww <= 2 && hh <= 2 &&
		    gtk_widget_get_visible (w))
			gtk_widget_set_visible (w, FALSE);
	}
	g_list_free (toplevels);
	if (GTK_IS_WINDOW (keep)) {
		GtkNative *native = gtk_widget_get_native (GTK_WIDGET (keep));
		GdkSurface *surf = native ? gtk_native_get_surface (native) : NULL;

		if (surf && GDK_IS_X11_SURFACE (surf))
			keep_xid = gdk_x11_surface_get_xid (GDK_X11_SURFACE (surf));
		if (keep_xid)
			g_hash_table_add (live, GUINT_TO_POINTER (keep_xid));
	}

	root = DefaultRootWindow (dpy);
	pid_atom = XInternAtom (dpy, "_NET_WM_PID", True);
	if (!XQueryTree (dpy, root, &root, &parent, &children, &n) || children == NULL) {
		g_hash_table_unref (live);
		return;
	}
	gdk_x11_display_error_trap_push (gdk_display_get_default ());
	for (i = 0; i < n; i++) {
		Window w = children[i];
		XWindowAttributes attrs;
		Atom actual = None;
		int fmt = 0;
		unsigned long nitems = 0, after = 0;
		unsigned char *prop = NULL;
		pid_t wpid = 0;
		XSetWindowAttributes sa;

		if (g_hash_table_contains (live, GUINT_TO_POINTER (w)))
			continue;
		if (!XGetWindowAttributes (dpy, w, &attrs))
			continue;
		if (attrs.map_state != IsViewable)
			continue;
		if (attrs.width > 2 || attrs.height > 2)
			continue;
		if (pid_atom != None &&
		    XGetWindowProperty (dpy, w, pid_atom, 0, 1, False, XA_CARDINAL,
					&actual, &fmt, &nitems, &after, &prop) == Success &&
		    prop != NULL) {
			if (nitems >= 1)
				wpid = (pid_t) *(unsigned long *) prop;
			XFree (prop);
		}
		if (wpid != pid)
			continue;
		sa.override_redirect = True;
		XChangeWindowAttributes (dpy, w, CWOverrideRedirect, &sa);
		{
			XWMHints *hints = XGetWMHints (dpy, w);
			Atom protocols[8];
			int nprot = 0;
			Atom wm_protocols = XInternAtom (dpy, "WM_PROTOCOLS", False);
			Atom take_focus = XInternAtom (dpy, "WM_TAKE_FOCUS", False);
			Atom ping = XInternAtom (dpy, "_NET_WM_PING", False);
			Atom actual_p = None;
			int fmt_p = 0;
			unsigned long nitems_p = 0, after_p = 0;
			unsigned char *pdata = NULL;
			int p;

			if (hints == NULL)
				hints = XAllocWMHints ();
			if (hints) {
				hints->flags |= InputHint | StateHint;
				hints->input = False;
				hints->initial_state = WithdrawnState;
				XSetWMHints (dpy, w, hints);
				XFree (hints);
			}
			if (XGetWindowProperty (dpy, w, wm_protocols, 0, 8, False, XA_ATOM,
						&actual_p, &fmt_p, &nitems_p, &after_p, &pdata) == Success &&
			    pdata != NULL) {
				Atom *old = (Atom *) pdata;
				for (p = 0; p < (int) nitems_p && nprot < 8; p++) {
					if (old[p] != take_focus)
						protocols[nprot++] = old[p];
				}
				XFree (pdata);
				if (nprot == 0) {
					protocols[nprot++] = ping;
				}
				XSetWMProtocols (dpy, w, protocols, nprot);
			}
		}
		XWithdrawWindow (dpy, w, DefaultScreen (dpy));
		XUnmapWindow (dpy, w);
		XMoveWindow (dpy, w, -2000, -2000);
		g_warning ("verne: hid dummy 1x1 native xid=0x%lx", (unsigned long) w);
	}
	gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
	XFree (children);
	g_hash_table_unref (live);
#else
	(void) keep;
#endif
}

static gboolean
verne_window_is_keep_native (GtkWidget *w)
{
	int dw = 0, dh = 0;
	int ww, hh;

	if (!GTK_IS_WINDOW (w) || GTK_IS_MENU (w))
		return FALSE;
	if (g_object_get_data (G_OBJECT (w), "verne-keep-native") != NULL)
		return TRUE;
	if (GTK_IS_DIALOG (w))
		return TRUE;
	if (GTK_IS_SHORTCUTS_WINDOW (w))
		return TRUE;
#ifdef ADW_TYPE_ABOUT_WINDOW
	if (ADW_IS_ABOUT_WINDOW (w))
		return TRUE;
#endif
	gtk_window_get_default_size (GTK_WINDOW (w), &dw, &dh);
	if (dw > 8 || dh > 8)
		return TRUE;
	ww = gtk_widget_get_width (w);
	hh = gtk_widget_get_height (w);
	return ww > 8 || hh > 8;
}

void
verne_window_keep_native (GtkWindow *window)
{
	if (GTK_IS_WINDOW (window))
		g_object_set_data (G_OBJECT (window), "verne-keep-native", GINT_TO_POINTER (1));
}

static void
verne_x11_lower_desktop_toplevels (void)
{
#ifdef GDK_WINDOWING_X11
	GListModel *model = gtk_window_get_toplevels ();
	guint i, n = g_list_model_get_n_items (model);

	for (i = 0; i < n; i++) {
		gpointer w = g_list_model_get_item (model, i);
		GdkSurface *s;

		if (w && GTK_IS_WINDOW (w) &&
		    (gtk_window_get_type_hint (GTK_WINDOW (w)) == GDK_WINDOW_TYPE_HINT_DESKTOP ||
		     g_object_get_data (G_OBJECT (w), "is_desktop_window") != NULL)) {
			s = gtk_native_get_surface (GTK_NATIVE (w));
			if (s && GDK_IS_X11_SURFACE (s)) {
				Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));

				gdk_x11_display_error_trap_push (gdk_display_get_default ());
				XLowerWindow (dpy, gdk_x11_surface_get_xid (s));
				gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
			}
		}
		if (w)
			g_object_unref (w);
	}
#endif
}

void
verne_window_present_keep (GtkWindow *window)
{
	if (!GTK_IS_WINDOW (window))
		return;
	verne_window_keep_native (window);
	gtk_widget_set_visible (GTK_WIDGET (window), TRUE);
	(gtk_window_present) (window);
#ifdef GDK_WINDOWING_X11
	{
		GtkNative *native = gtk_widget_get_native (GTK_WIDGET (window));
		GdkSurface *surface = native ? gtk_native_get_surface (native) : NULL;

		if (surface && GDK_IS_X11_SURFACE (surface)) {
			Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (surface));
			Window xid = gdk_x11_surface_get_xid (GDK_X11_SURFACE (surface));
			gboolean is_dest;

			is_dest = gtk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_DESKTOP ||
				  g_object_get_data (G_OBJECT (window), "is_desktop_window") != NULL;
			if (dpy && xid) {
				gdk_x11_display_error_trap_push (gdk_display_get_default ());
				if (is_dest)
					XLowerWindow (dpy, xid);
				else {
					verne_x11_lower_desktop_toplevels ();
					XRaiseWindow (dpy, xid);
				}
				gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
			}
		}
	}
#endif
}

GtkWidget *
verne_adw_window_from_body (GtkWidget *body, const char *title, int width, int height)
{
	GtkWidget *win;
	GtkWidget *header;
	GtkWidget *toolbar;

	if (width < 320)
		width = 320;
	if (height < 240)
		height = 240;

	win = g_object_new (ADW_TYPE_WINDOW, NULL);
	gtk_window_set_title (GTK_WINDOW (win), title ? title : "");
	gtk_window_set_default_size (GTK_WINDOW (win), width, height);
	gtk_widget_set_size_request (win, MAX (width - 80, 320), MAX (height - 80, 240));
	gtk_window_set_hide_on_close (GTK_WINDOW (win), TRUE);
	verne_window_keep_native (GTK_WINDOW (win));

	header = adw_header_bar_new ();
	toolbar = adw_toolbar_view_new ();
	adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar), header);
	if (GTK_IS_WIDGET (body)) {
		GtkWidget *parent = gtk_widget_get_parent (body);

		g_object_ref (body);
		if (GTK_IS_WINDOW (parent) && gtk_window_get_child (GTK_WINDOW (parent)) == body)
			gtk_window_set_child (GTK_WINDOW (parent), NULL);
		else if (parent != NULL)
			gtk_widget_unparent (body);
		gtk_widget_set_hexpand (body, TRUE);
		gtk_widget_set_vexpand (body, TRUE);
		adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar), body);
		g_object_unref (body);
	}
	adw_window_set_content (ADW_WINDOW (win), toolbar);
	return win;
}

void
verne_gtk_window_present (GtkWindow *window)
{
	if (!GTK_IS_WINDOW (window))
		return;
	if (gtk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_DESKTOP ||
	    g_object_get_data (G_OBJECT (window), "is_desktop_window") != NULL) {
		GtkWidget *widget = GTK_WIDGET (window);
		GdkSurface *surface;

		if (!gtk_widget_get_visible (widget))
			gtk_widget_set_visible (widget, TRUE);
		if (!gtk_widget_get_realized (widget))
			gtk_widget_realize (widget);
		if (gtk_widget_get_realized (widget) && !gtk_widget_get_mapped (widget))
			gtk_widget_map (widget);
		surface = gtk_native_get_surface (GTK_NATIVE (window));
		if (surface)
			gdk_window_lower (surface);
		return;
	}
	verne_window_present_keep (window);
}

static gboolean
verne_hide_dummy_idle (gpointer data)
{
	if (GTK_IS_WINDOW (data))
		verne_x11_hide_dummy_natives (GTK_WINDOW (data));
	return G_SOURCE_REMOVE;
}

static void
verne_dest_keep_below (GtkWindow *window, GParamSpec *pspec, gpointer data)
{
	GdkSurface *surface;

	(void) pspec;
	(void) data;
	if (!GTK_IS_WINDOW (window))
		return;
	if (gtk_window_get_type_hint (window) != GDK_WINDOW_TYPE_HINT_DESKTOP &&
	    g_object_get_data (G_OBJECT (window), "is_desktop_window") == NULL)
		return;
	surface = gtk_native_get_surface (GTK_NATIVE (window));
	if (surface)
		gdk_window_lower (surface);
}

static void
verne_dest_keep_below_map (GtkWidget *widget, gpointer data)
{
	(void) data;
	if (GTK_IS_WINDOW (widget))
		verne_dest_keep_below (GTK_WINDOW (widget), NULL, NULL);
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
		if (hint == GDK_WINDOW_TYPE_HINT_DESKTOP &&
		    g_object_get_data (G_OBJECT (window), "verne-dest-keep-below") == NULL) {
			g_object_set_data (G_OBJECT (window), "verne-dest-keep-below", GINT_TO_POINTER (1));
			g_signal_connect (window, "notify::is-active",
					  G_CALLBACK (verne_dest_keep_below), NULL);
			g_signal_connect (window, "map",
					  G_CALLBACK (verne_dest_keep_below_map), NULL);
		}
	} else if (hint == GDK_WINDOW_TYPE_HINT_POPUP_MENU ||
		   hint == GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU ||
		   hint == GDK_WINDOW_TYPE_HINT_MENU) {
		Atom type = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", False);
		Atom value = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);

		XChangeProperty (dpy, xid, type, XA_ATOM, 32, PropModeReplace,
				 (unsigned char *) &value, 1);
		XRaiseWindow (dpy, xid);
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
	verne_x11_hide_dummy_natives (window);
	if (g_object_get_data (G_OBJECT (window), "verne-hide-dummy-idle") == NULL) {
		g_object_set_data (G_OBJECT (window), "verne-hide-dummy-idle", GINT_TO_POINTER (1));
		g_idle_add_full (G_PRIORITY_LOW, verne_hide_dummy_idle,
				 g_object_ref (window), g_object_unref);
	}
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
		} else if (GTK_IS_STACK (container)) {
			GtkStackPage *page = gtk_stack_get_page (GTK_STACK (container), child);
			if (g_strcmp0 (name, "name") == 0) {
				const gchar *value = va_arg (args, const gchar *);
				if (page && value)
					gtk_stack_page_set_name (page, value);
			} else if (g_strcmp0 (name, "title") == 0) {
				const gchar *value = va_arg (args, const gchar *);
				if (page && value)
					gtk_stack_page_set_title (page, value);
			} else if (g_strcmp0 (name, "icon-name") == 0 ||
				   g_strcmp0 (name, "icon_name") == 0) {
				const gchar *value = va_arg (args, const gchar *);
				if (page && value)
					gtk_stack_page_set_icon_name (page, verne_map_icon_name (value));
			} else {
				(void) va_arg (args, gpointer);
			}
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
	if (width)
		*width = (window && GDK_IS_SURFACE (window)) ? gdk_surface_get_width (window) : 0;
	if (height)
		*height = (window && GDK_IS_SURFACE (window)) ? gdk_surface_get_height (window) : 0;
}

gint
gdk_window_get_origin (GdkSurface *window, gint *x, gint *y)
{
	int ox = 0, oy = 0;

	if (window != NULL && GDK_IS_SURFACE (window)) {
#ifdef GDK_WINDOWING_X11
		if (GDK_IS_X11_SURFACE (window)) {
			Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (window));
			Window child = None;

			XTranslateCoordinates (dpy, gdk_x11_surface_get_xid (window),
					       DefaultRootWindow (dpy),
					       0, 0, &ox, &oy, &child);
		}
#endif
	}
	if (x) *x = ox;
	if (y) *y = oy;
	return 1;
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
	if (window && GDK_IS_X11_SURFACE (window)) {
		GListModel *model = gtk_window_get_toplevels ();
		guint i, n = g_list_model_get_n_items (model);
		gboolean is_dest = FALSE;

		for (i = 0; i < n; i++) {
			gpointer w = g_list_model_get_item (model, i);
			GdkSurface *s;

			if (w && GTK_IS_WINDOW (w) &&
			    (gtk_window_get_type_hint (GTK_WINDOW (w)) == GDK_WINDOW_TYPE_HINT_DESKTOP ||
			     g_object_get_data (G_OBJECT (w), "is_desktop_window") != NULL)) {
				s = gtk_native_get_surface (GTK_NATIVE (w));
				if (s == window)
					is_dest = TRUE;
			}
			if (w)
				g_object_unref (w);
		}
		if (is_dest)
			XLowerWindow (gdk_x11_display_get_xdisplay (gdk_surface_get_display (window)),
				      gdk_x11_surface_get_xid (window));
		else
			XRaiseWindow (gdk_x11_display_get_xdisplay (gdk_surface_get_display (window)),
				      gdk_x11_surface_get_xid (window));
	}
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
	GdkDisplay *display;
	GdkMonitor *m;

	(void) screen;
	display = gdk_display_get_default ();
	if (display == NULL)
		return 1;
	m = gdk_display_get_monitor (display, monitor);
	if (m == NULL)
		m = gdk_display_get_monitor (display, 0);
	return m ? gdk_monitor_get_scale_factor (m) : 1;
}

gchar *
gdk_screen_get_monitor_plug_name (GdkScreen *screen, int monitor)
{
	(void) screen; (void) monitor;
	return g_strdup ("default");
}

struct _VerneKeymap {
	GObject parent_instance;
};

typedef struct _VerneKeymap VerneKeymap;
typedef struct _VerneKeymapClass {
	GObjectClass parent_class;
} VerneKeymapClass;

enum {
	VERNE_KEYMAP_DIRECTION_CHANGED,
	VERNE_KEYMAP_LAST_SIGNAL
};

static guint verne_keymap_signals[VERNE_KEYMAP_LAST_SIGNAL];

G_DEFINE_TYPE (VerneKeymap, verne_keymap, G_TYPE_OBJECT)

GType
gdk_keymap_get_type (void)
{
	return verne_keymap_get_type ();
}

static void
verne_keymap_class_init (VerneKeymapClass *klass)
{
	verne_keymap_signals[VERNE_KEYMAP_DIRECTION_CHANGED] =
		g_signal_new ("direction-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      0, NULL, NULL, NULL,
			      G_TYPE_NONE, 0);
}

static void
verne_keymap_init (VerneKeymap *keymap)
{
	(void) keymap;
}

static GdkKeymap *
verne_keymap_singleton (void)
{
	static GdkKeymap *keymap;

	if (keymap == NULL)
		keymap = g_object_new (GDK_TYPE_KEYMAP, NULL);
	return keymap;
}

GdkKeymap *
gdk_keymap_get_default (void)
{
	return verne_keymap_singleton ();
}

GdkKeymap *
gdk_keymap_get_for_display (GdkDisplay *display)
{
	(void) display;
	return verne_keymap_singleton ();
}

PangoDirection
gdk_keymap_get_direction (GdkKeymap *keymap)
{
	(void) keymap;
	return gtk_get_locale_direction () == GTK_TEXT_DIR_RTL
		? PANGO_DIRECTION_RTL
		: PANGO_DIRECTION_LTR;
}

#ifdef GDK_WINDOWING_X11
static Display *
verne_xdisplay (void)
{
	GdkDisplay *display = gdk_display_get_default ();

	if (display == NULL || !GDK_IS_X11_DISPLAY (display))
		return NULL;
	return gdk_x11_display_get_xdisplay (display);
}

static Atom
verne_gdk_atom_to_xatom (Display *dpy, GdkAtom atom)
{
	const gchar *name;

	if (dpy == NULL || atom == NULL)
		return None;
	name = (const gchar *) atom;
	if (g_strcmp0 (name, "PRIMARY") == 0)
		return XA_PRIMARY;
	if (g_strcmp0 (name, "SECONDARY") == 0)
		return XA_SECONDARY;
	if (g_strcmp0 (name, "STRING") == 0)
		return XA_STRING;
	if (g_strcmp0 (name, "ATOM") == 0)
		return XA_ATOM;
	if (g_strcmp0 (name, "INTEGER") == 0)
		return XA_INTEGER;
	if (g_strcmp0 (name, "WINDOW") == 0)
		return XA_WINDOW;
	if (g_strcmp0 (name, "CARDINAL") == 0)
		return XA_CARDINAL;
	if (g_strcmp0 (name, "UTF8_STRING") == 0)
		return XInternAtom (dpy, "UTF8_STRING", False);
	return XInternAtom (dpy, name, False);
}

static gboolean
verne_net_workarea (GdkRectangle *rect)
{
	Display *dpy;
	Window root;
	Atom workarea, actual_type = None;
	int actual_format = 0;
	unsigned long nitems = 0, bytes_after = 0;
	unsigned char *prop = NULL;
	unsigned long *values;
	gboolean ok = FALSE;

	dpy = verne_xdisplay ();
	if (dpy == NULL || rect == NULL)
		return FALSE;
	root = DefaultRootWindow (dpy);
	workarea = XInternAtom (dpy, "_NET_WORKAREA", True);
	if (workarea == None)
		return FALSE;
	if (XGetWindowProperty (dpy, root, workarea, 0, 4, False, XA_CARDINAL,
				&actual_type, &actual_format, &nitems, &bytes_after,
				&prop) != Success || prop == NULL)
		return FALSE;
	if (actual_type == XA_CARDINAL && actual_format == 32 && nitems >= 4) {
		values = (unsigned long *) prop;
		rect->x = (gint) values[0];
		rect->y = (gint) values[1];
		rect->width = (gint) values[2];
		rect->height = (gint) values[3];
		ok = rect->width > 0 && rect->height > 0;
	}
	XFree (prop);
	return ok;
}

static void
verne_inset_workarea_for_dock (GdkRectangle *work,
			       const GdkRectangle *geo,
			       const GdkRectangle *dock)
{
	GdkRectangle inter;

	if (work == NULL || geo == NULL || dock == NULL)
		return;
	if (dock->width < 8 || dock->height < 8)
		return;
	if (!gdk_rectangle_intersect (work, dock, &inter))
		return;

	if (dock->y <= geo->y + 8 && dock->width >= geo->width / 2) {
		gint bottom = work->y + work->height;
		gint top = MAX (work->y, dock->y + dock->height);

		if (bottom > top) {
			work->y = top;
			work->height = bottom - top;
		}
	} else if (dock->y + dock->height >= geo->y + geo->height - 8 &&
		   dock->width >= geo->width / 2) {
		gint new_h = MIN (work->y + work->height, dock->y) - work->y;

		if (new_h > 0)
			work->height = new_h;
	} else if (dock->x <= geo->x + 8 && dock->height >= geo->height / 2) {
		gint right = work->x + work->width;
		gint left = MAX (work->x, dock->x + dock->width);

		if (right > left) {
			work->x = left;
			work->width = right - left;
		}
	} else if (dock->x + dock->width >= geo->x + geo->width - 8 &&
		   dock->height >= geo->height / 2) {
		gint new_w = MIN (work->x + work->width, dock->x) - work->x;

		if (new_w > 0)
			work->width = new_w;
	}
}

static void
verne_subtract_docks (GdkRectangle *work, const GdkRectangle *geo)
{
	Display *dpy;
	Window root, parent = None, *children = NULL;
	unsigned int n = 0, i;
	Atom client_list, type_atom, dock_atom, actual = None;
	int fmt = 0;
	unsigned long nitems = 0, after = 0;
	unsigned char *prop = NULL;
	Window *clients = NULL;
	unsigned long nclients = 0;

	dpy = verne_xdisplay ();
	if (dpy == NULL || work == NULL || geo == NULL)
		return;
	root = DefaultRootWindow (dpy);
	client_list = XInternAtom (dpy, "_NET_CLIENT_LIST", True);
	type_atom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", True);
	dock_atom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DOCK", True);
	if (client_list == None || type_atom == None || dock_atom == None)
		return;
	if (XGetWindowProperty (dpy, root, client_list, 0, 256, False, XA_WINDOW,
				&actual, &fmt, &nitems, &after, &prop) == Success &&
	    prop != NULL && fmt == 32 && nitems > 0) {
		clients = (Window *) prop;
		nclients = nitems;
		prop = NULL;
	} else {
		if (prop)
			XFree (prop);
		if (!XQueryTree (dpy, root, &root, &parent, &children, &n) || children == NULL)
			return;
		clients = children;
		nclients = n;
		children = NULL;
	}

	gdk_x11_display_error_trap_push (gdk_display_get_default ());
	for (i = 0; i < nclients; i++) {
		Window w = clients[i];
		Atom wtype = None;
		unsigned char *tdata = NULL;
		Window child = None, dummy = None;
		int rx = 0, ry = 0;
		unsigned int rw = 0, rh = 0, bw = 0, depth = 0;
		GdkRectangle dock;

		if (XGetWindowProperty (dpy, w, type_atom, 0, 1, False, XA_ATOM,
					&actual, &fmt, &nitems, &after, &tdata) != Success ||
		    tdata == NULL || nitems < 1) {
			if (tdata)
				XFree (tdata);
			continue;
		}
		wtype = *(Atom *) tdata;
		XFree (tdata);
		if (wtype != dock_atom)
			continue;
		if (!XGetGeometry (dpy, w, &dummy, &rx, &ry, &rw, &rh, &bw, &depth))
			continue;
		if (!XTranslateCoordinates (dpy, w, DefaultRootWindow (dpy), 0, 0, &rx, &ry, &child))
			continue;
		dock.x = rx;
		dock.y = ry;
		dock.width = (gint) rw;
		dock.height = (gint) rh;
		verne_inset_workarea_for_dock (work, geo, &dock);
	}
	gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
	if (clients && clients != children)
		XFree (clients);
	if (children)
		XFree (children);
}
#endif

unsigned long
verne_x11_get_xid (gpointer window)
{
#ifdef GDK_WINDOWING_X11
	gpointer stored;

	if (window == NULL)
		return 0;
	stored = g_object_get_data (G_OBJECT (window), "verne-xid");
	if (stored)
		return (unsigned long) GPOINTER_TO_UINT (stored);
	if (GDK_IS_X11_SURFACE (window))
		return (unsigned long) gdk_x11_surface_get_xid (GDK_X11_SURFACE (window));
#else
	(void) window;
#endif
	return 0;
}

void
gdk_monitor_get_workarea (GdkMonitor *monitor, GdkRectangle *workarea)
{
	GdkRectangle geo = { 0, 0, 0, 0 };

	if (workarea == NULL)
		return;
	if (monitor)
		gdk_monitor_get_geometry (monitor, &geo);
	*workarea = geo;
#ifdef GDK_WINDOWING_X11
	{
		GdkRectangle net;

		if (verne_net_workarea (&net)) {
			gint x1 = MAX (net.x, geo.x);
			gint y1 = MAX (net.y, geo.y);
			gint x2 = MIN (net.x + net.width, geo.x + geo.width);
			gint y2 = MIN (net.y + net.height, geo.y + geo.height);

			if (x2 > x1 && y2 > y1) {
				workarea->x = x1;
				workarea->y = y1;
				workarea->width = x2 - x1;
				workarea->height = y2 - y1;
			}
		}
		verne_subtract_docks (workarea, &geo);
	}
#endif
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

typedef struct {
	GtkTreePath *path;
	GtkTreeViewDropPosition pos;
} VerneTreeDestRow;

static GHashTable *verne_tree_snapshot_origs;
static void (*verne_tree_view_snapshot_default) (GtkWidget *, GtkSnapshot *);

static GQuark
verne_tree_dest_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-tree-dest-row");
	return q;
}

static void
verne_tree_dest_free (gpointer data)
{
	VerneTreeDestRow *row = data;

	if (row == NULL)
		return;
	if (row->path)
		gtk_tree_path_free (row->path);
	g_free (row);
}

static int verne_tree_dest_paint_logs = 0;

static void
verne_paint_tree_dest_row (GtkTreeView *tree_view, GtkSnapshot *snapshot)
{
	VerneTreeDestRow *row;
	GdkRectangle rect = { 0 };
	GtkStyleContext *style;
	GdkRGBA accent = { 0.208, 0.518, 0.894, 1.0 };
	int ww, wh, wx, wy;
	double x, y, w, h;

	row = g_object_get_qdata (G_OBJECT (tree_view), verne_tree_dest_quark ());
	if (row == NULL || row->path == NULL)
		return;

	gtk_tree_view_get_background_area (tree_view, row->path, NULL, &rect);
	if (rect.width < 2 || rect.height < 2) {
		GtkTreeViewColumn *col = gtk_tree_view_get_column (tree_view, 0);

		if (col)
			gtk_tree_view_get_cell_area (tree_view, row->path, col, &rect);
	}
	gtk_tree_view_convert_bin_window_to_widget_coords (tree_view, rect.x, rect.y, &wx, &wy);
	if (wx >= -40 && wy >= -40) {
		rect.x = wx;
		rect.y = wy;
	}
	ww = gtk_widget_get_width (GTK_WIDGET (tree_view));
	wh = gtk_widget_get_height (GTK_WIDGET (tree_view));
	if (rect.width < 2)
		rect.width = MAX (ww - MAX (rect.x, 0) - 2, 4);
	if (verne_tree_dest_paint_logs < 8) {
		char *ps = gtk_tree_path_to_string (row->path);

		g_warning ("paint dest row path=%s pos=%d rect=%d,%d %dx%d widget=%dx%d",
			   ps ? ps : "?", (int) row->pos, rect.x, rect.y, rect.width, rect.height, ww, wh);
		g_free (ps);
		verne_tree_dest_paint_logs++;
	}
	if (rect.height < 2 || ww < 1 || wh < 1)
		return;

	style = gtk_widget_get_style_context (GTK_WIDGET (tree_view));
	if (!gtk_style_context_lookup_color (style, "accent_bg_color", &accent))
		gtk_style_context_lookup_color (style, "theme_selected_bg_color", &accent);

	if (row->pos == GTK_TREE_VIEW_DROP_BEFORE ||
	    row->pos == GTK_TREE_VIEW_DROP_AFTER) {
		y = (row->pos == GTK_TREE_VIEW_DROP_BEFORE) ? rect.y : rect.y + rect.height;
		gtk_snapshot_append_color (snapshot, &accent,
					   &GRAPHENE_RECT_INIT (4, (float) y - 1.5f,
								(float) MAX (ww - 8, 4), 3.0f));
	} else {
		GdkRGBA fill = accent;
		GdkRGBA edge = accent;

		x = 2;
		y = rect.y + 1;
		w = MAX (ww - 4, 4);
		h = MAX (rect.height - 2, 4);
		fill.alpha = 0.50;
		edge.alpha = 0.95;
		gtk_snapshot_append_color (snapshot, &fill,
					   &GRAPHENE_RECT_INIT ((float) x, (float) y, (float) w, (float) h));
		gtk_snapshot_append_color (snapshot, &edge,
					   &GRAPHENE_RECT_INIT ((float) x, (float) y, (float) w, 3.0f));
		gtk_snapshot_append_color (snapshot, &edge,
					   &GRAPHENE_RECT_INIT ((float) x, (float) (y + h - 3), (float) w, 3.0f));
		gtk_snapshot_append_color (snapshot, &edge,
					   &GRAPHENE_RECT_INIT ((float) x, (float) y, 3.0f, (float) h));
		gtk_snapshot_append_color (snapshot, &edge,
					   &GRAPHENE_RECT_INIT ((float) (x + w - 3), (float) y, 3.0f, (float) h));
	}
}

static void
verne_tree_view_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	void (*orig) (GtkWidget *, GtkSnapshot *) = NULL;
	GType type;

	if (verne_tree_snapshot_origs) {
		for (type = G_OBJECT_TYPE (widget); type != 0 && type != G_TYPE_OBJECT; type = g_type_parent (type)) {
			orig = g_hash_table_lookup (verne_tree_snapshot_origs, GSIZE_TO_POINTER (type));
			if (orig)
				break;
		}
	}
	if (orig == NULL)
		orig = verne_tree_view_snapshot_default;
	if (orig && orig != verne_tree_view_snapshot)
		orig (widget, snapshot);
	if (GTK_IS_TREE_VIEW (widget))
		verne_paint_tree_dest_row (GTK_TREE_VIEW (widget), snapshot);
}

static void
verne_tree_view_hook_snapshot (GtkTreeView *tree_view)
{
	GtkWidgetClass *klass;
	GType type;

	if (!GTK_IS_TREE_VIEW (tree_view))
		return;
	klass = GTK_WIDGET_GET_CLASS (tree_view);
	if (klass->snapshot == verne_tree_view_snapshot)
		return;
	type = G_OBJECT_TYPE (tree_view);
	if (verne_tree_snapshot_origs == NULL)
		verne_tree_snapshot_origs = g_hash_table_new (g_direct_hash, g_direct_equal);
	if (g_hash_table_lookup (verne_tree_snapshot_origs, GSIZE_TO_POINTER (type)) == NULL &&
	    klass->snapshot != NULL)
		g_hash_table_insert (verne_tree_snapshot_origs, GSIZE_TO_POINTER (type), klass->snapshot);
	if (verne_tree_view_snapshot_default == NULL && klass->snapshot != verne_tree_view_snapshot)
		verne_tree_view_snapshot_default = klass->snapshot;
	klass->snapshot = verne_tree_view_snapshot;
}

void
verne_tree_view_paint_dest_overlay (GtkWidget *widget, GtkSnapshot *snapshot)
{
	if (GTK_IS_TREE_VIEW (widget) && snapshot)
		verne_paint_tree_dest_row (GTK_TREE_VIEW (widget), snapshot);
}

static gboolean
verne_tree_dest_tick (GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
	VerneTreeDestRow *row;

	(void) clock;
	(void) data;
	row = g_object_get_qdata (G_OBJECT (widget), verne_tree_dest_quark ());
	if (row && row->path)
		gtk_widget_queue_draw (widget);
	return G_SOURCE_CONTINUE;
}

static void
verne_tree_dest_remove_tick (GtkWidget *widget)
{
	guint id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), "verne-dest-tick"));

	if (id) {
		gtk_widget_remove_tick_callback (widget, id);
		g_object_set_data (G_OBJECT (widget), "verne-dest-tick", NULL);
	}
}

static void
verne_tree_dest_ensure_tick (GtkWidget *widget)
{
	guint id;

	if (g_object_get_data (G_OBJECT (widget), "verne-dest-tick"))
		return;
	id = gtk_widget_add_tick_callback (widget, verne_tree_dest_tick, NULL, NULL);
	g_object_set_data (G_OBJECT (widget), "verne-dest-tick", GUINT_TO_POINTER (id));
}

void
verne_gtk_tree_view_set_drag_dest_row (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewDropPosition pos)
{
	VerneTreeDestRow *row;

	if (!GTK_IS_TREE_VIEW (tree_view))
		return;
	verne_tree_view_hook_snapshot (tree_view);
	if (path == NULL) {
		if (g_object_get_qdata (G_OBJECT (tree_view), verne_tree_dest_quark ()) != NULL)
			g_warning ("tree dest row cleared widget=%s", G_OBJECT_TYPE_NAME (tree_view));
		g_object_set_qdata (G_OBJECT (tree_view), verne_tree_dest_quark (), NULL);
		verne_tree_dest_remove_tick (GTK_WIDGET (tree_view));
		gtk_widget_queue_draw (GTK_WIDGET (tree_view));
		return;
	}
	{
		VerneTreeDestRow *prev;
		char *ps = gtk_tree_path_to_string (path);

		prev = g_object_get_qdata (G_OBJECT (tree_view), verne_tree_dest_quark ());
		if (prev == NULL || prev->pos != pos || prev->path == NULL ||
		    gtk_tree_path_compare (prev->path, path) != 0)
			g_warning ("tree dest row widget=%s path=%s pos=%d",
				   G_OBJECT_TYPE_NAME (tree_view), ps ? ps : "?", (int) pos);
		g_free (ps);
	}
	row = g_new0 (VerneTreeDestRow, 1);
	row->path = gtk_tree_path_copy (path);
	row->pos = pos;
	g_object_set_qdata_full (G_OBJECT (tree_view), verne_tree_dest_quark (), row, verne_tree_dest_free);
	verne_tree_dest_ensure_tick (GTK_WIDGET (tree_view));
	gtk_widget_queue_draw (GTK_WIDGET (tree_view));
	if (gtk_widget_get_parent (GTK_WIDGET (tree_view)))
		gtk_widget_queue_draw (gtk_widget_get_parent (GTK_WIDGET (tree_view)));
}

void
verne_gtk_tree_view_get_drag_dest_row (GtkTreeView *tree_view, GtkTreePath **path, GtkTreeViewDropPosition *pos)
{
	VerneTreeDestRow *row;

	if (path)
		*path = NULL;
	if (pos)
		*pos = 0;
	if (!GTK_IS_TREE_VIEW (tree_view))
		return;
	row = g_object_get_qdata (G_OBJECT (tree_view), verne_tree_dest_quark ());
	if (row == NULL)
		return;
	if (path && row->path)
		*path = gtk_tree_path_copy (row->path);
	if (pos)
		*pos = row->pos;
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

gboolean
gtk_targets_include_text (GdkAtom *targets, gint n_targets)
{
	gint i;

	for (i = 0; i < n_targets; i++) {
		const char *name = (const char *) targets[i];
		if (name == NULL)
			continue;
		if (g_strcmp0 (name, "UTF8_STRING") == 0 ||
		    g_strcmp0 (name, "TEXT") == 0 ||
		    g_strcmp0 (name, "STRING") == 0 ||
		    g_strcmp0 (name, "COMPOUND_TEXT") == 0 ||
		    g_strcmp0 (name, "text/plain") == 0 ||
		    g_str_has_prefix (name, "text/plain;"))
			return TRUE;
	}
	return FALSE;
}

gboolean
gtk_targets_include_uri (GdkAtom *targets, gint n_targets)
{
	gint i;

	for (i = 0; i < n_targets; i++) {
		const char *name = (const char *) targets[i];
		if (name == NULL)
			continue;
		if (g_strcmp0 (name, "text/uri-list") == 0 ||
		    g_strcmp0 (name, "application/vnd.portal.files") == 0)
			return TRUE;
	}
	return FALSE;
}

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
	if (GTK_IS_SWITCH (activatable) && GTK_IS_TOGGLE_ACTION (action)) {
		g_object_set_data (G_OBJECT (activatable), "verne-syncing-toggle", GINT_TO_POINTER (1));
		gtk_switch_set_active (GTK_SWITCH (activatable),
				       gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
		g_object_set_data (G_OBJECT (activatable), "verne-syncing-toggle", NULL);
	}
}

static void
on_related_toggle_toggled (GtkToggleButton *button, gpointer action)
{
	if (g_object_get_data (G_OBJECT (button), "verne-syncing-toggle"))
		return;
	if (GTK_IS_TOGGLE_ACTION (action))
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      gtk_toggle_button_get_active (button));
	else if (gtk_toggle_button_get_active (button))
		gtk_action_activate (GTK_ACTION (action));
}

static void
on_related_switch_notify (GObject *sw, GParamSpec *pspec, gpointer action)
{
	(void) pspec;
	if (g_object_get_data (sw, "verne-syncing-toggle"))
		return;
	if (GTK_IS_SWITCH (sw) && GTK_IS_TOGGLE_ACTION (action))
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      gtk_switch_get_active (GTK_SWITCH (sw)));
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
	if (GTK_IS_SWITCH (activatable) && GTK_IS_TOGGLE_ACTION (action)) {
		gtk_switch_set_active (GTK_SWITCH (activatable),
				       gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
		if (g_object_get_data (G_OBJECT (activatable), "verne-switch-notify") == NULL) {
			g_signal_connect (activatable, "notify::active",
					  G_CALLBACK (on_related_switch_notify), action);
			g_object_set_data (G_OBJECT (activatable), "verne-switch-notify",
					   GINT_TO_POINTER (1));
		}
		if (g_object_get_data (G_OBJECT (activatable), "verne-action-active") == NULL) {
			g_signal_connect_object (action, "notify::active",
						 G_CALLBACK (on_related_action_active),
						 activatable, 0);
			g_object_set_data (G_OBJECT (activatable), "verne-action-active",
					   GINT_TO_POINTER (1));
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

static void
verne_style_fg (GtkStyleContext *context, GdkRGBA *color)
{
	GtkStateFlags state = GTK_STATE_FLAG_NORMAL;

	if (color == NULL)
		return;
	color->red = color->green = color->blue = 1.0;
	color->alpha = 1.0;
	if (context == NULL)
		return;
	state = gtk_style_context_get_state (context);
	(gtk_style_context_get_color) (context, color);
	if (color->alpha >= 0.05)
		return;
	if (state & GTK_STATE_FLAG_SELECTED) {
		if (gtk_style_context_lookup_color (context, "theme_selected_fg_color", color) &&
		    color->alpha >= 0.05)
			return;
	}
	if (gtk_style_context_lookup_color (context, "theme_fg_color", color) &&
	    color->alpha >= 0.05)
		return;
	if (gtk_style_context_lookup_color (context, "theme_text_color", color) &&
	    color->alpha >= 0.05)
		return;
	color->red = color->green = color->blue = 1.0;
	color->alpha = 1.0;
}

void
verne_gtk_render_layout (GtkStyleContext *context, cairo_t *cr,
			 double x, double y, PangoLayout *layout)
{
	GdkRGBA color;

	if (cr == NULL || layout == NULL)
		return;
	verne_style_fg (context, &color);
	cairo_save (cr);
	gdk_cairo_set_source_rgba (cr, &color);
	cairo_move_to (cr, x, y);
	pango_cairo_show_layout (cr, layout);
	cairo_restore (cr);
}

void
verne_gtk_render_background (GtkStyleContext *context, cairo_t *cr,
			     double x, double y, double width, double height)
{
	GtkStateFlags state;
	GdkRGBA bg;

	if (cr == NULL || width <= 0 || height <= 0)
		return;
	state = context ? gtk_style_context_get_state (context) : GTK_STATE_FLAG_NORMAL;
	bg.red = bg.green = bg.blue = 0;
	bg.alpha = 0;
	if (state & GTK_STATE_FLAG_SELECTED) {
		if (context == NULL ||
		    !gtk_style_context_lookup_color (context, "theme_selected_bg_color", &bg) ||
		    bg.alpha < 0.05) {
			bg.red = 0.20;
			bg.green = 0.45;
			bg.blue = 0.85;
			bg.alpha = 0.85;
		}
	} else if (state & GTK_STATE_FLAG_PRELIGHT) {
		bg.red = bg.green = bg.blue = 1.0;
		bg.alpha = 0.12;
	} else if (context) {
		(void) gtk_style_context_lookup_color (context, "theme_bg_color", &bg);
	}
	if (bg.alpha < 0.02)
		return;
	cairo_save (cr);
	gdk_cairo_set_source_rgba (cr, &bg);
	cairo_rectangle (cr, x, y, width, height);
	cairo_fill (cr);
	cairo_restore (cr);
}

void
verne_gtk_render_frame (GtkStyleContext *context, cairo_t *cr,
			double x, double y, double width, double height)
{
	GdkRGBA color;

	if (cr == NULL || width <= 0 || height <= 0)
		return;
	verne_style_fg (context, &color);
	color.alpha = MIN (color.alpha, 0.6);
	cairo_save (cr);
	gdk_cairo_set_source_rgba (cr, &color);
	cairo_set_line_width (cr, 1.0);
	cairo_rectangle (cr, x + 0.5, y + 0.5, width - 1.0, height - 1.0);
	cairo_stroke (cr);
	cairo_restore (cr);
}

void
verne_gtk_render_focus (GtkStyleContext *context, cairo_t *cr,
			double x, double y, double width, double height)
{
	GdkRGBA color;
	const double dashes[] = { 1.0, 2.0 };

	if (cr == NULL || width <= 0 || height <= 0)
		return;
	verne_style_fg (context, &color);
	cairo_save (cr);
	gdk_cairo_set_source_rgba (cr, &color);
	cairo_set_line_width (cr, 1.0);
	cairo_set_dash (cr, dashes, 2, 0);
	cairo_rectangle (cr, x + 0.5, y + 0.5, width - 1.0, height - 1.0);
	cairo_stroke (cr);
	cairo_restore (cr);
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
GdkWindowTypeHint
gdk_window_get_type_hint (gpointer window)
{
#ifdef GDK_WINDOWING_X11
	Display *dpy;
	Window xid;
	Atom actual_type = None, prop, desktop, dialog, menu, dock, utility, splash, tooltip, combo, dnd, dropdown, popup, notification, toolbar, normal;
	int actual_format = 0;
	unsigned long nitems = 0, bytes_after = 0;
	unsigned char *data = NULL;
	Atom value = None;

	xid = (Window) verne_x11_get_xid (window);
	dpy = verne_xdisplay ();
	if (dpy == NULL || xid == 0)
		return GDK_WINDOW_TYPE_HINT_NORMAL;
	prop = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", True);
	if (prop == None)
		return GDK_WINDOW_TYPE_HINT_NORMAL;
	if (XGetWindowProperty (dpy, xid, prop, 0, 1, False, XA_ATOM,
				&actual_type, &actual_format, &nitems, &bytes_after,
				&data) != Success || data == NULL || nitems < 1) {
		if (data)
			XFree (data);
		return GDK_WINDOW_TYPE_HINT_NORMAL;
	}
	value = *(Atom *) data;
	XFree (data);
	desktop = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", True);
	dialog = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DIALOG", True);
	menu = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_MENU", True);
	dock = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DOCK", True);
	utility = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_UTILITY", True);
	splash = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_SPLASH", True);
	tooltip = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_TOOLTIP", True);
	combo = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_COMBO", True);
	dnd = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DND", True);
	dropdown = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", True);
	popup = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_POPUP_MENU", True);
	notification = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", True);
	toolbar = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", True);
	normal = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_NORMAL", True);
	if (value == desktop)
		return GDK_WINDOW_TYPE_HINT_DESKTOP;
	if (value == dialog)
		return GDK_WINDOW_TYPE_HINT_DIALOG;
	if (value == menu)
		return GDK_WINDOW_TYPE_HINT_MENU;
	if (value == dock)
		return GDK_WINDOW_TYPE_HINT_DOCK;
	if (value == utility)
		return GDK_WINDOW_TYPE_HINT_UTILITY;
	if (value == splash)
		return GDK_WINDOW_TYPE_HINT_SPLASHSCREEN;
	if (value == tooltip)
		return GDK_WINDOW_TYPE_HINT_TOOLTIP;
	if (value == combo)
		return GDK_WINDOW_TYPE_HINT_COMBO;
	if (value == dnd)
		return GDK_WINDOW_TYPE_HINT_DND;
	if (value == dropdown)
		return GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU;
	if (value == popup)
		return GDK_WINDOW_TYPE_HINT_POPUP_MENU;
	if (value == notification)
		return GDK_WINDOW_TYPE_HINT_NOTIFICATION;
	if (value == toolbar)
		return GDK_WINDOW_TYPE_HINT_TOOLBAR;
	if (value == normal)
		return GDK_WINDOW_TYPE_HINT_NORMAL;
#else
	(void) window;
#endif
	return GDK_WINDOW_TYPE_HINT_NORMAL;
}
cairo_surface_t *gdk_window_create_similar_surface (GdkSurface *window, cairo_content_t content, int w, int h) {
	(void) window;
	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;
	if (w > 8192)
		w = 8192;
	if (h > 8192)
		h = 8192;
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
gboolean
gdk_property_get (GdkSurface *window, GdkAtom property, GdkAtom type, gulong offset, gulong length,
		  gint pdelete, GdkAtom *actual_type, gint *actual_format, gint *actual_length, guchar **data)
{
#ifdef GDK_WINDOWING_X11
	Display *dpy;
	Window xid = None;
	Atom xprop, xtype, ret_type = None;
	int ret_format = 0;
	unsigned long nitems = 0, bytes_after = 0, xlength;
	unsigned char *ret = NULL;
	int res;

	if (actual_type)
		*actual_type = NULL;
	if (actual_format)
		*actual_format = 0;
	if (actual_length)
		*actual_length = 0;
	if (data)
		*data = NULL;

	dpy = verne_xdisplay ();
	if (dpy == NULL || property == NULL)
		return FALSE;
	xid = (Window) verne_x11_get_xid (window);
	/* XDirectSave lives on the drag source, which GTK4's GdkDrop surface
	 * is not. The XdndSelection owner is that source window. */
	if (g_strcmp0 ((const gchar *) property, "XdndDirectSave0") == 0) {
		Window owner = XGetSelectionOwner (dpy, XInternAtom (dpy, "XdndSelection", False));

		if (owner != None)
			xid = owner;
	}
	if (xid == 0)
		return FALSE;
	xprop = verne_gdk_atom_to_xatom (dpy, property);
	xtype = type ? verne_gdk_atom_to_xatom (dpy, type) : AnyPropertyType;
	if (xprop == None)
		return FALSE;
	xlength = (length + 3) / 4;
	if (xlength == 0)
		xlength = 1;
	res = XGetWindowProperty (dpy, xid, xprop, offset, xlength, pdelete ? True : False,
				  xtype, &ret_type, &ret_format, &nitems, &bytes_after, &ret);
	if (res != Success || ret == NULL) {
		if (ret)
			XFree (ret);
		if (xtype != AnyPropertyType) {
			res = XGetWindowProperty (dpy, xid, xprop, offset, xlength, False,
						  AnyPropertyType, &ret_type, &ret_format,
						  &nitems, &bytes_after, &ret);
		}
	}
	if (res != Success || ret == NULL) {
		if (ret)
			XFree (ret);
		return FALSE;
	}
	if (actual_type) {
		char *tname = ret_type ? XGetAtomName (dpy, ret_type) : NULL;

		*actual_type = tname ? gdk_atom_intern (tname, FALSE) : NULL;
		if (tname)
			XFree (tname);
	}
	if (actual_format)
		*actual_format = ret_format;
	if (ret_format == 32) {
		unsigned long *src = (unsigned long *) ret;
		guint32 *dst = g_new (guint32, nitems);
		unsigned long i;

		for (i = 0; i < nitems; i++)
			dst[i] = (guint32) src[i];
		if (actual_length)
			*actual_length = (gint) (nitems * 4);
		if (data)
			*data = (guchar *) dst;
		else
			g_free (dst);
	} else if (ret_format == 16) {
		if (actual_length)
			*actual_length = (gint) (nitems * 2);
		if (data)
			*data = g_memdup2 (ret, nitems * 2);
	} else {
		if (actual_length)
			*actual_length = (gint) nitems;
		if (data)
			*data = g_memdup2 (ret, nitems);
	}
	XFree (ret);
	return TRUE;
#else
	(void) window; (void) property; (void) type; (void) offset; (void) length; (void) pdelete;
	if (actual_type) *actual_type = NULL;
	if (actual_format) *actual_format = 0;
	if (actual_length) *actual_length = 0;
	if (data) *data = NULL;
	return FALSE;
#endif
}

void
gdk_property_change (GdkSurface *window, GdkAtom property, GdkAtom type, gint format, gint mode,
		     const guchar *data, gint nelements)
{
#ifdef GDK_WINDOWING_X11
	Display *dpy;
	Window xid = None;
	Atom xprop, xtype;
	int xmode = PropModeReplace;

	dpy = verne_xdisplay ();
	if (dpy == NULL || property == NULL || data == NULL || nelements < 0)
		return;
	xid = (Window) verne_x11_get_xid (window);
	if (g_strcmp0 ((const gchar *) property, "XdndDirectSave0") == 0) {
		Window owner = XGetSelectionOwner (dpy, XInternAtom (dpy, "XdndSelection", False));

		if (owner != None)
			xid = owner;
	}
	if (xid == 0)
		return;
	xprop = verne_gdk_atom_to_xatom (dpy, property);
	xtype = type ? verne_gdk_atom_to_xatom (dpy, type) : XA_STRING;
	if (xprop == None)
		return;
	if (mode == GDK_PROP_MODE_PREPEND)
		xmode = PropModePrepend;
	else if (mode == GDK_PROP_MODE_APPEND)
		xmode = PropModeAppend;
	if (format != 8 && format != 16 && format != 32)
		format = 8;
	XChangeProperty (dpy, xid, xprop, xtype, format, xmode, data, nelements);
	XFlush (dpy);
#else
	(void) window; (void) property; (void) type; (void) format; (void) mode; (void) data; (void) nelements;
#endif
}

GdkSurface *
gdk_selection_owner_get (GdkAtom selection)
{
#ifdef GDK_WINDOWING_X11
	Display *dpy;
	Window owner;
	GdkDisplay *display;

	dpy = verne_xdisplay ();
	display = gdk_display_get_default ();
	if (dpy == NULL || display == NULL)
		return NULL;
	owner = XGetSelectionOwner (dpy, verne_gdk_atom_to_xatom (dpy, selection));
	if (owner == None)
		return NULL;
	return gdk_x11_surface_lookup_for_display (display, owner);
#else
	(void) selection;
	return NULL;
#endif
}

gboolean
gdk_display_supports_selection_notification (GdkDisplay *display)
{
#ifdef GDK_WINDOWING_X11
	return display != NULL && GDK_IS_X11_DISPLAY (display);
#else
	(void) display;
	return FALSE;
#endif
}

gboolean
gdk_screen_get_setting (GdkScreen *screen, const gchar *name, GValue *value)
{
	GtkSettings *settings;
	GParamSpec *pspec;
	GValue src = G_VALUE_INIT;

	(void) screen;
	if (name == NULL || value == NULL)
		return FALSE;
	if (g_strcmp0 (name, "gdk-window-scaling-factor") == 0) {
		GdkDisplay *display = gdk_display_get_default ();
		GdkMonitor *monitor = display ? gdk_display_get_monitor (display, 0) : NULL;
		guint scale = monitor ? (guint) gdk_monitor_get_scale_factor (monitor) : 1u;

		if (G_VALUE_TYPE (value) == G_TYPE_UINT)
			g_value_set_uint (value, scale);
		else if (G_VALUE_TYPE (value) == G_TYPE_INT)
			g_value_set_int (value, (gint) scale);
		else if (G_VALUE_TYPE (value) == 0) {
			g_value_init (value, G_TYPE_UINT);
			g_value_set_uint (value, scale);
		} else
			return FALSE;
		return TRUE;
	}
	settings = gtk_settings_get_default ();
	if (settings == NULL)
		return FALSE;
	pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), name);
	if (pspec == NULL)
		return FALSE;
	g_value_init (&src, G_PARAM_SPEC_VALUE_TYPE (pspec));
	g_object_get_property (G_OBJECT (settings), name, &src);
	if (G_VALUE_TYPE (value) == 0)
		g_value_init (value, G_VALUE_TYPE (&src));
	if (!g_value_transform (&src, value)) {
		g_value_unset (&src);
		return FALSE;
	}
	g_value_unset (&src);
	return TRUE;
}

GList *
gdk_screen_get_window_stack (GdkScreen *screen)
{
#ifdef GDK_WINDOWING_X11
	Display *dpy;
	Window root;
	Atom stacking, list_atom, actual_type = None;
	int actual_format = 0;
	unsigned long nitems = 0, bytes_after = 0, i;
	unsigned char *prop = NULL;
	unsigned long *ids;
	GList *windows = NULL;

	(void) screen;
	dpy = verne_xdisplay ();
	if (dpy == NULL)
		return NULL;
	root = DefaultRootWindow (dpy);
	stacking = XInternAtom (dpy, "_NET_CLIENT_LIST_STACKING", True);
	list_atom = stacking != None ? stacking : XInternAtom (dpy, "_NET_CLIENT_LIST", True);
	if (list_atom == None)
		return NULL;
	if (XGetWindowProperty (dpy, root, list_atom, 0, 4096, False, XA_WINDOW,
				&actual_type, &actual_format, &nitems, &bytes_after,
				&prop) != Success || prop == NULL)
		return NULL;
	ids = (unsigned long *) prop;
	for (i = nitems; i-- > 0; ) {
		GObject *token = g_object_new (G_TYPE_OBJECT, NULL);

		g_object_set_data (token, "verne-xid", GUINT_TO_POINTER ((guint) ids[i]));
		windows = g_list_prepend (windows, token);
	}
	XFree (prop);
	return windows;
#else
	(void) screen;
	return NULL;
#endif
}

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
	static const struct {
		const gchar *id;
		const gchar *label;
	} map[] = {
		{ "cancel", "_Cancel" },
		{ "gtk-cancel", "_Cancel" },
		{ "ok", "_OK" },
		{ "gtk-ok", "_OK" },
		{ "gtk-yes", "_Yes" },
		{ "gtk-no", "_No" },
		{ "document-open", "_Open" },
		{ "gtk-open", "_Open" },
		{ "document-save", "_Save" },
		{ "gtk-save", "_Save" },
		{ "document-save-as", "Save _As" },
		{ "window-close", "_Close" },
		{ "gtk-close", "_Close" },
		{ "apply", "_Apply" },
		{ "gtk-apply", "_Apply" },
		{ "help-browser", "_Help" },
		{ "gtk-help", "_Help" },
		{ "edit-delete", "_Delete" },
		{ "gtk-delete", "_Delete" },
		{ "edit-cut", "Cu_t" },
		{ "gtk-cut", "Cu_t" },
		{ "edit-copy", "_Copy" },
		{ "gtk-copy", "_Copy" },
		{ "edit-paste", "_Paste" },
		{ "gtk-paste", "_Paste" },
		{ "edit-undo", "_Undo" },
		{ "gtk-undo", "_Undo" },
		{ "edit-redo", "_Redo" },
		{ "gtk-redo", "_Redo" },
		{ "edit-find", "_Find" },
		{ "gtk-find", "_Find" },
		{ "document-new", "_New" },
		{ "gtk-new", "_New" },
		{ "list-add", "_Add" },
		{ "gtk-add", "_Add" },
		{ "list-remove", "_Remove" },
		{ "gtk-remove", "_Remove" },
		{ "application-exit", "_Quit" },
		{ "gtk-quit", "_Quit" },
		{ "document-properties", "_Properties" },
		{ "gtk-properties", "_Properties" },
		{ "preferences-system", "_Preferences" },
		{ "gtk-preferences", "_Preferences" },
		{ "help-about", "_About" },
		{ "gtk-about", "_About" },
		{ "process-stop", "_Stop" },
		{ "gtk-stop", "_Stop" },
		{ "view-refresh", "_Refresh" },
		{ "gtk-refresh", "_Refresh" },
	};
	guint i;

	if (text == NULL || text[0] == '\0' || text[0] == '_')
		return text;
	if (strchr (text, ' ') != NULL)
		return text;
	for (i = 0; i < G_N_ELEMENTS (map); i++) {
		if (g_strcmp0 (text, map[i].id) == 0)
			return map[i].label;
	}
	return text;
}

GtkWidget *
verne_dialog_add_button (GtkDialog *dialog, const gchar *text, gint response)
{
	return (gtk_dialog_add_button) (dialog, verne_dialog_button_label (text), response);
}

void
verne_dialog_add_buttons (GtkDialog *dialog, const gchar *first_text, ...)
{
	va_list args;
	const gchar *text;

	va_start (args, first_text);
	text = first_text;
	while (text != NULL) {
		gint response = va_arg (args, gint);

		verne_dialog_add_button (dialog, text, response);
		text = va_arg (args, const gchar *);
	}
	va_end (args);
}

GtkWidget *
verne_dialog_new_with_buttons (const gchar *title, GtkWindow *parent, GtkDialogFlags flags,
			       const gchar *first_button_text, ...)
{
	GtkWidget *dialog;
	va_list args;
	const gchar *text;

	dialog = gtk_dialog_new ();
	if (title != NULL)
		gtk_window_set_title (GTK_WINDOW (dialog), title);
	if (parent != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
	if (flags & GTK_DIALOG_MODAL)
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	if (parent != NULL && (flags & GTK_DIALOG_DESTROY_WITH_PARENT))
		gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	verne_prepare_dialog (dialog);

	va_start (args, first_button_text);
	text = first_button_text;
	while (text != NULL) {
		gint response = va_arg (args, gint);

		verne_dialog_add_button (GTK_DIALOG (dialog), text, response);
		text = va_arg (args, const gchar *);
	}
	va_end (args);
	return dialog;
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

