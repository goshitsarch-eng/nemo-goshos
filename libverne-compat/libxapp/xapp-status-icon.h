#ifndef XAPP_STATUS_ICON_H
#define XAPP_STATUS_ICON_H
#include <glib-object.h>
#define XAPP_TYPE_STATUS_ICON (xapp_status_icon_get_type())
G_DECLARE_FINAL_TYPE (XAppStatusIcon, xapp_status_icon, XAPP, STATUS_ICON, GObject)
XAppStatusIcon *xapp_status_icon_new (void);
void xapp_status_icon_set_visible (XAppStatusIcon *icon, gboolean visible);
void xapp_status_icon_set_icon_name (XAppStatusIcon *icon, const gchar *name);
void xapp_status_icon_set_tooltip_text (XAppStatusIcon *icon, const gchar *text);
#endif
