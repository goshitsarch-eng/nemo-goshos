#include "config.h"
#include "verne-gtk-compat.h"
#include <string.h>

struct _GtkClipboard {
	GObject parent;
	GdkClipboard *gdk;
	GObject *owner;
	GtkClipboardGetFunc get_func;
	GtkClipboardClearFunc clear_func;
	gpointer user_data;
	GtkTargetEntry *targets;
	guint n_targets;
};

struct _GtkTargetList {
	guint ref;
	GArray *entries;
};

G_DEFINE_TYPE (GtkClipboard, gtk_clipboard, G_TYPE_OBJECT)
static GHashTable *clipboards;

static void gtk_clipboard_class_init (GtkClipboardClass *c) { (void) c; }
static void gtk_clipboard_init (GtkClipboard *c) { (void) c; }

GdkAtom
gdk_atom_intern (const gchar *atom_name, gboolean only_if_exists)
{
	(void) only_if_exists;
	return (GdkAtom) g_intern_string (atom_name);
}
gchar *
gdk_atom_name (GdkAtom atom)
{
	return g_strdup (atom ? (const char *) atom : "");
}

GtkTargetList *
gtk_target_list_new (const GtkTargetEntry *targets, guint ntarget)
{
	GtkTargetList *list = g_new0 (GtkTargetList, 1);
	list->ref = 1;
	list->entries = g_array_new (FALSE, TRUE, sizeof (GtkTargetEntry));
	if (targets && ntarget)
		g_array_append_vals (list->entries, targets, ntarget);
	return list;
}
void gtk_target_list_unref (GtkTargetList *list) {
	if (!list || --list->ref) return;
	g_array_free (list->entries, TRUE);
	g_free (list);
}
void gtk_target_list_add (GtkTargetList *list, GdkAtom target, guint flags, guint info) {
	GtkTargetEntry e = { (gchar *) target, flags, info };
	g_array_append_val (list->entries, e);
}
void gtk_target_list_add_uri_targets (GtkTargetList *list, guint info) {
	gtk_target_list_add (list, gdk_atom_intern ("text/uri-list", FALSE), 0, info);
}
void gtk_target_list_add_text_targets (GtkTargetList *list, guint info) {
	gtk_target_list_add (list, gdk_atom_intern ("text/plain", FALSE), 0, info);
}
void gtk_target_list_add_image_targets (GtkTargetList *list, guint info, gboolean writable) {
	(void) writable;
	gtk_target_list_add (list, gdk_atom_intern ("image/png", FALSE), 0, info);
}

static GtkClipboard *
clipboard_for (GdkDisplay *display, GdkAtom selection)
{
	GtkClipboard *c;
	gchar *key;
	if (clipboards == NULL)
		clipboards = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	if (display == NULL)
		display = gdk_display_get_default ();
	key = g_strdup_printf ("%p:%s", display, selection ? (char *) selection : "CLIPBOARD");
	c = g_hash_table_lookup (clipboards, key);
	if (c == NULL) {
		c = g_object_new (GTK_TYPE_CLIPBOARD, NULL);
		if (selection == GDK_SELECTION_PRIMARY)
			c->gdk = gdk_display_get_primary_clipboard (display);
		else
			c->gdk = gdk_display_get_clipboard (display);
		g_hash_table_insert (clipboards, key, c);
	} else
		g_free (key);
	return c;
}

GtkClipboard *gtk_clipboard_get (GdkAtom selection) {
	return clipboard_for (gdk_display_get_default (), selection);
}
GtkClipboard *gtk_clipboard_get_for_display (GdkDisplay *display, GdkAtom selection) {
	return clipboard_for (display, selection);
}

void
gtk_clipboard_set_text (GtkClipboard *clipboard, const gchar *text, gint len)
{
	gchar *s;
	if (len < 0)
		gdk_clipboard_set_text (clipboard->gdk, text ? text : "");
	else {
		s = g_strndup (text, len);
		gdk_clipboard_set_text (clipboard->gdk, s);
		g_free (s);
	}
}

void
gtk_clipboard_set_with_data (GtkClipboard *clipboard, const GtkTargetEntry *targets, guint n_targets,
			     GtkClipboardGetFunc get_func, GtkClipboardClearFunc clear_func, gpointer user_data)
{
	clipboard->get_func = get_func;
	clipboard->clear_func = clear_func;
	clipboard->user_data = user_data;
	g_free (clipboard->targets);
	clipboard->targets = g_memdup2 (targets, n_targets * sizeof (GtkTargetEntry));
	clipboard->n_targets = n_targets;
}

void
gtk_clipboard_set_with_owner (GtkClipboard *clipboard, const GtkTargetEntry *targets, guint n_targets,
			      GtkClipboardGetFunc get_func, GtkClipboardClearFunc clear_func, GObject *owner)
{
	gtk_clipboard_set_with_data (clipboard, targets, n_targets, get_func, clear_func, owner);
	clipboard->owner = owner;
}

void gtk_clipboard_set_can_store (GtkClipboard *clipboard, const GtkTargetEntry *targets, gint n_targets) {
	(void) clipboard; (void) targets; (void) n_targets;
}
void gtk_clipboard_clear (GtkClipboard *clipboard) {
	if (clipboard->clear_func)
		clipboard->clear_func (clipboard, clipboard->user_data);
	clipboard->get_func = NULL;
	gdk_clipboard_set_text (clipboard->gdk, "");
}

static GtkSelectionData *
selection_new (GdkAtom target)
{
	GtkSelectionData *s = g_new0 (GtkSelectionData, 1);
	s->target = target;
	return s;
}

void
gtk_clipboard_request_contents (GtkClipboard *clipboard, GdkAtom target, GtkClipboardReceivedFunc cb, gpointer data)
{
	GtkSelectionData *s = selection_new (target);
	if (clipboard->get_func)
		clipboard->get_func (clipboard, s, 0, clipboard->user_data);
	else {
		gdk_clipboard_read_text_async (clipboard->gdk, NULL, NULL, NULL);
	}
	cb (clipboard, s, data);
	gtk_selection_data_free (s);
}

void
gtk_clipboard_request_text (GtkClipboard *clipboard, GtkClipboardTextReceivedFunc cb, gpointer data)
{
	gdk_clipboard_read_text_async (clipboard->gdk, NULL, NULL, NULL);
	cb (clipboard, "", data);
}

void
gtk_clipboard_request_targets (GtkClipboard *clipboard, GtkClipboardTargetsReceivedFunc cb, gpointer data)
{
	GdkAtom atoms[1] = { gdk_atom_intern ("text/plain", FALSE) };
	(void) clipboard;
	cb (clipboard, atoms, 1, data);
}

GObject *gtk_clipboard_get_owner (GtkClipboard *clipboard) { return clipboard->owner; }

GtkSelectionData *
gtk_clipboard_wait_for_contents (GtkClipboard *clipboard, GdkAtom target)
{
	GtkSelectionData *s = selection_new (target);
	if (clipboard->get_func)
		clipboard->get_func (clipboard, s, 0, clipboard->user_data);
	return s;
}

gchar *
gtk_clipboard_wait_for_text (GtkClipboard *clipboard)
{
	return gdk_clipboard_read_text_finish (clipboard->gdk, NULL, NULL);
}

const guchar *gtk_selection_data_get_data (const GtkSelectionData *s) { return s ? s->data : NULL; }
gint gtk_selection_data_get_length (const GtkSelectionData *s) { return s ? s->length : 0; }
GdkAtom gtk_selection_data_get_target (const GtkSelectionData *s) { return s ? s->target : NULL; }
GdkAtom gtk_selection_data_get_data_type (const GtkSelectionData *s) { return s ? s->type : NULL; }
gint gtk_selection_data_get_format (const GtkSelectionData *s) { return s ? s->format : 0; }

void
gtk_selection_data_set (GtkSelectionData *s, GdkAtom type, gint format, const guchar *data, gint length)
{
	s->type = type;
	s->format = format;
	g_free (s->data);
	s->data = (length >= 0) ? g_memdup2 (data, length) : NULL;
	s->length = length;
}

void
gtk_selection_data_set_text (GtkSelectionData *s, const gchar *str, gint len)
{
	if (len < 0)
		len = str ? (gint) strlen (str) : 0;
	gtk_selection_data_set (s, gdk_atom_intern ("UTF8_STRING", FALSE), 8, (const guchar *) str, len);
}

GtkTargetEntry *
gtk_target_table_new_from_list (GtkTargetList *list, gint *n_targets)
{
	GtkTargetEntry *table;
	guint n;

	n = list && list->entries ? list->entries->len : 0;
	if (n_targets)
		*n_targets = (gint) n;
	if (n == 0)
		return NULL;
	table = g_new (GtkTargetEntry, n);
	memcpy (table, list->entries->data, n * sizeof (GtkTargetEntry));
	return table;
}

void
gtk_target_table_free (GtkTargetEntry *targets, gint n_targets)
{
	(void) n_targets;
	g_free (targets);
}

void
gtk_selection_data_set_uris (GtkSelectionData *s, gchar **uris)
{
	gchar *joined = uris ? g_strjoinv ("\r\n", uris) : g_strdup ("");
	gtk_selection_data_set (s, gdk_atom_intern ("text/uri-list", FALSE), 8, (guchar *) joined, strlen (joined));
	g_free (joined);
}

gchar **
gtk_selection_data_get_uris (const GtkSelectionData *s)
{
	if (!s || !s->data)
		return NULL;
	return g_strsplit ((char *) s->data, "\r\n", -1);
}

gboolean gtk_selection_data_targets_include_text (const GtkSelectionData *s) { (void) s; return TRUE; }
gboolean gtk_selection_data_targets_include_uri (const GtkSelectionData *s) { (void) s; return TRUE; }
GtkSelectionData *gtk_selection_data_copy (const GtkSelectionData *s) {
	GtkSelectionData *n = g_memdup2 (s, sizeof (*s));
	n->data = s->data ? g_memdup2 (s->data, s->length) : NULL;
	return n;
}
void gtk_selection_data_free (GtkSelectionData *s) { if (!s) return; g_free (s->data); g_free (s); }

void gtk_drag_dest_set (GtkWidget *widget, GtkDestDefaults flags, const GtkTargetEntry *targets, gint n_targets, GdkDragAction actions) {
	(void) flags; (void) targets; (void) n_targets;
	gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gtk_drop_target_new (G_TYPE_STRING, actions ? actions : GDK_ACTION_COPY)));
}
void gtk_drag_dest_unset (GtkWidget *widget) { (void) widget; }
void gtk_drag_source_set (GtkWidget *widget, GdkModifierType start_button_mask, const GtkTargetEntry *targets, gint n_targets, GdkDragAction actions) {
	GtkDragSource *src = gtk_drag_source_new ();
	(void) start_button_mask; (void) targets; (void) n_targets;
	gtk_drag_source_set_actions (src, actions ? actions : GDK_ACTION_COPY);
	gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (src));
}
void gtk_drag_source_unset (GtkWidget *widget) { (void) widget; }
void gtk_drag_finish (gpointer context, gboolean success, gboolean del, guint32 time) {
	(void) context; (void) success; (void) del; (void) time;
}
GdkDragContext *gtk_drag_begin_with_coordinates (GtkWidget *widget, GtkTargetList *targets, GdkDragAction actions, gint button, GdkEvent *event, gint x, gint y) {
	(void) widget; (void) targets; (void) actions; (void) button; (void) event; (void) x; (void) y;
	return NULL;
}
void gtk_drag_set_icon_pixbuf (GdkDragContext *context, GdkPixbuf *pixbuf, gint hot_x, gint hot_y) { (void) context; (void) pixbuf; (void) hot_x; (void) hot_y; }
void gtk_drag_set_icon_name (GdkDragContext *context, const gchar *name, gint hot_x, gint hot_y) { (void) context; (void) name; (void) hot_x; (void) hot_y; }
void gtk_drag_set_icon_default (GdkDragContext *context) { (void) context; }
void gtk_drag_set_icon_widget (GdkDragContext *context, GtkWidget *widget, gint hot_x, gint hot_y) { (void) context; (void) widget; (void) hot_x; (void) hot_y; }
GtkWidget *gtk_drag_get_source_widget (GdkDragContext *context) { (void) context; return NULL; }
gboolean gtk_drag_check_threshold (GtkWidget *widget, gint start_x, gint start_y, gint current_x, gint current_y) {
	(void) widget;
	return (ABS (current_x - start_x) > 4) || (ABS (current_y - start_y) > 4);
}
