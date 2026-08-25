#include "config.h"
#include "verne-gtk-compat.h"
#include <libxapp/xapp-favorites.h>
#include <libxapp/xapp-status-icon.h>
#include <libxapp/xapp-icon-chooser-dialog.h>
#include <libcinnamon-desktop/gnome-desktop-thumbnail.h>
#include <glib/gstdio.h>

struct _XAppFavorites {
	GObject parent;
	GHashTable *uris;
};

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
	g_signal_emit_by_name (favorites, "changed");
}

void
xapp_favorites_remove (XAppFavorites *favorites, const gchar *uri)
{
	g_hash_table_remove (favorites->uris, uri);
	g_signal_emit_by_name (favorites, "changed");
}

void
xapp_favorites_rename (XAppFavorites *favorites, const gchar *old_uri, const gchar *new_uri)
{
	if (g_hash_table_remove (favorites->uris, old_uri))
		g_hash_table_insert (favorites->uris, g_strdup (new_uri), g_strdup (new_uri));
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

struct _XAppStatusIcon { GObject parent; };
G_DEFINE_FINAL_TYPE (XAppStatusIcon, xapp_status_icon, G_TYPE_OBJECT)
static void xapp_status_icon_class_init (XAppStatusIconClass *c) {
	g_signal_new ("activate", G_TYPE_FROM_CLASS (c), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}
static void xapp_status_icon_init (XAppStatusIcon *i) { (void) i; }
XAppStatusIcon *xapp_status_icon_new (void) { return g_object_new (XAPP_TYPE_STATUS_ICON, NULL); }
void xapp_status_icon_set_visible (XAppStatusIcon *icon, gboolean visible) { (void) icon; (void) visible; }
void xapp_status_icon_set_icon_name (XAppStatusIcon *icon, const gchar *name) { (void) icon; (void) name; }
void xapp_status_icon_set_tooltip_text (XAppStatusIcon *icon, const gchar *text) { (void) icon; (void) text; }

GtkWidget *
xapp_icon_chooser_dialog_new (void)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons ("Choose Icon", NULL, 0,
							 "_Cancel", GTK_RESPONSE_CANCEL,
							 "_OK", GTK_RESPONSE_OK, NULL);
	GtkWidget *entry = gtk_entry_new ();
	g_object_set_data (G_OBJECT (dialog), "icon-entry", entry);
	gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), entry);
	return dialog;
}

void
xapp_icon_chooser_dialog_add_button (GtkDialog *dialog, GtkWidget *button, GtkResponseType response, GtkResponseType default_response)
{
	(void) default_response;
	gtk_dialog_add_action_widget (dialog, button, response);
}

gint xapp_icon_chooser_dialog_run (GtkDialog *dialog) { return gtk_dialog_run (dialog); }
gint xapp_icon_chooser_dialog_run_with_icon (GtkDialog *dialog, const gchar *icon) {
	GtkWidget *entry = g_object_get_data (G_OBJECT (dialog), "icon-entry");
	if (entry && icon)
		gtk_editable_set_text (GTK_EDITABLE (entry), icon);
	return gtk_dialog_run (dialog);
}
gchar *
xapp_icon_chooser_dialog_get_icon_string (GtkDialog *dialog)
{
	GtkWidget *entry = g_object_get_data (G_OBJECT (dialog), "icon-entry");
	return entry ? g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry))) : g_strdup ("folder");
}

struct _GnomeDesktopThumbnailFactory {
	GObject parent;
	GnomeDesktopThumbnailSize size;
};

G_DEFINE_FINAL_TYPE (GnomeDesktopThumbnailFactory, gnome_desktop_thumbnail_factory, G_TYPE_OBJECT)
static void gnome_desktop_thumbnail_factory_class_init (GnomeDesktopThumbnailFactoryClass *c) { (void) c; }
static void gnome_desktop_thumbnail_factory_init (GnomeDesktopThumbnailFactory *f) { f->size = GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE; }

GnomeDesktopThumbnailFactory *
gnome_desktop_thumbnail_factory_new (GnomeDesktopThumbnailSize size)
{
	GnomeDesktopThumbnailFactory *f = g_object_new (GNOME_DESKTOP_TYPE_THUMBNAIL_FACTORY, NULL);
	f->size = size;
	return f;
}

gboolean
gnome_desktop_thumbnail_factory_can_thumbnail (GnomeDesktopThumbnailFactory *factory, const char *uri, const char *mime_type, time_t mtime)
{
	(void) factory; (void) uri; (void) mtime;
	return mime_type && g_str_has_prefix (mime_type, "image/");
}

GdkPixbuf *
gnome_desktop_thumbnail_factory_generate_thumbnail (GnomeDesktopThumbnailFactory *factory, const char *uri, const char *mime_type)
{
	GFile *file;
	gchar *path;
	GdkPixbuf *pixbuf;
	int dim = factory->size == GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE ? 256 : 128;
	(void) mime_type;
	file = g_file_new_for_uri (uri);
	path = g_file_get_path (file);
	g_object_unref (file);
	if (path == NULL)
		return NULL;
	pixbuf = gdk_pixbuf_new_from_file_at_size (path, dim, dim, NULL);
	g_free (path);
	return pixbuf;
}

void
gnome_desktop_thumbnail_factory_save_thumbnail (GnomeDesktopThumbnailFactory *factory, GdkPixbuf *thumbnail, const char *uri, time_t original_mtime)
{
	(void) factory; (void) thumbnail; (void) uri; (void) original_mtime;
}

void
gnome_desktop_thumbnail_factory_create_failed_thumbnail (GnomeDesktopThumbnailFactory *factory, const char *uri, time_t mtime)
{
	(void) factory; (void) uri; (void) mtime;
}

gboolean
gnome_desktop_thumbnail_cache_check_permissions (GnomeDesktopThumbnailFactory *factory, gboolean strict)
{
	(void) factory; (void) strict;
	return TRUE;
}

void
gnome_desktop_thumbnail_cache_fix_permissions (void)
{
}

GtkTranslateFunc
verne_dummy_translate_func (void)
{
	return NULL;
}
