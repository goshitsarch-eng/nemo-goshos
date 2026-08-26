/* Current-event tracking, key propagation, and X11 root-window filters. */
#include "config.h"
#include "verne-gtk-compat.h"

#include <string.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

typedef struct {
	GdkEvent *event;
	GtkWidget *widget;
} VerneCurrentEvent;

static GQueue *verne_event_stack;
static GObject *verne_root_token;

typedef struct {
	GdkSurface *window;
	GdkFilterFunc func;
	gpointer data;
} VerneXFilter;

static GList *verne_x_filters;
static guint verne_workarea_source;
static guchar *verne_workarea_bytes;
static gsize verne_workarea_len;

void
verne_set_current_event (GtkWidget *widget, const GdkEvent *event)
{
	VerneCurrentEvent *cur;

	if (verne_event_stack == NULL)
		verne_event_stack = g_queue_new ();
	cur = g_new0 (VerneCurrentEvent, 1);
	cur->event = event ? gdk_event_copy (event) : NULL;
	cur->widget = widget;
	g_queue_push_head (verne_event_stack, cur);
}

void
verne_clear_current_event (void)
{
	VerneCurrentEvent *cur;

	if (verne_event_stack == NULL)
		return;
	cur = g_queue_pop_head (verne_event_stack);
	if (cur == NULL)
		return;
	if (cur->event)
		gdk_event_free (cur->event);
	g_free (cur);
}

GdkEvent *
gtk_get_current_event (void)
{
	VerneCurrentEvent *cur;

	if (verne_event_stack == NULL)
		return NULL;
	cur = g_queue_peek_head (verne_event_stack);
	if (cur == NULL || cur->event == NULL)
		return NULL;
	return gdk_event_copy (cur->event);
}

GtkWidget *
gtk_get_event_widget (GdkEvent *event)
{
	VerneCurrentEvent *cur;

	(void) event;
	if (verne_event_stack == NULL)
		return NULL;
	cur = g_queue_peek_head (verne_event_stack);
	return cur ? cur->widget : NULL;
}

	gboolean
gtk_get_current_event_state (GdkModifierType *state)
{
	VerneCurrentEvent *cur;
	GdkEvent *event;
	guint type;

	if (state == NULL)
		return FALSE;
	*state = 0;
	if (verne_event_stack == NULL)
		return FALSE;
	cur = g_queue_peek_head (verne_event_stack);
	if (cur == NULL || cur->event == NULL)
		return FALSE;
	event = cur->event;
	type = (guint) event->type;
	if (type == GDK_BUTTON_PRESS || type == GDK_BUTTON_RELEASE ||
	    type == (guint) GDK_2BUTTON_PRESS || type == (guint) GDK_3BUTTON_PRESS) {
		*state = event->button.state;
		return TRUE;
	}
	if (type == GDK_KEY_PRESS || type == GDK_KEY_RELEASE) {
		*state = event->key.state;
		return TRUE;
	}
	if (type == GDK_MOTION_NOTIFY) {
		*state = event->motion.state;
		return TRUE;
	}
	if (type == GDK_SCROLL) {
		*state = event->scroll.state;
		return TRUE;
	}
	if (type == GDK_ENTER_NOTIFY || type == GDK_LEAVE_NOTIFY) {
		*state = event->crossing.state;
		return TRUE;
	}
	return FALSE;
}

guint32
gtk_get_current_event_time (void)
{
	VerneCurrentEvent *cur;

	if (verne_event_stack == NULL)
		return GDK_CURRENT_TIME;
	cur = g_queue_peek_head (verne_event_stack);
	if (cur == NULL || cur->event == NULL)
		return (guint32) (g_get_monotonic_time () / 1000);
	return gdk_event_get_time (cur->event);
}

gboolean
gtk_window_propagate_key_event (GtkWindow *window, GdkEventKey *event)
{
	GtkWidget *focus;
	gboolean handled;

	if (window == NULL || event == NULL)
		return FALSE;
	focus = gtk_window_get_focus (window);
	if (focus == NULL || !gtk_widget_get_sensitive (focus) || !gtk_widget_get_mapped (focus))
		return FALSE;
	verne_set_current_event (focus, (const GdkEvent *) event);
	handled = gtk_widget_event (focus, (GdkEvent *) event);
	verne_clear_current_event ();
	return handled;
}

GdkSurface *
gdk_screen_get_root_window (GdkScreen *screen)
{
	(void) screen;
	if (verne_root_token == NULL)
		verne_root_token = g_object_new (G_TYPE_OBJECT, NULL);
	return (GdkSurface *) verne_root_token;
}

unsigned long
gdk_x11_get_xatom_by_name (const gchar *name)
{
#ifdef GDK_WINDOWING_X11
	GdkDisplay *display;

	if (name == NULL || name[0] == '\0')
		return 0;
	display = gdk_display_get_default ();
	if (display == NULL || !GDK_IS_X11_DISPLAY (display))
		return 0;
	return (unsigned long) gdk_x11_get_xatom_by_name_for_display (display, name);
#else
	(void) name;
	return 0;
#endif
}

#ifdef GDK_WINDOWING_X11
static void
verne_dispatch_x_filters (XEvent *xev)
{
	GdkEvent dummy;
	GList *l;

	memset (&dummy, 0, sizeof dummy);
	for (l = verne_x_filters; l; l = l->next) {
		VerneXFilter *f = l->data;
		if (f->func)
			f->func (xev, &dummy, f->data);
	}
}

static gboolean
verne_workarea_tick (gpointer data)
{
	GdkDisplay *display;
	Display *dpy;
	Window root;
	Atom workarea, actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop = NULL;
	gsize nbytes;
	gboolean changed = FALSE;

	(void) data;
	if (verne_x_filters == NULL)
		return G_SOURCE_CONTINUE;
	display = gdk_display_get_default ();
	if (display == NULL || !GDK_IS_X11_DISPLAY (display))
		return G_SOURCE_CONTINUE;
	dpy = gdk_x11_display_get_xdisplay (display);
	if (dpy == NULL)
		return G_SOURCE_CONTINUE;
	root = RootWindow (dpy, DefaultScreen (dpy));
	workarea = (Atom) gdk_x11_get_xatom_by_name ("_NET_WORKAREA");
	if (workarea == 0)
		return G_SOURCE_CONTINUE;
	if (XGetWindowProperty (dpy, root, workarea, 0, 256, False, XA_CARDINAL,
				&actual_type, &actual_format, &nitems, &bytes_after, &prop) != Success)
		return G_SOURCE_CONTINUE;
	nbytes = nitems * (actual_format == 32 ? 4 : (actual_format / 8));
	if (prop == NULL)
		return G_SOURCE_CONTINUE;
	if (verne_workarea_bytes == NULL || verne_workarea_len != nbytes ||
	    memcmp (verne_workarea_bytes, prop, nbytes) != 0) {
		g_free (verne_workarea_bytes);
		verne_workarea_bytes = g_memdup2 (prop, nbytes);
		verne_workarea_len = nbytes;
		changed = TRUE;
	}
	XFree (prop);
	if (changed) {
		XEvent xev;

		memset (&xev, 0, sizeof xev);
		xev.type = PropertyNotify;
		xev.xproperty.window = root;
		xev.xproperty.atom = workarea;
		xev.xproperty.state = PropertyNewValue;
		verne_dispatch_x_filters (&xev);
	}
	return G_SOURCE_CONTINUE;
}

static void
verne_ensure_workarea_watch (void)
{
	GdkDisplay *display;
	Display *dpy;
	Window root;
	XWindowAttributes attrs;

	display = gdk_display_get_default ();
	if (display && GDK_IS_X11_DISPLAY (display)) {
		dpy = gdk_x11_display_get_xdisplay (display);
		if (dpy) {
			root = RootWindow (dpy, DefaultScreen (dpy));
			if (XGetWindowAttributes (dpy, root, &attrs))
				XSelectInput (dpy, root, attrs.your_event_mask | PropertyChangeMask);
		}
	}
	if (verne_workarea_source == 0)
		verne_workarea_source = g_timeout_add (200, verne_workarea_tick, NULL);
}
#endif

void
gdk_window_add_filter (GdkSurface *window, GdkFilterFunc func, gpointer data)
{
	VerneXFilter *f;

	f = g_new0 (VerneXFilter, 1);
	f->window = window;
	f->func = func;
	f->data = data;
	verne_x_filters = g_list_append (verne_x_filters, f);
#ifdef GDK_WINDOWING_X11
	verne_ensure_workarea_watch ();
	verne_workarea_tick (NULL);
#endif
}

void
gdk_window_remove_filter (GdkSurface *window, gpointer func, gpointer data)
{
	GList *l;

	for (l = verne_x_filters; l; l = l->next) {
		VerneXFilter *f = l->data;
		if (f->window == window && f->func == (GdkFilterFunc) func && f->data == data) {
			verne_x_filters = g_list_delete_link (verne_x_filters, l);
			g_free (f);
			break;
		}
	}
}

void
gdk_window_set_events (GdkSurface *window, GdkEventMask event_mask)
{
	(void) event_mask;
#ifdef GDK_WINDOWING_X11
	if (window == (GdkSurface *) verne_root_token || window == gdk_screen_get_root_window (NULL))
		verne_ensure_workarea_watch ();
#else
	(void) window;
#endif
}
