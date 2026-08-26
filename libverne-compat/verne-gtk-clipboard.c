#include "config.h"
#include "verne-gtk-compat.h"
#include "verne-gtk-clipboard-private.h"
#include <string.h>
#include <gio/gio.h>

G_DEFINE_TYPE (GtkClipboard, gtk_clipboard, G_TYPE_OBJECT)
static GHashTable *clipboards;

static void gtk_clipboard_class_init (GtkClipboardClass *c)
{
	g_signal_new ("owner-change", G_TYPE_FROM_CLASS (c), G_SIGNAL_RUN_FIRST,
		      0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);
	g_signal_new ("owner_change", G_TYPE_FROM_CLASS (c), G_SIGNAL_RUN_FIRST,
		      0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_POINTER);
}
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
	list->magic = VERNE_TARGET_LIST_MAGIC;
	list->ref = 1;
	list->entries = g_array_new (FALSE, TRUE, sizeof (GtkTargetEntry));
	if (targets && ntarget)
		g_array_append_vals (list->entries, targets, ntarget);
	return list;
}

void
gtk_target_list_ref (GtkTargetList *list)
{
	if (list != NULL && list->magic == VERNE_TARGET_LIST_MAGIC)
		list->ref++;
}

void
gtk_target_list_unref (GtkTargetList *list)
{
	if (list == NULL)
		return;
	/* qdata destroy on pathbar buttons was invoking this on GObjects
	 * (GTypeInstance at offset 0), which then g_array_free'd an unaligned
	 * interior pointer and aborted in malloc. */
	if (list->magic != VERNE_TARGET_LIST_MAGIC)
		return;
	if (--list->ref)
		return;
	list->magic = 0;
	if (list->entries != NULL) {
		gpointer data = list->entries->data;
		/* A corrupted GArray (pathbar qdata teardown) has an unaligned
		 * data pointer; g_array_free then aborts in malloc. */
		if (data == NULL || (((guintptr) data) & (sizeof (void *) - 1)) == 0)
			g_array_free (list->entries, TRUE);
		list->entries = NULL;
	}
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
	verne_clipboard_install_content (clipboard);
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

static guint
clipboard_info_for_target (GtkClipboard *clipboard, GdkAtom target)
{
	guint i;

	if (clipboard->targets == NULL)
		return 0;
	for (i = 0; i < clipboard->n_targets; i++) {
		if (g_strcmp0 (clipboard->targets[i].target, (const char *) target) == 0)
			return clipboard->targets[i].info;
	}
	return 0;
}

void
gtk_clipboard_request_contents (GtkClipboard *clipboard, GdkAtom target, GtkClipboardReceivedFunc cb, gpointer data)
{
	GtkSelectionData *s = selection_new (target);

	if (clipboard->get_func)
		clipboard->get_func (clipboard, s, clipboard_info_for_target (clipboard, target),
				     clipboard->user_data);
	cb (clipboard, s, data);
	gtk_selection_data_free (s);
}

typedef struct {
	GtkClipboard *clipboard;
	GtkClipboardTextReceivedFunc cb;
	gpointer data;
} VerneClipboardTextReq;

static void
clipboard_text_ready (GObject *source, GAsyncResult *result, gpointer data)
{
	VerneClipboardTextReq *req = data;
	char *text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (source), result, NULL);
	req->cb (req->clipboard, text ? text : "", req->data);
	g_free (text);
	g_free (req);
}

void
gtk_clipboard_request_text (GtkClipboard *clipboard, GtkClipboardTextReceivedFunc cb, gpointer data)
{
	if (clipboard->get_func) {
		GtkSelectionData *s = selection_new (gdk_atom_intern ("text/plain", FALSE));
		gchar *text = NULL;
		clipboard->get_func (clipboard, s, 0, clipboard->user_data);
		if (s->data && s->length > 0)
			text = g_strndup ((char *) s->data, s->length);
		cb (clipboard, text ? text : "", data);
		g_free (text);
		gtk_selection_data_free (s);
		return;
	}
	if (clipboard->gdk) {
		VerneClipboardTextReq *req = g_new0 (VerneClipboardTextReq, 1);
		req->clipboard = clipboard;
		req->cb = cb;
		req->data = data;
		gdk_clipboard_read_text_async (clipboard->gdk, NULL, clipboard_text_ready, req);
		return;
	}
	cb (clipboard, "", data);
}

void
gtk_clipboard_request_targets (GtkClipboard *clipboard, GtkClipboardTargetsReceivedFunc cb, gpointer data)
{
	GdkAtom *atoms = NULL;
	gint n = 0;
	guint i;

	if (clipboard->targets && clipboard->n_targets > 0) {
		n = (gint) clipboard->n_targets;
		atoms = g_new (GdkAtom, (guint) n);
		for (i = 0; i < clipboard->n_targets; i++)
			atoms[i] = gdk_atom_intern (clipboard->targets[i].target, FALSE);
	}
	cb (clipboard, atoms, n, data);
	g_free (atoms);
}

GObject *gtk_clipboard_get_owner (GtkClipboard *clipboard) { return clipboard->owner; }

GtkSelectionData *
gtk_clipboard_wait_for_contents (GtkClipboard *clipboard, GdkAtom target)
{
	GtkSelectionData *s = selection_new (target);
	if (clipboard->get_func)
		clipboard->get_func (clipboard, s, clipboard_info_for_target (clipboard, target),
				     clipboard->user_data);
	return s;
}

typedef struct {
	GMainLoop *loop;
	char **text_out;
} VerneClipboardWaitText;

static void
clipboard_wait_text_ready (GObject *source, GAsyncResult *result, gpointer data)
{
	VerneClipboardWaitText *wait = data;
	*wait->text_out = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (source), result, NULL);
	if (wait->loop && g_main_loop_is_running (wait->loop))
		g_main_loop_quit (wait->loop);
}

gchar *
gtk_clipboard_wait_for_text (GtkClipboard *clipboard)
{
	if (clipboard->get_func) {
		GtkSelectionData *s = selection_new (gdk_atom_intern ("text/plain", FALSE));
		gchar *text = NULL;
		clipboard->get_func (clipboard, s, 0, clipboard->user_data);
		if (s->data && s->length > 0)
			text = g_strndup ((char *) s->data, s->length);
		gtk_selection_data_free (s);
		return text;
	}
	if (clipboard->gdk) {
		GMainLoop *loop = g_main_loop_new (NULL, FALSE);
		char *text = NULL;
		VerneClipboardWaitText wait = { loop, &text };
		gdk_clipboard_read_text_async (clipboard->gdk, NULL,
					       clipboard_wait_text_ready, &wait);
		g_timeout_add (500, (GSourceFunc) g_main_loop_quit, loop);
		g_main_loop_run (loop);
		g_main_loop_unref (loop);
		return text;
	}
	return NULL;
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
	/* GTK3 always NUL-terminates; callers such as g_uri_list_extract_uris
	 * treat get_data() as a C string. */
	if (length >= 0 && data != NULL) {
		s->data = g_malloc ((gsize) length + 1);
		memcpy (s->data, data, (gsize) length);
		s->data[length] = 0;
		s->length = length;
	} else {
		s->data = NULL;
		s->length = length;
	}
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
	GString *joined = g_string_new (NULL);
	guint i;

	if (uris) {
		for (i = 0; uris[i]; i++) {
			g_string_append (joined, uris[i]);
			g_string_append (joined, "\r\n");
		}
	}
	gtk_selection_data_set (s, gdk_atom_intern ("text/uri-list", FALSE), 8,
				(guchar *) joined->str, (gint) joined->len);
	g_string_free (joined, TRUE);
}

gchar **
gtk_selection_data_get_uris (const GtkSelectionData *s)
{
	gchar *text;
	gchar **uris;

	if (!s || !s->data || s->length <= 0)
		return NULL;
	text = g_strndup ((const gchar *) s->data, (gsize) s->length);
	uris = g_uri_list_extract_uris (text);
	g_free (text);
	return uris;
}

gboolean gtk_selection_data_targets_include_text (const GtkSelectionData *s) { (void) s; return TRUE; }
gboolean gtk_selection_data_targets_include_uri (const GtkSelectionData *s) { (void) s; return TRUE; }
GtkSelectionData *gtk_selection_data_copy (const GtkSelectionData *s) {
	GtkSelectionData *n;
	if (!s)
		return NULL;
	n = g_memdup2 (s, sizeof (*s));
	if (s->data && s->length >= 0) {
		n->data = g_malloc ((gsize) s->length + 1);
		memcpy (n->data, s->data, (gsize) s->length);
		n->data[s->length] = 0;
	} else {
		n->data = NULL;
	}
	return n;
}
void gtk_selection_data_free (GtkSelectionData *s) { if (!s) return; g_free (s->data); g_free (s); }

gboolean
gtk_target_list_find (GtkTargetList *list, GdkAtom target, guint *info)
{
	guint i;

	if (!list || !list->entries)
		return FALSE;
	for (i = 0; i < list->entries->len; i++) {
		GtkTargetEntry *e = &g_array_index (list->entries, GtkTargetEntry, i);
		if (e->target == (gchar *) target ||
		    g_strcmp0 (e->target, (const char *) target) == 0) {
			if (info)
				*info = e->info;
			return TRUE;
		}
	}
	return FALSE;
}
