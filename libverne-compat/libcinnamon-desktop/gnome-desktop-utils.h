#ifndef GNOME_DESKTOP_UTILS_H
#define GNOME_DESKTOP_UTILS_H
#include <glib.h>
static inline char *gnome_desktop_prepend_terminal_to_command_line (const char *cmd) {
	return g_strdup_printf ("x-terminal-emulator -e %s", cmd ? cmd : "");
}
#endif
