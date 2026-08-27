#include "config.h"
#include "verne-gtk-compat.h"
#include <stdlib.h>
#include <string.h>

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
strip_xml_block (GString *s, const char *open_tag, const char *close_tag)
{
	for (;;) {
		const char *found = strstr (s->str, open_tag);
		const char *end;
		gsize pos, n;
		if (found == NULL)
			break;
		end = strstr (found, close_tag);
		if (end == NULL)
			break;
		end += strlen (close_tag);
		while (*end == '\n' || *end == '\r')
			end++;
		pos = (gsize) (found - s->str);
		n = (gsize) (end - found);
		g_string_erase (s, pos, (gssize) n);
	}
}

static void
strip_packing_blocks (GString *s)
{
	strip_xml_block (s, "<packing>", "</packing>");
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

static gchar *
xml_prop_in_block (const char *block, const char *prop)
{
	gchar *pat = g_strdup_printf ("<property name=\"%s\"", prop);
	const char *found = strstr (block, pat);
	const char *gt;
	const char *end;
	gchar *value;

	g_free (pat);
	if (found == NULL)
		return NULL;
	gt = strchr (found, '>');
	if (gt == NULL)
		return NULL;
	end = strstr (gt + 1, "</property>");
	if (end == NULL)
		return NULL;
	value = g_strndup (gt + 1, (gsize) (end - (gt + 1)));
	g_strstrip (value);
	return value;
}

static gssize
find_object_open_for_close (const GString *s, gsize close_pos)
{
	int depth = 1;
	gsize i = close_pos;

	while (i > 0) {
		i--;
		if (i + 9 <= s->len && memcmp (s->str + i, "</object>", 9) == 0)
			depth++;
		else if (i + 7 <= s->len && memcmp (s->str + i, "<object", 7) == 0) {
			char next = s->str[i + 7];
			if (next == ' ' || next == '\t' || next == '\n' || next == '>') {
				depth--;
				if (depth == 0)
					return (gssize) i;
			}
		}
	}
	return -1;
}

static void
convert_numeric_align (GString *s, const char *from_name, const char *to_name)
{
	gchar *start_pat = g_strdup_printf ("<property name=\"%s\">", from_name);

	for (;;) {
		const char *found = strstr (s->str, start_pat);
		const char *end;
		gchar *raw;
		const char *align;
		gchar *repl;
		double v;
		gsize pos, n;

		if (found == NULL)
			break;
		end = strstr (found, "</property>");
		if (end == NULL)
			break;
		raw = g_strndup (found + strlen (start_pat),
				 (gsize) (end - (found + strlen (start_pat))));
		g_strstrip (raw);
		v = g_ascii_strtod (raw, NULL);
		align = (v < 0.25) ? "start" : ((v > 0.75) ? "end" : "center");
		repl = g_strdup_printf ("<property name=\"%s\">%s</property>", to_name, align);
		pos = (gsize) (found - s->str);
		n = (gsize) (end + strlen ("</property>") - found);
		g_string_erase (s, pos, (gssize) n);
		g_string_insert (s, pos, repl);
		g_free (repl);
		g_free (raw);
	}
	g_free (start_pat);
}

static void
convert_border_width (GString *s)
{
	const char *pat = "<property name=\"border-width\">";

	for (;;) {
		const char *found = strstr (s->str, pat);
		const char *end;
		gchar *raw;
		gchar *repl;
		gsize pos, n;

		if (found == NULL)
			break;
		end = strstr (found, "</property>");
		if (end == NULL)
			break;
		raw = g_strndup (found + strlen (pat),
				 (gsize) (end - (found + strlen (pat))));
		g_strstrip (raw);
		repl = g_strdup_printf ("<property name=\"margin-start\">%s</property>"
					"<property name=\"margin-end\">%s</property>"
					"<property name=\"margin-top\">%s</property>"
					"<property name=\"margin-bottom\">%s</property>",
					raw, raw, raw, raw);
		pos = (gsize) (found - s->str);
		n = (gsize) (end + strlen ("</property>") - found);
		g_string_erase (s, pos, (gssize) n);
		g_string_insert (s, pos, repl);
		g_free (repl);
		g_free (raw);
	}
}

static void
rewrite_packing_blocks (GString *s)
{
	for (;;) {
		const char *found = strstr (s->str, "<packing>");
		const char *end;
		gchar *block;
		gchar *left, *top, *width, *height, *expand;
		gsize pos, n, close_search;
		gboolean have_close;
		gssize obj_open = -1;
		gsize obj_close = 0;
		GString *insert;

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
		block = g_strndup (found, n);

		left = xml_prop_in_block (block, "left-attach");
		top = xml_prop_in_block (block, "top-attach");
		width = xml_prop_in_block (block, "width");
		height = xml_prop_in_block (block, "height");
		expand = xml_prop_in_block (block, "expand");

		close_search = pos;
		while (close_search > 0 && g_ascii_isspace (s->str[close_search - 1]))
			close_search--;
		have_close = (close_search >= 9 &&
			      memcmp (s->str + close_search - 9, "</object>", 9) == 0);
		if (have_close) {
			obj_close = close_search - 9;
			obj_open = find_object_open_for_close (s, obj_close);
		}

		insert = g_string_new (NULL);
		if (obj_open >= 0) {
			if (left != NULL || top != NULL) {
				g_string_append (insert, "<layout>");
				g_string_append_printf (insert, "<property name=\"column\">%s</property>",
							left ? left : "0");
				g_string_append_printf (insert, "<property name=\"row\">%s</property>",
							top ? top : "0");
				if (width)
					g_string_append_printf (insert, "<property name=\"column-span\">%s</property>", width);
				if (height)
					g_string_append_printf (insert, "<property name=\"row-span\">%s</property>", height);
				g_string_append (insert, "</layout>");
			}
			if (expand != NULL && g_ascii_strcasecmp (expand, "True") == 0) {
				g_string_append (insert, "<property name=\"hexpand\">True</property>");
				g_string_append (insert, "<property name=\"vexpand\">True</property>");
			}
			if (insert->len > 0)
				g_string_insert (s, obj_close, insert->str);
		}

		found = strstr (s->str, "<packing>");
		if (found) {
			end = strstr (found, "</packing>");
			if (end) {
				end += strlen ("</packing>");
				while (*end == '\n' || *end == '\r')
					end++;
				pos = (gsize) (found - s->str);
				n = (gsize) (end - found);
				g_string_erase (s, pos, (gssize) n);
			} else {
				g_string_erase (s, pos, 1);
			}
		}

		g_string_free (insert, TRUE);
		g_free (left);
		g_free (top);
		g_free (width);
		g_free (height);
		g_free (expand);
		g_free (block);
	}
}

typedef struct {
	gchar *name;
	gchar *title;
	gchar *icon;
} VerneStackPageInfo;

typedef struct {
	gchar *stack_id;
	GPtrArray *pages;
} VerneStackInfo;

static void
stack_page_info_free (gpointer data)
{
	VerneStackPageInfo *page = data;
	g_free (page->name);
	g_free (page->title);
	g_free (page->icon);
	g_free (page);
}

static void
stack_info_free (gpointer data)
{
	VerneStackInfo *info = data;
	g_free (info->stack_id);
	g_ptr_array_unref (info->pages);
	g_free (info);
}

static const char *
find_matching_object_end (const char *start, const char *limit)
{
	int depth = 0;
	const char *p = start;

	while (p < limit && *p) {
		if (memcmp (p, "<object", 7) == 0) {
			char next = p[7];
			if (next == ' ' || next == '\t' || next == '\n' || next == '>') {
				const char *gt = memchr (p, '>', (gsize) (limit - p));
				if (gt && gt > p && *(gt - 1) == '/') {
					p = gt;
				} else {
					depth++;
				}
			}
		} else if (memcmp (p, "</object>", 9) == 0) {
			depth--;
			if (depth == 0)
				return p + 9;
			p += 8;
		}
		p++;
	}
	return NULL;
}

static GPtrArray *
collect_stacks_from_xml (const char *xml)
{
	GPtrArray *stacks = g_ptr_array_new_with_free_func (stack_info_free);
	const char *p = xml;

	while ((p = strstr (p, "class=\"GtkStack\"")) != NULL) {
		const char *obj = p;
		const char *gt;
		const char *end;
		const char *id_attr;
		const char *q;
		VerneStackInfo *info;

		while (obj > xml && memcmp (obj, "<object", 7) != 0)
			obj--;
		if (memcmp (obj, "<object", 7) != 0) {
			p += 10;
			continue;
		}
		gt = strchr (obj, '>');
		if (gt == NULL)
			break;
		end = find_matching_object_end (obj, xml + strlen (xml));
		if (end == NULL) {
			p = gt + 1;
			continue;
		}

		info = g_new0 (VerneStackInfo, 1);
		info->pages = g_ptr_array_new_with_free_func (stack_page_info_free);
		id_attr = strstr (obj, "id=\"");
		if (id_attr && id_attr < gt) {
			const char *id_end = strchr (id_attr + 4, '"');
			if (id_end)
				info->stack_id = g_strndup (id_attr + 4, (gsize) (id_end - (id_attr + 4)));
		}

		q = obj;
		while (q < end) {
			const char *pack = strstr (q, "<packing>");
			const char *pack_end;
			gchar *block;
			gchar *name;
			gchar *title;

			if (pack == NULL || pack >= end)
				break;
			pack_end = strstr (pack, "</packing>");
			if (pack_end == NULL || pack_end >= end)
				break;
			pack_end += strlen ("</packing>");
			block = g_strndup (pack, (gsize) (pack_end - pack));
			name = xml_prop_in_block (block, "name");
			title = xml_prop_in_block (block, "title");
			if (name && title) {
				VerneStackPageInfo *page = g_new0 (VerneStackPageInfo, 1);
				gchar *icon = xml_prop_in_block (block, "icon-name");
				page->name = name;
				page->title = title;
				if (icon && g_str_has_prefix (icon, "xsi-")) {
					page->icon = g_strdup (icon + 4);
					g_free (icon);
				} else {
					page->icon = icon;
				}
				g_ptr_array_add (info->pages, page);
			} else {
				g_free (name);
				g_free (title);
			}
			g_free (block);
			q = pack_end;
		}

		if (info->pages->len > 0)
			g_ptr_array_add (stacks, info);
		else
			stack_info_free (info);

		p = end;
	}

	return stacks;
}

static void
verne_restore_builder_stacks (GtkBuilder *builder, const gchar *xml)
{
	GPtrArray *stacks;
	GSList *objects, *l;
	guint i;

	stacks = collect_stacks_from_xml (xml);
	for (i = 0; i < stacks->len; i++) {
		VerneStackInfo *info = g_ptr_array_index (stacks, i);
		GObject *obj;
		GtkStack *stack;
		GtkSelectionModel *model;
		guint n, j;

		if (info->stack_id == NULL)
			continue;
		obj = gtk_builder_get_object (builder, info->stack_id);
		if (!GTK_IS_STACK (obj)) {
			g_warning ("Verne: UI stack id '%s' was not constructed", info->stack_id);
			continue;
		}
		stack = GTK_STACK (obj);
		gtk_widget_set_hexpand (GTK_WIDGET (stack), TRUE);
		gtk_widget_set_vexpand (GTK_WIDGET (stack), TRUE);
		model = gtk_stack_get_pages (stack);
		n = g_list_model_get_n_items (G_LIST_MODEL (model));
		g_message ("Verne: GtkStack '%s' has %u children, %u titled pages in XML",
			   info->stack_id, n, info->pages->len);
		for (j = 0; j < n && j < info->pages->len; j++) {
			VerneStackPageInfo *pg = g_ptr_array_index (info->pages, j);
			GtkStackPage *page = g_list_model_get_item (G_LIST_MODEL (model), j);
			if (page == NULL)
				continue;
			if (pg->name)
				gtk_stack_page_set_name (page, pg->name);
			if (pg->title)
				gtk_stack_page_set_title (page, pg->title);
			if (pg->icon)
				gtk_stack_page_set_icon_name (page, pg->icon);
			g_object_unref (page);
		}
	}

	objects = gtk_builder_get_objects (builder);
	for (l = objects; l != NULL; l = l->next) {
		GtkStack *st;

		if (!GTK_IS_STACK_SIDEBAR (l->data))
			continue;
		st = gtk_stack_sidebar_get_stack (GTK_STACK_SIDEBAR (l->data));
		g_message ("Verne: GtkStackSidebar widget, stack=%p", (void *) st);
		gtk_widget_set_vexpand (GTK_WIDGET (l->data), TRUE);
		gtk_widget_set_hexpand (GTK_WIDGET (l->data), FALSE);
		gtk_widget_set_visible (GTK_WIDGET (l->data), TRUE);
		if (st == NULL && stacks->len > 0) {
			VerneStackInfo *info0 = g_ptr_array_index (stacks, 0);
			GObject *fallback;

			if (info0->stack_id) {
				fallback = gtk_builder_get_object (builder, info0->stack_id);
				if (GTK_IS_STACK (fallback))
					st = GTK_STACK (fallback);
			}
		}
		if (st != NULL)
			gtk_stack_sidebar_set_stack (GTK_STACK_SIDEBAR (l->data), st);
	}
	g_slist_free (objects);
	g_ptr_array_unref (stacks);
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

static void
verne_bind_action_widgets (GtkBuilder *builder, const gchar *xml)
{
	const char *p = xml;

	while ((p = strstr (p, "<action-widget")) != NULL) {
		const char *resp;
		const char *gt;
		const char *id_end;
		gchar *id;
		gint response;
		GObject *obj;

		resp = strstr (p, "response=\"");
		gt = strchr (p, '>');
		if (resp == NULL || gt == NULL || resp > gt) {
			p++;
			continue;
		}
		response = atoi (resp + 10);
		id_end = strstr (gt + 1, "</action-widget>");
		if (id_end == NULL)
			break;
		id = g_strndup (gt + 1, (gsize) (id_end - (gt + 1)));
		g_strstrip (id);
		obj = gtk_builder_get_object (builder, id);
		if (GTK_IS_WIDGET (obj)) {
			GtkWidget *button = GTK_WIDGET (obj);
			GtkWidget *dialog = GTK_WIDGET (gtk_widget_get_root (button));
			if (!GTK_IS_DIALOG (dialog))
				dialog = gtk_widget_get_ancestor (button, GTK_TYPE_DIALOG);
			if (GTK_IS_DIALOG (dialog)) {
				GtkWidget *parent = gtk_widget_get_parent (button);
				g_object_ref (button);
				if (parent)
					gtk_widget_unparent (button);
				gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, response);
				g_object_unref (button);
			}
		}
		g_free (id);
		p = id_end + 1;
	}
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
	replace_all (s, "class=\"GtkInfoBar\"", "class=\"GtkBox\"");

	replace_all (s, " name=\"can-focus\"", " name=\"focusable\"");
	replace_all (s, " name=\"margin-left\"", " name=\"margin-start\"");
	replace_all (s, " name=\"margin-right\"", " name=\"margin-end\"");
	replace_all (s, " name=\"left-padding\"", " name=\"margin-start\"");
	replace_all (s, " name=\"right-padding\"", " name=\"margin-end\"");
	replace_all (s, " name=\"top-padding\"", " name=\"margin-top\"");
	replace_all (s, " name=\"bottom-padding\"", " name=\"margin-bottom\"");
	replace_all (s, "xsi-", "");

	replace_all (s, " internal-child=\"vbox\"", " internal-child=\"content_area\"");
	replace_all (s, " internal-child=\"action_area\"", "");

	rewrite_stock_labels (s);
	convert_numeric_align (s, "xalign", "halign");
	convert_numeric_align (s, "yalign", "valign");
	convert_border_width (s);
	rewrite_packing_blocks (s);
	strip_xml_block (s, "<action-widgets>", "</action-widgets>");
	strip_xml_block (s, "<accel-groups>", "</accel-groups>");
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
	remove_property (s, "xpad");
	remove_property (s, "ypad");
	remove_property (s, "n-rows");
	remove_property (s, "n-columns");
	remove_property (s, "draw-indicator");

	strip_empty_child_tags (s);
	return g_string_free (s, FALSE);
}

#define VERNE_BUILDER_CALLBACKS_KEY "verne-builder-callbacks"
#define VERNE_BUILDER_SIGNALS_KEY "verne-builder-signals"
#define VERNE_BUILDER_CONNECTED_KEY "verne-builder-signals-connected"

typedef struct {
	gchar *object_id;
	gchar *name;
	gchar *handler;
	gchar *connect_object;
	gboolean swapped;
	gboolean after;
} VerneUiSignal;

static void
verne_ui_signal_free (gpointer data)
{
	VerneUiSignal *sig = data;

	g_free (sig->object_id);
	g_free (sig->name);
	g_free (sig->handler);
	g_free (sig->connect_object);
	g_free (sig);
}

static gchar *
xml_attr_value (const char *tag, const char *gt, const char *attr)
{
	gchar *pat;
	const char *p, *end, *q1;
	gsize alen;

	pat = g_strdup_printf ("%s=\"", attr);
	alen = strlen (pat);
	p = tag;
	while (p < gt) {
		p = strstr (p, pat);
		if (p == NULL || p >= gt) {
			g_free (pat);
			return NULL;
		}
		q1 = p + alen;
		end = memchr (q1, '"', (gsize) (gt - q1));
		g_free (pat);
		if (end == NULL)
			return NULL;
		return g_strndup (q1, (gsize) (end - q1));
	}
	g_free (pat);
	return NULL;
}

static void
verne_collect_builder_signals (GtkBuilder *builder, const char *xml)
{
	GPtrArray *id_stack;
	GPtrArray *signals;
	const char *p;

	if (xml == NULL)
		return;

	id_stack = g_ptr_array_new_with_free_func (g_free);
	signals = g_ptr_array_new_with_free_func (verne_ui_signal_free);
	p = xml;
	while (*p) {
		if (strncmp (p, "<object", 7) == 0 &&
		    (p[7] == ' ' || p[7] == '\t' || p[7] == '\n' || p[7] == '\r')) {
			const char *gt = strchr (p, '>');
			gchar *id;

			if (gt == NULL)
				break;
			id = xml_attr_value (p, gt, "id");
			if (gt > p && *(gt - 1) == '/') {
				g_free (id);
			} else {
				g_ptr_array_add (id_stack, id ? id : g_strdup (""));
			}
			p = gt + 1;
		} else if (strncmp (p, "</object>", 9) == 0) {
			if (id_stack->len > 0)
				g_ptr_array_remove_index (id_stack, id_stack->len - 1);
			p += 9;
		} else if (strncmp (p, "<signal", 7) == 0 &&
			   (p[7] == ' ' || p[7] == '\t' || p[7] == '\n' || p[7] == '\r')) {
			const char *gt = strchr (p, '>');
			VerneUiSignal *sig;
			gchar *swapped, *after;
			const gchar *cur_id = "";

			if (gt == NULL)
				break;
			if (id_stack->len > 0)
				cur_id = g_ptr_array_index (id_stack, id_stack->len - 1);
			sig = g_new0 (VerneUiSignal, 1);
			sig->object_id = g_strdup (cur_id);
			sig->name = xml_attr_value (p, gt, "name");
			sig->handler = xml_attr_value (p, gt, "handler");
			sig->connect_object = xml_attr_value (p, gt, "object");
			swapped = xml_attr_value (p, gt, "swapped");
			after = xml_attr_value (p, gt, "after");
			sig->swapped = swapped && g_strcmp0 (swapped, "no") != 0 &&
				       g_strcmp0 (swapped, "FALSE") != 0 &&
				       g_strcmp0 (swapped, "false") != 0;
			sig->after = after && g_strcmp0 (after, "no") != 0 &&
				     g_strcmp0 (after, "FALSE") != 0 &&
				     g_strcmp0 (after, "false") != 0;
			g_free (swapped);
			g_free (after);
			if (sig->object_id[0] != '\0' && sig->name && sig->handler)
				g_ptr_array_add (signals, sig);
			else
				verne_ui_signal_free (sig);
			p = gt + 1;
		} else {
			p++;
		}
	}
	g_ptr_array_unref (id_stack);
	g_object_set_data_full (G_OBJECT (builder), VERNE_BUILDER_SIGNALS_KEY,
				signals, (GDestroyNotify) g_ptr_array_unref);
}

static GHashTable *
verne_builder_callbacks (GtkBuilder *builder)
{
	GHashTable *table;

	table = g_object_get_data (G_OBJECT (builder), VERNE_BUILDER_CALLBACKS_KEY);
	if (table == NULL) {
		table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		g_object_set_data_full (G_OBJECT (builder), VERNE_BUILDER_CALLBACKS_KEY,
					table, (GDestroyNotify) g_hash_table_unref);
	}
	return table;
}

void
gtk_builder_add_callback_symbols (GtkBuilder *builder, const char *first, ...)
{
	va_list args;
	const char *name;
	GtkBuilderScope *scope;
	GtkBuilderCScope *cscope = NULL;

	if (builder == NULL || first == NULL)
		return;

	scope = gtk_builder_get_scope (builder);
	if (GTK_IS_BUILDER_CSCOPE (scope))
		cscope = GTK_BUILDER_CSCOPE (scope);
	else {
		scope = gtk_builder_cscope_new ();
		gtk_builder_set_scope (builder, scope);
		cscope = GTK_BUILDER_CSCOPE (scope);
		g_object_unref (scope);
	}

	va_start (args, first);
	name = first;
	while (name != NULL) {
		GCallback cb = va_arg (args, GCallback);

		g_hash_table_insert (verne_builder_callbacks (builder), g_strdup (name), (gpointer) cb);
		if (cscope && cb)
			gtk_builder_cscope_add_callback_symbol (cscope, name, cb);
		name = va_arg (args, const char *);
	}
	va_end (args);
}

void
gtk_builder_connect_signals (GtkBuilder *builder, gpointer user_data)
{
	GPtrArray *signals;
	GHashTable *cbs;
	guint i;

	if (builder == NULL)
		return;
	if (g_object_get_data (G_OBJECT (builder), VERNE_BUILDER_CONNECTED_KEY))
		return;

	signals = g_object_get_data (G_OBJECT (builder), VERNE_BUILDER_SIGNALS_KEY);
	cbs = g_object_get_data (G_OBJECT (builder), VERNE_BUILDER_CALLBACKS_KEY);
	if (signals == NULL)
		return;

	gtk_builder_set_current_object (builder, (user_data && G_IS_OBJECT (user_data))
						 ? G_OBJECT (user_data) : NULL);

	for (i = 0; i < signals->len; i++) {
		VerneUiSignal *sig = g_ptr_array_index (signals, i);
		GObject *obj;
		GCallback cb = NULL;
		gpointer data = user_data;
		gulong id;

		obj = gtk_builder_get_object (builder, sig->object_id);
		if (cbs)
			cb = g_hash_table_lookup (cbs, sig->handler);
		if (cb == NULL) {
			GtkBuilderScope *scope = gtk_builder_get_scope (builder);

			if (GTK_IS_BUILDER_CSCOPE (scope))
				cb = gtk_builder_cscope_lookup_callback_symbol (GTK_BUILDER_CSCOPE (scope),
									       sig->handler);
		}
		if (obj == NULL || cb == NULL) {
			g_warning ("Verne: could not connect %s::%s to %s",
				   sig->object_id, sig->name, sig->handler);
			continue;
		}
		if (sig->connect_object) {
			GObject *other = gtk_builder_get_object (builder, sig->connect_object);

			if (other)
				data = other;
		}
		if (sig->swapped)
			id = g_signal_connect_data (obj, sig->name, cb, data, NULL, G_CONNECT_SWAPPED);
		else if (sig->after)
			id = g_signal_connect_data (obj, sig->name, cb, data, NULL, G_CONNECT_AFTER);
		else
			id = g_signal_connect (obj, sig->name, cb, data);
		(void) id;
	}

	g_object_set_data (G_OBJECT (builder), VERNE_BUILDER_CONNECTED_KEY, GINT_TO_POINTER (TRUE));
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
		g_file_set_contents ("/tmp/verne-transformed.ui", transformed, -1, NULL);
		g_clear_error (&local);
		ok = gtk_builder_add_from_string (builder, buffer, length, error);
	} else if (error) {
		*error = NULL;
	}
	if (ok) {
		verne_bind_action_widgets (builder, buffer);
		verne_restore_builder_stacks (builder, buffer);
		verne_collect_builder_signals (builder, buffer);
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
