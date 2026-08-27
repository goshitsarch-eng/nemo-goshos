#include "config.h"
#include "verne-gtk-compat.h"
#include <libcinnamon-desktop/gnome-desktop-thumbnail.h>
#include <glib/gstdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
	gchar *tryexec;
	gchar *exec;
	gchar **mimes;
} VerneThumbnailer;

struct _GnomeDesktopThumbnailFactory {
	GObject parent;
	GnomeDesktopThumbnailSize size;
};

G_DEFINE_FINAL_TYPE (GnomeDesktopThumbnailFactory, gnome_desktop_thumbnail_factory, G_TYPE_OBJECT)

static GPtrArray *verne_thumbnailers;
static GMutex verne_thumbnailers_lock;

static void
gnome_desktop_thumbnail_factory_class_init (GnomeDesktopThumbnailFactoryClass *c)
{
	(void) c;
}

static void
gnome_desktop_thumbnail_factory_init (GnomeDesktopThumbnailFactory *f)
{
	f->size = GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE;
}

static int
verne_thumb_dim (GnomeDesktopThumbnailFactory *factory)
{
	return factory && factory->size == GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL ? 128 : 256;
}

static const char *
verne_thumb_size_name (GnomeDesktopThumbnailFactory *factory)
{
	return factory && factory->size == GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL ? "normal" : "large";
}

static gchar *
verne_thumb_md5 (const char *uri)
{
	return g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
}

static gchar *
verne_thumb_cache_dir (GnomeDesktopThumbnailFactory *factory)
{
	return g_build_filename (g_get_user_cache_dir (), "thumbnails",
				 verne_thumb_size_name (factory), NULL);
}

static gchar *
verne_thumb_fail_dir (void)
{
	return g_build_filename (g_get_user_cache_dir (), "thumbnails",
				 "fail", "gnome-thumbnail-factory", NULL);
}

static gchar *
verne_thumb_path_for_uri (GnomeDesktopThumbnailFactory *factory, const char *uri)
{
	gchar *hash = verne_thumb_md5 (uri);
	gchar *dir = verne_thumb_cache_dir (factory);
	gchar *path = g_strconcat (dir, "/", hash, ".png", NULL);
	g_free (hash);
	g_free (dir);
	return path;
}

static gchar *
verne_thumb_fail_path (const char *uri)
{
	gchar *hash = verne_thumb_md5 (uri);
	gchar *dir = verne_thumb_fail_dir ();
	gchar *path = g_strconcat (dir, "/", hash, ".png", NULL);
	g_free (hash);
	g_free (dir);
	return path;
}

static gboolean
verne_mime_matches (const char *mime, const char *pattern)
{
	const char *star;

	if (mime == NULL || pattern == NULL)
		return FALSE;
	if (g_ascii_strcasecmp (mime, pattern) == 0)
		return TRUE;
	star = strchr (pattern, '*');
	if (star && star[1] == '\0' && star > pattern && star[-1] == '/')
		return g_ascii_strncasecmp (mime, pattern, (gsize) (star - pattern)) == 0;
	return FALSE;
}

static void
verne_thumbnailer_free (gpointer data)
{
	VerneThumbnailer *t = data;
	if (!t)
		return;
	g_free (t->tryexec);
	g_free (t->exec);
	g_strfreev (t->mimes);
	g_free (t);
}

static void
verne_load_thumbnailers_dir (const char *dir)
{
	GDir *gd;
	const gchar *name;

	if (dir == NULL || !g_file_test (dir, G_FILE_TEST_IS_DIR))
		return;
	gd = g_dir_open (dir, 0, NULL);
	if (gd == NULL)
		return;
	while ((name = g_dir_read_name (gd)) != NULL) {
		gchar *path;
		GKeyFile *key;
		VerneThumbnailer *t;
		gchar *mimes;

		if (!g_str_has_suffix (name, ".thumbnailer"))
			continue;
		path = g_build_filename (dir, name, NULL);
		key = g_key_file_new ();
		if (!g_key_file_load_from_file (key, path, G_KEY_FILE_NONE, NULL)) {
			g_key_file_free (key);
			g_free (path);
			continue;
		}
		t = g_new0 (VerneThumbnailer, 1);
		t->exec = g_key_file_get_string (key, "Thumbnailer Entry", "Exec", NULL);
		t->tryexec = g_key_file_get_string (key, "Thumbnailer Entry", "TryExec", NULL);
		mimes = g_key_file_get_string (key, "Thumbnailer Entry", "MimeType", NULL);
		if (mimes) {
			t->mimes = g_strsplit (mimes, ";", -1);
			g_free (mimes);
		}
		if (t->exec == NULL) {
			verne_thumbnailer_free (t);
		} else {
			g_ptr_array_add (verne_thumbnailers, t);
		}
		g_key_file_free (key);
		g_free (path);
	}
	g_dir_close (gd);
}

static void
verne_ensure_thumbnailers (void)
{
	const gchar *const *data_dirs;
	int i;

	g_mutex_lock (&verne_thumbnailers_lock);
	if (verne_thumbnailers != NULL) {
		g_mutex_unlock (&verne_thumbnailers_lock);
		return;
	}
	verne_thumbnailers = g_ptr_array_new_with_free_func (verne_thumbnailer_free);
	{
		gchar *user_dir = g_build_filename (g_get_user_data_dir (), "thumbnailers", NULL);
		verne_load_thumbnailers_dir (user_dir);
		g_free (user_dir);
	}
	data_dirs = g_get_system_data_dirs ();
	for (i = 0; data_dirs[i] != NULL; i++) {
		gchar *dir = g_build_filename (data_dirs[i], "thumbnailers", NULL);
		verne_load_thumbnailers_dir (dir);
		g_free (dir);
	}
	verne_load_thumbnailers_dir ("/usr/share/thumbnailers");
	g_mutex_unlock (&verne_thumbnailers_lock);
}

static gboolean
verne_pixbuf_supports_mime (const char *mime_type)
{
	GSList *formats, *l;
	gboolean ok = FALSE;

	if (mime_type == NULL)
		return FALSE;
	formats = gdk_pixbuf_get_formats ();
	for (l = formats; l; l = l->next) {
		gchar **types = gdk_pixbuf_format_get_mime_types (l->data);
		int i;
		for (i = 0; types && types[i]; i++) {
			if (g_ascii_strcasecmp (types[i], mime_type) == 0) {
				ok = TRUE;
				break;
			}
		}
		g_strfreev (types);
		if (ok)
			break;
	}
	g_slist_free (formats);
	return ok;
}

static VerneThumbnailer *
verne_find_thumbnailer (const char *mime_type)
{
	guint i;

	if (mime_type == NULL)
		return NULL;
	verne_ensure_thumbnailers ();
	for (i = 0; i < verne_thumbnailers->len; i++) {
		VerneThumbnailer *t = g_ptr_array_index (verne_thumbnailers, i);
		int m;
		if (t->tryexec && t->tryexec[0]) {
			gchar *prog = g_find_program_in_path (t->tryexec);
			if (prog == NULL)
				continue;
			g_free (prog);
		}
		if (t->mimes == NULL)
			continue;
		for (m = 0; t->mimes[m] && t->mimes[m][0]; m++) {
			if (verne_mime_matches (mime_type, t->mimes[m]))
				return t;
		}
	}
	return NULL;
}

static gchar *
verne_expand_thumbnailer (const gchar *exec, const gchar *uri, const gchar *input, const gchar *output, int size)
{
	GString *s = g_string_new (NULL);
	const gchar *p;

	for (p = exec; *p; p++) {
		if (*p == '%' && p[1]) {
			switch (p[1]) {
			case 'u':
			case 'U':
				g_string_append (s, uri ? uri : "");
				p++;
				break;
			case 'i':
			case 'I':
				g_string_append (s, input ? input : (uri ? uri : ""));
				p++;
				break;
			case 'o':
			case 'O':
				g_string_append (s, output ? output : "");
				p++;
				break;
			case 's':
				g_string_append_printf (s, "%d", size);
				p++;
				break;
			case '%':
				g_string_append_c (s, '%');
				p++;
				break;
			default:
				g_string_append_c (s, *p);
				break;
			}
		} else {
			g_string_append_c (s, *p);
		}
	}
	return g_string_free (s, FALSE);
}

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
	gchar *fail;
	(void) factory;

	if (uri == NULL || mime_type == NULL)
		return FALSE;
	fail = verne_thumb_fail_path (uri);
	if (g_file_test (fail, G_FILE_TEST_EXISTS)) {
		GStatBuf st;
		if (g_stat (fail, &st) == 0 && (time_t) st.st_mtime >= mtime) {
			g_free (fail);
			return FALSE;
		}
	}
	g_free (fail);
	if (verne_pixbuf_supports_mime (mime_type))
		return TRUE;
	if (verne_find_thumbnailer (mime_type) != NULL)
		return TRUE;
	return g_str_has_prefix (mime_type, "image/");
}

static GdkPixbuf *
verne_run_thumbnailer (VerneThumbnailer *t, const char *uri, const char *path, int dim)
{
	gchar *tmp, *expanded;
	gchar **argv = NULL;
	GError *err = NULL;
	gint status = 0;
	GdkPixbuf *pixbuf = NULL;

	tmp = g_build_filename (g_get_tmp_dir (), "verne-thumb-XXXXXX.png", NULL);
	{
		gint fd = g_mkstemp (tmp);
		if (fd < 0) {
			g_free (tmp);
			return NULL;
		}
		close (fd);
	}
	expanded = verne_expand_thumbnailer (t->exec, uri, path, tmp, dim);
	if (!g_shell_parse_argv (expanded, NULL, &argv, &err)) {
		g_clear_error (&err);
		g_free (expanded);
		g_unlink (tmp);
		g_free (tmp);
		return NULL;
	}
	if (g_spawn_sync (NULL, argv, NULL,
			  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
			  NULL, NULL, NULL, NULL, &status, &err) &&
	    status == 0) {
		pixbuf = gdk_pixbuf_new_from_file_at_size (tmp, dim, dim, NULL);
	}
	g_clear_error (&err);
	g_strfreev (argv);
	g_free (expanded);
	g_unlink (tmp);
	g_free (tmp);
	return pixbuf;
}

GdkPixbuf *
gnome_desktop_thumbnail_factory_generate_thumbnail (GnomeDesktopThumbnailFactory *factory, const char *uri, const char *mime_type)
{
	GFile *file;
	gchar *path;
	GdkPixbuf *pixbuf = NULL;
	int dim = verne_thumb_dim (factory);
	VerneThumbnailer *t;

	if (uri == NULL)
		return NULL;
	file = g_file_new_for_uri (uri);
	path = g_file_get_path (file);
	g_object_unref (file);

	t = verne_find_thumbnailer (mime_type);
	if (t != NULL)
		pixbuf = verne_run_thumbnailer (t, uri, path, dim);
	if (pixbuf == NULL && path != NULL)
		pixbuf = gdk_pixbuf_new_from_file_at_size (path, dim, dim, NULL);
	g_free (path);
	return pixbuf;
}

static gboolean
verne_save_thumb_png (GdkPixbuf *thumbnail, const char *path, const char *uri, time_t original_mtime)
{
	gchar mtime_str[32];
	g_snprintf (mtime_str, sizeof mtime_str, "%ld", (long) original_mtime);
	return gdk_pixbuf_save (thumbnail, path, "png", NULL,
				"tEXt::Thumb::URI", uri,
				"tEXt::Thumb::MTime", mtime_str,
				NULL);
}

void
gnome_desktop_thumbnail_factory_save_thumbnail (GnomeDesktopThumbnailFactory *factory, GdkPixbuf *thumbnail, const char *uri, time_t original_mtime)
{
	gchar *dir;
	gchar *path;

	if (thumbnail == NULL || uri == NULL)
		return;
	dir = verne_thumb_cache_dir (factory);
	g_mkdir_with_parents (dir, 0700);
	path = verne_thumb_path_for_uri (factory, uri);
	verne_save_thumb_png (thumbnail, path, uri, original_mtime);
	g_free (path);
	g_free (dir);
}

void
gnome_desktop_thumbnail_factory_create_failed_thumbnail (GnomeDesktopThumbnailFactory *factory, const char *uri, time_t mtime)
{
	gchar *dir;
	gchar *path;
	GdkPixbuf *blank;

	(void) factory;
	if (uri == NULL)
		return;
	dir = verne_thumb_fail_dir ();
	g_mkdir_with_parents (dir, 0700);
	path = verne_thumb_fail_path (uri);
	blank = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
	gdk_pixbuf_fill (blank, 0);
	verne_save_thumb_png (blank, path, uri, mtime);
	g_object_unref (blank);
	g_free (path);
	g_free (dir);
}

gboolean
gnome_desktop_thumbnail_cache_check_permissions (GnomeDesktopThumbnailFactory *factory, gboolean strict)
{
	gchar *dir;
	GStatBuf st;

	(void) factory;
	dir = g_build_filename (g_get_user_cache_dir (), "thumbnails", NULL);
	g_mkdir_with_parents (dir, 0700);
	if (g_stat (dir, &st) != 0) {
		g_free (dir);
		return !strict;
	}
	if ((st.st_uid == getuid ()) && (st.st_mode & 0077) != 0)
		g_chmod (dir, 0700);
	if (g_stat (dir, &st) != 0) {
		g_free (dir);
		return !strict;
	}
	g_free (dir);
	if (!strict)
		return TRUE;
	return (st.st_mode & 0077) == 0;
}

void
gnome_desktop_thumbnail_cache_fix_permissions (void)
{
	gchar *dir = g_build_filename (g_get_user_cache_dir (), "thumbnails", NULL);
	g_mkdir_with_parents (dir, 0700);
	g_chmod (dir, 0700);
	g_free (dir);
}
