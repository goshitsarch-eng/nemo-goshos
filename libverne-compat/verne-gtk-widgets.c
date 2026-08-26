#include "config.h"
#include "verne-gtk-compat.h"
#include <graphene.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
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
gtk_container_class_init (GtkContainerClass *klass)
{
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
	GtkWidget *w = GTK_WIDGET (misc);
	if (GTK_IS_LABEL (w)) {
		gtk_label_set_xalign (GTK_LABEL (w), xalign);
		gtk_label_set_yalign (GTK_LABEL (w), yalign);
		return;
	}
	misc->xalign = xalign;
	misc->yalign = yalign;
	gtk_widget_set_halign (w, xalign < 0.33 ? GTK_ALIGN_START : (xalign > 0.66 ? GTK_ALIGN_END : GTK_ALIGN_CENTER));
	gtk_widget_set_valign (w, yalign < 0.33 ? GTK_ALIGN_START : (yalign > 0.66 ? GTK_ALIGN_END : GTK_ALIGN_CENTER));
}
void gtk_misc_get_alignment (GtkMisc *misc, gfloat *xalign, gfloat *yalign) {
	if (xalign) *xalign = misc->xalign;
	if (yalign) *yalign = misc->yalign;
}
void gtk_misc_set_padding (GtkMisc *misc, gint xpad, gint ypad) {
	misc->xpad = xpad; misc->ypad = ypad;
	gtk_widget_set_margin_start (GTK_WIDGET (misc), xpad);
	gtk_widget_set_margin_end (GTK_WIDGET (misc), xpad);
	gtk_widget_set_margin_top (GTK_WIDGET (misc), ypad);
	gtk_widget_set_margin_bottom (GTK_WIDGET (misc), ypad);
}
void gtk_misc_get_padding (GtkMisc *misc, gint *xpad, gint *ypad) {
	if (xpad) *xpad = misc->xpad;
	if (ypad) *ypad = misc->ypad;
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
	(void) baseline; (void) width; (void) height;
	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
		ChildPos *pos = g_hash_table_lookup (priv->child_pos, child);
		int cw = gtk_widget_get_width (child);
		int ch = gtk_widget_get_height (child);
		int x = pos ? pos->x : 0;
		int y = pos ? pos->y : 0;
		GskTransform *t = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y));
		if (cw < 1) cw = 1;
		if (ch < 1) ch = 1;
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
	gtk_widget_set_visible (GTK_WIDGET (window), FALSE);
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

static void
verne_menu_mapped (GtkWidget *w, gpointer data)
{
	GdkSurface *s;
	(void) data;
	s = gtk_native_get_surface (GTK_NATIVE (w));
#ifdef GDK_WINDOWING_X11
	if (s && GDK_IS_X11_SURFACE (s)) {
		gdk_x11_surface_set_skip_taskbar_hint (s, TRUE);
		gdk_x11_surface_set_skip_pager_hint (s, TRUE);
	}
#endif
	if (g_object_get_data (G_OBJECT (w), "verne-popup-pos") == NULL)
		return;
	verne_x11_move (GTK_WINDOW (w),
			GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "verne-popup-x")),
			GPOINTER_TO_INT (g_object_get_data (G_OBJECT (w), "verne-popup-y")));
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
	if (!gtk_window_is_active (GTK_WINDOW (menu)))
		gtk_menu_popdown (menu);
}

static gboolean
verne_menu_watch_focus (gpointer data)
{
	GtkWidget *w = data;
	if (GTK_IS_MENU (w) && gtk_widget_get_visible (w))
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
verne_ensure_menu_attach (GtkMenu *menu, GtkWidget *fallback)
{
	GtkWidget *attach = menu->attach ? menu->attach : fallback;
	GtkRoot *root;

	if (attach)
		menu->attach = attach;
	if (menu->attach == NULL)
		return;
	root = gtk_widget_get_root (menu->attach);
	if (GTK_IS_WINDOW (root) && !GTK_IS_MENU (root))
		gtk_window_set_transient_for (GTK_WINDOW (menu), GTK_WINDOW (root));
	else if (GTK_IS_MENU (root))
		gtk_window_set_transient_for (GTK_WINDOW (menu), GTK_WINDOW (root));
}

static void gtk_menu_class_init (GtkMenuClass *c) { (void) c; }
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
	gtk_widget_add_css_class (GTK_WIDGET (menu), "popup");
	gtk_widget_add_css_class (GTK_WIDGET (menu), "menu");
	menu->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class (menu->box, "menu");
	gtk_window_set_child (GTK_WINDOW (menu), menu->box);
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
verne_menu_hook_leaf_items (GtkMenu *menu)
{
	GtkWidget *ch;
	GtkWidget *box = gtk_menu_get_box (menu);
	for (ch = box ? gtk_widget_get_first_child (box) : NULL; ch; ch = gtk_widget_get_next_sibling (ch)) {
		if (g_object_get_data (G_OBJECT (ch), "verne-leaf-hooked"))
			continue;
		if (GTK_IS_MENU_ITEM (ch) && gtk_menu_item_get_submenu (GTK_MENU_ITEM (ch)))
			continue;
		if (GTK_IS_BUTTON (ch) || GTK_IS_CHECK_BUTTON (ch)) {
			g_signal_connect_swapped (ch, "clicked", G_CALLBACK (gtk_menu_popdown), menu);
			g_object_set_data (G_OBJECT (ch), "verne-leaf-hooked", GINT_TO_POINTER (1));
		}
	}
}

typedef struct {
	GtkMenu *menu;
	int x, y;
	gboolean has_pos;
} VernePopupData;

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

static void
verne_menu_hide_others (GtkMenu *keep)
{
	GListModel *model = gtk_window_get_toplevels ();
	guint i, n = g_list_model_get_n_items (model);
	for (i = 0; i < n; i++) {
		gpointer w = g_list_model_get_item (model, i);
		if (GTK_IS_MENU (w) && w != keep)
			gtk_menu_popdown (GTK_MENU (w));
		if (w)
			g_object_unref (w);
	}
}

static void
verne_toplevel_dismiss_menus (GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *toplevel;
	GtkWidget *picked;

	(void) n_press; (void) data;
	/* Never claim the sequence: this is a click-outside dismiss, not a grab. */
	gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);

	toplevel = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
	if (toplevel == NULL)
		return;
	picked = gtk_widget_pick (toplevel, x, y, GTK_PICK_DEFAULT);
	if (picked != NULL &&
	    (GTK_IS_MENU_ITEM (picked) || GTK_IS_MENU_BAR (picked) ||
	     GTK_IS_NOTEBOOK (picked) ||
	     gtk_widget_get_ancestor (picked, GTK_TYPE_MENU_ITEM) != NULL ||
	     gtk_widget_get_ancestor (picked, GTK_TYPE_MENU_BAR) != NULL ||
	     gtk_widget_get_ancestor (picked, GTK_TYPE_NOTEBOOK) != NULL))
		return;
	verne_menu_hide_others (NULL);
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
		if (GTK_IS_MENU (w) && gtk_widget_get_visible (GTK_WIDGET (w)) &&
		    g_object_get_data (w, "verne-dismissed") == NULL)
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

static gboolean
verne_menu_popup_idle (gpointer data)
{
	VernePopupData *p = data;
	GtkWidget *w = GTK_WIDGET (p->menu);
	GtkWidget *box;
	int nat_w = 180, nat_h = 32;

	if (!GTK_IS_MENU (p->menu)) {
		g_object_unref (p->menu);
		g_free (p);
		return G_SOURCE_REMOVE;
	}
	if (g_object_get_data (G_OBJECT (p->menu), "verne-dismissed")) {
		g_object_unref (p->menu);
		g_free (p);
		return G_SOURCE_REMOVE;
	}
	verne_ensure_menu_attach (p->menu, NULL);
	box = gtk_menu_get_box (p->menu);
	verne_menu_hook_leaf_items (p->menu);
	if (box) {
		gtk_widget_measure (box, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &nat_w, NULL, NULL);
		gtk_widget_measure (box, GTK_ORIENTATION_VERTICAL, nat_w, NULL, &nat_h, NULL, NULL);
	}
	if (nat_w < 180)
		nat_w = 180;
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
	verne_menu_hide_others (p->menu);
	gtk_widget_set_opacity (w, 1.0);
	gtk_widget_set_visible (w, TRUE);
	gtk_window_present (GTK_WINDOW (p->menu));
	gtk_widget_grab_focus (w);
	if (p->has_pos)
		verne_x11_move (GTK_WINDOW (p->menu), p->x, p->y);
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
	g_object_set_data (G_OBJECT (menu), "verne-dismissed", NULL);
	g_idle_add (verne_menu_popup_idle, p);
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
	GtkWidget *attach;
	if (menu == NULL)
		return;
	if (g_object_get_data (G_OBJECT (menu), "verne-popping"))
		return;
	g_object_set_data (G_OBJECT (menu), "verne-popping", GINT_TO_POINTER (1));
	g_object_set_data (G_OBJECT (menu), "verne-menu-hold", NULL);
	g_object_set_data (G_OBJECT (menu), "verne-dismissed", GINT_TO_POINTER (1));
	gtk_widget_set_visible (GTK_WIDGET (menu), FALSE);
	gtk_widget_set_opacity (GTK_WIDGET (menu), 0.0);
	verne_menu_unmap_surface (GTK_WIDGET (menu));
	attach = menu->attach;
	if (attach) {
		GtkWidget *parent_menu = gtk_widget_get_ancestor (attach, GTK_TYPE_MENU);
		if (parent_menu && parent_menu != GTK_WIDGET (menu))
			gtk_menu_popdown (GTK_MENU (parent_menu));
	}
	g_idle_add (verne_menu_unmap_idle, g_object_ref (menu));
	g_timeout_add (50, verne_menu_unmap_idle, g_object_ref (menu));
	g_object_set_data (G_OBJECT (menu), "verne-popping", NULL);
}
void gtk_menu_attach_to_widget (GtkMenu *menu, GtkWidget *attach, gpointer detacher) {
	(void) detacher;
	menu->attach = attach;
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
static void gtk_menu_item_dispose (GObject *o) {
	GtkMenuItem *item = GTK_MENU_ITEM (o);
	g_clear_pointer (&item->label, g_free);
	G_OBJECT_CLASS (gtk_menu_item_parent_class)->dispose (o);
}
static void gtk_menu_item_class_init (GtkMenuItemClass *c) { G_OBJECT_CLASS (c)->dispose = gtk_menu_item_dispose; }
static void
verne_menu_item_set_selected_shell (GtkWidget *item)
{
	GtkWidget *shell;

	shell = gtk_widget_get_ancestor (item, GTK_TYPE_MENU);
	if (shell == NULL)
		shell = gtk_widget_get_ancestor (item, GTK_TYPE_MENU_BAR);
	if (shell)
		g_object_set_data (G_OBJECT (shell), "verne-selected-item", item);
}

static void
verne_menu_item_enter (GtkEventControllerMotion *motion, gdouble x, gdouble y, gpointer data)
{
	(void) motion; (void) x; (void) y;
	verne_menu_item_set_selected_shell (GTK_WIDGET (data));
}

static void gtk_menu_item_init (GtkMenuItem *item) {
	GtkEventController *motion;
	gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
	gtk_widget_set_halign (GTK_WIDGET (item), GTK_ALIGN_FILL);
	motion = GTK_EVENT_CONTROLLER (gtk_event_controller_motion_new ());
	g_signal_connect (motion, "enter", G_CALLBACK (verne_menu_item_enter), item);
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
verne_submenu_clicked (GtkButton *item, gpointer data)
{
	GtkWidget *submenu = data;
	GtkWidget *parent_menu;
	int x = 0, y = 0;

	if (!GTK_IS_MENU (submenu))
		return;
	if (gtk_widget_get_visible (submenu) &&
	    g_object_get_data (G_OBJECT (submenu), "verne-dismissed") == NULL) {
		gtk_menu_popdown (GTK_MENU (submenu));
		return;
	}
	parent_menu = gtk_widget_get_ancestor (GTK_WIDGET (item), GTK_TYPE_MENU);
	if (parent_menu)
		g_object_set_data (G_OBJECT (parent_menu), "verne-menu-hold", GINT_TO_POINTER (1));
	{
		GtkWidget *bar = gtk_widget_get_ancestor (GTK_WIDGET (item), GTK_TYPE_MENU_BAR);
		if (bar)
			g_object_set_data (G_OBJECT (bar), "verne-selected-item", item);
	}
	gtk_menu_attach_to_widget (GTK_MENU (submenu), GTK_WIDGET (item), NULL);
	verne_widget_root_xy (GTK_WIDGET (item), &x, &y);
	/* Menubar items drop down; nested items open to the right. */
	if (gtk_widget_get_ancestor (GTK_WIDGET (item), GTK_TYPE_MENU_BAR) && parent_menu == NULL) {
		/* verne_widget_root_xy already returns the item's bottom-left. */
	} else {
		x += gtk_widget_get_width (GTK_WIDGET (item));
		y -= gtk_widget_get_height (GTK_WIDGET (item));
	}
	verne_menu_popup_now (GTK_MENU (submenu), x, y, TRUE);
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
void gtk_image_menu_item_set_image (GtkMenuItem *item, GtkWidget *image) { item->image = image; }
GtkWidget *gtk_image_menu_item_get_image (GtkMenuItem *item) { return item->image; }
GtkWidget *gtk_image_menu_item_new_with_label (const gchar *label) { return gtk_menu_item_new_with_label (label); }
GtkWidget *gtk_image_menu_item_new_from_stock (const gchar *stock_id, gpointer accel_group) {
	(void) accel_group;
	return gtk_menu_item_new_with_label (stock_id);
}
void gtk_image_menu_item_set_always_show_image (GtkMenuItem *item, gboolean always_show) { (void) item; (void) always_show; }

GType gtk_separator_menu_item_get_type (void) { return GTK_TYPE_SEPARATOR; }
GtkWidget *gtk_separator_menu_item_new (void) { return gtk_separator_new (GTK_ORIENTATION_HORIZONTAL); }

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
			gtk_menu_popdown (GTK_MENU (menu));
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
verne_editable_wants_key (GtkWidget *focus, guint key, GdkModifierType mods)
{
	if (focus == NULL || !GTK_IS_EDITABLE (focus))
		return FALSE;
	if (!(mods & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK)))
		return TRUE;
	if ((mods & GDK_CONTROL_MASK) && !(mods & GDK_ALT_MASK)) {
		switch (key) {
		case GDK_KEY_a:
		case GDK_KEY_A:
		case GDK_KEY_c:
		case GDK_KEY_C:
		case GDK_KEY_v:
		case GDK_KEY_V:
		case GDK_KEY_x:
		case GDK_KEY_X:
		case GDK_KEY_z:
		case GDK_KEY_Z:
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
	(void) widget; (void) accel_signal; (void) accel_group; (void) accel_key; (void) accel_mods; (void) accel_flags;
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
