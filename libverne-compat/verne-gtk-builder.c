#include "config.h"
#include "verne-gtk-compat.h"

#ifdef gtk_builder_add_from_string
#undef gtk_builder_add_from_string
#endif
#ifdef gtk_builder_add_from_file
#undef gtk_builder_add_from_file
#endif
#ifdef gtk_builder_add_from_resource
#undef gtk_builder_add_from_resource
#endif

static void
replace_all (GString *s, const char *from, const char *to)
{
	const char *p;
	gssize from_len = (gssize) strlen (from);
	gssize to_len = (gssize) strlen (to);
	gsize i = 0;

	if (from_len == 0)
		return;
	while (i + (gsize) from_len <= s->len) {
		p = s->str + i;
		if (memcmp (p, from, (size_t) from_len) == 0) {
			g_string_erase (s, i, from_len);
			g_string_insert_len (s, i, to, to_len);
			i += (gsize) to_len;
		} else {
			i++;
		}
	}
}

static void
remove_property (GString *s, const char *name)
{
	char *start_pat = g_strdup_printf ("<property name=\"%s\">", name);
	gsize start_len = strlen (start_pat);

	for (;;) {
		const char *found = strstr (s->str, start_pat);
		const char *end;
		gsize pos, n;
		if (found == NULL)
			break;
		end = strstr (found, "</property>");
		if (end == NULL)
			break;
		end += strlen ("</property>");
		while (*end == '\n' || *end == '\r')
			end++;
		pos = (gsize) (found - s->str);
		n = (gsize) (end - found);
		g_string_erase (s, pos, (gssize) n);
	}
	g_free (start_pat);
}

static void
strip_packing_blocks (GString *s)
{
	for (;;) {
		const char *found = strstr (s->str, "<packing>");
		const char *end;
		gsize pos, n;
		if (found == NULL)
			break;
		end = strstr (found, "</packing>");
		if (end == NULL)
			break;
		end += strlen ("</packing>");
		while (*end == '\n' || *end == '\r')
			end++;
		pos = (gsize) (found - s->str);
		n = (gsize) (end - found);
		g_string_erase (s, pos, (gssize) n);
	}
}

static void
strip_requires (GString *s)
{
	for (;;) {
		const char *found = strstr (s->str, "<requires ");
		const char *end;
		gsize pos, n;
		if (found == NULL)
			break;
		end = strstr (found, "/>");
		if (end == NULL)
			break;
		end += 2;
		while (*end == '\n' || *end == '\r')
			end++;
		pos = (gsize) (found - s->str);
		n = (gsize) (end - found);
		g_string_erase (s, pos, (gssize) n);
	}
}

static void
strip_placeholders (GString *s)
{
	replace_all (s, "<placeholder/>", "");
	replace_all (s, "<placeholder />", "");
}

static void
strip_empty_child_tags (GString *s)
{
	replace_all (s, "<child>\n        </child>", "");
	replace_all (s, "<child></child>", "");
	replace_all (s, "<child/>", "");
}

static GHashTable *
collect_image_icons (const char *xml)
{
	GHashTable *table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	const char *p = xml;

	while ((p = strstr (p, "<object class=\"GtkImage\"")) != NULL) {
		const char *id_attr = strstr (p, "id=\"");
		const char *icon;
		const char *obj_end;
		gchar *id;
		const char *id_end;

		if (id_attr == NULL || id_attr > p + 80)
			break;
		id_attr += 4;
		id_end = strchr (id_attr, '"');
		if (id_end == NULL)
			break;
		id = g_strndup (id_attr, (gsize) (id_end - id_attr));
		obj_end = strstr (p, "</object>");
		icon = strstr (p, "<property name=\"icon-name\">");
		if (icon && obj_end && icon < obj_end) {
			const char *val = icon + strlen ("<property name=\"icon-name\">");
			const char *val_end = strstr (val, "</property>");
			if (val_end)
				g_hash_table_insert (table, id, g_strndup (val, (gsize) (val_end - val)));
			else
				g_free (id);
		} else {
			g_free (id);
		}
		p = p + 20;
	}
	return table;
}

static void
rewrite_image_properties (GString *s, GHashTable *icons)
{
	for (;;) {
		const char *found = strstr (s->str, "<property name=\"image\">");
		const char *end;
		const char *val;
		gchar *id;
		const char *icon;
		gchar *child;
		gsize pos, n;

		if (found == NULL)
			break;
		val = found + strlen ("<property name=\"image\">");
		end = strstr (val, "</property>");
		if (end == NULL)
			break;
		id = g_strndup (val, (gsize) (end - val));
		g_strstrip (id);
		icon = g_hash_table_lookup (icons, id);
		if (icon == NULL)
			icon = id;
		child = g_strdup_printf ("<child><object class=\"GtkImage\"><property name=\"icon-name\">%s</property></object></child>",
					 icon);
		pos = (gsize) (found - s->str);
		n = (gsize) (end + strlen ("</property>") - found);
		g_string_erase (s, pos, (gssize) n);
		g_string_insert (s, pos, child);
		g_free (child);
		g_free (id);
	}
}

static void
rewrite_stock_labels (GString *s)
{
	struct { const char *from; const char *to; } map[] = {
		{ "<property name=\"label\">gtk-sort-ascending</property>", "<property name=\"label\">Sort</property>" },
		{ "<property name=\"label\">gtk-jump-to</property>", "<property name=\"label\">Jump</property>" },
		{ "<property name=\"label\">gtk-remove</property>", "<property name=\"label\">Remove</property>" },
		{ "<property name=\"label\">gtk-close</property>", "<property name=\"label\">Close</property>" },
		{ "<property name=\"label\">gtk-ok</property>", "<property name=\"label\">OK</property>" },
		{ "<property name=\"label\">gtk-cancel</property>", "<property name=\"label\">Cancel</property>" },
		{ "<property name=\"label\">gtk-add</property>", "<property name=\"label\">Add</property>" },
		{ "<property name=\"label\">gtk-apply</property>", "<property name=\"label\">Apply</property>" },
		{ "<property name=\"label\">gtk-help</property>", "<property name=\"label\">Help</property>" },
	};
	guint i;
	for (i = 0; i < G_N_ELEMENTS (map); i++)
		replace_all (s, map[i].from, map[i].to);
}

static gchar *
verne_transform_gtk3_ui (const gchar *xml, gssize len)
{
	GString *s;
	GHashTable *icons;

	if (len < 0)
		len = (gssize) strlen (xml);
	s = g_string_new_len (xml, (gsize) len);

	strip_requires (s);
	icons = collect_image_icons (s->str);
	rewrite_image_properties (s, icons);
	g_hash_table_destroy (icons);

	replace_all (s, "XAppStackSidebar", "GtkStackSidebar");
	replace_all (s, "XAppGtkWindow", "GtkWindow");
	replace_all (s, "GtkButtonBox", "GtkBox");
	replace_all (s, "GtkAlignment", "GtkBox");
	replace_all (s, "GtkHButtonBox", "GtkBox");
	replace_all (s, "GtkVButtonBox", "GtkBox");
	replace_all (s, "GtkHBox", "GtkBox");
	replace_all (s, "GtkVBox", "GtkBox");
	replace_all (s, "GtkEventBox", "GtkBox");
	replace_all (s, "GtkHSeparator", "GtkSeparator");
	replace_all (s, "GtkVSeparator", "GtkSeparator");
	replace_all (s, "GtkTable", "GtkGrid");
	replace_all (s, "GtkArrow", "GtkImage");
	replace_all (s, "GtkRadioButton", "GtkCheckButton");
	replace_all (s, "GtkViewport", "GtkBox");
	replace_all (s, "class=\"GtkDialog\"", "class=\"GtkWindow\"");
	replace_all (s, "class=\"GtkInfoBar\"", "class=\"GtkBox\"");

	replace_all (s, " name=\"can-focus\"", " name=\"focusable\"");
	replace_all (s, " name=\"margin-left\"", " name=\"margin-start\"");
	replace_all (s, " name=\"margin-right\"", " name=\"margin-end\"");
	replace_all (s, " name=\"left-padding\"", " name=\"margin-start\"");
	replace_all (s, " name=\"right-padding\"", " name=\"margin-end\"");
	replace_all (s, " name=\"top-padding\"", " name=\"margin-top\"");
	replace_all (s, " name=\"bottom-padding\"", " name=\"margin-bottom\"");
	replace_all (s, "xsi-", "");

	replace_all (s, " internal-child=\"vbox\"", "");
	replace_all (s, " internal-child=\"action_area\"", "");
	replace_all (s, " internal-child=\"content_area\"", "");

	rewrite_stock_labels (s);
	strip_packing_blocks (s);
	strip_placeholders (s);

	remove_property (s, "window-position");
	remove_property (s, "type-hint");
	remove_property (s, "no-show-all");
	remove_property (s, "layout-style");
	remove_property (s, "use-stock");
	remove_property (s, "can-default");
	remove_property (s, "shadow-type");
	remove_property (s, "ignore-hidden");
	remove_property (s, "caps-lock-warning");
	remove_property (s, "primary-icon-activatable");
	remove_property (s, "secondary-icon-activatable");
	remove_property (s, "primary-icon-sensitive");
	remove_property (s, "secondary-icon-sensitive");
	remove_property (s, "focus-on-click");
	remove_property (s, "xalign");
	remove_property (s, "yalign");
	remove_property (s, "xpad");
	remove_property (s, "ypad");
	remove_property (s, "n-rows");
	remove_property (s, "n-columns");
	remove_property (s, "row-homogeneous");
	remove_property (s, "column-homogeneous");
	remove_property (s, "border-width");
	remove_property (s, "draw-indicator");

	strip_empty_child_tags (s);
	return g_string_free (s, FALSE);
}

gboolean
verne_gtk_builder_add_from_string (GtkBuilder *builder, const gchar *buffer, gssize length, GError **error)
{
	gchar *transformed;
	gboolean ok;
	GError *local = NULL;

	transformed = verne_transform_gtk3_ui (buffer, length);
	ok = gtk_builder_add_from_string (builder, transformed, -1, &local);
	if (!ok) {
		g_warning ("Verne GTK3 UI transform failed to load: %s", local ? local->message : "unknown");
		g_clear_error (&local);
		ok = gtk_builder_add_from_string (builder, buffer, length, error);
	} else if (error) {
		*error = NULL;
	}
	g_free (transformed);
	return ok;
}

gboolean
verne_gtk_builder_add_from_file (GtkBuilder *builder, const gchar *filename, GError **error)
{
	gchar *contents = NULL;
	gsize len = 0;

	if (!g_file_get_contents (filename, &contents, &len, error))
		return FALSE;
	if (!verne_gtk_builder_add_from_string (builder, contents, (gssize) len, error)) {
		g_free (contents);
		return FALSE;
	}
	g_free (contents);
	return TRUE;
}

gboolean
verne_gtk_builder_add_from_resource (GtkBuilder *builder, const gchar *path, GError **error)
{
	GBytes *bytes;
	gconstpointer data;
	gsize len;

	bytes = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, error);
	if (bytes == NULL)
		return FALSE;
	data = g_bytes_get_data (bytes, &len);
	if (!verne_gtk_builder_add_from_string (builder, data, (gssize) len, error)) {
		g_bytes_unref (bytes);
		return FALSE;
	}
	g_bytes_unref (bytes);
	return TRUE;
}
