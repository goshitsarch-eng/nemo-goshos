#ifndef XAPP_ICON_CHOOSER_DIALOG_H
#define XAPP_ICON_CHOOSER_DIALOG_H
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XAPP_TYPE_ICON_CHOOSER_DIALOG (xapp_icon_chooser_dialog_get_type ())
G_DECLARE_FINAL_TYPE (XAppIconChooserDialog, xapp_icon_chooser_dialog, XAPP, ICON_CHOOSER_DIALOG, GtkDialog)

XAppIconChooserDialog *xapp_icon_chooser_dialog_new (void);
void xapp_icon_chooser_dialog_add_button (XAppIconChooserDialog *dialog,
					  GtkWidget *button,
					  GtkPackType packing,
					  GtkResponseType response_id);
gint xapp_icon_chooser_dialog_run (XAppIconChooserDialog *dialog);
gint xapp_icon_chooser_dialog_run_with_icon (XAppIconChooserDialog *dialog, const gchar *icon);
gchar *xapp_icon_chooser_dialog_get_icon_string (XAppIconChooserDialog *dialog);

G_END_DECLS
#endif
