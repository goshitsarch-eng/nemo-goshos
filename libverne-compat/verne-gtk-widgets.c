#include "config.h"
#include "verne-gtk-compat.h"
#include <string.h>
#include <cairo.h>
#include <graphene.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#endif

/* ---------- GtkContainer ---------- */
G_DEFINE_TYPE (GtkContainer, gtk_container, GTK_TYPE_WIDGET)

static void
verne_snapshot_css_background (GtkWidget *widget, GtkSnapshot *snapshot)
{
	int width;
	int height;

	width = gtk_widget_get_width (widget);
	height = gtk_widget_get_height (widget);
	if (width <= 0 || height <= 0)
		return;
	gtk_snapshot_render_background (snapshot,
					gtk_widget_get_style_context (widget),
					0, 0, width, height);
}

static void
verne_chrome_snapshot (GtkWidget *widget, GtkSnapshot *snapshot, GtkWidgetClass *parent_class)
{
	int width;
	int height;
	const GdkRGBA bg = { 0.922f, 0.922f, 0.922f, 1.0f };

	width = gtk_widget_get_width (widget);
	height = gtk_widget_get_height (widget);
	if (width > 0 && height > 0)
		gtk_snapshot_append_color (snapshot, &bg, &GRAPHENE_RECT_INIT (0, 0, (float) width, (float) height));
	if (parent_class != NULL && parent_class->snapshot != NULL)
		parent_class->snapshot (widget, snapshot);
}

static void
gtk_container_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	GtkWidget *child;

	verne_snapshot_css_background (widget, snapshot);
	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
		if (gtk_widget_get_width (child) <= 0 || gtk_widget_get_height (child) <= 0)
			continue;
		gtk_widget_snapshot_child (widget, child, snapshot);
	}
}

static void
gtk_container_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
	GtkWidget *child;
	(void) width;
	(void) height;
	(void) baseline;
	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
		int cw, ch;
		if (!gtk_widget_should_layout (child))
			continue;
		cw = gtk_widget_get_width (child);
		ch = gtk_widget_get_height (child);
		if (cw > 0 && ch > 0)
			continue;
		gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &cw, NULL, NULL);
		gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, -1, NULL, &ch, NULL, NULL);
		if (cw < 1)
			cw = 1;
		if (ch < 1)
			ch = 1;
		gtk_widget_allocate (child, cw, ch, -1, NULL);
	}
}

static void
gtk_container_real_add (GtkContainer *container, GtkWidget *child)
{
	gtk_widget_set_parent (child, GTK_WIDGET (container));
}

static void
gtk_container_real_remove (GtkContainer *container, GtkWidget *child)
{
	if (gtk_widget_get_parent (child) == GTK_WIDGET (container))
		gtk_widget_unparent (child);
}

static void
gtk_container_real_forall (GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer data)
{
	GtkWidget *child, *next;
	(void) include_internals;
	child = gtk_widget_get_first_child (GTK_WIDGET (container));
	while (child) {
		next = gtk_widget_get_next_sibling (child);
		callback (child, data);
		child = next;
	}
}

static void
gtk_container_dispose (GObject *object)
{
	GtkWidget *widget = GTK_WIDGET (object);
	GtkWidget *child;

	/* GTK4 requires children to be unparented before finalize. Custom
	 * GtkContainer subclasses (NemoPathBar) add children with set_parent
	 * and never unparent them on destroy. */
	while ((child = gtk_widget_get_first_child (widget)) != NULL)
		gtk_widget_unparent (child);

	G_OBJECT_CLASS (gtk_container_parent_class)->dispose (object);
}

static void
gtk_container_class_init (GtkContainerClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = gtk_container_dispose;
	GTK_WIDGET_CLASS (klass)->snapshot = gtk_container_snapshot;
	GTK_WIDGET_CLASS (klass)->size_allocate = gtk_container_size_allocate;
	klass->add = gtk_container_real_add;
	klass->remove = gtk_container_real_remove;
	klass->forall = gtk_container_real_forall;
}

static void
gtk_container_init (GtkContainer *container)
{
	(void) container;
}

/* ---------- GtkBin ---------- */
G_DEFINE_TYPE (GtkBin, gtk_bin, GTK_TYPE_CONTAINER)

static void
gtk_bin_add (GtkContainer *container, GtkWidget *child)
{
	GtkBin *bin = GTK_BIN (container);
	if (bin->child)
		gtk_widget_unparent (bin->child);
	bin->child = child;
	gtk_widget_set_parent (child, GTK_WIDGET (container));
}

static void
gtk_bin_remove (GtkContainer *container, GtkWidget *child)
{
	GtkBin *bin = GTK_BIN (container);
	if (bin->child == child) {
		bin->child = NULL;
		gtk_widget_unparent (child);
	}
}

static void
gtk_bin_dispose (GObject *object)
{
	GtkBin *bin = GTK_BIN (object);
	g_clear_pointer (&bin->child, gtk_widget_unparent);
	G_OBJECT_CLASS (gtk_bin_parent_class)->dispose (object);
}

static void
gtk_bin_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	GtkBin *bin = GTK_BIN (widget);
	const GdkRGBA bg = { 0.922f, 0.922f, 0.922f, 1.0f };
	int width;
	int height;

	width = gtk_widget_get_width (widget);
	height = gtk_widget_get_height (widget);
	if (width > 0 && height > 0)
		gtk_snapshot_append_color (snapshot, &bg, &GRAPHENE_RECT_INIT (0, 0, (float) width, (float) height));
	if (bin->child)
		gtk_widget_snapshot_child (widget, bin->child, snapshot);
}

static void
gtk_bin_class_init (GtkBinClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = gtk_bin_dispose;
	GTK_WIDGET_CLASS (klass)->snapshot = gtk_bin_snapshot;
	GTK_CONTAINER_CLASS (klass)->add = gtk_bin_add;
	GTK_CONTAINER_CLASS (klass)->remove = gtk_bin_remove;
}
static void gtk_bin_init (GtkBin *bin) { bin->child = NULL; gtk_widget_set_layout_manager (GTK_WIDGET (bin), gtk_bin_layout_new ()); }

GtkWidget *
gtk_bin_get_child (GtkBin *bin)
{
	gpointer obj = bin;
	if (obj == NULL)
		return NULL;
	if (VERNE_IS_SCROLLED_WINDOW (obj))
		return (gtk_scrolled_window_get_child) (verne_to_gtk_sw (obj));
	if (GTK_IS_SCROLLED_WINDOW (obj) && !VERNE_IS_SCROLLED_WINDOW (obj))
		return (gtk_scrolled_window_get_child) ((GtkScrolledWindow *) obj);
	if (GTK_IS_WINDOW (obj))
		return gtk_window_get_child (GTK_WINDOW (obj));
	if (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_BIN))
		return ((GtkBin *) obj)->child;
	return NULL;
}

/* ---------- GtkEventBox : GtkBin ---------- */
typedef struct _GtkEventBoxClass { GtkBinClass parent_class; } GtkEventBoxClass;
struct _GtkEventBox { GtkBin parent; };
G_DEFINE_TYPE (GtkEventBox, gtk_event_box, GTK_TYPE_BIN)
static void
gtk_event_box_class_init (GtkEventBoxClass *c)
{
	gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (c), "eventbox");
}
static void
gtk_event_box_init (GtkEventBox *b)
{
	gtk_widget_add_css_class (GTK_WIDGET (b), "eventbox");
}
GtkWidget *gtk_event_box_new (void) { return g_object_new (GTK_TYPE_EVENT_BOX, NULL); }
void gtk_event_box_set_visible_window (GtkEventBox *box, gboolean visible) { (void) box; (void) visible; }
void gtk_event_box_set_above_child (GtkEventBox *box, gboolean above) { (void) box; (void) above; }

/* ---------- GtkMisc ---------- */
G_DEFINE_TYPE (GtkMisc, verne_misc, GTK_TYPE_WIDGET)
static void verne_misc_class_init (GtkMiscClass *c) { (void) c; }
static void verne_misc_init (GtkMisc *m) { m->xalign = 0.5; m->yalign = 0.5; }
void gtk_misc_set_alignment (GtkMisc *misc, gfloat xalign, gfloat yalign)
{
	GtkWidget *w;

	if (misc == NULL)
		return;
	w = GTK_WIDGET (misc);
	if (!GTK_IS_WIDGET (w))
		return;
	if (GTK_IS_LABEL (w)) {
		gtk_label_set_xalign (GTK_LABEL (w), xalign);
		gtk_label_set_yalign (GTK_LABEL (w), yalign);
		return;
	}
	if (G_TYPE_CHECK_INSTANCE_TYPE (w, GTK_TYPE_MISC)) {
		misc->xalign = xalign;
		misc->yalign = yalign;
	}
	gtk_widget_set_halign (w, xalign < 0.33 ? GTK_ALIGN_START : (xalign > 0.66 ? GTK_ALIGN_END : GTK_ALIGN_CENTER));
	gtk_widget_set_valign (w, yalign < 0.33 ? GTK_ALIGN_START : (yalign > 0.66 ? GTK_ALIGN_END : GTK_ALIGN_CENTER));
}
void gtk_misc_get_alignment (GtkMisc *misc, gfloat *xalign, gfloat *yalign) {
	GtkWidget *w;
	gfloat xa = 0.5, ya = 0.5;

	if (misc == NULL)
		return;
	w = GTK_WIDGET (misc);
	if (GTK_IS_WIDGET (w) && G_TYPE_CHECK_INSTANCE_TYPE (w, GTK_TYPE_MISC)) {
		xa = misc->xalign;
		ya = misc->yalign;
	} else if (GTK_IS_WIDGET (w)) {
		GtkAlign ha = gtk_widget_get_halign (w);
		GtkAlign va = gtk_widget_get_valign (w);
		xa = (ha == GTK_ALIGN_START) ? 0.0 : ((ha == GTK_ALIGN_END) ? 1.0 : 0.5);
		ya = (va == GTK_ALIGN_START) ? 0.0 : ((va == GTK_ALIGN_END) ? 1.0 : 0.5);
	}
	if (xalign) *xalign = xa;
	if (yalign) *yalign = ya;
}
void gtk_misc_set_padding (GtkMisc *misc, gint xpad, gint ypad) {
	GtkWidget *w;

	if (misc == NULL)
		return;
	w = GTK_WIDGET (misc);
	if (!GTK_IS_WIDGET (w))
		return;
	if (G_TYPE_CHECK_INSTANCE_TYPE (w, GTK_TYPE_MISC)) {
		misc->xpad = xpad;
		misc->ypad = ypad;
	}
	gtk_widget_set_margin_start (w, xpad);
	gtk_widget_set_margin_end (w, xpad);
	gtk_widget_set_margin_top (w, ypad);
	gtk_widget_set_margin_bottom (w, ypad);
}
void gtk_misc_get_padding (GtkMisc *misc, gint *xpad, gint *ypad) {
	GtkWidget *w;

	if (misc == NULL)
		return;
	w = GTK_WIDGET (misc);
	if (GTK_IS_WIDGET (w) && G_TYPE_CHECK_INSTANCE_TYPE (w, GTK_TYPE_MISC)) {
		if (xpad) *xpad = misc->xpad;
		if (ypad) *ypad = misc->ypad;
		return;
	}
	if (xpad) *xpad = GTK_IS_WIDGET (w) ? gtk_widget_get_margin_start (w) : 0;
	if (ypad) *ypad = GTK_IS_WIDGET (w) ? gtk_widget_get_margin_top (w) : 0;
}

/* ---------- GtkLayout (scrollable canvas parent) ---------- */
typedef struct {
	guint width, height;
	GtkAdjustment *hadj, *vadj;
	GtkScrollablePolicy hscroll_policy, vscroll_policy;
	GHashTable *child_pos;
} GtkLayoutPrivate;

static void gtk_layout_scrollable_init (GtkScrollableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkLayout, gtk_layout, GTK_TYPE_WIDGET,
			 G_ADD_PRIVATE (GtkLayout)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, gtk_layout_scrollable_init))

enum { LAYOUT_PROP_0, LAYOUT_PROP_HADJ, LAYOUT_PROP_VADJ, LAYOUT_PROP_HSCROLL, LAYOUT_PROP_VSCROLL, LAYOUT_N_PROPS };

typedef struct { int x, y; } ChildPos;

static void
gtk_layout_set_adjustment (GtkAdjustment **store, GtkAdjustment *adj)
{
	if (*store == adj)
		return;
	if (*store)
		g_object_unref (*store);
	if (adj)
		*store = g_object_ref (adj);
	else
		*store = gtk_adjustment_new (0, 0, 0, 0, 0, 0);
}

static void
gtk_layout_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (GTK_LAYOUT (object));
	switch (prop_id) {
	case LAYOUT_PROP_HADJ: g_value_set_object (value, priv->hadj); break;
	case LAYOUT_PROP_VADJ: g_value_set_object (value, priv->vadj); break;
	case LAYOUT_PROP_HSCROLL: g_value_set_enum (value, priv->hscroll_policy); break;
	case LAYOUT_PROP_VSCROLL: g_value_set_enum (value, priv->vscroll_policy); break;
	default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_layout_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (GTK_LAYOUT (object));
	switch (prop_id) {
	case LAYOUT_PROP_HADJ: gtk_layout_set_adjustment (&priv->hadj, g_value_get_object (value)); break;
	case LAYOUT_PROP_VADJ: gtk_layout_set_adjustment (&priv->vadj, g_value_get_object (value)); break;
	case LAYOUT_PROP_HSCROLL: priv->hscroll_policy = g_value_get_enum (value); break;
	case LAYOUT_PROP_VSCROLL: priv->vscroll_policy = g_value_get_enum (value); break;
	default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_layout_dispose (GObject *object)
{
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (GTK_LAYOUT (object));
	GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (object));
	while (child) {
		GtkWidget *next = gtk_widget_get_next_sibling (child);
		gtk_widget_unparent (child);
		child = next;
	}
	g_clear_object (&priv->hadj);
	g_clear_object (&priv->vadj);
	g_clear_pointer (&priv->child_pos, g_hash_table_destroy);
	G_OBJECT_CLASS (gtk_layout_parent_class)->dispose (object);
}

static void
gtk_layout_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
		    int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (GTK_LAYOUT (widget));
	(void) for_size;
	if (minimum_baseline)
		*minimum_baseline = -1;
	if (natural_baseline)
		*natural_baseline = -1;
	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		*minimum = 0;
		*natural = MAX ((int) priv->width, 1);
	} else {
		*minimum = 0;
		*natural = MAX ((int) priv->height, 1);
	}
}

static void
gtk_layout_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	GtkWidget *child;

	/* Do not paint CSS here: EelCanvas/NemoIconContainer draw icons in a
	 * GTK3 draw() that wrapped_snapshot runs first; a later background
	 * would cover those icons. */
	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
		if (gtk_widget_get_width (child) <= 0 || gtk_widget_get_height (child) <= 0)
			continue;
		gtk_widget_snapshot_child (widget, child, snapshot);
	}
}

static void
gtk_layout_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (GTK_LAYOUT (widget));
	GtkWidget *child;
	(void) baseline;
	(void) width;
	(void) height;
	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
		ChildPos *pos = g_hash_table_lookup (priv->child_pos, child);
		int min_w = 0, nat_w = 0, min_h = 0, nat_h = 0;
		int cw, ch, x, y;
		GskTransform *t;

		if (!gtk_widget_should_layout (child))
			continue;
		gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, &min_w, &nat_w, NULL, NULL);
		cw = MAX (nat_w, min_w);
		if (cw < 1)
			cw = MAX (gtk_widget_get_width (child), 1);
		gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, cw, &min_h, &nat_h, NULL, NULL);
		ch = MAX (nat_h, min_h);
		if (ch < 1)
			ch = MAX (gtk_widget_get_height (child), 1);
		x = pos ? pos->x : 0;
		y = pos ? pos->y : 0;
		t = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT ((float) x, (float) y));
		gtk_widget_allocate (child, cw, ch, -1, t);
	}
}

static void
gtk_layout_class_init (GtkLayoutClass *klass)
{
	GObjectClass *oc = G_OBJECT_CLASS (klass);
	GtkWidgetClass *wc = GTK_WIDGET_CLASS (klass);
	oc->get_property = gtk_layout_get_property;
	oc->set_property = gtk_layout_set_property;
	oc->dispose = gtk_layout_dispose;
	wc->snapshot = gtk_layout_snapshot;
	wc->size_allocate = gtk_layout_size_allocate;
	wc->measure = gtk_layout_measure;
	gtk_widget_class_set_css_name (wc, "layout");
	g_object_class_override_property (oc, LAYOUT_PROP_HADJ, "hadjustment");
	g_object_class_override_property (oc, LAYOUT_PROP_VADJ, "vadjustment");
	g_object_class_override_property (oc, LAYOUT_PROP_HSCROLL, "hscroll-policy");
	g_object_class_override_property (oc, LAYOUT_PROP_VSCROLL, "vscroll-policy");
}

static void
gtk_layout_init (GtkLayout *layout)
{
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (layout);
	priv->width = priv->height = 100;
	priv->hadj = gtk_adjustment_new (0, 0, 100, 1, 10, 10);
	priv->vadj = gtk_adjustment_new (0, 0, 100, 1, 10, 10);
	priv->child_pos = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
	gtk_widget_set_overflow (GTK_WIDGET (layout), GTK_OVERFLOW_HIDDEN);
	gtk_widget_set_hexpand (GTK_WIDGET (layout), TRUE);
	gtk_widget_set_vexpand (GTK_WIDGET (layout), TRUE);
	gtk_widget_set_halign (GTK_WIDGET (layout), GTK_ALIGN_FILL);
	gtk_widget_set_valign (GTK_WIDGET (layout), GTK_ALIGN_FILL);
	gtk_widget_add_css_class (GTK_WIDGET (layout), "view");
}

static void gtk_layout_scrollable_init (GtkScrollableInterface *iface) { (void) iface; }

GtkWidget *
gtk_layout_new (GtkAdjustment *hadjustment, GtkAdjustment *vadjustment)
{
	GtkWidget *w = g_object_new (GTK_TYPE_LAYOUT, NULL);
	if (hadjustment)
		gtk_layout_set_hadjustment (GTK_LAYOUT (w), hadjustment);
	if (vadjustment)
		gtk_layout_set_vadjustment (GTK_LAYOUT (w), vadjustment);
	return w;
}

void
gtk_layout_put (GtkLayout *layout, GtkWidget *child, gint x, gint y)
{
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (layout);
	ChildPos *pos = g_new (ChildPos, 1);
	pos->x = x; pos->y = y;
	gtk_widget_set_parent (child, GTK_WIDGET (layout));
	g_hash_table_insert (priv->child_pos, child, pos);
	gtk_widget_queue_allocate (GTK_WIDGET (layout));
}

void
gtk_layout_move (GtkLayout *layout, GtkWidget *child, gint x, gint y)
{
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (layout);
	ChildPos *pos = g_hash_table_lookup (priv->child_pos, child);
	if (pos) { pos->x = x; pos->y = y; gtk_widget_queue_allocate (GTK_WIDGET (layout)); }
}

void
gtk_layout_set_size (GtkLayout *layout, guint width, guint height)
{
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (layout);
	priv->width = width; priv->height = height;
	if (priv->hadj)
		gtk_adjustment_set_upper (priv->hadj, width);
	if (priv->vadj)
		gtk_adjustment_set_upper (priv->vadj, height);
}

void
gtk_layout_get_size (GtkLayout *layout, guint *width, guint *height)
{
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (layout);
	if (width) *width = priv->width;
	if (height) *height = priv->height;
}

GtkAdjustment *gtk_layout_get_hadjustment (GtkLayout *layout) {
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (layout); return priv->hadj;
}
GtkAdjustment *gtk_layout_get_vadjustment (GtkLayout *layout) {
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (layout); return priv->vadj;
}
void gtk_layout_set_hadjustment (GtkLayout *layout, GtkAdjustment *adj) {
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (layout);
	gtk_layout_set_adjustment (&priv->hadj, adj);
}
void gtk_layout_set_vadjustment (GtkLayout *layout, GtkAdjustment *adj) {
	GtkLayoutPrivate *priv = gtk_layout_get_instance_private (layout);
	gtk_layout_set_adjustment (&priv->vadj, adj);
}
GdkSurface *gtk_layout_get_bin_window (GtkLayout *layout) {
	return gtk_widget_get_window (GTK_WIDGET (layout));
}

/* ---------- Statusbar ---------- */
typedef struct _GtkStatusbarClass { GtkBoxClass parent_class; } GtkStatusbarClass;
struct _GtkStatusbar { GtkBox parent; GtkLabel *label; guint next_id; GHashTable *stacks; };
G_DEFINE_TYPE (GtkStatusbar, gtk_statusbar, GTK_TYPE_BOX)
static void
gtk_statusbar_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	verne_chrome_snapshot (widget, snapshot, GTK_WIDGET_CLASS (gtk_statusbar_parent_class));
}
static void
gtk_statusbar_class_init (GtkStatusbarClass *c)
{
	gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (c), "statusbar");
	GTK_WIDGET_CLASS (c)->snapshot = gtk_statusbar_snapshot;
}
static void gtk_statusbar_init (GtkStatusbar *bar) {
	bar->label = GTK_LABEL (gtk_label_new (NULL));
	gtk_widget_set_hexpand (GTK_WIDGET (bar->label), TRUE);
	gtk_label_set_xalign (bar->label, 0);
	gtk_box_append (GTK_BOX (bar), GTK_WIDGET (bar->label));
	bar->next_id = 1;
	bar->stacks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_ptr_array_unref);
	gtk_widget_add_css_class (GTK_WIDGET (bar), "statusbar");
}
GtkWidget *gtk_statusbar_new (void) { return g_object_new (GTK_TYPE_STATUSBAR, NULL); }
guint gtk_statusbar_get_context_id (GtkStatusbar *bar, const gchar *context_description) {
	(void) context_description;
	return bar->next_id++;
}
static void statusbar_update (GtkStatusbar *bar) {
	GHashTableIter iter; gpointer k, v; const char *text = "";
	g_hash_table_iter_init (&iter, bar->stacks);
	while (g_hash_table_iter_next (&iter, &k, &v)) {
		GPtrArray *arr = v;
		if (arr->len)
			text = arr->pdata[arr->len - 1];
	}
	gtk_label_set_text (bar->label, text);
}
guint gtk_statusbar_push (GtkStatusbar *bar, guint context_id, const gchar *text) {
	GPtrArray *arr = g_hash_table_lookup (bar->stacks, GUINT_TO_POINTER (context_id));
	if (!arr) {
		arr = g_ptr_array_new_with_free_func (g_free);
		g_hash_table_insert (bar->stacks, GUINT_TO_POINTER (context_id), arr);
	}
	g_ptr_array_add (arr, g_strdup (text));
	statusbar_update (bar);
	return arr->len;
}
void gtk_statusbar_pop (GtkStatusbar *bar, guint context_id) {
	GPtrArray *arr = g_hash_table_lookup (bar->stacks, GUINT_TO_POINTER (context_id));
	if (arr && arr->len) g_ptr_array_remove_index (arr, arr->len - 1);
	statusbar_update (bar);
}
void gtk_statusbar_remove_all (GtkStatusbar *bar, guint context_id) {
	g_hash_table_remove (bar->stacks, GUINT_TO_POINTER (context_id));
	statusbar_update (bar);
}
GtkWidget *gtk_statusbar_get_message_area (GtkStatusbar *bar) { return GTK_WIDGET (bar); }

/* ---------- Toolbar ---------- */
typedef struct _GtkToolbarClass { GtkBoxClass parent_class; } GtkToolbarClass;
struct _GtkToolbar { GtkBox parent; };
G_DEFINE_TYPE (GtkToolbar, gtk_toolbar, GTK_TYPE_BOX)
static void
gtk_toolbar_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	verne_chrome_snapshot (widget, snapshot, GTK_WIDGET_CLASS (gtk_toolbar_parent_class));
}
static void
gtk_toolbar_class_init (GtkToolbarClass *c)
{
	gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (c), "toolbar");
	GTK_WIDGET_CLASS (c)->snapshot = gtk_toolbar_snapshot;
}
static void gtk_toolbar_init (GtkToolbar *t) {
	gtk_orientable_set_orientation (GTK_ORIENTABLE (t), GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_add_css_class (GTK_WIDGET (t), "toolbar");
	gtk_widget_set_hexpand (GTK_WIDGET (t), TRUE);
}
GtkWidget *gtk_toolbar_new (void) { return g_object_new (GTK_TYPE_TOOLBAR, NULL); }
void gtk_toolbar_insert (GtkToolbar *toolbar, GtkWidget *item, gint pos) {
	gtk_box_append (GTK_BOX (toolbar), item);
	if (pos >= 0)
		gtk_box_reorder_child (GTK_BOX (toolbar), item, pos);
}
void gtk_toolbar_set_icon_size (GtkToolbar *toolbar, GtkIconSize size) { (void) toolbar; (void) size; }
void gtk_toolbar_set_style (GtkToolbar *toolbar, gint style) { (void) toolbar; (void) style; }
void gtk_toolbar_set_show_arrow (GtkToolbar *toolbar, gboolean show) { (void) toolbar; (void) show; }

/* ---------- Menu / menubar / menuitem ---------- */
typedef struct _GtkMenuClass { GtkWindowClass parent_class; } GtkMenuClass;
struct _GtkMenu { GtkWindow parent; GtkWidget *box; GtkWidget *attach; };
G_DEFINE_TYPE (GtkMenu, gtk_menu, GTK_TYPE_WINDOW)

static void verne_menu_popup_now (GtkMenu *menu, int root_x, int root_y, gboolean has_pos);
static void verne_menu_set_override_redirect (GtkWidget *w);
static void verne_menu_embed_on_desktop (GtkWidget *w);
static void verne_menu_unembed (GtkWidget *w);
static GtkWindow *verne_menu_dest_attach_window (GtkWidget *menu);
static gboolean verne_menu_popup_dest_overlay (GtkMenu *menu, int root_x, int root_y);
static gboolean verne_menu_popup_dest_popover (GtkMenu *menu, int root_x, int root_y);
static void verne_menu_popdown_dest_popover (GtkMenu *menu);
static void verne_menu_open_submenu (GtkWidget *item, GtkWidget *submenu);
static void verne_menu_popup_submenu (GtkWidget *item, GtkWidget *submenu, gboolean toggle);
static void verne_menu_hide_others (GtkMenu *keep);
static void verne_menu_hide_others_later (void);
static void verne_overlay_activate_leaf (GtkWidget *btn);
static void verne_menu_item_cancel_submenu_timeout (GtkWidget *item);
static void verne_widget_clear_active (GtkWidget *w);
static void verne_menu_set_submenu_item (GtkMenu *menu, GtkWidget *item);

static void
verne_menu_ensure_css (void)
{
	static GtkCssProvider *provider;
	if (provider)
		return;
	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_string (provider,
		"window.popup.menu, window.popup.menu box.menu {\n"
		"  background-color: #ffffff;\n"
		"  background-image: none;\n"
		"  color: #1e1e1e;\n"
		"  opacity: 1;\n"
		"}\n"
		"window.popup.menu {\n"
		"  border-radius: 12px;\n"
		"  border: 1px solid #c0c0c0;\n"
		"  padding: 6px;\n"
		"}\n"
		"window.popup.menu button, window.popup.menu checkbutton {\n"
		"  padding: 6px 12px;\n"
		"  border-radius: 6px;\n"
		"  color: #1e1e1e;\n"
		"  background-color: transparent;\n"
		"}\n"
		"window.popup.menu button:hover, window.popup.menu checkbutton:hover {\n"
		"  background-color: #e6e6e6;\n"
		"}\n"
		"window.popup.menu button.separator,\n"
		"popover.menu button.separator,\n"
		"box.verne-dest-menu button.separator {\n"
		"  min-height: 0;\n"
		"  min-width: 0;\n"
		"  padding: 4px 10px;\n"
		"  background-color: transparent;\n"
		"}\n"
		"window.popup.menu button.separator:hover,\n"
		"popover.menu button.separator:hover,\n"
		"box.verne-dest-menu button.separator:hover {\n"
		"  background-color: transparent;\n"
		"}\n"
		"window.popup.menu button.separator separator,\n"
		"popover.menu button.separator separator,\n"
		"box.verne-dest-menu button.separator separator {\n"
		"  min-height: 1px;\n"
		"  background-color: #d0d0d0;\n"
		"}\n"
		"popover.menu, popover.menu > contents, popover.menu box {\n"
		"  background-color: #ffffff;\n"
		"  background-image: none;\n"
		"  color: #1e1e1e;\n"
		"}\n"
		"popover.menu button, popover.menu checkbutton {\n"
		"  padding: 6px 12px;\n"
		"  color: #1e1e1e;\n"
		"  background-color: transparent;\n"
		"}\n"
		"popover.menu button:hover, popover.menu checkbutton:hover {\n"
		"  background-color: #e6e6e6;\n"
		"}\n"
		"box.verne-dest-menu {\n"
		"  background-color: #ffffff;\n"
		"  background-image: none;\n"
		"  color: #1e1e1e;\n"
		"  border-radius: 12px;\n"
		"  border: 1px solid #c0c0c0;\n"
		"  padding: 6px;\n"
		"  min-width: 240px;\n"
		"}\n"
		"box.verne-dest-menu button, box.verne-dest-menu checkbutton {\n"
		"  padding: 6px 12px;\n"
		"  min-height: 32px;\n"
		"  color: #1e1e1e;\n"
		"  background-color: transparent;\n"
		"}\n"
		"box.verne-dest-menu button:hover, box.verne-dest-menu checkbutton:hover {\n"
		"  background-color: #e6e6e6;\n"
		"}\n"
		"scrolledwindow.verne-dest-menu {\n"
		"  background-color: #ffffff;\n"
		"  border-radius: 12px;\n"
		"  border: 1px solid #c0c0c0;\n"
		"  min-width: 240px;\n"
		"}\n"
		"scrolledwindow.verne-dest-menu box.verne-dest-menu {\n"
		"  border: none;\n"
		"  border-radius: 0;\n"
		"  min-width: 0;\n"
		"}\n"
		"box.verne-overlay-scroll {\n"
		"  background-color: #ffffff;\n"
		"  background-image: none;\n"
		"  color: #1e1e1e;\n"
		"  border-radius: 12px;\n"
		"  border: 1px solid #c0c0c0;\n"
		"  min-width: 240px;\n"
		"}\n"
		"box.verne-overlay-scroll box.verne-dest-menu {\n"
		"  border: none;\n"
		"  border-radius: 0;\n"
		"  min-width: 0;\n"
		"  padding: 6px;\n"
		"}\n"
		".menubar {\n"
		"  background-color: #f6f5f4;\n"
		"  min-height: 28px;\n"
		"}\n"
		".menubar > button {\n"
		"  padding: 4px 10px;\n"
		"}\n");
	gtk_style_context_add_provider_for_display (gdk_display_get_default (),
						    GTK_STYLE_PROVIDER (provider),
						    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static gboolean
verne_menu_close_request (GtkWindow *window, gpointer data)
{
	(void) data;
	gtk_menu_popdown (GTK_MENU (window));
	return TRUE;
}

static void
verne_x11_move (GtkWindow *win, int x, int y)
{
#ifdef GDK_WINDOWING_X11
	GdkSurface *s = gtk_native_get_surface (GTK_NATIVE (win));
	if (s == NULL || !GDK_IS_X11_SURFACE (s))
		return;
	XMoveWindow (gdk_x11_display_get_xdisplay (gdk_surface_get_display (s)),
		     gdk_x11_surface_get_xid (s), x, y);
#else
	(void) win; (void) x; (void) y;
#endif
}

static GtkWindow *
verne_menu_dest_attach_window (GtkWidget *menu)
{
	GtkWidget *attach;
	GtkRoot *root;

	if (!GTK_IS_MENU (menu))
		return NULL;
	attach = GTK_MENU (menu)->attach;
	if (attach == NULL || !GTK_IS_WIDGET (attach))
		return NULL;
	root = gtk_widget_get_root (attach);
	if (!GTK_IS_WINDOW (root) || GTK_IS_MENU (root))
		return NULL;
	if (gtk_window_get_type_hint (GTK_WINDOW (root)) == GDK_WINDOW_TYPE_HINT_DESKTOP ||
	    g_object_get_data (G_OBJECT (root), "is_desktop_window") != NULL)
		return GTK_WINDOW (root);
	return NULL;
}

static GtkWidget *
verne_menu_overlay_host (GtkMenu *menu)
{
	GtkWindow *dest;
	GtkWidget *start;
	GtkWidget *w;

	dest = verne_menu_dest_attach_window (GTK_WIDGET (menu));
	if (dest)
		return GTK_WIDGET (dest);

	if (!GTK_IS_MENU (menu))
		return NULL;
	start = menu->attach;
	if (!GTK_IS_WIDGET (start))
		start = g_object_get_data (G_OBJECT (menu), "verne-submenu-item");
	for (w = start; GTK_IS_WIDGET (w); w = gtk_widget_get_parent (w)) {
		GtkWidget *overlay;

		if (GTK_IS_MENU (w))
			continue;
		overlay = g_object_get_data (G_OBJECT (w), "verne-file-menu-overlay");
		if (overlay == NULL)
			overlay = g_object_get_data (G_OBJECT (w), "verne-dest-menu-overlay");
		if (GTK_IS_OVERLAY (overlay))
			return w;
		if (GTK_IS_WINDOW (w) && !GTK_IS_MENU (w))
			return w;
	}
	return NULL;
}

static GtkWidget *
verne_menu_overlay_widget (GtkWidget *host)
{
	GtkWidget *overlay;

	if (!GTK_IS_WIDGET (host))
		return NULL;
	overlay = g_object_get_data (G_OBJECT (host), "verne-dest-menu-overlay");
	if (!GTK_IS_OVERLAY (overlay))
		overlay = g_object_get_data (G_OBJECT (host), "verne-file-menu-overlay");
	return GTK_IS_OVERLAY (overlay) ? overlay : NULL;
}

static GtkAdjustment *
verne_overlay_vadj_from_widget (GtkWidget *widget)
{
	GtkWidget *w;
	GtkAdjustment *va;

	for (w = widget; GTK_IS_WIDGET (w); w = gtk_widget_get_parent (w)) {
		va = g_object_get_data (G_OBJECT (w), "verne-overlay-vadj");
		if (GTK_IS_ADJUSTMENT (va))
			return va;
		if (GTK_IS_SCROLLED_WINDOW (w))
			return gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (w));
	}
	return NULL;
}

static void
verne_overlay_clip_value_changed (GtkAdjustment *va, gpointer data)
{
	GtkWidget *clip = data;
	GtkWidget *box;
	int y;

	if (!GTK_IS_FIXED (clip) || !GTK_IS_ADJUSTMENT (va))
		return;
	box = gtk_widget_get_first_child (clip);
	if (!GTK_IS_WIDGET (box))
		return;
	y = - (int) gtk_adjustment_get_value (va);
	gtk_fixed_move (GTK_FIXED (clip), box, 0, y);
}

static GtkWidget *
verne_menu_get_scroll (GtkMenu *menu)
{
	GtkWidget *scroll;

	if (!GTK_IS_MENU (menu))
		return NULL;
	scroll = g_object_get_data (G_OBJECT (menu), "verne-menu-scroll");
	if (!GTK_IS_WIDGET (scroll))
		return NULL;
	if (GTK_IS_SCROLLED_WINDOW (scroll) ||
	    gtk_widget_has_css_class (scroll, "verne-overlay-scroll"))
		return scroll;
	return NULL;
}

static void
verne_menu_unparent_overlay_extra (GtkWidget *widget)
{
	GtkWidget *parent;

	if (!GTK_IS_WIDGET (widget))
		return;
	parent = gtk_widget_get_parent (widget);
	if (parent == NULL)
		return;
	g_object_ref (widget);
	if (GTK_IS_OVERLAY (parent))
		gtk_overlay_remove_overlay (GTK_OVERLAY (parent), widget);
	else if (GTK_IS_SCROLLED_WINDOW (parent))
		gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (parent), NULL);
	else if (GTK_IS_FIXED (parent))
		gtk_fixed_remove (GTK_FIXED (parent), widget);
	else if (GTK_IS_BOX (parent) &&
		 gtk_widget_has_css_class (parent, "verne-overlay-scroll"))
		gtk_box_remove (GTK_BOX (parent), widget);
	else
		gtk_widget_unparent (widget);
	g_object_unref (widget);
}

static int
verne_menu_force_item_heights (GtkWidget *box, int box_w)
{
	GtkWidget *ch;
	int total = 0;

	if (!GTK_IS_WIDGET (box))
		return 0;
	if (box_w < 1)
		box_w = 240;
	for (ch = gtk_widget_get_first_child (box); ch;
	     ch = gtk_widget_get_next_sibling (ch)) {
		int nat_h = 0;

		if (!gtk_widget_get_visible (ch))
			continue;
		gtk_widget_measure (ch, GTK_ORIENTATION_VERTICAL, box_w,
				    NULL, &nat_h, NULL, NULL);
		if (GTK_IS_SEPARATOR_MENU_ITEM (ch)) {
			if (nat_h < 1 || nat_h > 16)
				nat_h = 9;
		} else {
			if (nat_h < 32)
				nat_h = 32;
			if (nat_h > 48)
				nat_h = 38;
		}
		gtk_widget_set_valign (ch, GTK_ALIGN_START);
		gtk_widget_set_vexpand (ch, FALSE);
		gtk_widget_set_size_request (ch, -1, nat_h);
		total += nat_h;
	}
	return total;
}

static GtkWidget *
verne_menu_ensure_scroll (GtkMenu *menu, GtkWidget *box, int view_w, int view_h,
			  int nat_w, int nat_h)
{
	GtkWidget *extra = verne_menu_get_scroll (menu);
	GtkWidget *clip = NULL;
	GtkWidget *bar;
	GtkAdjustment *va = NULL;

	/* GtkScrolledWindow as an overlay child leaves the inner GtkBox
	 * unallocated (blank white panel + scrollbar). Clip with GtkFixed so
	 * rows keep the same allocation path as unscoped menus. */
	if (nat_w < 1)
		nat_w = view_w > 16 ? view_w - 16 : 240;
	if (nat_h < view_h)
		nat_h = view_h;

	if (extra == NULL || !gtk_widget_has_css_class (extra, "verne-overlay-scroll")) {
		extra = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_add_css_class (extra, "verne-dest-menu");
		gtk_widget_add_css_class (extra, "verne-overlay-scroll");
		gtk_widget_add_css_class (extra, "background");
		gtk_widget_set_halign (extra, GTK_ALIGN_START);
		gtk_widget_set_valign (extra, GTK_ALIGN_START);
		gtk_widget_set_hexpand (extra, FALSE);
		gtk_widget_set_vexpand (extra, FALSE);
		clip = gtk_fixed_new ();
		gtk_widget_set_overflow (clip, GTK_OVERFLOW_HIDDEN);
		gtk_widget_set_hexpand (clip, TRUE);
		gtk_widget_set_vexpand (clip, FALSE);
		gtk_widget_set_halign (clip, GTK_ALIGN_FILL);
		gtk_widget_set_valign (clip, GTK_ALIGN_FILL);
		gtk_box_append (GTK_BOX (extra), clip);
		va = gtk_adjustment_new (0, 0, 1, 24, 80, 1);
		g_object_ref_sink (va);
		g_object_set_data_full (G_OBJECT (extra), "verne-overlay-vadj",
					va, g_object_unref);
		g_object_set_data_full (G_OBJECT (clip), "verne-overlay-vadj",
					g_object_ref (va), g_object_unref);
		g_signal_connect (va, "value-changed",
				  G_CALLBACK (verne_overlay_clip_value_changed), clip);
		bar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, va);
		gtk_widget_set_vexpand (bar, TRUE);
		gtk_box_append (GTK_BOX (extra), bar);
		g_object_set_data (G_OBJECT (extra), "verne-overlay-clip", clip);
		g_object_ref_sink (extra);
		g_object_set_data_full (G_OBJECT (menu), "verne-menu-scroll",
					extra, g_object_unref);
	} else {
		clip = g_object_get_data (G_OBJECT (extra), "verne-overlay-clip");
		va = g_object_get_data (G_OBJECT (extra), "verne-overlay-vadj");
	}
	if (!GTK_IS_FIXED (clip))
		return extra;

	if (gtk_widget_get_parent (box) != clip) {
		if (gtk_widget_get_parent (box) != NULL)
			verne_menu_unparent_overlay_extra (box);
		gtk_fixed_put (GTK_FIXED (clip), box, 0, 0);
	}

	{
		int stacked = verne_menu_force_item_heights (box, nat_w);

		if (stacked > nat_h)
			nat_h = stacked;
	}
	gtk_widget_set_size_request (box, nat_w, nat_h);
	gtk_widget_set_size_request (clip, nat_w, view_h);
	gtk_widget_set_size_request (extra, view_w, view_h);
	if (GTK_IS_ADJUSTMENT (va)) {
		g_signal_handlers_block_by_func (va,
						 G_CALLBACK (verne_overlay_clip_value_changed),
						 clip);
		gtk_adjustment_configure (va, 0, 0, (gdouble) nat_h, 32.0,
					  (gdouble) view_h, (gdouble) view_h);
		g_signal_handlers_unblock_by_func (va,
						   G_CALLBACK (verne_overlay_clip_value_changed),
						   clip);
		gtk_fixed_move (GTK_FIXED (clip), box, 0, 0);
	}
	gtk_widget_set_visible (box, TRUE);
	gtk_widget_set_visible (clip, TRUE);
	gtk_widget_set_visible (extra, TRUE);
	return extra;
}

static void
verne_menu_restore_box_to_window (GtkMenu *menu)
{
	GtkWidget *popover;
	GtkWidget *box;
	GtkWidget *parent;
	GtkWidget *scroll;

	if (!GTK_IS_MENU (menu) || menu->box == NULL)
		return;
	box = menu->box;
	parent = gtk_widget_get_parent (box);
	scroll = verne_menu_get_scroll (menu);
	if (parent != NULL &&
	    (GTK_IS_SCROLLED_WINDOW (parent) || GTK_IS_FIXED (parent))) {
		GtkWidget *extra = parent;
		GtkWidget *ov;

		while (GTK_IS_WIDGET (extra) &&
		       gtk_widget_get_parent (extra) != NULL &&
		       !GTK_IS_OVERLAY (gtk_widget_get_parent (extra)))
			extra = gtk_widget_get_parent (extra);
		ov = extra ? gtk_widget_get_parent (extra) : NULL;

		g_object_ref (box);
		verne_widget_clear_active (box);
		verne_menu_unparent_overlay_extra (box);
		if (GTK_IS_OVERLAY (ov) && GTK_IS_WIDGET (extra))
			gtk_overlay_remove_overlay (GTK_OVERLAY (ov), extra);
		if (GTK_IS_WIDGET (extra))
			gtk_widget_set_visible (extra, FALSE);
		gtk_widget_remove_css_class (box, "verne-dest-menu");
		gtk_widget_set_size_request (box, -1, -1);
		gtk_widget_set_margin_start (box, 0);
		gtk_widget_set_margin_top (box, 0);
		gtk_widget_set_margin_end (box, 0);
		gtk_widget_set_margin_bottom (box, 0);
		gtk_widget_set_visible (box, TRUE);
		gtk_window_set_child (GTK_WINDOW (menu), box);
		g_object_unref (box);
		g_object_set_data (G_OBJECT (menu), "verne-dest-overlay", NULL);
		return;
	}
	if (scroll != NULL && gtk_widget_get_parent (scroll) != NULL &&
	    GTK_IS_OVERLAY (gtk_widget_get_parent (scroll))) {
		gtk_overlay_remove_overlay (GTK_OVERLAY (gtk_widget_get_parent (scroll)), scroll);
		gtk_widget_set_visible (scroll, FALSE);
	}
	if (parent != NULL && GTK_IS_OVERLAY (parent)) {
		g_object_ref (box);
		verne_widget_clear_active (box);
		gtk_overlay_remove_overlay (GTK_OVERLAY (parent), box);
		gtk_widget_remove_css_class (box, "verne-dest-menu");
		gtk_widget_set_size_request (box, -1, -1);
		gtk_widget_set_margin_start (box, 0);
		gtk_widget_set_margin_top (box, 0);
		gtk_widget_set_margin_end (box, 0);
		gtk_widget_set_margin_bottom (box, 0);
		gtk_widget_set_visible (box, TRUE);
		gtk_window_set_child (GTK_WINDOW (menu), box);
		g_object_unref (box);
		g_object_set_data (G_OBJECT (menu), "verne-dest-overlay", NULL);
		return;
	}
	popover = g_object_get_data (G_OBJECT (menu), "verne-dest-popover");
	if (GTK_IS_POPOVER (popover) && gtk_popover_get_child (GTK_POPOVER (popover)) == box) {
		g_object_ref (box);
		gtk_popover_set_child (GTK_POPOVER (popover), NULL);
		if (gtk_window_get_child (GTK_WINDOW (menu)) != box)
			gtk_window_set_child (GTK_WINDOW (menu), box);
		g_object_unref (box);
	} else if (gtk_window_get_child (GTK_WINDOW (menu)) != box &&
		   gtk_widget_get_parent (box) == NULL) {
		gtk_window_set_child (GTK_WINDOW (menu), box);
	}
}

static gboolean
verne_menu_box_is_dest_overlay (GtkWidget *box)
{
	GtkWidget *parent;

	if (box == NULL || !GTK_IS_WIDGET (box) || !gtk_widget_is_visible (box))
		return FALSE;
	parent = gtk_widget_get_parent (box);
	while (GTK_IS_WIDGET (parent) &&
	       (GTK_IS_SCROLLED_WINDOW (parent) ||
		GTK_IS_FIXED (parent) ||
		GTK_IS_VIEWPORT (parent) ||
		(GTK_IS_BOX (parent) &&
		 gtk_widget_has_css_class (parent, "verne-dest-menu")))) {
		if (!gtk_widget_is_visible (parent))
			return FALSE;
		parent = gtk_widget_get_parent (parent);
	}
	return gtk_widget_has_css_class (box, "verne-dest-menu") &&
	       GTK_IS_OVERLAY (parent);
}

static gboolean
verne_menu_overlay_chrome_showing (GtkMenu *menu)
{
	GtkWidget *scroll;
	GtkWidget *parent;

	if (!GTK_IS_MENU (menu))
		return FALSE;
	scroll = verne_menu_get_scroll (menu);
	if (!GTK_IS_WIDGET (scroll) || !gtk_widget_get_visible (scroll))
		return FALSE;
	parent = gtk_widget_get_parent (scroll);
	return GTK_IS_OVERLAY (parent);
}

static gboolean
verne_menu_is_dest_overlay (gpointer menu)
{
	return GTK_IS_MENU (menu) &&
	       (verne_menu_box_is_dest_overlay (GTK_MENU (menu)->box) ||
		verne_menu_overlay_chrome_showing (GTK_MENU (menu)));
}

static gboolean
verne_any_dest_overlay_visible (void)
{
	GListModel *model = gtk_window_get_toplevels ();
	guint i, n = g_list_model_get_n_items (model);
	gboolean any = FALSE;

	for (i = 0; i < n; i++) {
		gpointer w = g_list_model_get_item (model, i);
		if (verne_menu_is_dest_overlay (w))
			any = TRUE;
		if (w)
			g_object_unref (w);
	}
	return any;
}

static const gchar *
verne_dest_item_label (GtkWidget *w)
{
	const gchar *lab = NULL;

	if (w == NULL)
		return "";
	if (GTK_IS_MENU_ITEM (w))
		lab = gtk_menu_item_get_label (GTK_MENU_ITEM (w));
	if ((lab == NULL || lab[0] == '\0') && GTK_IS_BUTTON (w))
		lab = gtk_button_get_label (GTK_BUTTON (w));
	return lab ? lab : "";
}

/* GTK3 GtkMenu hides leading, trailing, and consecutive separators so
 * empty placeholders (volume actions, etc.) do not leave extra lines. */
static void
verne_menu_update_separators (GtkWidget *box)
{
	GtkWidget *ch;
	GtkWidget *pending_sep = NULL;
	gboolean seen_item = FALSE;

	if (box == NULL)
		return;
	for (ch = gtk_widget_get_first_child (box); ch;
	     ch = gtk_widget_get_next_sibling (ch)) {
		if (GTK_IS_SEPARATOR_MENU_ITEM (ch) || GTK_IS_SEPARATOR (ch)) {
			gtk_widget_set_no_show_all (ch, TRUE);
			gtk_widget_set_visible (ch, FALSE);
			pending_sep = seen_item ? ch : NULL;
			continue;
		}
		if (!gtk_widget_get_visible (ch))
			continue;
		{
			const gchar *lab = verne_dest_item_label (ch);
			if (lab == NULL || lab[0] == '\0')
				continue;
		}
		if (pending_sep != NULL) {
			gtk_widget_set_no_show_all (pending_sep, FALSE);
			gtk_widget_set_visible (pending_sep, TRUE);
			pending_sep = NULL;
		}
		seen_item = TRUE;
	}
}

/* Dest overlay menus often have unallocated or overlay-sized children
 * (pick hits the box; compute_bounds may return the whole menu). Map
 * clicks by stacking gtk_widget_measure heights and skip separators. */
static GtkWidget *
verne_dest_menu_button_at (GtkWidget *box, double lx, double ly)
{
	GtkWidget *ch;
	GtkWidget *hit = NULL;
	GtkWidget *nearest = NULL;
	double nearest_dist = G_MAXDOUBLE;
	double y = 0;
	int box_w;
	gboolean in_separator = FALSE;
	GString *dump;

	if (box == NULL)
		return NULL;
	verne_menu_update_separators (box);
	{
		int view_w = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (box), "verne-dest-menu-w"));
		GtkWidget *parent = gtk_widget_get_parent (box);

		if (GTK_IS_FIXED (parent))
			parent = gtk_widget_get_parent (parent);
		if ((GTK_IS_SCROLLED_WINDOW (parent) ||
		     (GTK_IS_BOX (parent) &&
		      gtk_widget_has_css_class (parent, "verne-overlay-scroll"))) &&
		    view_w > 48 && lx >= view_w - 18)
			return NULL;
	}
	box_w = gtk_widget_get_width (box);
	if (box_w < 1)
		box_w = 240;
	dump = g_string_new ("verne: dest menu layout");

	for (ch = gtk_widget_get_first_child (box); ch;
	     ch = gtk_widget_get_next_sibling (ch)) {
		double y0, y1, mid, dist;
		int nat_h = 0;

		if (!gtk_widget_get_visible (ch))
			continue;
		if (!GTK_IS_BUTTON (ch) && !GTK_IS_CHECK_BUTTON (ch))
			continue;

		gtk_widget_measure (ch, GTK_ORIENTATION_VERTICAL, box_w,
				    NULL, &nat_h, NULL, NULL);
		if (nat_h < 1 || nat_h > 48)
			nat_h = GTK_IS_SEPARATOR_MENU_ITEM (ch) ? 9 : 28;
		y0 = y;
		y1 = y + nat_h;
		y = y1;
		g_string_append_printf (dump, " | %.0f-%.0f %s '%s'",
					y0, y1, G_OBJECT_TYPE_NAME (ch),
					verne_dest_item_label (ch));

		if (GTK_IS_SEPARATOR_MENU_ITEM (ch)) {
			if (ly >= y0 && ly < y1)
				in_separator = TRUE;
			continue;
		}

		mid = (y0 + y1) / 2.0;
		dist = ly < mid ? (mid - ly) : (ly - mid);
		if (dist < nearest_dist) {
			nearest_dist = dist;
			nearest = ch;
		}
		if (ly >= y0 && ly < y1)
			hit = ch;
	}

	g_warning ("%s ly=%.0f hit=%s", dump->str, ly,
		   verne_dest_item_label (hit ? hit : (in_separator ? NULL : nearest)));
	g_string_free (dump, TRUE);

	if (hit != NULL)
		return hit;
	/* A click on the separator between Vim and Other Application must
	 * not snap to the nearest row. */
	if (in_separator)
		return NULL;
	if (nearest != NULL && nearest_dist <= 6.0)
		return nearest;
	return NULL;
}

static void
verne_widget_clear_active (GtkWidget *w)
{
	GtkWidget *ch;

	if (!GTK_IS_WIDGET (w))
		return;
	gtk_widget_unset_state_flags (w, GTK_STATE_FLAG_ACTIVE);
	for (ch = gtk_widget_get_first_child (w); ch;
	     ch = gtk_widget_get_next_sibling (ch))
		verne_widget_clear_active (ch);
}

static guint verne_hide_others_idle_id;

static gboolean
verne_menu_hide_others_idle (gpointer data)
{
	(void) data;
	verne_hide_others_idle_id = 0;
	verne_menu_hide_others (NULL);
	return G_SOURCE_REMOVE;
}

static void
verne_menu_hide_others_later (void)
{
	if (verne_hide_others_idle_id != 0)
		return;
	verne_hide_others_idle_id = g_idle_add (verne_menu_hide_others_idle, NULL);
}

static GtkWidget *verne_overlay_busy_btn;

static gboolean
verne_overlay_activate_leaf_idle (gpointer data)
{
	GtkWidget *btn = data;

	verne_overlay_busy_btn = NULL;
	if (GTK_IS_WIDGET (btn) &&
	    g_object_get_data (G_OBJECT (btn), "verne-destroyed") == NULL &&
	    !GTK_IS_SEPARATOR_MENU_ITEM (btn)) {
		g_warning ("verne: dest overlay idle-activate %s label=%s",
			   G_OBJECT_TYPE_NAME (btn), verne_dest_item_label (btn));
		g_signal_emit_by_name (btn, "clicked");
	}
	verne_menu_hide_others_later ();
	g_object_unref (btn);
	return G_SOURCE_REMOVE;
}

static void
verne_overlay_activate_leaf (GtkWidget *btn)
{
	if (btn == NULL || btn == verne_overlay_busy_btn || GTK_IS_SEPARATOR_MENU_ITEM (btn))
		return;
	if (GTK_IS_MENU_ITEM (btn) && gtk_menu_item_get_submenu (GTK_MENU_ITEM (btn))) {
		g_warning ("verne: dest overlay activate submenu %s label=%s",
			   G_OBJECT_TYPE_NAME (btn), verne_dest_item_label (btn));
		verne_menu_open_submenu (btn, gtk_menu_item_get_submenu (GTK_MENU_ITEM (btn)));
		return;
	}
	/* Overlay press and the window dismiss-click both see the same
	 * File-menu row. Activating Connect/About twice SIGSEGV'd the file
	 * process (tree sidebar UAF while the first dialog mapped). */
	verne_overlay_busy_btn = btn;
	g_object_ref (btn);
	g_warning ("verne: dest overlay activate %s label=%s",
		   G_OBJECT_TYPE_NAME (btn), verne_dest_item_label (btn));
	/* Dest's capture click is still on the stack here. Creating a
	 * document (or starting F2 rename) mutates the dest canvas and
	 * SIGSEGV'd dest. Run the action after that gesture finishes. */
	g_idle_add (verne_overlay_activate_leaf_idle, btn);
}

static GtkWidget *
verne_overlay_clip_widget (GtkWidget *widget)
{
	GtkWidget *w;
	GtkWidget *clip;

	for (w = widget; GTK_IS_WIDGET (w); w = gtk_widget_get_parent (w)) {
		clip = g_object_get_data (G_OBJECT (w), "verne-overlay-clip");
		if (GTK_IS_FIXED (clip))
			return clip;
		if (GTK_IS_FIXED (w))
			return w;
	}
	return NULL;
}

static GtkWidget *
verne_overlay_item_box_from (GtkWidget *widget)
{
	GtkWidget *clip = verne_overlay_clip_widget (widget);
	GtkWidget *box;

	if (GTK_IS_FIXED (clip)) {
		box = gtk_widget_get_first_child (clip);
		if (GTK_IS_WIDGET (box))
			return box;
	}
	if (GTK_IS_SCROLLED_WINDOW (widget))
		return gtk_scrolled_window_get_child (GTK_SCROLLED_WINDOW (widget));
	return widget;
}

/* GtkFixed positions the tall item box with a translate transform
 * (gtk_fixed_move). Overflowing children keep allocation at 0,0, so
 * GtkGestureClick on the box reports viewport Y, not content Y. */
static void
verne_overlay_map_click_to_content (GtkWidget *host, double *x, double *y)
{
	GtkWidget *box;
	GtkWidget *clip;
	GtkAdjustment *va;
	double fx = 0, fy = 0;

	if (host == NULL || y == NULL)
		return;

	box = verne_overlay_item_box_from (host);
	clip = verne_overlay_clip_widget (host);
	va = verne_overlay_vadj_from_widget (host);
	if (GTK_IS_FIXED (clip) && GTK_IS_WIDGET (box) &&
	    gtk_widget_get_parent (box) == clip)
		gtk_fixed_get_child_position (GTK_FIXED (clip), box, &fx, &fy);

	if (fy < -0.5) {
		*y -= fy;
		if (x != NULL)
			*x -= fx;
		return;
	}
	if (va != NULL)
		*y += gtk_adjustment_get_value (va);
}

static GtkWidget *
verne_overlay_hit_box (GtkWidget *widget, double *x, double *y)
{
	GtkWidget *box;
	GtkWidget *parent;
	int sw = 0;

	if (widget == NULL)
		return NULL;

	parent = gtk_widget_get_parent (widget);
	if (GTK_IS_SCROLLED_WINDOW (widget) ||
	    GTK_IS_FIXED (widget) ||
	    (GTK_IS_BOX (widget) &&
	     gtk_widget_has_css_class (widget, "verne-overlay-scroll")))
		sw = gtk_widget_get_width (widget);
	else if (GTK_IS_SCROLLED_WINDOW (parent) ||
		 GTK_IS_FIXED (parent) ||
		 (GTK_IS_BOX (parent) &&
		  gtk_widget_has_css_class (parent, "verne-overlay-scroll")))
		sw = gtk_widget_get_width (parent);
	if (sw > 48 && x != NULL && *x >= sw - 20)
		return NULL;

	verne_overlay_map_click_to_content (widget, x, y);
	box = verne_overlay_item_box_from (widget);
	return box != NULL ? box : widget;
}

static void
verne_dest_overlay_pressed (GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *host = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
	GtkWidget *box;
	GtkWidget *btn;
	GtkMenu *menu = GTK_IS_MENU (data) ? GTK_MENU (data) : NULL;
	double lx = x, ly = y;

	(void) n_press;
	if (host == NULL)
		return;
	{
		GtkWidget *parent = gtk_widget_get_parent (host);
		int sw = 0;

		if (GTK_IS_SCROLLED_WINDOW (host) ||
		    GTK_IS_FIXED (host) ||
		    (GTK_IS_BOX (host) &&
		     gtk_widget_has_css_class (host, "verne-overlay-scroll")))
			sw = gtk_widget_get_width (host);
		else if (GTK_IS_SCROLLED_WINDOW (parent) ||
			 GTK_IS_FIXED (parent) ||
			 (GTK_IS_BOX (parent) &&
			  gtk_widget_has_css_class (parent, "verne-overlay-scroll")))
			sw = gtk_widget_get_width (parent);
		if (sw > 48 && x >= sw - 20) {
			g_warning ("verne: overlay press in scrollbar gutter x=%.0f sw=%d", x, sw);
			return;
		}
	}
	/* Tall menus live in a GtkScrolledWindow. Hit-test the inner box
	 * with the viewport Y plus adjustment, or Preferences / last
	 * items never activate. */
	{
		GtkAdjustment *va = verne_overlay_vadj_from_widget (host);
		g_warning ("verne: overlay press host=%s y=%.0f vadj=%.0f",
			   G_OBJECT_TYPE_NAME (host), y,
			   va ? gtk_adjustment_get_value (va) : 0);
	}
	box = verne_overlay_hit_box (host, &lx, &ly);
	if (box == NULL)
		return;
	g_warning ("verne: overlay press mapped ly=%.0f box=%s",
		   ly, G_OBJECT_TYPE_NAME (box));
	btn = verne_dest_menu_button_at (box, lx, ly);
	if (btn == NULL || btn == box || GTK_IS_SEPARATOR_MENU_ITEM (btn))
		return;
	gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
	verne_overlay_activate_leaf (btn);
	(void) menu;
}

static GtkAdjustment *
verne_overlay_menu_vadj (GtkWidget *widget)
{
	if (GTK_IS_SCROLLED_WINDOW (widget))
		return gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (widget));
	return verne_overlay_vadj_from_widget (widget);
}

static gboolean
verne_dest_overlay_scroll (GtkEventControllerScroll *controller,
			   gdouble dx, gdouble dy, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller));
	GtkAdjustment *va = verne_overlay_menu_vadj (widget);

	(void) data;
	(void) dx;
	if (va == NULL)
		return FALSE;
	if (dy > -0.01 && dy < 0.01)
		return FALSE;
	{
		gdouble step = (dy > -2.0 && dy < 2.0) ? dy * 48.0 : dy;
		gdouble lower = gtk_adjustment_get_lower (va);
		gdouble upper = gtk_adjustment_get_upper (va) - gtk_adjustment_get_page_size (va);
		gdouble next = gtk_adjustment_get_value (va) + step;

		if (upper < lower)
			upper = lower;
		if (next < lower)
			next = lower;
		if (next > upper)
			next = upper;
		gtk_adjustment_set_value (va, next);
	}
	g_warning ("verne: overlay menu scroll dy=%.1f value=%.0f",
		   dy, gtk_adjustment_get_value (va));
	return TRUE;
}

static void
verne_dest_overlay_wheel_button (GtkGestureClick *gesture, gint n_press,
				 gdouble x, gdouble y, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
	GtkAdjustment *va;
	guint button;

	(void) n_press;
	(void) x;
	(void) y;
	(void) data;
	if (widget == NULL)
		return;
	button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
	if (button != 4 && button != 5)
		return;
	va = verne_overlay_menu_vadj (widget);
	if (va == NULL)
		return;
	gtk_adjustment_set_value (va,
				  gtk_adjustment_get_value (va) + (button == 5 ? 48.0 : -48.0));
	g_warning ("verne: overlay menu wheel button=%u value=%.0f",
		   button, gtk_adjustment_get_value (va));
	gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static gboolean
verne_overlay_escape (GtkEventControllerKey *controller, guint keyval, guint keycode,
		      GdkModifierType state, gpointer data)
{
	(void) controller;
	(void) keycode;
	(void) state;
	(void) data;
	if (keyval != GDK_KEY_Escape)
		return FALSE;
	verne_menu_hide_others (NULL);
	return TRUE;
}

static void
verne_overlay_ungrab_keyboard (void)
{
}

static void
verne_overlay_grab_keyboard (GtkWidget *host)
{
	GtkNative *native;
	GdkSurface *surface;

	if (!GTK_IS_WIDGET (host))
		return;
	native = gtk_widget_get_native (host);
	surface = native ? gtk_native_get_surface (native) : NULL;
	verne_overlay_ungrab_keyboard ();
#ifdef GDK_WINDOWING_X11
	if (surface && GDK_IS_X11_SURFACE (surface)) {
		Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (surface));
		Window xid = gdk_x11_surface_get_xid (surface);

		if (dpy && xid) {
			gdk_x11_display_error_trap_push (gdk_display_get_default ());
			XSetInputFocus (dpy, xid, RevertToPointerRoot, CurrentTime);
			gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
		}
	}
#endif
	if (GTK_IS_WINDOW (host)) {
		GtkEventController *keys;

		if (g_object_get_data (G_OBJECT (host), "verne-dest-escape") == NULL) {
			keys = gtk_event_controller_key_new ();
			gtk_event_controller_set_propagation_phase (keys, GTK_PHASE_CAPTURE);
			g_signal_connect (keys, "key-pressed", G_CALLBACK (verne_overlay_escape), NULL);
			gtk_widget_add_controller (host, keys);
			g_object_set_data (G_OBJECT (host), "verne-dest-escape", GINT_TO_POINTER (1));
		}
	}
}

static void
verne_overlay_attach_scroll_controllers (GtkWidget *widget)
{
	GtkEventController *scroll;
	GtkEventController *keys;
	GtkGesture *wheel;

	if (widget == NULL || g_object_get_data (G_OBJECT (widget), "verne-dest-scroll"))
		return;
	scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
	gtk_event_controller_set_propagation_phase (scroll, GTK_PHASE_CAPTURE);
	g_signal_connect (scroll, "scroll",
			  G_CALLBACK (verne_dest_overlay_scroll), NULL);
	gtk_widget_add_controller (widget, scroll);
	wheel = gtk_gesture_click_new ();
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (wheel), 0);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (wheel),
						    GTK_PHASE_CAPTURE);
	g_signal_connect (wheel, "pressed",
			  G_CALLBACK (verne_dest_overlay_wheel_button), NULL);
	gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (wheel));
	gtk_widget_set_focusable (widget, TRUE);
	keys = gtk_event_controller_key_new ();
	gtk_event_controller_set_propagation_phase (keys, GTK_PHASE_CAPTURE);
	g_signal_connect (keys, "key-pressed", G_CALLBACK (verne_overlay_escape), NULL);
	gtk_widget_add_controller (widget, keys);
	g_object_set_data (G_OBJECT (widget), "verne-dest-scroll", GINT_TO_POINTER (1));
}

static gboolean
verne_menu_popup_dest_overlay (GtkMenu *menu, int root_x, int root_y)
{
	GtkWidget *host;
	GtkWidget *overlay;
	GtkWidget *box;
	int lx = root_x, ly = root_y;

	host = verne_menu_overlay_host (menu);
	if (host == NULL) {
		g_warning ("verne: overlay skip (no host) menu=%s",
			   G_OBJECT_TYPE_NAME (menu));
		return FALSE;
	}
	overlay = verne_menu_overlay_widget (host);
	if (!GTK_IS_OVERLAY (overlay)) {
		g_warning ("verne: overlay skip (no overlay widget)");
		return FALSE;
	}
	box = menu->box;
	if (box == NULL)
		return FALSE;

	if (!GTK_IS_WIDGET (box))
		return FALSE;
	verne_menu_update_separators (box);
	g_object_ref (box);
	if (gtk_window_get_child (GTK_WINDOW (menu)) == box)
		gtk_window_set_child (GTK_WINDOW (menu), NULL);
	if (!GTK_IS_WIDGET (box)) {
		g_object_unref (box);
		return FALSE;
	}
	gtk_widget_add_css_class (box, "verne-dest-menu");
	g_object_set_data (G_OBJECT (box), "verne-overlay-menu", menu);
	gtk_widget_add_css_class (box, "background");
	gtk_widget_set_halign (box, GTK_ALIGN_START);
	gtk_widget_set_valign (box, GTK_ALIGN_START);
	gtk_widget_set_hexpand (box, FALSE);
	gtk_widget_set_vexpand (box, FALSE);
	/* Previous overlay popups leave size-request + margins on the box.
	 * Measuring with those still set inflates 272x288 into 592x568 and
	 * pins the next menu at the old click. */
	gtk_widget_set_size_request (box, -1, -1);
	gtk_widget_set_margin_start (box, 0);
	gtk_widget_set_margin_top (box, 0);
	gtk_widget_set_margin_end (box, 0);
	gtk_widget_set_margin_bottom (box, 0);
	/* Place at the popup coords captured at click time. Re-querying the
	 * live pointer here moves the menu if the pointer moved during idle. */
#ifdef GDK_WINDOWING_X11
	{
		GdkSurface *s = GTK_IS_NATIVE (host) ? gtk_native_get_surface (GTK_NATIVE (host)) : NULL;
		if (s && GDK_IS_X11_SURFACE (s)) {
			Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));
			Window child = 0;
			int nx = 0, ny = 0;

			if (XTranslateCoordinates (dpy, DefaultRootWindow (dpy),
						   gdk_x11_surface_get_xid (s),
						   root_x, root_y, &nx, &ny, &child)) {
				lx = nx;
				ly = ny;
			}
		}
	}
#endif
	{
		int nat_w = 240, nat_h = 80;
		int dest_w = 0, dest_h = 0;
		int view_w, view_h;
		gboolean need_scroll = FALSE;
		GtkWidget *extra;
		GdkSurface *ds;

		gtk_widget_measure (box, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &nat_w, NULL, NULL);
		if (nat_w < 240)
			nat_w = 240;
		{
			int stacked = verne_menu_force_item_heights (box, nat_w);

			gtk_widget_measure (box, GTK_ORIENTATION_VERTICAL, nat_w,
					    NULL, &nat_h, NULL, NULL);
			if (stacked > nat_h)
				nat_h = stacked;
		}
		if (nat_h < 80)
			nat_h = 80;
		ds = GTK_IS_NATIVE (host) ? gtk_native_get_surface (GTK_NATIVE (host)) : NULL;
		if (ds != NULL) {
			dest_w = gdk_surface_get_width (ds);
			dest_h = gdk_surface_get_height (ds);
		}
		if (dest_w < 64)
			dest_w = gtk_widget_get_width (host);
		if (dest_h < 64)
			dest_h = gtk_widget_get_height (host);
		if (dest_w < 64)
			dest_w = 1920;
		if (dest_h < 64)
			dest_h = 1200;
		if (lx + nat_w > dest_w - 8)
			lx = dest_w - 8 - nat_w;
		if (lx < 8)
			lx = 8;
		/* Fit in the host window. Prefer keeping the click Y; if the
		 * menu is taller than the remaining space, clamp height and
		 * slide up (and scroll) so the last items stay reachable. */
		view_h = MIN (nat_h, MAX (dest_h - 16, 80));
		if (ly + view_h > dest_h - 8)
			ly = dest_h - 8 - view_h;
		if (ly < 8)
			ly = 8;
		view_w = nat_w;
		need_scroll = nat_h > view_h + 4;
		if (need_scroll)
			view_w = nat_w + 16;
		if (lx + view_w > dest_w - 8)
			lx = MAX (8, dest_w - 8 - view_w);
		if (need_scroll)
			extra = verne_menu_ensure_scroll (menu, box, view_w, view_h, nat_w, nat_h);
		else {
			GtkWidget *scroll = verne_menu_get_scroll (menu);

			if (gtk_widget_get_parent (box) != NULL &&
			    gtk_widget_get_parent (box) != overlay)
				verne_menu_unparent_overlay_extra (box);
			if (scroll != NULL && GTK_IS_OVERLAY (gtk_widget_get_parent (scroll))) {
				gtk_overlay_remove_overlay (GTK_OVERLAY (gtk_widget_get_parent (scroll)), scroll);
				gtk_widget_set_visible (scroll, FALSE);
			}
			gtk_widget_set_size_request (box, view_w, view_h);
			extra = box;
		}
		if (gtk_widget_get_parent (extra) != overlay) {
			if (gtk_widget_get_parent (extra) != NULL)
				verne_menu_unparent_overlay_extra (extra);
			gtk_overlay_add_overlay (GTK_OVERLAY (overlay), extra);
		}
		gtk_widget_set_size_request (extra, view_w, view_h);
		g_object_set_data (G_OBJECT (box), "verne-dest-menu-x", GINT_TO_POINTER (lx));
		g_object_set_data (G_OBJECT (box), "verne-dest-menu-y", GINT_TO_POINTER (ly));
		g_object_set_data (G_OBJECT (box), "verne-dest-menu-w", GINT_TO_POINTER (view_w));
		g_object_set_data (G_OBJECT (box), "verne-dest-menu-h", GINT_TO_POINTER (view_h));
		if (extra != box) {
			g_object_set_data (G_OBJECT (extra), "verne-dest-menu-x", GINT_TO_POINTER (lx));
			g_object_set_data (G_OBJECT (extra), "verne-dest-menu-y", GINT_TO_POINTER (ly));
			g_object_set_data (G_OBJECT (extra), "verne-dest-menu-w", GINT_TO_POINTER (view_w));
			g_object_set_data (G_OBJECT (extra), "verne-dest-menu-h", GINT_TO_POINTER (view_h));
		}
		g_warning ("verne: dest menu overlay at %d,%d size %dx%d dest=%dx%d scroll=%d content=%d",
			   lx, ly, view_w, view_h, dest_w, dest_h, need_scroll, nat_h);
		gtk_overlay_set_clip_overlay (GTK_OVERLAY (overlay), extra, TRUE);
		gtk_widget_set_margin_start (extra, lx);
		gtk_widget_set_margin_top (extra, ly);
		gtk_widget_set_margin_start (box, extra == box ? lx : 0);
		gtk_widget_set_margin_top (box, extra == box ? ly : 0);
		gtk_widget_set_visible (extra, TRUE);
		gtk_widget_set_can_target (extra, TRUE);
		gtk_widget_set_visible (box, TRUE);
		gtk_widget_set_can_target (box, TRUE);
		gtk_widget_set_focusable (extra, TRUE);
		gtk_widget_grab_focus (extra);
		verne_overlay_attach_scroll_controllers (box);
		if (extra != box) {
			verne_overlay_attach_scroll_controllers (extra);
			if (g_object_get_data (G_OBJECT (extra), "verne-dest-click") == NULL) {
				GtkGesture *click = gtk_gesture_click_new ();

				gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), 1);
				gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (click),
									    GTK_PHASE_CAPTURE);
				g_signal_connect (click, "pressed",
						  G_CALLBACK (verne_dest_overlay_pressed), menu);
				gtk_widget_add_controller (extra, GTK_EVENT_CONTROLLER (click));
				g_object_set_data (G_OBJECT (extra), "verne-dest-click",
						   GINT_TO_POINTER (1));
			}
		}
	}
	{
		GtkWidget *ch;
		GString *dump = g_string_new ("verne: dest menu items");

		for (ch = gtk_widget_get_first_child (box); ch;
		     ch = gtk_widget_get_next_sibling (ch)) {
			if (!gtk_widget_get_visible (ch))
				continue;
			gtk_widget_set_can_target (ch, !GTK_IS_SEPARATOR_MENU_ITEM (ch));
			if (GTK_IS_BUTTON (ch) || GTK_IS_CHECK_BUTTON (ch))
				g_string_append_printf (dump, " | %s '%s'",
							G_OBJECT_TYPE_NAME (ch),
							verne_dest_item_label (ch));
		}
		g_warning ("%s", dump->str);
		g_string_free (dump, TRUE);
	}
	gtk_widget_queue_allocate (overlay);
	gtk_widget_queue_draw (host);
	gtk_widget_set_visible (GTK_WIDGET (menu), FALSE);
	if (g_object_get_data (G_OBJECT (box), "verne-dest-click") == NULL) {
		GtkGesture *click = gtk_gesture_click_new ();

		gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), 1);
		gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (click),
							    GTK_PHASE_CAPTURE);
		g_signal_connect (click, "pressed",
				  G_CALLBACK (verne_dest_overlay_pressed), menu);
		gtk_widget_add_controller (box, GTK_EVENT_CONTROLLER (click));
		g_object_set_data (G_OBJECT (box), "verne-dest-click", GINT_TO_POINTER (1));
	}
	g_object_set_data (G_OBJECT (menu), "verne-dest-overlay", overlay);
	verne_overlay_grab_keyboard (host);
	g_object_unref (box);
	return TRUE;
}

static void
verne_menu_popover_closed (GtkPopover *popover, gpointer data)
{
	GtkMenu *menu = data;

	(void) popover;
	if (!GTK_IS_MENU (menu))
		return;
	if (g_object_get_data (G_OBJECT (menu), "verne-popover-closing"))
		return;
	g_object_set_data (G_OBJECT (menu), "verne-popover-closing", GINT_TO_POINTER (1));
	verne_menu_restore_box_to_window (menu);
	if (!g_object_get_data (G_OBJECT (menu), "verne-dismissed"))
		gtk_menu_popdown (menu);
	g_object_set_data (G_OBJECT (menu), "verne-popover-closing", NULL);
}

static void
verne_menu_popdown_dest_popover (GtkMenu *menu)
{
	GtkWidget *popover;
	GtkWidget *box;

	if (!GTK_IS_MENU (menu))
		return;
	box = menu->box;
	if (GTK_IS_WIDGET (box))
		verne_widget_clear_active (box);
	popover = g_object_get_data (G_OBJECT (menu), "verne-dest-popover");
	verne_menu_restore_box_to_window (menu);
	if (GTK_IS_POPOVER (popover)) {
		g_signal_handlers_block_by_func (popover, verne_menu_popover_closed, menu);
		gtk_popover_popdown (GTK_POPOVER (popover));
		g_signal_handlers_unblock_by_func (popover, verne_menu_popover_closed, menu);
	}
}

static gboolean
verne_menu_popup_dest_popover (GtkMenu *menu, int root_x, int root_y)
{
	GtkWindow *dest;
	GtkWidget *parent;
	GtkWidget *popover;
	GtkWidget *box;
	GdkRectangle rect;
	int lx = root_x, ly = root_y;

	dest = verne_menu_dest_attach_window (GTK_WIDGET (menu));
	if (dest == NULL)
		return FALSE;
	parent = GTK_WIDGET (dest);

	popover = g_object_get_data (G_OBJECT (menu), "verne-dest-popover");
	if (!GTK_IS_POPOVER (popover)) {
		popover = gtk_popover_new ();
		gtk_popover_set_has_arrow (GTK_POPOVER (popover), FALSE);
		gtk_popover_set_autohide (GTK_POPOVER (popover), TRUE);
		gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);
		gtk_widget_add_css_class (popover, "menu");
		g_signal_connect (popover, "closed", G_CALLBACK (verne_menu_popover_closed), menu);
		g_object_set_data_full (G_OBJECT (menu), "verne-dest-popover",
					g_object_ref_sink (popover), g_object_unref);
	}
	if (gtk_widget_get_parent (popover) != parent) {
		if (gtk_widget_get_parent (popover) != NULL)
			gtk_widget_unparent (popover);
		gtk_widget_set_parent (popover, parent);
	}

	box = menu->box;
	if (box && gtk_widget_get_parent (box) != popover) {
		g_object_ref (box);
		if (gtk_window_get_child (GTK_WINDOW (menu)) == box)
			gtk_window_set_child (GTK_WINDOW (menu), NULL);
		else if (gtk_widget_get_parent (box) != NULL)
			gtk_widget_unparent (box);
		gtk_popover_set_child (GTK_POPOVER (popover), box);
		g_object_unref (box);
	}

#ifdef GDK_WINDOWING_X11
	{
		GdkSurface *s = gtk_native_get_surface (GTK_NATIVE (dest));
		if (s && GDK_IS_X11_SURFACE (s)) {
			Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));
			Window child = 0;
			int nx = 0, ny = 0;
			graphene_point_t pt;

			if (XTranslateCoordinates (dpy, DefaultRootWindow (dpy),
						   gdk_x11_surface_get_xid (s),
						   root_x, root_y, &nx, &ny, &child)) {
				if (gtk_widget_compute_point (GTK_WIDGET (dest), parent,
							      &GRAPHENE_POINT_INIT ((float) nx, (float) ny),
							      &pt)) {
					lx = (int) pt.x;
					ly = (int) pt.y;
				} else {
					lx = nx;
					ly = ny;
				}
			}
		}
	}
#endif
	rect.x = lx;
	rect.y = ly;
	rect.width = 1;
	rect.height = 1;
	gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
	gtk_widget_set_visible (GTK_WIDGET (menu), FALSE);
	gtk_popover_popup (GTK_POPOVER (popover));
#ifdef GDK_WINDOWING_X11
	{
		GdkSurface *ps = GTK_IS_NATIVE (popover) ? gtk_native_get_surface (GTK_NATIVE (popover)) : NULL;
		GdkSurface *ds = gtk_native_get_surface (GTK_NATIVE (dest));
		if (ps && ds && GDK_IS_X11_SURFACE (ps) && GDK_IS_X11_SURFACE (ds)) {
			Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (ps));
			Window pop_xid = gdk_x11_surface_get_xid (ps);
			Window dest_xid = gdk_x11_surface_get_xid (ds);
			XSetWindowAttributes attrs;
			Window root = 0, parent = 0, *children = NULL;
			unsigned n = 0;

			attrs.override_redirect = True;
			XChangeWindowAttributes (dpy, pop_xid, CWOverrideRedirect, &attrs);
			if (XQueryTree (dpy, pop_xid, &root, &parent, &children, &n)) {
				if (children)
					XFree (children);
			}
			if (parent != dest_xid) {
				XReparentWindow (dpy, pop_xid, dest_xid, lx, ly);
				g_warning ("verne: dest popover reparent 0x%lx -> dest 0x%lx at %d,%d (was parent 0x%lx)",
					   (unsigned long) pop_xid, (unsigned long) dest_xid, lx, ly,
					   (unsigned long) parent);
			}
			XMapRaised (dpy, pop_xid);
			XRaiseWindow (dpy, pop_xid);
			XFlush (dpy);
		}
	}
#endif
	g_warning ("verne: dest menu popover at %d,%d parent=%s",
		   lx, ly, G_OBJECT_TYPE_NAME (parent));
	return TRUE;
}

#ifdef GDK_WINDOWING_X11
static GdkSurface *
verne_menu_x11_surface (GtkWidget *w)
{
	GdkSurface *s;

	if (w == NULL || !GTK_IS_NATIVE (w))
		return NULL;
	s = gtk_native_get_surface (GTK_NATIVE (w));
	if (s == NULL || !GDK_IS_X11_SURFACE (s))
		return NULL;
	return s;
}

static void
verne_x11_consider_dest_canvas (Display *dpy, Window xid, Window skip,
				Window *best, int *best_area)
{
	XWindowAttributes wa;
	XClassHint ch = { 0, 0 };
	char *name = NULL;
	int area;
	gboolean class_ok = FALSE;

	if (xid == 0 || xid == skip)
		return;
	if (!XGetWindowAttributes (dpy, xid, &wa) || wa.map_state != IsViewable)
		return;
	if (wa.width < 400 || wa.height < 400)
		return;
	if (XFetchName (dpy, xid, &name) && name) {
		gboolean is_menu = (name[0] == ' ' && name[1] == '\0');
		XFree (name);
		if (is_menu)
			return;
	}
	if (XGetClassHint (dpy, xid, &ch)) {
		if (ch.res_class != NULL &&
		    (g_ascii_strcasecmp (ch.res_class, "nemo-desktop") == 0 ||
		     g_str_has_prefix (ch.res_class, "nemo-desktop")))
			class_ok = TRUE;
		if (ch.res_name)
			XFree (ch.res_name);
		if (ch.res_class)
			XFree (ch.res_class);
	}
	if (!class_ok)
		return;
	area = wa.width * wa.height;
	if (area > *best_area) {
		*best_area = area;
		*best = xid;
	}
}

static void
verne_x11_walk_dest_canvas (Display *dpy, Window start, Window skip,
			    Window *best, int *best_area, int depth)
{
	Window root = 0, parent = 0, *children = NULL;
	unsigned n = 0, i;

	verne_x11_consider_dest_canvas (dpy, start, skip, best, best_area);
	if (depth <= 0)
		return;
	if (!XQueryTree (dpy, start, &root, &parent, &children, &n))
		return;
	for (i = 0; i < n; i++)
		verne_x11_walk_dest_canvas (dpy, children[i], skip, best, best_area, depth - 1);
	if (children)
		XFree (children);
}

static Window
verne_x11_find_dest_canvas (Display *dpy, Window skip)
{
	Window best = 0;
	int best_area = 0;
	GListModel *model;
	guint i, n;

	if (dpy == NULL)
		return 0;
	verne_x11_walk_dest_canvas (dpy, DefaultRootWindow (dpy), skip, &best, &best_area, 3);

	model = gtk_window_get_toplevels ();
	n = g_list_model_get_n_items (model);
	for (i = 0; i < n; i++) {
		gpointer w = g_list_model_get_item (model, i);
		GdkSurface *s;

		if (w && GTK_IS_WINDOW (w) && !GTK_IS_MENU (w) &&
		    (gtk_window_get_type_hint (GTK_WINDOW (w)) == GDK_WINDOW_TYPE_HINT_DESKTOP ||
		     g_object_get_data (w, "is_desktop_window") != NULL)) {
			s = gtk_native_get_surface (GTK_NATIVE (w));
			if (s && GDK_IS_X11_SURFACE (s))
				verne_x11_walk_dest_canvas (dpy, gdk_x11_surface_get_xid (s),
							    skip, &best, &best_area, 2);
		}
		if (w)
			g_object_unref (w);
	}
	return best;
}
#endif

static void
verne_x11_lower_desktop_windows (void)
{
#ifdef GDK_WINDOWING_X11
	GdkDisplay *gdpy = gdk_display_get_default ();
	GListModel *model = gtk_window_get_toplevels ();
	guint i, n = g_list_model_get_n_items (model);
	Display *dpy = NULL;
	Window canvas;

	/* File-manager windows live in a different process from dest.
	 * Use the default X display so we can still XLowerWindow dest's
	 * canvas (class nemo-desktop) and keep File/Edit menus on top. */
	if (gdpy != NULL && GDK_IS_X11_DISPLAY (gdpy))
		dpy = gdk_x11_display_get_xdisplay (gdpy);

	for (i = 0; i < n; i++) {
		gpointer w = g_list_model_get_item (model, i);
		GdkSurface *s;

		if (w && GTK_IS_WINDOW (w) && !GTK_IS_MENU (w) &&
		    (gtk_window_get_type_hint (GTK_WINDOW (w)) == GDK_WINDOW_TYPE_HINT_DESKTOP ||
		     g_object_get_data (w, "is_desktop_window") != NULL)) {
			s = gtk_native_get_surface (GTK_NATIVE (w));
			if (s && GDK_IS_X11_SURFACE (s)) {
				dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));
				XLowerWindow (dpy, gdk_x11_surface_get_xid (s));
			}
		}
		if (w)
			g_object_unref (w);
	}
	if (dpy) {
		canvas = verne_x11_find_dest_canvas (dpy, 0);
		if (canvas)
			XLowerWindow (dpy, canvas);
	}
#endif
}

static void
verne_menu_log_layout (GtkMenu *menu, const char *kind, int x, int y, int nat_w, int nat_h)
{
	GtkWidget *box;
	GtkWidget *ch;
	GString *dump;
	double cy = 0;
	int box_w;

	box = gtk_menu_get_box (menu);
	box_w = nat_w > 0 ? nat_w : 240;
	dump = g_string_new (NULL);
	g_string_printf (dump, "verne: %s menu at %d,%d size %dx%d",
			 kind ? kind : "menu", x, y, nat_w, nat_h);
	for (ch = box ? gtk_widget_get_first_child (box) : NULL; ch;
	     ch = gtk_widget_get_next_sibling (ch)) {
		int nat_item = 0;

		if (!gtk_widget_get_visible (ch))
			continue;
		gtk_widget_measure (ch, GTK_ORIENTATION_VERTICAL, box_w,
				    NULL, &nat_item, NULL, NULL);
		if (nat_item < 1 || nat_item > 48)
			nat_item = GTK_IS_SEPARATOR_MENU_ITEM (ch) ? 9 : 28;
		g_string_append_printf (dump, " | %.0f-%.0f %s '%s'",
					cy, cy + nat_item,
					G_OBJECT_TYPE_NAME (ch),
					verne_dest_item_label (ch));
		cy += nat_item;
	}
	g_warning ("%s", dump->str);
	g_string_free (dump, TRUE);
}

static void
verne_menu_restack_file_popup (GtkWidget *w)
{
#ifdef GDK_WINDOWING_X11
	GdkSurface *s;
	Display *dpy;
	Window xid;
	Window canvas;

	if (w == NULL || verne_menu_dest_attach_window (w) != NULL)
		return;
	s = verne_menu_x11_surface (w);
	if (s == NULL)
		return;
	dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));
	xid = gdk_x11_surface_get_xid (s);
	canvas = verne_x11_find_dest_canvas (dpy, xid);
	if (canvas)
		XLowerWindow (dpy, canvas);
	if (g_object_get_data (G_OBJECT (w), "verne-popup-pos") != NULL)
		verne_x11_move (GTK_WINDOW (w),
				GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "verne-popup-x")),
				GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "verne-popup-y")));
	XMapRaised (dpy, xid);
	XRaiseWindow (dpy, xid);
	XFlush (dpy);
#else
	(void) w;
#endif
}

static void
verne_menu_unembed (GtkWidget *w)
{
#ifdef GDK_WINDOWING_X11
	GdkSurface *s = verne_menu_x11_surface (w);
	gpointer canvas_ptr;

	if (s == NULL)
		return;
	canvas_ptr = g_object_get_data (G_OBJECT (w), "verne-embed-canvas");
	if (canvas_ptr == NULL)
		return;
	{
		Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));
		Window xid = gdk_x11_surface_get_xid (s);

		XReparentWindow (dpy, xid, DefaultRootWindow (dpy), -20000, -20000);
		g_object_set_data (G_OBJECT (w), "verne-embed-canvas", NULL);
		XFlush (dpy);
	}
#else
	(void) w;
#endif
}

static void
verne_menu_embed_on_desktop (GtkWidget *w)
{
#ifdef GDK_WINDOWING_X11
	GdkSurface *s = verne_menu_x11_surface (w);
	Display *dpy;
	Window menu_xid, canvas, child;
	int lx = 0, ly = 0, mx = 0, my = 0;
	XWindowAttributes wa;
	XSetWindowAttributes attrs;

	if (s == NULL || verne_menu_dest_attach_window (w) == NULL)
		return;
	dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));
	menu_xid = gdk_x11_surface_get_xid (s);
	canvas = verne_x11_find_dest_canvas (dpy, menu_xid);
	if (canvas == 0)
		return;

	{
		Window root = 0, parent = 0, *children = NULL;
		unsigned n = 0;

		if (XQueryTree (dpy, menu_xid, &root, &parent, &children, &n)) {
			if (children)
				XFree (children);
			if (parent == canvas) {
				XMapRaised (dpy, menu_xid);
				XRaiseWindow (dpy, menu_xid);
				g_object_set_data (G_OBJECT (w), "verne-embed-canvas",
						   GSIZE_TO_POINTER ((gsize) canvas));
				return;
			}
		}
	}

	attrs.override_redirect = True;
	XChangeWindowAttributes (dpy, menu_xid, CWOverrideRedirect, &attrs);

	if (g_object_get_data (G_OBJECT (w), "verne-popup-pos")) {
		mx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "verne-popup-x"));
		my = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "verne-popup-y"));
	} else if (XGetWindowAttributes (dpy, menu_xid, &wa)) {
		mx = wa.x;
		my = wa.y;
	}
	if (!XTranslateCoordinates (dpy, DefaultRootWindow (dpy), canvas,
				    mx, my, &lx, &ly, &child)) {
		lx = mx;
		ly = my;
	}
	XReparentWindow (dpy, menu_xid, canvas, lx, ly);
	XMapRaised (dpy, menu_xid);
	XRaiseWindow (dpy, menu_xid);
	g_object_set_data (G_OBJECT (w), "verne-embed-canvas",
			   GSIZE_TO_POINTER ((gsize) canvas));
	XFlush (dpy);
	g_warning ("verne: dest menu embed xid=0x%lx canvas=0x%lx at %d,%d",
		   (unsigned long) menu_xid, (unsigned long) canvas, lx, ly);
#else
	(void) w;
#endif
}

static gboolean
verne_menu_keep_above (gpointer data)
{
	GtkWidget *w = data;

	if (!GTK_IS_MENU (w) ||
	    g_object_get_data (G_OBJECT (w), "verne-dismissed")) {
		verne_menu_unembed (w);
		g_object_unref (w);
		return G_SOURCE_REMOVE;
	}
	if (GTK_MENU (w)->box != NULL &&
	    GTK_IS_OVERLAY (gtk_widget_get_parent (GTK_MENU (w)->box)))
		return G_SOURCE_CONTINUE;
	if (!gtk_widget_get_visible (w)) {
		verne_menu_unembed (w);
		g_object_unref (w);
		return G_SOURCE_REMOVE;
	}
	if (verne_menu_dest_attach_window (w) != NULL) {
		verne_menu_set_override_redirect (w);
		verne_menu_embed_on_desktop (w);
		return G_SOURCE_CONTINUE;
	}
	verne_x11_lower_desktop_windows ();
	verne_menu_set_override_redirect (w);
	verne_menu_restack_file_popup (w);
	if (g_object_get_data (G_OBJECT (w), "verne-menu-hold") == NULL) {
#ifdef GDK_WINDOWING_X11
		GdkSurface *s = verne_menu_x11_surface (w);

		if (s != NULL) {
			Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));
			Window root_ret = 0, child = 0;
			int rx = 0, ry = 0, wx = 0, wy = 0;
			unsigned int mask = 0;
			int mx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "verne-popup-x"));
			int my = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "verne-popup-y"));
			int mw = gtk_widget_get_width (w);
			int mh = gtk_widget_get_height (w);

			if (mw < 1)
				mw = 240;
			if (mh < 1)
				mh = 32;
			if (XQueryPointer (dpy, DefaultRootWindow (dpy), &root_ret, &child,
					   &rx, &ry, &wx, &wy, &mask) &&
			    (mask & (Button1Mask | Button3Mask)) &&
			    (rx < mx || ry < my || rx > mx + mw || ry > my + mh)) {
				g_warning ("verne: file menu click-outside %d,%d menu=%d,%d %dx%d",
					   rx, ry, mx, my, mw, mh);
				gtk_menu_popdown (GTK_MENU (w));
				g_object_unref (w);
				return G_SOURCE_REMOVE;
			}
		}
#endif
	}
	return G_SOURCE_CONTINUE;
}

static void
verne_menu_set_override_redirect (GtkWidget *w)
{
#ifdef GDK_WINDOWING_X11
	GdkSurface *s = verne_menu_x11_surface (w);
	if (s) {
		Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));
		Window xid = gdk_x11_surface_get_xid (s);
		XSetWindowAttributes attrs;
		Atom type, value, state, above;
		Atom state_atoms[4];
		int n_state = 0;

		attrs.override_redirect = True;
		XChangeWindowAttributes (dpy, xid, CWOverrideRedirect, &attrs);
		type = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", False);
		value = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
		XChangeProperty (dpy, xid, type, XA_ATOM, 32, PropModeReplace,
				 (unsigned char *) &value, 1);
		state = XInternAtom (dpy, "_NET_WM_STATE", False);
		above = XInternAtom (dpy, "_NET_WM_STATE_ABOVE", False);
		state_atoms[n_state++] = above;
		state_atoms[n_state++] = XInternAtom (dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
		state_atoms[n_state++] = XInternAtom (dpy, "_NET_WM_STATE_SKIP_PAGER", False);
		XChangeProperty (dpy, xid, state, XA_ATOM, 32, PropModeReplace,
				 (unsigned char *) state_atoms, n_state);
		XRaiseWindow (dpy, xid);
		XFlush (dpy);
	}
#else
	(void) w;
#endif
}

static void
verne_menu_mapped (GtkWidget *w, gpointer data)
{
	(void) data;
	verne_menu_set_override_redirect (w);
#ifdef GDK_WINDOWING_X11
	{
		GdkSurface *s = gtk_native_get_surface (GTK_NATIVE (w));
		if (s && GDK_IS_X11_SURFACE (s)) {
			gdk_x11_surface_set_skip_taskbar_hint (s, TRUE);
			gdk_x11_surface_set_skip_pager_hint (s, TRUE);
		}
	}
#endif
	if (g_object_get_data (G_OBJECT (w), "verne-popup-pos") != NULL &&
	    g_object_get_data (G_OBJECT (w), "verne-embed-canvas") == NULL)
		verne_x11_move (GTK_WINDOW (w),
				GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "verne-popup-x")),
				GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "verne-popup-y")));
	verne_menu_set_override_redirect (w);
	if (verne_menu_dest_attach_window (w) != NULL)
		verne_menu_embed_on_desktop (w);
	else
		verne_menu_restack_file_popup (w);
}

static gboolean
verne_menu_key (GtkEventControllerKey *controller, guint keyval, guint keycode,
		GdkModifierType state, gpointer data)
{
	(void) controller; (void) keycode; (void) state;
	if (keyval == GDK_KEY_Escape) {
		gtk_menu_popdown (GTK_MENU (data));
		return TRUE;
	}
	return FALSE;
}

static void
verne_menu_is_active (GObject *obj, GParamSpec *pspec, gpointer data)
{
	GtkMenu *menu = GTK_MENU (obj);
	(void) pspec; (void) data;
	if (!gtk_widget_get_visible (GTK_WIDGET (menu)))
		return;
	if (g_object_get_data (obj, "verne-menu-hold"))
		return;
	/* Dest's full-screen canvas steals is-active from override-redirect
	 * file File/Edit menus, which parked them at -20000 before the user
	 * could click Preferences or Connect to Server. Dismiss via Escape,
	 * leaf activate, and click-outside instead. */
	(void) menu;
}

static gboolean
verne_menu_watch_focus (gpointer data)
{
	GtkWidget *w = data;
	if (GTK_IS_MENU (w))
		g_object_set_data (G_OBJECT (w), "verne-menu-hold", NULL);
	g_object_unref (w);
	return G_SOURCE_REMOVE;
}

static void
verne_pointer_root_xy (int *x, int *y)
{
	*x = 0;
	*y = 0;
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
				*x = rx;
				*y = ry;
			}
		}
	}
#endif
}

static void
verne_widget_root_xy (GtkWidget *widget, int *x, int *y)
{
	graphene_point_t pt;
	GtkRoot *root;
	*x = 0;
	*y = 0;
	if (widget == NULL)
		return;
	root = gtk_widget_get_root (widget);
	if (root == NULL ||
	    !gtk_widget_compute_point (widget, GTK_WIDGET (root),
				       &GRAPHENE_POINT_INIT (0, gtk_widget_get_height (widget)),
				       &pt)) {
		verne_pointer_root_xy (x, y);
		return;
	}
#ifdef GDK_WINDOWING_X11
	{
		GdkSurface *s = gtk_native_get_surface (GTK_NATIVE (root));
		if (s && GDK_IS_X11_SURFACE (s)) {
			Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));
			Window child;
			XTranslateCoordinates (dpy, gdk_x11_surface_get_xid (s),
					       DefaultRootWindow (dpy),
					       (int) pt.x, (int) pt.y, x, y, &child);
			return;
		}
	}
#endif
	verne_pointer_root_xy (x, y);
}

static void
verne_menu_set_attach (GtkMenu *menu, GtkWidget *attach)
{
	if (menu->attach == attach)
		return;
	if (menu->attach != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (menu->attach), (gpointer *) &menu->attach);
		menu->attach = NULL;
	}
	if (attach != NULL && GTK_IS_WIDGET (attach)) {
		menu->attach = attach;
		g_object_add_weak_pointer (G_OBJECT (attach), (gpointer *) &menu->attach);
	}
}

static void
verne_ensure_menu_attach (GtkMenu *menu, GtkWidget *fallback)
{
	GtkWidget *attach = menu->attach ? menu->attach : fallback;
	GtkRoot *root;

	if (attach && GTK_IS_WIDGET (attach))
		verne_menu_set_attach (menu, attach);
	if (menu->attach == NULL || !GTK_IS_WIDGET (menu->attach))
		return;
	root = gtk_widget_get_root (menu->attach);
	if (GTK_IS_WINDOW (root) && !GTK_IS_MENU (root) &&
	    gtk_window_get_type_hint (GTK_WINDOW (root)) != GDK_WINDOW_TYPE_HINT_DESKTOP)
		gtk_window_set_transient_for (GTK_WINDOW (menu), GTK_WINDOW (root));
	else if (GTK_IS_MENU (root))
		gtk_window_set_transient_for (GTK_WINDOW (menu), GTK_WINDOW (root));
}

static void gtk_menu_dispose (GObject *object)
{
	GtkMenu *menu = GTK_MENU (object);

	verne_menu_set_attach (menu, NULL);
	verne_menu_set_submenu_item (menu, NULL);
	if (menu->box != NULL) {
		GtkWidget *parent = gtk_widget_get_parent (menu->box);

		if (parent != NULL) {
			if (GTK_IS_WINDOW (parent))
				gtk_window_set_child (GTK_WINDOW (parent), NULL);
			else if (GTK_IS_POPOVER (parent))
				gtk_popover_set_child (GTK_POPOVER (parent), NULL);
			else if (GTK_IS_OVERLAY (parent))
				gtk_overlay_remove_overlay (GTK_OVERLAY (parent), menu->box);
			else
				gtk_widget_unparent (menu->box);
		}
		g_clear_object (&menu->box);
	}
	G_OBJECT_CLASS (gtk_menu_parent_class)->dispose (object);
}

static void gtk_menu_class_init (GtkMenuClass *c)
{
	G_OBJECT_CLASS (c)->dispose = gtk_menu_dispose;
	/* GTK3 GtkMenuShell::deactivate / selection-done. Needed so tree
	 * popups free popup_file and nemo_drag_drop_action_ask can quit
	 * its nested main loop when the Ask menu is dismissed. */
	if (g_signal_lookup ("deactivate", GTK_TYPE_MENU) == 0)
		g_signal_new ("deactivate",
			      G_TYPE_FROM_CLASS (c),
			      G_SIGNAL_RUN_FIRST,
			      0, NULL, NULL, NULL,
			      G_TYPE_NONE, 0);
	if (g_signal_lookup ("selection-done", GTK_TYPE_MENU) == 0)
		g_signal_new ("selection-done",
			      G_TYPE_FROM_CLASS (c),
			      G_SIGNAL_RUN_FIRST,
			      0, NULL, NULL, NULL,
			      G_TYPE_NONE, 0);
}
static void
gtk_menu_init (GtkMenu *menu)
{
	GtkEventController *keys;

	verne_menu_ensure_css ();
	gtk_window_set_decorated (GTK_WINDOW (menu), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (menu), FALSE);
	gtk_window_set_deletable (GTK_WINDOW (menu), FALSE);
	gtk_window_set_title (GTK_WINDOW (menu), " ");
	gtk_window_set_hide_on_close (GTK_WINDOW (menu), TRUE);
	gtk_window_set_type_hint (GTK_WINDOW (menu), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
	gtk_widget_add_css_class (GTK_WIDGET (menu), "popup");
	gtk_widget_add_css_class (GTK_WIDGET (menu), "menu");
	menu->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	g_object_ref_sink (menu->box);
	gtk_widget_add_css_class (menu->box, "menu");
	gtk_window_set_child (GTK_WINDOW (menu), menu->box);
	g_signal_connect (menu, "realize", G_CALLBACK (verne_menu_set_override_redirect), NULL);
	g_signal_connect (menu, "close-request", G_CALLBACK (verne_menu_close_request), NULL);
	g_signal_connect (menu, "map", G_CALLBACK (verne_menu_mapped), NULL);
	g_signal_connect (menu, "notify::is-active", G_CALLBACK (verne_menu_is_active), NULL);
	keys = gtk_event_controller_key_new ();
	gtk_event_controller_set_propagation_phase (keys, GTK_PHASE_CAPTURE);
	g_signal_connect (keys, "key-pressed", G_CALLBACK (verne_menu_key), menu);
	gtk_widget_add_controller (GTK_WIDGET (menu), keys);
}
GtkWidget *gtk_menu_new (void) { return g_object_new (GTK_TYPE_MENU, NULL); }

static void
verne_menu_leaf_clicked (GtkWidget *item, gpointer data)
{
	GtkMenu *menu = data;

	if (!GTK_IS_MENU (menu) || !GTK_IS_WIDGET (item))
		return;
	if (g_object_get_data (G_OBJECT (menu), "verne-dismissed"))
		return;

	/* GTK3 emits GtkMenuItem::activate before the shell deactivates.
	 * Ask-drop and other menus wait on activate to record the choice;
	 * popping down first would quit those loops with chosen==0.
	 * Block this clicked handler so GtkButton::activate cannot re-enter. */
	g_object_ref (menu);
	g_signal_handlers_block_by_func (item, G_CALLBACK (verne_menu_leaf_clicked), menu);
	/* UI-manager items already run the GtkAction from GtkButton::clicked.
	 * Emitting activate here re-enters clicked and opened About / Connect
	 * twice, then SIGSEGV'd the file window on close. Ask-drop items have
	 * no action and still need GtkMenuItem::activate. */
	if (g_object_get_data (G_OBJECT (item), "verne-action-clicked") == NULL &&
	    g_signal_lookup ("activate", G_OBJECT_TYPE (item)) != 0)
		g_signal_emit_by_name (item, "activate");
	g_signal_handlers_unblock_by_func (item, G_CALLBACK (verne_menu_leaf_clicked), menu);
	/* Dismiss the whole cascade. gtk_menu_popdown() only closes this
	 * menu and its children so a parent File/View menu can stay open
	 * while a nested submenu is showing. */
	verne_menu_hide_others_later ();
	g_object_unref (menu);
}

static void
verne_menu_disconnect_leaf_hooks (GtkMenu *menu)
{
	GtkWidget *ch;
	GtkWidget *box;

	if (menu == NULL)
		return;
	box = gtk_menu_get_box (menu);
	for (ch = box ? gtk_widget_get_first_child (box) : NULL; ch; ch = gtk_widget_get_next_sibling (ch)) {
		if (!g_object_get_data (G_OBJECT (ch), "verne-leaf-hooked"))
			continue;
		g_signal_handlers_disconnect_by_func (ch, G_CALLBACK (verne_menu_leaf_clicked), menu);
		g_object_set_data (G_OBJECT (ch), "verne-leaf-hooked", NULL);
	}
}

static void
verne_menu_hook_leaf_items (GtkMenu *menu)
{
	GtkWidget *ch;
	GtkWidget *box = gtk_menu_get_box (menu);
	for (ch = box ? gtk_widget_get_first_child (box) : NULL; ch; ch = gtk_widget_get_next_sibling (ch)) {
		if (g_object_get_data (G_OBJECT (ch), "verne-leaf-hooked"))
			continue;
		if (GTK_IS_SEPARATOR (ch) || !gtk_widget_get_sensitive (ch))
			continue;
		if (GTK_IS_MENU_ITEM (ch) && gtk_menu_item_get_submenu (GTK_MENU_ITEM (ch)))
			continue;
		if (GTK_IS_BUTTON (ch) || GTK_IS_CHECK_BUTTON (ch)) {
			g_signal_connect (ch, "clicked", G_CALLBACK (verne_menu_leaf_clicked), menu);
			g_object_set_data (G_OBJECT (ch), "verne-leaf-hooked", GINT_TO_POINTER (1));
		}
	}
}

typedef struct {
	GtkMenu *menu;
	int x, y;
	gboolean has_pos;
	guint serial;
} VernePopupData;

static guint
verne_menu_serial (GtkMenu *menu)
{
	return GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (menu), "verne-popup-serial"));
}

static guint
verne_menu_bump_serial (GtkMenu *menu)
{
	guint s = verne_menu_serial (menu) + 1;

	g_object_set_data (G_OBJECT (menu), "verne-popup-serial", GUINT_TO_POINTER (s));
	return s;
}

static void
verne_menu_set_input_enabled (GtkWidget *w, gboolean enabled)
{
	GdkSurface *s;

	if (w == NULL || !GTK_IS_NATIVE (w))
		return;
	s = gtk_native_get_surface (GTK_NATIVE (w));
	if (s == NULL)
		return;
	if (enabled) {
		gdk_surface_set_input_region (s, NULL);
	} else {
		cairo_region_t *empty = cairo_region_create ();
		gdk_surface_set_input_region (s, empty);
		cairo_region_destroy (empty);
	}
}

static void
verne_menu_unmap_surface (GtkWidget *w)
{
	GdkSurface *s;

	if (w == NULL || !GTK_IS_NATIVE (w))
		return;
	s = gtk_native_get_surface (GTK_NATIVE (w));
	if (s == NULL)
		return;
	gdk_surface_hide (s);
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_SURFACE (s)) {
		Display *dpy = gdk_x11_display_get_xdisplay (gdk_surface_get_display (s));
		Window xid = gdk_x11_surface_get_xid (s);
		XUnmapWindow (dpy, xid);
		XFlush (dpy);
	}
#endif
}

static GtkMenu *
verne_menu_from_item (GtkWidget *item)
{
	GtkWidget *parent;
	GtkWidget *walk;

	if (!GTK_IS_WIDGET (item))
		return NULL;

	parent = gtk_widget_get_ancestor (item, GTK_TYPE_MENU);
	if (GTK_IS_MENU (parent))
		return GTK_MENU (parent);

	for (walk = item; walk != NULL; walk = gtk_widget_get_parent (walk)) {
		parent = g_object_get_data (G_OBJECT (walk), "verne-overlay-menu");
		if (GTK_IS_MENU (parent))
			return GTK_MENU (parent);
	}
	return NULL;
}

static GtkMenu *
verne_menu_from_item (GtkWidget *item);

static void
verne_menu_submenu_item_gone (gpointer data, GObject *gone)
{
	GtkMenu *menu = data;

	if (!GTK_IS_MENU (menu))
		return;
	if (g_object_get_data (G_OBJECT (menu), "verne-submenu-item") == gone)
		g_object_set_data (G_OBJECT (menu), "verne-submenu-item", NULL);
}

static void
verne_menu_set_submenu_item (GtkMenu *menu, GtkWidget *item)
{
	GtkWidget *old;

	if (!GTK_IS_MENU (menu))
		return;
	old = g_object_get_data (G_OBJECT (menu), "verne-submenu-item");
	if (old == item)
		return;
	if (old != NULL)
		g_object_weak_unref (G_OBJECT (old), verne_menu_submenu_item_gone, menu);
	g_object_set_data (G_OBJECT (menu), "verne-submenu-item", item);
	if (item != NULL)
		g_object_weak_ref (G_OBJECT (item), verne_menu_submenu_item_gone, menu);
}

static GtkMenu *
verne_menu_parent_menu (GtkMenu *menu)
{
	GtkWidget *item;
	GtkWidget *attach;
	GtkWidget *parent;

	if (menu == NULL)
		return NULL;

	item = g_object_get_data (G_OBJECT (menu), "verne-submenu-item");
	if (GTK_IS_WIDGET (item)) {
		parent = GTK_WIDGET (verne_menu_from_item (item));
		if (GTK_IS_MENU (parent) && parent != GTK_WIDGET (menu))
			return GTK_MENU (parent);
	}

	attach = menu->attach;
	if (GTK_IS_WIDGET (attach)) {
		parent = gtk_widget_get_ancestor (attach, GTK_TYPE_MENU);
		if (GTK_IS_MENU (parent) && parent != GTK_WIDGET (menu))
			return GTK_MENU (parent);
	}

	return NULL;
}

static gboolean
verne_menu_is_ancestor_of (GtkMenu *maybe_anc, GtkMenu *desc)
{
	GtkMenu *cur = desc;
	int guard = 0;

	if (maybe_anc == NULL || desc == NULL || maybe_anc == desc)
		return FALSE;

	while (cur != NULL && guard++ < 8) {
		cur = verne_menu_parent_menu (cur);
		if (cur == maybe_anc)
			return TRUE;
	}
	return FALSE;
}

static void
verne_menu_hide_overlay_leftovers (GtkMenu *keep)
{
	GListModel *model = gtk_window_get_toplevels ();
	guint i, n = g_list_model_get_n_items (model);
	GtkWidget *keep_box = (keep != NULL) ? keep->box : NULL;

	for (i = 0; i < n; i++) {
		gpointer w = g_list_model_get_item (model, i);
		GtkWidget *overlay;
		GtkWidget *ch, *next;

		if (!GTK_IS_WINDOW (w) || GTK_IS_MENU (w)) {
			if (w)
				g_object_unref (w);
			continue;
		}
		overlay = g_object_get_data (G_OBJECT (w), "verne-file-menu-overlay");
		if (!GTK_IS_OVERLAY (overlay))
			overlay = g_object_get_data (G_OBJECT (w), "verne-dest-menu-overlay");
		if (GTK_IS_OVERLAY (overlay)) {
			for (ch = gtk_widget_get_first_child (overlay); ch; ch = next) {
				GtkWidget *walk;
				gboolean keep_here = FALSE;

				next = gtk_widget_get_next_sibling (ch);
				if (ch == keep_box)
					continue;
				if (ch == gtk_overlay_get_child (GTK_OVERLAY (overlay)))
					continue;
				for (walk = keep_box; GTK_IS_WIDGET (walk); walk = gtk_widget_get_parent (walk)) {
					if (walk == ch) {
						keep_here = TRUE;
						break;
					}
				}
				if (keep_here)
					continue;
				if (!gtk_widget_has_css_class (ch, "verne-dest-menu"))
					continue;
				gtk_widget_set_visible (ch, FALSE);
				gtk_widget_set_can_target (ch, FALSE);
			}
		}
		if (w)
			g_object_unref (w);
	}
}

static void
verne_menu_hide_others (GtkMenu *keep)
{
	GListModel *model = gtk_window_get_toplevels ();
	guint i, n = g_list_model_get_n_items (model);
	for (i = 0; i < n; i++) {
		gpointer w = g_list_model_get_item (model, i);
		if (GTK_IS_MENU (w) && w != keep &&
		    !(keep != NULL && verne_menu_is_ancestor_of (GTK_MENU (w), keep)))
			gtk_menu_popdown (GTK_MENU (w));
		if (w)
			g_object_unref (w);
	}
	verne_menu_hide_overlay_leftovers (keep);
	if (keep == NULL)
		verne_overlay_ungrab_keyboard ();
}

static void
verne_dest_customize_activate_at (GtkWidget *toplevel, GtkWidget *wrap, double x, double y)
{
	GtkWidget *picked;
	GtkWidget *w;

	picked = gtk_widget_pick (toplevel, x, y, GTK_PICK_DEFAULT);
	g_warning ("verne: dest customize click %.0f,%.0f pick=%s",
		   x, y, picked ? G_OBJECT_TYPE_NAME (picked) : "NULL");
	for (w = picked; GTK_IS_WIDGET (w) && w != wrap && w != toplevel;
	     w = gtk_widget_get_parent (w)) {
		if (GTK_IS_SWITCH (w) || GTK_IS_LIST_BOX_ROW (w) || GTK_IS_LABEL (w) ||
		    GTK_IS_IMAGE (w)) {
			GtkWidget *sw = GTK_IS_SWITCH (w) ? w : NULL;
			GtkWidget *row = w;
			GtkWidget *ch;

			while (sw == NULL && GTK_IS_WIDGET (row) && row != wrap && row != toplevel) {
				if (GTK_IS_SWITCH (row)) {
					sw = row;
					break;
				}
				for (ch = gtk_widget_get_first_child (row); ch && sw == NULL;
				     ch = gtk_widget_get_next_sibling (ch)) {
					if (GTK_IS_SWITCH (ch))
						sw = ch;
					else if (GTK_IS_BOX (ch)) {
						GtkWidget *inner;
						for (inner = gtk_widget_get_first_child (ch);
						     inner && sw == NULL;
						     inner = gtk_widget_get_next_sibling (inner)) {
							if (GTK_IS_SWITCH (inner))
								sw = inner;
						}
					}
				}
				if (GTK_IS_LIST_BOX_ROW (row))
					break;
				row = gtk_widget_get_parent (row);
			}
			if (GTK_IS_SWITCH (sw)) {
				gboolean newv = !gtk_switch_get_active (GTK_SWITCH (sw));
				GtkAction *action;

				gtk_switch_set_active (GTK_SWITCH (sw), newv);
				action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (sw));
				if (GTK_IS_TOGGLE_ACTION (action))
					gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), newv);
				g_warning ("verne: dest customize toggle switch active=%d action=%s",
					   newv, action ? gtk_action_get_name (action) : "none");
				return;
			}
			if (GTK_IS_SWITCH (w))
				return;
		}
		if (GTK_IS_COMBO_BOX (w)) {
			gtk_combo_box_popup (GTK_COMBO_BOX (w));
			g_warning ("verne: dest customize popup combo");
			return;
		}
		if (GTK_IS_RANGE (w)) {
			GtkOrientation ori = gtk_orientable_get_orientation (GTK_ORIENTABLE (w));
			graphene_rect_t bounds;
			GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (w));
			double lo, hi, t = 0.5;

			if (adj && gtk_widget_compute_bounds (w, toplevel, &bounds) &&
			    bounds.size.width > 1 && bounds.size.height > 1) {
				lo = gtk_adjustment_get_lower (adj);
				hi = gtk_adjustment_get_upper (adj);
				if (ori == GTK_ORIENTATION_VERTICAL)
					t = (y - bounds.origin.y) / bounds.size.height;
				else
					t = (x - bounds.origin.x) / bounds.size.width;
				if (t < 0)
					t = 0;
				if (t > 1)
					t = 1;
				gtk_range_set_value (GTK_RANGE (w), lo + t * (hi - lo));
			}
			g_warning ("verne: dest customize range");
			return;
		}
		if (GTK_IS_BUTTON (w)) {
			g_warning ("verne: dest customize click button %s", G_OBJECT_TYPE_NAME (w));
			g_signal_emit_by_name (w, "clicked");
			return;
		}
	}
}

static void
verne_toplevel_dismiss_menus (GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *toplevel;
	GtkWidget *picked;
	gboolean dest_overlay_open;

	(void) n_press; (void) data;

	toplevel = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
	if (toplevel == NULL)
		return;
	{
		GtkWidget *wrap = g_object_get_data (G_OBJECT (toplevel), "verne-dest-customize-wrap");

		if (GTK_IS_WIDGET (wrap) && gtk_widget_get_visible (wrap)) {
			int ww = gtk_widget_get_width (wrap);
			int wh = gtk_widget_get_height (wrap);
			int dw = gtk_widget_get_width (toplevel);
			int dh = gtk_widget_get_height (toplevel);
			int wx, wy;
			guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

			if (ww < 100)
				ww = 694;
			if (wh < 100)
				wh = 497;
			if (dw < ww)
				dw = ww;
			if (dh < wh)
				dh = wh;
			wx = (dw - ww) / 2;
			wy = (dh - wh) / 2;
			if (x >= wx && x < wx + ww && y >= wy && y < wy + wh) {
				gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
				if (button == 1 && x < wx + 56 && y < wy + 48) {
					GtkWidget *close_btn = g_object_get_data (G_OBJECT (wrap),
										  "verne-dest-customize-close");

					if (GTK_IS_BUTTON (close_btn))
						g_signal_emit_by_name (close_btn, "clicked");
					g_warning ("verne: dest customize close at %.0f,%.0f wrap=%d,%d %dx%d",
						   x, y, wx, wy, ww, wh);
				} else if (button == 1) {
					verne_dest_customize_activate_at (toplevel, wrap, x, y);
				} else {
					g_warning ("verne: dest customize click %.0f,%.0f wrap=%d,%d %dx%d",
						   x, y, wx, wy, ww, wh);
				}
				return;
			}
		}
	}
	dest_overlay_open = verne_any_dest_overlay_visible ();
	picked = gtk_widget_pick (toplevel, x, y, GTK_PICK_DEFAULT);
	if (dest_overlay_open && GTK_IS_WIDGET (toplevel)) {
		GListModel *model = gtk_window_get_toplevels ();
		guint i, n = g_list_model_get_n_items (model);
		guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

		for (i = 0; i < n; i++) {
			gpointer w = g_list_model_get_item (model, i);
			GtkWidget *box;
			int mx, my, mw, mh;

			if (!verne_menu_is_dest_overlay (w)) {
				if (w)
					g_object_unref (w);
				continue;
			}
			box = GTK_MENU (w)->box;
			mx = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (box), "verne-dest-menu-x"));
			my = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (box), "verne-dest-menu-y"));
			mw = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (box), "verne-dest-menu-w"));
			mh = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (box), "verne-dest-menu-h"));
			g_warning ("verne: dest dismiss pick=%s click=%.0f,%.0f menu=%d,%d %dx%d",
				   picked ? G_OBJECT_TYPE_NAME (picked) : "NULL",
				   x, y, mx, my, mw, mh);
			if (mw > 0 && mh > 0 &&
			    x >= mx && x < mx + mw && y >= my && y < my + mh) {
				GtkWidget *btn;
				GtkWidget *hit_box;
				double lx = x - mx, ly = y - my;

				hit_box = verne_overlay_hit_box (box, &lx, &ly);
				if (hit_box != NULL)
					box = hit_box;
				btn = verne_dest_menu_button_at (box, lx, ly);
				g_warning ("verne: dest menu hit ly=%.0f btn=%s label=%s",
					   ly,
					   btn ? G_OBJECT_TYPE_NAME (btn) : "NULL",
					   verne_dest_item_label (btn));
				if (button == 1 && btn != NULL && btn != box &&
				    !GTK_IS_SEPARATOR_MENU_ITEM (btn) &&
				    (GTK_IS_BUTTON (btn) || GTK_IS_CHECK_BUTTON (btn))) {
					gtk_gesture_set_state (GTK_GESTURE (gesture),
							       GTK_EVENT_SEQUENCE_CLAIMED);
					g_warning ("verne: dest overlay rect-activate %s label=%s",
						   G_OBJECT_TYPE_NAME (btn),
						   verne_dest_item_label (btn));
					if (w)
						g_object_unref (w);
					verne_overlay_activate_leaf (btn);
					return;
				}
				if (w)
					g_object_unref (w);
				/* Scrollbar / padding inside the menu rect must not dismiss. */
				gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
				return;
			}
			if (w)
				g_object_unref (w);
		}
	}
	/* Click-outside dismiss must not grab the sequence. */
	gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
	if (picked != NULL && GTK_IS_WIDGET (picked)) {
		GtkWidget *walk;
		for (walk = picked; walk != NULL; walk = gtk_widget_get_parent (walk)) {
			if (gtk_widget_has_css_class (walk, "verne-dest-menu")) {
				GtkWidget *btn = picked;
				guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

				while (btn != NULL && btn != walk &&
				       !GTK_IS_BUTTON (btn) && !GTK_IS_CHECK_BUTTON (btn))
					btn = gtk_widget_get_parent (btn);
				if (button == 1 && btn != NULL && btn != walk &&
				    !GTK_IS_SEPARATOR_MENU_ITEM (btn) &&
				    (GTK_IS_BUTTON (btn) || GTK_IS_CHECK_BUTTON (btn))) {
					gtk_gesture_set_state (GTK_GESTURE (gesture),
							       GTK_EVENT_SEQUENCE_CLAIMED);
					g_warning ("verne: dest overlay dismiss-activate %s",
						   G_OBJECT_TYPE_NAME (btn));
					verne_overlay_activate_leaf (btn);
					return;
				}
				verne_menu_hide_others_later ();
				return;
			}
			if (gtk_widget_has_css_class (walk, "menu") && GTK_IS_BOX (walk))
				return;
		}
		/* File-window chrome: do not dismiss menus when clicking the
		 * menubar, a menu item, or notebook tabs. Dest canvas lives
		 * inside a notebook, so skip that guard while a dest overlay
		 * menu is painted — otherwise click-outside never hides it. */
		if (!dest_overlay_open &&
		    (GTK_IS_MENU_ITEM (picked) || GTK_IS_MENU_BAR (picked) ||
		     GTK_IS_NOTEBOOK (picked) ||
		     gtk_widget_get_ancestor (picked, GTK_TYPE_MENU_ITEM) != NULL ||
		     gtk_widget_get_ancestor (picked, GTK_TYPE_MENU_BAR) != NULL ||
		     gtk_widget_get_ancestor (picked, GTK_TYPE_NOTEBOOK) != NULL))
			return;
	}
	verne_menu_hide_others_later ();
}

static gboolean verne_toplevel_escape_menus (GtkEventControllerKey *controller, guint keyval, guint keycode,
					     GdkModifierType state, gpointer data);

static void
verne_menu_attach_dismiss_on_toplevels (void)
{
	GListModel *model = gtk_window_get_toplevels ();
	guint i, n = g_list_model_get_n_items (model);
	for (i = 0; i < n; i++) {
		gpointer w = g_list_model_get_item (model, i);
		if (w && GTK_IS_WINDOW (w) && !GTK_IS_MENU (w) &&
		    g_object_get_data (w, "verne-menu-dismiss-hooked") == NULL) {
			GtkGesture *click = gtk_gesture_click_new ();
			gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), GDK_BUTTON_PRIMARY);
			gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (click),
								    GTK_PHASE_CAPTURE);
			g_signal_connect (click, "pressed",
					  G_CALLBACK (verne_toplevel_dismiss_menus), NULL);
			gtk_widget_add_controller (GTK_WIDGET (w), GTK_EVENT_CONTROLLER (click));
			{
				GtkEventController *keys = gtk_event_controller_key_new ();
				gtk_event_controller_set_propagation_phase (keys, GTK_PHASE_CAPTURE);
				g_signal_connect (keys, "key-pressed",
						  G_CALLBACK (verne_toplevel_escape_menus), NULL);
				gtk_widget_add_controller (GTK_WIDGET (w), keys);
			}
			g_object_set_data (w, "verne-menu-dismiss-hooked", GINT_TO_POINTER (1));
		}
		if (w)
			g_object_unref (w);
	}
}

static gboolean
verne_menu_unmap_idle (gpointer data)
{
	GtkWidget *w = data;
	if (GTK_IS_MENU (w) &&
	    (g_object_get_data (G_OBJECT (w), "verne-dismissed") ||
	     !gtk_widget_get_visible (w))) {
		gtk_widget_set_visible (w, FALSE);
		gtk_widget_set_opacity (w, 0.0);
		verne_menu_unmap_surface (w);
	}
	g_object_unref (w);
	return G_SOURCE_REMOVE;
}

static gboolean
verne_any_menu_visible (void)
{
	GListModel *model = gtk_window_get_toplevels ();
	guint i, n = g_list_model_get_n_items (model);
	gboolean any = FALSE;
	for (i = 0; i < n; i++) {
		gpointer w = g_list_model_get_item (model, i);
		if (GTK_IS_MENU (w) && g_object_get_data (w, "verne-dismissed") == NULL &&
		    (gtk_widget_get_visible (GTK_WIDGET (w)) || verne_menu_is_dest_overlay (w)))
			any = TRUE;
		if (w)
			g_object_unref (w);
	}
	return any;
}

static gboolean
verne_toplevel_escape_menus (GtkEventControllerKey *controller, guint keyval, guint keycode,
			     GdkModifierType state, gpointer data)
{
	(void) controller; (void) keycode; (void) state; (void) data;
	if (keyval != GDK_KEY_Escape)
		return FALSE;
	if (!verne_any_menu_visible ())
		return FALSE;
	verne_menu_hide_others (NULL);
	return TRUE;
}

static void
verne_menu_refresh_accels (GtkMenu *menu)
{
	GtkWidget *box, *child;
	const gchar *path;

	if (menu == NULL)
		return;
	box = gtk_menu_get_box (menu);
	if (box == NULL)
		return;
	for (child = gtk_widget_get_first_child (box); child; child = gtk_widget_get_next_sibling (child)) {
		if (!GTK_IS_MENU_ITEM (child))
			continue;
		path = g_object_get_data (G_OBJECT (child), "verne-accel-path");
		if (path)
			gtk_menu_item_set_accel_path (GTK_MENU_ITEM (child), path);
	}
}

static gboolean
verne_menu_popup_idle (gpointer data)
{
	VernePopupData *p = data;
	GtkWidget *w = GTK_WIDGET (p->menu);
	GtkWidget *box;
	int nat_w = 220, nat_h = 32;

	if (!GTK_IS_MENU (p->menu)) {
		g_object_unref (p->menu);
		g_free (p);
		return G_SOURCE_REMOVE;
	}
	if (g_object_get_data (G_OBJECT (p->menu), "verne-dismissed") ||
	    p->serial != verne_menu_serial (p->menu)) {
		g_warning ("verne: menu popup idle bail dismissed=%p serial=%u/%u",
			   g_object_get_data (G_OBJECT (p->menu), "verne-dismissed"),
			   p->serial, verne_menu_serial (p->menu));
		g_object_unref (p->menu);
		g_free (p);
		return G_SOURCE_REMOVE;
	}
	verne_ensure_menu_attach (p->menu, NULL);
	verne_menu_refresh_accels (p->menu);
	box = gtk_menu_get_box (p->menu);
	verne_menu_update_separators (box);
	verne_menu_hook_leaf_items (p->menu);
	if (box) {
		gtk_widget_measure (box, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &nat_w, NULL, NULL);
		gtk_widget_measure (box, GTK_ORIENTATION_VERTICAL, nat_w, NULL, &nat_h, NULL, NULL);
	}
	if (nat_w < 240)
		nat_w = 240;
	if (nat_h < 24)
		nat_h = 24;
	gtk_window_set_default_size (GTK_WINDOW (p->menu), nat_w, nat_h);
	if (p->has_pos) {
		g_object_set_data (G_OBJECT (w), "verne-popup-pos", GINT_TO_POINTER (1));
		g_object_set_data (G_OBJECT (w), "verne-popup-x", GINT_TO_POINTER (p->x));
		g_object_set_data (G_OBJECT (w), "verne-popup-y", GINT_TO_POINTER (p->y));
	}
	g_object_set_data (G_OBJECT (w), "verne-menu-hold", GINT_TO_POINTER (1));
	g_object_set_data (G_OBJECT (w), "verne-dismissed", NULL);
	verne_menu_attach_dismiss_on_toplevels ();
	if (verne_hide_others_idle_id != 0) {
		g_source_remove (verne_hide_others_idle_id);
		verne_hide_others_idle_id = 0;
	}
	verne_menu_hide_others (p->menu);
	if (verne_menu_popup_dest_overlay (p->menu, p->x, p->y) ||
	    verne_menu_popup_dest_popover (p->menu, p->x, p->y)) {
		g_timeout_add (350, verne_menu_watch_focus, g_object_ref (w));
		g_object_unref (p->menu);
		g_free (p);
		return G_SOURCE_REMOVE;
	}
	gtk_widget_set_opacity (w, 1.0);
	gtk_widget_set_visible (w, TRUE);
	gtk_window_present (GTK_WINDOW (p->menu));
	verne_menu_set_input_enabled (w, TRUE);
	gtk_widget_grab_focus (w);
	if (p->has_pos)
		verne_x11_move (GTK_WINDOW (p->menu), p->x, p->y);
	verne_menu_set_override_redirect (w);
	if (verne_menu_dest_attach_window (w) != NULL) {
		verne_menu_embed_on_desktop (w);
	} else {
		verne_x11_lower_desktop_windows ();
		verne_menu_restack_file_popup (w);
		verne_menu_log_layout (p->menu, "file", p->x, p->y, nat_w, nat_h);
	}
	if (g_object_get_data (G_OBJECT (w), "verne-keep-above") == NULL) {
		g_object_set_data (G_OBJECT (w), "verne-keep-above", GINT_TO_POINTER (1));
		g_timeout_add (50, verne_menu_keep_above, g_object_ref (w));
	}
	g_timeout_add (350, verne_menu_watch_focus, g_object_ref (w));
	g_object_unref (p->menu);
	g_free (p);
	return G_SOURCE_REMOVE;
}

static void
verne_menu_popup_now (GtkMenu *menu, int root_x, int root_y, gboolean has_pos)
{
	VernePopupData *p;

	if (menu == NULL)
		return;
	p = g_new0 (VernePopupData, 1);
	p->menu = g_object_ref (menu);
	p->x = root_x;
	p->y = root_y;
	p->has_pos = has_pos;
	p->serial = verne_menu_bump_serial (menu);
	g_object_set_data (G_OBJECT (menu), "verne-dismissed", NULL);
	g_idle_add (verne_menu_popup_idle, p);
}

static void
verne_menu_emit_shell_signal (GtkMenu *menu, const char *name)
{
	guint sid;
	GSignalQuery query;
	gboolean handled = FALSE;

	sid = g_signal_lookup (name, G_OBJECT_TYPE (menu));
	if (sid == 0)
		return;
	g_signal_query (sid, &query);
	if (query.return_type == G_TYPE_NONE)
		g_signal_emit (menu, sid, 0);
	else
		g_signal_emit (menu, sid, 0, &handled);
}

void gtk_menu_popup_at_pointer (GtkMenu *menu, const GdkEvent *trigger) {
	int x = 0, y = 0;
	(void) trigger;
	verne_ensure_menu_attach (menu, NULL);
	verne_pointer_root_xy (&x, &y);
	verne_menu_popup_now (menu, x, y, TRUE);
}
void gtk_menu_popup_at_rect (GtkMenu *menu, GdkSurface *rect_window, const GdkRectangle *rect,
			     GdkGravity rect_anchor, GdkGravity menu_anchor, const GdkEvent *trigger) {
	int x = 0, y = 0;
	(void) rect_window; (void) rect_anchor; (void) menu_anchor; (void) trigger; (void) rect;
	verne_ensure_menu_attach (menu, NULL);
	verne_pointer_root_xy (&x, &y);
	verne_menu_popup_now (menu, x, y, TRUE);
}
void gtk_menu_popup_at_widget (GtkMenu *menu, GtkWidget *widget, GdkGravity widget_anchor, GdkGravity menu_anchor, const GdkEvent *trigger) {
	int x = 0, y = 0;
	(void) widget_anchor; (void) menu_anchor; (void) trigger;
	verne_ensure_menu_attach (menu, widget);
	verne_widget_root_xy (widget, &x, &y);
	verne_menu_popup_now (menu, x, y, TRUE);
}
void gtk_menu_popup (GtkMenu *menu, GtkWidget *parent_menu_shell, GtkWidget *parent_menu_item, gpointer func, gpointer data, guint button, guint32 activate_time) {
	int x = 0, y = 0;
	(void) parent_menu_shell; (void) func; (void) data; (void) button; (void) activate_time;
	verne_ensure_menu_attach (menu, parent_menu_item ? parent_menu_item : parent_menu_shell);
	if (parent_menu_item)
		verne_widget_root_xy (parent_menu_item, &x, &y);
	else
		verne_pointer_root_xy (&x, &y);
	verne_menu_popup_now (menu, x, y, TRUE);
}
void gtk_menu_popdown (GtkMenu *menu) {
	GListModel *model;
	guint i, n;

	if (menu == NULL)
		return;
	if (g_object_get_data (G_OBJECT (menu), "verne-popping"))
		return;
	g_object_set_data (G_OBJECT (menu), "verne-popping", GINT_TO_POINTER (1));
	g_object_set_data (G_OBJECT (menu), "verne-menu-hold", NULL);
	g_object_set_data (G_OBJECT (menu), "verne-dismissed", GINT_TO_POINTER (1));
	g_object_set_data (G_OBJECT (menu), "verne-keep-above", NULL);
	/* Close nested children, but never the parent. GTK3 popdown of a
	 * submenu leaves File/View/dest menus visible. */
	model = gtk_window_get_toplevels ();
	n = g_list_model_get_n_items (model);
	for (i = 0; i < n; i++) {
		gpointer w = g_list_model_get_item (model, i);
		if (GTK_IS_MENU (w) && w != menu &&
		    verne_menu_is_ancestor_of (menu, GTK_MENU (w)))
			gtk_menu_popdown (GTK_MENU (w));
		if (w)
			g_object_unref (w);
	}
	verne_menu_disconnect_leaf_hooks (menu);
	verne_menu_bump_serial (menu);
	verne_menu_set_submenu_item (menu, NULL);
	verne_menu_popdown_dest_popover (menu);
	verne_menu_unembed (GTK_WIDGET (menu));
	gtk_widget_set_visible (GTK_WIDGET (menu), FALSE);
	gtk_widget_set_opacity (GTK_WIDGET (menu), 0.0);
	verne_menu_set_input_enabled (GTK_WIDGET (menu), FALSE);
	verne_x11_move (GTK_WINDOW (menu), -20000, -20000);
	verne_menu_unmap_surface (GTK_WIDGET (menu));
	verne_menu_emit_shell_signal (menu, "deactivate");
	verne_menu_emit_shell_signal (menu, "selection-done");
	g_idle_add (verne_menu_unmap_idle, g_object_ref (menu));
	g_timeout_add (50, verne_menu_unmap_idle, g_object_ref (menu));
	g_object_set_data (G_OBJECT (menu), "verne-popping", NULL);
}
void gtk_menu_attach_to_widget (GtkMenu *menu, GtkWidget *attach, gpointer detacher) {
	(void) detacher;
	verne_menu_set_attach (menu, attach);
	verne_ensure_menu_attach (menu, attach);
}
GtkWidget *gtk_menu_get_attach_widget (GtkMenu *menu) { return menu->attach; }
GtkWidget *gtk_menu_get_box (GtkMenu *menu) { return menu->box; }

GtkWidget *
gtk_menu_shell_get_selected_item (gpointer menu_shell)
{
	if (menu_shell == NULL)
		return NULL;
	return g_object_get_data (G_OBJECT (menu_shell), "verne-selected-item");
}

void gtk_menu_shell_append (gpointer menu_shell, GtkWidget *child) {
	if (GTK_IS_MENU (menu_shell))
		gtk_box_append (GTK_BOX (GTK_MENU (menu_shell)->box), child);
	else if (GTK_IS_BOX (menu_shell))
		gtk_box_append (GTK_BOX (menu_shell), child);
}
void gtk_menu_shell_prepend (gpointer menu_shell, GtkWidget *child) {
	if (GTK_IS_MENU (menu_shell))
		gtk_box_prepend (GTK_BOX (GTK_MENU (menu_shell)->box), child);
	else if (GTK_IS_BOX (menu_shell))
		gtk_box_prepend (GTK_BOX (menu_shell), child);
}
void gtk_menu_shell_insert (gpointer menu_shell, GtkWidget *child, gint position) {
	gtk_menu_shell_append (menu_shell, child);
	(void) position;
}

void
gtk_menu_shell_select_first (gpointer menu_shell, gboolean search_sensitive)
{
	GtkWidget *box = NULL;
	GtkWidget *child;

	if (menu_shell == NULL)
		return;
	if (GTK_IS_MENU (menu_shell))
		box = GTK_MENU (menu_shell)->box;
	else if (GTK_IS_WIDGET (menu_shell))
		box = GTK_WIDGET (menu_shell);
	if (box == NULL)
		return;
	for (child = gtk_widget_get_first_child (box); child; child = gtk_widget_get_next_sibling (child)) {
		if (!gtk_widget_get_visible (child))
			continue;
		if (search_sensitive && !gtk_widget_get_sensitive (child))
			continue;
		if (GTK_IS_SEPARATOR (child) || GTK_IS_SEPARATOR_MENU_ITEM (child))
			continue;
		g_object_set_data (G_OBJECT (menu_shell), "verne-selected-item", child);
		if (gtk_widget_get_focusable (child))
			gtk_widget_grab_focus (child);
		break;
	}
}

void
verne_im_multicontext_append_menuitems (gpointer context, gpointer menushell)
{
	GtkWidget *item;
	const gchar *module;

	(void) context;
	if (menushell == NULL)
		return;
	module = g_getenv ("GTK_IM_MODULE");
	item = gtk_check_menu_item_new_with_label (module && module[0] ? module : "Simple");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
	gtk_widget_set_sensitive (item, FALSE);
	gtk_widget_show (item);
	gtk_menu_shell_append (menushell, item);
}

typedef struct _GtkMenuBarClass { GtkBoxClass parent_class; } GtkMenuBarClass;
struct _GtkMenuBar { GtkBox parent; };
G_DEFINE_TYPE (GtkMenuBar, gtk_menu_bar, GTK_TYPE_BOX)
static void verne_menubar_visible (GObject *obj, GParamSpec *pspec, gpointer data);
static void
gtk_menu_bar_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	verne_chrome_snapshot (widget, snapshot, GTK_WIDGET_CLASS (gtk_menu_bar_parent_class));
}
static void
gtk_menu_bar_class_init (GtkMenuBarClass *c)
{
	gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (c), "menubar");
	GTK_WIDGET_CLASS (c)->snapshot = gtk_menu_bar_snapshot;
}
static void gtk_menu_bar_init (GtkMenuBar *bar) {
	gtk_orientable_set_orientation (GTK_ORIENTABLE (bar), GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_add_css_class (GTK_WIDGET (bar), "menubar");
	g_signal_connect (bar, "notify::visible", G_CALLBACK (verne_menubar_visible), NULL);
}
GtkWidget *gtk_menu_bar_new (void) { return g_object_new (GTK_TYPE_MENU_BAR, NULL); }

static void
verne_menubar_visible (GObject *obj, GParamSpec *pspec, gpointer data)
{
	(void) pspec; (void) data;
	if (!gtk_widget_get_visible (GTK_WIDGET (obj)))
		verne_menu_hide_others (NULL);
}

G_DEFINE_TYPE (GtkMenuItem, gtk_menu_item, GTK_TYPE_BUTTON)

#define VERNE_SUBMENU_HOVER_MS 200

static void
verne_menu_item_cancel_submenu_timeout (GtkWidget *item)
{
	guint id;

	if (item == NULL)
		return;
	id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (item), "verne-submenu-timeout"));
	if (id == 0)
		return;
	g_source_remove (id);
	g_object_set_data (G_OBJECT (item), "verne-submenu-timeout", NULL);
	g_object_unref (item);
}

static void
verne_menu_popdown_item_submenu (GtkMenuItem *item)
{
	if (item == NULL || item->submenu == NULL || !GTK_IS_MENU (item->submenu))
		return;
	if (gtk_widget_get_visible (item->submenu) ||
	    verne_menu_is_dest_overlay (item->submenu))
		gtk_menu_popdown (GTK_MENU (item->submenu));
}

static gboolean
verne_menu_item_submenu_timeout (gpointer data)
{
	GtkWidget *item = data;
	GtkWidget *submenu;

	g_object_set_data (G_OBJECT (item), "verne-submenu-timeout", NULL);
	submenu = GTK_IS_MENU_ITEM (item) ? gtk_menu_item_get_submenu (GTK_MENU_ITEM (item)) : NULL;
	if (submenu != NULL && gtk_widget_is_sensitive (item) && gtk_widget_get_visible (item))
		verne_menu_popup_submenu (item, submenu, FALSE);
	g_object_unref (item);
	return G_SOURCE_REMOVE;
}

static void
verne_menu_item_schedule_submenu (GtkWidget *item)
{
	guint id;

	verne_menu_item_cancel_submenu_timeout (item);
	g_object_ref (item);
	id = g_timeout_add (VERNE_SUBMENU_HOVER_MS, verne_menu_item_submenu_timeout, item);
	g_object_set_data (G_OBJECT (item), "verne-submenu-timeout", GUINT_TO_POINTER (id));
}

static void gtk_menu_item_dispose (GObject *o) {
	GtkMenuItem *item = GTK_MENU_ITEM (o);
	verne_menu_item_cancel_submenu_timeout (GTK_WIDGET (item));
	g_clear_pointer (&item->label, g_free);
	G_OBJECT_CLASS (gtk_menu_item_parent_class)->dispose (o);
}
static void gtk_menu_item_class_init (GtkMenuItemClass *c)
{
	G_OBJECT_CLASS (c)->dispose = gtk_menu_item_dispose;
	if (g_signal_lookup ("activate", GTK_TYPE_MENU_ITEM) == 0)
		g_signal_new ("activate",
			      G_TYPE_FROM_CLASS (c),
			      G_SIGNAL_RUN_FIRST,
			      0, NULL, NULL, NULL,
			      G_TYPE_NONE, 0);
}
static void
verne_menu_item_set_selected_shell (GtkWidget *item)
{
	GtkWidget *shell;

	shell = GTK_WIDGET (verne_menu_from_item (item));
	if (shell == NULL)
		shell = gtk_widget_get_ancestor (item, GTK_TYPE_MENU_BAR);
	if (shell)
		g_object_set_data (G_OBJECT (shell), "verne-selected-item", item);
}

static void
verne_menu_item_enter (GtkEventControllerMotion *motion, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *item = data;
	GtkWidget *shell;
	GtkWidget *prev;
	GtkWidget *submenu;

	(void) motion; (void) x; (void) y;

	shell = GTK_WIDGET (verne_menu_from_item (item));
	if (shell == NULL)
		shell = gtk_widget_get_ancestor (item, GTK_TYPE_MENU_BAR);
	prev = shell ? g_object_get_data (G_OBJECT (shell), "verne-selected-item") : NULL;
	/* Only swap nested submenus inside a GtkMenu. Menubar File/Edit stay
	 * click-to-open; hovering Edit must not pop down an open File menu. */
	if (GTK_IS_MENU (shell) && GTK_IS_MENU_ITEM (prev) && prev != item) {
		verne_menu_item_cancel_submenu_timeout (prev);
		verne_menu_popdown_item_submenu (GTK_MENU_ITEM (prev));
	}
	verne_menu_item_set_selected_shell (item);

	submenu = GTK_IS_MENU_ITEM (item) ? gtk_menu_item_get_submenu (GTK_MENU_ITEM (item)) : NULL;
	if (submenu != NULL && GTK_IS_MENU (shell) && gtk_widget_is_sensitive (item))
		verne_menu_item_schedule_submenu (item);
}

static void
verne_menu_item_leave (GtkEventControllerMotion *motion, gpointer data)
{
	(void) motion;
	verne_menu_item_cancel_submenu_timeout (GTK_WIDGET (data));
}

static void gtk_menu_item_init (GtkMenuItem *item) {
	GtkEventController *motion;
	gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
	gtk_widget_set_halign (GTK_WIDGET (item), GTK_ALIGN_FILL);
	motion = GTK_EVENT_CONTROLLER (gtk_event_controller_motion_new ());
	g_signal_connect (motion, "enter", G_CALLBACK (verne_menu_item_enter), item);
	g_signal_connect (motion, "leave", G_CALLBACK (verne_menu_item_leave), item);
	gtk_widget_add_controller (GTK_WIDGET (item), motion);
}
GtkWidget *gtk_menu_item_new (void) { return g_object_new (GTK_TYPE_MENU_ITEM, NULL); }
GtkWidget *gtk_menu_item_new_with_label (const gchar *label) {
	GtkWidget *w = gtk_menu_item_new ();
	gtk_menu_item_set_label (GTK_MENU_ITEM (w), label);
	return w;
}
GtkWidget *gtk_menu_item_new_with_mnemonic (const gchar *label) { return gtk_menu_item_new_with_label (label); }
static void
verne_menu_popup_submenu (GtkWidget *item, GtkWidget *submenu, gboolean toggle)
{
	GtkWidget *parent_menu;
	GtkRoot *root;
	int x = 0, y = 0;
	gboolean shown;

	if (!GTK_IS_WIDGET (item) || !GTK_IS_MENU (submenu))
		return;
	shown = gtk_widget_get_visible (submenu) &&
		g_object_get_data (G_OBJECT (submenu), "verne-dismissed") == NULL;
	if (!shown && verne_menu_is_dest_overlay (submenu) &&
	    g_object_get_data (G_OBJECT (submenu), "verne-dismissed") == NULL)
		shown = TRUE;
	g_warning ("verne: submenu open item=%s vis=%d dismissed=%p toggle=%d",
		   G_OBJECT_TYPE_NAME (item),
		   gtk_widget_get_visible (submenu),
		   g_object_get_data (G_OBJECT (submenu), "verne-dismissed"),
		   toggle);
	if (shown) {
		if (toggle)
			gtk_menu_popdown (GTK_MENU (submenu));
		return;
	}
	parent_menu = GTK_WIDGET (verne_menu_from_item (item));
	if (parent_menu)
		g_object_set_data (G_OBJECT (parent_menu), "verne-menu-hold", GINT_TO_POINTER (1));
	{
		GtkWidget *bar = gtk_widget_get_ancestor (item, GTK_TYPE_MENU_BAR);
		if (bar)
			g_object_set_data (G_OBJECT (bar), "verne-selected-item", item);
	}
	verne_widget_root_xy (item, &x, &y);
	if (gtk_widget_get_ancestor (item, GTK_TYPE_MENU_BAR) && parent_menu == NULL) {
		/* verne_widget_root_xy already returns the item's bottom-left. */
	} else {
		x += gtk_widget_get_width (item);
		y -= gtk_widget_get_height (item);
	}
	root = gtk_widget_get_root (item);
	if (GTK_IS_WINDOW (root) &&
	    (gtk_window_get_type_hint (GTK_WINDOW (root)) == GDK_WINDOW_TYPE_HINT_DESKTOP ||
	     g_object_get_data (G_OBJECT (root), "is_desktop_window") != NULL))
		gtk_menu_attach_to_widget (GTK_MENU (submenu), GTK_WIDGET (root), NULL);
	else
		gtk_menu_attach_to_widget (GTK_MENU (submenu), item, NULL);
	g_object_set_data (G_OBJECT (submenu), "verne-dismissed", NULL);
	verne_menu_set_submenu_item (GTK_MENU (submenu), item);
	if (verne_hide_others_idle_id != 0) {
		g_source_remove (verne_hide_others_idle_id);
		verne_hide_others_idle_id = 0;
	}
	verne_menu_hide_others (GTK_MENU (submenu));
	if (verne_menu_popup_dest_overlay (GTK_MENU (submenu), x, y) ||
	    verne_menu_popup_dest_popover (GTK_MENU (submenu), x, y))
		return;
	verne_menu_popup_now (GTK_MENU (submenu), x, y, TRUE);
}

static void
verne_menu_open_submenu (GtkWidget *item, GtkWidget *submenu)
{
	verne_menu_popup_submenu (item, submenu, TRUE);
}

static void
verne_submenu_clicked (GtkButton *item, gpointer data)
{
	verne_menu_open_submenu (GTK_WIDGET (item), data);
}

void gtk_menu_item_set_submenu (GtkMenuItem *item, GtkWidget *submenu)
{
	if (item->submenu == submenu)
		return;
	if (item->submenu && g_object_get_data (G_OBJECT (item), "verne-sub-hooked")) {
		g_signal_handlers_disconnect_by_func (item, G_CALLBACK (verne_submenu_clicked), item->submenu);
		g_object_set_data (G_OBJECT (item), "verne-sub-hooked", NULL);
	}
	item->submenu = submenu;
	if (submenu == NULL)
		return;
	g_signal_connect (item, "clicked", G_CALLBACK (verne_submenu_clicked), submenu);
	g_object_set_data (G_OBJECT (item), "verne-sub-hooked", GINT_TO_POINTER (1));
}
GtkWidget *gtk_menu_item_get_submenu (GtkMenuItem *item) { return item->submenu; }
void gtk_menu_item_set_label (GtkMenuItem *item, const gchar *label) {
	g_free (item->label); item->label = g_strdup (label);
	gtk_button_set_use_underline (GTK_BUTTON (item), TRUE);
	gtk_button_set_label (GTK_BUTTON (item), label);
}
const gchar *gtk_menu_item_get_label (GtkMenuItem *item) { return item->label; }

void
gtk_menu_item_set_accel_path (GtkMenuItem *item, const gchar *accel_path)
{
	GtkAccelKey key;
	gchar *accel_label = NULL, *combined;
	const gchar *base;
	GtkAction *action;

	if (item == NULL)
		return;
	g_object_set_data_full (G_OBJECT (item), "verne-accel-path",
				g_strdup (accel_path), g_free);
	memset (&key, 0, sizeof key);
	if (accel_path && accel_path[0] && gtk_accel_map_lookup_entry (accel_path, &key) && key.accel_key)
		accel_label = gtk_accelerator_get_label (key.accel_key, key.accel_mods);
	if (accel_label == NULL || accel_label[0] == '\0') {
		g_free (accel_label);
		accel_label = NULL;
		action = g_object_get_data (G_OBJECT (item), "verne-action");
		if (action && action->accelerator && action->accelerator[0]) {
			guint accel_key = 0;
			GdkModifierType accel_mods = 0;

			gtk_accelerator_parse (action->accelerator, &accel_key, &accel_mods);
			if (accel_key)
				accel_label = gtk_accelerator_get_label (accel_key, accel_mods);
		}
	}
	if (accel_label == NULL || accel_label[0] == '\0') {
		g_free (accel_label);
		return;
	}
	base = item->label ? item->label : gtk_button_get_label (GTK_BUTTON (item));
	if (base == NULL)
		base = "";
	combined = g_strdup_printf ("%s    %s", base, accel_label);
	gtk_button_set_use_underline (GTK_BUTTON (item), TRUE);
	gtk_button_set_label (GTK_BUTTON (item), combined);
	g_free (combined);
	g_free (accel_label);
}
void gtk_image_menu_item_set_image (GtkMenuItem *item, GtkWidget *image) { item->image = image; }
GtkWidget *gtk_image_menu_item_get_image (GtkMenuItem *item) { return item->image; }
GtkWidget *gtk_image_menu_item_new_with_label (const gchar *label) { return gtk_menu_item_new_with_label (label); }
GtkWidget *gtk_image_menu_item_new_from_stock (const gchar *stock_id, gpointer accel_group) {
	(void) accel_group;
	return gtk_menu_item_new_with_label (stock_id);
}
void gtk_image_menu_item_set_always_show_image (GtkMenuItem *item, gboolean always_show) { (void) item; (void) always_show; }

typedef struct _GtkSeparatorMenuItemClass { GtkMenuItemClass parent_class; } GtkSeparatorMenuItemClass;
typedef struct _GtkSeparatorMenuItem GtkSeparatorMenuItem;
struct _GtkSeparatorMenuItem { GtkMenuItem parent; };
G_DEFINE_TYPE (GtkSeparatorMenuItem, gtk_separator_menu_item, GTK_TYPE_MENU_ITEM)

static void
gtk_separator_menu_item_class_init (GtkSeparatorMenuItemClass *c)
{
	(void) c;
}

static void
gtk_separator_menu_item_init (GtkSeparatorMenuItem *item)
{
	GtkWidget *sep;

	gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
	gtk_widget_set_can_focus (GTK_WIDGET (item), FALSE);
	gtk_widget_set_can_target (GTK_WIDGET (item), FALSE);
	gtk_widget_add_css_class (GTK_WIDGET (item), "separator");
	gtk_widget_add_css_class (GTK_WIDGET (item), "flat");
	gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
	sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_hexpand (sep, TRUE);
	gtk_widget_set_valign (sep, GTK_ALIGN_CENTER);
	gtk_button_set_child (GTK_BUTTON (item), sep);
}

GtkWidget *
gtk_separator_menu_item_new (void)
{
	return g_object_new (GTK_TYPE_SEPARATOR_MENU_ITEM, NULL);
}

enum { CHECK_MENU_TOGGLED, CHECK_MENU_LAST };
static guint check_menu_signals[CHECK_MENU_LAST];

typedef struct _GtkCheckMenuItemClass {
	GtkMenuItemClass parent_class;
	void (* toggled) (GtkCheckMenuItem *check_menu_item);
} GtkCheckMenuItemClass;

struct _GtkCheckMenuItem {
	GtkMenuItem parent;
	gboolean active;
	gchar *raw_label;
};

static void gtk_check_menu_item_class_init (GtkCheckMenuItemClass *klass);
static void gtk_check_menu_item_init (GtkCheckMenuItem *item);
G_DEFINE_TYPE (GtkCheckMenuItem, gtk_check_menu_item, GTK_TYPE_MENU_ITEM)

static void
gtk_check_menu_item_sync_label (GtkCheckMenuItem *item)
{
	const gchar *raw = item->raw_label ? item->raw_label : "";
	gchar *shown = g_strdup_printf ("%s%s", item->active ? "✓  " : "     ", raw);
	gtk_menu_item_set_label (GTK_MENU_ITEM (item), shown);
	g_free (shown);
}

static void
gtk_check_menu_item_dispose (GObject *object)
{
	GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM (object);
	g_clear_pointer (&item->raw_label, g_free);
	G_OBJECT_CLASS (gtk_check_menu_item_parent_class)->dispose (object);
}

static void
gtk_check_menu_item_clicked (GtkButton *button)
{
	GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM (button);
	GtkButtonClass *parent_button;

	item->active = !item->active;
	gtk_check_menu_item_sync_label (item);
	g_signal_emit (item, check_menu_signals[CHECK_MENU_TOGGLED], 0);
	/* GTK4 GtkButtonClass->clicked is NULL; the signal is enough. */
	parent_button = GTK_BUTTON_CLASS (gtk_check_menu_item_parent_class);
	if (parent_button != NULL && parent_button->clicked != NULL)
		parent_button->clicked (button);
	{
		GtkWidget *menu = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_MENU);
		if (menu)
			verne_menu_hide_others_later ();
	}
}

static void
gtk_check_menu_item_class_init (GtkCheckMenuItemClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = gtk_check_menu_item_dispose;
	GTK_BUTTON_CLASS (klass)->clicked = gtk_check_menu_item_clicked;
	check_menu_signals[CHECK_MENU_TOGGLED] =
		g_signal_new ("toggled", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GtkCheckMenuItemClass, toggled),
			      NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gtk_check_menu_item_init (GtkCheckMenuItem *item)
{
	item->active = FALSE;
	item->raw_label = NULL;
}

GtkWidget *
gtk_check_menu_item_new_with_mnemonic (const gchar *label)
{
	GtkWidget *w = g_object_new (GTK_TYPE_CHECK_MENU_ITEM, NULL);
	GTK_CHECK_MENU_ITEM (w)->raw_label = g_strdup (label);
	gtk_check_menu_item_sync_label (GTK_CHECK_MENU_ITEM (w));
	return w;
}

void
gtk_check_menu_item_set_active (GtkCheckMenuItem *item, gboolean is_active)
{
	if (item == NULL || item->active == !!is_active)
		return;
	item->active = !!is_active;
	gtk_check_menu_item_sync_label (item);
}

gboolean
gtk_check_menu_item_get_active (GtkCheckMenuItem *item)
{
	return item ? item->active : FALSE;
}

/* images / buttons */
GtkWidget *
gtk_image_new_from_stock (const gchar *stock_id, int size)
{
	return verne_gtk_image_new_from_icon_name (stock_id, size);
}
void
gtk_image_set_from_stock (GtkImage *image, const gchar *stock_id, int size)
{
	(void) size;
	gtk_image_set_from_icon_name (image, stock_id);
}
void
gtk_button_set_image (GtkButton *button, GtkWidget *image)
{
	gtk_button_set_child (button, image);
}
GtkWidget *
gtk_button_get_image (GtkButton *button)
{
	return gtk_button_get_child (button);
}
GtkWidget *
gtk_button_new_from_stock (const gchar *stock_id)
{
	return (gtk_button_new_from_icon_name) (verne_map_icon_name (stock_id));
}

/* accel group */
typedef struct {
	guint key;
	GdkModifierType mods;
	GtkAction *action;
	GtkAccelGroup *group;
} VerneAccelEntry;

typedef struct _GtkAccelGroupClass { GObjectClass parent_class; } GtkAccelGroupClass;
struct _GtkAccelGroup {
	GObject parent;
	GPtrArray *entries;
};

static void
verne_accel_entry_on_action_gone (gpointer data, GObject *dead)
{
	GtkAccelGroup *group = data;
	guint i;

	if (group == NULL || group->entries == NULL)
		return;
	for (i = group->entries->len; i-- > 0; ) {
		VerneAccelEntry *e = g_ptr_array_index (group->entries, i);
		if ((GObject *) e->action == dead) {
			e->action = NULL;
			g_ptr_array_remove_index (group->entries, i);
		}
	}
}

static void
verne_accel_entry_free (gpointer data)
{
	VerneAccelEntry *e = data;

	if (e->action) {
		g_object_weak_unref (G_OBJECT (e->action), verne_accel_entry_on_action_gone, e->group);
		e->action = NULL;
	}
	g_free (e);
}

static void
gtk_accel_group_finalize (GObject *object)
{
	GtkAccelGroup *g = (GtkAccelGroup *) object;
	g_clear_pointer (&g->entries, g_ptr_array_unref);
	G_OBJECT_CLASS (g_type_class_peek_parent (G_OBJECT_GET_CLASS (object)))->finalize (object);
}

G_DEFINE_TYPE (GtkAccelGroup, gtk_accel_group, G_TYPE_OBJECT)
static void gtk_accel_group_class_init (GtkAccelGroupClass *c)
{
	G_OBJECT_CLASS (c)->finalize = gtk_accel_group_finalize;
}
static void gtk_accel_group_init (GtkAccelGroup *g)
{
	g->entries = g_ptr_array_new_with_free_func (verne_accel_entry_free);
}
GtkAccelGroup *gtk_accel_group_new (void) { return g_object_new (gtk_accel_group_get_type (), NULL); }

void
verne_accel_group_connect_action (GtkAccelGroup *group, GtkAction *action, const gchar *accelerator)
{
	VerneAccelEntry *e;
	guint key = 0;
	GdkModifierType mods = 0;
	guint i;

	if (group == NULL || action == NULL || accelerator == NULL || accelerator[0] == '\0')
		return;
	gtk_accelerator_parse (accelerator, &key, &mods);
	if (key == 0)
		return;
	key = gdk_keyval_to_lower (key);
	for (i = 0; i < group->entries->len; i++) {
		VerneAccelEntry *cur = g_ptr_array_index (group->entries, i);
		if (cur->action == action && cur->key == key && cur->mods == mods)
			return;
	}
	e = g_new0 (VerneAccelEntry, 1);
	e->key = key;
	e->mods = mods;
	e->action = action;
	e->group = group;
	g_object_weak_ref (G_OBJECT (action), verne_accel_entry_on_action_gone, group);
	g_ptr_array_add (group->entries, e);
}

void
verne_accel_group_disconnect_action (GtkAccelGroup *group, GtkAction *action)
{
	guint i;

	if (group == NULL || group->entries == NULL || action == NULL)
		return;
	for (i = group->entries->len; i-- > 0; ) {
		VerneAccelEntry *e = g_ptr_array_index (group->entries, i);
		if (e->action == action)
			g_ptr_array_remove_index (group->entries, i);
	}
}

static gboolean
verne_widget_really_showing (GtkWidget *w)
{
	return w != NULL &&
	       gtk_widget_get_mapped (w) &&
	       gtk_widget_get_visible (w) &&
	       gtk_widget_get_width (w) > 2 &&
	       gtk_widget_get_height (w) > 2;
}

static gboolean
verne_editable_wants_key (GtkWidget *focus, guint key, GdkModifierType mods)
{
	if (focus == NULL || !GTK_IS_EDITABLE (focus))
		return FALSE;
	/* Function keys, Escape, and Menu are window/view accelerators (F2 rename, F3 split). */
	if (key == GDK_KEY_Escape || key == GDK_KEY_Menu || key == GDK_KEY_F10)
		return FALSE;
	if ((key >= GDK_KEY_F1 && key <= GDK_KEY_F35) ||
	    (key >= GDK_KEY_KP_F1 && key <= GDK_KEY_KP_F4))
		return FALSE;
	if (!(mods & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK)))
		return TRUE;
	if ((mods & GDK_CONTROL_MASK) && !(mods & GDK_ALT_MASK)) {
		/* File-manager Undo/Redo must run unless this editable is an
		 * in-place editor (rename cell, search, canvas label). A
		 * leftover location-bar GtkEntry would otherwise swallow
		 * Ctrl+Z as a no-op text undo. */
		if (key == GDK_KEY_z || key == GDK_KEY_Z ||
		    key == GDK_KEY_y || key == GDK_KEY_Y) {
			GtkWidget *w;

			/* GTK4 GtkEntry focuses an inner GtkText. Hidden search
			 * (NemoQueryEditor) can keep that GtkText as focus.
			 * Only swallow Undo inside a visible editor. */
			if (!verne_widget_really_showing (focus))
				return FALSE;
			for (w = focus; w != NULL; w = gtk_widget_get_parent (w)) {
				const gchar *tn = G_OBJECT_TYPE_NAME (w);
				if (!verne_widget_really_showing (w))
					continue;
				if (GTK_IS_SEARCH_ENTRY (w) || GTK_IS_TEXT_VIEW (w) ||
				    GTK_IS_TREE_VIEW (w))
					return TRUE;
				if (tn != NULL &&
				    (strstr (tn, "IconContainer") != NULL ||
				     strstr (tn, "EelCanvas") != NULL ||
				     strstr (tn, "EditableLabel") != NULL ||
				     strstr (tn, "QueryEditor") != NULL))
					return TRUE;
			}
			return FALSE;
		}
		switch (key) {
		case GDK_KEY_a:
		case GDK_KEY_A:
		case GDK_KEY_c:
		case GDK_KEY_C:
		case GDK_KEY_v:
		case GDK_KEY_V:
		case GDK_KEY_x:
		case GDK_KEY_X:
		case GDK_KEY_Left:
		case GDK_KEY_Right:
		case GDK_KEY_Home:
		case GDK_KEY_End:
		case GDK_KEY_BackSpace:
		case GDK_KEY_Delete:
		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
			return TRUE;
		default:
			break;
		}
	}
	return FALSE;
}

static gboolean
verne_accel_group_activate (GtkAccelGroup *group, guint key, GdkModifierType mods)
{
	gint i;

	if (group == NULL || group->entries == NULL)
		return FALSE;
	for (i = (gint) group->entries->len - 1; i >= 0; i--) {
		VerneAccelEntry *e = g_ptr_array_index (group->entries, i);
		if (e->action == NULL || e->key != key || e->mods != mods)
			continue;
		if (!gtk_action_get_sensitive (e->action) || !gtk_action_get_visible (e->action))
			continue;
		gtk_action_activate (e->action);
		return TRUE;
	}
	return FALSE;
}

static gboolean
verne_accel_key_pressed (GtkEventControllerKey *self, guint keyval, guint keycode,
			 GdkModifierType state, gpointer data)
{
	GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
	GtkWidget *focus;
	GSList *groups = g_object_get_data (G_OBJECT (widget), "verne-accel-groups");
	guint key = gdk_keyval_to_lower (keyval);
	GdkModifierType mods = state & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SHIFT_MASK | GDK_SUPER_MASK);

	(void) keycode;
	(void) data;
	focus = widget;
	if (focus) {
		GtkRoot *root = gtk_widget_get_root (focus);
		focus = root ? gtk_root_get_focus (root) : NULL;
	}
	/* GTK4 focuses GtkText inside GtkEntry, so QueryEditor/LocationBar
	 * GtkBindingSet shortcuts never see Escape. Emit their cancel
	 * signals when a visible ancestor owns that key. */
	if (keyval == GDK_KEY_Escape && (mods & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK)) == 0) {
		GtkWidget *w;

		if (verne_any_menu_visible ()) {
			verne_menu_hide_others (NULL);
			return TRUE;
		}
		for (w = focus; w != NULL; w = gtk_widget_get_parent (w)) {
			const gchar *tn = G_OBJECT_TYPE_NAME (w);

			if (!verne_widget_really_showing (w) || tn == NULL)
				continue;
			if (strstr (tn, "QueryEditor") != NULL ||
			    strstr (tn, "LocationBar") != NULL) {
				if (g_signal_lookup ("cancel", G_OBJECT_TYPE (w)) != 0)
					g_signal_emit_by_name (w, "cancel");
				return TRUE;
			}
		}
	}
	if (verne_editable_wants_key (focus, keyval, mods))
		return FALSE;
	for (; groups != NULL; groups = groups->next) {
		if (verne_accel_group_activate (groups->data, key, mods))
			return TRUE;
	}
	return FALSE;
}

void gtk_window_add_accel_group (GtkWindow *window, GtkAccelGroup *accel)
{
	GtkEventController *controller;
	GSList *groups;

	if (window == NULL || accel == NULL)
		return;
	groups = g_object_get_data (G_OBJECT (window), "verne-accel-groups");
	if (g_slist_find (groups, accel) == NULL) {
		groups = g_slist_append (groups, accel);
		g_object_set_data (G_OBJECT (window), "verne-accel-groups", groups);
	}
	if (g_object_get_data (G_OBJECT (window), "verne-accel-controller"))
		return;
	controller = GTK_EVENT_CONTROLLER (gtk_event_controller_key_new ());
	gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_CAPTURE);
	g_signal_connect (controller, "key-pressed", G_CALLBACK (verne_accel_key_pressed), window);
	gtk_widget_add_controller (GTK_WIDGET (window), controller);
	g_object_set_data (G_OBJECT (window), "verne-accel-controller", controller);
}
void gtk_window_remove_accel_group (GtkWindow *window, GtkAccelGroup *accel)
{
	GSList *groups;

	if (window == NULL || accel == NULL)
		return;
	groups = g_object_get_data (G_OBJECT (window), "verne-accel-groups");
	groups = g_slist_remove (groups, accel);
	g_object_set_data (G_OBJECT (window), "verne-accel-groups", groups);
}
void gtk_widget_add_accelerator (GtkWidget *widget, const gchar *accel_signal, GtkAccelGroup *accel_group,
				 guint accel_key, GdkModifierType accel_mods, GtkAccelFlags accel_flags)
{
	GtkAction *action;
	gchar *name;

	(void) accel_signal;
	(void) accel_flags;
	if (widget == NULL || accel_group == NULL || accel_key == 0)
		return;
	name = g_strdup_printf ("verne-widget-accel-%p", widget);
	action = gtk_action_new (name, NULL, NULL, NULL);
	g_free (name);
	g_free (action->accelerator);
	action->accelerator = gtk_accelerator_name (accel_key, accel_mods);
	g_signal_connect_swapped (action, "activate", G_CALLBACK (gtk_widget_activate), widget);
	verne_accel_group_connect_action (accel_group, action, action->accelerator);
	g_object_set_data_full (G_OBJECT (widget), "verne-widget-accel-action", action, g_object_unref);
}

GtkIconTheme *
gtk_icon_theme_get_for_screen (GdkDisplay *screen)
{
	return gtk_icon_theme_get_for_display (screen ? screen : gdk_display_get_default ());
}

GdkPixbuf *
gtk_icon_theme_load_icon (GtkIconTheme *theme, const gchar *name, gint size, GtkIconLookupFlags flags, GError **error)
{
	GtkIconPaintable *p;
	GdkPixbuf *pixbuf;
	(void) flags;
	p = gtk_icon_theme_lookup_icon_for_scale (theme, name, size, 1, flags);
	pixbuf = gtk_icon_info_load_icon (p, error);
	if (p)
		g_object_unref (p);
	return pixbuf;
}

G_DEFINE_TYPE (VerneScrolledWindow, verne_scrolled_window, GTK_TYPE_BOX)

static void
verne_scrolled_window_class_init (VerneScrolledWindowClass *klass)
{
	klass->scrollbar_spacing = 0;
}

static void
verne_scrolled_window_init (VerneScrolledWindow *sw)
{
	gtk_widget_set_hexpand (GTK_WIDGET (sw), TRUE);
	gtk_widget_set_vexpand (GTK_WIDGET (sw), TRUE);
	gtk_widget_set_halign (GTK_WIDGET (sw), GTK_ALIGN_FILL);
	gtk_widget_set_valign (GTK_WIDGET (sw), GTK_ALIGN_FILL);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (sw), GTK_ORIENTATION_VERTICAL);
	sw->inner = (gtk_scrolled_window_new) ();
	gtk_widget_set_hexpand (sw->inner, TRUE);
	gtk_widget_set_vexpand (sw->inner, TRUE);
	gtk_widget_set_halign (sw->inner, GTK_ALIGN_FILL);
	gtk_widget_set_valign (sw->inner, GTK_ALIGN_FILL);
	gtk_widget_add_css_class (sw->inner, "view");
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw->inner),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_append (GTK_BOX (sw), sw->inner);
}

GtkWidget *
verne_scrolled_window_get_inner (gpointer widget)
{
	if (widget != NULL && VERNE_IS_SCROLLED_WINDOW (widget))
		return VERNE_SCROLLED_WINDOW (widget)->inner;
	return widget;
}

G_DEFINE_TYPE (VerneInfoBar, verne_info_bar, GTK_TYPE_BOX)

enum {
	VERNE_INFO_BAR_RESPONSE,
	VERNE_INFO_BAR_N_SIGNALS
};

static guint verne_info_bar_signals[VERNE_INFO_BAR_N_SIGNALS];

static void
verne_info_bar_inner_response (GtkInfoBar *inner,
			       gint response_id,
			       VerneInfoBar *bar)
{
	(void) inner;
	g_signal_emit (bar, verne_info_bar_signals[VERNE_INFO_BAR_RESPONSE], 0, response_id);
}

static void
verne_info_bar_class_init (VerneInfoBarClass *klass)
{
	verne_info_bar_signals[VERNE_INFO_BAR_RESPONSE] =
		g_signal_new ("response",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, NULL,
			      G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
verne_info_bar_init (VerneInfoBar *bar)
{
	gtk_orientable_set_orientation (GTK_ORIENTABLE (bar), GTK_ORIENTATION_VERTICAL);
	/* Parentheses bypass gtk_info_bar_new() → verne_info_bar_new(). */
	bar->inner = (gtk_info_bar_new) ();
	bar->content_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	bar->action_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_hexpand (bar->content_area, TRUE);
	/* GTK4 only accepts GtkActionable widgets in the action area, so keep
	 * both custom boxes as content children. add_button() still targets
	 * the native action area via verne_to_gtk_ib(). */
	(gtk_info_bar_add_child) ((GtkInfoBar *) bar->inner, bar->content_area);
	(gtk_info_bar_add_child) ((GtkInfoBar *) bar->inner, bar->action_area);
	gtk_widget_set_hexpand (bar->inner, TRUE);
	gtk_box_append (GTK_BOX (bar), bar->inner);
	g_signal_connect (bar->inner, "response",
			  G_CALLBACK (verne_info_bar_inner_response), bar);
}

GtkWidget *
verne_info_bar_new (void)
{
	return g_object_new (VERNE_TYPE_INFO_BAR, NULL);
}

GtkWidget *
verne_info_bar_get_inner (gpointer widget)
{
	if (widget != NULL && VERNE_IS_INFO_BAR (widget))
		return VERNE_INFO_BAR (widget)->inner;
	return widget;
}
