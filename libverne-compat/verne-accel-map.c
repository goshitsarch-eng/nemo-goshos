/* GTK3 GtkAccelMap load/save/lookup for custom shortcut persistence. */
#include "config.h"
#include "verne-gtk-compat.h"

#include <stdio.h>
#include <string.h>

struct _GtkAccelMap {
	GObject parent;
};

struct _GtkAccelMapClass {
	GObjectClass parent_class;
};

enum {
	ACCEL_MAP_CHANGED,
	ACCEL_MAP_LAST_SIGNAL
};

static guint accel_map_signals[ACCEL_MAP_LAST_SIGNAL];
static GtkAccelMap *default_accel_map;
static GHashTable *accel_entries;

G_DEFINE_TYPE (GtkAccelMap, gtk_accel_map, G_TYPE_OBJECT)

static void
gtk_accel_map_class_init (GtkAccelMapClass *klass)
{
	accel_map_signals[ACCEL_MAP_CHANGED] =
		g_signal_new ("changed", G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
			      G_TYPE_NONE, 3,
			      G_TYPE_STRING, G_TYPE_UINT, GDK_TYPE_MODIFIER_TYPE);
}

static void
gtk_accel_map_init (GtkAccelMap *map)
{
	(void) map;
	if (accel_entries == NULL)
		accel_entries = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

GtkAccelMap *
gtk_accel_map_get (void)
{
	if (default_accel_map == NULL)
		default_accel_map = g_object_new (GTK_TYPE_ACCEL_MAP, NULL);
	return default_accel_map;
}

static void
ensure_map (void)
{
	(void) gtk_accel_map_get ();
}

void
gtk_accel_map_add_entry (const gchar *accel_path, guint accel_key, GdkModifierType accel_mods)
{
	GtkAccelKey *existing;

	if (accel_path == NULL || accel_path[0] == '\0')
		return;
	ensure_map ();
	existing = g_hash_table_lookup (accel_entries, accel_path);
	if (existing)
		return;
	existing = g_new0 (GtkAccelKey, 1);
	existing->accel_key = accel_key;
	existing->accel_mods = accel_mods;
	g_hash_table_insert (accel_entries, g_strdup (accel_path), existing);
}

gboolean
gtk_accel_map_lookup_entry (const gchar *accel_path, GtkAccelKey *key)
{
	GtkAccelKey *existing;

	if (accel_path == NULL)
		return FALSE;
	ensure_map ();
	existing = g_hash_table_lookup (accel_entries, accel_path);
	if (existing == NULL)
		return FALSE;
	if (key)
		*key = *existing;
	return TRUE;
}

gboolean
gtk_accel_map_change_entry (const gchar *accel_path, guint accel_key,
			    GdkModifierType accel_mods, gboolean replace)
{
	GtkAccelKey *existing;

	if (accel_path == NULL || accel_path[0] == '\0')
		return FALSE;
	ensure_map ();
	existing = g_hash_table_lookup (accel_entries, accel_path);
	if (existing) {
		if (!replace)
			return FALSE;
		if (existing->accel_key == accel_key && existing->accel_mods == accel_mods)
			return TRUE;
		existing->accel_key = accel_key;
		existing->accel_mods = accel_mods;
	} else {
		existing = g_new0 (GtkAccelKey, 1);
		existing->accel_key = accel_key;
		existing->accel_mods = accel_mods;
		g_hash_table_insert (accel_entries, g_strdup (accel_path), existing);
	}
	g_signal_emit (gtk_accel_map_get (), accel_map_signals[ACCEL_MAP_CHANGED], 0,
		       accel_path, accel_key, accel_mods);
	return TRUE;
}

void
gtk_accel_map_save (const gchar *file_name)
{
	FILE *fp;
	GHashTableIter iter;
	gpointer key, value;

	if (file_name == NULL)
		return;
	ensure_map ();
	fp = fopen (file_name, "w");
	if (fp == NULL)
		return;
	fprintf (fp, "; verne GtkAccelMap rc-file         -*- scheme -*-\n");
	fprintf (fp, "; this file is an automated accelerator map dump\n;\n");
	g_hash_table_iter_init (&iter, accel_entries);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *path = key;
		GtkAccelKey *ak = value;
		gchar *name = gtk_accelerator_name (ak->accel_key, ak->accel_mods);

		fprintf (fp, "(gtk_accel_path \"%s\" \"%s\")\n", path, name ? name : "");
		g_free (name);
	}
	fclose (fp);
}

static gchar *
parse_quoted (const gchar **pp)
{
	GString *s;
	const gchar *p = *pp;

	while (*p == ' ' || *p == '\t')
		p++;
	if (*p != '"')
		return NULL;
	p++;
	s = g_string_new (NULL);
	while (*p && *p != '"') {
		if (*p == '\\' && p[1]) {
			p++;
			g_string_append_c (s, *p++);
		} else {
			g_string_append_c (s, *p++);
		}
	}
	if (*p == '"')
		p++;
	*pp = p;
	return g_string_free (s, FALSE);
}

void
gtk_accel_map_load (const gchar *file_name)
{
	gchar *contents = NULL;
	const gchar *p;

	if (file_name == NULL)
		return;
	ensure_map ();
	if (!g_file_get_contents (file_name, &contents, NULL, NULL) || contents == NULL)
		return;
	p = contents;
	while (*p) {
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
			p++;
		if (*p == ';' || *p == '#') {
			while (*p && *p != '\n')
				p++;
			continue;
		}
		if (g_str_has_prefix (p, "(gtk_accel_path")) {
			gchar *path, *accel;
			guint key = 0;
			GdkModifierType mods = 0;
			GtkAccelKey *ak;

			p += strlen ("(gtk_accel_path");
			path = parse_quoted (&p);
			accel = parse_quoted (&p);
			while (*p && *p != '\n')
				p++;
			if (path) {
				if (accel && accel[0])
					gtk_accelerator_parse (accel, &key, &mods);
				ak = g_new0 (GtkAccelKey, 1);
				ak->accel_key = key;
				ak->accel_mods = mods;
				g_hash_table_insert (accel_entries, path, ak);
				path = NULL;
			}
			g_free (path);
			g_free (accel);
			continue;
		}
		while (*p && *p != '\n')
			p++;
	}
	g_free (contents);
}
