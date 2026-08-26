/* GTK3 drag-and-drop / clipboard-with-data implemented on GTK4. */
#include "config.h"
#include "verne-gtk-compat.h"
#include "verne-gtk-clipboard-private.h"

#include <string.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk/x11/gdkx.h>
#include <graphene.h>
#include <X11/Xlib.h>

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
	double dest_x;
	double dest_y;
	int hot_x;
	int hot_y;
	guint poll_id;
	gboolean drop_emitted;
} VerneLocalDrag;

typedef struct _VerneLocalDragClass {
	GObjectClass parent_class;
} VerneLocalDragClass;

G_DEFINE_TYPE (VerneLocalDrag, verne_local_drag, G_TYPE_OBJECT)

#define VERNE_IS_LOCAL_DRAG(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), verne_local_drag_get_type ()))

static void
verne_local_drag_finalize (GObject *object)
{
	VerneLocalDrag *self = (VerneLocalDrag *) object;

	if (self->poll_id) {
		g_source_remove (self->poll_id);
		self->poll_id = 0;
	}
	if (self->icon_window) {
		gtk_window_destroy (GTK_WINDOW (self->icon_window));
		self->icon_window = NULL;
		self->picture = NULL;
	}
	if (self->targets) {
		gtk_target_list_unref (self->targets);
		self->targets = NULL;
	}
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

static gboolean on_async_drop (GtkDropTargetAsync *self, GdkDrop *drop, double x, double y, gpointer data);
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
	g_object_set_qdata (G_OBJECT (context), drop_xy_quark (),
			    GINT_TO_POINTER (((int) x & 0xffff) | (((int) y & 0xffff) << 16)));
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
	selected = (GdkDragAction) GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (drop), selected_action_quark ()));
	if (selected == 0 && handled)
		selected = GDK_ACTION_COPY;
	if (selected)
		gdk_drop_status (drop, gdk_drop_get_actions (drop), selected);
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
	g_debug ("drop at %.0f,%.0f on %s", x, y, G_OBJECT_TYPE_NAME (widget));
	g_signal_emit_by_name (widget, "drag-drop", drop, (int) x, (int) y, GDK_CURRENT_TIME, &handled);
	g_debug ("drag-drop handled=%d", handled);
	return handled;
}

static GtkWidget *
verne_local_find_dest (VerneLocalDrag *local, double *x, double *y)
{
	GdkSeat *seat;
	GdkDevice *device;
	GList *toplevels, *l;

	seat = gdk_display_get_default_seat (gdk_display_get_default ());
	device = seat ? gdk_seat_get_pointer (seat) : NULL;
	if (device == NULL)
		return NULL;

	toplevels = gtk_window_list_toplevels ();
	for (l = toplevels; l; l = l->next) {
		GtkWidget *win = l->data;
		GtkNative *native;
		GdkSurface *surface;
		GtkWidget *root, *picked, *w;
		double sx = 0, sy = 0, tx = 0, ty = 0, px, py;
		int sw, sh;

		if (win == local->icon_window)
			continue;
		native = GTK_NATIVE (win);
		surface = gtk_native_get_surface (native);
		if (surface == NULL)
			continue;
		gdk_surface_get_device_position (surface, device, &sx, &sy, NULL);
		sw = gdk_surface_get_width (surface);
		sh = gdk_surface_get_height (surface);
		if (sx < 0 || sy < 0 || sx >= sw || sy >= sh)
			continue;
		gtk_native_get_surface_transform (native, &tx, &ty);
		px = sx - tx;
		py = sy - ty;
		root = GTK_WIDGET (gtk_widget_get_root (win));
		picked = gtk_widget_pick (root, px, py, GTK_PICK_DEFAULT);
		for (w = picked; w; w = gtk_widget_get_parent (w)) {
			if (g_object_get_qdata (G_OBJECT (w), dest_quark ())) {
				graphene_point_t p;
				if (gtk_widget_compute_point (root, w, &GRAPHENE_POINT_INIT ((float) px, (float) py), &p)) {
					*x = p.x;
					*y = p.y;
				} else {
					*x = px;
					*y = py;
				}
				g_list_free (toplevels);
				return w;
			}
		}
		if (local->source && gtk_widget_get_native (local->source) == native &&
		    g_object_get_qdata (G_OBJECT (local->source), dest_quark ())) {
			graphene_point_t p;
			if (gtk_widget_compute_point (root, local->source,
						      &GRAPHENE_POINT_INIT ((float) px, (float) py), &p)) {
				*x = p.x;
				*y = p.y;
			} else {
				*x = px;
				*y = py;
			}
			g_list_free (toplevels);
			return local->source;
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
	local->current_dest = dest;
	local->dest_x = x;
	local->dest_y = y;
	if (dest == NULL)
		return;
	g_debug ("local dest motion at %.0f,%.0f on %s", x, y, G_OBJECT_TYPE_NAME (dest));
	pack_drop_xy (local, x, y);
	g_signal_emit_by_name (dest, "drag-motion", local, (int) x, (int) y, GDK_CURRENT_TIME, &handled);
	selected = (GdkDragAction) GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (local), selected_action_quark ()));
	if (selected == 0 && handled)
		selected = GDK_ACTION_COPY;
	if (selected)
		local->selected = selected;
}

static void
verne_local_cleanup (VerneLocalDrag *local)
{
	GtkWidget *source;

	if (local == NULL)
		return;
	source = local->source;
	if (local->poll_id) {
		g_source_remove (local->poll_id);
		local->poll_id = 0;
	}
	if (local->icon_window) {
		gtk_window_destroy (GTK_WINDOW (local->icon_window));
		local->icon_window = NULL;
		local->picture = NULL;
	}
	if (source)
		g_object_set_data (G_OBJECT (source), "verne-active-drag", NULL);
	g_object_unref (local);
}

static void
verne_local_emit_drop (VerneLocalDrag *local)
{
	gboolean handled = FALSE;

	if (local->drop_emitted)
		return;
	local->drop_emitted = TRUE;
	if (local->current_dest == NULL)
		verne_local_update_dest (local);
	if (local->current_dest) {
		GtkWidget *source = local->source;

		g_warning ("completing local drop on %s at %.0f,%.0f",
			   G_OBJECT_TYPE_NAME (local->current_dest), local->dest_x, local->dest_y);
		pack_drop_xy (local, local->dest_x, local->dest_y);
		g_signal_emit_by_name (local->current_dest, "drag-drop", local,
				       (int) local->dest_x, (int) local->dest_y, GDK_CURRENT_TIME, &handled);
		g_warning ("local drag-drop handled=%d", handled);
		if (source && g_object_get_data (G_OBJECT (source), "verne-active-drag") == (gpointer) local) {
			g_signal_emit_by_name (source, "drag-end", local);
			verne_local_cleanup (local);
		}
	} else {
		g_warning ("local drop with no dest, cancelling");
		g_signal_emit_by_name (local->source, "drag-failed", local, 0, &handled);
		g_signal_emit_by_name (local->source, "drag-end", local);
		verne_local_cleanup (local);
	}
}

static gboolean
verne_local_poll (gpointer data)
{
	GtkWidget *source = data;
	VerneLocalDrag *local;

	if (!GTK_IS_WIDGET (source))
		return G_SOURCE_REMOVE;
	local = g_object_get_data (G_OBJECT (source), "verne-active-drag");
	if (local == NULL || !VERNE_IS_LOCAL_DRAG (local) || local->drop_emitted)
		return G_SOURCE_REMOVE;
	verne_local_move_icon (local);
	verne_local_update_dest (local);
	if (!verne_local_button1_down ()) {
		local->poll_id = 0;
		g_warning ("local DnD poll saw button1 up, dropping");
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
		verne_local_emit_drop (drag);
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
		if (formats == NULL || gdk_content_formats_contain_mime_type (formats, e->target)) {
			result = (GdkAtom) e->target;
			break;
		}
	}
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
	g_debug ("drag-data-received mime=%s len=%d info=%u", mime ? mime : "(null)", sel.length, rd->info);
	g_free (sel.data);
	g_free (rd);
}

void
gtk_drag_get_data (GtkWidget *widget, GdkDragContext *context, GdkAtom target, guint32 time)
{
	VerneDropRead *rd;
	const char *mimes[2];
	gpointer packed;

	if (!context)
		return;
	if (VERNE_IS_LOCAL_DRAG (context)) {
		VerneLocalDrag *local = (VerneLocalDrag *) context;
		GtkSelectionData sel = { 0 };
		guint src_info, dst_info;
		gint x, y;

		packed = g_object_get_qdata (G_OBJECT (context), drop_xy_quark ());
		x = GPOINTER_TO_INT (packed) & 0xffff;
		y = (GPOINTER_TO_INT (packed) >> 16) & 0xffff;
		sel.target = target ? target : (GdkAtom) "text/uri-list";
		sel.type = sel.target;
		sel.format = 8;
		src_info = info_for_target (local->targets, sel.target);
		dst_info = info_for_target (gtk_drag_dest_get_target_list (widget), sel.target);
		g_signal_emit_by_name (local->source, "drag-data-get", local, &sel, src_info, time);
		g_warning ("local drag-data-get target=%s len=%d src_info=%u dst_info=%u",
			   sel.target ? (const char *) sel.target : "(null)", sel.length, src_info, dst_info);
		g_signal_emit_by_name (widget, "drag-data-received", local, x, y, &sel, dst_info, time);
		g_free (sel.data);
		return;
	}
	if (!GDK_IS_DROP (context))
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
	GtkWidget *source = NULL;
	GdkDrag *drag = NULL;

	(void) time;
	if (!context)
		return;
	if (VERNE_IS_LOCAL_DRAG (context)) {
		VerneLocalDrag *local = (VerneLocalDrag *) context;
		g_warning ("gtk_drag_finish local success=%d del=%d", success, del);
		if (del && local->source)
			g_signal_emit_by_name (local->source, "drag-data-delete", local);
		if (local->source)
			g_signal_emit_by_name (local->source, "drag-end", local);
		verne_local_cleanup (local);
		return;
	}
	action = (GdkDragAction) GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (context), selected_action_quark ()));
	if (success && action == 0)
		action = GDK_ACTION_COPY;
	if (GDK_IS_DROP (context)) {
		drag = gdk_drop_get_drag (GDK_DROP (context));
		gdk_drop_finish (GDK_DROP (context), success ? action : 0);
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
	g_object_set_qdata (G_OBJECT (context), selected_action_quark (), GINT_TO_POINTER ((int) action));
	if (VERNE_IS_LOCAL_DRAG (context))
		((VerneLocalDrag *) context)->selected = action;
	else if (GDK_IS_DROP (context))
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
	if (VERNE_IS_LOCAL_DRAG (context))
		return ((VerneLocalDrag *) context)->selected;
	if (GDK_IS_DRAG (context))
		return gdk_drag_get_selected_action (GDK_DRAG (context));
	return 0;
}

GdkDragAction
gdk_drag_context_get_suggested_action (GdkDragContext *context)
{
	if (context && VERNE_IS_LOCAL_DRAG (context)) {
		GdkDragAction a = ((VerneLocalDrag *) context)->actions;
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
		targets->ref++;
	local->actions = actions ? actions : (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	if (local->actions & GDK_ACTION_MOVE)
		local->selected = GDK_ACTION_MOVE;
	else
		local->selected = GDK_ACTION_COPY;
	g_object_set_qdata (G_OBJECT (local), source_widget_quark (), widget);
	g_object_set_data (G_OBJECT (widget), "verne-active-drag", local);
	local->poll_id = g_timeout_add (16, verne_local_poll, widget);
	g_signal_emit_by_name (widget, "drag-begin", local);
	verne_dnd_local_motion (widget);
	g_warning ("local drag started from %s", G_OBJECT_TYPE_NAME (widget));
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
	GtkWidget *parent;

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
		parent = GTK_WIDGET (gtk_widget_get_root (local->source));
		if (GTK_IS_WINDOW (parent))
			gtk_window_set_transient_for (GTK_WINDOW (local->icon_window), GTK_WINDOW (parent));
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

	if (!surface)
		return;
	cairo_surface_get_device_offset (surface, &dx, &dy);
	pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0,
					      cairo_image_surface_get_width (surface),
					      cairo_image_surface_get_height (surface));
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
