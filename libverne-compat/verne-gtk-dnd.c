/* GTK3 drag-and-drop / clipboard-with-data implemented on GTK4. */
#include "config.h"
#include "verne-gtk-compat.h"
#include "verne-gtk-clipboard-private.h"

#include <string.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

static GQuark
dest_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-drop-target");
	return q;
}

static GQuark
dest_targets_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-drop-targets");
	return q;
}

static GQuark
drop_xy_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-drop-xy");
	return q;
}

static GQuark
selected_action_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-selected-action");
	return q;
}

static GQuark
source_widget_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-source-widget");
	return q;
}

static GdkContentFormats *
formats_from_entries (const GtkTargetEntry *targets, gint n_targets)
{
	const char **mimes;
	GdkContentFormats *formats;
	gint i;

	mimes = g_new0 (const char *, MAX (n_targets, 0) + 1);
	for (i = 0; i < n_targets; i++)
		mimes[i] = targets[i].target ? targets[i].target : "text/uri-list";
	formats = gdk_content_formats_new (mimes, (guint) MAX (n_targets, 0));
	g_free (mimes);
	return formats;
}

static GdkContentFormats *
formats_from_list (GtkTargetList *list)
{
	if (!list || !list->entries)
		return gdk_content_formats_new (NULL, 0);
	return formats_from_entries ((GtkTargetEntry *) list->entries->data, (gint) list->entries->len);
}

static guint
info_for_target (GtkTargetList *list, GdkAtom target)
{
	guint i;
	if (!list || !list->entries)
		return 0;
	for (i = 0; i < list->entries->len; i++) {
		GtkTargetEntry *e = &g_array_index (list->entries, GtkTargetEntry, i);
		if (g_strcmp0 (e->target, (const char *) target) == 0)
			return e->info;
	}
	return 0;
}

/* ---------- GdkContentProvider that asks GTK3 drag-data-get / clipboard get ---------- */
typedef struct {
	GdkContentProvider parent;
	GtkWidget *widget;
	GtkClipboard *clipboard;
	GtkTargetList *targets;
} VerneContentProvider;

typedef struct {
	GdkContentProviderClass parent_class;
} VerneContentProviderClass;

G_DEFINE_TYPE (VerneContentProvider, verne_content_provider, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
verne_content_provider_ref_formats (GdkContentProvider *provider)
{
	VerneContentProvider *self = (VerneContentProvider *) provider;
	return formats_from_list (self->targets);
}

static void
verne_content_provider_write_mime_type_async (GdkContentProvider *provider,
					      const char *mime_type,
					      GOutputStream *stream,
					      int io_priority,
					      GCancellable *cancellable,
					      GAsyncReadyCallback callback,
					      gpointer user_data)
{
	VerneContentProvider *self = (VerneContentProvider *) provider;
	GTask *task = g_task_new (provider, cancellable, callback, user_data);
	GtkSelectionData sel = { 0 };
	guint info;

	(void) io_priority;
	sel.target = (GdkAtom) mime_type;
	sel.type = (GdkAtom) mime_type;
	sel.format = 8;
	info = info_for_target (self->targets, sel.target);

	if (self->clipboard && self->clipboard->get_func) {
		self->clipboard->get_func (self->clipboard, &sel, info, self->clipboard->user_data);
	} else if (self->widget) {
		g_signal_emit_by_name (self->widget, "drag-data-get", NULL, &sel, info, 0U);
	}

	if (sel.data && sel.length > 0)
		g_output_stream_write_all (stream, sel.data, (gsize) sel.length, NULL, cancellable, NULL);
	g_free (sel.data);
	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

static gboolean
verne_content_provider_write_mime_type_finish (GdkContentProvider *provider,
					       GAsyncResult *result,
					       GError **error)
{
	(void) provider;
	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
verne_content_provider_finalize (GObject *object)
{
	VerneContentProvider *self = (VerneContentProvider *) object;
	if (self->targets)
		gtk_target_list_unref (self->targets);
	G_OBJECT_CLASS (verne_content_provider_parent_class)->finalize (object);
}

static void verne_content_provider_finalize (GObject *object);
static gboolean verne_content_provider_get_value (GdkContentProvider *provider, GValue *value, GError **error);

static void
verne_content_provider_class_init (VerneContentProviderClass *klass)
{
	GdkContentProviderClass *cclass = GDK_CONTENT_PROVIDER_CLASS (klass);
	G_OBJECT_CLASS (klass)->finalize = verne_content_provider_finalize;
	cclass->ref_formats = verne_content_provider_ref_formats;
	cclass->ref_storable_formats = verne_content_provider_ref_formats;
	cclass->write_mime_type_async = verne_content_provider_write_mime_type_async;
	cclass->write_mime_type_finish = verne_content_provider_write_mime_type_finish;
	cclass->get_value = verne_content_provider_get_value;
}

static void
verne_content_provider_init (VerneContentProvider *self)
{
	(void) self;
}

static GdkContentProvider *
verne_content_provider_new_for_widget (GtkWidget *widget, GtkTargetList *targets)
{
	VerneContentProvider *p = g_object_new (verne_content_provider_get_type (), NULL);
	p->widget = widget;
	p->targets = targets;
	if (targets)
		targets->ref++;
	return GDK_CONTENT_PROVIDER (p);
}

static GdkContentProvider *
verne_content_provider_new_for_clipboard (GtkClipboard *clipboard)
{
	VerneContentProvider *p = g_object_new (verne_content_provider_get_type (), NULL);
	p->clipboard = clipboard;
	p->targets = gtk_target_list_new (clipboard->targets, clipboard->n_targets);
	return GDK_CONTENT_PROVIDER (p);
}

/* ---------- destination: GtkDropTargetAsync -> GTK3 drag-* signals ---------- */
static GdkDragAction
on_async_motion (GtkDropTargetAsync *self, GdkDrop *drop, double x, double y, gpointer data)
{
	GtkWidget *widget = data;
	gboolean handled = FALSE;
	GdkDragAction selected;
	guint32 time = GDK_CURRENT_TIME;

	(void) self;
	g_object_set_qdata (G_OBJECT (drop), drop_xy_quark (),
			    GINT_TO_POINTER (((int) x & 0xffff) | (((int) y & 0xffff) << 16)));
	g_signal_emit_by_name (widget, "drag-motion", drop, (int) x, (int) y, time, &handled);
	selected = (GdkDragAction) GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (drop), selected_action_quark ()));
	if (selected == 0 && handled)
		selected = GDK_ACTION_COPY;
	if (selected)
		gdk_drop_status (drop, gdk_drop_get_actions (drop), selected);
	return selected;
}

static void
on_async_leave (GtkDropTargetAsync *self, GdkDrop *drop, gpointer data)
{
	(void) self;
	g_signal_emit_by_name ((GtkWidget *) data, "drag-leave", drop, GDK_CURRENT_TIME);
}

static gboolean
on_async_drop (GtkDropTargetAsync *self, GdkDrop *drop, double x, double y, gpointer data)
{
	GtkWidget *widget = data;
	gboolean handled = FALSE;

	(void) self;
	g_object_set_qdata (G_OBJECT (drop), drop_xy_quark (),
			    GINT_TO_POINTER (((int) x & 0xffff) | (((int) y & 0xffff) << 16)));
	g_signal_emit_by_name (widget, "drag-drop", drop, (int) x, (int) y, GDK_CURRENT_TIME, &handled);
	return handled;
}

static gboolean
on_async_accept (GtkDropTargetAsync *self, GdkDrop *drop, gpointer data)
{
	(void) self;
	(void) drop;
	(void) data;
	return TRUE;
}

void
gtk_drag_dest_set (GtkWidget *widget, GtkDestDefaults flags, const GtkTargetEntry *targets, gint n_targets, GdkDragAction actions)
{
	GtkDropTargetAsync *async;
	GdkContentFormats *formats;
	GtkTargetList *list;

	(void) flags;
	async = g_object_get_qdata (G_OBJECT (widget), dest_quark ());
	list = gtk_target_list_new (targets, (guint) MAX (n_targets, 0));
	g_object_set_qdata_full (G_OBJECT (widget), dest_targets_quark (), list, (GDestroyNotify) gtk_target_list_unref);
	formats = formats_from_entries (targets, n_targets);
	if (async == NULL) {
		async = gtk_drop_target_async_new (formats, actions ? actions : (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
		gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (async), GTK_PHASE_CAPTURE);
		g_signal_connect (async, "accept", G_CALLBACK (on_async_accept), widget);
		g_signal_connect (async, "drag-enter", G_CALLBACK (on_async_motion), widget);
		g_signal_connect (async, "drag-motion", G_CALLBACK (on_async_motion), widget);
		g_signal_connect (async, "drag-leave", G_CALLBACK (on_async_leave), widget);
		g_signal_connect (async, "drop", G_CALLBACK (on_async_drop), widget);
		gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (async));
		g_object_set_qdata (G_OBJECT (widget), dest_quark (), async);
	} else {
		gtk_drop_target_async_set_formats (async, formats);
		gtk_drop_target_async_set_actions (async, actions ? actions : (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
	}
	gdk_content_formats_unref (formats);
}

void
gtk_drag_dest_unset (GtkWidget *widget)
{
	GtkDropTargetAsync *async = g_object_get_qdata (G_OBJECT (widget), dest_quark ());
	if (async)
		gtk_widget_remove_controller (widget, GTK_EVENT_CONTROLLER (async));
	g_object_set_qdata (G_OBJECT (widget), dest_quark (), NULL);
	g_object_set_qdata (G_OBJECT (widget), dest_targets_quark (), NULL);
}

void
gtk_drag_dest_set_target_list (GtkWidget *widget, GtkTargetList *list)
{
	GtkDropTargetAsync *async = g_object_get_qdata (G_OBJECT (widget), dest_quark ());
	GdkContentFormats *formats;

	if (list)
		list->ref++;
	g_object_set_qdata_full (G_OBJECT (widget), dest_targets_quark (), list, (GDestroyNotify) gtk_target_list_unref);
	if (async && list) {
		formats = formats_from_list (list);
		gtk_drop_target_async_set_formats (async, formats);
		gdk_content_formats_unref (formats);
	}
}

GtkTargetList *
gtk_drag_dest_get_target_list (GtkWidget *widget)
{
	return g_object_get_qdata (G_OBJECT (widget), dest_targets_quark ());
}

void
gtk_drag_dest_set_track_motion (GtkWidget *widget, gboolean track)
{
	(void) widget;
	(void) track;
}

GdkAtom
gtk_drag_dest_find_target (GtkWidget *widget, GdkDragContext *context, GtkTargetList *list)
{
	GdkContentFormats *formats = NULL;
	guint i;

	if (list == NULL)
		list = gtk_drag_dest_get_target_list (widget);
	if (context && GDK_IS_DROP (context))
		formats = gdk_drop_get_formats (GDK_DROP (context));
	else if (context && GDK_IS_DRAG (context))
		formats = gdk_drag_get_formats (GDK_DRAG (context));
	if (list == NULL || list->entries == NULL)
		return gdk_atom_intern ("text/uri-list", FALSE);
	for (i = 0; i < list->entries->len; i++) {
		GtkTargetEntry *e = &g_array_index (list->entries, GtkTargetEntry, i);
		if (formats == NULL || gdk_content_formats_contain_mime_type (formats, e->target))
			return (GdkAtom) e->target;
	}
	return NULL;
}

typedef struct {
	GtkWidget *widget;
	GdkDrop *drop;
	gint x, y;
	guint info;
	guint32 time;
} VerneDropRead;

static void
drop_read_done (GObject *source, GAsyncResult *result, gpointer data)
{
	VerneDropRead *rd = data;
	const char *mime = NULL;
	GInputStream *stream;
	GBytes *bytes = NULL;
	GtkSelectionData sel = { 0 };

	stream = gdk_drop_read_finish (GDK_DROP (source), result, &mime, NULL);
	if (stream) {
		bytes = g_input_stream_read_bytes (stream, 1 << 20, NULL, NULL);
		g_object_unref (stream);
	}
	sel.target = (GdkAtom) (mime ? mime : "text/uri-list");
	sel.type = sel.target;
	sel.format = 8;
	if (bytes) {
		gsize n;
		sel.data = (guchar *) g_bytes_unref_to_data (bytes, &n);
		sel.length = (gint) n;
	}
	g_signal_emit_by_name (rd->widget, "drag-data-received", rd->drop, rd->x, rd->y, &sel, rd->info, rd->time);
	g_free (sel.data);
	g_free (rd);
}

void
gtk_drag_get_data (GtkWidget *widget, GdkDragContext *context, GdkAtom target, guint32 time)
{
	VerneDropRead *rd;
	const char *mimes[2];
	gpointer packed;

	if (!context || !GDK_IS_DROP (context))
		return;
	rd = g_new0 (VerneDropRead, 1);
	rd->widget = widget;
	rd->drop = GDK_DROP (context);
	packed = g_object_get_qdata (G_OBJECT (context), drop_xy_quark ());
	rd->x = GPOINTER_TO_INT (packed) & 0xffff;
	rd->y = (GPOINTER_TO_INT (packed) >> 16) & 0xffff;
	rd->time = time;
	rd->info = info_for_target (gtk_drag_dest_get_target_list (widget), target);
	mimes[0] = target ? (const char *) target : "text/uri-list";
	mimes[1] = NULL;
	gdk_drop_read_async (GDK_DROP (context), mimes, G_PRIORITY_DEFAULT, NULL, drop_read_done, rd);
}

void
gtk_drag_finish (gpointer context, gboolean success, gboolean del, guint32 time)
{
	GdkDragAction action = 0;
	GtkWidget *source;

	(void) time;
	if (!context)
		return;
	action = (GdkDragAction) GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (context), selected_action_quark ()));
	if (success && action == 0)
		action = GDK_ACTION_COPY;
	if (GDK_IS_DROP (context))
		gdk_drop_finish (GDK_DROP (context), success ? action : 0);
	source = g_object_get_qdata (G_OBJECT (context), source_widget_quark ());
	if (del && source)
		g_signal_emit_by_name (source, "drag-data-delete", context);
	if (GDK_IS_DRAG (context))
		gdk_drag_drop_done (GDK_DRAG (context), success);
}

void
gdk_drag_status (GdkDragContext *context, GdkDragAction action, guint32 time)
{
	(void) time;
	if (!context)
		return;
	g_object_set_qdata (G_OBJECT (context), selected_action_quark (), GINT_TO_POINTER ((int) action));
	if (GDK_IS_DROP (context))
		gdk_drop_status (GDK_DROP (context), gdk_drop_get_actions (GDK_DROP (context)), action);
}

GdkDragAction
gdk_drag_context_get_selected_action (GdkDragContext *context)
{
	GdkDragAction stored;

	if (!context)
		return 0;
	stored = (GdkDragAction) GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (context), selected_action_quark ()));
	if (stored)
		return stored;
	if (GDK_IS_DRAG (context))
		return gdk_drag_get_selected_action (GDK_DRAG (context));
	return 0;
}

GdkDragAction
gdk_drag_context_get_suggested_action (GdkDragContext *context)
{
	if (context && GDK_IS_DROP (context)) {
		GdkDragAction a = gdk_drop_get_actions (GDK_DROP (context));
		if (a & GDK_ACTION_COPY)
			return GDK_ACTION_COPY;
		return a;
	}
	if (context && GDK_IS_DRAG (context))
		return gdk_drag_get_actions (GDK_DRAG (context));
	return GDK_ACTION_COPY;
}

GdkDragAction
gdk_drag_context_get_actions (GdkDragContext *context)
{
	if (context && GDK_IS_DROP (context))
		return gdk_drop_get_actions (GDK_DROP (context));
	if (context && GDK_IS_DRAG (context))
		return gdk_drag_get_actions (GDK_DRAG (context));
	return GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK;
}

GdkSurface *
gdk_drag_context_get_source_window (GdkDragContext *context)
{
	if (context && GDK_IS_DROP (context)) {
		GdkDrag *drag = gdk_drop_get_drag (GDK_DROP (context));
		if (drag)
			return gdk_drag_get_surface (drag);
		return gdk_drop_get_surface (GDK_DROP (context));
	}
	if (context && GDK_IS_DRAG (context))
		return gdk_drag_get_surface (GDK_DRAG (context));
	return NULL;
}

GtkWidget *
gtk_drag_get_source_widget (GdkDragContext *context)
{
	if (!context)
		return NULL;
	return g_object_get_qdata (G_OBJECT (context), source_widget_quark ());
}

void
gtk_drag_highlight (GtkWidget *widget)
{
	gtk_widget_add_css_class (widget, "drop-target");
}

void
gtk_drag_unhighlight (GtkWidget *widget)
{
	gtk_widget_remove_css_class (widget, "drop-target");
}

static GQuark
source_ctrl_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-drag-source");
	return q;
}

static GQuark
source_targets_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-drag-source-targets");
	return q;
}

static gboolean
verne_content_provider_get_value (GdkContentProvider *provider, GValue *value, GError **error)
{
	VerneContentProvider *self = (VerneContentProvider *) provider;
	GtkSelectionData sel = { 0 };
	const char *mime;
	guint info;

	if (G_VALUE_HOLDS_STRING (value))
		mime = "text/plain";
	else if (G_VALUE_TYPE (value) == GDK_TYPE_FILE_LIST)
		mime = "text/uri-list";
	else {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unsupported content type");
		return FALSE;
	}

	sel.target = (GdkAtom) mime;
	sel.type = (GdkAtom) mime;
	sel.format = 8;
	info = info_for_target (self->targets, sel.target);
	if (self->clipboard && self->clipboard->get_func)
		self->clipboard->get_func (self->clipboard, &sel, info, self->clipboard->user_data);
	else if (self->widget)
		g_signal_emit_by_name (self->widget, "drag-data-get", NULL, &sel, info, 0U);

	if (sel.data == NULL || sel.length <= 0) {
		g_free (sel.data);
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "No data");
		return FALSE;
	}
	if (G_VALUE_HOLDS_STRING (value)) {
		gchar *text = g_strndup ((char *) sel.data, sel.length);
		g_value_take_string (value, text);
	} else {
		gchar **uris = g_strsplit ((char *) sel.data, "\r\n", -1);
		GFile **files;
		guint i, n = 0;
		GdkFileList *list;
		for (i = 0; uris && uris[i]; i++) {
			if (uris[i][0] != '\0' && uris[i][0] != '#')
				n++;
		}
		files = g_new0 (GFile *, n);
		n = 0;
		for (i = 0; uris && uris[i]; i++) {
			if (uris[i][0] != '\0' && uris[i][0] != '#')
				files[n++] = g_file_new_for_uri (uris[i]);
		}
		list = gdk_file_list_new_from_array (files, n);
		g_value_take_boxed (value, list);
		for (i = 0; i < n; i++)
			g_object_unref (files[i]);
		g_free (files);
		g_strfreev (uris);
	}
	g_free (sel.data);
	return TRUE;
}
void
gtk_drag_source_set (GtkWidget *widget, GdkModifierType start_button_mask, const GtkTargetEntry *targets, gint n_targets, GdkDragAction actions)
{
	GtkDragSource *src = g_object_get_qdata (G_OBJECT (widget), source_ctrl_quark ());
	GtkTargetList *list = gtk_target_list_new (targets, (guint) MAX (n_targets, 0));
	GdkContentProvider *provider = verne_content_provider_new_for_widget (widget, list);

	(void) start_button_mask;
	if (src == NULL) {
		src = gtk_drag_source_new ();
		gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (src));
		g_object_set_qdata (G_OBJECT (widget), source_ctrl_quark (), src);
	}
	gtk_drag_source_set_actions (src, actions ? actions : GDK_ACTION_COPY);
	gtk_drag_source_set_content (src, provider);
	g_object_set_qdata_full (G_OBJECT (widget), source_targets_quark (), list,
				 (GDestroyNotify) gtk_target_list_unref);
	g_object_unref (provider);
}

void
gtk_drag_source_unset (GtkWidget *widget)
{
	GtkDragSource *src = g_object_get_qdata (G_OBJECT (widget), source_ctrl_quark ());
	if (src)
		gtk_widget_remove_controller (widget, GTK_EVENT_CONTROLLER (src));
	g_object_set_qdata (G_OBJECT (widget), source_ctrl_quark (), NULL);
	g_object_set_qdata (G_OBJECT (widget), source_targets_quark (), NULL);
}

void
gtk_drag_source_set_target_list (GtkWidget *widget, GtkTargetList *list)
{
	GtkDragSource *src = g_object_get_qdata (G_OBJECT (widget), source_ctrl_quark ());
	GdkContentProvider *provider;

	if (list)
		list->ref++;
	g_object_set_qdata_full (G_OBJECT (widget), source_targets_quark (), list,
				 (GDestroyNotify) gtk_target_list_unref);
	if (src && list) {
		provider = verne_content_provider_new_for_widget (widget, list);
		gtk_drag_source_set_content (src, provider);
		g_object_unref (provider);
	}
}

GtkTargetList *
gtk_drag_source_get_target_list (GtkWidget *widget)
{
	return g_object_get_qdata (G_OBJECT (widget), source_targets_quark ());
}

static void
on_dnd_finished (GdkDrag *drag, gpointer widget)
{
	g_signal_emit_by_name (widget, "drag-end", drag);
}

static void
on_drag_cancel (GdkDrag *drag, GdkDragCancelReason reason, gpointer widget)
{
	gboolean handled = FALSE;
	g_signal_emit_by_name (widget, "drag-failed", drag, (int) reason, &handled);
	g_signal_emit_by_name (widget, "drag-end", drag);
}

GdkDragContext *
gtk_drag_begin_with_coordinates (GtkWidget *widget, GtkTargetList *targets, GdkDragAction actions, gint button, GdkEvent *event, gint x, gint y)
{
	GdkSurface *surface;
	GdkDevice *device = NULL;
	GdkSeat *seat;
	GdkContentProvider *provider;
	GdkDrag *drag;
	GtkNative *native;

	(void) button;
	(void) event;
	if (targets == NULL)
		targets = gtk_drag_source_get_target_list (widget);
	native = gtk_widget_get_native (widget);
	surface = native ? gtk_native_get_surface (native) : NULL;
	seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
	if (seat)
		device = gdk_seat_get_pointer (seat);
	if (surface == NULL || device == NULL)
		return NULL;
	provider = verne_content_provider_new_for_widget (widget, targets);
	drag = gdk_drag_begin (surface, device, provider, actions ? actions : GDK_ACTION_COPY, (double) x, (double) y);
	g_object_unref (provider);
	if (drag) {
		g_object_set_qdata (G_OBJECT (drag), source_widget_quark (), widget);
		g_signal_connect (drag, "dnd-finished", G_CALLBACK (on_dnd_finished), widget);
		g_signal_connect (drag, "cancel", G_CALLBACK (on_drag_cancel), widget);
		g_signal_emit_by_name (widget, "drag-begin", drag);
	}
	return (GdkDragContext *) drag;
}

gpointer
gtk_drag_begin (GtkWidget *widget, GtkTargetList *targets, GdkDragAction actions, gint button, GdkEvent *event)
{
	gint x = 0, y = 0;
	if (event) {
		x = (gint) event->button.x;
		y = (gint) event->button.y;
	}
	return gtk_drag_begin_with_coordinates (widget, targets, actions, button, event, x, y);
}

void
gtk_drag_set_icon_pixbuf (GdkDragContext *context, GdkPixbuf *pixbuf, gint hot_x, gint hot_y)
{
	GdkTexture *texture;

	if (!context || !pixbuf || !GDK_IS_DRAG (context))
		return;
	texture = gdk_texture_new_for_pixbuf (pixbuf);
	gtk_drag_icon_set_from_paintable (GDK_DRAG (context), GDK_PAINTABLE (texture), hot_x, hot_y);
	g_object_unref (texture);
}

void
gtk_drag_set_icon_name (GdkDragContext *context, const gchar *name, gint hot_x, gint hot_y)
{
	GtkIconPaintable *icon;
	GtkIconTheme *theme;

	if (!context || !GDK_IS_DRAG (context))
		return;
	theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
	icon = gtk_icon_theme_lookup_icon (theme, name ? name : "text-x-generic", NULL, 48, 1, GTK_TEXT_DIR_NONE, 0);
	if (icon) {
		gtk_drag_icon_set_from_paintable (GDK_DRAG (context), GDK_PAINTABLE (icon), hot_x, hot_y);
		g_object_unref (icon);
	}
}

void
gtk_drag_set_icon_default (GdkDragContext *context)
{
	gtk_drag_set_icon_name (context, "text-x-generic", 0, 0);
}

void
gtk_drag_set_icon_widget (GdkDragContext *context, GtkWidget *widget, gint hot_x, gint hot_y)
{
	(void) widget;
	gtk_drag_set_icon_default (context);
	if (context && GDK_IS_DRAG (context))
		gdk_drag_set_hotspot (GDK_DRAG (context), hot_x, hot_y);
}

void
gtk_drag_set_icon_surface (GdkDragContext *context, cairo_surface_t *surface)
{
	GdkPixbuf *pixbuf;

	if (!surface)
		return;
	pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0,
					      cairo_image_surface_get_width (surface),
					      cairo_image_surface_get_height (surface));
	if (pixbuf) {
		gtk_drag_set_icon_pixbuf (context, pixbuf, 0, 0);
		g_object_unref (pixbuf);
	}
}

void
verne_clipboard_install_content (GtkClipboard *clipboard)
{
	GdkContentProvider *provider;

	if (clipboard == NULL || clipboard->gdk == NULL || clipboard->get_func == NULL)
		return;
	provider = verne_content_provider_new_for_clipboard (clipboard);
	gdk_clipboard_set_content (clipboard->gdk, provider);
	g_object_unref (provider);
}
