#ifndef GNOME_DESKTOP_THUMBNAIL_H
#define GNOME_DESKTOP_THUMBNAIL_H
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL,
	GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE
} GnomeDesktopThumbnailSize;

#define GNOME_DESKTOP_TYPE_THUMBNAIL_FACTORY (gnome_desktop_thumbnail_factory_get_type())
G_DECLARE_FINAL_TYPE (GnomeDesktopThumbnailFactory, gnome_desktop_thumbnail_factory, GNOME_DESKTOP, THUMBNAIL_FACTORY, GObject)

GnomeDesktopThumbnailFactory *gnome_desktop_thumbnail_factory_new (GnomeDesktopThumbnailSize size);
gboolean gnome_desktop_thumbnail_factory_can_thumbnail (GnomeDesktopThumbnailFactory *factory, const char *uri, const char *mime_type, time_t mtime);
GdkPixbuf *gnome_desktop_thumbnail_factory_generate_thumbnail (GnomeDesktopThumbnailFactory *factory, const char *uri, const char *mime_type);
void gnome_desktop_thumbnail_factory_save_thumbnail (GnomeDesktopThumbnailFactory *factory, GdkPixbuf *thumbnail, const char *uri, time_t original_mtime);
void gnome_desktop_thumbnail_factory_create_failed_thumbnail (GnomeDesktopThumbnailFactory *factory, const char *uri, time_t mtime);
gboolean gnome_desktop_thumbnail_cache_check_permissions (GnomeDesktopThumbnailFactory *factory, gboolean strict);
void gnome_desktop_thumbnail_cache_fix_permissions (void);

G_END_DECLS
#endif
