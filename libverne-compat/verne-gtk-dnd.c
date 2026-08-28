/* GTK3 drag-and-drop / clipboard-with-data implemented on GTK4. */
#include "config.h"
#include "verne-gtk-compat.h"
#include "verne-gtk-clipboard-private.h"

#include <string.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <gtk/gtk.h>
#include <gdk/x11/gdkx.h>
#include <graphene.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

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
dest_flags_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-drop-flags");
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
drop_read_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-drop-read-inflight");
	return q;
}

static GQuark
drop_cache_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-drop-read-cache");
	return q;
}

static GList *verne_drop_dests;

static void
verne_drop_dest_gone (gpointer data, GObject *widget)
{
	(void) data;
	verne_drop_dests = g_list_remove (verne_drop_dests, widget);
}

static void
verne_drop_dest_register (GtkWidget *widget)
{
	if (widget == NULL || g_list_find (verne_drop_dests, widget))
		return;
	verne_drop_dests = g_list_prepend (verne_drop_dests, widget);
	g_object_weak_ref (G_OBJECT (widget), verne_drop_dest_gone, NULL);
}

static void
verne_drop_dest_unregister (GtkWidget *widget)
{
	if (widget == NULL || g_list_find (verne_drop_dests, widget) == NULL)
		return;
	g_object_weak_unref (G_OBJECT (widget), verne_drop_dest_gone, NULL);
	verne_drop_dests = g_list_remove (verne_drop_dests, widget);
}

typedef struct {
	int x;
	int y;
} VerneDropXY;

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
	GdkDrag *drag;
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
		g_signal_emit_by_name (self->widget, "drag-data-get", self->drag, &sel, info, 0U);
	}

	if (sel.data && sel.length > 0)
		g_output_stream_write_all (stream, sel.data, (gsize) sel.length, NULL, cancellable, NULL);
	g_debug ("content write mime=%s len=%d widget=%s", mime_type, sel.length,
		   self->widget ? G_OBJECT_TYPE_NAME (self->widget) : "none");
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
		gtk_target_list_ref (targets);
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

/* ---------- in-process DnD (avoids gdk_drag_begin's X11 nested grab) ---------- */
typedef struct _VerneLocalDrag {
	GObject parent_instance;
	GtkWidget *source;
	GtkTargetList *targets;
	GdkDragAction actions;
	GdkDragAction selected;
	GtkWidget *icon_window;
	GtkWidget *picture;
	GtkWidget *current_dest;
	GdkDrag *native_drag;
	Window xdnd_source;
	Window xdnd_target;
	unsigned long *xdnd_atoms;
	guint n_xdnd_atoms;
	guint xdnd_fd_id;
	guint xdnd_finish_timeout;
	double dest_x;
	double dest_y;
	int hot_x;
	int hot_y;
	guint poll_id;
	gboolean drop_emitted;
	gboolean handed_over;
	gboolean xdnd_accepted;
	gboolean xdnd_dropped;
	gboolean xdnd_finished;
	gboolean xdnd_created_window;
	gboolean dest_finished;
	gboolean xdnd_served_data;
} VerneLocalDrag;

typedef struct _VerneLocalDragClass {
	GObjectClass parent_class;
} VerneLocalDragClass;

G_DEFINE_TYPE (VerneLocalDrag, verne_local_drag, G_TYPE_OBJECT)

#define VERNE_IS_LOCAL_DRAG(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), verne_local_drag_get_type ()))

static void verne_xdnd_teardown (VerneLocalDrag *local);
static gboolean verne_idle_destroy_window (gpointer data);
static void verne_local_cleanup (VerneLocalDrag *local);

static void
verne_local_drag_finalize (GObject *object)
{
	VerneLocalDrag *self = (VerneLocalDrag *) object;

	g_clear_handle_id (&self->poll_id, g_source_remove);
	if (self->icon_window) {
		gtk_widget_set_visible (self->icon_window, FALSE);
		g_timeout_add_seconds (2, verne_idle_destroy_window, g_object_ref (self->icon_window));
		self->icon_window = NULL;
		self->picture = NULL;
	}
	if (self->targets) {
		gtk_target_list_unref (self->targets);
		self->targets = NULL;
	}
	if (self->native_drag) {
		g_signal_handlers_disconnect_by_data (self->native_drag, self);
		g_object_unref (self->native_drag);
		self->native_drag = NULL;
	}
	verne_xdnd_teardown (self);
	G_OBJECT_CLASS (verne_local_drag_parent_class)->finalize (object);
}

static void
verne_local_drag_class_init (VerneLocalDragClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = verne_local_drag_finalize;
}

static void
verne_local_drag_init (VerneLocalDrag *self)
{
	(void) self;
}

/* ---------- destination: GtkDropTargetAsync -> GTK3 drag-* signals ---------- */
static GtkWidget *pending_drop_widget;
static GdkDrop *pending_drop;
static double pending_drop_x, pending_drop_y;
static gboolean drop_already_emitted;
static GtkWidget *forward_last_dest;

static void verne_reset_drop_async_controllers (void);
static void verne_queue_gdk_drop_finish (GdkDrop *drop, GdkDragAction action);

static GdkDragAction on_async_motion (GtkDropTargetAsync *self, GdkDrop *drop, double x, double y, gpointer data);
static gboolean on_async_drop (GtkDropTargetAsync *self, GdkDrop *drop, double x, double y, gpointer data);
static void on_async_leave (GtkDropTargetAsync *self, GdkDrop *drop, gpointer data);
static gboolean on_async_accept (GtkDropTargetAsync *self, GdkDrop *drop, gpointer data);
static int verne_widget_depth (GtkWidget *widget);
static gboolean verne_point_in_widget (GtkWidget *root, GtkWidget *widget, double px, double py,
				       double *out_x, double *out_y);

static GQuark
forwarder_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-drop-forwarder");
	return q;
}

static GtkWidget *
verne_dest_for_native_xy (GtkNative *native, double x, double y, double *out_x, double *out_y)
{
	GtkWidget *root, *best = NULL, *fallback = NULL;
	GList *d;
	int best_depth = -1;
	double bx = x, by = y, fb_x = x, fb_y = y;

	if (native == NULL)
		return NULL;
	root = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (native)));
	if (root == NULL)
		root = GTK_WIDGET (native);
	for (d = verne_drop_dests; d; d = d->next) {
		GtkWidget *dest = d->data;
		double ox = 0, oy = 0;
		int depth;
		graphene_point_t p;

		if (!GTK_IS_WIDGET (dest) || gtk_widget_get_native (dest) != native)
			continue;
		if (g_object_get_qdata (G_OBJECT (dest), forwarder_quark ()))
			continue;
		if (verne_point_in_widget (root, dest, x, y, &ox, &oy)) {
			const char *tn = G_OBJECT_TYPE_NAME (dest);

			depth = verne_widget_depth (dest);
			if (tn && (strstr (tn, "Icon") || strstr (tn, "Canvas") ||
				   strstr (tn, "TreeView") || strstr (tn, "Places")))
				depth += 1000;
			else if (GTK_IS_BUTTON (dest))
				depth -= 100;
			if (depth >= best_depth) {
				best = dest;
				bx = ox;
				by = oy;
				best_depth = depth;
			}
		} else if (fallback == NULL &&
			   gtk_widget_compute_point (root, dest,
						     &GRAPHENE_POINT_INIT ((float) x, (float) y), &p)) {
			fallback = dest;
			fb_x = p.x;
			fb_y = p.y;
		}
	}
	if (best == NULL) {
		best = fallback;
		bx = fb_x;
		by = fb_y;
	}
	if (out_x)
		*out_x = bx;
	if (out_y)
		*out_y = by;
	return best;
}

static void
verne_forward_leave_dest (GtkDropTargetAsync *self, GdkDrop *drop)
{
	if (forward_last_dest && GTK_IS_WIDGET (forward_last_dest))
		on_async_leave (self, drop, forward_last_dest);
	forward_last_dest = NULL;
}

static GdkDragAction
on_forward_motion (GtkDropTargetAsync *self, GdkDrop *drop, double x, double y, gpointer data)
{
	GtkWidget *win = data;
	GtkWidget *dest;
	double dx = x, dy = y;

	dest = verne_dest_for_native_xy (GTK_NATIVE (win), x, y, &dx, &dy);
	if (forward_last_dest && forward_last_dest != dest)
		on_async_leave (self, drop, forward_last_dest);
	forward_last_dest = dest;
	if (dest == NULL)
		return GDK_ACTION_COPY;
	return on_async_motion (self, drop, dx, dy, dest);
}

static gboolean
on_forward_drop (GtkDropTargetAsync *self, GdkDrop *drop, double x, double y, gpointer data)
{
	GtkWidget *win = data;
	GtkWidget *dest;
	double dx = x, dy = y;

	dest = verne_dest_for_native_xy (GTK_NATIVE (win), x, y, &dx, &dy);
	g_debug ("forward drop on %s -> %s at %.0f,%.0f (win %.0f,%.0f)",
		   G_OBJECT_TYPE_NAME (win),
		   dest ? G_OBJECT_TYPE_NAME (dest) : "(none)", dx, dy, x, y);
	if (dest == NULL)
		return FALSE;
	forward_last_dest = dest;
	return on_async_drop (self, drop, dx, dy, dest);
}

static void
on_forward_leave (GtkDropTargetAsync *self, GdkDrop *drop, gpointer data)
{
	(void) data;
	verne_forward_leave_dest (self, drop);
}

static void
verne_ensure_native_drop_forwarder (GtkWidget *widget)
{
	GtkWidget *win;
	GtkDropTargetAsync *async;

	if (widget == NULL)
		return;
	win = GTK_WIDGET (gtk_widget_get_native (widget));
	if (win == NULL || !GTK_IS_WINDOW (win) || GTK_IS_MENU (win))
		return;
	if (g_object_get_qdata (G_OBJECT (win), forwarder_quark ()))
		return;
	/* NULL formats means "offer me everything". The hard-coded list here
	 * used to reject anything else outright - a GTK_TREE_MODEL_ROW drag
	 * (reordering bookmarks in the sidebar) or an XdndDirectSave0 drag from
	 * a browser never reached a destination at all. Which drags are
	 * actually acceptable is the destination's decision, below. */
	async = gtk_drop_target_async_new (NULL,
					   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (async), GTK_PHASE_CAPTURE);
	g_signal_connect (async, "accept", G_CALLBACK (on_async_accept), win);
	g_signal_connect (async, "drag-enter", G_CALLBACK (on_forward_motion), win);
	g_signal_connect (async, "drag-motion", G_CALLBACK (on_forward_motion), win);
	g_signal_connect (async, "drag-leave", G_CALLBACK (on_forward_leave), win);
	g_signal_connect (async, "drop", G_CALLBACK (on_forward_drop), win);
	gtk_widget_add_controller (win, GTK_EVENT_CONTROLLER (async));
	g_object_set_qdata (G_OBJECT (win), forwarder_quark (), async);
	g_debug ("installed drop forwarder on %s native=%s",
		   G_OBJECT_TYPE_NAME (widget), G_OBJECT_TYPE_NAME (win));
}

static void
on_dest_realize_forwarder (GtkWidget *widget, gpointer data)
{
	gpointer p;

	(void) data;
	verne_ensure_native_drop_forwarder (widget);
		p = g_object_get_qdata (G_OBJECT (widget), dest_quark ());
	if (GTK_IS_DROP_TARGET_ASYNC (p) &&
	    gtk_widget_get_native (widget) &&
	    g_object_get_qdata (G_OBJECT (gtk_widget_get_native (widget)), forwarder_quark ())) {
		gtk_widget_remove_controller (widget, GTK_EVENT_CONTROLLER (p));
		g_object_set_qdata (G_OBJECT (widget), dest_quark (), NULL);
	}
}

static void
verne_schedule_native_drop_forwarder (GtkWidget *widget)
{
	verne_ensure_native_drop_forwarder (widget);
	if (gtk_widget_get_native (widget) == NULL &&
	    g_object_get_data (G_OBJECT (widget), "verne-fwd-realize") == NULL) {
		g_object_set_data (G_OBJECT (widget), "verne-fwd-realize", GINT_TO_POINTER (1));
		g_signal_connect (widget, "realize", G_CALLBACK (on_dest_realize_forwarder), NULL);
	}
}

static void verne_local_cleanup (VerneLocalDrag *local);
static void verne_local_update_dest (VerneLocalDrag *local);
static void verne_local_move_icon (VerneLocalDrag *local);
static void verne_local_emit_drop (VerneLocalDrag *local);

static void
clear_pending_drop (void)
{
	if (pending_drop)
		g_object_unref (pending_drop);
	pending_drop = NULL;
	pending_drop_widget = NULL;
	pending_drop_x = pending_drop_y = 0;
	drop_already_emitted = FALSE;
}

static void
pack_drop_xy (gpointer context, double x, double y)
{
	VerneDropXY *xy;

	if (context == NULL)
		return;
	xy = g_new (VerneDropXY, 1);
	xy->x = (int) x;
	xy->y = (int) y;
	g_object_set_qdata_full (G_OBJECT (context), drop_xy_quark (), xy, g_free);
}

static void
unpack_drop_xy (gpointer context, int *x, int *y)
{
	VerneDropXY *xy;

	if (x)
		*x = 0;
	if (y)
		*y = 0;
	if (context == NULL)
		return;
	xy = g_object_get_qdata (G_OBJECT (context), drop_xy_quark ());
	if (xy == NULL)
		return;
	if (x)
		*x = xy->x;
	if (y)
		*y = xy->y;
}

static void
verne_drop_status_safe (GdkDrop *drop, GdkDragAction preferred)
{
	GdkDragAction actions;

	if (drop == NULL)
		return;
	actions = gdk_drop_get_actions (drop);
	if (actions == 0)
		actions = GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK;
	if (preferred == 0 || (preferred & actions) == 0) {
		if (actions & GDK_ACTION_COPY)
			preferred = GDK_ACTION_COPY;
		else if (actions & GDK_ACTION_MOVE)
			preferred = GDK_ACTION_MOVE;
		else
			preferred = actions;
	}
	gdk_drop_status (drop, actions, preferred);
}

static GdkDragAction
on_async_motion (GtkDropTargetAsync *self, GdkDrop *drop, double x, double y, gpointer data)
{
	GtkWidget *widget = data;
	gboolean handled = FALSE;
	GdkDragAction selected;
	guint32 time = GDK_CURRENT_TIME;

	(void) self;
	g_debug ("drop dest motion at %.0f,%.0f on %s", x, y, G_OBJECT_TYPE_NAME (widget));
	pack_drop_xy (drop, x, y);
	g_signal_emit_by_name (widget, "drag-motion", drop, (int) x, (int) y, time, &handled);
	/* GTK3 replied for a destination registered with GTK_DEST_DEFAULT_MOTION;
	 * such a destination has no drag-motion handler of its own. */
	if (!handled &&
	    (GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (widget), dest_flags_quark ())) & GTK_DEST_DEFAULT_MOTION))
		handled = TRUE;
	selected = (GdkDragAction) GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (drop), selected_action_quark ()));
	if (selected == 0 && handled)
		selected = GDK_ACTION_COPY;
	if (selected)
		verne_drop_status_safe (drop, selected);
	{
		GdkDrag *src_drag = gdk_drop_get_drag (drop);
		if (src_drag) {
			gpointer src = g_object_get_qdata (G_OBJECT (src_drag), source_widget_quark ());
			if (src)
				g_object_set_qdata (G_OBJECT (drop), source_widget_quark (), src);
		}
	}
	if (pending_drop != drop) {
		if (pending_drop)
			g_object_unref (pending_drop);
		pending_drop = g_object_ref (drop);
		/* This latch stops one GdkDrop being delivered twice. It was
		 * only ever cleared by gtk_drag_finish(), so a destination that
		 * never finished a drop - a read that timed out, a drop nothing
		 * accepted - killed drag and drop process-wide until restart.
		 * A new drop means the previous one is over either way. */
		drop_already_emitted = FALSE;
	}
	pending_drop_widget = widget;
	pending_drop_x = x;
	pending_drop_y = y;
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
	if (drop_already_emitted && drop == pending_drop)
		return TRUE;
	drop_already_emitted = TRUE;
	pack_drop_xy (drop, x, y);
	if (g_object_get_qdata (G_OBJECT (drop), selected_action_quark ()) == NULL)
		g_object_set_qdata (G_OBJECT (drop), selected_action_quark (),
				    GINT_TO_POINTER ((int) GDK_ACTION_COPY));
	g_debug ("async drop at %.0f,%.0f on %s selected=%d",
		   x, y, G_OBJECT_TYPE_NAME (widget),
		   GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (drop), selected_action_quark ())));
	g_signal_emit_by_name (widget, "drag-drop", drop, (int) x, (int) y, GDK_CURRENT_TIME, &handled);
	/* GTK3 fetched the data itself for a destination registered with
	 * GTK_DEST_DEFAULT_DROP - the properties window icon, the location bar
	 * and the templates list only connect drag-data-received - so without
	 * this their drops did nothing at all. */
	if (!handled &&
	    (GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (widget), dest_flags_quark ())) & GTK_DEST_DEFAULT_DROP)) {
		GdkAtom target = gtk_drag_dest_find_target (widget, (GdkDragContext *) drop, NULL);

		if (target != NULL) {
			gtk_drag_get_data (widget, (GdkDragContext *) drop, target, GDK_CURRENT_TIME);
			handled = TRUE;
		}
	}
	g_debug ("drag-drop handled=%d", handled);
	return handled;
}

static int
verne_widget_depth (GtkWidget *widget)
{
	int depth = 0;

	while (widget) {
		depth++;
		widget = gtk_widget_get_parent (widget);
	}
	return depth;
}

static char *
verne_widget_path (GtkWidget *widget)
{
	GString *s;
	GtkWidget *w;

	s = g_string_new (NULL);
	for (w = widget; w; w = gtk_widget_get_parent (w)) {
		if (s->len)
			g_string_prepend_c (s, '/');
		g_string_prepend (s, G_OBJECT_TYPE_NAME (w));
	}
	return g_string_free (s, FALSE);
}

static gboolean
verne_target_is_file_transfer (GdkAtom target)
{
	const char *name = (const char *) target;

	return name != NULL &&
	       (g_strcmp0 (name, "text/uri-list") == 0 ||
		g_strcmp0 (name, "x-special/gnome-icon-list") == 0 ||
		g_strcmp0 (name, "x-special/gnome-copied-files") == 0);
}

static gboolean
verne_target_list_find (GtkTargetList *list, GdkAtom target, guint *info)
{
	guint i;

	if (!list || !list->entries)
		return FALSE;
	for (i = 0; i < list->entries->len; i++) {
		GtkTargetEntry *e = &g_array_index (list->entries, GtkTargetEntry, i);
		if (g_strcmp0 (e->target, (const char *) target) == 0) {
			if (info)
				*info = e->info;
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean
verne_point_in_widget (GtkWidget *root, GtkWidget *widget, double px, double py,
		       double *out_x, double *out_y)
{
	graphene_point_t p;
	int width, height;

	if (!GTK_IS_WIDGET (widget) || !gtk_widget_get_mapped (widget) ||
	    !gtk_widget_get_visible (widget))
		return FALSE;
	if (!gtk_widget_compute_point (root, widget,
				       &GRAPHENE_POINT_INIT ((float) px, (float) py), &p))
		return FALSE;
	width = gtk_widget_get_width (widget);
	height = gtk_widget_get_height (widget);
	if (width < 1 || height < 1)
		return FALSE;
	if (p.x < 0 || p.y < 0 || p.x >= (float) width || p.y >= (float) height)
		return FALSE;
	if (out_x)
		*out_x = p.x;
	if (out_y)
		*out_y = p.y;
	return TRUE;
}

/* Pointer coords in a native's widget space using root X11 position.
 * gdk_surface_get_device_position() only succeeds for the surface the
 * pointer is currently in, so the drag ghost window would hide file
 * views and dest motion/highlight would never run. */
static gboolean
verne_native_pointer_widget_coords (GtkNative *native, double *px, double *py)
{
	GdkSurface *surface;
	Display *dpy;
	Window root, child = None;
	int rx = 0, ry = 0, wx = 0, wy = 0, ox = 0, oy = 0;
	unsigned int mask = 0;
	double tx = 0, ty = 0, sx, sy;
	int sw, sh;

	if (native == NULL)
		return FALSE;
	surface = gtk_native_get_surface (native);
	if (surface == NULL || !GDK_IS_X11_SURFACE (surface))
		return FALSE;
	dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	if (dpy == NULL)
		return FALSE;
	if (!XQueryPointer (dpy, DefaultRootWindow (dpy), &root, &child,
			    &rx, &ry, &wx, &wy, &mask))
		return FALSE;
	if (!XTranslateCoordinates (dpy, gdk_x11_surface_get_xid (surface),
				    DefaultRootWindow (dpy), 0, 0, &ox, &oy, &child))
		return FALSE;
	gtk_native_get_surface_transform (native, &tx, &ty);
	sx = (double) rx - (double) ox;
	sy = (double) ry - (double) oy;
	sw = gdk_surface_get_width (surface);
	sh = gdk_surface_get_height (surface);
	if (sx < 0 || sy < 0 || sx >= (double) sw || sy >= (double) sh)
		return FALSE;
	if (px)
		*px = sx - tx;
	if (py)
		*py = sy - ty;
	return TRUE;
}

static GtkWidget *
verne_local_find_dest (VerneLocalDrag *local, double *x, double *y)
{
	GList *toplevels, *l, *d;

	toplevels = gtk_window_list_toplevels ();
	for (l = toplevels; l; l = l->next) {
		GtkWidget *win = l->data;
		GtkNative *native;
		GtkWidget *root, *picked, *w, *best = NULL;
		double px = 0, py = 0, ox = 0, oy = 0, bx = 0, by = 0;
		int best_depth = -1;

		if (win == local->icon_window)
			continue;
		if (GTK_IS_MENU (win))
			continue;
		if (gtk_widget_get_width (win) <= 1 || gtk_widget_get_height (win) <= 1)
			continue;
		native = GTK_NATIVE (win);
		if (!verne_native_pointer_widget_coords (native, &px, &py))
			continue;
		root = GTK_WIDGET (gtk_widget_get_root (win));

		picked = gtk_widget_pick (root, px, py,
					  GTK_PICK_INSENSITIVE | GTK_PICK_NON_TARGETABLE);
		/* Prefer a dest on the pick ancestor chain. A sibling GtkTreeView
		 * (list/compact vs places) can also contain the point via a
		 * transform, and used to steal sidebar drops. */
		for (w = picked; w; w = gtk_widget_get_parent (w)) {
		if (g_object_get_qdata (G_OBJECT (w), dest_targets_quark ()) == NULL)
				continue;
			if (!verne_point_in_widget (root, w, px, py, &ox, &oy))
				continue;
			best = w;
			bx = ox;
			by = oy;
			best_depth = verne_widget_depth (w);
			break;
		}

		if (best == NULL) {
			for (d = verne_drop_dests; d; d = d->next) {
				GtkWidget *dest = d->data;
				int depth;

				if (!GTK_IS_WIDGET (dest) || gtk_widget_get_native (dest) != native)
					continue;
				if (!verne_point_in_widget (root, dest, px, py, &ox, &oy))
					continue;
				depth = verne_widget_depth (dest);
				if (depth >= best_depth) {
					best = dest;
					bx = ox;
					by = oy;
					best_depth = depth;
				}
			}
		}

		if (best) {
			*x = bx;
			*y = by;
			g_list_free (toplevels);
			return best;
		}
	}
	g_list_free (toplevels);
	return NULL;
}

static void
verne_local_move_icon (VerneLocalDrag *local)
{
	Display *dpy;
	Window root, child;
	int rx = 0, ry = 0, wx, wy;
	unsigned int mask = 0;

	if (local->icon_window == NULL)
		return;
	dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	if (dpy == NULL)
		return;
	if (XQueryPointer (dpy, DefaultRootWindow (dpy), &root, &child, &rx, &ry, &wx, &wy, &mask))
		gtk_window_move (GTK_WINDOW (local->icon_window), rx - local->hot_x, ry - local->hot_y);
}

static gboolean
verne_local_button1_down (void)
{
	Display *dpy;
	Window root, child;
	int rx, ry, wx, wy;
	unsigned int mask = 0;

	dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	if (dpy == NULL)
		return TRUE;
	if (!XQueryPointer (dpy, DefaultRootWindow (dpy), &root, &child, &rx, &ry, &wx, &wy, &mask))
		return TRUE;
	return (mask & Button1Mask) != 0;
}

static void
verne_local_update_dest (VerneLocalDrag *local)
{
	double x = 0, y = 0;
	GtkWidget *dest;
	gboolean handled = FALSE;
	GdkDragAction selected;

	dest = verne_local_find_dest (local, &x, &y);
	if (local->current_dest && local->current_dest != dest)
		g_signal_emit_by_name (local->current_dest, "drag-leave", local, GDK_CURRENT_TIME);
	if (dest != local->current_dest) {
		char *path = dest ? verne_widget_path (dest) : g_strdup ("none");

		g_debug ("local dest now %s at %.0f,%.0f", path, x, y);
		g_free (path);
	}
	local->current_dest = dest;
	local->dest_x = x;
	local->dest_y = y;
	if (dest == NULL)
		return;
	g_debug ("local dest motion at %.0f,%.0f on %s", x, y, G_OBJECT_TYPE_NAME (dest));
	pack_drop_xy (local, x, y);
	g_signal_emit_by_name (dest, "drag-motion", local, (int) x, (int) y, GDK_CURRENT_TIME, &handled);
	selected = (GdkDragAction) GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (local), selected_action_quark ()));
	if (selected == 0 && handled) {
		/* drag-motion often gdk_drag_status(0) over empty canvas.
		 * Keep MOVE so dest/file icon rearranges are not COPY no-ops. */
		if (local->selected)
			selected = local->selected;
		else
			selected = GDK_ACTION_COPY;
	}
	if (selected)
		local->selected = selected;
}

static void
verne_xdnd_teardown (VerneLocalDrag *local)
{
	Display *dpy;

	if (local->xdnd_fd_id) {
		g_source_remove (local->xdnd_fd_id);
		local->xdnd_fd_id = 0;
	}
	if (local->xdnd_finish_timeout) {
		g_source_remove (local->xdnd_finish_timeout);
		local->xdnd_finish_timeout = 0;
	}
	dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	if (dpy && local->xdnd_source) {
		gdk_x11_display_error_trap_push (gdk_display_get_default ());
		if (XGetSelectionOwner (dpy, XInternAtom (dpy, "XdndSelection", False)) == local->xdnd_source)
			XSetSelectionOwner (dpy, XInternAtom (dpy, "XdndSelection", False), None,
					    gdk_x11_display_get_user_time (gdk_display_get_default ()));
		if (local->xdnd_created_window)
			XDestroyWindow (dpy, local->xdnd_source);
		XFlush (dpy);
		gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
		local->xdnd_source = None;
		local->xdnd_created_window = FALSE;
	}
	g_clear_pointer (&local->xdnd_atoms, g_free);
	local->n_xdnd_atoms = 0;
	local->xdnd_target = None;
}

static void
verne_local_source_gone (gpointer data, GObject *dead)
{
	VerneLocalDrag *local = data;

	(void) dead;
	local->source = NULL;
	g_clear_handle_id (&local->poll_id, g_source_remove);
	if (!local->drop_emitted) {
		local->drop_emitted = TRUE;
		verne_local_cleanup (local);
	}
}

static gboolean
verne_idle_destroy_window (gpointer data)
{
	GtkWidget *window = data;

	if (GTK_IS_WINDOW (window))
		gtk_window_destroy (GTK_WINDOW (window));
	else if (GTK_IS_WIDGET (window))
		gtk_widget_unparent (window);
	g_object_unref (window);
	return G_SOURCE_REMOVE;
}

static gboolean
verne_idle_finish_local (gpointer data)
{
	VerneLocalDrag *local = data;
	GtkWidget *source;

	g_debug ("idle finish local drag");
	source = local->source;
	if (source && GTK_IS_WIDGET (source) &&
	    g_object_get_data (G_OBJECT (source), "verne-active-drag") == (gpointer) local)
		g_signal_emit_by_name (source, "drag-end", local);
	g_debug ("idle finish after drag-end");
	verne_local_cleanup (local);
	g_debug ("idle finish after cleanup");
	return G_SOURCE_REMOVE;
}

static void
verne_local_cleanup (VerneLocalDrag *local)
{
	GtkWidget *source;

	if (local == NULL)
		return;
	source = local->source;
	g_clear_handle_id (&local->poll_id, g_source_remove);
	if (local->icon_window) {
		GtkWidget *icon = local->icon_window;

		local->icon_window = NULL;
		local->picture = NULL;
		gtk_widget_set_visible (icon, FALSE);
		g_timeout_add_seconds (2, verne_idle_destroy_window, g_object_ref (icon));
	}
	if (source) {
		g_object_weak_unref (G_OBJECT (source), verne_local_source_gone, local);
		g_object_set_data (G_OBJECT (source), "verne-active-drag", NULL);
	}
	verne_xdnd_teardown (local);
	if (local->native_drag) {
		g_signal_handlers_disconnect_by_data (local->native_drag, local);
		g_clear_object (&local->native_drag);
	}
	g_object_unref (local);
}

static void
verne_local_emit_drop (VerneLocalDrag *local)
{
	gboolean handled = FALSE;
	GtkWidget *source;
	GtkWidget *dest;

	if (local->drop_emitted)
		return;
	local->drop_emitted = TRUE;
	if (local->icon_window)
		gtk_widget_set_visible (local->icon_window, FALSE);
	if (local->current_dest == NULL)
		verne_local_update_dest (local);
	source = local->source;
	dest = local->current_dest;
	if (dest) {
		char *path = verne_widget_path (dest);

		g_debug ("completing local drop on %s at %.0f,%.0f",
			   path, local->dest_x, local->dest_y);
		g_free (path);
		pack_drop_xy (local, local->dest_x, local->dest_y);
		g_signal_emit_by_name (dest, "drag-drop", local,
				       (int) local->dest_x, (int) local->dest_y, GDK_CURRENT_TIME, &handled);
		/* Same as the foreign-drop path: a destination registered with
		 * GTK_DEST_DEFAULT_DROP has no drag-drop handler and expects the
		 * data to be fetched for it. */
		if (!handled &&
		    (GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (dest), dest_flags_quark ())) & GTK_DEST_DEFAULT_DROP)) {
			GdkAtom target = gtk_drag_dest_find_target (dest, (GdkDragContext *) local, NULL);

			if (target != NULL) {
				gtk_drag_get_data (dest, (GdkDragContext *) local, target, GDK_CURRENT_TIME);
				handled = TRUE;
			}
		}
		g_debug ("local drag-drop handled=%d", handled);
		g_signal_emit_by_name (dest, "drag-leave", local, GDK_CURRENT_TIME);
	} else {
		g_debug ("local drop with no dest, cancelling");
		if (source)
			g_signal_emit_by_name (source, "drag-failed", local, 0, &handled);
	}
	g_debug ("local drop scheduled idle finish dest=%s",
		   dest ? G_OBJECT_TYPE_NAME (dest) : "none");
	/* Finish outside the drag-drop stack: copy/move UI and highlight
	 * teardown re-enter GTK if they run before drag-drop returns. */
	g_idle_add (verne_idle_finish_local, local);
}

static gboolean
verne_pointer_over_own_toplevel (VerneLocalDrag *local)
{
	GList *toplevels, *l;

	toplevels = gtk_window_list_toplevels ();
	for (l = toplevels; l; l = l->next) {
		GtkWidget *win = l->data;
		double px = 0, py = 0;

		if (win == local->icon_window)
			continue;
		if (GTK_IS_MENU (win))
			continue;
		/* Hidden dummy 1×1 natives (titled "Verne") would otherwise
		 * look like the pointer is inside them when device-position fails. */
		if (gtk_widget_get_width (win) <= 1 || gtk_widget_get_height (win) <= 1)
			continue;
		if (verne_native_pointer_widget_coords (GTK_NATIVE (win), &px, &py)) {
			g_list_free (toplevels);
			return TRUE;
		}
	}
	g_list_free (toplevels);
	return FALSE;
}

static gboolean
verne_window_is_xdnd_aware (Display *dpy, Window w)
{
	Atom xdnd_aware = XInternAtom (dpy, "XdndAware", False);
	Atom actual = None;
	int format = 0;
	unsigned long n = 0, bytes = 0;
	unsigned char *prop = NULL;
	gboolean aware = FALSE;

	if (XGetWindowProperty (dpy, w, xdnd_aware, 0, 1, False, AnyPropertyType,
				&actual, &format, &n, &bytes, &prop) == Success && n > 0)
		aware = TRUE;
	if (prop)
		XFree (prop);
	return aware;
}

static gboolean
verne_tree_contains_xid (Display *dpy, Window w, GHashTable *skip)
{
	Window root = None, parent = None, *children = NULL;
	unsigned int nchild = 0, i;

	if (g_hash_table_contains (skip, GUINT_TO_POINTER (w)))
		return TRUE;
	if (!XQueryTree (dpy, w, &root, &parent, &children, &nchild))
		return FALSE;
	for (i = 0; i < nchild; i++) {
		if (verne_tree_contains_xid (dpy, children[i], skip)) {
			XFree (children);
			return TRUE;
		}
	}
	if (children)
		XFree (children);
	return FALSE;
}

static Window
verne_find_xdnd_in_tree (Display *dpy, Window w)
{
	Window root = None, parent = None, *children = NULL;
	unsigned int nchild = 0, i;
	Window found = None;

	if (verne_window_is_xdnd_aware (dpy, w))
		return w;
	if (!XQueryTree (dpy, w, &root, &parent, &children, &nchild))
		return None;
	for (i = nchild; i > 0; i--) {
		found = verne_find_xdnd_in_tree (dpy, children[i - 1]);
		if (found != None)
			break;
	}
	if (children)
		XFree (children);
	return found;
}

static void
verne_skip_add_native (GHashTable *skip, GtkWidget *widget)
{
	GtkNative *native;
	GdkSurface *surf;

	if (widget == NULL)
		return;
	native = gtk_widget_get_native (widget);
	surf = native ? gtk_native_get_surface (native) : NULL;
	if (surf)
		g_hash_table_add (skip, GUINT_TO_POINTER (gdk_x11_surface_get_xid (surf)));
}

static void
verne_xdnd_collect_skip (GHashTable *skip, VerneLocalDrag *local)
{
	GList *toplevels, *l;

	if (local->xdnd_source)
		g_hash_table_add (skip, GUINT_TO_POINTER (local->xdnd_source));
	verne_skip_add_native (skip, local->icon_window);
	verne_skip_add_native (skip, local->source);
	/* Skip dummy 1×1 natives only. Other same-process windows (a second
	 * file window) must remain XDND targets; skipping every toplevel
	 * sent those drops through to nemo-desktop underneath. */
	toplevels = gtk_window_list_toplevels ();
	for (l = toplevels; l; l = l->next) {
		GtkWidget *win = l->data;

		if (gtk_widget_get_width (win) <= 1 || gtk_widget_get_height (win) <= 1)
			verne_skip_add_native (skip, win);
	}
	g_list_free (toplevels);
}

static gchar *
verne_xid_wm_class (Display *dpy, Window w)
{
	XClassHint hint = { 0 };
	Window root = None, parent = None, cur = w, *children = NULL;
	unsigned int nchild = 0;
	gchar *out = NULL;
	int hops;

	gdk_x11_display_error_trap_push (gdk_display_get_default ());
	for (hops = 0; hops < 16 && cur != None; hops++) {
		if (XGetClassHint (dpy, cur, &hint)) {
			const char *a = hint.res_class ? hint.res_class : "";
			const char *b = hint.res_name ? hint.res_name : "";
			out = g_strdup_printf ("%s %s", a, b);
			if (hint.res_name)
				XFree (hint.res_name);
			if (hint.res_class)
				XFree (hint.res_class);
			break;
		}
		children = NULL;
		nchild = 0;
		if (!XQueryTree (dpy, cur, &root, &parent, &children, &nchild))
			break;
		if (children)
			XFree (children);
		if (parent == None || parent == cur || parent == root)
			break;
		cur = parent;
	}
	gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
	return out;
}

static gboolean
verne_wm_class_is_verne_desktop (const gchar *wmclass)
{
	gchar *folded;
	gboolean match;

	if (wmclass == NULL || wmclass[0] == '\0')
		return FALSE;
	folded = g_ascii_strdown (wmclass, -1);
	match = strstr (folded, "nemo-desktop") != NULL ||
		strstr (folded, "verne-desktop") != NULL;
	g_free (folded);
	return match;
}

static gboolean
verne_wm_class_is_foreign_desktop (const gchar *wmclass)
{
	gchar *folded;
	gboolean match;

	if (wmclass == NULL || wmclass[0] == '\0')
		return FALSE;
	folded = g_ascii_strdown (wmclass, -1);
	match = strstr (folded, "xfdesktop") != NULL ||
		strstr (folded, "nautilus-desktop") != NULL ||
		strstr (folded, "pcmanfm") != NULL;
	g_free (folded);
	return match;
}

static Window
verne_xdnd_target_at_pointer (Display *dpy, VerneLocalDrag *local, int *root_x, int *root_y)
{
	Window root, root_ret, parent, *children = NULL;
	Window target = None, fallback = None;
	int rx = 0, ry = 0, wx = 0, wy = 0;
	unsigned int mask = 0, nchild = 0, i;
	GHashTable *skip;

	root = DefaultRootWindow (dpy);
	if (!XQueryPointer (dpy, root, &root_ret, &parent, &rx, &ry, &wx, &wy, &mask))
		return None;
	if (root_x)
		*root_x = rx;
	if (root_y)
		*root_y = ry;
	skip = g_hash_table_new (g_direct_hash, g_direct_equal);
	verne_xdnd_collect_skip (skip, local);
	if (!XQueryTree (dpy, root, &root_ret, &parent, &children, &nchild) || children == NULL) {
		g_hash_table_destroy (skip);
		return None;
	}
	for (i = nchild; i > 0; i--) {
		Window w = children[i - 1];
		Window found;
		XWindowAttributes attr;
		gchar *wmclass;

		if (!XGetWindowAttributes (dpy, w, &attr) || attr.map_state != IsViewable)
			continue;
		if (attr.width <= 1 || attr.height <= 1)
			continue;
		if (rx < attr.x || ry < attr.y || rx >= attr.x + attr.width || ry >= attr.y + attr.height)
			continue;
		if (verne_tree_contains_xid (dpy, w, skip))
			continue;
		found = verne_find_xdnd_in_tree (dpy, w);
		if (found == None)
			continue;
		wmclass = verne_xid_wm_class (dpy, found);
		/* xfdesktop (and similar) often sit above Verne dest. Skip them
		 * so dest still receives drops. Other apps stacked above dest
		 * (Thunar, etc.) must win — dest is a full-screen canvas. */
		if (verne_wm_class_is_foreign_desktop (wmclass)) {
			if (fallback == None)
				fallback = found;
			g_free (wmclass);
			continue;
		}
		g_free (wmclass);
		target = found;
		break;
	}
	XFree (children);
	g_hash_table_destroy (skip);
	if (target != None)
		return target;
	return fallback;
}

static void
verne_xdnd_client_message (Display *dpy, Window target, Atom type, long l0, long l1, long l2, long l3, long l4)
{
	XClientMessageEvent ev = { 0 };

	if (target == None)
		return;
	ev.type = ClientMessage;
	ev.window = target;
	ev.message_type = type;
	ev.format = 32;
	ev.data.l[0] = l0;
	ev.data.l[1] = l1;
	ev.data.l[2] = l2;
	ev.data.l[3] = l3;
	ev.data.l[4] = l4;
	gdk_x11_display_error_trap_push (gdk_display_get_default ());
	XSendEvent (dpy, target, False, NoEventMask, (XEvent *) &ev);
	XFlush (dpy);
	gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
}

static Atom
verne_xdnd_action_atom (Display *dpy, VerneLocalDrag *local)
{
	if (local->selected & GDK_ACTION_MOVE)
		return XInternAtom (dpy, "XdndActionMove", False);
	return XInternAtom (dpy, "XdndActionCopy", False);
}

static void
verne_xdnd_send_leave (VerneLocalDrag *local)
{
	Display *dpy;

	if (local->xdnd_target == None || local->xdnd_source == None)
		return;
	dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	verne_xdnd_client_message (dpy, local->xdnd_target,
				   XInternAtom (dpy, "XdndLeave", False),
				   (long) local->xdnd_source, 0, 0, 0, 0);
	local->xdnd_target = None;
	local->xdnd_accepted = FALSE;
}

static void
verne_xdnd_send_enter (VerneLocalDrag *local, Window target)
{
	Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	long flags = 5L << 24;

	if (local->n_xdnd_atoms > 3)
		flags |= 1;
	verne_xdnd_client_message (dpy, target,
				   XInternAtom (dpy, "XdndEnter", False),
				   (long) local->xdnd_source, flags,
				   local->n_xdnd_atoms > 0 ? (long) local->xdnd_atoms[0] : 0,
				   local->n_xdnd_atoms > 1 ? (long) local->xdnd_atoms[1] : 0,
				   local->n_xdnd_atoms > 2 ? (long) local->xdnd_atoms[2] : 0);
	local->xdnd_target = target;
	local->xdnd_accepted = FALSE;
}

static void
verne_xdnd_send_position (VerneLocalDrag *local)
{
	Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	int rx = 0, ry = 0;
	Window target;
	guint32 time;

	if (local->xdnd_source == None)
		return;
	target = verne_xdnd_target_at_pointer (dpy, local, &rx, &ry);
	if (target != local->xdnd_target) {
		if (local->xdnd_target != None)
			verne_xdnd_send_leave (local);
		if (target != None)
			verne_xdnd_send_enter (local, target);
	}
	if (local->xdnd_target == None)
		return;
	time = gdk_x11_display_get_user_time (gdk_display_get_default ());
	verne_xdnd_client_message (dpy, local->xdnd_target,
				   XInternAtom (dpy, "XdndPosition", False),
				   (long) local->xdnd_source, 0,
				   ((long) rx << 16) | (ry & 0xffff),
				   (long) time,
				   (long) verne_xdnd_action_atom (dpy, local));
}

static void
verne_xdnd_handle_selection_request (VerneLocalDrag *local, const XSelectionRequestEvent *req)
{
	Display *dpy = req->display;
	XSelectionEvent notify = { 0 };
	gboolean success = FALSE;
	char *name;

	notify.type = SelectionNotify;
	notify.serial = 0;
	notify.send_event = True;
	notify.display = dpy;
	notify.requestor = req->requestor;
	notify.selection = req->selection;
	notify.target = req->target;
	notify.property = req->property;
	notify.time = req->time;

	gdk_x11_display_error_trap_push (gdk_display_get_default ());
	name = XGetAtomName (dpy, req->target);
	if (name && g_strcmp0 (name, "TARGETS") == 0) {
		XChangeProperty (dpy, req->requestor, req->property, XA_ATOM, 32,
				 PropModeReplace, (unsigned char *) local->xdnd_atoms,
				 (int) local->n_xdnd_atoms);
		success = TRUE;
	} else if (name && local->source) {
		GtkSelectionData sel = { 0 };
		guint info;
		const char *mime = name;

		if (g_strcmp0 (name, "STRING") == 0 || g_strcmp0 (name, "TEXT") == 0 ||
		    g_strcmp0 (name, "UTF8_STRING") == 0 || g_strcmp0 (name, "text/plain") == 0)
			mime = "text/uri-list";
		sel.target = (GdkAtom) mime;
		sel.type = sel.target;
		sel.format = 8;
		info = info_for_target (local->targets, sel.target);
		g_signal_emit_by_name (local->source, "drag-data-get", local, &sel, info, req->time);
	g_debug ("xdnd SelectionRequest from=0x%lx mime=%s len=%d data=%.*s",
		   (unsigned long) req->requestor, mime, sel.length, MAX (sel.length, 0),
		   sel.data ? (const char *) sel.data : "");
		if (sel.data && sel.length > 0) {
			XChangeProperty (dpy, req->requestor, req->property, req->target,
					 sel.format ? sel.format : 8, PropModeReplace,
					 sel.data, sel.length);
			success = TRUE;
			local->xdnd_served_data = TRUE;
			local->xdnd_accepted = TRUE;
		}
		g_free (sel.data);
	}
	if (name)
		XFree (name);
	if (!success)
		notify.property = None;
	XSendEvent (dpy, req->requestor, False, NoEventMask, (XEvent *) &notify);
	XFlush (dpy);
	gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
}

static Bool
verne_xdnd_event_pred (Display *dpy, XEvent *ev, XPointer arg)
{
	Window source = (Window) (guintptr) arg;
	Atom status, finished;

	(void) dpy;
	if (ev->xany.window != source)
		return False;
	if (ev->type == SelectionRequest)
		return True;
	if (ev->type != ClientMessage)
		return False;
	status = XInternAtom (dpy, "XdndStatus", False);
	finished = XInternAtom (dpy, "XdndFinished", False);
	return ev->xclient.message_type == status || ev->xclient.message_type == finished;
}

static void
verne_xdnd_pump (VerneLocalDrag *local)
{
	Display *dpy;
	XEvent ev;

	if (local->xdnd_source == None)
		return;
	dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	while (XCheckIfEvent (dpy, &ev, verne_xdnd_event_pred, (XPointer) (guintptr) local->xdnd_source)) {
		if (ev.type == SelectionRequest) {
			verne_xdnd_handle_selection_request (local, &ev.xselectionrequest);
			continue;
		}
		if (ev.type == ClientMessage) {
			const char *atom_name = NULL;

			if (ev.xclient.message_type)
				atom_name = gdk_x11_get_xatom_name_for_display (gdk_display_get_default (), ev.xclient.message_type);
			if (g_strcmp0 (atom_name, "XdndStatus") == 0) {
				local->xdnd_accepted = (ev.xclient.data.l[1] & 1) != 0;
				g_debug ("xdnd status accept=%d", local->xdnd_accepted);
			} else if (g_strcmp0 (atom_name, "XdndFinished") == 0) {
				g_debug ("xdnd finished accept=%ld", ev.xclient.data.l[1] & 1);
				local->xdnd_finished = TRUE;
			}
		}
	}
}

static gboolean
verne_xdnd_xfd (gint fd, GIOCondition cond, gpointer data)
{
	(void) fd;
	(void) cond;
	verne_xdnd_pump (data);
	return G_SOURCE_CONTINUE;
}

static gboolean
verne_xdnd_finish_timeout (gpointer data)
{
	VerneLocalDrag *local = data;
	GtkWidget *source = local->source;
	gboolean handled = FALSE;

	local->xdnd_finish_timeout = 0;
	if (local->drop_emitted)
		return G_SOURCE_REMOVE;
	local->drop_emitted = TRUE;
	g_debug ("xdnd finish timeout, accepted=%d served=%d",
		   local->xdnd_accepted, local->xdnd_served_data);
	if (source) {
		if (!local->xdnd_accepted && !local->xdnd_served_data)
			g_signal_emit_by_name (source, "drag-failed", local, 0, &handled);
		g_signal_emit_by_name (source, "drag-end", local);
	}
	verne_local_cleanup (local);
	return G_SOURCE_REMOVE;
}

static void
verne_xdnd_wait_status (VerneLocalDrag *local, int timeout_ms)
{
	gint64 deadline;
	gint64 last_pos = 0;

	deadline = g_get_monotonic_time () + (gint64) timeout_ms * 1000;
	while (g_get_monotonic_time () < deadline) {
		verne_xdnd_pump (local);
		if (local->xdnd_accepted || local->xdnd_finished)
			return;
		if (g_get_monotonic_time () - last_pos > 40000) {
			verne_xdnd_send_position (local);
			last_pos = g_get_monotonic_time ();
		}
		g_usleep (2000);
	}
	verne_xdnd_pump (local);
}

static void
verne_copy_uris_to_desktop (VerneLocalDrag *local)
{
	GtkSelectionData sel = { 0 };
	gchar **uris;
	const char *desk;
	GFile *ddir;
	guint info, i;
	GdkDragAction action;

	if (local->source == NULL)
		return;
	sel.target = gdk_atom_intern ("text/uri-list", FALSE);
	sel.type = sel.target;
	sel.format = 8;
	info = info_for_target (local->targets, sel.target);
	g_signal_emit_by_name (local->source, "drag-data-get", local, &sel, info, GDK_CURRENT_TIME);
	uris = gtk_selection_data_get_uris (&sel);
	g_free (sel.data);
	if (uris == NULL || uris[0] == NULL) {
		g_strfreev (uris);
		return;
	}
	desk = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
	if (desk == NULL || desk[0] == '\0')
		desk = g_get_home_dir ();
	ddir = g_file_new_for_path (desk);
	action = local->selected ? local->selected : GDK_ACTION_COPY;
	for (i = 0; uris[i] != NULL; i++) {
		GFile *src, *dst;
		char *base;
		GError *err = NULL;
		gboolean ok;

		src = g_file_new_for_uri (uris[i]);
		base = g_file_get_basename (src);
		if (base == NULL || base[0] == '\0') {
			g_object_unref (src);
			g_free (base);
			continue;
		}
		dst = g_file_get_child (ddir, base);
		if (action & GDK_ACTION_MOVE)
			ok = g_file_move (src, dst, G_FILE_COPY_NONE, NULL, NULL, NULL, &err);
		else
			ok = g_file_copy (src, dst, G_FILE_COPY_NONE, NULL, NULL, NULL, &err);
		g_debug ("desktop drop %s %s -> %s ok=%d err=%s",
			   (action & GDK_ACTION_MOVE) ? "move" : "copy",
			   uris[i], base, ok, err ? err->message : "none");
		g_clear_error (&err);
		g_object_unref (src);
		g_object_unref (dst);
		g_free (base);
	}
	g_object_unref (ddir);
	g_strfreev (uris);
	local->xdnd_accepted = TRUE;
	local->xdnd_served_data = TRUE;
}

static void
verne_xdnd_send_drop (VerneLocalDrag *local)
{
	Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	guint32 time;
	gchar *wmclass;

	if (local->xdnd_target == None)
		verne_xdnd_send_position (local);
	verne_xdnd_pump (local);
	verne_xdnd_wait_status (local, 500);
	if (local->xdnd_target == None || local->xdnd_source == None) {
		g_debug ("XdndDrop skipped, no target");
		return;
	}
	wmclass = verne_xid_wm_class (dpy, local->xdnd_target);
	if (verne_wm_class_is_verne_desktop (wmclass)) {
		/* End dest DnD first. Copying before Leave lets dest teardown
		 * mark the new NemoFile gone, so the icon never sticks.
		 * Prefer COPY so dest sees a created-file event. */
		local->selected = GDK_ACTION_COPY;
		verne_xdnd_send_leave (local);
		verne_xdnd_pump (local);
		g_usleep (150000);
		verne_copy_uris_to_desktop (local);
		local->xdnd_dropped = TRUE;
		local->xdnd_finished = TRUE;
		g_debug ("desktop drop finished without XdndDrop class=%s",
			   wmclass ? wmclass : "?");
		g_free (wmclass);
		if (local->xdnd_finish_timeout)
			g_source_remove (local->xdnd_finish_timeout);
		local->xdnd_finish_timeout = g_timeout_add (50, verne_xdnd_finish_timeout, local);
		return;
	}
	time = gdk_x11_display_get_user_time (gdk_display_get_default ());
	verne_xdnd_client_message (dpy, local->xdnd_target,
				   XInternAtom (dpy, "XdndDrop", False),
				   (long) local->xdnd_source, 0, (long) time, 0, 0);
	local->xdnd_dropped = TRUE;
	g_debug ("sent XdndDrop to 0x%lx from 0x%lx accepted=%d class=%s",
		   (unsigned long) local->xdnd_target, (unsigned long) local->xdnd_source,
		   local->xdnd_accepted, wmclass ? wmclass : "?");
	g_free (wmclass);
	if (local->xdnd_finish_timeout)
		g_source_remove (local->xdnd_finish_timeout);
	local->xdnd_finish_timeout = g_timeout_add_seconds (3, verne_xdnd_finish_timeout, local);
}

static void
verne_local_handover_native (VerneLocalDrag *local)
{
	Display *dpy;
	GdkDisplay *gdk_dpy;
	GtkNative *native;
	GdkSurface *surface;
	Atom xdnd_aware, xdnd_sel, xdnd_typelist, xdnd_actionlist;
	unsigned long version = 5;
	unsigned long actions[3];
	guint32 time;
	guint i;

	if (local->handed_over || local->source == NULL)
		return;
	gdk_dpy = gdk_display_get_default ();
	dpy = GDK_DISPLAY_XDISPLAY (gdk_dpy);
	if (dpy == NULL)
		return;

	native = gtk_widget_get_native (local->source);
	surface = native ? gtk_native_get_surface (native) : NULL;
	(void) surface;
	/* Always use a dedicated source window. If XdndSelection is owned by
	 * the GTK4 file-manager surface, GDK swallows XdndStatus and
	 * SelectionRequest and the foreign drop target never gets data. */
	{
		XSetWindowAttributes swa;

		swa.override_redirect = True;
		swa.event_mask = PropertyChangeMask | StructureNotifyMask;
		gdk_x11_display_error_trap_push (gdk_dpy);
		local->xdnd_source = XCreateWindow (dpy, DefaultRootWindow (dpy),
						    0, 0, 1, 1, 0, CopyFromParent, InputOutput,
						    CopyFromParent, CWOverrideRedirect | CWEventMask, &swa);
		XMapRaised (dpy, local->xdnd_source);
		local->xdnd_created_window = TRUE;
		gdk_x11_display_error_trap_pop_ignored (gdk_dpy);
	}

	gdk_x11_display_error_trap_push (gdk_dpy);
	xdnd_aware = XInternAtom (dpy, "XdndAware", False);
	XChangeProperty (dpy, local->xdnd_source, xdnd_aware, XA_ATOM, 32,
			 PropModeReplace, (unsigned char *) &version, 1);

	local->n_xdnd_atoms = local->targets && local->targets->entries ? local->targets->entries->len : 0;
	if (local->n_xdnd_atoms == 0) {
		local->n_xdnd_atoms = 1;
		local->xdnd_atoms = g_new0 (unsigned long, 1);
		local->xdnd_atoms[0] = XInternAtom (dpy, "text/uri-list", False);
	} else {
		local->xdnd_atoms = g_new0 (unsigned long, local->n_xdnd_atoms);
		for (i = 0; i < local->n_xdnd_atoms; i++) {
			GtkTargetEntry *e = &g_array_index (local->targets->entries, GtkTargetEntry, i);
			local->xdnd_atoms[i] = XInternAtom (dpy, e->target ? e->target : "text/uri-list", False);
		}
	}
	xdnd_typelist = XInternAtom (dpy, "XdndTypeList", False);
	XChangeProperty (dpy, local->xdnd_source, xdnd_typelist, XA_ATOM, 32,
			 PropModeReplace, (unsigned char *) local->xdnd_atoms, (int) local->n_xdnd_atoms);
	actions[0] = XInternAtom (dpy, "XdndActionCopy", False);
	actions[1] = XInternAtom (dpy, "XdndActionMove", False);
	actions[2] = XInternAtom (dpy, "XdndActionLink", False);
	xdnd_actionlist = XInternAtom (dpy, "XdndActionList", False);
	XChangeProperty (dpy, local->xdnd_source, xdnd_actionlist, XA_ATOM, 32,
			 PropModeReplace, (unsigned char *) actions, 3);

	time = gdk_x11_display_get_user_time (gdk_dpy);
	xdnd_sel = XInternAtom (dpy, "XdndSelection", False);
	XSetSelectionOwner (dpy, xdnd_sel, local->xdnd_source, time);
	XFlush (dpy);
	gdk_x11_display_error_trap_pop_ignored (gdk_dpy);

	if (XGetSelectionOwner (dpy, xdnd_sel) != local->xdnd_source) {
		g_warning ("failed to own XdndSelection");
		verne_xdnd_teardown (local);
		return;
	}

	local->xdnd_fd_id = g_unix_fd_add_full (G_PRIORITY_HIGH,
						ConnectionNumber (dpy), G_IO_IN,
						verne_xdnd_xfd, local, NULL);
	local->handed_over = TRUE;
	if ((local->actions & GDK_ACTION_COPY) && !(local->selected & GDK_ACTION_MOVE))
		local->selected = GDK_ACTION_COPY;
	verne_xdnd_send_position (local);
	g_debug ("handed local drag over to XDND source=0x%lx types=%u created=%d",
		   (unsigned long) local->xdnd_source, local->n_xdnd_atoms, local->xdnd_created_window);
}

static gboolean
verne_local_poll (gpointer data)
{
	VerneLocalDrag *local = data;
	GtkWidget *source;

	if (local == NULL || !VERNE_IS_LOCAL_DRAG (local) || local->drop_emitted) {
		if (local)
			local->poll_id = 0;
		return G_SOURCE_REMOVE;
	}
	source = local->source;
	if (source == NULL || !GTK_IS_WIDGET (source)) {
		local->poll_id = 0;
		return G_SOURCE_REMOVE;
	}
	if (local->handed_over) {
		verne_local_move_icon (local);
		if (!verne_pointer_over_own_toplevel (local)) {
			verne_xdnd_pump (local);
			if (local->xdnd_finished) {
				local->poll_id = 0;
				local->drop_emitted = TRUE;
				g_signal_emit_by_name (source, "drag-end", local);
				verne_local_cleanup (local);
				return G_SOURCE_REMOVE;
			}
			if (!local->xdnd_dropped) {
				if (!verne_local_button1_down ()) {
					verne_xdnd_send_drop (local);
				} else {
					verne_xdnd_send_position (local);
				}
			}
			return G_SOURCE_CONTINUE;
		}
		if (local->xdnd_target != None) {
			g_debug ("reclaiming local drag from XDND target=0x%lx",
				   (unsigned long) local->xdnd_target);
			verne_xdnd_send_leave (local);
		}
		local->handed_over = FALSE;
	}
	verne_local_move_icon (local);
	verne_local_update_dest (local);
	if (local->current_dest == NULL && !verne_pointer_over_own_toplevel (local))
		verne_local_handover_native (local);
	if (local->handed_over)
		return G_SOURCE_CONTINUE;
	if (!verne_local_button1_down ()) {
		local->poll_id = 0;
		g_debug ("local DnD poll saw button1 up, dropping");
		verne_local_emit_drop (local);
		return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}

void
verne_dnd_local_motion (GtkWidget *widget)
{
	VerneLocalDrag *local;

	if (widget == NULL)
		return;
	local = g_object_get_data (G_OBJECT (widget), "verne-active-drag");
	if (local == NULL || !VERNE_IS_LOCAL_DRAG (local) || local->drop_emitted)
		return;
	verne_local_move_icon (local);
	verne_local_update_dest (local);
}

void
verne_dnd_gesture_end (GtkWidget *widget)
{
	gpointer drag;

	if (widget == NULL)
		return;
	drag = g_object_get_data (G_OBJECT (widget), "verne-active-drag");
	if (drag == NULL)
		return;
	if (VERNE_IS_LOCAL_DRAG (drag)) {
		VerneLocalDrag *local = drag;

		/* GtkGestureDrag can end when the pointer leaves the source
		 * widget even while button1 is still down. Do not treat that
		 * as a cancelled drop — hand over to native XDND instead. */
		if (verne_local_button1_down () && !local->handed_over) {
			verne_local_update_dest (local);
			if (local->current_dest == NULL && !verne_pointer_over_own_toplevel (local))
				verne_local_handover_native (local);
			return;
		}
		if (local->handed_over)
			return;
		/* Button already up: if we never hit a local dest, still try XDND
		 * so a drop over another app is not cancelled as dest=none. */
		if (local->current_dest == NULL && !verne_pointer_over_own_toplevel (local)) {
			verne_local_handover_native (local);
			if (local->handed_over) {
				verne_xdnd_send_drop (local);
				return;
			}
		}
		verne_local_emit_drop (local);
		return;
	}
	if (drop_already_emitted)
		return;
	if (pending_drop == NULL || pending_drop_widget == NULL)
		return;
	g_debug ("completing drop from gesture-end on %s -> %s",
		 G_OBJECT_TYPE_NAME (widget), G_OBJECT_TYPE_NAME (pending_drop_widget));
	on_async_drop (NULL, pending_drop, pending_drop_x, pending_drop_y, pending_drop_widget);
}

static void
verne_dnd_cleanup_source (GtkWidget *widget)
{
	gpointer drag;

	if (widget == NULL)
		return;
	drag = g_object_get_data (G_OBJECT (widget), "verne-active-drag");
	if (drag && VERNE_IS_LOCAL_DRAG (drag)) {
		verne_local_cleanup (drag);
		return;
	}
	g_object_set_data (G_OBJECT (widget), "verne-active-drag", NULL);
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
	GtkWidget *native;
	gpointer existing;

	g_object_set_qdata (G_OBJECT (widget), dest_flags_quark (), GINT_TO_POINTER ((int) flags));
	list = gtk_target_list_new (targets, (guint) MAX (n_targets, 0));
	g_object_set_qdata_full (G_OBJECT (widget), dest_targets_quark (), list, (GDestroyNotify) gtk_target_list_unref);
	formats = formats_from_entries (targets, n_targets);
	verne_drop_dest_register (widget);
	verne_schedule_native_drop_forwarder (widget);

	/* A native-window GtkDropTargetAsync already owns GdkDrop. A second
	 * controller on the dest widget keeps GTK's self->drop after
	 * gdk_drop_finish and trips gtk_drop_target_async_handle_event
	 * (self->drop == drop) on the next foreign drop. dest_targets qdata
	 * still marks dests so local pick finds them. */
	native = gtk_widget_get_native (widget) ? GTK_WIDGET (gtk_widget_get_native (widget)) : NULL;
	if (native == NULL ||
	    (GTK_IS_WINDOW (native) && !GTK_IS_MENU (native) &&
	     g_object_get_qdata (G_OBJECT (native), forwarder_quark ()))) {
		existing = g_object_get_qdata (G_OBJECT (widget), dest_quark ());
		if (GTK_IS_DROP_TARGET_ASYNC (existing))
			gtk_widget_remove_controller (widget, GTK_EVENT_CONTROLLER (existing));
		g_object_set_qdata (G_OBJECT (widget), dest_quark (), NULL);
		gdk_content_formats_unref (formats);
		return;
	}

	existing = g_object_get_qdata (G_OBJECT (widget), dest_quark ());
	async = GTK_IS_DROP_TARGET_ASYNC (existing) ? existing : NULL;
	if (async == NULL) {
		/* gtk_drop_target_async_new () takes ownership of formats. */
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
		gdk_content_formats_unref (formats);
	}
}

void
gtk_drag_dest_unset (GtkWidget *widget)
{
	gpointer async = g_object_get_qdata (G_OBJECT (widget), dest_quark ());
	if (GTK_IS_DROP_TARGET_ASYNC (async))
		gtk_widget_remove_controller (widget, GTK_EVENT_CONTROLLER (async));
	g_object_set_qdata (G_OBJECT (widget), dest_quark (), NULL);
	g_object_set_qdata (G_OBJECT (widget), dest_targets_quark (), NULL);
	verne_drop_dest_unregister (widget);
}

void
gtk_drag_dest_set_target_list (GtkWidget *widget, GtkTargetList *list)
{
	gpointer async;
	GdkContentFormats *formats;

	if (list)
		gtk_target_list_ref (list);
	g_object_set_qdata_full (G_OBJECT (widget), dest_targets_quark (), list, (GDestroyNotify) gtk_target_list_unref);
	async = g_object_get_qdata (G_OBJECT (widget), dest_quark ());
	if (GTK_IS_DROP_TARGET_ASYNC (async) && list) {
		formats = formats_from_list (list);
		gtk_drop_target_async_set_formats (GTK_DROP_TARGET_ASYNC (async), formats);
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
	gboolean free_formats = FALSE;
	GdkAtom result = NULL;
	guint i;

	if (list == NULL)
		list = gtk_drag_dest_get_target_list (widget);
	if (context && VERNE_IS_LOCAL_DRAG (context)) {
		formats = formats_from_list (((VerneLocalDrag *) context)->targets);
		free_formats = TRUE;
	} else if (context && GDK_IS_DROP (context))
		formats = gdk_drop_get_formats (GDK_DROP (context));
	else if (context && GDK_IS_DRAG (context))
		formats = gdk_drag_get_formats (GDK_DRAG (context));
	if (list == NULL || list->entries == NULL) {
		if (free_formats && formats)
			gdk_content_formats_unref (formats);
		return gdk_atom_intern ("text/uri-list", FALSE);
	}
	for (i = 0; i < list->entries->len; i++) {
		GtkTargetEntry *e = &g_array_index (list->entries, GtkTargetEntry, i);
		gboolean match;

		if (g_strcmp0 (e->target, "GTK_TREE_MODEL_ROW") == 0 &&
		    formats != NULL &&
		    !gdk_content_formats_contain_mime_type (formats, e->target))
			continue;
		if (formats == NULL)
			match = verne_target_is_file_transfer ((GdkAtom) e->target) ||
				g_strcmp0 (e->target, "GTK_TREE_MODEL_ROW") != 0;
		else
			match = gdk_content_formats_contain_mime_type (formats, e->target);
		if (match) {
			result = (GdkAtom) e->target;
			break;
		}
	}
	if (result == NULL)
		result = gdk_atom_intern ("text/uri-list", FALSE);
	if (free_formats && formats)
		gdk_content_formats_unref (formats);
	return result;
}

typedef struct {
	GtkWidget *widget;
	GdkDrop *drop;
	gint x, y;
	guint info;
	guint32 time;
	char *mime;
	GCancellable *cancellable;
	GInputStream *stream;
	GOutputStream *mem;
	guint timeout_id;
	int refs;
	gboolean completed;
} VerneDropRead;

static void
drop_read_free (VerneDropRead *rd)
{
	if (rd->timeout_id != 0) {
		g_source_remove (rd->timeout_id);
		rd->timeout_id = 0;
	}
	if (rd->drop && g_object_get_qdata (G_OBJECT (rd->drop), drop_read_quark ()) == rd)
		g_object_set_qdata (G_OBJECT (rd->drop), drop_read_quark (), NULL);
	g_clear_object (&rd->stream);
	g_clear_object (&rd->mem);
	g_clear_object (&rd->cancellable);
	g_clear_object (&rd->widget);
	g_clear_object (&rd->drop);
	g_free (rd->mime);
	g_free (rd);
}

static void
drop_read_unref (VerneDropRead *rd)
{
	if (rd == NULL)
		return;
	if (--rd->refs > 0)
		return;
	drop_read_free (rd);
}

static void
drop_deliver_bytes (GtkWidget *widget, GdkDrop *drop, const char *mime,
		    GBytes *bytes, int x, int y, guint info, guint32 time)
{
	GtkSelectionData sel = { 0 };
	gsize n = 0;
	guchar *copy = NULL;
	char *mime_copy;

	/* gtk_drag_finish from the receive handler frees drop qdata
	 * (including the mime string). Keep a local copy for sel.target
	 * and the log. GTK3 selection payloads are NUL-terminated; GBytes
	 * from gdk_drop_read are not, and uri-list parsers treat them as C
	 * strings. */
	mime_copy = g_strdup (mime && mime[0] ? mime : "text/uri-list");
	sel.target = (GdkAtom) mime_copy;
	sel.type = sel.target;
	sel.format = 8;
	if (bytes) {
		const guint8 *data = g_bytes_get_data (bytes, &n);

		copy = g_malloc (n + 1);
		if (n > 0)
			memcpy (copy, data, n);
		copy[n] = '\0';
		sel.data = copy;
	}
	sel.length = (gint) n;
	if (widget && GTK_IS_WIDGET (widget)) {
		g_signal_emit_by_name (widget, "drag-data-received",
				       drop, x, y, &sel, info, time);
	}
	g_debug ("drag-data-received mime=%s len=%d info=%u dest=%s xy=%d,%d",
		   mime_copy, sel.length, info,
		   widget ? G_OBJECT_TYPE_NAME (widget) : "(null)", x, y);
	g_free (copy);
	g_free (mime_copy);
}

static void
drop_read_emit (VerneDropRead *rd, GBytes *bytes)
{
	if (rd == NULL)
		return;
	if (bytes && rd->drop) {
		g_object_set_qdata_full (G_OBJECT (rd->drop), drop_cache_quark (),
					 g_bytes_ref (bytes), (GDestroyNotify) g_bytes_unref);
		if (rd->mime)
			g_object_set_data_full (G_OBJECT (rd->drop), "verne-drop-mime",
						g_strdup (rd->mime), g_free);
	}
	if (!rd->completed) {
		rd->completed = TRUE;
		if (rd->widget && GTK_IS_WIDGET (rd->widget)) {
			unpack_drop_xy (rd->drop, &rd->x, &rd->y);
			drop_deliver_bytes (rd->widget, rd->drop,
					    rd->mime ? rd->mime : "text/uri-list",
					    bytes, rd->x, rd->y, rd->info, rd->time);
		} else if (rd->drop && bytes == NULL) {
			verne_queue_gdk_drop_finish (rd->drop, 0);
		}
	}
	if (bytes)
		g_bytes_unref (bytes);
	{
		int n = 1;

		if (rd->timeout_id != 0) {
			g_source_remove (rd->timeout_id);
			rd->timeout_id = 0;
			n++;
		}
		rd->refs -= n;
		if (rd->refs <= 0)
			drop_read_free (rd);
	}
}

static gboolean
drop_read_timeout (gpointer data)
{
	VerneDropRead *rd = data;

	rd->timeout_id = 0;
	g_debug ("drop-read timeout dest=%s — waiting for async, not emitting empty drop",
		   rd->widget ? G_OBJECT_TYPE_NAME (rd->widget) : "(null)");
	drop_read_unref (rd);
	return G_SOURCE_REMOVE;
}

static void
drop_splice_done (GObject *source, GAsyncResult *result, gpointer data)
{
	VerneDropRead *rd = data;
	GError *err = NULL;
	GBytes *bytes = NULL;

	g_output_stream_splice_finish (G_OUTPUT_STREAM (source), result, &err);
	if (err) {
		gboolean cancelled = g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED);

		g_warning ("drop-read splice failed: %s", err->message);
		g_clear_error (&err);
		if (cancelled || rd->completed) {
			drop_read_unref (rd);
			return;
		}
	} else if (rd->mem) {
		bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (rd->mem));
	}
	if (rd->completed) {
		if (bytes)
			g_bytes_unref (bytes);
		drop_read_unref (rd);
		return;
	}
	if (bytes == NULL) {
		g_debug ("drop-read splice produced no bytes dest=%s",
			   rd->widget ? G_OBJECT_TYPE_NAME (rd->widget) : "(null)");
		drop_read_unref (rd);
		return;
	}
	drop_read_emit (rd, bytes);
}

static void
drop_read_done (GObject *source, GAsyncResult *result, gpointer data)
{
	VerneDropRead *rd = data;
	const char *mime = NULL;
	GInputStream *stream;
	GError *err = NULL;

	stream = gdk_drop_read_finish (GDK_DROP (source), result, &mime, &err);
	if (err) {
		gboolean cancelled = g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED);

		g_warning ("gdk_drop_read_finish failed: %s", err->message);
		g_clear_error (&err);
		if (cancelled) {
			if (stream)
				g_object_unref (stream);
			drop_read_unref (rd);
			return;
		}
	}
	if (rd->mime == NULL)
		rd->mime = g_strdup (mime ? mime : "text/uri-list");
	if (rd->widget && rd->mime)
		rd->info = info_for_target (gtk_drag_dest_get_target_list (rd->widget),
					    (GdkAtom) rd->mime);
	if (rd->completed) {
		if (stream)
			g_object_unref (stream);
		drop_read_unref (rd);
		return;
	}
	if (stream == NULL) {
		g_debug ("gdk_drop_read_finish returned no stream dest=%s",
			   rd->widget ? G_OBJECT_TYPE_NAME (rd->widget) : "(null)");
		drop_read_unref (rd);
		return;
	}
	rd->stream = stream;
	rd->mem = g_memory_output_stream_new_resizable ();
	g_output_stream_splice_async (rd->mem, stream,
				      G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
				      G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
				      G_PRIORITY_DEFAULT, rd->cancellable,
				      drop_splice_done, rd);
}

void
gtk_drag_get_data (GtkWidget *widget, GdkDragContext *context, GdkAtom target, guint32 time)
{
	VerneDropRead *rd;

	if (!context)
		return;
	if (VERNE_IS_LOCAL_DRAG (context)) {
		VerneLocalDrag *local = (VerneLocalDrag *) context;
		GtkSelectionData sel = { 0 };
		guint src_info, dst_info;
		gint x, y;

		unpack_drop_xy (context, &x, &y);
		sel.target = target ? target : (GdkAtom) "text/uri-list";
		sel.type = sel.target;
		sel.format = 8;
		src_info = info_for_target (local->targets, sel.target);
		dst_info = 0;
		if (!verne_target_list_find (gtk_drag_dest_get_target_list (widget),
					     sel.target, &dst_info) &&
		    verne_target_is_file_transfer (sel.target)) {
			if (!verne_target_list_find (gtk_drag_dest_get_target_list (widget),
						     (GdkAtom) "text/uri-list", &dst_info))
				dst_info = 0;
		}
		g_signal_emit_by_name (local->source, "drag-data-get", local, &sel, src_info, time);
		g_debug ("local drag-data-get target=%s len=%d src_info=%u dst_info=%u xy=%d,%d dest=%s",
			   sel.target ? (const char *) sel.target : "(null)", sel.length, src_info, dst_info,
			   x, y, widget ? G_OBJECT_TYPE_NAME (widget) : "(null)");
		g_signal_emit_by_name (widget, "drag-data-received", local, x, y, &sel, dst_info, time);
		g_free (sel.data);
		return;
	}
	if (!GDK_IS_DROP (context))
		return;
	{
		GBytes *cached = g_object_get_qdata (G_OBJECT (context), drop_cache_quark ());
		VerneDropRead *inflight = g_object_get_qdata (G_OBJECT (context), drop_read_quark ());
		int x, y;

		unpack_drop_xy (context, &x, &y);
		if (cached != NULL) {
			const char *mime = g_object_get_data (G_OBJECT (context), "verne-drop-mime");
			guint info = info_for_target (gtk_drag_dest_get_target_list (widget),
						      target ? target : (GdkAtom) (mime ? mime : "text/uri-list"));

			g_debug ("drop-read cache hit dest=%s mime=%s xy=%d,%d",
				   widget ? G_OBJECT_TYPE_NAME (widget) : "(null)",
				   mime ? mime : "(null)", x, y);
			drop_deliver_bytes (widget, GDK_DROP (context),
					    mime ? mime : (target ? (const char *) target : "text/uri-list"),
					    cached, x, y, info, time);
			return;
		}
		if (inflight != NULL && !inflight->completed) {
			inflight->x = x;
			inflight->y = y;
			inflight->time = time;
			g_debug ("drop-read coalesced dest=%s xy=%d,%d",
				   widget ? G_OBJECT_TYPE_NAME (widget) : "(null)", x, y);
			return;
		}
	}
	rd = g_new0 (VerneDropRead, 1);
	rd->widget = g_object_ref (widget);
	rd->drop = g_object_ref (GDK_DROP (context));
	unpack_drop_xy (context, &rd->x, &rd->y);
	rd->time = time;
	rd->info = info_for_target (gtk_drag_dest_get_target_list (widget), target);
	rd->cancellable = g_cancellable_new ();
	rd->refs = 2; /* timeout + gdk_drop_read_async chain */
	/* GDK X11 drop streams need the main loop. A sync read here deadlocks
	 * dest after mouse-up. Coalesce in-flight reads and cache bytes so drop
	 * can reuse motion data instead of flooding gdk_drop_read_async. */
	rd->timeout_id = g_timeout_add (4000, drop_read_timeout, rd);
	g_object_set_qdata (G_OBJECT (context), drop_read_quark (), rd);
	{
		GdkContentFormats *formats = gdk_drop_get_formats (GDK_DROP (context));
		const char *wanted[] = {
			target ? (const char *) target : NULL,
			"x-special/gnome-icon-list",
			"text/uri-list",
			"x-special/gnome-copied-files",
			"text/plain",
			NULL
		};
		const char *mimes[8];
		guint n = 0, i, j;

		for (i = 0; wanted[i] && n < 6; i++) {
			gboolean dup = FALSE;

			if (wanted[i] == NULL || wanted[i][0] == '\0')
				continue;
			for (j = 0; j < n; j++) {
				if (g_strcmp0 (mimes[j], wanted[i]) == 0)
					dup = TRUE;
			}
			if (dup)
				continue;
			if (formats == NULL ||
			    gdk_content_formats_contain_mime_type (formats, wanted[i]))
				mimes[n++] = wanted[i];
		}
		if (n == 0 && formats) {
			gsize n_mime = 0;
			const char * const *all = gdk_content_formats_get_mime_types (formats, &n_mime);
			guint k;

			for (k = 0; all && k < n_mime && n < 6; k++) {
				if (all[k] && all[k][0])
					mimes[n++] = all[k];
			}
		}
		if (n == 0) {
			mimes[n++] = "x-special/gnome-icon-list";
			mimes[n++] = "text/uri-list";
		}
		mimes[n] = NULL;
		{
			char *fmt = formats ? gdk_content_formats_to_string (formats) : NULL;

			g_debug ("drop-read async dest=%s n=%u first=%s formats=%s",
				   widget ? G_OBJECT_TYPE_NAME (widget) : "(null)", n,
				   mimes[0], fmt ? fmt : "(null)");
			g_free (fmt);
		}
		gdk_drop_read_async (GDK_DROP (context), mimes, G_PRIORITY_DEFAULT,
				     rd->cancellable, drop_read_done, rd);
	}
}

typedef struct {
	GdkDrop *drop;
	GdkDragAction action;
} VerneDropFinish;

static void
verne_reset_drop_async_controllers (void)
{
	GList *toplevels, *l, *d;

	forward_last_dest = NULL;
	toplevels = gtk_window_list_toplevels ();
	for (l = toplevels; l; l = l->next) {
		GtkWidget *win = l->data;
		GtkDropTargetAsync *async;

		if (!GTK_IS_WINDOW (win))
			continue;
		async = g_object_get_qdata (G_OBJECT (win), forwarder_quark ());
		if (async == NULL)
			continue;
		gtk_widget_remove_controller (win, GTK_EVENT_CONTROLLER (async));
		g_object_set_qdata (G_OBJECT (win), forwarder_quark (), NULL);
	}
	g_list_free (toplevels);

	for (d = verne_drop_dests; d; d = d->next) {
		GtkWidget *dest = d->data;
		gpointer p;

		if (!GTK_IS_WIDGET (dest))
			continue;
		p = g_object_get_qdata (G_OBJECT (dest), dest_quark ());
		if (GTK_IS_DROP_TARGET_ASYNC (p)) {
			GdkContentFormats *formats;
			GdkDragAction actions;
			GtkDropTargetAsync *old = p;
			GtkDropTargetAsync *async;

			formats = gtk_drop_target_async_get_formats (old);
			actions = gtk_drop_target_async_get_actions (old);
			if (formats)
				gdk_content_formats_ref (formats);
			gtk_widget_remove_controller (dest, GTK_EVENT_CONTROLLER (old));
			async = gtk_drop_target_async_new (formats,
							   actions ? actions : (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));
			gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (async), GTK_PHASE_CAPTURE);
			g_signal_connect (async, "accept", G_CALLBACK (on_async_accept), dest);
			g_signal_connect (async, "drag-enter", G_CALLBACK (on_async_motion), dest);
			g_signal_connect (async, "drag-motion", G_CALLBACK (on_async_motion), dest);
			g_signal_connect (async, "drag-leave", G_CALLBACK (on_async_leave), dest);
			g_signal_connect (async, "drop", G_CALLBACK (on_async_drop), dest);
			gtk_widget_add_controller (dest, GTK_EVENT_CONTROLLER (async));
			g_object_set_qdata (G_OBJECT (dest), dest_quark (), async);
		}
		verne_ensure_native_drop_forwarder (dest);
	}
}

static gboolean
verne_drop_finish_idle (gpointer data)
{
	VerneDropFinish *fin = data;

	if (fin->drop && GDK_IS_DROP (fin->drop)) {
		g_debug ("gdk_drop_finish idle action=%d", (int) fin->action);
		gdk_drop_finish (fin->drop, fin->action);
	}
	verne_reset_drop_async_controllers ();
	g_clear_object (&fin->drop);
	g_free (fin);
	return G_SOURCE_REMOVE;
}

static void
verne_queue_gdk_drop_finish (GdkDrop *drop, GdkDragAction action)
{
	VerneDropFinish *fin;

	if (drop == NULL)
		return;
	if (g_object_get_data (G_OBJECT (drop), "verne-drop-finish-queued"))
		return;
	g_object_set_data (G_OBJECT (drop), "verne-drop-finish-queued", GINT_TO_POINTER (1));
	fin = g_new0 (VerneDropFinish, 1);
	fin->drop = g_object_ref (drop);
	fin->action = action;
	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, verne_drop_finish_idle, fin, NULL);
}

void
gtk_drag_finish (gpointer context, gboolean success, gboolean del, guint32 time)
{
	GdkDragAction action = 0;
	GtkWidget *source = NULL;
	GdkDrag *drag = NULL;

	(void) time;
	if (!context)
		return;
	if (VERNE_IS_LOCAL_DRAG (context)) {
		VerneLocalDrag *local = (VerneLocalDrag *) context;
		g_debug ("gtk_drag_finish local success=%d del=%d", success, del);
		if (del && local->source)
			g_signal_emit_by_name (local->source, "drag-data-delete", local);
		local->dest_finished = TRUE;
		/* Do not unref here: Nemo finishes from drag-data-received while
		 * still inside drag-drop. Cleanup runs in verne_local_emit_drop. */
		return;
	}
	action = (GdkDragAction) GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (context), selected_action_quark ()));
	if (success && action == 0)
		action = GDK_ACTION_COPY;
	if (GDK_IS_DROP (context)) {
		drag = gdk_drop_get_drag (GDK_DROP (context));
		g_object_set_qdata (G_OBJECT (context), drop_cache_quark (), NULL);
		g_object_set_qdata (G_OBJECT (context), drop_read_quark (), NULL);
		g_object_set_data (G_OBJECT (context), "verne-drop-mime", NULL);
		/* Finish after DROP_START returns, then replace DropTargetAsync
		 * so GTK's private self->drop is not left pointing at a finished
		 * GdkDrop (next foreign drop asserts self->drop == drop). */
		verne_queue_gdk_drop_finish (GDK_DROP (context), success ? action : 0);
	} else if (GDK_IS_DRAG (context)) {
		drag = GDK_DRAG (context);
	}
	if (drag)
		source = g_object_get_qdata (G_OBJECT (drag), source_widget_quark ());
	if (source == NULL)
		source = g_object_get_qdata (G_OBJECT (context), source_widget_quark ());
	if (del && source)
		g_signal_emit_by_name (source, "drag-data-delete", drag ? (gpointer) drag : context);
	if (drag)
		gdk_drag_drop_done (drag, success);
	verne_dnd_cleanup_source (source);
	clear_pending_drop ();
}

void
gdk_drag_status (GdkDragContext *context, GdkDragAction action, guint32 time)
{
	(void) time;
	if (!context)
		return;
	/* Motion over empty dest reports action 0. Do not wipe MOVE. */
	if (action == 0 && VERNE_IS_LOCAL_DRAG (context) &&
	    ((VerneLocalDrag *) context)->selected != 0)
		return;
	g_object_set_qdata (G_OBJECT (context), selected_action_quark (), GINT_TO_POINTER ((int) action));
	if (VERNE_IS_LOCAL_DRAG (context))
		((VerneLocalDrag *) context)->selected = action;
	else if (GDK_IS_DROP (context))
		verne_drop_status_safe (GDK_DROP (context), action);
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
	if (VERNE_IS_LOCAL_DRAG (context))
		return ((VerneLocalDrag *) context)->selected;
	if (GDK_IS_DROP (context)) {
		GdkDragAction a = gdk_drop_get_actions (GDK_DROP (context));
		/* Foreign XDND often delivers Drop before dest motion can
		 * gdk_drag_status(). Default to COPY so receive_dropped_icons
		 * does not no-op with selected_action==0. */
		if (a & GDK_ACTION_COPY)
			return GDK_ACTION_COPY;
		if (a & GDK_ACTION_MOVE)
			return GDK_ACTION_MOVE;
		if (a)
			return a;
		return GDK_ACTION_COPY;
	}
	if (GDK_IS_DRAG (context))
		return gdk_drag_get_selected_action (GDK_DRAG (context));
	return 0;
}

static GdkModifierType
verne_pointer_modifiers (void)
{
	GdkModifierType state = 0;

	if (gtk_get_current_event_state (&state) && state != 0)
		return state;
#ifdef GDK_WINDOWING_X11
	{
		GdkDisplay *gdpy = gdk_display_get_default ();
		if (gdpy && GDK_IS_X11_DISPLAY (gdpy)) {
			Display *dpy = gdk_x11_display_get_xdisplay (gdpy);
			Window root_ret, child;
			int rx = 0, ry = 0, wx = 0, wy = 0;
			unsigned int mask = 0;
			if (XQueryPointer (dpy, DefaultRootWindow (dpy), &root_ret, &child,
					   &rx, &ry, &wx, &wy, &mask)) {
				if (mask & ShiftMask)
					state |= GDK_SHIFT_MASK;
				if (mask & ControlMask)
					state |= GDK_CONTROL_MASK;
				if (mask & Mod1Mask)
					state |= GDK_MOD1_MASK;
				if (mask & Mod4Mask)
					state |= GDK_SUPER_MASK;
			}
		}
	}
#endif
	return state;
}

/* GTK3 Gdk maps pointer modifiers onto the suggested drop action:
 * Alt = Ask, Ctrl+Shift = Link, Ctrl = Copy, Shift = Move. */
static GdkDragAction
verne_suggested_from_modifiers (GdkDragAction actions)
{
	GdkModifierType state = verne_pointer_modifiers ();

	if ((state & GDK_MOD1_MASK) && (actions & GDK_ACTION_ASK))
		return GDK_ACTION_ASK;
	if ((state & GDK_CONTROL_MASK) && (state & GDK_SHIFT_MASK) && (actions & GDK_ACTION_LINK))
		return GDK_ACTION_LINK;
	if ((state & GDK_CONTROL_MASK) && (actions & GDK_ACTION_COPY))
		return GDK_ACTION_COPY;
	if ((state & GDK_SHIFT_MASK) && (actions & GDK_ACTION_MOVE))
		return GDK_ACTION_MOVE;
	return 0;
}

/* Whether the user is holding a modifier that names a drop action, so a
 * destination can tell "copy because Ctrl" from "copy because that is the
 * default". */
GdkDragAction
verne_drag_forced_action (void)
{
	return verne_suggested_from_modifiers (GDK_ACTION_ASK | GDK_ACTION_LINK |
					       GDK_ACTION_COPY | GDK_ACTION_MOVE);
}

GdkDragAction
gdk_drag_context_get_suggested_action (GdkDragContext *context)
{
	if (context && VERNE_IS_LOCAL_DRAG (context)) {
		GdkDragAction a = ((VerneLocalDrag *) context)->actions;
		GdkDragAction forced = verne_suggested_from_modifiers (a);
		if (forced)
			return forced;
		if (a & GDK_ACTION_MOVE)
			return GDK_ACTION_MOVE;
		if (a & GDK_ACTION_COPY)
			return GDK_ACTION_COPY;
		return a;
	}
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
	if (context && VERNE_IS_LOCAL_DRAG (context))
		return ((VerneLocalDrag *) context)->actions;
	if (context && GDK_IS_DROP (context))
		return gdk_drop_get_actions (GDK_DROP (context));
	if (context && GDK_IS_DRAG (context))
		return gdk_drag_get_actions (GDK_DRAG (context));
	return GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK;
}

GdkSurface *
gdk_drag_context_get_source_window (GdkDragContext *context)
{
	if (context && VERNE_IS_LOCAL_DRAG (context)) {
		GtkWidget *source = ((VerneLocalDrag *) context)->source;
		GtkNative *native = source ? gtk_widget_get_native (source) : NULL;
		return native ? gtk_native_get_surface (native) : NULL;
	}
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
	if (VERNE_IS_LOCAL_DRAG (context))
		return ((VerneLocalDrag *) context)->source;
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
		g_signal_emit_by_name (self->widget, "drag-data-get", self->drag, &sel, info, 0U);

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
		gtk_target_list_ref (list);
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

GdkDragContext *
gtk_drag_begin_with_coordinates (GtkWidget *widget, GtkTargetList *targets, GdkDragAction actions, gint button, GdkEvent *event, gint x, gint y)
{
	VerneLocalDrag *local;

	(void) button;
	(void) event;
	(void) x;
	(void) y;
	if (targets == NULL)
		targets = gtk_drag_source_get_target_list (widget);
	if (widget == NULL)
		return NULL;

	local = g_object_new (verne_local_drag_get_type (), NULL);
	local->source = widget;
	local->targets = targets;
	if (targets)
		gtk_target_list_ref (targets);
	local->actions = actions ? actions : (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	if (local->actions & GDK_ACTION_MOVE)
		local->selected = GDK_ACTION_MOVE;
	else
		local->selected = GDK_ACTION_COPY;
	g_object_set_qdata (G_OBJECT (local), source_widget_quark (), widget);
	g_object_set_data (G_OBJECT (widget), "verne-active-drag", local);
	g_object_weak_ref (G_OBJECT (widget), verne_local_source_gone, local);
	local->poll_id = g_timeout_add (16, verne_local_poll, local);
	g_signal_emit_by_name (widget, "drag-begin", local);
	verne_dnd_local_motion (widget);
	g_debug ("local drag started from %s", G_OBJECT_TYPE_NAME (widget));
	return (GdkDragContext *) local;
}

gpointer
gtk_drag_begin (GtkWidget *widget, GtkTargetList *targets, GdkDragAction actions, gint button, GdkEvent *event)
{
	gint bx = 0, by = 0;
	if (event) {
		bx = (gint) event->button.x;
		by = (gint) event->button.y;
	}
	return gtk_drag_begin_with_coordinates (widget, targets, actions, button, event, bx, by);
}

static void
verne_local_icon_realize (GtkWidget *widget, gpointer data)
{
	GdkSurface *surface;
	cairo_region_t *empty;

	(void) data;
	surface = gtk_native_get_surface (GTK_NATIVE (widget));
	if (surface == NULL)
		return;
	empty = cairo_region_create ();
	gdk_surface_set_input_region (surface, empty);
	cairo_region_destroy (empty);
}

static void
verne_local_ensure_icon (VerneLocalDrag *local, GdkPixbuf *pixbuf, int hot_x, int hot_y)
{
	GdkTexture *texture;

	local->hot_x = hot_x;
	local->hot_y = hot_y;
	texture = gdk_texture_new_for_pixbuf (pixbuf);
	if (local->icon_window == NULL) {
		local->icon_window = gtk_window_new ();
		gtk_window_set_decorated (GTK_WINDOW (local->icon_window), FALSE);
		gtk_window_set_resizable (GTK_WINDOW (local->icon_window), FALSE);
		gtk_window_set_title (GTK_WINDOW (local->icon_window), "verne-dnd-icon");
		gtk_window_set_type_hint (GTK_WINDOW (local->icon_window), GDK_WINDOW_TYPE_HINT_DND);
		gtk_widget_set_can_target (local->icon_window, FALSE);
		gtk_widget_set_can_focus (local->icon_window, FALSE);
		g_object_set_data (G_OBJECT (local->icon_window), "verne-skip-taskbar", GINT_TO_POINTER (1));
		g_object_set_data (G_OBJECT (local->icon_window), "verne-skip-pager", GINT_TO_POINTER (1));
		local->picture = gtk_picture_new_for_paintable (GDK_PAINTABLE (texture));
		gtk_picture_set_can_shrink (GTK_PICTURE (local->picture), FALSE);
		gtk_window_set_child (GTK_WINDOW (local->icon_window), local->picture);
		g_signal_connect (local->icon_window, "realize", G_CALLBACK (verne_local_icon_realize), NULL);
		gtk_window_set_default_size (GTK_WINDOW (local->icon_window),
					     gdk_pixbuf_get_width (pixbuf),
					     gdk_pixbuf_get_height (pixbuf));
		verne_local_move_icon (local);
		gtk_window_present (GTK_WINDOW (local->icon_window));
	} else if (local->picture) {
		gtk_picture_set_paintable (GTK_PICTURE (local->picture), GDK_PAINTABLE (texture));
	}
	g_object_unref (texture);
	verne_local_move_icon (local);
}

void
gtk_drag_set_icon_pixbuf (GdkDragContext *context, GdkPixbuf *pixbuf, gint hot_x, gint hot_y)
{
	GdkTexture *texture;

	if (!context || !pixbuf)
		return;
	if (VERNE_IS_LOCAL_DRAG (context)) {
		verne_local_ensure_icon ((VerneLocalDrag *) context, pixbuf, hot_x, hot_y);
		return;
	}
	if (!GDK_IS_DRAG (context))
		return;
	texture = gdk_texture_new_for_pixbuf (pixbuf);
	gtk_drag_icon_set_from_paintable (GDK_DRAG (context), GDK_PAINTABLE (texture), hot_x, hot_y);
	g_object_unref (texture);
}

void
gtk_drag_set_icon_name (GdkDragContext *context, const gchar *name, gint hot_x, gint hot_y)
{
	GtkIconTheme *theme;
	GdkPixbuf *pixbuf;

	if (!context)
		return;
	theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
	pixbuf = gtk_icon_theme_load_icon (theme, name ? name : "text-x-generic", 48, 0, NULL);
	if (pixbuf && VERNE_IS_LOCAL_DRAG (context)) {
		verne_local_ensure_icon ((VerneLocalDrag *) context, pixbuf, hot_x, hot_y);
		g_object_unref (pixbuf);
		return;
	}
	if (pixbuf && GDK_IS_DRAG (context)) {
		GdkTexture *texture = gdk_texture_new_for_pixbuf (pixbuf);
		gtk_drag_icon_set_from_paintable (GDK_DRAG (context), GDK_PAINTABLE (texture), hot_x, hot_y);
		g_object_unref (texture);
		g_object_unref (pixbuf);
		return;
	}
	if (pixbuf)
		g_object_unref (pixbuf);
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
	if (context && VERNE_IS_LOCAL_DRAG (context)) {
		((VerneLocalDrag *) context)->hot_x = hot_x;
		((VerneLocalDrag *) context)->hot_y = hot_y;
	} else if (context && GDK_IS_DRAG (context))
		gdk_drag_set_hotspot (GDK_DRAG (context), hot_x, hot_y);
}

void
gtk_drag_set_icon_surface (GdkDragContext *context, cairo_surface_t *surface)
{
	GdkPixbuf *pixbuf;
	double dx = 0, dy = 0;
	int w, h;

	if (!surface)
		return;
	if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_IMAGE)
		return;
	w = cairo_image_surface_get_width (surface);
	h = cairo_image_surface_get_height (surface);
	if (w < 1 || h < 1)
		return;
	cairo_surface_get_device_offset (surface, &dx, &dy);
	pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, w, h);
	if (pixbuf) {
		gtk_drag_set_icon_pixbuf (context, pixbuf, (gint) (-dx), (gint) (-dy));
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
