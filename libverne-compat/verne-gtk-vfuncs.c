/* GTK3 class vfuncs mapped onto GTK4 event controllers / snapshot / measure. */
#include "config.h"
#include "verne-gtk-compat.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <graphene.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

typedef struct {
	VerneButtonEvent button_press;
	VerneButtonEvent button_release;
	VerneMotionEvent motion;
	VerneKeyEvent key_press;
	VerneKeyEvent key_release;
	VerneScrollEvent scroll;
	VerneCrossingEvent enter;
	VerneCrossingEvent leave;
	VerneFocusEvent focus_in;
	VerneFocusEvent focus_out;
	VerneDrawEvent draw;
	VerneSizeAllocate size_allocate;
	VerneDestroyFunc destroy;
	VernePreferredSize pref_width;
	VernePreferredSize pref_height;
	VerneDeleteEvent delete_event;
	VernePopupMenu popup_menu;
	VerneStyleUpdated style_updated;
	VerneGrabNotify grab_notify;
	VerneShowFunc show;
	GtkWidgetClass *klass;
	void (*orig_snapshot) (GtkWidget *, GtkSnapshot *);
	void (*orig_realize) (GtkWidget *);
	void (*orig_unrealize) (GtkWidget *);
	void (*realize) (GtkWidget *);
	void (*unrealize) (GtkWidget *);
	void (*state_changed) (GtkWidget *, GtkStateType);
	gpointer get_accessible;
	void (*orig_state_flags_changed) (GtkWidget *, GtkStateFlags);
	void (*orig_size_allocate) (GtkWidget *, int, int, int);
	void (*orig_measure) (GtkWidget *, GtkOrientation, int, int *, int *, int *, int *);
	void (*orig_constructed) (GObject *);
	void (*orig_dispose) (GObject *);
	void (*orig_show) (GtkWidget *);
	void (*orig_hide) (GtkWidget *);
	void (*orig_css_changed) (GtkWidget *, GtkCssStyleChange *);
	VerneWindowStateEvent window_state;
	VerneShowFunc hide;
	gboolean wrapped;
	gboolean in_realize;
	gboolean in_unrealize;
	gboolean in_show;
	gboolean in_size_allocate;
} VerneVfuncs;

static void (*orig_gtk_widget_realize) (GtkWidget *);
static void (*orig_drawing_area_realize) (GtkWidget *);
static gboolean event_signals_registered;
static guint verne_draw_signal_id;

static gboolean
verne_widget_has_draw_handlers (GtkWidget *widget)
{
	if (verne_draw_signal_id == 0)
		verne_draw_signal_id = g_signal_lookup ("draw", GTK_TYPE_WIDGET);
	if (verne_draw_signal_id == 0 || widget == NULL)
		return FALSE;
	return g_signal_has_handler_pending (widget, verne_draw_signal_id, 0, FALSE);
}

static void
verne_emit_draw_signal (GtkWidget *widget, cairo_t *cr)
{
	gboolean handled = FALSE;

	if (widget == NULL || cr == NULL || !verne_widget_has_draw_handlers (widget))
		return;
	g_signal_emit (widget, verne_draw_signal_id, 0, cr, &handled);
}

static void
verne_drawing_area_draw (GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data)
{
	(void) width;
	(void) height;
	(void) data;
	verne_emit_draw_signal (GTK_WIDGET (area), cr);
}

static void
verne_drawing_area_realize (GtkWidget *widget)
{
	if (GTK_IS_DRAWING_AREA (widget) &&
	    g_object_get_data (G_OBJECT (widget), "verne-draw-func") == NULL) {
		gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (widget),
						verne_drawing_area_draw, NULL, NULL);
		g_object_set_data (G_OBJECT (widget), "verne-draw-func", GINT_TO_POINTER (1));
	}
	if (orig_drawing_area_realize)
		orig_drawing_area_realize (widget);
	else if (orig_gtk_widget_realize)
		orig_gtk_widget_realize (widget);
}

static GHashTable *vfunc_table;
static GQuark verne_controllers_quark;

static VerneVfuncs *
lookup_vfuncs_type (GType type)
{
	VerneVfuncs *v;

	if (vfunc_table == NULL)
		vfunc_table = g_hash_table_new (g_direct_hash, g_direct_equal);

	while (type != 0 && type != GTK_TYPE_WIDGET && type != G_TYPE_OBJECT) {
		v = g_hash_table_lookup (vfunc_table, GSIZE_TO_POINTER (type));
		if (v != NULL)
			return v;
		type = g_type_parent (type);
	}
	return NULL;
}

static VerneVfuncs *
ensure_vfuncs (GtkWidgetClass *klass)
{
	GType type = G_TYPE_FROM_CLASS (klass);
	VerneVfuncs *v;

	if (vfunc_table == NULL)
		vfunc_table = g_hash_table_new (g_direct_hash, g_direct_equal);

	v = g_hash_table_lookup (vfunc_table, GSIZE_TO_POINTER (type));
	if (v == NULL) {
		v = g_new0 (VerneVfuncs, 1);
		v->klass = klass;
		g_hash_table_insert (vfunc_table, GSIZE_TO_POINTER (type), v);
	}
	return v;
}

static void
fill_button_event (GdkEvent *ev, GtkGestureClick *click, gint n_press, gdouble x, gdouble y)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (click));
	GdkEvent *ge = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (click));

	memset (ev, 0, sizeof (*ev));
	if (n_press >= 3)
		ev->button.type = GDK_3BUTTON_PRESS;
	else if (n_press == 2)
		ev->button.type = GDK_2BUTTON_PRESS;
	else
		ev->button.type = GDK_BUTTON_PRESS;
	ev->button.window = gtk_widget_get_window (widget);
	ev->button.x = x;
	ev->button.y = y;
	ev->button.x_root = x;
	ev->button.y_root = y;
	ev->button.button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));
	if (ge && !verne_gdk_event_is_synth (ge)) {
		guint native_button = gdk_button_event_get_button (ge);
		if (native_button != 0)
			ev->button.button = native_button;
	}
	ev->button.state = ge ? gdk_event_get_modifier_state (ge) : 0;
	if (ev->button.button == 2)
		ev->button.state |= GDK_BUTTON2_MASK;
	else if (ev->button.button == 1)
		ev->button.state |= GDK_BUTTON1_MASK;
	else if (ev->button.button == 3)
		ev->button.state |= GDK_BUTTON3_MASK;
	ev->button.time = ge ? gdk_event_get_time (ge) : GDK_CURRENT_TIME;
}

static gboolean
emit_widget_event (GtkWidget *widget, const char *signal, gpointer event)
{
	gboolean handled = FALSE;
	g_signal_emit_by_name (widget, signal, event, &handled);
	return handled;
}

static void
emit_motion (GtkWidget *widget, gdouble x, gdouble y, guint state, guint32 time)
{
	VerneVfuncs *v;
	GdkEvent ev;

	if (!GTK_IS_WIDGET (widget) || !gtk_widget_get_realized (widget) ||
	    !gtk_widget_get_mapped (widget) ||
	    g_object_get_data (G_OBJECT (widget), "verne-destroyed"))
		return;
	v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	memset (&ev, 0, sizeof (ev));
	ev.motion.type = GDK_MOTION_NOTIFY;
	ev.motion.window = gtk_widget_get_window (widget);
	ev.motion.x = x;
	ev.motion.y = y;
	ev.motion.x_root = x;
	ev.motion.y_root = y;
	ev.motion.state = state;
	ev.motion.time = time ? time : GDK_CURRENT_TIME;
	verne_set_current_event (widget, &ev);
	if (!emit_widget_event (widget, "motion-notify-event", &ev) && v && v->motion)
		v->motion (widget, &ev.motion);
	verne_clear_current_event ();
}

static void
on_pressed (GtkGestureClick *click, gint n_press, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (click));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEvent ev;
	gboolean handled = FALSE;

	fill_button_event (&ev, click, n_press, x, y);
	verne_set_current_event (widget, &ev);
	handled = emit_widget_event (widget, "button-press-event", &ev);
	if (!handled && v && v->button_press)
		handled = v->button_press (widget, &ev.button);
	verne_clear_current_event ();
	/* Do not claim on press. Claiming takes a GTK4 pointer grab which
	 * suppresses GtkEventControllerMotion, so icon/list DnD never starts.
	 * GtkGestureDrag (grouped below) delivers button-down motion instead.
	 */
	(void) handled;
}

static void
on_released (GtkGestureClick *click, gint n_press, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (click));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEvent ev;

	/* A live GdkDrag owns the pointer; synthesizing BUTTON_RELEASE here
	 * would cancel GTK4 DND. Complete the in-process drop instead. */
	if (g_object_get_data (G_OBJECT (widget), "verne-active-drag")) {
		verne_dnd_gesture_end (widget);
		return;
	}

	fill_button_event (&ev, click, n_press, x, y);
	ev.button.type = GDK_BUTTON_RELEASE;
	verne_set_current_event (widget, &ev);
	if (!emit_widget_event (widget, "button-release-event", &ev) && v && v->button_release)
		v->button_release (widget, &ev.button);
	verne_clear_current_event ();
}

static void
on_motion (GtkEventControllerMotion *motion, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));
	GdkEvent *ge;
	guint state = 0;
	guint32 time = GDK_CURRENT_TIME;

	ge = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (motion));
	if (ge) {
		state = gdk_event_get_modifier_state (ge);
		time = gdk_event_get_time (ge);
	}
	emit_motion (widget, x, y, state, time);
}

static void
on_drag_update (GtkGestureDrag *drag, gdouble offset_x, gdouble offset_y, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drag));
	GdkEvent *ge;
	double sx = 0, sy = 0;
	guint button;
	guint state = 0;
	guint32 time = GDK_CURRENT_TIME;

	gtk_gesture_drag_get_start_point (drag, &sx, &sy);
	button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (drag));
	if (button == 1)
		state |= GDK_BUTTON1_MASK;
	else if (button == 2)
		state |= GDK_BUTTON2_MASK;
	else if (button == 3)
		state |= GDK_BUTTON3_MASK;
	ge = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (drag));
	if (ge) {
		state |= gdk_event_get_modifier_state (ge);
		time = gdk_event_get_time (ge);
	}
	emit_motion (widget, sx + offset_x, sy + offset_y, state, time);
	if (g_object_get_data (G_OBJECT (widget), "verne-active-drag"))
		verne_dnd_local_motion (widget);
}

static void
on_local_drag_end (GtkGestureDrag *drag, gdouble offset_x, gdouble offset_y, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drag));

	(void) offset_x;
	(void) offset_y;
	(void) data;
	verne_dnd_gesture_end (widget);
}

static void
on_enter (GtkEventControllerMotion *motion, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));
	VerneVfuncs *v;
	GdkEvent ev;

	if (!GTK_IS_WIDGET (widget) || !gtk_widget_get_mapped (widget) ||
	    g_object_get_data (G_OBJECT (widget), "verne-destroyed"))
		return;
	v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));

	memset (&ev, 0, sizeof (ev));
	ev.crossing.type = GDK_ENTER_NOTIFY;
	ev.crossing.x = x;
	ev.crossing.y = y;
	ev.crossing.mode = GDK_CROSSING_NORMAL;
	verne_set_current_event (widget, &ev);
	if (!emit_widget_event (widget, "enter-notify-event", &ev) && v && v->enter)
		v->enter (widget, &ev.crossing);
	verne_clear_current_event ();
}

static void
on_leave (GtkEventControllerMotion *motion, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));
	VerneVfuncs *v;
	GdkEvent ev;

	if (!GTK_IS_WIDGET (widget) ||
	    g_object_get_data (G_OBJECT (widget), "verne-destroyed"))
		return;
	v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));

	memset (&ev, 0, sizeof (ev));
	ev.crossing.type = GDK_LEAVE_NOTIFY;
	ev.crossing.mode = GDK_CROSSING_NORMAL;
	verne_set_current_event (widget, &ev);
	if (!emit_widget_event (widget, "leave-notify-event", &ev) && v && v->leave)
		v->leave (widget, &ev.crossing);
	verne_clear_current_event ();
}

static gboolean
on_key (GtkEventControllerKey *self, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEvent ev;
	GdkEvent *ge;
	gboolean press = GPOINTER_TO_INT (data);
	gboolean handled = FALSE;
	gchar strbuf[8];
	gunichar ch;

	memset (&ev, 0, sizeof (ev));
	ev.key.type = press ? GDK_KEY_PRESS : GDK_KEY_RELEASE;
	ev.key.keyval = keyval;
	ev.key.hardware_keycode = keycode;
	ev.key.state = state;
	ev.key.window = gtk_widget_get_window (widget);
	ge = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (self));
	ev.key.time = ge ? gdk_event_get_time (ge) : GDK_CURRENT_TIME;
	ch = gdk_keyval_to_unicode (keyval);
	if (ch != 0 && !g_unichar_iscntrl (ch)) {
		gint n = g_unichar_to_utf8 (ch, strbuf);
		strbuf[n] = '\0';
		ev.key.string = strbuf;
		ev.key.length = n;
	}
	verne_set_current_event (widget, &ev);
	if (press) {
		if (emit_widget_event (widget, "key-press-event", &ev))
			handled = TRUE;
		else if (v && v->key_press)
			handled = v->key_press (widget, &ev.key);
	} else {
		if (emit_widget_event (widget, "key-release-event", &ev))
			handled = TRUE;
		else if (v && v->key_release)
			handled = v->key_release (widget, &ev.key);
	}
	verne_clear_current_event ();
	return handled;
}

static gboolean
on_scroll (GtkEventControllerScroll *self, gdouble dx, gdouble dy, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEvent ev;

	memset (&ev, 0, sizeof (ev));
	ev.scroll.type = GDK_SCROLL;
	ev.scroll.delta_x = dx;
	ev.scroll.delta_y = dy;
	if (dy > 0)
		ev.scroll.direction = GDK_SCROLL_DOWN;
	else if (dy < 0)
		ev.scroll.direction = GDK_SCROLL_UP;
	else if (dx > 0)
		ev.scroll.direction = GDK_SCROLL_RIGHT;
	else
		ev.scroll.direction = GDK_SCROLL_LEFT;
	ev.scroll.window = gtk_widget_get_window (widget);
	verne_set_current_event (widget, &ev);
	if (emit_widget_event (widget, "scroll-event", &ev)) {
		verne_clear_current_event ();
		return TRUE;
	}
	if (v && v->scroll) {
		gboolean handled = v->scroll (widget, &ev.scroll);
		verne_clear_current_event ();
		return handled;
	}
	verne_clear_current_event ();
	return FALSE;
}

static void
on_focus_enter (GtkEventControllerFocus *self, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEventFocus ev;

	memset (&ev, 0, sizeof (ev));
	ev.type = GDK_FOCUS_CHANGE;
	ev.in = TRUE;
	if (!emit_widget_event (widget, "focus-in-event", &ev) && v && v->focus_in)
		v->focus_in (widget, &ev);
}

static void
on_focus_leave (GtkEventControllerFocus *self, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEventFocus ev;

	memset (&ev, 0, sizeof (ev));
	ev.type = GDK_FOCUS_CHANGE;
	ev.in = FALSE;
	if (!emit_widget_event (widget, "focus-out-event", &ev) && v && v->focus_out)
		v->focus_out (widget, &ev);
}

static gboolean
on_close_request (GtkWindow *window, gpointer data)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (window));
	GdkEventAny ev;
	gboolean handled = FALSE;

	memset (&ev, 0, sizeof (ev));
	ev.type = GDK_DELETE;
	handled = emit_widget_event (GTK_WIDGET (window), "delete-event", &ev);
	if (!handled && v && v->delete_event)
		handled = v->delete_event (GTK_WIDGET (window), &ev);
	return handled;
}

static void
on_window_state_notify (GtkWindow *window, GParamSpec *pspec, gpointer data)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (window));
	GdkEventWindowState ev;

	memset (&ev, 0, sizeof (ev));
	ev.type = GDK_WINDOW_STATE;
	ev.window = gtk_widget_get_window (GTK_WIDGET (window));
	if (gtk_window_is_maximized (window))
		ev.new_window_state |= GDK_WINDOW_STATE_MAXIMIZED;
	if (gtk_window_is_fullscreen (window))
		ev.new_window_state |= GDK_WINDOW_STATE_FULLSCREEN;
	ev.changed_mask = GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN;
	emit_widget_event (GTK_WIDGET (window), "window-state-event", &ev);
	if (v && v->window_state)
		v->window_state (GTK_WIDGET (window), &ev);
	(void) pspec;
	(void) data;
}

static void
ensure_controllers (GtkWidget *widget)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GtkGesture *click;
	GtkEventController *motion, *key, *scroll, *focus;

	if (g_object_get_qdata (G_OBJECT (widget), verne_controllers_quark))
		return;
	g_object_set_qdata (G_OBJECT (widget), verne_controllers_quark, GINT_TO_POINTER (1));

	click = gtk_gesture_click_new ();
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), 0);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (click), GTK_PHASE_CAPTURE);
	g_signal_connect (click, "pressed", G_CALLBACK (on_pressed), NULL);
	g_signal_connect (click, "released", G_CALLBACK (on_released), NULL);
	gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (click));

	{
		GtkGesture *drag = gtk_gesture_drag_new ();
		gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (drag), 1);
		gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (drag), GTK_PHASE_CAPTURE);
		g_signal_connect (drag, "drag-update", G_CALLBACK (on_drag_update), NULL);
		g_signal_connect (drag, "drag-end", G_CALLBACK (on_local_drag_end), NULL);
		gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (drag));
		gtk_gesture_group (click, drag);
	}

	motion = gtk_event_controller_motion_new ();
	gtk_event_controller_set_propagation_phase (motion, GTK_PHASE_CAPTURE);
	g_signal_connect (motion, "motion", G_CALLBACK (on_motion), NULL);
	g_signal_connect (motion, "enter", G_CALLBACK (on_enter), NULL);
	g_signal_connect (motion, "leave", G_CALLBACK (on_leave), NULL);
	gtk_widget_add_controller (widget, motion);

	key = gtk_event_controller_key_new ();
	g_signal_connect (key, "key-pressed", G_CALLBACK (on_key), GINT_TO_POINTER (TRUE));
	g_signal_connect (key, "key-released", G_CALLBACK (on_key), GINT_TO_POINTER (FALSE));
	gtk_widget_add_controller (widget, key);

	scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
	g_signal_connect (scroll, "scroll", G_CALLBACK (on_scroll), NULL);
	gtk_widget_add_controller (widget, scroll);

	focus = gtk_event_controller_focus_new ();
	g_signal_connect (focus, "enter", G_CALLBACK (on_focus_enter), NULL);
	g_signal_connect (focus, "leave", G_CALLBACK (on_focus_leave), NULL);
	gtk_widget_add_controller (widget, focus);

	if (GTK_IS_WINDOW (widget)) {
		g_signal_connect (widget, "close-request", G_CALLBACK (on_close_request), NULL);
		g_signal_connect (widget, "notify::maximized", G_CALLBACK (on_window_state_notify), NULL);
		g_signal_connect (widget, "notify::fullscreened", G_CALLBACK (on_window_state_notify), NULL);
	}
	(void) v;
}

static VerneVfuncs *
lookup_vfuncs_realize (GType type)
{
	VerneVfuncs *v;

	if (vfunc_table == NULL)
		return NULL;
	while (type != 0 && type != GTK_TYPE_WIDGET && type != G_TYPE_OBJECT) {
		v = g_hash_table_lookup (vfunc_table, GSIZE_TO_POINTER (type));
		if (v != NULL && !v->in_realize)
			return v;
		type = g_type_parent (type);
	}
	return NULL;
}

static VerneVfuncs *
lookup_vfuncs_unrealize (GType type)
{
	VerneVfuncs *v;

	if (vfunc_table == NULL)
		return NULL;
	while (type != 0 && type != GTK_TYPE_WIDGET && type != G_TYPE_OBJECT) {
		v = g_hash_table_lookup (vfunc_table, GSIZE_TO_POINTER (type));
		if (v != NULL && !v->in_unrealize)
			return v;
		type = g_type_parent (type);
	}
	return NULL;
}

static VerneVfuncs *
lookup_vfuncs_show (GType type)
{
	VerneVfuncs *v;

	if (vfunc_table == NULL)
		return NULL;
	while (type != 0 && type != GTK_TYPE_WIDGET && type != G_TYPE_OBJECT) {
		v = g_hash_table_lookup (vfunc_table, GSIZE_TO_POINTER (type));
		if (v != NULL && !v->in_show)
			return v;
		type = g_type_parent (type);
	}
	return NULL;
}

static VerneVfuncs *
lookup_vfuncs_size_allocate (GType type)
{
	VerneVfuncs *v;

	if (vfunc_table == NULL)
		return NULL;
	while (type != 0 && type != GTK_TYPE_WIDGET && type != G_TYPE_OBJECT) {
		v = g_hash_table_lookup (vfunc_table, GSIZE_TO_POINTER (type));
		if (v != NULL && !v->in_size_allocate)
			return v;
		type = g_type_parent (type);
	}
	return NULL;
}

static void
call_gtk4_realize (GtkWidget *widget, VerneVfuncs *v)
{
	if (gtk_widget_get_realized (widget))
		return;
	if (v && v->orig_realize)
		v->orig_realize (widget);
	else if (orig_gtk_widget_realize)
		orig_gtk_widget_realize (widget);
}

static GQuark
verne_presenting_quark (void)
{
	static GQuark q;
	if (q == 0)
		q = g_quark_from_static_string ("verne-presenting-window");
	return q;
}

static void
verne_window_present_safe (GtkWindow *window)
{
	GtkWidget *widget;

	g_return_if_fail (GTK_IS_WINDOW (window));
	widget = GTK_WIDGET (window);

	if (g_object_get_qdata (G_OBJECT (window), verne_presenting_quark ()))
		return;
	g_object_set_qdata (G_OBJECT (window), verne_presenting_quark (), GINT_TO_POINTER (1));

	verne_prepare_dialog (widget);

	if (!gtk_widget_get_visible (widget))
		gtk_widget_set_visible (widget, TRUE);

	if (!gtk_widget_get_realized (widget))
		gtk_widget_realize (widget);

	if (gtk_widget_get_realized (widget) && !gtk_widget_get_mapped (widget))
		gtk_widget_map (widget);

	if (gtk_widget_get_realized (widget))
		gtk_window_present (window);

	g_object_set_qdata (G_OBJECT (window), verne_presenting_quark (), NULL);
}

static void
wrapped_realize (GtkWidget *widget)
{
	VerneVfuncs *v = lookup_vfuncs_realize (G_OBJECT_TYPE (widget));

	ensure_controllers (widget);
	if (v == NULL) {
		if (orig_gtk_widget_realize)
			orig_gtk_widget_realize (widget);
		return;
	}

	v->in_realize = TRUE;

	/* GTK3 realize handlers usually chain to the parent class. Run them
	 * first so that chain reaches GTK4 native realize. Handlers that do
	 * not chain (and gtk_widget_set_realized is a no-op here) leave the
	 * widget unrealized; fall back to the saved GTK4 vfunc. */
	if (v->realize)
		v->realize (widget);

	call_gtk4_realize (widget, v);

	v->in_realize = FALSE;
}

static void
wrapped_unrealize (GtkWidget *widget)
{
	VerneVfuncs *v = lookup_vfuncs_unrealize (G_OBJECT_TYPE (widget));

	if (v == NULL)
		return;

	v->in_unrealize = TRUE;
	if (v->unrealize)
		v->unrealize (widget);
	if (gtk_widget_get_realized (widget) && v->orig_unrealize)
		v->orig_unrealize (widget);
	v->in_unrealize = FALSE;
}

static void
wrapped_state_flags_changed (GtkWidget *widget, GtkStateFlags previous)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));

	if (v && v->state_changed)
		v->state_changed (widget, previous);
	if (v && v->orig_state_flags_changed)
		v->orig_state_flags_changed (widget, previous);
}

static void
wrapped_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	GType type;
	VerneDrawEvent draw = NULL;
	void (*orig_snapshot) (GtkWidget *, GtkSnapshot *) = NULL;
	int w, h;
	cairo_t *cr;
	GtkWidget *child;

	if (g_object_get_data (G_OBJECT (widget), "verne-in-snapshot")) {
		if (gtk_widget_get_first_child (widget)) {
			for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
				if (gtk_widget_get_width (child) <= 0 || gtk_widget_get_height (child) <= 0)
					continue;
				gtk_widget_snapshot_child (widget, child, snapshot);
			}
		}
		return;
	}
	g_object_set_data (G_OBJECT (widget), "verne-in-snapshot", GINT_TO_POINTER (1));

	w = gtk_widget_get_width (widget);
	h = gtk_widget_get_height (widget);
	if (w <= 0 || h <= 0) {
		GtkAllocation allocation;
		gtk_widget_get_allocation (widget, &allocation);
		if (w <= 0)
			w = allocation.width;
		if (h <= 0)
			h = allocation.height;
	}

	/* Walk the instance type so a subclass that only overrides events still
	 * runs the ancestor GTK3 draw (EelCanvas) and the real GTK4 snapshot
	 * (GtkLayout). orig_snapshot is often wrapped_snapshot itself. */
	type = G_OBJECT_TYPE (widget);
	while (type != 0 && type != GTK_TYPE_WIDGET && type != G_TYPE_OBJECT) {
		VerneVfuncs *cand = NULL;
		if (vfunc_table)
			cand = g_hash_table_lookup (vfunc_table, GSIZE_TO_POINTER (type));
		if (cand) {
			if (draw == NULL && cand->draw)
				draw = cand->draw;
			if (orig_snapshot == NULL && cand->orig_snapshot &&
			    cand->orig_snapshot != wrapped_snapshot)
				orig_snapshot = cand->orig_snapshot;
		}
		type = g_type_parent (type);
	}

	verne_paint_desktop_wallpaper (widget, snapshot, w, h);

	if (w > 0 && h > 0 && (draw || verne_widget_has_draw_handlers (widget))) {
		cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT (0, 0, w, h));
		verne_paint_desktop_wallpaper_cairo (widget, cr, w, h);
		if (draw)
			draw (widget, cr);
		verne_emit_draw_signal (widget, cr);
		cairo_destroy (cr);
	}

	if (orig_snapshot)
		orig_snapshot (widget, snapshot);
	else {
		for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
			if (gtk_widget_get_width (child) <= 0 || gtk_widget_get_height (child) <= 0)
				continue;
			gtk_widget_snapshot_child (widget, child, snapshot);
		}
	}
	g_object_set_data (G_OBJECT (widget), "verne-in-snapshot", NULL);
}

static void
wrapped_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
	VerneVfuncs *v = lookup_vfuncs_size_allocate (G_OBJECT_TYPE (widget));
	GtkAllocation alloc;

	alloc.x = 0;
	alloc.y = 0;
	alloc.width = width;
	alloc.height = height;

	if (v == NULL)
		return;

	v->in_size_allocate = TRUE;
	/* GTK4 must record the allocation; gtk_widget_set_allocation is a no-op
	 * in the compat layer, so GTK3 size_allocate alone leaves children
	 * unallocated and snapshot/malloc-corrupts. */
	if (v->orig_size_allocate)
		v->orig_size_allocate (widget, width, height, baseline);
	if (v->size_allocate)
		v->size_allocate (widget, &alloc);
	{
		GtkWidget *child;
		for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
			int cw = gtk_widget_get_width (child);
			int ch = gtk_widget_get_height (child);
			if (!gtk_widget_get_visible (child) || !gtk_widget_get_child_visible (child))
				continue;
			if (cw > 0 && ch > 0)
				continue;
			gtk_widget_allocate (child, 1, 1, -1, NULL);
		}
	}
	v->in_size_allocate = FALSE;
}

static void
wrapped_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
		 int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	gint min = 0, nat = 0;

	if (v && orientation == GTK_ORIENTATION_HORIZONTAL && v->pref_width)
		v->pref_width (widget, &min, &nat);
	else if (v && orientation == GTK_ORIENTATION_VERTICAL && v->pref_height)
		v->pref_height (widget, &min, &nat);
	else if (v && v->orig_measure) {
		v->orig_measure (widget, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
		return;
	}
	if (minimum)
		*minimum = min;
	if (natural)
		*natural = nat;
	if (minimum_baseline)
		*minimum_baseline = -1;
	if (natural_baseline)
		*natural_baseline = -1;
}

static void
wrapped_dispose (GObject *object)
{
	GType type;
	VerneVfuncs *v;
	void (*real_dispose) (GObject *) = NULL;

	if (g_object_get_data (object, "verne-disposing"))
		return;
	g_object_set_data (object, "verne-disposing", GINT_TO_POINTER (1));

	if (!g_object_get_data (object, "verne-destroyed")) {
		g_object_set_data (object, "verne-destroyed", GINT_TO_POINTER (1));
		v = lookup_vfuncs_type (G_OBJECT_TYPE (object));
		if (v && v->destroy)
			v->destroy (GTK_WIDGET (object));
	} else {
		v = lookup_vfuncs_type (G_OBJECT_TYPE (object));
	}

	/* orig_dispose on a subclass is often wrapped_dispose itself (parent
	 * class already wrapped). Walk to the first real GObject dispose. */
	for (type = G_OBJECT_TYPE (object); type != 0 && type != G_TYPE_NONE; type = g_type_parent (type)) {
		VerneVfuncs *cur = (vfunc_table != NULL)
			? g_hash_table_lookup (vfunc_table, GSIZE_TO_POINTER (type))
			: NULL;
		if (cur && cur->orig_dispose && cur->orig_dispose != wrapped_dispose) {
			real_dispose = cur->orig_dispose;
			break;
		}
	}
	if (real_dispose)
		real_dispose (object);
}

static void
wrapped_show (GtkWidget *widget)
{
	VerneVfuncs *v = lookup_vfuncs_show (G_OBJECT_TYPE (widget));

	if (v) {
		v->in_show = TRUE;
		if (v->show)
			v->show (widget);
		else if (v->orig_show)
			v->orig_show (widget);
		else
			gtk_widget_set_visible (widget, TRUE);
		v->in_show = FALSE;
	} else {
		gtk_widget_set_visible (widget, TRUE);
	}

	/* GTK4 gtk_widget_show / gtk_widget_real_show do not map toplevels.
	 * Present after the GTK3 show hook so Adwaita chrome actually appears.
	 * GtkMenu is a GtkWindow but GTK3 show() on a menu does not pop it up. */
	if (GTK_IS_WINDOW (widget) && !GTK_IS_MENU (widget))
		verne_window_present_safe (GTK_WINDOW (widget));
}

#undef gtk_widget_show
void
verne_gtk_widget_show (GtkWidget *widget)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));
	/* GTK3 gtk_widget_show() on a GtkMenu does not pop it up. */
	if (GTK_IS_POPOVER (widget) || GTK_IS_MENU (widget))
		return;
	gtk_widget_set_visible (widget, TRUE);
	if (GTK_IS_WINDOW (widget))
		verne_window_present_safe (GTK_WINDOW (widget));
}

#undef gtk_widget_realize
void
verne_gtk_widget_realize (GtkWidget *widget)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));
	/* GtkPopover is a GtkNative but still needs a parent surface. Realizing
	 * an unrooted menu/popover creates a popup with a NULL parent and aborts. */
	if (GTK_IS_POPOVER (widget))
		return;
	if (!GTK_IS_WINDOW (widget) && gtk_widget_get_root (widget) == NULL)
		return;
	(gtk_widget_realize) (widget);
}

static void
wrapped_hide (GtkWidget *widget)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));

	if (v && v->hide)
		v->hide (widget);
	else if (v && v->orig_hide)
		v->orig_hide (widget);
	else
		gtk_widget_set_visible (widget, FALSE);
}

static void
wrapped_css_changed (GtkWidget *widget, GtkCssStyleChange *change)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));

	if (v && v->orig_css_changed)
		v->orig_css_changed (widget, change);
	if (v && v->style_updated)
		v->style_updated (widget);
}

static void
wrap_class (VerneVfuncs *v)
{
	GObjectClass *oclass;
	GtkWidgetClass *wclass;

	wclass = v->klass;
	oclass = G_OBJECT_CLASS (wclass);

	if (!v->wrapped) {
		v->wrapped = TRUE;
		v->orig_snapshot = wclass->snapshot;
		v->orig_realize = wclass->realize;
		v->orig_unrealize = wclass->unrealize;
		v->orig_size_allocate = wclass->size_allocate;
		v->orig_measure = wclass->measure;
		v->orig_dispose = oclass->dispose;
		v->orig_state_flags_changed = wclass->state_flags_changed;
		v->orig_show = wclass->show;
		v->orig_hide = wclass->hide;
		v->orig_css_changed = wclass->css_changed;
		wclass->realize = wrapped_realize;
		wclass->unrealize = wrapped_unrealize;
	}
	if (v->state_changed)
		wclass->state_flags_changed = wrapped_state_flags_changed;
	/* Always intercept snapshot so ancestor GTK3 draw (EelCanvas) runs even
	 * when a subclass only overrides events and copies wrapped_snapshot. */
	wclass->snapshot = wrapped_snapshot;
	if (v->size_allocate)
		wclass->size_allocate = wrapped_size_allocate;
	if (v->pref_width || v->pref_height)
		wclass->measure = wrapped_measure;
	if (v->destroy)
		oclass->dispose = wrapped_dispose;
	if (v->show || g_type_is_a (G_TYPE_FROM_CLASS (oclass), GTK_TYPE_WINDOW))
		wclass->show = wrapped_show;
	if (v->hide)
		wclass->hide = wrapped_hide;
	if (v->style_updated)
		wclass->css_changed = wrapped_css_changed;
}

#define SETTER(field) \
	VerneVfuncs *v = ensure_vfuncs (klass); \
	v->field = handler; \
	wrap_class (v);

void verne_widget_class_set_button_press_event (GtkWidgetClass *klass, VerneButtonEvent handler) { SETTER (button_press); }
void verne_widget_class_set_button_release_event (GtkWidgetClass *klass, VerneButtonEvent handler) { SETTER (button_release); }
void verne_widget_class_set_motion_notify_event (GtkWidgetClass *klass, VerneMotionEvent handler) { SETTER (motion); }
void verne_widget_class_set_key_press_event (GtkWidgetClass *klass, VerneKeyEvent handler) { SETTER (key_press); }
void verne_widget_class_set_key_release_event (GtkWidgetClass *klass, VerneKeyEvent handler) { SETTER (key_release); }
void verne_widget_class_set_scroll_event (GtkWidgetClass *klass, VerneScrollEvent handler) { SETTER (scroll); }
void verne_widget_class_set_enter_notify_event (GtkWidgetClass *klass, VerneCrossingEvent handler) { SETTER (enter); }
void verne_widget_class_set_leave_notify_event (GtkWidgetClass *klass, VerneCrossingEvent handler) { SETTER (leave); }
void verne_widget_class_set_focus_in_event (GtkWidgetClass *klass, VerneFocusEvent handler) { SETTER (focus_in); }
void verne_widget_class_set_focus_out_event (GtkWidgetClass *klass, VerneFocusEvent handler) { SETTER (focus_out); }
void verne_widget_class_set_draw (GtkWidgetClass *klass, VerneDrawEvent handler) { SETTER (draw); }
void verne_widget_class_set_size_allocate (GtkWidgetClass *klass, VerneSizeAllocate handler) { SETTER (size_allocate); }
void verne_widget_class_set_destroy (GtkWidgetClass *klass, VerneDestroyFunc handler) { SETTER (destroy); }
void verne_widget_class_set_get_preferred_width (GtkWidgetClass *klass, VernePreferredSize handler) { SETTER (pref_width); }
void verne_widget_class_set_get_preferred_height (GtkWidgetClass *klass, VernePreferredSize handler) { SETTER (pref_height); }
void verne_widget_class_set_delete_event (GtkWidgetClass *klass, VerneDeleteEvent handler) { SETTER (delete_event); }
void verne_widget_class_set_popup_menu (GtkWidgetClass *klass, VernePopupMenu handler) { SETTER (popup_menu); }
void verne_widget_class_set_style_updated (GtkWidgetClass *klass, VerneStyleUpdated handler) { SETTER (style_updated); }
void verne_widget_class_set_grab_notify (GtkWidgetClass *klass, VerneGrabNotify handler) { SETTER (grab_notify); }
void verne_widget_class_set_show (GtkWidgetClass *klass, VerneShowFunc handler) { SETTER (show); }
void verne_widget_class_set_configure_event (GtkWidgetClass *klass, gpointer handler) { (void) klass; (void) handler; }
void verne_widget_class_set_get_accessible (GtkWidgetClass *klass, gpointer handler) { SETTER (get_accessible); }
void verne_widget_class_set_realize (GtkWidgetClass *klass, void (*handler) (GtkWidget *)) { SETTER (realize); }
void verne_widget_class_set_unrealize (GtkWidgetClass *klass, void (*handler) (GtkWidget *)) { SETTER (unrealize); }
void verne_widget_class_set_state_changed (GtkWidgetClass *klass, void (*handler) (GtkWidget *, GtkStateType)) { SETTER (state_changed); }
void verne_widget_class_set_screen_changed (GtkWidgetClass *klass, gpointer handler) { (void) klass; (void) handler; }
void verne_widget_class_set_window_state_event (GtkWidgetClass *klass, VerneWindowStateEvent handler) { SETTER (window_state); }
void verne_widget_class_set_hide (GtkWidgetClass *klass, VerneShowFunc handler) { SETTER (hide); }

gboolean
verne_widget_chain_button_press (gpointer parent_class, GtkWidget *widget, GdkEventButton *event)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_TYPE_FROM_CLASS (parent_class));
	if (v && v->button_press)
		return v->button_press (widget, event);
	return FALSE;
}

gboolean
verne_widget_chain_button_release (gpointer parent_class, GtkWidget *widget, GdkEventButton *event)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_TYPE_FROM_CLASS (parent_class));
	if (v && v->button_release)
		return v->button_release (widget, event);
	return FALSE;
}

gboolean
verne_widget_chain_motion (gpointer parent_class, GtkWidget *widget, GdkEventMotion *event)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_TYPE_FROM_CLASS (parent_class));
	if (v && v->motion)
		return v->motion (widget, event);
	return FALSE;
}

gboolean
verne_widget_chain_key_press (gpointer parent_class, GtkWidget *widget, GdkEventKey *event)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_TYPE_FROM_CLASS (parent_class));
	if (v && v->key_press)
		return v->key_press (widget, event);
	return FALSE;
}

gboolean
verne_widget_chain_key_release (gpointer parent_class, GtkWidget *widget, GdkEventKey *event)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_TYPE_FROM_CLASS (parent_class));
	if (v && v->key_release)
		return v->key_release (widget, event);
	return FALSE;
}

gboolean
verne_widget_chain_scroll (gpointer parent_class, GtkWidget *widget, GdkEventScroll *event)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_TYPE_FROM_CLASS (parent_class));
	if (v && v->scroll)
		return v->scroll (widget, event);
	return FALSE;
}

gboolean
verne_widget_chain_draw (gpointer parent_class, GtkWidget *widget, cairo_t *cr)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_TYPE_FROM_CLASS (parent_class));
	if (v && v->draw)
		return v->draw (widget, cr);
	return FALSE;
}

void
verne_widget_chain_size_allocate (gpointer parent_class, GtkWidget *widget, GtkAllocation *allocation)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_TYPE_FROM_CLASS (parent_class));
	if (v && v->size_allocate)
		v->size_allocate (widget, allocation);
}

void
verne_widget_chain_destroy (gpointer parent_class, GtkWidget *widget)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_TYPE_FROM_CLASS (parent_class));
	if (v && v->destroy)
		v->destroy (widget);
}

void
verne_widget_invoke_destroy (GtkWidget *widget)
{
	VerneVfuncs *v;

	if (widget == NULL)
		return;
	if (g_object_get_data (G_OBJECT (widget), "verne-destroyed"))
		return;
	g_object_set_data (G_OBJECT (widget), "verne-destroyed", GINT_TO_POINTER (1));
	v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	if (v && v->destroy)
		v->destroy (widget);
}

void
verne_widget_chain_show (gpointer parent_class, GtkWidget *widget)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_TYPE_FROM_CLASS (parent_class));
	if (v && v->orig_show)
		v->orig_show (widget);
	else
		gtk_widget_set_visible (widget, TRUE);
}

gboolean
verne_widget_chain_focus_in (gpointer parent_class, GtkWidget *widget, GdkEventFocus *event)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_TYPE_FROM_CLASS (parent_class));
	if (v && v->focus_in)
		return v->focus_in (widget, event);
	return FALSE;
}

gboolean
gtk_widget_event (GtkWidget *widget, GdkEvent *event)
{
	const char *name = NULL;
	gboolean handled;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		name = "button-press-event";
		break;
	case GDK_BUTTON_RELEASE:
		name = "button-release-event";
		break;
	case GDK_MOTION_NOTIFY:
		name = "motion-notify-event";
		break;
	case GDK_KEY_PRESS:
		name = "key-press-event";
		break;
	case GDK_KEY_RELEASE:
		name = "key-release-event";
		break;
	case GDK_SCROLL:
		name = "scroll-event";
		break;
	case GDK_ENTER_NOTIFY:
		name = "enter-notify-event";
		break;
	case GDK_LEAVE_NOTIFY:
		name = "leave-notify-event";
		break;
	case GDK_FOCUS_CHANGE:
		name = event->focus_change.in ? "focus-in-event" : "focus-out-event";
		break;
	case GDK_DELETE:
		name = "delete-event";
		break;
	default:
		if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)
			name = "button-press-event";
		else if (event->type == GDK_WINDOW_STATE)
			name = "window-state-event";
		break;
	}
	if (name == NULL)
		return FALSE;
	verne_set_current_event (widget, event);
	handled = emit_widget_event (widget, name, event);
	verne_clear_current_event ();
	return handled;
}

static void
global_widget_realize (GtkWidget *widget)
{
	ensure_controllers (widget);
	if (GTK_IS_DRAWING_AREA (widget) &&
	    g_object_get_data (G_OBJECT (widget), "verne-draw-func") == NULL) {
		gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (widget),
						verne_drawing_area_draw, NULL, NULL);
		g_object_set_data (G_OBJECT (widget), "verne-draw-func", GINT_TO_POINTER (1));
	}
	if (orig_gtk_widget_realize)
		orig_gtk_widget_realize (widget);
}

static void
register_event_signals (void)
{
	GtkWidgetClass *wclass;
	const struct {
		const char *name;
		guint n_params;
	} signals[] = {
		{ "button-press-event", 1 },
		{ "button-release-event", 1 },
		{ "motion-notify-event", 1 },
		{ "key-press-event", 1 },
		{ "key-release-event", 1 },
		{ "scroll-event", 1 },
		{ "enter-notify-event", 1 },
		{ "leave-notify-event", 1 },
		{ "focus-in-event", 1 },
		{ "focus-out-event", 1 },
		{ "delete-event", 1 },
		{ "window-state-event", 1 },
		{ "popup-menu", 0 },
		{ "selection-done", 0 },
		{ "event-after", 1 },
		{ "event_after", 1 },
		{ "focus", 1 },
		{ "size-allocate", 1 },
		{ "draw", 1 },
	};
	static const struct {
		const char *name;
		GType ret;
		guint n_params;
		GType p0, p1, p2, p3, p4, p5;
	} drag_signals[] = {
		{ "drag-begin", G_TYPE_NONE, 1, G_TYPE_POINTER, 0, 0, 0, 0, 0 },
		{ "drag-end", G_TYPE_NONE, 1, G_TYPE_POINTER, 0, 0, 0, 0, 0 },
		{ "drag-data-delete", G_TYPE_NONE, 1, G_TYPE_POINTER, 0, 0, 0, 0, 0 },
		{ "drag-leave", G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_UINT, 0, 0, 0, 0 },
		{ "drag-motion", G_TYPE_BOOLEAN, 4, G_TYPE_POINTER, G_TYPE_INT, G_TYPE_INT, G_TYPE_UINT, 0, 0 },
		{ "drag-drop", G_TYPE_BOOLEAN, 4, G_TYPE_POINTER, G_TYPE_INT, G_TYPE_INT, G_TYPE_UINT, 0, 0 },
		{ "drag-failed", G_TYPE_BOOLEAN, 2, G_TYPE_POINTER, G_TYPE_INT, 0, 0, 0, 0 },
		{ "drag-data-get", G_TYPE_NONE, 4, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT, 0, 0 },
		{ "drag-data-received", G_TYPE_NONE, 6, G_TYPE_POINTER, G_TYPE_INT, G_TYPE_INT, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT },
	};
	guint i;

	if (event_signals_registered)
		return;
	event_signals_registered = TRUE;
	wclass = g_type_class_ref (GTK_TYPE_WIDGET);
	for (i = 0; i < G_N_ELEMENTS (signals); i++) {
		if (g_signal_lookup (signals[i].name, GTK_TYPE_WIDGET) != 0)
			continue;
		if (signals[i].n_params == 0) {
			g_signal_new (signals[i].name, GTK_TYPE_WIDGET,
				      G_SIGNAL_RUN_LAST, 0,
				      g_signal_accumulator_true_handled, NULL,
				      NULL, G_TYPE_BOOLEAN, 0);
		} else {
			g_signal_new (signals[i].name, GTK_TYPE_WIDGET,
				      G_SIGNAL_RUN_LAST, 0,
				      g_signal_accumulator_true_handled, NULL,
				      NULL, G_TYPE_BOOLEAN, 1, G_TYPE_POINTER);
		}
	}
	for (i = 0; i < G_N_ELEMENTS (drag_signals); i++) {
		GType params[6];
		guint n;

		if (g_signal_lookup (drag_signals[i].name, GTK_TYPE_WIDGET) != 0)
			continue;
		n = drag_signals[i].n_params;
		params[0] = drag_signals[i].p0;
		params[1] = drag_signals[i].p1;
		params[2] = drag_signals[i].p2;
		params[3] = drag_signals[i].p3;
		params[4] = drag_signals[i].p4;
		params[5] = drag_signals[i].p5;
		g_signal_newv (drag_signals[i].name, GTK_TYPE_WIDGET,
			       G_SIGNAL_RUN_LAST, NULL,
			       drag_signals[i].ret == G_TYPE_BOOLEAN ? g_signal_accumulator_true_handled : NULL,
			       NULL, NULL, drag_signals[i].ret, n, params);
	}
	orig_gtk_widget_realize = wclass->realize;
	wclass->realize = global_widget_realize;
	verne_draw_signal_id = g_signal_lookup ("draw", GTK_TYPE_WIDGET);
	{
		GtkWidgetClass *da = GTK_WIDGET_CLASS (g_type_class_ref (GTK_TYPE_DRAWING_AREA));
		if (da->realize != verne_drawing_area_realize) {
			orig_drawing_area_realize = da->realize;
			da->realize = verne_drawing_area_realize;
		}
		g_type_class_unref (da);
	}
}

static void
verne_set_uninstalled_schema_dir (void)
{
#ifdef VERNE_UNINSTALLED_SCHEMA_DIR
	const char *cur = g_getenv ("GSETTINGS_SCHEMA_DIR");
	if (cur == NULL || cur[0] == '\0')
		g_setenv ("GSETTINGS_SCHEMA_DIR", VERNE_UNINSTALLED_SCHEMA_DIR, FALSE);
	else if (strstr (cur, VERNE_UNINSTALLED_SCHEMA_DIR) == NULL) {
		char *joined = g_strconcat (VERNE_UNINSTALLED_SCHEMA_DIR, ":", cur, NULL);
		g_setenv ("GSETTINGS_SCHEMA_DIR", joined, TRUE);
		g_free (joined);
	}
#endif
}

#ifdef __GNUC__
__attribute__((constructor))
static void
verne_schema_dir_ctor (void)
{
	verne_set_uninstalled_schema_dir ();
}
#endif

static void
verne_crash_handler (int sig)
{
	void *frames[64];
	int n, i;
	char buf[80];
	int len;

	len = snprintf (buf, sizeof buf, "\n*** Verne crash signal %d ***\n", sig);
	if (len > 0)
		write (STDERR_FILENO, buf, (size_t) len);
	/* backtrace_symbols_fd allocates; skip it on allocator abort. */
	if (sig != SIGABRT) {
		n = backtrace (frames, 64);
		for (i = 0; i < n; i++) {
			len = snprintf (buf, sizeof buf, "  %p\n", frames[i]);
			if (len > 0)
				write (STDERR_FILENO, buf, (size_t) len);
		}
	}
	_exit (128 + sig);
}

static void
verne_install_crash_handler (void)
{
	static stack_t ss;
	struct sigaction sa;

	ss.ss_sp = malloc (SIGSTKSZ * 4);
	ss.ss_size = ss.ss_sp ? SIGSTKSZ * 4 : 0;
	ss.ss_flags = 0;
	if (ss.ss_sp)
		sigaltstack (&ss, NULL);

	memset (&sa, 0, sizeof sa);
	sa.sa_handler = verne_crash_handler;
	sa.sa_flags = SA_ONSTACK;
	sigaction (SIGSEGV, &sa, NULL);
	sigaction (SIGABRT, &sa, NULL);
	sigaction (SIGBUS, &sa, NULL);
}

static gboolean verne_forcing_icon_theme;

static void
verne_force_adwaita_icon_theme (GtkSettings *settings)
{
	gchar *name = NULL;

	if (settings == NULL || verne_forcing_icon_theme)
		return;
	g_object_get (settings, "gtk-icon-theme-name", &name, NULL);
	if (g_strcmp0 (name, "Adwaita") != 0) {
		verne_forcing_icon_theme = TRUE;
		g_object_set (settings, "gtk-icon-theme-name", "Adwaita", NULL);
		verne_forcing_icon_theme = FALSE;
	}
	g_free (name);
}

static void
verne_on_icon_theme_changed (GObject *settings, GParamSpec *pspec, gpointer data)
{
	(void) pspec;
	(void) data;
	verne_force_adwaita_icon_theme (GTK_SETTINGS (settings));
}

void
verne_compat_init (void)
{
	static gboolean inited;
	GtkSettings *settings;

	verne_set_uninstalled_schema_dir ();
	if (inited)
		return;
	inited = TRUE;
	verne_install_crash_handler ();
	verne_controllers_quark = g_quark_from_static_string ("verne-controllers");
	if (vfunc_table == NULL)
		vfunc_table = g_hash_table_new (g_direct_hash, g_direct_equal);
	register_event_signals ();

	settings = gtk_settings_get_default ();
	if (settings) {
		g_object_set (settings,
			      "gtk-theme-name", "Adwaita",
			      "gtk-icon-theme-name", "Adwaita",
			      NULL);
		g_signal_connect (settings, "notify::gtk-icon-theme-name",
				  G_CALLBACK (verne_on_icon_theme_changed), NULL);
		verne_force_adwaita_icon_theme (settings);
	}
}

GdkSurface *
gtk_widget_get_window (GtkWidget *widget)
{
	GtkNative *native;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
	native = gtk_widget_get_native (widget);
	if (native == NULL)
		return NULL;
	return gtk_native_get_surface (native);
}

gboolean
gtk_widget_is_drawable (GtkWidget *widget)
{
	return gtk_widget_get_mapped (widget);
}

void
gtk_widget_get_pointer (GtkWidget *widget, gint *x, gint *y)
{
	GdkSeat *seat;
	GdkDevice *pointer;
	gdouble dx = 0, dy = 0;

	seat = gdk_display_get_default_seat (gtk_widget_get_display (widget));
	pointer = seat ? gdk_seat_get_pointer (seat) : NULL;
	if (pointer)
		gdk_device_get_surface_at_position (pointer, &dx, &dy);
	if (x) *x = (gint) dx;
	if (y) *y = (gint) dy;
}

void
gtk_widget_get_allocation (GtkWidget *widget, GtkAllocation *allocation)
{
	g_return_if_fail (allocation != NULL);
	allocation->x = 0;
	allocation->y = 0;
	allocation->width = gtk_widget_get_width (widget);
	allocation->height = gtk_widget_get_height (widget);
}

void
gtk_widget_set_allocation (GtkWidget *widget, const GtkAllocation *allocation)
{
	(void) widget;
	(void) allocation;
}

int
verne_screen_width (void)
{
	GdkDisplay *d = gdk_display_get_default ();
	GdkMonitor *m = d ? gdk_display_get_monitor_at_surface (d, NULL) : NULL;
	GdkRectangle r;

	if (m == NULL && d != NULL) {
		GListModel *list = gdk_display_get_monitors (d);
		if (g_list_model_get_n_items (list) > 0)
			m = g_list_model_get_item (list, 0);
		if (m) {
			gdk_monitor_get_geometry (m, &r);
			g_object_unref (m);
			return r.width;
		}
	}
	if (m) {
		gdk_monitor_get_geometry (m, &r);
		return r.width;
	}
	return 1920;
}

int
verne_screen_height (void)
{
	GdkDisplay *d = gdk_display_get_default ();
	GListModel *list;
	GdkMonitor *m;
	GdkRectangle r;

	if (d == NULL)
		return 1080;
	list = gdk_display_get_monitors (d);
	if (g_list_model_get_n_items (list) == 0)
		return 1080;
	m = g_list_model_get_item (list, 0);
	gdk_monitor_get_geometry (m, &r);
	g_object_unref (m);
	return r.height;
}

int
verne_screen_n_monitors (void)
{
	GdkDisplay *d = gdk_display_get_default ();
	if (d == NULL)
		return 1;
	return (int) g_list_model_get_n_items (gdk_display_get_monitors (d));
}

gpointer
gtk_widget_get_accessible (GtkWidget *widget)
{
	VerneVfuncs *v;
	AtkObject *(*handler) (GtkWidget *);

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
	v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	if (v && v->get_accessible) {
		handler = (AtkObject * (*) (GtkWidget *)) v->get_accessible;
		return handler (widget);
	}
	return atk_gobject_accessible_for_object (G_OBJECT (widget));
}

gint
gtk_icon_size_lookup (GtkIconSize size, gint *width, gint *height)
{
	gint s = 16;
	if (size == GTK_ICON_SIZE_LARGE)
		s = 48;
	else if (size == GTK_ICON_SIZE_NORMAL)
		s = 24;
	if (width) *width = s;
	if (height) *height = s;
	return TRUE;
}

gboolean
gtk_show_uri_on_window (GtkWindow *parent, const char *uri, guint32 timestamp, GError **error)
{
	GtkUriLauncher *launcher = gtk_uri_launcher_new (uri);
	(void) timestamp;
	gtk_uri_launcher_launch (launcher, parent, NULL, NULL, NULL);
	g_object_unref (launcher);
	(void) error;
	return TRUE;
}
