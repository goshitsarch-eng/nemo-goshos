/* GTK3-style file chooser that applies custom filters while listing. */
#include "config.h"
#include "verne-gtk-compat.h"
#include <glib/gi18n.h>
#include <string.h>

typedef struct {
	GtkFileChooserAction action;
	GFile *folder;
	GFile *selected;
	GList *filters;
	GtkFileFilter *active_filter;
	GtkWidget *path;
	GtkWidget *list;
	GtkWidget *filter_combo;
	GtkWidget *up;
	gint accept_response;
} VerneFileChooser;

static GQuark
verne_fc_quark (void)
{
	static GQuark q;
	if (q == 0)
		q = g_quark_from_static_string ("verne-file-chooser");
	return q;
}

gboolean
verne_is_file_chooser (gpointer widget)
{
	return widget != NULL && g_object_get_qdata (G_OBJECT (widget), verne_fc_quark ()) != NULL;
}

static VerneFileChooser *
verne_fc_get (gpointer widget)
{
	return widget ? g_object_get_qdata (G_OBJECT (widget), verne_fc_quark ()) : NULL;
}

static void
verne_fc_free (gpointer p)
{
	VerneFileChooser *fc = p;
	g_clear_object (&fc->folder);
	g_clear_object (&fc->selected);
	g_list_free_full (fc->filters, g_object_unref);
	g_free (fc);
}

static void
verne_fc_clear_list (VerneFileChooser *fc)
{
	GtkWidget *child;

	if (fc->list == NULL)
		return;
	while ((child = gtk_widget_get_first_child (fc->list)) != NULL)
		gtk_list_box_remove (GTK_LIST_BOX (fc->list), child);
}

static gboolean
verne_fc_file_visible (VerneFileChooser *fc, GFile *file, GFileInfo *info)
{
	if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
		return TRUE;
	if (fc->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
		return FALSE;
	if (fc->active_filter == NULL)
		return TRUE;
	if (verne_file_filter_has_custom (fc->active_filter))
		return verne_file_filter_accepts_file (fc->active_filter, file);
	if (GTK_IS_FILTER (fc->active_filter))
		return gtk_filter_match (GTK_FILTER (fc->active_filter), G_OBJECT (info));
	return TRUE;
}

static void
verne_fc_set_selected (VerneFileChooser *fc, GFile *file)
{
	g_clear_object (&fc->selected);
	if (file)
		fc->selected = g_object_ref (file);
}

static GtkWidget *
verne_fc_row_for_info (VerneFileChooser *fc, GFile *file, GFileInfo *info)
{
	GtkWidget *row, *box, *img, *label;
	const gchar *name, *ctype, *icon_name;
	gboolean is_dir;
	GIcon *gicon;

	name = g_file_info_get_display_name (info);
	if (name == NULL)
		name = g_file_info_get_name (info);
	is_dir = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_margin_start (box, 6);
	gtk_widget_set_margin_end (box, 6);
	gtk_widget_set_margin_top (box, 2);
	gtk_widget_set_margin_bottom (box, 2);
	gicon = g_file_info_get_icon (info);
	if (gicon)
		img = gtk_image_new_from_gicon (gicon, GTK_ICON_SIZE_BUTTON);
	else {
		icon_name = is_dir ? "folder" : "text-x-generic";
		ctype = g_file_info_get_content_type (info);
		if (ctype && g_content_type_is_a (ctype, "image/*"))
			icon_name = "image-x-generic";
		img = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
	}
	gtk_image_set_pixel_size (GTK_IMAGE (img), 24);
	label = gtk_label_new (name);
	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_hexpand (label, TRUE);
	gtk_box_append (GTK_BOX (box), img);
	gtk_box_append (GTK_BOX (box), label);
	g_object_set_data_full (G_OBJECT (box), "verne-file", g_object_ref (file), g_object_unref);
	g_object_set_data (G_OBJECT (box), "verne-is-dir", GINT_TO_POINTER (is_dir));
	(void) fc;
	return box;
}

static void
verne_fc_rebuild (VerneFileChooser *fc)
{
	GFileEnumerator *en;
	GFileInfo *info;
	gchar *path;

	verne_fc_clear_list (fc);
	if (fc->folder == NULL)
		return;
	path = g_file_get_path (fc->folder);
	if (path && fc->path)
		gtk_editable_set_text (GTK_EDITABLE (fc->path), path);
	g_free (path);

	en = g_file_enumerate_children (fc->folder,
					G_FILE_ATTRIBUTE_STANDARD_NAME ","
					G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
					G_FILE_ATTRIBUTE_STANDARD_TYPE ","
					G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
					G_FILE_ATTRIBUTE_STANDARD_ICON,
					G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (en == NULL)
		return;
	while ((info = g_file_enumerator_next_file (en, NULL, NULL)) != NULL) {
		const gchar *name = g_file_info_get_name (info);
		GFile *child;

		if (name == NULL || name[0] == '.') {
			g_object_unref (info);
			continue;
		}
		child = g_file_get_child (fc->folder, name);
		if (verne_fc_file_visible (fc, child, info))
			gtk_list_box_append (GTK_LIST_BOX (fc->list), verne_fc_row_for_info (fc, child, info));
		g_object_unref (child);
		g_object_unref (info);
	}
	g_object_unref (en);
}

static void
verne_fc_go (VerneFileChooser *fc, GFile *dir)
{
	if (dir == NULL)
		return;
	g_set_object (&fc->folder, dir);
	verne_fc_set_selected (fc, fc->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ? dir : NULL);
	verne_fc_rebuild (fc);
}

static void
on_up (GtkButton *button, gpointer data)
{
	GtkWidget *dialog = data;
	VerneFileChooser *fc = verne_fc_get (dialog);
	GFile *parent;

	(void) button;
	if (fc == NULL || fc->folder == NULL)
		return;
	parent = g_file_get_parent (fc->folder);
	if (parent) {
		verne_fc_go (fc, parent);
		g_object_unref (parent);
	}
}

static void
on_home (GtkButton *button, gpointer data)
{
	GFile *home = g_file_new_for_path (g_get_home_dir ());
	(void) button;
	verne_fc_go (verne_fc_get (data), home);
	g_object_unref (home);
}

static void
on_path_activate (GtkEntry *entry, gpointer data)
{
	const gchar *text = gtk_editable_get_text (GTK_EDITABLE (entry));
	GFile *dir;

	if (text == NULL || text[0] == '\0')
		return;
	dir = g_file_new_for_path (text);
	if (g_file_query_file_type (dir, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_DIRECTORY)
		verne_fc_go (verne_fc_get (data), dir);
	g_object_unref (dir);
}

static void
on_row_selected (GtkListBox *box, GtkListBoxRow *row, gpointer data)
{
	VerneFileChooser *fc = verne_fc_get (data);
	GtkWidget *child;
	GFile *file;
	(void) box;
	if (fc == NULL || row == NULL)
		return;
	child = gtk_list_box_row_get_child (row);
	file = g_object_get_data (G_OBJECT (child), "verne-file");
	verne_fc_set_selected (fc, file);
}

static void
on_row_activated (GtkListBox *box, GtkListBoxRow *row, gpointer data)
{
	GtkWidget *dialog = data;
	VerneFileChooser *fc = verne_fc_get (dialog);
	GtkWidget *child;
	GFile *file;
	gboolean is_dir;

	(void) box;
	if (fc == NULL || row == NULL)
		return;
	child = gtk_list_box_row_get_child (row);
	file = g_object_get_data (G_OBJECT (child), "verne-file");
	is_dir = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), "verne-is-dir"));
	if (is_dir) {
		verne_fc_go (fc, file);
		return;
	}
	verne_fc_set_selected (fc, file);
	gtk_dialog_response (GTK_DIALOG (dialog), fc->accept_response);
}

static void
on_filter_changed (GtkComboBox *combo, gpointer data)
{
	VerneFileChooser *fc = verne_fc_get (data);
	gint idx;
	GList *l;
	gint i;

	if (fc == NULL)
		return;
	idx = gtk_combo_box_get_active (combo);
	fc->active_filter = NULL;
	for (l = fc->filters, i = 0; l; l = l->next, i++) {
		if (i == idx) {
			fc->active_filter = l->data;
			break;
		}
	}
	verne_fc_rebuild (fc);
}

gboolean
verne_file_chooser_set_current_folder (gpointer chooser, const gchar *filename)
{
	VerneFileChooser *fc = verne_fc_get (chooser);
	GFile *file;
	gboolean ok;

	if (filename == NULL)
		return FALSE;
	if (fc) {
		file = g_file_new_for_path (filename);
		verne_fc_go (fc, file);
		g_object_unref (file);
		return TRUE;
	}
	if (!GTK_IS_FILE_CHOOSER (chooser))
		return FALSE;
	file = g_file_new_for_path (filename);
	ok = (gtk_file_chooser_set_current_folder) (GTK_FILE_CHOOSER (chooser), file, NULL);
	g_object_unref (file);
	return ok;
}

GFile *
verne_file_chooser_get_file (gpointer chooser)
{
	VerneFileChooser *fc = verne_fc_get (chooser);

	if (fc) {
		if (fc->selected)
			return g_object_ref (fc->selected);
		if (fc->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER && fc->folder)
			return g_object_ref (fc->folder);
		return NULL;
	}
	if (GTK_IS_FILE_CHOOSER (chooser))
		return (gtk_file_chooser_get_file) (GTK_FILE_CHOOSER (chooser));
	return NULL;
}

gchar *
verne_file_chooser_get_uri (gpointer chooser)
{
	GFile *file = verne_file_chooser_get_file (chooser);
	gchar *uri = file ? g_file_get_uri (file) : NULL;
	if (file)
		g_object_unref (file);
	return uri;
}

void
verne_file_chooser_add_filter (gpointer chooser, GtkFileFilter *filter)
{
	VerneFileChooser *fc = verne_fc_get (chooser);
	const gchar *name;

	if (fc == NULL) {
		if (GTK_IS_FILE_CHOOSER (chooser) && filter)
			(gtk_file_chooser_add_filter) (GTK_FILE_CHOOSER (chooser), filter);
		return;
	}
	if (filter == NULL)
		return;
	fc->filters = g_list_append (fc->filters, g_object_ref (filter));
	if (fc->active_filter == NULL)
		fc->active_filter = filter;
	name = gtk_file_filter_get_name (filter);
	if (fc->filter_combo) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (fc->filter_combo),
						name && name[0] ? name : _("Filter"));
		if (g_list_length (fc->filters) == 1)
			gtk_combo_box_set_active (GTK_COMBO_BOX (fc->filter_combo), 0);
		gtk_widget_set_visible (fc->filter_combo, TRUE);
	}
	verne_fc_rebuild (fc);
}

GtkFileFilter *
verne_file_chooser_get_filter (gpointer chooser)
{
	VerneFileChooser *fc = verne_fc_get (chooser);
	if (fc)
		return fc->active_filter;
	if (GTK_IS_FILE_CHOOSER (chooser))
		return (gtk_file_chooser_get_filter) (GTK_FILE_CHOOSER (chooser));
	return NULL;
}

static void
verne_fc_build_ui (GtkWidget *dialog, VerneFileChooser *fc)
{
	GtkWidget *content, *box, *nav, *home, *scrolled;

	content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start (box, 12);
	gtk_widget_set_margin_end (box, 12);
	gtk_widget_set_margin_top (box, 12);
	gtk_widget_set_margin_bottom (box, 12);
	gtk_widget_set_hexpand (box, TRUE);
	gtk_widget_set_vexpand (box, TRUE);

	nav = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	fc->up = gtk_button_new_from_icon_name ("go-up", GTK_ICON_SIZE_BUTTON);
	home = gtk_button_new_from_icon_name ("go-home", GTK_ICON_SIZE_BUTTON);
	fc->path = gtk_entry_new ();
	gtk_widget_set_hexpand (fc->path, TRUE);
	g_signal_connect (fc->up, "clicked", G_CALLBACK (on_up), dialog);
	g_signal_connect (home, "clicked", G_CALLBACK (on_home), dialog);
	g_signal_connect (fc->path, "activate", G_CALLBACK (on_path_activate), dialog);
	gtk_box_append (GTK_BOX (nav), fc->up);
	gtk_box_append (GTK_BOX (nav), home);
	gtk_box_append (GTK_BOX (nav), fc->path);

	fc->list = gtk_list_box_new ();
	gtk_list_box_set_selection_mode (GTK_LIST_BOX (fc->list), GTK_SELECTION_SINGLE);
	g_signal_connect (fc->list, "row-selected", G_CALLBACK (on_row_selected), dialog);
	g_signal_connect (fc->list, "row-activated", G_CALLBACK (on_row_activated), dialog);
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled), 280);
	gtk_widget_set_vexpand (scrolled, TRUE);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled), fc->list);

	fc->filter_combo = gtk_combo_box_text_new ();
	gtk_widget_set_visible (fc->filter_combo, FALSE);
	g_signal_connect (fc->filter_combo, "changed", G_CALLBACK (on_filter_changed), dialog);

	gtk_box_append (GTK_BOX (box), nav);
	gtk_box_append (GTK_BOX (box), scrolled);
	gtk_box_append (GTK_BOX (box), fc->filter_combo);
	gtk_box_append (GTK_BOX (content), box);
}

GtkWidget *
verne_file_chooser_dialog_new (const char *title, GtkWindow *parent,
			       GtkFileChooserAction action,
			       const char *first_button_text, ...)
{
	GtkWidget *dialog;
	VerneFileChooser *fc;
	va_list args;
	const char *text;
	GFile *home;

	dialog = g_object_new (GTK_TYPE_DIALOG,
			       "title", title ? title : _("Select a file"),
			       "transient-for", parent,
			       "modal", TRUE,
			       "use-header-bar", TRUE,
			       NULL);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 640, 480);
	fc = g_new0 (VerneFileChooser, 1);
	fc->action = action;
	fc->accept_response = GTK_RESPONSE_ACCEPT;
	g_object_set_qdata_full (G_OBJECT (dialog), verne_fc_quark (), fc, verne_fc_free);

	va_start (args, first_button_text);
	for (text = first_button_text; text != NULL; text = va_arg (args, const char *)) {
		gint response = va_arg (args, int);
		gtk_dialog_add_button (GTK_DIALOG (dialog), text, response);
		if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_ACCEPT)
			fc->accept_response = response;
	}
	va_end (args);

	verne_fc_build_ui (dialog, fc);
	home = g_file_new_for_path (g_get_home_dir ());
	verne_fc_go (fc, home);
	g_object_unref (home);
	return dialog;
}
