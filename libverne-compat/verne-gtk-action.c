#include "config.h"
#include "verne-gtk-compat.h"
#include <string.h>

enum {
	ACTION_PROP_0,
	ACTION_PROP_NAME,
	ACTION_PROP_SENSITIVE,
	ACTION_PROP_VISIBLE,
	ACTION_PROP_LABEL,
	ACTION_PROP_TOOLTIP,
	ACTION_PROP_ICON_NAME,
	ACTION_PROP_STOCK_ID,
	ACTION_PROP_SHORT_LABEL,
	ACTION_PROP_IS_IMPORTANT,
	ACTION_PROP_GICON,
	ACTION_N_PROPS
};

enum {
	ACTION_ACTIVATE,
	ACTION_LAST
};

static guint action_signals[ACTION_LAST];

G_DEFINE_TYPE (GtkAction, gtk_action, G_TYPE_OBJECT)

static void
gtk_action_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GtkAction *action = GTK_ACTION (object);
	switch (prop_id) {
	case ACTION_PROP_NAME:
		if (action->name == NULL)
			action->name = g_value_dup_string (value);
		break;
	case ACTION_PROP_SENSITIVE:
		gtk_action_set_sensitive (action, g_value_get_boolean (value));
		break;
	case ACTION_PROP_VISIBLE:
		gtk_action_set_visible (action, g_value_get_boolean (value));
		break;
	case ACTION_PROP_LABEL:
		gtk_action_set_label (action, g_value_get_string (value));
		break;
	case ACTION_PROP_TOOLTIP:
		gtk_action_set_tooltip (action, g_value_get_string (value));
		break;
	case ACTION_PROP_ICON_NAME:
		gtk_action_set_icon_name (action, g_value_get_string (value));
		break;
	case ACTION_PROP_STOCK_ID:
		gtk_action_set_stock_id (action, g_value_get_string (value));
		break;
	case ACTION_PROP_SHORT_LABEL:
		gtk_action_set_short_label (action, g_value_get_string (value));
		break;
	case ACTION_PROP_IS_IMPORTANT:
		action->is_important = g_value_get_boolean (value);
		break;
	case ACTION_PROP_GICON:
		gtk_action_set_gicon (action, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_action_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GtkAction *action = GTK_ACTION (object);
	switch (prop_id) {
	case ACTION_PROP_NAME:
		g_value_set_string (value, action->name);
		break;
	case ACTION_PROP_SENSITIVE:
		g_value_set_boolean (value, action->sensitive);
		break;
	case ACTION_PROP_VISIBLE:
		g_value_set_boolean (value, action->visible);
		break;
	case ACTION_PROP_LABEL:
		g_value_set_string (value, action->label);
		break;
	case ACTION_PROP_TOOLTIP:
		g_value_set_string (value, action->tooltip);
		break;
	case ACTION_PROP_ICON_NAME:
		g_value_set_string (value, action->icon_name);
		break;
	case ACTION_PROP_STOCK_ID:
		g_value_set_string (value, action->stock_id);
		break;
	case ACTION_PROP_SHORT_LABEL:
		g_value_set_string (value, gtk_action_get_short_label (action));
		break;
	case ACTION_PROP_IS_IMPORTANT:
		g_value_set_boolean (value, action->is_important);
		break;
	case ACTION_PROP_GICON:
		g_value_set_object (value, action->gicon);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_action_finalize (GObject *object)
{
	GtkAction *action = GTK_ACTION (object);
	g_free (action->name);
	g_free (action->label);
	g_free (action->short_label);
	g_free (action->tooltip);
	g_free (action->stock_id);
	g_free (action->icon_name);
	g_free (action->accelerator);
	g_free (action->accel_path);
	g_clear_object (&action->gicon);
	G_OBJECT_CLASS (gtk_action_parent_class)->finalize (object);
}

static void
gtk_action_real_activate (GtkAction *action)
{
	(void) action;
}

static void
gtk_action_real_connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	(void) action;
	(void) proxy;
}

static void
gtk_action_class_init (GtkActionClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);
	oclass->finalize = gtk_action_finalize;
	oclass->set_property = gtk_action_set_property;
	oclass->get_property = gtk_action_get_property;
	klass->activate = gtk_action_real_activate;
	klass->connect_proxy = gtk_action_real_connect_proxy;
	klass->disconnect_proxy = gtk_action_real_connect_proxy;
	action_signals[ACTION_ACTIVATE] =
		g_signal_new ("activate", G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (GtkActionClass, activate),
			      NULL, NULL, NULL, G_TYPE_NONE, 0);
	g_object_class_install_property (oclass, ACTION_PROP_NAME,
		g_param_spec_string ("name", NULL, NULL, NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (oclass, ACTION_PROP_SENSITIVE,
		g_param_spec_boolean ("sensitive", NULL, NULL, TRUE, G_PARAM_READWRITE));
	g_object_class_install_property (oclass, ACTION_PROP_VISIBLE,
		g_param_spec_boolean ("visible", NULL, NULL, TRUE, G_PARAM_READWRITE));
	g_object_class_install_property (oclass, ACTION_PROP_LABEL,
		g_param_spec_string ("label", NULL, NULL, NULL, G_PARAM_READWRITE));
	g_object_class_install_property (oclass, ACTION_PROP_TOOLTIP,
		g_param_spec_string ("tooltip", NULL, NULL, NULL, G_PARAM_READWRITE));
	g_object_class_install_property (oclass, ACTION_PROP_ICON_NAME,
		g_param_spec_string ("icon-name", NULL, NULL, NULL, G_PARAM_READWRITE));
	g_object_class_install_property (oclass, ACTION_PROP_STOCK_ID,
		g_param_spec_string ("stock-id", NULL, NULL, NULL, G_PARAM_READWRITE));
	g_object_class_install_property (oclass, ACTION_PROP_SHORT_LABEL,
		g_param_spec_string ("short_label", NULL, NULL, NULL, G_PARAM_READWRITE));
	g_object_class_install_property (oclass, ACTION_PROP_IS_IMPORTANT,
		g_param_spec_boolean ("is-important", NULL, NULL, FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (oclass, ACTION_PROP_GICON,
		g_param_spec_object ("gicon", NULL, NULL, G_TYPE_ICON, G_PARAM_READWRITE));
}

static void
gtk_action_init (GtkAction *action)
{
	action->sensitive = TRUE;
	action->visible = TRUE;
	action->visible_horizontal = TRUE;
	action->visible_vertical = TRUE;
}

GtkAction *
gtk_action_new (const gchar *name, const gchar *label, const gchar *tooltip, const gchar *stock_id)
{
	GtkAction *action = g_object_new (GTK_TYPE_ACTION, NULL);
	action->name = g_strdup (name);
	action->label = g_strdup (label);
	action->tooltip = g_strdup (tooltip);
	action->stock_id = g_strdup (stock_id);
	return action;
}

void
gtk_action_activate (GtkAction *action)
{
	g_return_if_fail (GTK_IS_ACTION (action));
	g_signal_emit (action, action_signals[ACTION_ACTIVATE], 0);
}

const gchar *gtk_action_get_name (GtkAction *action) { return action ? action->name : NULL; }
void gtk_action_set_sensitive (GtkAction *action, gboolean sensitive) {
	if (!action || !GTK_IS_ACTION (action) || action->sensitive == sensitive) return;
	action->sensitive = sensitive;
	g_object_notify (G_OBJECT (action), "sensitive");
}
gboolean gtk_action_get_sensitive (GtkAction *action) { return action ? action->sensitive : FALSE; }
gboolean gtk_action_is_sensitive (GtkAction *action) { return gtk_action_get_sensitive (action); }
void gtk_action_set_visible (GtkAction *action, gboolean visible) {
	if (!action || !GTK_IS_ACTION (action) || action->visible == visible) return;
	action->visible = visible;
	g_object_notify (G_OBJECT (action), "visible");
}
gboolean gtk_action_get_visible (GtkAction *action) { return action ? action->visible : FALSE; }
void gtk_action_set_label (GtkAction *action, const gchar *label) {
	g_return_if_fail (GTK_IS_ACTION (action));
	g_free (action->label); action->label = g_strdup (label);
	g_object_notify (G_OBJECT (action), "label");
}
const gchar *gtk_action_get_label (GtkAction *action) { return action ? action->label : NULL; }
void gtk_action_set_short_label (GtkAction *action, const gchar *label) {
	g_return_if_fail (GTK_IS_ACTION (action));
	g_free (action->short_label); action->short_label = g_strdup (label);
	g_object_notify (G_OBJECT (action), "short_label");
}
const gchar *gtk_action_get_short_label (GtkAction *action) { return action && action->short_label ? action->short_label : gtk_action_get_label (action); }
void gtk_action_set_tooltip (GtkAction *action, const gchar *tooltip) {
	g_return_if_fail (GTK_IS_ACTION (action));
	g_free (action->tooltip); action->tooltip = g_strdup (tooltip);
	g_object_notify (G_OBJECT (action), "tooltip");
}
const gchar *gtk_action_get_tooltip (GtkAction *action) { return action ? action->tooltip : NULL; }
void gtk_action_set_icon_name (GtkAction *action, const gchar *icon_name) {
	g_return_if_fail (GTK_IS_ACTION (action));
	g_free (action->icon_name); action->icon_name = g_strdup (icon_name);
	g_object_notify (G_OBJECT (action), "icon-name");
}
const gchar *gtk_action_get_icon_name (GtkAction *action) { return action ? action->icon_name : NULL; }
void gtk_action_set_gicon (GtkAction *action, GIcon *icon) {
	if (action == NULL)
		return;
	g_clear_object (&action->gicon);
	if (icon)
		action->gicon = g_object_ref (icon);
	g_object_notify (G_OBJECT (action), "gicon");
}
GIcon *gtk_action_get_gicon (GtkAction *action) { return action ? action->gicon : NULL; }
void gtk_action_set_stock_id (GtkAction *action, const gchar *stock_id) {
	g_return_if_fail (GTK_IS_ACTION (action));
	g_free (action->stock_id); action->stock_id = g_strdup (stock_id);
}
const gchar *gtk_action_get_stock_id (GtkAction *action) { return action ? action->stock_id : NULL; }
void gtk_action_set_is_important (GtkAction *action, gboolean is_important) { if (action) action->is_important = is_important; }
gboolean gtk_action_get_is_important (GtkAction *action) { return action ? action->is_important : FALSE; }

void
gtk_action_set_accel_path (GtkAction *action, const gchar *accel_path)
{
	GtkAccelKey key;

	if (action == NULL)
		return;
	g_free (action->accel_path);
	action->accel_path = g_strdup (accel_path);
	if (accel_path && action->accelerator && action->accelerator[0]) {
		guint accel_key = 0;
		GdkModifierType accel_mods = 0;

		gtk_accelerator_parse (action->accelerator, &accel_key, &accel_mods);
		if (accel_key)
			gtk_accel_map_add_entry (accel_path, accel_key, accel_mods);
	}
	memset (&key, 0, sizeof key);
	if (accel_path && gtk_accel_map_lookup_entry (accel_path, &key) && key.accel_key) {
		gchar *name = gtk_accelerator_name (key.accel_key, key.accel_mods);
		g_free (action->accelerator);
		action->accelerator = name;
	}
}

const gchar *
gtk_action_get_accel_path (GtkAction *action)
{
	return action ? action->accel_path : NULL;
}

void
gtk_action_set_accel_group (GtkAction *action, gpointer accel_group)
{
	if (action)
		action->accel_group = accel_group;
}

void
gtk_action_connect_accelerator (GtkAction *action)
{
	if (action == NULL || action->accel_group == NULL || action->accelerator == NULL)
		return;
	verne_accel_group_connect_action (action->accel_group, action, action->accelerator);
}

void
gtk_action_disconnect_accelerator (GtkAction *action)
{
	if (action == NULL || action->accel_group == NULL)
		return;
	verne_accel_group_disconnect_action (action->accel_group, action);
}

void gtk_action_set_visible_horizontal (GtkAction *action, gboolean visible) { if (action) action->visible_horizontal = visible; }
GList *gtk_action_get_proxies (GtkAction *action) { (void) action; return NULL; }
void gtk_action_block_activate (GtkAction *action) { (void) action; }
void gtk_action_unblock_activate (GtkAction *action) { (void) action; }

/* Toggle */
enum { TOGGLE_TOGGLED, TOGGLE_LAST };
static guint toggle_signals[TOGGLE_LAST];
enum { TOGGLE_PROP_ACTIVE = 1 };

G_DEFINE_TYPE (GtkToggleAction, gtk_toggle_action, GTK_TYPE_ACTION)

static void
gtk_toggle_action_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	if (prop_id == TOGGLE_PROP_ACTIVE)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (object), g_value_get_boolean (value));
	else
		G_OBJECT_CLASS (gtk_toggle_action_parent_class)->set_property (object, prop_id, value, pspec);
}

static void
gtk_toggle_action_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	if (prop_id == TOGGLE_PROP_ACTIVE)
		g_value_set_boolean (value, GTK_TOGGLE_ACTION (object)->active);
	else
		G_OBJECT_CLASS (gtk_toggle_action_parent_class)->get_property (object, prop_id, value, pspec);
}

static void
gtk_toggle_action_real_activate (GtkAction *action)
{
	GtkToggleAction *toggle = GTK_TOGGLE_ACTION (action);
	gtk_toggle_action_set_active (toggle, !toggle->active);
}

static void gtk_toggle_action_class_init (GtkToggleActionClass *klass)
{
	GObjectClass *oc = G_OBJECT_CLASS (klass);
	oc->set_property = gtk_toggle_action_set_property;
	oc->get_property = gtk_toggle_action_get_property;
	GTK_ACTION_CLASS (klass)->activate = gtk_toggle_action_real_activate;
	toggle_signals[TOGGLE_TOGGLED] =
		g_signal_new ("toggled", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkToggleActionClass, toggled), NULL, NULL, NULL, G_TYPE_NONE, 0);
	g_object_class_install_property (oc, TOGGLE_PROP_ACTIVE,
		g_param_spec_boolean ("active", NULL, NULL, FALSE, G_PARAM_READWRITE));
}
static void gtk_toggle_action_init (GtkToggleAction *action) { action->active = FALSE; }

GtkToggleAction *
gtk_toggle_action_new (const gchar *name, const gchar *label, const gchar *tooltip, const gchar *stock_id)
{
	GtkToggleAction *a = g_object_new (GTK_TYPE_TOGGLE_ACTION, NULL);
	GTK_ACTION (a)->name = g_strdup (name);
	GTK_ACTION (a)->label = g_strdup (label);
	GTK_ACTION (a)->tooltip = g_strdup (tooltip);
	GTK_ACTION (a)->stock_id = g_strdup (stock_id);
	return a;
}

void
gtk_toggle_action_set_active (GtkToggleAction *action, gboolean is_active)
{
	if (!action || action->active == is_active)
		return;
	action->active = is_active;
	g_object_notify (G_OBJECT (action), "active");
	g_signal_emit (action, toggle_signals[TOGGLE_TOGGLED], 0);
}

gboolean gtk_toggle_action_get_active (GtkToggleAction *action) { return action ? action->active : FALSE; }

/* Radio */
enum { RADIO_CHANGED, RADIO_LAST };
static guint radio_signals[RADIO_LAST];

G_DEFINE_TYPE (GtkRadioAction, gtk_radio_action, GTK_TYPE_TOGGLE_ACTION)

void gtk_radio_action_set_current_value (GtkRadioAction *action, gint value);

static void
gtk_radio_action_real_activate (GtkAction *action)
{
	GtkRadioAction *radio = GTK_RADIO_ACTION (action);
	gtk_radio_action_set_current_value (radio, radio->value);
}

static void gtk_radio_action_class_init (GtkRadioActionClass *klass)
{
	GTK_ACTION_CLASS (klass)->activate = gtk_radio_action_real_activate;
	radio_signals[RADIO_CHANGED] =
		g_signal_new ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkRadioActionClass, changed), NULL, NULL, NULL,
			      G_TYPE_NONE, 1, GTK_TYPE_RADIO_ACTION);
}
static void gtk_radio_action_init (GtkRadioAction *action) { action->group = NULL; action->value = 0; }

GtkRadioAction *
gtk_radio_action_new (const gchar *name, const gchar *label, const gchar *tooltip, const gchar *stock_id, gint value)
{
	GtkRadioAction *a = g_object_new (GTK_TYPE_RADIO_ACTION, NULL);
	GTK_ACTION (a)->name = g_strdup (name);
	GTK_ACTION (a)->label = g_strdup (label);
	GTK_ACTION (a)->tooltip = g_strdup (tooltip);
	GTK_ACTION (a)->stock_id = g_strdup (stock_id);
	a->value = value;
	a->group = g_slist_prepend (NULL, a);
	return a;
}

void
gtk_radio_action_set_group (GtkRadioAction *action, GSList *group)
{
	if (action->group && action->group->data == action && action->group->next == NULL)
		g_slist_free (action->group);
	action->group = g_slist_prepend (group, action);
}

GSList *gtk_radio_action_get_group (GtkRadioAction *action) { return action ? action->group : NULL; }

void
gtk_radio_action_set_current_value (GtkRadioAction *action, gint value)
{
	GSList *l;
	GtkRadioAction *current = NULL;
	GtkRadioAction *emit_on;
	static gboolean in_set_current;

	if (!action || in_set_current)
		return;
	for (l = action->group; l; l = l->next) {
		GtkRadioAction *r = l->data;
		if (r->value == value && gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (r)))
			return;
	}
	in_set_current = TRUE;
	for (l = action->group; l; l = l->next) {
		GtkRadioAction *r = l->data;
		gboolean match = (r->value == value);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (r), match);
		if (match)
			current = r;
	}
	if (current == NULL) {
		in_set_current = FALSE;
		return;
	}
	/* Nemo connects "changed" to the first action in the group. */
	emit_on = action->group ? action->group->data : action;
	g_signal_emit (emit_on, radio_signals[RADIO_CHANGED], 0, current);
	in_set_current = FALSE;
}

gint
gtk_radio_action_get_current_value (GtkRadioAction *action)
{
	GSList *l;
	for (l = action->group; l; l = l->next) {
		GtkRadioAction *r = l->data;
		if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (r)))
			return r->value;
	}
	return action->value;
}

/* Action group */
struct _GtkActionGroup {
	GObject parent;
	gchar *name;
	GHashTable *actions;
	gboolean sensitive;
	gboolean visible;
	GtkTranslateFunc translate_func;
	gpointer translate_data;
	GDestroyNotify translate_notify;
	GtkAccelGroup *accel;
};

G_DEFINE_TYPE (GtkActionGroup, gtk_action_group, G_TYPE_OBJECT)

static void
gtk_action_group_finalize (GObject *object)
{
	GtkActionGroup *group = GTK_ACTION_GROUP (object);
	GHashTable *actions;

	g_free (group->name);
	group->name = NULL;
	/* Steal the table first. Unreffing GtkActions during destroy can
	 * re-enter menu updates which look up this group. */
	actions = group->actions;
	group->actions = NULL;
	if (actions)
		g_hash_table_destroy (actions);
	if (group->translate_notify)
		group->translate_notify (group->translate_data);
	group->translate_notify = NULL;
	G_OBJECT_CLASS (gtk_action_group_parent_class)->finalize (object);
}

static void gtk_action_group_class_init (GtkActionGroupClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = gtk_action_group_finalize;
	g_signal_new ("connect-proxy", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
		      0, NULL, NULL, NULL, G_TYPE_NONE, 2, GTK_TYPE_ACTION, GTK_TYPE_WIDGET);
	g_signal_new ("disconnect-proxy", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
		      0, NULL, NULL, NULL, G_TYPE_NONE, 2, GTK_TYPE_ACTION, GTK_TYPE_WIDGET);
	g_signal_new ("pre-activate", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
		      0, NULL, NULL, NULL, G_TYPE_NONE, 1, GTK_TYPE_ACTION);
	g_signal_new ("post-activate", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
		      0, NULL, NULL, NULL, G_TYPE_NONE, 1, GTK_TYPE_ACTION);
}
static void gtk_action_group_init (GtkActionGroup *group)
{
	group->actions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	group->sensitive = TRUE;
	group->visible = TRUE;
}

GtkActionGroup *
gtk_action_group_new (const gchar *name)
{
	GtkActionGroup *g = g_object_new (GTK_TYPE_ACTION_GROUP, NULL);
	g->name = g_strdup (name);
	return g;
}

const gchar *gtk_action_group_get_name (GtkActionGroup *group) { return group->name; }

GtkAction *
gtk_action_group_get_action (GtkActionGroup *group, const gchar *action_name)
{
	if (group == NULL || !GTK_IS_ACTION_GROUP (group) || group->actions == NULL || action_name == NULL)
		return NULL;
	return g_hash_table_lookup (group->actions, action_name);
}

static void
list_action (gpointer key, gpointer value, gpointer data)
{
	GList **list = data;
	(void) key;
	*list = g_list_prepend (*list, value);
}

GList *
gtk_action_group_list_actions (GtkActionGroup *group)
{
	GList *list = NULL;
	if (group == NULL || group->actions == NULL)
		return NULL;
	g_hash_table_foreach (group->actions, list_action, &list);
	return list;
}

void
gtk_action_group_add_action (GtkActionGroup *group, GtkAction *action)
{
	gtk_action_group_add_action_with_accel (group, action, NULL);
}

void
gtk_action_group_add_action_with_accel (GtkActionGroup *group, GtkAction *action, const gchar *accelerator)
{
	gchar *path = NULL;

	if (action->name)
		g_hash_table_insert (group->actions, g_strdup (action->name), g_object_ref (action));
	if (accelerator && accelerator[0] != '\0') {
		g_free (action->accelerator);
		action->accelerator = g_strdup (accelerator);
	}
	if (group->name && action->name)
		path = g_strdup_printf ("<Actions>/%s/%s", group->name, action->name);
	if (path) {
		gtk_action_set_accel_path (action, path);
		g_free (path);
	}
	if (group->accel)
		gtk_action_set_accel_group (action, group->accel);
	if (group->accel && action->accelerator)
		verne_accel_group_connect_action (group->accel, action, action->accelerator);
}

void
verne_action_group_bind_accels (GtkActionGroup *group, GtkAccelGroup *accel)
{
	GList *actions, *l;

	if (group == NULL || accel == NULL)
		return;
	group->accel = accel;
	actions = gtk_action_group_list_actions (group);
	for (l = actions; l; l = l->next) {
		GtkAction *a = l->data;
		if (a == NULL)
			continue;
		gtk_action_set_accel_group (a, accel);
		if (a->accelerator)
			verne_accel_group_connect_action (accel, a, a->accelerator);
	}
	g_list_free (actions);
}

void
verne_action_group_unbind_accels (GtkActionGroup *group, GtkAccelGroup *accel)
{
	GList *actions, *l;

	if (group == NULL || accel == NULL)
		return;
	actions = gtk_action_group_list_actions (group);
	for (l = actions; l; l = l->next) {
		GtkAction *a = l->data;
		if (a)
			verne_accel_group_disconnect_action (accel, a);
	}
	g_list_free (actions);
}

void
gtk_action_group_remove_action (GtkActionGroup *group, GtkAction *action)
{
	if (action->name)
		g_hash_table_remove (group->actions, action->name);
}

static gchar *
translate (GtkActionGroup *group, const gchar *str)
{
	if (str == NULL)
		return NULL;
	if (group->translate_func)
		return g_strdup (group->translate_func (str, group->translate_data));
	return g_strdup (g_dgettext (NULL, str));
}

void
gtk_action_group_add_actions (GtkActionGroup *group, const GtkActionEntry *entries, guint n_entries, gpointer user_data)
{
	guint i;
	for (i = 0; i < n_entries; i++) {
		GtkAction *a = gtk_action_new (entries[i].name,
					       translate (group, entries[i].label),
					       translate (group, entries[i].tooltip),
					       entries[i].stock_id);
		if (entries[i].stock_id && entries[i].stock_id[0] != '\0' && a->icon_name == NULL)
			gtk_action_set_icon_name (a, entries[i].stock_id);
		if (entries[i].callback)
			g_signal_connect (a, "activate", entries[i].callback, user_data);
		gtk_action_group_add_action_with_accel (group, a, entries[i].accelerator);
		g_object_unref (a);
	}
}

void
gtk_action_group_add_toggle_actions (GtkActionGroup *group, const GtkToggleActionEntry *entries, guint n_entries, gpointer user_data)
{
	guint i;
	for (i = 0; i < n_entries; i++) {
		GtkToggleAction *a = gtk_toggle_action_new (entries[i].name,
							    translate (group, entries[i].label),
							    translate (group, entries[i].tooltip),
							    entries[i].stock_id);
		gtk_toggle_action_set_active (a, entries[i].is_active);
		if (entries[i].callback)
			g_signal_connect (a, "toggled", entries[i].callback, user_data);
		gtk_action_group_add_action_with_accel (group, GTK_ACTION (a), entries[i].accelerator);
		g_object_unref (a);
	}
}

void
gtk_action_group_add_radio_actions (GtkActionGroup *group, const GtkRadioActionEntry *entries, guint n_entries, gint value, GCallback on_change, gpointer user_data)
{
	guint i;
	GSList *glist = NULL;
	GtkRadioAction *first = NULL;
	for (i = 0; i < n_entries; i++) {
		GtkRadioAction *a = gtk_radio_action_new (entries[i].name,
							  translate (group, entries[i].label),
							  translate (group, entries[i].tooltip),
							  entries[i].stock_id,
							  entries[i].value);
		glist = g_slist_append (glist, a);
		a->group = glist;
		if (first == NULL)
			first = a;
		if (entries[i].value == value)
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (a), TRUE);
		gtk_action_group_add_action_with_accel (group, GTK_ACTION (a), entries[i].accelerator);
		g_object_unref (a);
	}
	if (first) {
		for (GSList *l = glist; l; l = l->next)
			GTK_RADIO_ACTION (l->data)->group = glist;
		if (on_change)
			g_signal_connect (first, "changed", on_change, user_data);
	}
}

void gtk_action_group_set_sensitive (GtkActionGroup *group, gboolean sensitive) { group->sensitive = sensitive; }
gboolean gtk_action_group_get_sensitive (GtkActionGroup *group) { return group->sensitive; }
void gtk_action_group_set_visible (GtkActionGroup *group, gboolean visible) { group->visible = visible; }
void gtk_action_group_set_translation_domain (GtkActionGroup *group, const gchar *domain) { (void) group; (void) domain; }
void gtk_action_group_set_translate_func (GtkActionGroup *group, GtkTranslateFunc func, gpointer data, GDestroyNotify notify)
{
	group->translate_func = func;
	group->translate_data = data;
	group->translate_notify = notify;
}
gchar *(*gtk_action_group_get_translate_func (GtkActionGroup *group)) (const gchar *, gpointer)
{
	return group->translate_func;
}
