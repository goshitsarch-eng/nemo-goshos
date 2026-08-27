/* Adwaita icon chooser with a searchable theme grid (XApp API). */
#include "config.h"
#include "verne-gtk-compat.h"
#include <libxapp/xapp-icon-chooser-dialog.h>
#include <glib/gi18n.h>
#include <string.h>

enum {
	PROP_0,
	PROP_ALLOW_PATHS
};

struct _XAppIconChooserDialog {
	GtkDialog parent;
	GtkWidget *search;
	GtkWidget *flow;
	GtkWidget *scrolled;
	GtkWidget *selected_label;
	GtkWidget *browse;
	GtkWidget *preview;
	gchar *selected;
	gchar **all_names;
	gboolean allow_paths;
	guint rebuild_id;
};

G_DEFINE_FINAL_TYPE (XAppIconChooserDialog, xapp_icon_chooser_dialog, GTK_TYPE_DIALOG)

static const char *featured_icons[] = {
	"folder", "user-home", "user-desktop", "user-trash", "computer",
	"drive-harddisk", "drive-harddisk-symbolic", "media-optical",
	"network-server", "folder-documents", "folder-download",
	"folder-pictures", "folder-music", "folder-videos", "folder-publicshare",
	"text-x-generic", "image-x-generic", "audio-x-generic", "video-x-generic",
	"application-x-executable", "package-x-generic", "font-x-generic",
	"application-pdf", "text-html", "inode-directory",
	"emblem-favorite", "emblem-readonly", "emblem-symbolic-link", "emblem-shared",
	"dialog-information", "dialog-warning", "dialog-error", "dialog-password",
	"system-run", "system-search", "preferences-system", "applications-system",
	"utilities-terminal", "accessories-text-editor", "web-browser",
	"document-new", "document-open", "document-save", "edit-copy",
	"go-home", "go-up", "view-refresh", "list-add",
	NULL
};

static void
verne_icon_chooser_clear_flow (XAppIconChooserDialog *self)
{
	GtkWidget *child;

	if (self->flow == NULL)
		return;
	while ((child = gtk_widget_get_first_child (self->flow)) != NULL)
		gtk_flow_box_remove (GTK_FLOW_BOX (self->flow), child);
}

static void
verne_icon_chooser_set_selected (XAppIconChooserDialog *self, const gchar *value)
{
	g_free (self->selected);
	self->selected = g_strdup (value);
	if (self->selected_label) {
		if (value && value[0] != '\0')
			gtk_label_set_text (GTK_LABEL (self->selected_label), value);
		else
			gtk_label_set_text (GTK_LABEL (self->selected_label), _("No icon selected"));
	}
	if (self->preview) {
		if (value && value[0] == '/')
			gtk_image_set_from_file (GTK_IMAGE (self->preview), value);
		else if (value && value[0] != '\0')
			gtk_image_set_from_icon_name (GTK_IMAGE (self->preview), value, GTK_ICON_SIZE_DIALOG);
		else
			gtk_image_set_from_icon_name (GTK_IMAGE (self->preview), "image-missing", GTK_ICON_SIZE_DIALOG);
		gtk_image_set_pixel_size (GTK_IMAGE (self->preview), 48);
	}
}

static void
verne_icon_chooser_add_name (XAppIconChooserDialog *self, const gchar *name)
{
	GtkWidget *img;
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *child;

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_size_request (box, 72, 72);
	img = gtk_image_new_from_icon_name (name, 32);
	gtk_image_set_pixel_size (GTK_IMAGE (img), 32);
	gtk_widget_set_halign (img, GTK_ALIGN_CENTER);
	label = gtk_label_new (name);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (GTK_LABEL (label), 10);
	gtk_widget_add_css_class (label, "caption");
	gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
	gtk_box_append (GTK_BOX (box), img);
	gtk_box_append (GTK_BOX (box), label);
	gtk_widget_set_tooltip_text (box, name);
	gtk_flow_box_append (GTK_FLOW_BOX (self->flow), box);
	child = gtk_widget_get_parent (box);
	if (self->selected && g_strcmp0 (self->selected, name) == 0 && GTK_IS_FLOW_BOX_CHILD (child))
		gtk_flow_box_select_child (GTK_FLOW_BOX (self->flow), GTK_FLOW_BOX_CHILD (child));
}

static gint
verne_icon_name_ptr_cmp (gconstpointer a, gconstpointer b, gpointer data)
{
	const gchar *sa = *(const gchar * const *) a;
	const gchar *sb = *(const gchar * const *) b;
	(void) data;
	return g_strcmp0 (sa, sb);
}

static gboolean
verne_icon_name_matches (const gchar *name, const gchar *needle)
{
	gchar *hay, *need;
	gboolean ok;

	if (needle == NULL || needle[0] == '\0')
		return TRUE;
	hay = g_utf8_strdown (name, -1);
	need = g_utf8_strdown (needle, -1);
	ok = strstr (hay, need) != NULL;
	g_free (hay);
	g_free (need);
	return ok;
}

static void
verne_icon_chooser_ensure_names (XAppIconChooserDialog *self)
{
	GtkIconTheme *theme;

	if (self->all_names)
		return;
	theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
	self->all_names = gtk_icon_theme_get_icon_names (theme);
	if (self->all_names) {
		guint n = g_strv_length (self->all_names);
		g_qsort_with_data (self->all_names, (int) n, sizeof (gchar *),
				   (GCompareDataFunc) verne_icon_name_ptr_cmp, NULL);
	}
}

static gboolean
verne_icon_chooser_rebuild (gpointer data)
{
	XAppIconChooserDialog *self = data;
	const gchar *needle;
	int added = 0;
	int i;
	const int empty_cap = 96;
	const int search_cap = 600;

	self->rebuild_id = 0;
	verne_icon_chooser_ensure_names (self);
	verne_icon_chooser_clear_flow (self);
	needle = self->search ? gtk_editable_get_text (GTK_EDITABLE (self->search)) : "";

	if (needle[0] == '\0') {
		for (i = 0; featured_icons[i]; i++) {
			GtkIconTheme *theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
			if (!gtk_icon_theme_has_icon (theme, featured_icons[i]))
				continue;
			verne_icon_chooser_add_name (self, featured_icons[i]);
			added++;
		}
	}

	if (self->all_names) {
		for (i = 0; self->all_names[i]; i++) {
			const gchar *name = self->all_names[i];
			if (!verne_icon_name_matches (name, needle))
				continue;
			if (needle[0] == '\0' && added >= empty_cap)
				break;
			if (needle[0] != '\0' && added >= search_cap)
				break;
			if (needle[0] == '\0') {
				int f;
				gboolean feat = FALSE;
				for (f = 0; featured_icons[f]; f++) {
					if (g_strcmp0 (featured_icons[f], name) == 0) {
						feat = TRUE;
						break;
					}
				}
				if (feat)
					continue;
			}
			verne_icon_chooser_add_name (self, name);
			added++;
		}
	}
	return G_SOURCE_REMOVE;
}

static void
verne_icon_chooser_queue_rebuild (XAppIconChooserDialog *self)
{
	if (self->rebuild_id)
		g_source_remove (self->rebuild_id);
	self->rebuild_id = g_idle_add (verne_icon_chooser_rebuild, self);
}

static void
on_search_changed (GtkEditable *editable, gpointer data)
{
	(void) editable;
	verne_icon_chooser_queue_rebuild (XAPP_ICON_CHOOSER_DIALOG (data));
}

static void
on_selected_children (GtkFlowBox *box, gpointer data)
{
	XAppIconChooserDialog *self = data;
	GList *sel;
	GtkWidget *boxw;
	const gchar *name;

	sel = gtk_flow_box_get_selected_children (box);
	if (sel == NULL)
		return;
	boxw = gtk_flow_box_child_get_child (sel->data);
	name = gtk_widget_get_tooltip_text (boxw);
	if (name)
		verne_icon_chooser_set_selected (self, name);
	g_list_free (sel);
}

static void
on_child_activated (GtkFlowBox *box, GtkFlowBoxChild *child, gpointer data)
{
	XAppIconChooserDialog *self = data;
	GtkWidget *boxw = gtk_flow_box_child_get_child (child);
	const gchar *name = gtk_widget_get_tooltip_text (boxw);

	(void) box;
	if (name)
		verne_icon_chooser_set_selected (self, name);
	gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_OK);
}

static void
on_browse_clicked (GtkButton *button, gpointer data)
{
	XAppIconChooserDialog *self = data;
	GtkWidget *chooser;
	GtkFileFilter *filter;
	gint response;

	(void) button;
	chooser = gtk_file_chooser_dialog_new (_("Choose an image"),
					       GTK_WINDOW (self),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       _("_Cancel"), GTK_RESPONSE_CANCEL,
					       _("_Open"), GTK_RESPONSE_ACCEPT,
					       NULL);
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Images"));
	gtk_file_filter_add_pixbuf_formats (filter);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
	response = gtk_dialog_run (GTK_DIALOG (chooser));
	if (response == GTK_RESPONSE_ACCEPT) {
		gchar *path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
		if (path)
			verne_icon_chooser_set_selected (self, path);
		g_free (path);
	}
	gtk_window_destroy (GTK_WINDOW (chooser));
}

static void
on_extra_button_clicked (GtkButton *button, gpointer data)
{
	gint response = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "verne-response"));
	gtk_dialog_response (GTK_DIALOG (data), response);
}

static void
xapp_icon_chooser_dialog_set_property (GObject *object, guint prop_id,
				       const GValue *value, GParamSpec *pspec)
{
	XAppIconChooserDialog *self = XAPP_ICON_CHOOSER_DIALOG (object);

	if (prop_id == PROP_ALLOW_PATHS) {
		self->allow_paths = g_value_get_boolean (value);
		if (self->browse)
			gtk_widget_set_visible (self->browse, self->allow_paths);
	} else {
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
xapp_icon_chooser_dialog_get_property (GObject *object, guint prop_id,
				       GValue *value, GParamSpec *pspec)
{
	XAppIconChooserDialog *self = XAPP_ICON_CHOOSER_DIALOG (object);

	if (prop_id == PROP_ALLOW_PATHS)
		g_value_set_boolean (value, self->allow_paths);
	else
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
xapp_icon_chooser_dialog_dispose (GObject *object)
{
	XAppIconChooserDialog *self = XAPP_ICON_CHOOSER_DIALOG (object);

	if (self->rebuild_id) {
		g_source_remove (self->rebuild_id);
		self->rebuild_id = 0;
	}
	g_clear_pointer (&self->selected, g_free);
	g_clear_pointer (&self->all_names, g_strfreev);
	G_OBJECT_CLASS (xapp_icon_chooser_dialog_parent_class)->dispose (object);
}

static void
xapp_icon_chooser_dialog_class_init (XAppIconChooserDialogClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	oclass->set_property = xapp_icon_chooser_dialog_set_property;
	oclass->get_property = xapp_icon_chooser_dialog_get_property;
	oclass->dispose = xapp_icon_chooser_dialog_dispose;
	g_object_class_install_property (oclass, PROP_ALLOW_PATHS,
					 g_param_spec_boolean ("allow-paths", NULL, NULL, FALSE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
xapp_icon_chooser_dialog_init (XAppIconChooserDialog *self)
{
	GtkWidget *content;
	GtkWidget *box;
	GtkWidget *header;
	GtkWidget *preview_row;

	gtk_window_set_title (GTK_WINDOW (self), _("Choose an icon"));
	gtk_window_set_default_size (GTK_WINDOW (self), 680, 520);
	gtk_window_set_modal (GTK_WINDOW (self), TRUE);

	gtk_dialog_add_button (GTK_DIALOG (self), _("_Cancel"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (self), _("_OK"), GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_OK);

	content = gtk_dialog_get_content_area (GTK_DIALOG (self));
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start (box, 12);
	gtk_widget_set_margin_end (box, 12);
	gtk_widget_set_margin_top (box, 12);
	gtk_widget_set_margin_bottom (box, 12);
	gtk_widget_set_hexpand (box, TRUE);
	gtk_widget_set_vexpand (box, TRUE);

	header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
	self->search = gtk_search_entry_new ();
	gtk_widget_set_hexpand (self->search, TRUE);
	gtk_search_entry_set_placeholder_text (GTK_SEARCH_ENTRY (self->search),
					       _("Search icons"));
	g_signal_connect (self->search, "search-changed", G_CALLBACK (on_search_changed), self);
	self->browse = gtk_button_new_with_mnemonic (_("_Browse…"));
	gtk_widget_set_visible (self->browse, FALSE);
	g_signal_connect (self->browse, "clicked", G_CALLBACK (on_browse_clicked), self);
	gtk_box_append (GTK_BOX (header), self->search);
	gtk_box_append (GTK_BOX (header), self->browse);

	self->flow = gtk_flow_box_new ();
	gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (self->flow), GTK_SELECTION_SINGLE);
	gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (self->flow), 4);
	gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self->flow), 10);
	gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (self->flow), TRUE);
	g_signal_connect (self->flow, "selected-children-changed", G_CALLBACK (on_selected_children), self);
	g_signal_connect (self->flow, "child-activated", G_CALLBACK (on_child_activated), self);

	self->scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->scrolled),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (self->scrolled), 320);
	gtk_widget_set_vexpand (self->scrolled, TRUE);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->scrolled), self->flow);

	preview_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
	self->preview = gtk_image_new_from_icon_name ("image-missing", GTK_ICON_SIZE_DIALOG);
	gtk_image_set_pixel_size (GTK_IMAGE (self->preview), 48);
	self->selected_label = gtk_label_new (_("No icon selected"));
	gtk_label_set_xalign (GTK_LABEL (self->selected_label), 0.0);
	gtk_label_set_ellipsize (GTK_LABEL (self->selected_label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_widget_set_hexpand (self->selected_label, TRUE);
	gtk_box_append (GTK_BOX (preview_row), self->preview);
	gtk_box_append (GTK_BOX (preview_row), self->selected_label);

	gtk_box_append (GTK_BOX (box), header);
	gtk_box_append (GTK_BOX (box), self->scrolled);
	gtk_box_append (GTK_BOX (box), preview_row);
	gtk_box_append (GTK_BOX (content), box);

	verne_icon_chooser_queue_rebuild (self);
}

XAppIconChooserDialog *
xapp_icon_chooser_dialog_new (void)
{
	return g_object_new (XAPP_TYPE_ICON_CHOOSER_DIALOG,
			     "use-header-bar", TRUE,
			     "title", _("Choose an icon"),
			     NULL);
}

void
xapp_icon_chooser_dialog_add_button (XAppIconChooserDialog *dialog,
				     GtkWidget *button,
				     GtkPackType packing,
				     GtkResponseType response_id)
{
	GtkWidget *titlebar;

	g_return_if_fail (XAPP_IS_ICON_CHOOSER_DIALOG (dialog));
	g_return_if_fail (GTK_IS_WIDGET (button));

	g_object_set_data (G_OBJECT (button), "verne-response", GINT_TO_POINTER (response_id));
	g_signal_connect (button, "clicked", G_CALLBACK (on_extra_button_clicked), dialog);
	gtk_widget_set_visible (button, TRUE);

	titlebar = gtk_window_get_titlebar (GTK_WINDOW (dialog));
	if (GTK_IS_HEADER_BAR (titlebar)) {
		if (packing == GTK_PACK_START)
			gtk_header_bar_pack_start (GTK_HEADER_BAR (titlebar), button);
		else
			gtk_header_bar_pack_end (GTK_HEADER_BAR (titlebar), button);
	} else {
		gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, response_id);
	}
}

gint
xapp_icon_chooser_dialog_run (XAppIconChooserDialog *dialog)
{
	verne_icon_chooser_queue_rebuild (dialog);
	return gtk_dialog_run (GTK_DIALOG (dialog));
}

gint
xapp_icon_chooser_dialog_run_with_icon (XAppIconChooserDialog *dialog, const gchar *icon)
{
	if (icon && icon[0] != '\0') {
		verne_icon_chooser_set_selected (dialog, icon);
		if (icon[0] != '/' && dialog->search)
			gtk_editable_set_text (GTK_EDITABLE (dialog->search), icon);
	}
	return xapp_icon_chooser_dialog_run (dialog);
}

gchar *
xapp_icon_chooser_dialog_get_icon_string (XAppIconChooserDialog *dialog)
{
	const gchar *search;

	if (dialog->selected && dialog->selected[0] != '\0')
		return g_strdup (dialog->selected);
	search = dialog->search ? gtk_editable_get_text (GTK_EDITABLE (dialog->search)) : NULL;
	if (search && search[0] != '\0')
		return g_strdup (search);
	return g_strdup ("folder");
}
