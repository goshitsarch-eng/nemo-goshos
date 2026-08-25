#ifndef XAPP_FAVORITES_H
#define XAPP_FAVORITES_H
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define XAPP_TYPE_FAVORITES (xapp_favorites_get_type ())
G_DECLARE_FINAL_TYPE (XAppFavorites, xapp_favorites, XAPP, FAVORITES, GObject)

typedef struct {
	gchar *uri;
	gchar *display_name;
} XAppFavoriteInfo;

XAppFavorites *xapp_favorites_get_default (void);
void xapp_favorites_add (XAppFavorites *favorites, const gchar *uri);
void xapp_favorites_remove (XAppFavorites *favorites, const gchar *uri);
void xapp_favorites_rename (XAppFavorites *favorites, const gchar *old_uri, const gchar *new_uri);
gint xapp_favorites_get_n_favorites (XAppFavorites *favorites);
GList *xapp_favorites_get_favorites (XAppFavorites *favorites, const gchar **mimetypes);
XAppFavoriteInfo *xapp_favorites_find_by_uri (XAppFavorites *favorites, const gchar *uri);
void xapp_favorite_info_free (XAppFavoriteInfo *info);

G_END_DECLS
#endif
