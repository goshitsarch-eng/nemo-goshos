/* GTK3 class vfuncs mapped onto GTK4 event controllers / snapshot / measure. */
#include "config.h"
#include "verne-gtk-compat.h"

#include <graphene.h>

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
	gboolean wrapped;
} VerneVfuncs;

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
fill_button_event (GdkEventButton *ev, GtkGestureClick *click, gint n_press, gdouble x, gdouble y)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (click));
	GdkEvent *ge = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (click));

	memset (ev, 0, sizeof (*ev));
	if (n_press >= 3)
		ev->type = GDK_3BUTTON_PRESS;
	else if (n_press == 2)
		ev->type = GDK_2BUTTON_PRESS;
	else
		ev->type = GDK_BUTTON_PRESS;
	ev->window = gtk_widget_get_window (widget);
	ev->x = x;
	ev->y = y;
	ev->x_root = x;
	ev->y_root = y;
	ev->button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));
	ev->state = ge ? gdk_event_get_modifier_state (ge) : 0;
	ev->time = ge ? gdk_event_get_time (ge) : GDK_CURRENT_TIME;
}

static void
on_pressed (GtkGestureClick *click, gint n_press, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (click));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEventButton ev;

	if (v == NULL || v->button_press == NULL)
		return;
	fill_button_event (&ev, click, n_press, x, y);
	if (v->button_press (widget, &ev))
		gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
on_released (GtkGestureClick *click, gint n_press, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (click));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEventButton ev;

	if (v == NULL || v->button_release == NULL)
		return;
	fill_button_event (&ev, click, n_press, x, y);
	ev.type = GDK_BUTTON_RELEASE;
	v->button_release (widget, &ev);
}

static void
on_motion (GtkEventControllerMotion *motion, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEventMotion ev;
	GdkEvent *ge;

	if (v == NULL || v->motion == NULL)
		return;
	memset (&ev, 0, sizeof (ev));
	ge = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (motion));
	ev.type = GDK_MOTION_NOTIFY;
	ev.window = gtk_widget_get_window (widget);
	ev.x = x;
	ev.y = y;
	ev.x_root = x;
	ev.y_root = y;
	ev.state = ge ? gdk_event_get_modifier_state (ge) : 0;
	ev.time = ge ? gdk_event_get_time (ge) : GDK_CURRENT_TIME;
	v->motion (widget, &ev);
}

static void
on_enter (GtkEventControllerMotion *motion, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEventCrossing ev;

	if (v == NULL || v->enter == NULL)
		return;
	memset (&ev, 0, sizeof (ev));
	ev.type = GDK_ENTER_NOTIFY;
	ev.x = x;
	ev.y = y;
	ev.mode = GDK_CROSSING_NORMAL;
	v->enter (widget, &ev);
}

static void
on_leave (GtkEventControllerMotion *motion, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEventCrossing ev;

	if (v == NULL || v->leave == NULL)
		return;
	memset (&ev, 0, sizeof (ev));
	ev.type = GDK_LEAVE_NOTIFY;
	ev.mode = GDK_CROSSING_NORMAL;
	v->leave (widget, &ev);
}

static gboolean
on_key (GtkEventControllerKey *self, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEventKey ev;
	gboolean press = GPOINTER_TO_INT (data);

	if (v == NULL)
		return FALSE;
	memset (&ev, 0, sizeof (ev));
	ev.type = press ? GDK_KEY_PRESS : GDK_KEY_RELEASE;
	ev.keyval = keyval;
	ev.hardware_keycode = keycode;
	ev.state = state;
	ev.window = gtk_widget_get_window (widget);
	if (press && v->key_press)
		return v->key_press (widget, &ev);
	if (!press && v->key_release)
		return v->key_release (widget, &ev);
	return FALSE;
}

static gboolean
on_scroll (GtkEventControllerScroll *self, gdouble dx, gdouble dy, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEventScroll ev;

	if (v == NULL || v->scroll == NULL)
		return FALSE;
	memset (&ev, 0, sizeof (ev));
	ev.type = GDK_SCROLL;
	ev.delta_x = dx;
	ev.delta_y = dy;
	if (dy > 0)
		ev.direction = GDK_SCROLL_DOWN;
	else if (dy < 0)
		ev.direction = GDK_SCROLL_UP;
	else if (dx > 0)
		ev.direction = GDK_SCROLL_RIGHT;
	else
		ev.direction = GDK_SCROLL_LEFT;
	ev.window = gtk_widget_get_window (widget);
	return v->scroll (widget, &ev);
}

static void
on_focus_enter (GtkEventControllerFocus *self, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEventFocus ev;

	if (v == NULL || v->focus_in == NULL)
		return;
	memset (&ev, 0, sizeof (ev));
	ev.type = GDK_FOCUS_CHANGE;
	ev.in = TRUE;
	v->focus_in (widget, &ev);
}

static void
on_focus_leave (GtkEventControllerFocus *self, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GdkEventFocus ev;

	if (v == NULL || v->focus_out == NULL)
		return;
	memset (&ev, 0, sizeof (ev));
	ev.type = GDK_FOCUS_CHANGE;
	ev.in = FALSE;
	v->focus_out (widget, &ev);
}

static gboolean
on_close_request (GtkWindow *window, gpointer data)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (window));
	GdkEventAny ev;

	if (v == NULL || v->delete_event == NULL)
		return FALSE;
	memset (&ev, 0, sizeof (ev));
	ev.type = GDK_DELETE;
	return v->delete_event (GTK_WIDGET (window), &ev);
}

static void
ensure_controllers (GtkWidget *widget)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GtkGesture *click;
	GtkEventController *motion, *key, *scroll, *focus;

	if (v == NULL)
		return;
	if (g_object_get_qdata (G_OBJECT (widget), verne_controllers_quark))
		return;
	g_object_set_qdata (G_OBJECT (widget), verne_controllers_quark, GINT_TO_POINTER (1));

	if (v->button_press || v->button_release) {
		click = gtk_gesture_click_new ();
		gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), 0);
		gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (click), GTK_PHASE_CAPTURE);
		if (v->button_press)
			g_signal_connect (click, "pressed", G_CALLBACK (on_pressed), NULL);
		if (v->button_release)
			g_signal_connect (click, "released", G_CALLBACK (on_released), NULL);
		gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (click));
	}
	if (v->motion || v->enter || v->leave) {
		motion = gtk_event_controller_motion_new ();
		if (v->motion)
			g_signal_connect (motion, "motion", G_CALLBACK (on_motion), NULL);
		if (v->enter)
			g_signal_connect (motion, "enter", G_CALLBACK (on_enter), NULL);
		if (v->leave)
			g_signal_connect (motion, "leave", G_CALLBACK (on_leave), NULL);
		gtk_widget_add_controller (widget, motion);
	}
	if (v->key_press || v->key_release) {
		key = gtk_event_controller_key_new ();
		if (v->key_press)
			g_signal_connect (key, "key-pressed", G_CALLBACK (on_key), GINT_TO_POINTER (TRUE));
		if (v->key_release)
			g_signal_connect (key, "key-released", G_CALLBACK (on_key), GINT_TO_POINTER (FALSE));
		gtk_widget_add_controller (widget, key);
	}
	if (v->scroll) {
		scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
		g_signal_connect (scroll, "scroll", G_CALLBACK (on_scroll), NULL);
		gtk_widget_add_controller (widget, scroll);
	}
	if (v->focus_in || v->focus_out) {
		focus = gtk_event_controller_focus_new ();
		if (v->focus_in)
			g_signal_connect (focus, "enter", G_CALLBACK (on_focus_enter), NULL);
		if (v->focus_out)
			g_signal_connect (focus, "leave", G_CALLBACK (on_focus_leave), NULL);
		gtk_widget_add_controller (widget, focus);
	}
	if (v->delete_event && GTK_IS_WINDOW (widget))
		g_signal_connect (widget, "close-request", G_CALLBACK (on_close_request), NULL);
}

static void
wrapped_realize (GtkWidget *widget)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));

	ensure_controllers (widget);
	if (v && v->realize)
		v->realize (widget);
	else if (v && v->orig_realize)
		v->orig_realize (widget);
}

static void
wrapped_unrealize (GtkWidget *widget)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));

	if (v && v->unrealize)
		v->unrealize (widget);
	else if (v && v->orig_unrealize)
		v->orig_unrealize (widget);
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
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	int w, h;
	cairo_t *cr;

	w = gtk_widget_get_width (widget);
	h = gtk_widget_get_height (widget);
	if (v && v->draw && w > 0 && h > 0) {
		cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT (0, 0, w, h));
		v->draw (widget, cr);
		cairo_destroy (cr);
	}
	if (v && v->orig_snapshot)
		v->orig_snapshot (widget, snapshot);
	else {
		GtkWidget *child;
		for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child))
			gtk_widget_snapshot_child (widget, child, snapshot);
	}
}

static void
wrapped_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (widget));
	GtkAllocation alloc;

	alloc.x = 0;
	alloc.y = 0;
	alloc.width = width;
	alloc.height = height;
	if (v && v->size_allocate)
		v->size_allocate (widget, &alloc);
	else if (v && v->orig_size_allocate)
		v->orig_size_allocate (widget, width, height, baseline);
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
	VerneVfuncs *v = lookup_vfuncs_type (G_OBJECT_TYPE (object));

	if (v && v->destroy)
		v->destroy (GTK_WIDGET (object));
	if (v && v->orig_dispose)
		v->orig_dispose (object);
}

static void
wrap_class (VerneVfuncs *v)
{
	GObjectClass *oclass;
	GtkWidgetClass *wclass;

	if (v->wrapped)
		return;
	v->wrapped = TRUE;
	wclass = v->klass;
	oclass = G_OBJECT_CLASS (wclass);

	v->orig_snapshot = wclass->snapshot;
	v->orig_realize = wclass->realize;
	v->orig_unrealize = wclass->unrealize;
	v->orig_size_allocate = wclass->size_allocate;
	v->orig_measure = wclass->measure;
	v->orig_dispose = oclass->dispose;
	v->orig_state_flags_changed = wclass->state_flags_changed;

	wclass->realize = wrapped_realize;
	wclass->unrealize = wrapped_unrealize;
	if (v->state_changed)
		wclass->state_flags_changed = wrapped_state_flags_changed;
	if (v->draw)
		wclass->snapshot = wrapped_snapshot;
	if (v->size_allocate)
		wclass->size_allocate = wrapped_size_allocate;
	if (v->pref_width || v->pref_height)
		wclass->measure = wrapped_measure;
	if (v->destroy)
		oclass->dispose = wrapped_dispose;
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
verne_widget_chain_show (gpointer parent_class, GtkWidget *widget)
{
	VerneVfuncs *v = lookup_vfuncs_type (G_TYPE_FROM_CLASS (parent_class));
	if (v && v->show)
		v->show (widget);
	else
		gtk_widget_set_visible (widget, TRUE);
}

void
verne_compat_init (void)
{
	verne_controllers_quark = g_quark_from_static_string ("verne-controllers");
	if (vfunc_table == NULL)
		vfunc_table = g_hash_table_new (g_direct_hash, g_direct_equal);
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
