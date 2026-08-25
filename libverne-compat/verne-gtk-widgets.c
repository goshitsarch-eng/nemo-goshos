#include "config.h"
#include "verne-gtk-compat.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <graphene.h>

/* ---------- GtkContainer ---------- */
G_DEFINE_TYPE (GtkContainer, gtk_container, GTK_TYPE_WIDGET)

static void
gtk_container_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	GtkWidget *child;
	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child))
		gtk_widget_snapshot_child (widget, child, snapshot);
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
	if (bin->child)
		gtk_widget_snapshot_child (widget, bin->child, snapshot);
}

static void
gtk_bin_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
	GtkBin *bin = GTK_BIN (widget);
	(void) baseline;
	if (bin->child)
		gtk_widget_allocate (bin->child, width, height, -1, NULL);
}

static void
gtk_bin_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
		 int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
	GtkBin *bin = GTK_BIN (widget);
	if (bin->child)
		gtk_widget_measure (bin->child, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
	else {
		*minimum = *natural = 0;
		if (minimum_baseline) *minimum_baseline = -1;
		if (natural_baseline) *natural_baseline = -1;
	}
}

static void
gtk_bin_class_init (GtkBinClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = gtk_bin_dispose;
	GTK_CONTAINER_CLASS (klass)->add = gtk_bin_add;
	GTK_CONTAINER_CLASS (klass)->remove = gtk_bin_remove;
}
static void gtk_bin_init (GtkBin *bin) { bin->child = NULL; gtk_widget_set_layout_manager (GTK_WIDGET (bin), gtk_bin_layout_new ()); }

GtkWidget *
gtk_bin_get_child (GtkBin *bin)
{
	return bin ? bin->child : NULL;
}

/* ---------- GtkEventBox : GtkBin ---------- */
typedef struct _GtkEventBoxClass { GtkBinClass parent_class; } GtkEventBoxClass;
struct _GtkEventBox { GtkBin parent; };
G_DEFINE_TYPE (GtkEventBox, gtk_event_box, GTK_TYPE_BIN)
static void gtk_event_box_class_init (GtkEventBoxClass *c) { (void) c; }
static void gtk_event_box_init (GtkEventBox *b) { (void) b; }
GtkWidget *gtk_event_box_new (void) { return g_object_new (GTK_TYPE_EVENT_BOX, NULL); }
void gtk_event_box_set_visible_window (GtkEventBox *box, gboolean visible) { (void) box; (void) visible; }
void gtk_event_box_set_above_child (GtkEventBox *box, gboolean above) { (void) box; (void) above; }

/* ---------- GtkMisc ---------- */
G_DEFINE_TYPE (GtkMisc, gtk_misc, GTK_TYPE_WIDGET)
static void gtk_misc_class_init (GtkMiscClass *c) { (void) c; }
static void gtk_misc_init (GtkMisc *m) { m->xalign = 0.5; m->yalign = 0.5; }
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
gtk_layout_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
	GtkWidget *child;
	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child))
		gtk_widget_snapshot_child (widget, child, snapshot);
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
static void gtk_statusbar_class_init (GtkStatusbarClass *c) { (void) c; }
static void gtk_statusbar_init (GtkStatusbar *bar) {
	bar->label = GTK_LABEL (gtk_label_new (NULL));
	gtk_widget_set_hexpand (GTK_WIDGET (bar->label), TRUE);
	gtk_label_set_xalign (bar->label, 0);
	gtk_box_append (GTK_BOX (bar), GTK_WIDGET (bar->label));
	bar->next_id = 1;
	bar->stacks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) g_ptr_array_unref);
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
static void gtk_toolbar_class_init (GtkToolbarClass *c) { (void) c; }
static void gtk_toolbar_init (GtkToolbar *t) {
	gtk_orientable_set_orientation (GTK_ORIENTABLE (t), GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_add_css_class (GTK_WIDGET (t), "toolbar");
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
typedef struct _GtkMenuClass { GtkPopoverClass parent_class; } GtkMenuClass;
struct _GtkMenu { GtkPopover parent; GtkWidget *box; GtkWidget *attach; };
G_DEFINE_TYPE (GtkMenu, gtk_menu, GTK_TYPE_POPOVER)
static void gtk_menu_class_init (GtkMenuClass *c) { (void) c; }
static void gtk_menu_init (GtkMenu *menu) {
	menu->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class (menu->box, "menu");
	gtk_popover_set_child (GTK_POPOVER (menu), menu->box);
	gtk_popover_set_has_arrow (GTK_POPOVER (menu), FALSE);
	gtk_popover_set_autohide (GTK_POPOVER (menu), TRUE);
}
GtkWidget *gtk_menu_new (void) { return g_object_new (GTK_TYPE_MENU, NULL); }
void gtk_menu_popup_at_pointer (GtkMenu *menu, const GdkEvent *trigger) {
	(void) trigger;
	gtk_popover_popup (GTK_POPOVER (menu));
}
void gtk_menu_popup_at_rect (GtkMenu *menu, GdkSurface *rect_window, const GdkRectangle *rect,
			     GdkGravity rect_anchor, GdkGravity menu_anchor, const GdkEvent *trigger) {
	(void) rect_window; (void) rect; (void) rect_anchor; (void) menu_anchor; (void) trigger;
	gtk_popover_popup (GTK_POPOVER (menu));
}
void gtk_menu_popup_at_widget (GtkMenu *menu, GtkWidget *widget, GdkGravity widget_anchor, GdkGravity menu_anchor, const GdkEvent *trigger) {
	(void) widget_anchor; (void) menu_anchor; (void) trigger;
	if (gtk_widget_get_parent (GTK_WIDGET (menu)) == NULL)
		gtk_widget_set_parent (GTK_WIDGET (menu), widget);
	gtk_popover_popup (GTK_POPOVER (menu));
}
void gtk_menu_popup (GtkMenu *menu, GtkWidget *parent_menu_shell, GtkWidget *parent_menu_item, gpointer func, gpointer data, guint button, guint32 activate_time) {
	(void) parent_menu_shell; (void) parent_menu_item; (void) func; (void) data; (void) button; (void) activate_time;
	gtk_popover_popup (GTK_POPOVER (menu));
}
void gtk_menu_popdown (GtkMenu *menu) { gtk_popover_popdown (GTK_POPOVER (menu)); }
void gtk_menu_attach_to_widget (GtkMenu *menu, GtkWidget *attach, gpointer detacher) {
	(void) detacher;
	menu->attach = attach;
	if (gtk_widget_get_parent (GTK_WIDGET (menu)) == NULL)
		gtk_widget_set_parent (GTK_WIDGET (menu), attach);
}
GtkWidget *gtk_menu_get_attach_widget (GtkMenu *menu) { return menu->attach; }
GtkWidget *gtk_menu_get_box (GtkMenu *menu) { return menu->box; }

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
static void gtk_menu_bar_class_init (GtkMenuBarClass *c) { (void) c; }
static void gtk_menu_bar_init (GtkMenuBar *bar) {
	gtk_orientable_set_orientation (GTK_ORIENTABLE (bar), GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_add_css_class (GTK_WIDGET (bar), "menubar");
}
GtkWidget *gtk_menu_bar_new (void) { return g_object_new (GTK_TYPE_MENU_BAR, NULL); }

G_DEFINE_TYPE (GtkMenuItem, gtk_menu_item, GTK_TYPE_BUTTON)
static void gtk_menu_item_dispose (GObject *o) {
	GtkMenuItem *item = GTK_MENU_ITEM (o);
	g_clear_pointer (&item->label, g_free);
	G_OBJECT_CLASS (gtk_menu_item_parent_class)->dispose (o);
}
static void gtk_menu_item_class_init (GtkMenuItemClass *c) { G_OBJECT_CLASS (c)->dispose = gtk_menu_item_dispose; }
static void gtk_menu_item_init (GtkMenuItem *item) {
	gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
	gtk_widget_set_halign (GTK_WIDGET (item), GTK_ALIGN_FILL);
}
GtkWidget *gtk_menu_item_new (void) { return g_object_new (GTK_TYPE_MENU_ITEM, NULL); }
GtkWidget *gtk_menu_item_new_with_label (const gchar *label) {
	GtkWidget *w = gtk_menu_item_new ();
	gtk_menu_item_set_label (GTK_MENU_ITEM (w), label);
	return w;
}
GtkWidget *gtk_menu_item_new_with_mnemonic (const gchar *label) { return gtk_menu_item_new_with_label (label); }
void gtk_menu_item_set_submenu (GtkMenuItem *item, GtkWidget *submenu) { item->submenu = submenu; }
GtkWidget *gtk_menu_item_get_submenu (GtkMenuItem *item) { return item->submenu; }
void gtk_menu_item_set_label (GtkMenuItem *item, const gchar *label) {
	g_free (item->label); item->label = g_strdup (label);
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

typedef struct _GtkCheckMenuItemClass { GtkCheckButtonClass parent_class; } GtkCheckMenuItemClass;
struct _GtkCheckMenuItem { GtkCheckButton parent; };
G_DEFINE_TYPE (GtkCheckMenuItem, gtk_check_menu_item, GTK_TYPE_CHECK_BUTTON)
static void gtk_check_menu_item_class_init (GtkCheckMenuItemClass *c) { (void) c; }
static void gtk_check_menu_item_init (GtkCheckMenuItem *i) { (void) i; }
GtkWidget *gtk_check_menu_item_new_with_mnemonic (const gchar *label) {
	GtkWidget *w = g_object_new (GTK_TYPE_CHECK_MENU_ITEM, NULL);
	gtk_check_button_set_label (GTK_CHECK_BUTTON (w), label);
	return w;
}
void gtk_check_menu_item_set_active (GtkCheckMenuItem *item, gboolean is_active) {
	gtk_check_button_set_active (GTK_CHECK_BUTTON (item), is_active);
}
gboolean gtk_check_menu_item_get_active (GtkCheckMenuItem *item) {
	return gtk_check_button_get_active (GTK_CHECK_BUTTON (item));
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
	return (gtk_button_new_from_icon_name) (stock_id);
}

/* accel group stub */
typedef struct _GtkAccelGroupClass { GObjectClass parent_class; } GtkAccelGroupClass;
struct _GtkAccelGroup { GObject parent; };
G_DEFINE_TYPE (GtkAccelGroup, gtk_accel_group, G_TYPE_OBJECT)
static void gtk_accel_group_class_init (GtkAccelGroupClass *c) { (void) c; }
static void gtk_accel_group_init (GtkAccelGroup *g) { (void) g; }
GtkAccelGroup *gtk_accel_group_new (void) { return g_object_new (gtk_accel_group_get_type (), NULL); }
void gtk_window_add_accel_group (GtkWindow *window, GtkAccelGroup *accel) { (void) window; (void) accel; }
void gtk_window_remove_accel_group (GtkWindow *window, GtkAccelGroup *accel) { (void) window; (void) accel; }
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
	GdkPixbuf *pixbuf = NULL;
	(void) flags;
	p = gtk_icon_theme_lookup_icon (theme, name, NULL, size, 1, GTK_TEXT_DIR_NONE, 0);
	if (p) {
		GFile *file = gtk_icon_paintable_get_file (p);
		if (file) {
			gchar *path = g_file_get_path (file);
			if (path)
				pixbuf = gdk_pixbuf_new_from_file_at_size (path, size, size, error);
			g_free (path);
			g_object_unref (file);
		}
		g_object_unref (p);
	}
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
	gtk_orientable_set_orientation (GTK_ORIENTABLE (sw), GTK_ORIENTATION_VERTICAL);
	sw->inner = (gtk_scrolled_window_new) ();
	gtk_widget_set_hexpand (sw->inner, TRUE);
	gtk_widget_set_vexpand (sw->inner, TRUE);
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

static void
verne_info_bar_class_init (VerneInfoBarClass *klass)
{
	(void) klass;
}

static void
verne_info_bar_init (VerneInfoBar *bar)
{
	gtk_orientable_set_orientation (GTK_ORIENTABLE (bar), GTK_ORIENTATION_VERTICAL);
	bar->inner = gtk_info_bar_new ();
	bar->content_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	bar->action_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	(gtk_info_bar_add_child) (GTK_INFO_BAR (bar->inner), bar->content_area);
	(gtk_info_bar_add_action_widget) (GTK_INFO_BAR (bar->inner), bar->action_area, 0);
	gtk_widget_set_hexpand (bar->inner, TRUE);
	gtk_box_append (GTK_BOX (bar), bar->inner);
}

GtkWidget *
verne_info_bar_get_inner (gpointer widget)
{
	if (widget != NULL && VERNE_IS_INFO_BAR (widget))
		return VERNE_INFO_BAR (widget)->inner;
	return widget;
}
