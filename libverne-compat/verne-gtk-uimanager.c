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
	guint update_idle;
	guint dirty : 1;
	guint rebuilding : 1;
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
	if (GTK_IS_ACTION (action))
		gtk_action_activate (action);
	if (GTK_IS_CHECK_MENU_ITEM (button) && GTK_IS_TOGGLE_ACTION (action))
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (button),
						gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void
on_toggle_action_notify_active (GtkAction *action, GParamSpec *pspec, gpointer item)
{
	(void) pspec;
	if (GTK_IS_TOGGLE_ACTION (action) && GTK_IS_CHECK_MENU_ITEM (item))
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
						gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void
on_action_notify_visible (GtkAction *action, GParamSpec *pspec, gpointer item)
{
	gboolean visible;

	(void) pspec;
	if (!GTK_IS_WIDGET (item))
		return;
	visible = gtk_action_get_visible (action);
	gtk_widget_set_no_show_all (item, !visible);
	gtk_widget_set_visible (item, visible);
}

static void
on_action_notify_sensitive (GtkAction *action, GParamSpec *pspec, gpointer item)
{
	(void) pspec;
	if (GTK_IS_WIDGET (item))
		gtk_widget_set_sensitive (item, gtk_action_get_sensitive (action));
}

static void
on_action_notify_label (GtkAction *action, GParamSpec *pspec, gpointer item)
{
	const gchar *label;

	(void) pspec;
	if (!GTK_IS_MENU_ITEM (item))
		return;
	label = gtk_action_get_label (action);
	gtk_menu_item_set_label (GTK_MENU_ITEM (item), label ? label : "");
	{
		const gchar *accel_path = gtk_action_get_accel_path (action);
		if (accel_path)
			gtk_menu_item_set_accel_path (GTK_MENU_ITEM (item), accel_path);
	}
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
		g_signal_connect (item, "clicked", G_CALLBACK (on_item_activate), action);
		g_object_set_data (G_OBJECT (item), "verne-action-clicked", GINT_TO_POINTER (1));
		g_signal_connect_object (action, "notify::active",
					 G_CALLBACK (on_toggle_action_notify_active), item, 0);
	} else {
		item = gtk_menu_item_new_with_mnemonic (label ? label : "");
		if (action) {
			g_signal_connect (item, "clicked", G_CALLBACK (on_item_activate), action);
			g_object_set_data (G_OBJECT (item), "verne-action-clicked", GINT_TO_POINTER (1));
		}
	}
	if (action && !gtk_action_get_visible (action)) {
		gtk_widget_set_no_show_all (item, TRUE);
		gtk_widget_set_visible (item, FALSE);
	}
	if (action && !gtk_action_get_sensitive (action))
		gtk_widget_set_sensitive (item, FALSE);
	if (action) {
		g_signal_connect_object (action, "notify::visible",
					 G_CALLBACK (on_action_notify_visible), item, 0);
		g_signal_connect_object (action, "notify::sensitive",
					 G_CALLBACK (on_action_notify_sensitive), item, 0);
		g_signal_connect_object (action, "notify::label",
					 G_CALLBACK (on_action_notify_label), item, 0);
		g_object_set_data (G_OBJECT (item), "verne-action", action);
		{
			const gchar *accel_path = gtk_action_get_accel_path (action);
			if (accel_path)
				gtk_menu_item_set_accel_path (GTK_MENU_ITEM (item), accel_path);
		}
	}
	return item;
}

static void build_menu_add_node (GtkUIManager *self, UiNode *c, GtkWidget *shell, gboolean menubar, const gchar *path);

static void
clear_shell_children (GtkWidget *shell)
{
	GtkWidget *box = shell;
	GtkWidget *child;

	if (GTK_IS_MENU (shell))
		box = gtk_menu_get_box (GTK_MENU (shell));
	if (!GTK_IS_BOX (box))
		return;
	while ((child = gtk_widget_get_first_child (box)) != NULL)
		gtk_box_remove (GTK_BOX (box), child);
}

static void
harvest_submenus (UiNode *node)
{
	GList *l;

	if (GTK_IS_MENU_ITEM (node->widget)) {
		GtkWidget *sub = gtk_menu_item_get_submenu (GTK_MENU_ITEM (node->widget));
		if (GTK_IS_MENU (sub)) {
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (node->widget), NULL);
			gtk_widget_set_visible (sub, FALSE);
			node->widget = sub;
		}
	} else if (GTK_IS_MENU (node->widget)) {
		gtk_widget_set_visible (node->widget, FALSE);
	}

	for (l = node->children; l; l = l->next)
		harvest_submenus (l->data);
}

static void
register_widget (GtkUIManager *self, const gchar *path, GtkWidget *w)
{
	if (self->widgets && path && w)
		g_hash_table_insert (self->widgets, g_strdup (path), w);
}

static GtkWidget *
build_menu (GtkUIManager *self, UiNode *node, gboolean menubar, const gchar *path)
{
	GtkWidget *shell;
	GList *l;

	if (menubar) {
		if (GTK_IS_MENU_BAR (node->widget))
			shell = node->widget;
		else
			shell = gtk_menu_bar_new ();
		clear_shell_children (shell);
	} else {
		if (GTK_IS_MENU (node->widget))
			shell = node->widget;
		else
			shell = gtk_menu_new ();
		gtk_widget_set_visible (shell, FALSE);
		clear_shell_children (shell);
	}

	register_widget (self, path, shell);
	for (l = node->children; l; l = l->next) {
		UiNode *c = l->data;
		gchar *child_path = g_strdup_printf ("%s/%s", path, c->name ? c->name : "item");
		build_menu_add_node (self, c, shell, menubar, child_path);
		g_free (child_path);
	}
	node->widget = shell;
	return shell;
}

static void
build_menu_add_node (GtkUIManager *self, UiNode *c, GtkWidget *shell, gboolean menubar, const gchar *path)
{
	GtkWidget *child;

	if (c->type == GTK_UI_MANAGER_ACCELERATOR)
		return;
	if (c->type == GTK_UI_MANAGER_PLACEHOLDER) {
		GList *pl;
		for (pl = c->children; pl; pl = pl->next) {
			UiNode *n = pl->data;
			gchar *child_path = g_strdup_printf ("%s/%s", path, n->name ? n->name : "item");
			build_menu_add_node (self, n, shell, menubar, child_path);
			g_free (child_path);
		}
		c->widget = shell;
		register_widget (self, path, shell);
		return;
	}

	if (c->type == GTK_UI_MANAGER_MENU ||
	    (c->children && c->type != GTK_UI_MANAGER_SEPARATOR &&
	     c->type != GTK_UI_MANAGER_MENUITEM && c->type != GTK_UI_MANAGER_TOOLITEM)) {
		GtkWidget *submenu = build_menu (self, c, FALSE, path);
		GtkAction *action = lookup_action (self, c->action ? c->action : c->name);
		const gchar *label = action ? gtk_action_get_label (action) : c->name;
		child = gtk_menu_item_new_with_mnemonic (label ? label : "");
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (child), submenu);
		if (menubar)
			gtk_widget_add_css_class (child, "flat");
	} else {
		child = build_menu_item_for_action (self, c);
	}
	gtk_menu_shell_append (shell, child);
	c->widget = child;
	register_widget (self, path, child);
}

static void queue_update (GtkUIManager *self);
static gboolean do_updates_idle (gpointer data);

static gboolean
ui_node_menu_mapped (UiNode *node)
{
	GList *l;

	if (GTK_IS_MENU (node->widget) &&
	    gtk_widget_is_visible (node->widget) &&
	    gtk_widget_get_mapped (node->widget))
		return TRUE;
	for (l = node->children; l; l = l->next) {
		if (ui_node_menu_mapped (l->data))
			return TRUE;
	}
	return FALSE;
}

static void
rebuild (GtkUIManager *self)
{
	GList *l;

	if (self->rebuilding)
		return;
	/* File-menu show merges extension items, which queues a rebuild.
	 * Rebuilding while the overlay is mapped clear_shell_children's the
	 * open menu and leaves a blank popover. GTK3 updates in place;
	 * wait until every GtkMenu is hidden. */
	if (ui_node_menu_mapped (self->root)) {
		self->dirty = TRUE;
		if (self->update_idle == 0)
			self->update_idle = g_timeout_add (150, (GSourceFunc) do_updates_idle, self);
		return;
	}
	self->rebuilding = TRUE;
	self->dirty = FALSE;

	if (self->widgets == NULL)
		self->widgets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	harvest_submenus (self->root);
	g_hash_table_remove_all (self->widgets);

	for (l = self->root->children; l; l = l->next) {
		UiNode *n = l->data;
		GtkWidget *w = NULL;
		gchar *path;
		if (n->type == GTK_UI_MANAGER_ACCELERATOR ||
		    n->type == GTK_UI_MANAGER_TOOLBAR)
			continue;
		path = g_strdup_printf ("/%s", n->name ? n->name : "ui");
		if (n->type == GTK_UI_MANAGER_MENUBAR || g_strcmp0 (n->name, "MenuBar") == 0)
			w = build_menu (self, n, TRUE, path);
		else
			w = build_menu (self, n, FALSE, path);
		g_free (path);
		n->widget = w;
	}

	self->rebuilding = FALSE;
	/* GTK3 emits "actions-changed" when action groups change, not on
	 * every widget rebuild. Emitting here re-entered File-menu setup
	 * (nemo_window_connect_file_menu) and dest rebuilt /MenuBar/File
	 * every frame. */
	if (self->dirty)
		queue_update (self);
}

static gboolean
do_updates_idle (gpointer data)
{
	GtkUIManager *self = data;

	self->update_idle = 0;
	if (self->dirty)
		rebuild (self);
	return G_SOURCE_REMOVE;
}

static void
queue_update (GtkUIManager *self)
{
	self->dirty = TRUE;
	if (self->rebuilding)
		return;
	if (self->update_idle == 0)
		self->update_idle = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
						     do_updates_idle, self, NULL);
}

static void
gtk_ui_manager_finalize (GObject *object)
{
	GtkUIManager *self = GTK_UI_MANAGER (object);
	GList *groups = self->action_groups;

	if (self->update_idle != 0) {
		g_source_remove (self->update_idle);
		self->update_idle = 0;
	}
	self->action_groups = NULL;
	g_list_free_full (groups, g_object_unref);
	if (self->root)
		ui_node_free (self->root);
	self->root = NULL;
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
	if (self == NULL || group == NULL)
		return;
	self->action_groups = g_list_insert (self->action_groups, g_object_ref (group), pos < 0 ? -1 : pos);
	verne_action_group_bind_accels (group, self->accel);
	g_signal_emit_by_name (self, "actions-changed");
}

void
gtk_ui_manager_remove_action_group (GtkUIManager *self, GtkActionGroup *group)
{
	GList *link;

	if (self == NULL || group == NULL)
		return;
	link = g_list_find (self->action_groups, group);
	if (link == NULL)
		return;
	if (self->accel)
		verne_action_group_unbind_accels (group, self->accel);
	self->action_groups = g_list_delete_link (self->action_groups, link);
	g_object_unref (group);
	g_signal_emit_by_name (self, "actions-changed");
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
	/* Merge into an existing same-named sibling (GTK3 UI manager behavior). */
	{
		GList *l;
		UiNode *existing = NULL;
		GtkUIManagerItemType t = tag_type (el);
		if (name) {
			for (l = parent->children; l; l = l->next) {
				UiNode *c = l->data;
				if (g_strcmp0 (c->name, name) == 0) {
					existing = c;
					break;
				}
			}
		}
		if (existing && (existing->type == t ||
				 existing->type == GTK_UI_MANAGER_PLACEHOLDER ||
				 t == GTK_UI_MANAGER_PLACEHOLDER ||
				 existing->type == GTK_UI_MANAGER_MENU ||
				 existing->type == GTK_UI_MANAGER_POPUP ||
				 t == GTK_UI_MANAGER_MENU ||
				 t == GTK_UI_MANAGER_POPUP)) {
			ctx->stack = g_list_prepend (ctx->stack, existing);
			return;
		}
		node = ui_node_new (name, action, t, ctx->merge_id);
	}
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
	queue_update (self);
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
	queue_update (self);
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
	queue_update (self);
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
	gtk_ui_manager_ensure_update (self);
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
	UiNode *n;
	gtk_ui_manager_ensure_update (self);
	n = ui_node_find_path (self->root, path);
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
	if (self == NULL || self->rebuilding)
		return;
	if (self->update_idle != 0) {
		g_source_remove (self->update_idle);
		self->update_idle = 0;
	}
	if (self->dirty)
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
