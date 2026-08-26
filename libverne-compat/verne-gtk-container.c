#include "config.h"
#include "verne-gtk-compat.h"

static guint
container_n_children (GtkWidget *container)
{
	guint n = 0;
	GtkWidget *child;
	for (child = gtk_widget_get_first_child (container); child; child = gtk_widget_get_next_sibling (child))
		n++;
	return n;
}

void
gtk_container_add (gpointer container_ptr, GtkWidget *child)
{
	GtkWidget *container = GTK_WIDGET (container_ptr);
	g_return_if_fail (GTK_IS_WIDGET (container));
	g_return_if_fail (GTK_IS_WIDGET (child));

	if (ADW_IS_APPLICATION_WINDOW (container)) {
		adw_application_window_set_content (ADW_APPLICATION_WINDOW (container), child);
		return;
	}

	if (VERNE_IS_SCROLLED_WINDOW (container)) {
		(gtk_scrolled_window_set_child) (verne_to_gtk_sw (container), child);
		return;
	}
	if (VERNE_IS_INFO_BAR (container)) {
		(gtk_info_bar_add_child) (verne_to_gtk_ib (container), child);
		return;
	}

	/* Custom GtkContainer subclasses such as NemoPathBar */
	if (G_TYPE_CHECK_INSTANCE_TYPE (container, gtk_container_get_type ()) &&
	    !GTK_IS_WINDOW (container) && !GTK_IS_BOX (container) &&
	    !GTK_IS_GRID (container) && !GTK_IS_NOTEBOOK (container) &&
	    !GTK_IS_OVERLAY (container) && !GTK_IS_DIALOG (container)) {
		GtkContainerClass *klass = (GtkContainerClass *) G_OBJECT_GET_CLASS (container);
		if (klass && klass->add) {
			klass->add (GTK_CONTAINER (container), child);
			return;
		}
	}

	if (GTK_IS_DIALOG (container)) {
		gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (container))), child);
		return;
	}

	if (GTK_IS_WINDOW (container)) {
		if (gtk_window_get_child (GTK_WINDOW (container)) == NULL)
			gtk_window_set_child (GTK_WINDOW (container), child);
		else {
			GtkWidget *existing = gtk_window_get_child (GTK_WINDOW (container));
			if (GTK_IS_BOX (existing) || GTK_IS_GRID (existing))
				gtk_container_add (existing, child);
			else {
				GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
				gtk_window_set_child (GTK_WINDOW (container), box);
				if (existing)
					gtk_box_append (GTK_BOX (box), existing);
				gtk_box_append (GTK_BOX (box), child);
			}
		}
	} else if (GTK_IS_BOX (container)) {
		gtk_box_append (GTK_BOX (container), child);
	} else if (GTK_IS_GRID (container)) {
		int row = (int) container_n_children (container);
		gtk_grid_attach (GTK_GRID (container), child, 0, row, 1, 1);
	} else if (GTK_IS_SCROLLED_WINDOW (container)) {
		gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (container), child);
	} else if (GTK_IS_FRAME (container)) {
		gtk_frame_set_child (GTK_FRAME (container), child);
	} else if (GTK_IS_REVEALER (container)) {
		gtk_revealer_set_child (GTK_REVEALER (container), child);
	} else if (GTK_IS_OVERLAY (container)) {
		GtkWidget *main_child = gtk_overlay_get_child (GTK_OVERLAY (container));
		/* Extra overlay widgets (floating bar, etc.) must not steal the
		 * main child slot. After a view switch the previous main child is
		 * gone — always restore content with set_child, not add_overlay. */
		if (main_child == NULL || main_child == child)
			gtk_overlay_set_child (GTK_OVERLAY (container), child);
		else
			gtk_overlay_add_overlay (GTK_OVERLAY (container), child);
	} else if (GTK_IS_PANED (container)) {
		if (gtk_paned_get_start_child (GTK_PANED (container)) == NULL)
			gtk_paned_set_start_child (GTK_PANED (container), child);
		else
			gtk_paned_set_end_child (GTK_PANED (container), child);
	} else if (GTK_IS_NOTEBOOK (container)) {
		gtk_notebook_append_page (GTK_NOTEBOOK (container), child, NULL);
	} else if (GTK_IS_EXPANDER (container)) {
		gtk_expander_set_child (GTK_EXPANDER (container), child);
	} else if (GTK_IS_VIEWPORT (container)) {
		gtk_viewport_set_child (GTK_VIEWPORT (container), child);
	} else if (GTK_IS_SEARCH_BAR (container)) {
		gtk_search_bar_set_child (GTK_SEARCH_BAR (container), child);
	} else if (GTK_IS_POPOVER (container)) {
		gtk_popover_set_child (GTK_POPOVER (container), child);
	} else if (GTK_IS_INFO_BAR (container)) {
		gtk_info_bar_add_child (GTK_INFO_BAR (container), child);
	} else if (GTK_IS_FLOW_BOX (container)) {
		gtk_flow_box_append (GTK_FLOW_BOX (container), child);
	} else if (GTK_IS_LIST_BOX (container)) {
		gtk_list_box_append (GTK_LIST_BOX (container), child);
	} else if (GTK_IS_STACK (container)) {
		gtk_stack_add_child (GTK_STACK (container), child);
	} else if (GTK_IS_ACTION_BAR (container)) {
		gtk_action_bar_set_center_widget (GTK_ACTION_BAR (container), child);
	} else if (GTK_IS_HEADER_BAR (container)) {
		gtk_header_bar_set_title_widget (GTK_HEADER_BAR (container), child);
	} else if (GTK_IS_BUTTON (container)) {
		/* GTK4 GtkButton owns a single child; gtk_widget_set_parent
		 * leaves that child unaccounted for and aborts on dispose
		 * (pathbar Home/xdg buttons pack a GtkBox of image+label). */
		gtk_button_set_child (GTK_BUTTON (container), child);
	} else if (GTK_IS_BIN (container)) {
		GtkBin *bin = GTK_BIN (container);
		if (bin->child)
			gtk_widget_unparent (bin->child);
		bin->child = child;
		gtk_widget_set_parent (child, container);
	} else {
		gtk_widget_set_parent (child, container);
	}
}

void
gtk_container_remove (gpointer container_ptr, GtkWidget *child)
{
	GtkWidget *container = GTK_WIDGET (container_ptr);
	g_return_if_fail (GTK_IS_WIDGET (container));
	g_return_if_fail (GTK_IS_WIDGET (child));

	if (VERNE_IS_SCROLLED_WINDOW (container)) {
		GtkWidget *inner = verne_scrolled_window_get_inner (container);
		GtkWidget *sw_child;

		sw_child = (gtk_scrolled_window_get_child) (GTK_SCROLLED_WINDOW (inner));
		if (sw_child == child)
			(gtk_scrolled_window_set_child) (GTK_SCROLLED_WINDOW (inner), NULL);
		else if (child == inner)
			gtk_box_remove (GTK_BOX (container), child);
		return;
	}

	if (G_TYPE_CHECK_INSTANCE_TYPE (container, gtk_container_get_type ()) &&
	    !GTK_IS_WINDOW (container) && !GTK_IS_BOX (container) &&
	    !GTK_IS_GRID (container) && !GTK_IS_NOTEBOOK (container) &&
	    !GTK_IS_OVERLAY (container) && !GTK_IS_DIALOG (container)) {
		GtkContainerClass *klass = (GtkContainerClass *) G_OBJECT_GET_CLASS (container);
		if (klass && klass->remove) {
			klass->remove (GTK_CONTAINER (container), child);
			return;
		}
	}

	if (GTK_IS_BOX (container))
		gtk_box_remove (GTK_BOX (container), child);
	else if (GTK_IS_GRID (container))
		gtk_grid_remove (GTK_GRID (container), child);
	else if (GTK_IS_WINDOW (container) && gtk_window_get_child (GTK_WINDOW (container)) == child)
		gtk_window_set_child (GTK_WINDOW (container), NULL);
	else if (GTK_IS_OVERLAY (container)) {
		if (gtk_overlay_get_child (GTK_OVERLAY (container)) == child)
			gtk_overlay_set_child (GTK_OVERLAY (container), NULL);
		else
			gtk_overlay_remove_overlay (GTK_OVERLAY (container), child);
	} else if (GTK_IS_REVEALER (container) && gtk_revealer_get_child (GTK_REVEALER (container)) == child)
		gtk_revealer_set_child (GTK_REVEALER (container), NULL);
	else if (GTK_IS_VIEWPORT (container) && gtk_viewport_get_child (GTK_VIEWPORT (container)) == child)
		gtk_viewport_set_child (GTK_VIEWPORT (container), NULL);
	else if (GTK_IS_EXPANDER (container) && gtk_expander_get_child (GTK_EXPANDER (container)) == child)
		gtk_expander_set_child (GTK_EXPANDER (container), NULL);
	else if (GTK_IS_SCROLLED_WINDOW (container))
		gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (container), NULL);
	else if (GTK_IS_FRAME (container))
		gtk_frame_set_child (GTK_FRAME (container), NULL);
	else if (GTK_IS_NOTEBOOK (container)) {
		int page = gtk_notebook_page_num (GTK_NOTEBOOK (container), child);
		if (page >= 0)
			gtk_notebook_remove_page (GTK_NOTEBOOK (container), page);
	} else if (GTK_IS_LIST_BOX (container))
		gtk_list_box_remove (GTK_LIST_BOX (container), child);
	else if (GTK_IS_STACK (container))
		gtk_stack_remove (GTK_STACK (container), child);
	else if (GTK_IS_PANED (container)) {
		if (gtk_paned_get_start_child (GTK_PANED (container)) == child)
			gtk_paned_set_start_child (GTK_PANED (container), NULL);
		else if (gtk_paned_get_end_child (GTK_PANED (container)) == child)
			gtk_paned_set_end_child (GTK_PANED (container), NULL);
	} else if (GTK_IS_BUTTON (container)) {
		if (gtk_button_get_child (GTK_BUTTON (container)) == child)
			gtk_button_set_child (GTK_BUTTON (container), NULL);
	} else if (GTK_IS_BIN (container)) {
		GtkBin *bin = GTK_BIN (container);
		if (bin->child == child) {
			gtk_widget_unparent (child);
			bin->child = NULL;
		}
	} else if (gtk_widget_get_parent (child) == container)
		gtk_widget_unparent (child);
}

void
gtk_container_foreach (GtkWidget *container, GtkCallback callback, gpointer callback_data)
{
	GtkWidget *child, *next;
	g_return_if_fail (callback != NULL);
	child = gtk_widget_get_first_child (container);
	while (child) {
		next = gtk_widget_get_next_sibling (child);
		callback (child, callback_data);
		child = next;
	}
}

GList *
gtk_container_get_children (GtkWidget *container)
{
	GList *list = NULL;
	GtkWidget *child;
	for (child = gtk_widget_get_last_child (container); child; child = gtk_widget_get_prev_sibling (child))
		list = g_list_prepend (list, child);
	return list;
}

void
gtk_container_set_border_width (GtkWidget *container, guint border_width)
{
	gtk_widget_set_margin_top (container, border_width);
	gtk_widget_set_margin_bottom (container, border_width);
	gtk_widget_set_margin_start (container, border_width);
	gtk_widget_set_margin_end (container, border_width);
}

guint
gtk_container_get_border_width (GtkWidget *container)
{
	return gtk_widget_get_margin_top (container);
}

void
gtk_container_set_focus_child (GtkWidget *container, GtkWidget *child)
{
	if (child)
		gtk_widget_grab_focus (child);
	(void) container;
}

GtkWidget *
gtk_container_get_focus_child (GtkWidget *container)
{
	if (GTK_IS_WINDOW (container))
		return gtk_window_get_focus (GTK_WINDOW (container));
	return gtk_widget_get_focus_child (container);
}

void
gtk_box_pack_start (GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding)
{
	GtkOrientation ori = gtk_orientable_get_orientation (GTK_ORIENTABLE (box));
	if (ori == GTK_ORIENTATION_HORIZONTAL) {
		gtk_widget_set_hexpand (child, expand);
		gtk_widget_set_halign (child, fill ? GTK_ALIGN_FILL : GTK_ALIGN_START);
		if (padding) {
			gtk_widget_set_margin_start (child, padding);
			gtk_widget_set_margin_end (child, padding);
		}
	} else {
		gtk_widget_set_vexpand (child, expand);
		gtk_widget_set_valign (child, fill ? GTK_ALIGN_FILL : GTK_ALIGN_START);
		if (padding) {
			gtk_widget_set_margin_top (child, padding);
			gtk_widget_set_margin_bottom (child, padding);
		}
	}
	gtk_box_append (box, child);
}

void
gtk_box_pack_end (GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding)
{
	gtk_box_pack_start (box, child, expand, fill, padding);
}

void
gtk_box_reorder_child (GtkBox *box, GtkWidget *child, gint position)
{
	GtkWidget *sib = gtk_widget_get_first_child (GTK_WIDGET (box));
	gint i = 0;
	if (position <= 0) {
		gtk_box_reorder_child_after (box, child, NULL);
		return;
	}
	while (sib && i < position - 1) {
		sib = gtk_widget_get_next_sibling (sib);
		i++;
	}
	gtk_box_reorder_child_after (box, child, sib);
}

void
gtk_box_set_child_packing (GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding, GtkPackType pack_type)
{
	(void) box;
	(void) pack_type;
	gtk_widget_set_hexpand (child, expand);
	gtk_widget_set_vexpand (child, expand);
	gtk_widget_set_halign (child, fill ? GTK_ALIGN_FILL : GTK_ALIGN_START);
	if (padding)
		gtk_widget_set_margin_start (child, padding);
}

void
gtk_paned_pack1 (GtkPaned *paned, GtkWidget *child, gboolean resize, gboolean shrink)
{
	gtk_paned_set_start_child (paned, child);
	gtk_paned_set_resize_start_child (paned, resize);
	gtk_paned_set_shrink_start_child (paned, shrink);
}

void
gtk_paned_pack2 (GtkPaned *paned, GtkWidget *child, gboolean resize, gboolean shrink)
{
	gtk_paned_set_end_child (paned, child);
	gtk_paned_set_resize_end_child (paned, resize);
	gtk_paned_set_shrink_end_child (paned, shrink);
}

void
gtk_widget_destroy (GtkWidget *widget)
{
	GtkWidget *parent;

	if (widget == NULL)
		return;

	/* Hold a ref across unparent so dispose matches GTK3 gtk_widget_destroy. */
	g_object_ref (widget);

	if (!GTK_IS_WINDOW (widget))
		verne_widget_invoke_destroy (widget);

	parent = gtk_widget_get_parent (widget);
	if (parent != NULL)
		gtk_container_remove (parent, widget);
	else if (GTK_IS_WINDOW (widget)) {
		/* Hide first so a WM-destroyed native is not torn down twice. */
		gtk_widget_set_visible (widget, FALSE);
		if (gtk_widget_get_mapped (widget))
			gtk_widget_unrealize (widget);
		gtk_window_destroy (GTK_WINDOW (widget));
	}

	g_object_unref (widget);
}

void
gtk_widget_show_all (GtkWidget *widget)
{
	GtkWidget *child;

	if (widget == NULL)
		return;
	/* GTK3 gtk_widget_show_all() on a GtkMenu shows items, not the popup. */
	if (GTK_IS_POPOVER (widget) || GTK_IS_MENU (widget)) {
		GtkWidget *box = GTK_IS_MENU (widget) ? gtk_menu_get_box (GTK_MENU (widget))
						      : gtk_popover_get_child (GTK_POPOVER (widget));
		if (box)
			gtk_widget_show_all (box);
		return;
	}
	gtk_widget_set_visible (widget, TRUE);
	for (child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child))
		gtk_widget_show_all (child);
}

void
gtk_widget_reparent (GtkWidget *widget, GtkWidget *new_parent)
{
	GtkWidget *old = gtk_widget_get_parent (widget);
	if (old)
		gtk_container_remove (old, widget);
	gtk_container_add (new_parent, widget);
}

void
gtk_scrolled_window_add_with_viewport (GtkScrolledWindow *sw, GtkWidget *child)
{
	gtk_scrolled_window_set_child (sw, child);
}

static gint dialog_response;
static GMainLoop *dialog_loop;

static void
dialog_response_cb (GtkDialog *dialog, gint response, gpointer data)
{
	(void) dialog;
	(void) data;
	dialog_response = response;
	if (dialog_loop)
		g_main_loop_quit (dialog_loop);
}

static gboolean
verne_dialog_close_request (GtkWindow *window, gpointer data)
{
	(void) data;
	gtk_widget_set_visible (GTK_WIDGET (window), FALSE);
	if (GTK_IS_DIALOG (window))
		gtk_dialog_response (GTK_DIALOG (window), GTK_RESPONSE_DELETE_EVENT);
	return TRUE;
}

void
verne_prepare_dialog (GtkWidget *widget)
{
	GtkApplication *app;
	GtkWindow *active;

	if (widget == NULL || !GTK_IS_DIALOG (widget))
		return;
	if (g_object_get_data (G_OBJECT (widget), "verne-dialog-prepared"))
		return;
	g_object_set_data (G_OBJECT (widget), "verne-dialog-prepared", GINT_TO_POINTER (1));

	gtk_window_set_hide_on_close (GTK_WINDOW (widget), TRUE);
	g_signal_connect (widget, "close-request", G_CALLBACK (verne_dialog_close_request), NULL);

	if (gtk_window_get_transient_for (GTK_WINDOW (widget)) == NULL) {
		app = GTK_APPLICATION (g_application_get_default ());
		if (app) {
			active = gtk_application_get_active_window (app);
			if (active && GTK_WIDGET (active) != widget)
				gtk_window_set_transient_for (GTK_WINDOW (widget), active);
		}
	}
}

gint
gtk_dialog_run (GtkDialog *dialog)
{
	gulong id;
	dialog_response = GTK_RESPONSE_NONE;
	dialog_loop = g_main_loop_new (NULL, FALSE);
	id = g_signal_connect (dialog, "response", G_CALLBACK (dialog_response_cb), NULL);
	verne_prepare_dialog (GTK_WIDGET (dialog));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_widget_set_visible (GTK_WIDGET (dialog), TRUE);
	gtk_window_present (GTK_WINDOW (dialog));
	g_main_loop_run (dialog_loop);
	g_signal_handler_disconnect (dialog, id);
	g_main_loop_unref (dialog_loop);
	dialog_loop = NULL;
	return dialog_response;
}

GtkWidget *
gtk_table_new (guint rows, guint columns, gboolean homogeneous)
{
	GtkWidget *grid = gtk_grid_new ();
	(void) rows;
	(void) columns;
	gtk_grid_set_column_homogeneous (GTK_GRID (grid), homogeneous);
	gtk_grid_set_row_homogeneous (GTK_GRID (grid), homogeneous);
	return grid;
}

void
gtk_table_attach_defaults (GtkGrid *table, GtkWidget *child, guint left, guint right, guint top, guint bottom)
{
	gtk_grid_attach (table, child, (int) left, (int) top, (int) (right - left), (int) (bottom - top));
}

GtkWidget *
gtk_alignment_new (gfloat xalign, gfloat yalign, gfloat xscale, gfloat yscale)
{
	GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	(void) xscale;
	(void) yscale;
	gtk_widget_set_halign (box, xalign < 0.25 ? GTK_ALIGN_START : (xalign > 0.75 ? GTK_ALIGN_END : GTK_ALIGN_CENTER));
	gtk_widget_set_valign (box, yalign < 0.25 ? GTK_ALIGN_START : (yalign > 0.75 ? GTK_ALIGN_END : GTK_ALIGN_CENTER));
	return box;
}

void
gtk_alignment_set_padding (GtkWidget *alignment, guint top, guint bottom, guint left, guint right)
{
	gtk_widget_set_margin_top (alignment, top);
	gtk_widget_set_margin_bottom (alignment, bottom);
	gtk_widget_set_margin_start (alignment, left);
	gtk_widget_set_margin_end (alignment, right);
}
