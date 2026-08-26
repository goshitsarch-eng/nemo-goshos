#include "config.h"
#include "verne-gtk-compat.h"
#include <libxapp/xapp-favorites.h>
#include <libxapp/xapp-status-icon.h>
#include <libxapp/xapp-gtk-window.h>
#include <glib/gstdio.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

struct _XAppFavorites {
	GObject parent;
	GHashTable *uris;
};

static void verne_favorites_load (XAppFavorites *self);
static void verne_favorites_save (XAppFavorites *self);

G_DEFINE_FINAL_TYPE (XAppFavorites, xapp_favorites, G_TYPE_OBJECT)

static XAppFavorites *default_favorites;

static void
xapp_favorites_finalize (GObject *object)
{
	XAppFavorites *f = XAPP_FAVORITES (object);
	g_hash_table_destroy (f->uris);
	G_OBJECT_CLASS (xapp_favorites_parent_class)->finalize (object);
}

static void
xapp_favorites_class_init (XAppFavoritesClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = xapp_favorites_finalize;
	g_signal_new ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
xapp_favorites_init (XAppFavorites *self)
{
	self->uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	verne_favorites_load (self);
}

XAppFavorites *
xapp_favorites_get_default (void)
{
	if (default_favorites == NULL)
		default_favorites = g_object_new (XAPP_TYPE_FAVORITES, NULL);
	return default_favorites;
}

void
xapp_favorites_add (XAppFavorites *favorites, const gchar *uri)
{
	g_hash_table_insert (favorites->uris, g_strdup (uri), g_strdup (uri));
	verne_favorites_save (favorites);
	g_signal_emit_by_name (favorites, "changed");
}

void
xapp_favorites_remove (XAppFavorites *favorites, const gchar *uri)
{
	g_hash_table_remove (favorites->uris, uri);
	verne_favorites_save (favorites);
	g_signal_emit_by_name (favorites, "changed");
}

void
xapp_favorites_rename (XAppFavorites *favorites, const gchar *old_uri, const gchar *new_uri)
{
	if (g_hash_table_remove (favorites->uris, old_uri))
		g_hash_table_insert (favorites->uris, g_strdup (new_uri), g_strdup (new_uri));
	verne_favorites_save (favorites);
	g_signal_emit_by_name (favorites, "changed");
}

gint
xapp_favorites_get_n_favorites (XAppFavorites *favorites)
{
	return (gint) g_hash_table_size (favorites->uris);
}

GList *
xapp_favorites_get_favorites (XAppFavorites *favorites, const gchar **mimetypes)
{
	GList *list = NULL;
	GHashTableIter iter;
	gpointer key, value;
	(void) mimetypes;
	g_hash_table_iter_init (&iter, favorites->uris);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		XAppFavoriteInfo *info = g_new0 (XAppFavoriteInfo, 1);
		info->uri = g_strdup (key);
		info->display_name = g_path_get_basename (key);
		list = g_list_prepend (list, info);
	}
	return list;
}

XAppFavoriteInfo *
xapp_favorites_find_by_uri (XAppFavorites *favorites, const gchar *uri)
{
	if (!g_hash_table_contains (favorites->uris, uri))
		return NULL;
	XAppFavoriteInfo *info = g_new0 (XAppFavoriteInfo, 1);
	info->uri = g_strdup (uri);
	info->display_name = g_path_get_basename (uri);
	return info;
}

void
xapp_favorite_info_free (XAppFavoriteInfo *info)
{
	if (!info) return;
	g_free (info->uri);
	g_free (info->display_name);
	g_free (info);
}

static gchar *
verne_favorites_path (void)
{
	return g_build_filename (g_get_user_config_dir (), "xapp", "favorites", NULL);
}

static void
verne_favorites_load (XAppFavorites *self)
{
	gchar *path = verne_favorites_path ();
	gchar *contents = NULL;
	gchar **lines;
	int i;

	if (!g_file_get_contents (path, &contents, NULL, NULL)) {
		g_free (path);
		return;
	}
	lines = g_strsplit (contents, "\n", -1);
	for (i = 0; lines[i]; i++) {
		g_strstrip (lines[i]);
		if (lines[i][0] == '\0' || lines[i][0] == '#')
			continue;
		g_hash_table_insert (self->uris, g_strdup (lines[i]), g_strdup (lines[i]));
	}
	g_strfreev (lines);
	g_free (contents);
	g_free (path);
}

static void
verne_favorites_save (XAppFavorites *self)
{
	GHashTableIter iter;
	gpointer key;
	GString *out;
	gchar *path;
	gchar *dir;

	path = verne_favorites_path ();
	dir = g_path_get_dirname (path);
	g_mkdir_with_parents (dir, 0700);
	g_free (dir);
	out = g_string_new ("");
	g_hash_table_iter_init (&iter, self->uris);
	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		g_string_append (out, (const gchar *) key);
		g_string_append_c (out, '\n');
	}
	g_file_set_contents (path, out->str, out->len, NULL);
	g_string_free (out, TRUE);
	g_free (path);
}

struct _XAppStatusIcon {
	GObject parent;
	GtkWidget *window;
	GtkWidget *image;
	gchar *icon_name;
	gchar *tooltip;
	gboolean visible;
};

static void
xapp_status_icon_clicked (GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer data)
{
	XAppStatusIcon *icon = data;
	(void) gesture; (void) n_press; (void) x; (void) y;
	g_signal_emit_by_name (icon, "activate", 1u, (guint) (g_get_monotonic_time () / 1000));
}

static void
xapp_status_icon_place (XAppStatusIcon *icon)
{
	GdkDisplay *display;
	GListModel *monitors;
	GdkMonitor *monitor;
	GdkRectangle geo;
	int x, y;

	if (icon->window == NULL)
		return;
	display = gdk_display_get_default ();
	if (display == NULL)
		return;
	monitors = gdk_display_get_monitors (display);
	if (g_list_model_get_n_items (monitors) == 0)
		return;
	monitor = g_list_model_get_item (monitors, 0);
	gdk_monitor_get_geometry (monitor, &geo);
	x = geo.x + geo.width - 64;
	y = geo.y + 48;
	g_object_unref (monitor);
	gtk_window_set_default_size (GTK_WINDOW (icon->window), 56, 56);
#ifdef GDK_WINDOWING_X11
	{
		GdkSurface *s = gtk_native_get_surface (GTK_NATIVE (icon->window));
		if (s && GDK_IS_X11_SURFACE (s)) {
			Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));
			Window xid = gdk_x11_surface_get_xid (s);
			Atom state = XInternAtom (dpy, "_NET_WM_STATE", False);
			Atom atoms[3];
			atoms[0] = XInternAtom (dpy, "_NET_WM_STATE_ABOVE", False);
			atoms[1] = XInternAtom (dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
			atoms[2] = XInternAtom (dpy, "_NET_WM_STATE_SKIP_PAGER", False);
			XChangeProperty (dpy, xid, state, XA_ATOM, 32, PropModeReplace,
					 (unsigned char *) atoms, 3);
			XMoveWindow (dpy, xid, x, y);
			XRaiseWindow (dpy, xid);
		}
	}
#else
	(void) x; (void) y;
#endif
}

#ifdef GDK_WINDOWING_X11
static void
verne_status_icon_mapped (GtkWidget *w, gpointer data)
{
	GdkSurface *s;
	(void) data;
	s = gtk_native_get_surface (GTK_NATIVE (w));
	if (s && GDK_IS_X11_SURFACE (s)) {
		gdk_x11_surface_set_skip_taskbar_hint (s, TRUE);
		gdk_x11_surface_set_skip_pager_hint (s, TRUE);
	}
}
#endif

static void
xapp_status_icon_ensure_window (XAppStatusIcon *icon)
{
	GtkGesture *click;
	GtkWidget *img;

	if (icon->window)
		return;
	icon->window = gtk_window_new ();
	gtk_window_set_decorated (GTK_WINDOW (icon->window), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (icon->window), FALSE);
	gtk_window_set_title (GTK_WINDOW (icon->window), "Verne File Operations");
	gtk_window_set_hide_on_close (GTK_WINDOW (icon->window), TRUE);
	gtk_widget_set_size_request (icon->window, 56, 56);
	{
		GtkApplication *app = GTK_APPLICATION (g_application_get_default ());
		if (app)
			gtk_application_add_window (app, GTK_WINDOW (icon->window));
	}
	gtk_widget_add_css_class (icon->window, "osd");
	gtk_widget_add_css_class (icon->window, "verne-status-icon");
	{
		GtkCssProvider *css = gtk_css_provider_new ();
		gtk_css_provider_load_from_string (css,
			"window.verne-status-icon { background-color: alpha(@window_bg_color, 0.95);"
			" border-radius: 8px; border: 1px solid alpha(@window_fg_color, 0.35);"
			" min-width: 40px; min-height: 40px; padding: 4px; }");
		gtk_style_context_add_provider_for_display (gdk_display_get_default (),
							    GTK_STYLE_PROVIDER (css),
							    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_unref (css);
	}
#ifdef GDK_WINDOWING_X11
	g_signal_connect_swapped (icon->window, "map", G_CALLBACK (xapp_status_icon_place), icon);
	g_signal_connect (icon->window, "map", G_CALLBACK (verne_status_icon_mapped), NULL);
#endif
	img = gtk_image_new_from_icon_name (icon->icon_name ? icon->icon_name : "system-run", GTK_ICON_SIZE_BUTTON);
	gtk_image_set_pixel_size (GTK_IMAGE (img), 24);
	gtk_widget_set_hexpand (img, TRUE);
	gtk_widget_set_vexpand (img, TRUE);
	gtk_window_set_child (GTK_WINDOW (icon->window), img);
	icon->image = img;
	if (icon->tooltip)
		gtk_widget_set_tooltip_text (icon->window, icon->tooltip);
	click = gtk_gesture_click_new ();
	g_signal_connect (click, "pressed", G_CALLBACK (xapp_status_icon_clicked), icon);
	gtk_widget_add_controller (icon->window, GTK_EVENT_CONTROLLER (click));
}

G_DEFINE_FINAL_TYPE (XAppStatusIcon, xapp_status_icon, G_TYPE_OBJECT)

static void
xapp_status_icon_dispose (GObject *object)
{
	XAppStatusIcon *icon = XAPP_STATUS_ICON (object);
	if (icon->window) {
		gtk_window_destroy (GTK_WINDOW (icon->window));
		icon->window = NULL;
		icon->image = NULL;
	}
	g_clear_pointer (&icon->icon_name, g_free);
	g_clear_pointer (&icon->tooltip, g_free);
	G_OBJECT_CLASS (xapp_status_icon_parent_class)->dispose (object);
}

static void xapp_status_icon_class_init (XAppStatusIconClass *c) {
	G_OBJECT_CLASS (c)->dispose = xapp_status_icon_dispose;
	g_signal_new ("activate", G_TYPE_FROM_CLASS (c), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
		      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
}
static void xapp_status_icon_init (XAppStatusIcon *i) {
	i->icon_name = g_strdup ("system-run");
}
XAppStatusIcon *xapp_status_icon_new (void) { return g_object_new (XAPP_TYPE_STATUS_ICON, NULL); }

void
xapp_status_icon_set_visible (XAppStatusIcon *icon, gboolean visible)
{
	if (icon == NULL)
		return;
	visible = !!visible;
	if (icon->visible == visible && icon->window != NULL) {
		if (!visible)
			return;
		xapp_status_icon_place (icon);
		return;
	}
	icon->visible = visible;
	if (!visible) {
		if (icon->window)
			gtk_widget_set_visible (icon->window, FALSE);
		return;
	}
	xapp_status_icon_ensure_window (icon);
	gtk_widget_set_visible (icon->window, TRUE);
	gtk_window_present (GTK_WINDOW (icon->window));
	xapp_status_icon_place (icon);
}

void
xapp_status_icon_set_icon_name (XAppStatusIcon *icon, const gchar *name)
{
	if (icon == NULL)
		return;
	g_free (icon->icon_name);
	icon->icon_name = g_strdup (name);
	if (icon->image && name) {
		GtkIconTheme *theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
		const gchar *use = name;
		if (theme && !gtk_icon_theme_has_icon (theme, name))
			use = "system-run";
		gtk_image_set_from_icon_name (GTK_IMAGE (icon->image), use);
		gtk_image_set_pixel_size (GTK_IMAGE (icon->image), 24);
	}
}

void
xapp_status_icon_set_tooltip_text (XAppStatusIcon *icon, const gchar *text)
{
	if (icon == NULL)
		return;
	g_free (icon->tooltip);
	icon->tooltip = g_strdup (text);
	if (icon->window)
		gtk_widget_set_tooltip_text (icon->window, text);
}

void
xapp_gtk_window_set_progress (GtkWindow *window, int progress)
{
#ifdef GDK_WINDOWING_X11
	GdkSurface *s;
	Display *dpy;
	Window xid;
	Atom atom;
	unsigned long val;

	if (window == NULL)
		return;
	s = gtk_native_get_surface (GTK_NATIVE (window));
	if (s == NULL || !GDK_IS_X11_SURFACE (s))
		return;
	dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));
	xid = gdk_x11_surface_get_xid (s);
	atom = XInternAtom (dpy, "_NET_WM_XAPP_PROGRESS", False);
	val = progress < 0 ? 0 : (progress > 100 ? 100 : (unsigned long) progress);
	XChangeProperty (dpy, xid, atom, XA_CARDINAL, 32, PropModeReplace,
			 (unsigned char *) &val, 1);
#else
	(void) window;
	(void) progress;
#endif
}

GtkTranslateFunc
verne_dummy_translate_func (void)
{
	return NULL;
}
