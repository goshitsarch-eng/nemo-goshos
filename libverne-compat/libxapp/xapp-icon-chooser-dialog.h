#ifndef XAPP_ICON_CHOOSER_DIALOG_H
#define XAPP_ICON_CHOOSER_DIALOG_H
#include <gtk/gtk.h>
#define XAPP_TYPE_ICON_CHOOSER_DIALOG GTK_TYPE_DIALOG
#define XAPP_ICON_CHOOSER_DIALOG(o) GTK_DIALOG(o)
GtkWidget *xapp_icon_chooser_dialog_new (void);
void xapp_icon_chooser_dialog_add_button (GtkDialog *dialog, GtkWidget *button, GtkResponseType response, GtkResponseType default_response);
gint xapp_icon_chooser_dialog_run (GtkDialog *dialog);
gint xapp_icon_chooser_dialog_run_with_icon (GtkDialog *dialog, const gchar *icon);
gchar *xapp_icon_chooser_dialog_get_icon_string (GtkDialog *dialog);
#endif
