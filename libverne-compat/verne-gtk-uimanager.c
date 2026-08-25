#include "config.h"
#include "verne-gtk-compat.h"

typedef struct {
	gchar *name;
	gchar *action;
	GtkUIManagerItemType type;
	guint merge_id;
	GList *children; /* UiNode */
	GtkWidget *widget;
} UiNode;

struct _GtkUIManager {
	GObject parent;
	GList *action_groups; /* GtkActionGroup */
	UiNode *root;
	guint next_merge;
	GtkAccelGroup *accel;
	GHashTable *widgets; /* path -> widget */
};

G_DEFINE_TYPE (GtkUIManager, gtk_ui_manager, G_TYPE_OBJECT)

static UiNode *
ui_node_new (const gchar *name, const gchar *action, GtkUIManagerItemType type, guint merge_id)
{
	UiNode *n = g_new0 (UiNode, 1);
	n->name = g_strdup (name);
	n->action = g_strdup (action);
	n->type = type;
	n->merge_id = merge_id;
	return n;
}

static void
ui_node_free (UiNode *n)
{
	g_list_free_full (n->children, (GDestroyNotify) ui_node_free);
	g_free (n->name);
	g_free (n->action);
	g_free (n);
}

static UiNode *
ui_node_find (UiNode *node, const gchar *name)
{
	GList *l;
	if (node->name && g_strcmp0 (node->name, name) == 0)
		return node;
	for (l = node->children; l; l = l->next) {
		UiNode *f = ui_node_find (l->data, name);
		if (f)
			return f;
	}
	return NULL;
}

static UiNode *
ui_node_find_path (UiNode *root, const gchar *path)
{
	gchar **parts;
	UiNode *cur = root;
	int i;
	if (path == NULL || path[0] == '\0')
		return root;
	parts = g_strsplit (path[0] == '/' ? path + 1 : path, "/", -1);
	for (i = 0; parts[i]; i++) {
		GList *l;
		UiNode *next = NULL;
		if (parts[i][0] == '\0')
			continue;
		for (l = cur->children; l; l = l->next) {
			UiNode *c = l->data;
			if (g_strcmp0 (c->name, parts[i]) == 0) {
				next = c;
				break;
			}
		}
		if (next == NULL) {
			g_strfreev (parts);
			return NULL;
		}
		cur = next;
	}
	g_strfreev (parts);
	return cur;
}

static GtkAction *
lookup_action (GtkUIManager *self, const gchar *name)
{
	GList *l;
	if (name == NULL)
		return NULL;
	for (l = self->action_groups; l; l = l->next) {
		GtkAction *a = gtk_action_group_get_action (l->data, name);
		if (a)
			return a;
	}
	return NULL;
}

static void
on_item_activate (GtkButton *button, gpointer data)
{
	GtkAction *action = data;
	(void) button;
	if (action)
		gtk_action_activate (action);
}

static GtkWidget *
build_menu_item_for_action (GtkUIManager *self, UiNode *node)
{
	GtkAction *action = lookup_action (self, node->action ? node->action : node->name);
	const gchar *label = action ? gtk_action_get_label (action) : (node->name ? node->name : "");
	GtkWidget *item;

	if (node->type == GTK_UI_MANAGER_SEPARATOR)
		return gtk_separator_menu_item_new ();

	if (action && GTK_IS_TOGGLE_ACTION (action)) {
		item = gtk_check_menu_item_new_with_mnemonic (label ? label : "");
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
						gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
		g_signal_connect_swapped (item, "toggled", G_CALLBACK (gtk_action_activate), action);
	} else {
		item = gtk_menu_item_new_with_mnemonic (label ? label : "");
		if (action)
			g_signal_connect (item, "clicked", G_CALLBACK (on_item_activate), action);
	}
	if (action && !gtk_action_get_visible (action))
		gtk_widget_set_visible (item, FALSE);
	if (action && !gtk_action_get_sensitive (action))
		gtk_widget_set_sensitive (item, FALSE);
	return item;
}

static GtkWidget *
build_menu (GtkUIManager *self, UiNode *node, gboolean menubar)
{
	GtkWidget *shell;
	GList *l;

	if (menubar)
		shell = gtk_menu_bar_new ();
	else
		shell = gtk_menu_new ();

	for (l = node->children; l; l = l->next) {
		UiNode *c = l->data;
		GtkWidget *child;

		if (c->type == GTK_UI_MANAGER_MENU || (c->children && c->type != GTK_UI_MANAGER_SEPARATOR)) {
			GtkWidget *submenu = build_menu (self, c, FALSE);
			GtkAction *action = lookup_action (self, c->action ? c->action : c->name);
			const gchar *label = action ? gtk_action_get_label (action) : c->name;
			if (menubar) {
				GtkWidget *btn = gtk_menu_button_new ();
				gtk_menu_button_set_label (GTK_MENU_BUTTON (btn), label ? label : "");
				gtk_menu_button_set_popover (GTK_MENU_BUTTON (btn), submenu);
				gtk_box_append (GTK_BOX (shell), btn);
				c->widget = btn;
				continue;
			} else {
				child = gtk_menu_item_new_with_mnemonic (label ? label : "");
				gtk_menu_item_set_submenu (GTK_MENU_ITEM (child), submenu);
			}
		} else if (c->type == GTK_UI_MANAGER_PLACEHOLDER) {
			GtkWidget *sub = build_menu (self, c, menubar);
			/* flatten placeholder children */
			if (menubar) {
				GtkWidget *ch = gtk_widget_get_first_child (sub);
				while (ch) {
					GtkWidget *next = gtk_widget_get_next_sibling (ch);
					g_object_ref (ch);
					gtk_container_remove (sub, ch);
					gtk_box_append (GTK_BOX (shell), ch);
					g_object_unref (ch);
					ch = next;
				}
			} else {
				/* append items from submenu box */
				GtkMenu *sm = GTK_MENU (sub);
				GtkWidget *box = gtk_menu_get_box (sm);
				GtkWidget *ch = gtk_widget_get_first_child (box);
				while (ch) {
					GtkWidget *next = gtk_widget_get_next_sibling (ch);
					g_object_ref (ch);
					gtk_box_remove (GTK_BOX (box), ch);
					gtk_box_append (GTK_BOX (gtk_menu_get_box (GTK_MENU (shell))), ch);
					g_object_unref (ch);
					ch = next;
				}
			}
			c->widget = sub;
			continue;
		} else {
			child = build_menu_item_for_action (self, c);
		}
		gtk_menu_shell_append (shell, child);
		c->widget = child;
	}
	node->widget = shell;
	return shell;
}

static void
rebuild (GtkUIManager *self)
{
	GList *l;
	if (self->widgets)
		g_hash_table_remove_all (self->widgets);
	else
		self->widgets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (l = self->root->children; l; l = l->next) {
		UiNode *n = l->data;
		GtkWidget *w = NULL;
		gchar *path;
		if (n->type == GTK_UI_MANAGER_MENUBAR || g_strcmp0 (n->name, "MenuBar") == 0)
			w = build_menu (self, n, TRUE);
		else
			w = build_menu (self, n, FALSE);
		path = g_strdup_printf ("/%s", n->name ? n->name : "ui");
		g_hash_table_insert (self->widgets, path, w);
		n->widget = w;
	}
}

static void
gtk_ui_manager_finalize (GObject *object)
{
	GtkUIManager *self = GTK_UI_MANAGER (object);
	g_list_free_full (self->action_groups, g_object_unref);
	if (self->root)
		ui_node_free (self->root);
	g_clear_object (&self->accel);
	g_clear_pointer (&self->widgets, g_hash_table_destroy);
	G_OBJECT_CLASS (gtk_ui_manager_parent_class)->finalize (object);
}

static void
gtk_ui_manager_class_init (GtkUIManagerClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = gtk_ui_manager_finalize;
	g_signal_new ("connect-proxy", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
		      0, NULL, NULL, NULL, G_TYPE_NONE, 2, GTK_TYPE_ACTION, GTK_TYPE_WIDGET);
	g_signal_new ("disconnect-proxy", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
		      0, NULL, NULL, NULL, G_TYPE_NONE, 2, GTK_TYPE_ACTION, GTK_TYPE_WIDGET);
	g_signal_new ("add-widget", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
		      0, NULL, NULL, NULL, G_TYPE_NONE, 1, GTK_TYPE_WIDGET);
	g_signal_new ("actions-changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
		      0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gtk_ui_manager_init (GtkUIManager *self)
{
	self->root = ui_node_new ("ui", NULL, GTK_UI_MANAGER_AUTO, 0);
	self->next_merge = 1;
	self->accel = gtk_accel_group_new ();
}

GtkUIManager *
gtk_ui_manager_new (void)
{
	return g_object_new (GTK_TYPE_UI_MANAGER, NULL);
}

void
gtk_ui_manager_insert_action_group (GtkUIManager *self, GtkActionGroup *group, gint pos)
{
	self->action_groups = g_list_insert (self->action_groups, g_object_ref (group), pos < 0 ? -1 : pos);
}

void
gtk_ui_manager_remove_action_group (GtkUIManager *self, GtkActionGroup *group)
{
	self->action_groups = g_list_remove (self->action_groups, group);
	g_object_unref (group);
}

GList *
gtk_ui_manager_get_action_groups (GtkUIManager *self)
{
	return self->action_groups;
}

typedef struct {
	GList *stack; /* UiNode */
	GtkUIManager *self;
	guint merge_id;
} ParseCtx;

static GtkUIManagerItemType
tag_type (const gchar *el)
{
	if (g_strcmp0 (el, "menubar") == 0) return GTK_UI_MANAGER_MENUBAR;
	if (g_strcmp0 (el, "menu") == 0) return GTK_UI_MANAGER_MENU;
	if (g_strcmp0 (el, "toolbar") == 0) return GTK_UI_MANAGER_TOOLBAR;
	if (g_strcmp0 (el, "popup") == 0) return GTK_UI_MANAGER_POPUP;
	if (g_strcmp0 (el, "menuitem") == 0) return GTK_UI_MANAGER_MENUITEM;
	if (g_strcmp0 (el, "toolitem") == 0) return GTK_UI_MANAGER_TOOLITEM;
	if (g_strcmp0 (el, "separator") == 0) return GTK_UI_MANAGER_SEPARATOR;
	if (g_strcmp0 (el, "placeholder") == 0) return GTK_UI_MANAGER_PLACEHOLDER;
	if (g_strcmp0 (el, "accelerator") == 0) return GTK_UI_MANAGER_ACCELERATOR;
	return GTK_UI_MANAGER_AUTO;
}

static void
parse_start (GMarkupParseContext *context, const gchar *el, const gchar **names, const gchar **values, gpointer data, GError **error)
{
	ParseCtx *ctx = data;
	UiNode *parent = ctx->stack->data;
	const gchar *name = NULL, *action = NULL;
	int i;
	UiNode *node;
	(void) context; (void) error;
	if (g_strcmp0 (el, "ui") == 0)
		return;
	for (i = 0; names[i]; i++) {
		if (g_strcmp0 (names[i], "name") == 0) name = values[i];
		if (g_strcmp0 (names[i], "action") == 0) action = values[i];
	}
	if (name == NULL)
		name = action;
	node = ui_node_new (name, action, tag_type (el), ctx->merge_id);
	parent->children = g_list_append (parent->children, node);
	ctx->stack = g_list_prepend (ctx->stack, node);
}

static void
parse_end (GMarkupParseContext *context, const gchar *el, gpointer data, GError **error)
{
	ParseCtx *ctx = data;
	(void) context; (void) error;
	if (g_strcmp0 (el, "ui") == 0)
		return;
	if (ctx->stack && ctx->stack->next)
		ctx->stack = g_list_delete_link (ctx->stack, ctx->stack);
}

static guint
parse_ui (GtkUIManager *self, const gchar *buffer, gssize length, GError **error)
{
	ParseCtx ctx;
	GMarkupParser parser = { parse_start, parse_end, NULL, NULL, NULL };
	GMarkupParseContext *pc;
	guint id = self->next_merge++;
	ctx.self = self;
	ctx.merge_id = id;
	ctx.stack = g_list_prepend (NULL, self->root);
	pc = g_markup_parse_context_new (&parser, 0, &ctx, NULL);
	if (length < 0)
		length = strlen (buffer);
	if (!g_markup_parse_context_parse (pc, buffer, length, error) ||
	    !g_markup_parse_context_end_parse (pc, error)) {
		g_markup_parse_context_free (pc);
		g_list_free (ctx.stack);
		return 0;
	}
	g_markup_parse_context_free (pc);
	g_list_free (ctx.stack);
	rebuild (self);
	return id;
}

guint
gtk_ui_manager_add_ui_from_string (GtkUIManager *self, const gchar *buffer, gssize length, GError **error)
{
	return parse_ui (self, buffer, length, error);
}

guint
gtk_ui_manager_add_ui_from_resource (GtkUIManager *self, const gchar *path, GError **error)
{
	GBytes *bytes = g_resources_lookup_data (path, 0, error);
	guint id;
	gsize size;
	const gchar *data;
	if (bytes == NULL)
		return 0;
	data = g_bytes_get_data (bytes, &size);
	id = parse_ui (self, data, (gssize) size, error);
	g_bytes_unref (bytes);
	return id;
}

guint
gtk_ui_manager_add_ui_from_file (GtkUIManager *self, const gchar *filename, GError **error)
{
	gchar *buf = NULL;
	gsize len = 0;
	guint id;
	if (!g_file_get_contents (filename, &buf, &len, error))
		return 0;
	id = parse_ui (self, buf, (gssize) len, error);
	g_free (buf);
	return id;
}

void
gtk_ui_manager_add_ui (GtkUIManager *self, guint merge_id, const gchar *path, const gchar *name, const gchar *action, GtkUIManagerItemType type, gboolean top)
{
	UiNode *parent = ui_node_find_path (self->root, path);
	UiNode *node;
	if (parent == NULL)
		parent = self->root;
	node = ui_node_new (name, action, type, merge_id);
	if (top)
		parent->children = g_list_prepend (parent->children, node);
	else
		parent->children = g_list_append (parent->children, node);
	rebuild (self);
}

static void
remove_merge (UiNode *node, guint merge_id)
{
	GList *l = node->children;
	while (l) {
		GList *next = l->next;
		UiNode *c = l->data;
		remove_merge (c, merge_id);
		if (c->merge_id == merge_id) {
			node->children = g_list_delete_link (node->children, l);
			ui_node_free (c);
		}
		l = next;
	}
}

void
gtk_ui_manager_remove_ui (GtkUIManager *self, guint merge_id)
{
	remove_merge (self->root, merge_id);
	rebuild (self);
}

guint
gtk_ui_manager_new_merge_id (GtkUIManager *self)
{
	return self->next_merge++;
}

GtkWidget *
gtk_ui_manager_get_widget (GtkUIManager *self, const gchar *path)
{
	UiNode *n;
	if (self->widgets) {
		GtkWidget *w = g_hash_table_lookup (self->widgets, path);
		if (w)
			return w;
	}
	n = ui_node_find_path (self->root, path);
	return n ? n->widget : NULL;
}

GtkAction *
gtk_ui_manager_get_action (GtkUIManager *self, const gchar *path)
{
	UiNode *n = ui_node_find_path (self->root, path);
	if (n == NULL)
		return NULL;
	return lookup_action (self, n->action ? n->action : n->name);
}

gpointer
gtk_ui_manager_get_accel_group (GtkUIManager *self)
{
	return self->accel;
}

void
gtk_ui_manager_ensure_update (GtkUIManager *self)
{
	rebuild (self);
}

gchar *
gtk_ui_manager_get_ui (GtkUIManager *self)
{
	(void) self;
	return g_strdup ("<ui/>");
}

void
gtk_ui_manager_set_add_tearoffs (GtkUIManager *self, gboolean add)
{
	(void) self;
	(void) add;
}
