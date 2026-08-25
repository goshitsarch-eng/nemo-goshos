#pragma once
#include <gdk/x11/gdkx.h>

#ifndef gdk_x11_window_get_xid
#define gdk_x11_window_get_xid gdk_x11_surface_get_xid
#endif
#ifndef GDK_IS_X11_WINDOW
#define GDK_IS_X11_WINDOW GDK_IS_X11_SURFACE
#endif
#ifndef GDK_X11_WINDOW
#define GDK_X11_WINDOW GDK_X11_SURFACE
#endif
#ifndef gdk_x11_window_lookup_for_display
#define gdk_x11_window_lookup_for_display gdk_x11_surface_lookup_for_display
#endif
