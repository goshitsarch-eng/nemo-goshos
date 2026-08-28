/* Verne GTK4 / Adwaita compatibility — GTK3 APIs implemented on GTK4.
 * Internal symbols stay Nemo* for extension parity; UI branding is Verne.
 */
#ifndef VERNE_GTK_COMPAT_H
#define VERNE_GTK_COMPAT_H

/* Search helpers and other non-GTK translation units still get -include'd. */
#if defined(__has_include) && !__has_include(<gtk/gtk.h>)
#define VERNE_GTK_COMPAT_SKIP 1
#endif

#ifndef VERNE_GTK_COMPAT_SKIP

#include <gtk/gtk.h>
#include <adwaita.h>
#include <atk/atk.h>
#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#include <cairo.h>
#include <pango/pango.h>
#include <pwd.h>

G_BEGIN_DECLS

typedef void (*GtkCallback) (GtkWidget *widget, gpointer data);
typedef gchar *(*GtkTranslateFunc) (const gchar *path, gpointer func_data);
typedef struct _GtkAccelGroup GtkAccelGroup;
typedef struct _GtkAccelGroupClass GtkAccelGroupClass;
typedef struct _GdkDrag GdkDragContext;
typedef struct _GtkClipboard GtkClipboard;
typedef gchar *GdkAtom;

typedef enum { GTK_ACCEL_VISIBLE = 1 << 0, GTK_ACCEL_LOCKED = 1 << 1, GTK_ACCEL_MASK = 0x07 } GtkAccelFlags;

typedef struct {
	guint accel_key;
	GdkModifierType accel_mods;
	guint accel_flags : 16;
} GtkAccelKey;

typedef struct _GtkAccelMap GtkAccelMap;
typedef struct _GtkAccelMapClass GtkAccelMapClass;
#define GTK_TYPE_ACCEL_MAP (gtk_accel_map_get_type ())
GType gtk_accel_map_get_type (void);
GtkAccelMap *gtk_accel_map_get (void);
void gtk_accel_map_save (const gchar *file_name);
void gtk_accel_map_load (const gchar *file_name);
void gtk_accel_map_add_entry (const gchar *accel_path, guint accel_key, GdkModifierType accel_mods);
gboolean gtk_accel_map_lookup_entry (const gchar *accel_path, GtkAccelKey *key);
gboolean gtk_accel_map_change_entry (const gchar *accel_path, guint accel_key, GdkModifierType accel_mods, gboolean replace);

typedef struct {
	GdkAtom target;
	GdkAtom type;
	gint format;
	guchar *data;
	gint length;
	GdkDisplay *display;
} GtkSelectionData;

typedef void (*GtkClipboardGetFunc) (GtkClipboard *clipboard, GtkSelectionData *selection_data, guint info, gpointer user_data);
typedef void (*GtkClipboardClearFunc) (GtkClipboard *clipboard, gpointer user_data);
typedef void (*GtkClipboardReceivedFunc) (GtkClipboard *clipboard, GtkSelectionData *selection_data, gpointer data);
typedef void (*GtkClipboardTextReceivedFunc) (GtkClipboard *clipboard, const gchar *text, gpointer data);
typedef void (*GtkClipboardTargetsReceivedFunc) (GtkClipboard *clipboard, GdkAtom *atoms, gint n_atoms, gpointer data);

#define VERNE_DISPLAY_NAME "Verne"
#define VERNE_DISPLAY_NAME_NC_ "Verne"

#ifndef GDK_2BUTTON_PRESS
#define GDK_2BUTTON_PRESS ((GdkEventType) 1001)
#endif
#ifndef GDK_3BUTTON_PRESS
#define GDK_3BUTTON_PRESS ((GdkEventType) 1002)
#endif
#ifndef GDK_WINDOW_STATE
#define GDK_WINDOW_STATE ((GdkEventType) 1003)
#endif

#ifndef GTK_STATE_FLAG_PRELIGHT
#define GTK_STATE_FLAG_PRELIGHT GTK_STATE_FLAG_PRELIGHT
#endif

/* --- GTK3 event structs (names unused by GTK4) --- */
typedef cairo_rectangle_int_t GtkAllocation;

typedef struct {
	GdkEventType type;
	GdkSurface *window;
	gint8 send_event;
} GdkEventAny;

typedef struct {
	GdkEventType type;
	GdkSurface *window;
	gint8 send_event;
	guint32 time;
	gdouble x;
	gdouble y;
	gdouble *axes;
	guint state;
	guint button;
	GdkDevice *device;
	gdouble x_root;
	gdouble y_root;
} GdkEventButton;

typedef struct {
	GdkEventType type;
	GdkSurface *window;
	gint8 send_event;
	guint32 time;
	gdouble x;
	gdouble y;
	gdouble *axes;
	guint state;
	gint16 is_hint;
	GdkDevice *device;
	gdouble x_root;
	gdouble y_root;
} GdkEventMotion;

typedef struct {
	GdkEventType type;
	GdkSurface *window;
	gint8 send_event;
	guint32 time;
	guint state;
	guint keyval;
	gint length;
	gchar *string;
	guint16 hardware_keycode;
	guint8 group;
	guint is_modifier : 1;
} GdkEventKey;

typedef struct {
	GdkEventType type;
	GdkSurface *window;
	gint8 send_event;
	GdkSurface *subwindow;
	guint32 time;
	gdouble x;
	gdouble y;
	gdouble x_root;
	gdouble y_root;
	GdkCrossingMode mode;
	GdkNotifyType detail;
	gboolean focus;
	guint state;
} GdkEventCrossing;

typedef struct {
	GdkEventType type;
	GdkSurface *window;
	gint8 send_event;
	gint16 in;
} GdkEventFocus;

typedef struct {
	GdkEventType type;
	GdkSurface *window;
	gint8 send_event;
	guint32 time;
	gdouble x;
	gdouble y;
	guint state;
	GdkScrollDirection direction;
	GdkDevice *device;
	gdouble x_root;
	gdouble y_root;
	gdouble delta_x;
	gdouble delta_y;
} GdkEventScroll;

typedef enum {
	GDK_WINDOW_STATE_WITHDRAWN = 1 << 0,
	GDK_WINDOW_STATE_ICONIFIED = 1 << 1,
	GDK_WINDOW_STATE_MAXIMIZED = 1 << 2,
	GDK_WINDOW_STATE_STICKY = 1 << 3,
	GDK_WINDOW_STATE_FULLSCREEN = 1 << 4,
	GDK_WINDOW_STATE_ABOVE = 1 << 5,
	GDK_WINDOW_STATE_BELOW = 1 << 6,
	GDK_WINDOW_STATE_FOCUSED = 1 << 7,
	GDK_WINDOW_STATE_TILED = 1 << 8
} GdkWindowState;

typedef struct {
	GdkEventType type;
	GdkSurface *window;
	gint8 send_event;
	GdkWindowState changed_mask;
	GdkWindowState new_window_state;
} GdkEventWindowState;

typedef union {
	GdkEventType type;
	GdkEventAny any;
	GdkEventButton button;
	GdkEventMotion motion;
	GdkEventKey key;
	GdkEventCrossing crossing;
	GdkEventFocus focus;
	GdkEventFocus focus_change;
	GdkEventScroll scroll;
	GdkEventWindowState window_state;
} VerneGdkEvent;

/* Complete GTK4's opaque GdkEvent so GTK3 field access compiles.
 * Layout matches the historic GdkEvent union. */
struct _GdkEvent {
	union {
		GdkEventType type;
		GdkEventAny any;
		GdkEventButton button;
		GdkEventMotion motion;
		GdkEventKey key;
		GdkEventCrossing crossing;
		GdkEventFocus focus;
		GdkEventFocus focus_change;
		GdkEventScroll scroll;
		GdkEventWindowState window_state;
	};
};

#define GdkWindow GdkSurface
#define GDK_WINDOW GDK_SURFACE
#define GDK_IS_WINDOW GDK_IS_SURFACE
#define GDK_TYPE_WINDOW GDK_TYPE_SURFACE
#define gdk_window_get_display gdk_surface_get_display
static inline int
verne_gdk_window_get_width (GdkSurface *window)
{
	return (window != NULL && GDK_IS_SURFACE (window)) ? gdk_surface_get_width (window) : 0;
}
static inline int
verne_gdk_window_get_height (GdkSurface *window)
{
	return (window != NULL && GDK_IS_SURFACE (window)) ? gdk_surface_get_height (window) : 0;
}
#define gdk_window_get_width(w) verne_gdk_window_get_width (w)
#define gdk_window_get_height(w) verne_gdk_window_get_height (w)
#define gdk_window_get_scale_factor gdk_surface_get_scale_factor
#define gdk_cairo_set_source_window(cr, win, x, y) ((void)0)

#define GdkScreen GdkDisplay
#define GDK_SCREEN GDK_DISPLAY
#define GDK_TYPE_SCREEN GDK_TYPE_DISPLAY
#define GDK_IS_SCREEN GDK_IS_DISPLAY
#define gdk_screen_get_default gdk_display_get_default
#define gdk_screen_get_display(s) (s)
#define gdk_window_get_screen(w) gdk_surface_get_display(w)
#define gdk_screen_width() verne_screen_width()
#define gdk_screen_height() verne_screen_height()
#define gdk_screen_get_width(s) verne_screen_width()
#define gdk_screen_get_height(s) verne_screen_height()
#define gdk_screen_get_monitor_at_window(s,w) 0
#define gdk_screen_get_n_monitors(s) verne_screen_n_monitors()
#define gdk_screen_get_primary_monitor(s) 0
#define gdk_screen_is_composited(s) TRUE

int verne_screen_width (void);
int verne_screen_height (void);
int verne_screen_n_monitors (void);

/* --- container / packing --- */
void gtk_container_add (gpointer container, GtkWidget *child);
void gtk_container_remove (gpointer container, GtkWidget *child);
void gtk_container_foreach (GtkWidget *container, GtkCallback callback, gpointer callback_data);
GList *gtk_container_get_children (GtkWidget *container);
void gtk_container_set_border_width (GtkWidget *container, guint border_width);
guint gtk_container_get_border_width (GtkWidget *container);
void gtk_container_set_focus_child (GtkWidget *container, GtkWidget *child);
GtkWidget *gtk_container_get_focus_child (GtkWidget *container);
void gtk_container_child_set (GtkWidget *container, GtkWidget *child, const gchar *first_property_name, ...);

typedef struct _GtkContainer {
	GtkWidget parent;
} GtkContainer;

typedef struct _GtkContainerClass {
	GtkWidgetClass parent_class;
	void (* add) (GtkContainer *container, GtkWidget *widget);
	void (* remove) (GtkContainer *container, GtkWidget *widget);
	void (* forall) (GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data);
	void (* set_focus_child) (GtkContainer *container, GtkWidget *child);
	GType (* child_type) (GtkContainer *container);
	gchar * (* composite_name) (GtkContainer *container, GtkWidget *child);
	void (* set_child_property) (GtkContainer *container, GtkWidget *child, guint property_id, const GValue *value, GParamSpec *pspec);
	void (* get_child_property) (GtkContainer *container, GtkWidget *child, guint property_id, GValue *value, GParamSpec *pspec);
	GtkWidget * (* get_path_for_child) (GtkContainer *container, GtkWidget *child);
	unsigned int _handle_border_width : 1;
	gpointer padding[8];
} GtkContainerClass;

GType gtk_container_get_type (void);
#define GTK_TYPE_CONTAINER (gtk_container_get_type ())
#define GTK_CONTAINER(w) ((GtkContainer *)(w))
#define GTK_IS_CONTAINER(w) GTK_IS_WIDGET(w)
#define GTK_CONTAINER_CLASS(klass) ((GtkContainerClass *)(klass))
#define GTK_IS_CONTAINER_CLASS(klass) ((klass) != NULL)
#define GTK_CONTAINER_GET_CLASS(obj) ((GtkContainerClass *)G_OBJECT_GET_CLASS(obj))

void gtk_box_pack_start (GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding);
void gtk_box_pack_end (GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding);
void gtk_box_reorder_child (GtkBox *box, GtkWidget *child, gint position);
void gtk_box_set_child_packing (GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding, GtkPackType pack_type);

void gtk_paned_pack1 (GtkPaned *paned, GtkWidget *child, gboolean resize, gboolean shrink);
void gtk_paned_pack2 (GtkPaned *paned, GtkWidget *child, gboolean resize, gboolean shrink);

void gtk_widget_destroy (GtkWidget *widget);
void gtk_widget_show_all (GtkWidget *widget);
void gtk_widget_set_no_show_all (GtkWidget *widget, gboolean no_show_all);
gboolean gtk_widget_get_no_show_all (GtkWidget *widget);
void verne_gtk_widget_show (GtkWidget *widget);
void verne_gtk_widget_realize (GtkWidget *widget);
#undef gtk_widget_show
#define gtk_widget_show(w) verne_gtk_widget_show (w)
#undef gtk_widget_realize
#define gtk_widget_realize(w) verne_gtk_widget_realize (w)
void gtk_widget_reparent (GtkWidget *widget, GtkWidget *new_parent);
void gtk_widget_get_allocation (GtkWidget *widget, GtkAllocation *allocation);
void gtk_widget_set_allocation (GtkWidget *widget, const GtkAllocation *allocation);
gboolean gtk_widget_intersect (GtkWidget *widget, const GdkRectangle *area, GdkRectangle *intersection);

#define gtk_widget_set_redraw_on_allocate(w,b) ((void)0)
#define gtk_widget_set_has_window(w,b) ((void)0)
#define gtk_widget_get_has_window(w) FALSE
#define gtk_widget_add_events(w,m) ((void)0)
#define gtk_widget_set_events(w,m) ((void)0)
#define gtk_widget_get_events(w) 0
#define gtk_widget_set_app_paintable(w,b) ((void)0)
#define gtk_widget_set_double_buffered(w,b) ((void)0)
#define gtk_widget_input_shape_combine_region(w,r) ((void)0)
#define gtk_widget_shape_combine_region(w,r) ((void)0)

GdkSurface *gtk_widget_get_window (GtkWidget *widget);
gboolean gtk_widget_is_drawable (GtkWidget *widget);
void gtk_widget_get_pointer (GtkWidget *widget, gint *x, gint *y);

#define gtk_cairo_should_draw_window(cr, win) TRUE
#define gtk_cairo_transform_to_window(cr, widget, win) ((void)0)
gboolean gtk_widget_event (GtkWidget *widget, GdkEvent *event);
gboolean gtk_true (void);
gboolean gtk_false (void);
void gtk_widget_destroyed (GtkWidget *widget, GtkWidget **widget_pointer);
void gtk_container_check_resize (gpointer container);

/* gtk_scrolled_window_new lost adj arguments */
static inline GtkWidget *
verne_gtk_scrolled_window_new (void *ha, void *va)
{
	GtkWidget *sw = (gtk_scrolled_window_new) ();
	(void) ha; (void) va;
	return sw;
}
#undef gtk_scrolled_window_new
#define gtk_scrolled_window_new(ha, va) verne_gtk_scrolled_window_new (ha, va)

void gtk_scrolled_window_add_with_viewport (GtkScrolledWindow *sw, GtkWidget *child);

/* Mint xsi-* names map onto freedesktop / Adwaita names. */
static inline const char *
verne_map_icon_name (const char *name)
{
	if (name != NULL && g_str_has_prefix (name, "xsi-"))
		name = name + 4;
	if (g_strcmp0 (name, "view-compact-symbolic") == 0)
		return "view-continuous-symbolic";
	if (g_strcmp0 (name, "view-dual-symbolic") == 0)
		return "view-paged-symbolic";
	if (g_strcmp0 (name, "preview-symbolic") == 0)
		return "image-x-generic-symbolic";
	if (g_strcmp0 (name, "toolbox-symbolic") == 0)
		return "emblem-system-symbolic";
	if (g_strcmp0 (name, "text-case-symbolic") == 0)
		return "insert-text-symbolic";
	if (g_strcmp0 (name, "use-regex-symbolic") == 0)
		return "edit-find-replace-symbolic";
	if (g_strcmp0 (name, "tab-new-symbolic") == 0)
		return "list-add-symbolic";
	if (g_strcmp0 (name, "nemo-sidebar-places-symbolic") == 0)
		return "user-bookmarks-symbolic";
	if (g_strcmp0 (name, "nemo-sidebar-tree-symbolic") == 0)
		return "view-list-ordered-symbolic";
	if (g_strcmp0 (name, "nemo-sidebar-hide-symbolic") == 0)
		return "view-conceal-symbolic";
	if (g_strcmp0 (name, "nemo-sidebar-show-symbolic") == 0)
		return "sidebar-show-symbolic";
	if (g_strcmp0 (name, "favorite-symbolic") == 0)
		return "starred-symbolic";
	if (g_strcmp0 (name, "unfavorite-symbolic") == 0)
		return "non-starred-symbolic";
	if (g_strcmp0 (name, "ok") == 0)
		return "emblem-ok-symbolic";
	if (g_strcmp0 (name, "stop") == 0)
		return "process-stop-symbolic";
	if (g_strcmp0 (name, "drive-harddisk-symbolic") == 0) {
		GdkDisplay *display = gdk_display_get_default ();
		GtkIconTheme *theme = display ? gtk_icon_theme_get_for_display (display) : NULL;
		if (theme && !gtk_icon_theme_has_icon (theme, "drive-harddisk-symbolic")) {
			if (gtk_icon_theme_has_icon (theme, "computer-symbolic"))
				return "computer-symbolic";
			if (gtk_icon_theme_has_icon (theme, "drive-harddisk"))
				return "drive-harddisk";
			return "folder-symbolic";
		}
	}
	return name;
}

/* image helpers: GTK4 dropped icon-size argument */
static inline GtkWidget *
verne_gtk_image_new_from_icon_name (const char *name, int size)
{
	GtkWidget *image = (gtk_image_new_from_icon_name) (verne_map_icon_name (name));
	if (size >= 48)
		gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
	else if (size > 0)
		gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_NORMAL);
	return image;
}
#define gtk_image_new_from_icon_name(name, size) verne_gtk_image_new_from_icon_name (name, size)

static inline void
verne_gtk_image_set_from_icon_name (GtkImage *image, const char *name, int size)
{
	(void) size;
	(gtk_image_set_from_icon_name) (image, verne_map_icon_name (name));
}
#define gtk_image_set_from_icon_name(image, name, ...) verne_gtk_image_set_from_icon_name (image, name, 0)

static inline void
verne_gtk_entry_set_icon_from_icon_name (GtkEntry *entry,
					 GtkEntryIconPosition pos,
					 const char *name)
{
	(gtk_entry_set_icon_from_icon_name) (entry, pos, name ? verne_map_icon_name (name) : NULL);
}
#define gtk_entry_set_icon_from_icon_name(entry, pos, name) \
	verne_gtk_entry_set_icon_from_icon_name ((entry), (pos), (name))

static inline GtkWidget *
verne_gtk_image_new_from_gicon (GIcon *icon, int size)
{
	(void) size;
	return (gtk_image_new_from_gicon) (icon);
}
#define gtk_image_new_from_gicon(icon, ...) verne_gtk_image_new_from_gicon (icon, 0)

static inline void
verne_gtk_widget_size_allocate (GtkWidget *widget, const GtkAllocation *allocation)
{
	GtkAllocation copy = *allocation;
	if (copy.width < 1)
		copy.width = 1;
	if (copy.height < 1)
		copy.height = 1;
	(gtk_widget_size_allocate) (widget, &copy, -1);
}
#define gtk_widget_size_allocate(widget, allocation, ...) verne_gtk_widget_size_allocate (widget, allocation)

static inline void
verne_gtk_style_context_get_padding (GtkStyleContext *context, GtkStateFlags state, GtkBorder *padding)
{
	(void) state;
	(gtk_style_context_get_padding) (context, padding);
}
#define gtk_style_context_get_padding(context, state, padding) verne_gtk_style_context_get_padding (context, state, padding)

gboolean verne_file_chooser_set_current_folder (gpointer chooser, const gchar *filename);
#define gtk_file_chooser_set_current_folder(chooser, filename) verne_file_chooser_set_current_folder (chooser, filename)

void verne_gtk_tree_view_enable_model_drag_source (GtkTreeView *tree_view, GdkModifierType start_button_mask,
						   gconstpointer targets, gint n_targets, GdkDragAction actions);
#define gtk_tree_view_enable_model_drag_source(tv, mask, targets, n, actions) \
	verne_gtk_tree_view_enable_model_drag_source (tv, mask, targets, n, actions)

/* GTK4's dest-row highlight freeze_notify's a missing bin window. Store the
 * path ourselves so list/sidebar drop hit-testing still works. */
void verne_gtk_tree_view_set_drag_dest_row (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewDropPosition pos);
void verne_gtk_tree_view_get_drag_dest_row (GtkTreeView *tree_view, GtkTreePath **path, GtkTreeViewDropPosition *pos);
void verne_tree_view_paint_dest_overlay (GtkWidget *widget, GtkSnapshot *snapshot);
#if defined(gtk_tree_view_set_drag_dest_row)
#undef gtk_tree_view_set_drag_dest_row
#endif
#if defined(gtk_tree_view_get_drag_dest_row)
#undef gtk_tree_view_get_drag_dest_row
#endif
#define gtk_tree_view_set_drag_dest_row(tv, path, pos) verne_gtk_tree_view_set_drag_dest_row ((tv), (path), (pos))
#define gtk_tree_view_get_drag_dest_row(tv, path, pos) verne_gtk_tree_view_get_drag_dest_row ((tv), (path), (pos))

GtkWidget *gtk_image_new_from_stock (const gchar *stock_id, int size);
void gtk_image_set_from_stock (GtkImage *image, const gchar *stock_id, int size);
void gtk_button_set_image (GtkButton *button, GtkWidget *image);
GtkWidget *gtk_button_get_image (GtkButton *button);
GtkWidget *gtk_button_new_from_stock (const gchar *stock_id);

static inline void
verne_gtk_button_set_label (GtkButton *button, const char *label)
{
	if (label == NULL || label[0] == '\0')
		return;
	(gtk_button_set_label) (button, label);
}
#if defined(gtk_button_set_label)
#undef gtk_button_set_label
#endif
#define gtk_button_set_label(b, l) verne_gtk_button_set_label ((b), (l))

#ifndef GTK_ICON_SIZE_MENU
#define GTK_ICON_SIZE_MENU GTK_ICON_SIZE_NORMAL
#define GTK_ICON_SIZE_BUTTON GTK_ICON_SIZE_NORMAL
#define GTK_ICON_SIZE_SMALL_TOOLBAR GTK_ICON_SIZE_NORMAL
#define GTK_ICON_SIZE_LARGE_TOOLBAR GTK_ICON_SIZE_LARGE
#define GTK_ICON_SIZE_DND GTK_ICON_SIZE_LARGE
#define GTK_ICON_SIZE_DIALOG GTK_ICON_SIZE_LARGE
#endif

/* dialog run (nested loop, GTK3 behavior) */
void verne_prepare_dialog (GtkWidget *widget);
gint gtk_dialog_run (GtkDialog *dialog);
const gchar *verne_dialog_button_label (const gchar *text);
GtkWidget *verne_dialog_add_button (GtkDialog *dialog, const gchar *text, gint response);
void verne_dialog_add_buttons (GtkDialog *dialog, const gchar *first_text, ...);
GtkWidget *verne_dialog_new_with_buttons (const gchar *title, GtkWindow *parent, GtkDialogFlags flags,
					  const gchar *first_button_text, ...);
#undef gtk_dialog_add_button
#define gtk_dialog_add_button(d, t, r) verne_dialog_add_button ((d), (t), (r))
#undef gtk_dialog_add_buttons
#define gtk_dialog_add_buttons verne_dialog_add_buttons
#undef gtk_dialog_new_with_buttons
#define gtk_dialog_new_with_buttons verne_dialog_new_with_buttons

/* stock / about */
#define GTK_STOCK_OK "ok"
#define GTK_STOCK_CANCEL "cancel"
#define GTK_STOCK_CLOSE "window-close"
#define GTK_STOCK_APPLY "apply"
#define GTK_STOCK_HELP "help-browser"
#define GTK_STOCK_OPEN "document-open"
#define GTK_STOCK_SAVE "document-save"
#define GTK_STOCK_SAVE_AS "document-save-as"
#define GTK_STOCK_QUIT "application-exit"
#define GTK_STOCK_DIALOG_ERROR "dialog-error"
#define GTK_STOCK_DIALOG_INFO "dialog-information"
#define GTK_STOCK_DIALOG_WARNING "dialog-warning"
#define GTK_STOCK_DIALOG_QUESTION "dialog-question"
#define GTK_STOCK_DIRECTORY "folder"
#define GTK_STOCK_FILE "text-x-generic"
#define GTK_STOCK_HOME "user-home"
#define GTK_STOCK_REFRESH "view-refresh"
#define GTK_STOCK_FIND "edit-find"
#define GTK_STOCK_CUT "edit-cut"
#define GTK_STOCK_COPY "edit-copy"
#define GTK_STOCK_PASTE "edit-paste"
#define GTK_STOCK_DELETE "edit-delete"
#define GTK_STOCK_NEW "document-new"
#define GTK_STOCK_ADD "list-add"
#define GTK_STOCK_REMOVE "list-remove"
#define GTK_STOCK_UNDO "edit-undo"
#define GTK_STOCK_REDO "edit-redo"
#define GTK_STOCK_PROPERTIES "document-properties"
#define GTK_STOCK_PREFERENCES "preferences-system"
#define GTK_STOCK_ABOUT "help-about"
#define GTK_STOCK_EDIT "document-edit"
#define GTK_STOCK_EXECUTE "system-run"
#define GTK_STOCK_STOP "process-stop"
#define GTK_STOCK_YES "gtk-yes"
#define GTK_STOCK_NO "gtk-no"
#define GTK_STOCK_MEDIA_PLAY "media-playback-start"
#define GTK_STOCK_MISSING_IMAGE "image-missing"
#define GTK_STOCK_CLEAR "edit-clear"
#define GTK_STOCK_SELECT_ALL "edit-select-all"
#define GTK_STOCK_ZOOM_IN "zoom-in"
#define GTK_STOCK_ZOOM_OUT "zoom-out"
#define GTK_STOCK_ZOOM_100 "zoom-original"
#define GTK_STOCK_GO_UP "go-up"
#define GTK_STOCK_GO_BACK "go-previous"
#define GTK_STOCK_GO_FORWARD "go-next"
#define GTK_STOCK_GO_HOME "go-home"
#define GTK_STOCK_INFO "dialog-information"
#define GTK_STOCK_DND "dnd-none"
#define GTK_STOCK_DND_MULTIPLE "dnd-multiple"
#define GTK_STOCK_CONVERT "gtk-convert"
#define GTK_STOCK_HARDDISK "drive-harddisk"
#define GTK_STOCK_NETWORK "network-workgroup"
#define GTK_STOCK_PRINT "document-print"
#define GTK_STOCK_INDEX "folder"
#define GTK_STOCK_COLOR_PICKER "color-picker"
#define GTK_STOCK_LEAVE_FULLSCREEN "view-restore"
#define GTK_STOCK_FULLSCREEN "view-fullscreen"

/* --- GtkAction --- */
#define GTK_TYPE_ACTION (gtk_action_get_type ())
#define GTK_ACTION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ACTION, GtkAction))
#define GTK_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ACTION, GtkActionClass))
#define GTK_IS_ACTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ACTION))
#define GTK_IS_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ACTION))
#define GTK_ACTION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ACTION, GtkActionClass))

typedef struct _GtkAction GtkAction;
typedef struct _GtkActionClass GtkActionClass;

struct _GtkAction {
	GObject parent;
	gchar *name;
	gchar *label;
	gchar *short_label;
	gchar *tooltip;
	gchar *stock_id;
	gchar *icon_name;
	GIcon *gicon;
	gboolean sensitive;
	gboolean visible;
	gboolean important;
	gboolean visible_horizontal;
	gboolean visible_vertical;
	gboolean is_important;
	gchar *accelerator;
	gchar *accel_path;
	gpointer accel_group;
};

struct _GtkActionClass {
	GObjectClass parent_class;
	void (* activate) (GtkAction *action);
	GtkWidget *(* create_menu_item) (GtkAction *action);
	GtkWidget *(* create_tool_item) (GtkAction *action);
	void (* connect_proxy) (GtkAction *action, GtkWidget *proxy);
	void (* disconnect_proxy) (GtkAction *action, GtkWidget *proxy);
	GType menu_item_type;
	GType toolbar_item_type;
};

GType gtk_action_get_type (void);
GtkAction *gtk_action_new (const gchar *name, const gchar *label, const gchar *tooltip, const gchar *stock_id);
void gtk_action_activate (GtkAction *action);
const gchar *gtk_action_get_name (GtkAction *action);
void gtk_action_set_sensitive (GtkAction *action, gboolean sensitive);
gboolean gtk_action_get_sensitive (GtkAction *action);
gboolean gtk_action_is_sensitive (GtkAction *action);
void gtk_action_set_visible (GtkAction *action, gboolean visible);
gboolean gtk_action_get_visible (GtkAction *action);
void gtk_action_set_label (GtkAction *action, const gchar *label);
const gchar *gtk_action_get_label (GtkAction *action);
void gtk_action_set_short_label (GtkAction *action, const gchar *label);
const gchar *gtk_action_get_short_label (GtkAction *action);
void gtk_action_set_tooltip (GtkAction *action, const gchar *tooltip);
const gchar *gtk_action_get_tooltip (GtkAction *action);
void gtk_action_set_icon_name (GtkAction *action, const gchar *icon_name);
const gchar *gtk_action_get_icon_name (GtkAction *action);
void gtk_action_set_gicon (GtkAction *action, GIcon *icon);
GIcon *gtk_action_get_gicon (GtkAction *action);
void gtk_action_set_stock_id (GtkAction *action, const gchar *stock_id);
const gchar *gtk_action_get_stock_id (GtkAction *action);
void gtk_action_set_is_important (GtkAction *action, gboolean is_important);
gboolean gtk_action_get_is_important (GtkAction *action);
void gtk_action_set_accel_path (GtkAction *action, const gchar *accel_path);
const gchar *gtk_action_get_accel_path (GtkAction *action);
void gtk_action_set_accel_group (GtkAction *action, gpointer accel_group);
void gtk_action_connect_accelerator (GtkAction *action);
void gtk_action_disconnect_accelerator (GtkAction *action);
void gtk_action_set_visible_horizontal (GtkAction *action, gboolean visible);
GList *gtk_action_get_proxies (GtkAction *action);
void gtk_action_block_activate (GtkAction *action);
void gtk_action_unblock_activate (GtkAction *action);

#define GTK_TYPE_TOGGLE_ACTION (gtk_toggle_action_get_type ())
#define GTK_TOGGLE_ACTION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TOGGLE_ACTION, GtkToggleAction))
#define GTK_IS_TOGGLE_ACTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TOGGLE_ACTION))
typedef struct _GtkToggleAction GtkToggleAction;
typedef struct _GtkToggleActionClass GtkToggleActionClass;
struct _GtkToggleAction { GtkAction parent; gboolean active; };
struct _GtkToggleActionClass { GtkActionClass parent_class; void (* toggled) (GtkToggleAction *action); };
GType gtk_toggle_action_get_type (void);
GtkToggleAction *gtk_toggle_action_new (const gchar *name, const gchar *label, const gchar *tooltip, const gchar *stock_id);
void gtk_toggle_action_set_active (GtkToggleAction *action, gboolean is_active);
gboolean gtk_toggle_action_get_active (GtkToggleAction *action);

#define GTK_TYPE_RADIO_ACTION (gtk_radio_action_get_type ())
#define GTK_RADIO_ACTION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_RADIO_ACTION, GtkRadioAction))
#define GTK_IS_RADIO_ACTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_RADIO_ACTION))
typedef struct _GtkRadioAction GtkRadioAction;
typedef struct _GtkRadioActionClass GtkRadioActionClass;
struct _GtkRadioAction { GtkToggleAction parent; GSList *group; gint value; };
struct _GtkRadioActionClass { GtkToggleActionClass parent_class; void (* changed) (GtkRadioAction *action, GtkRadioAction *current); };
GType gtk_radio_action_get_type (void);
GtkRadioAction *gtk_radio_action_new (const gchar *name, const gchar *label, const gchar *tooltip, const gchar *stock_id, gint value);
void gtk_radio_action_set_group (GtkRadioAction *action, GSList *group);
GSList *gtk_radio_action_get_group (GtkRadioAction *action);
void gtk_radio_action_set_current_value (GtkRadioAction *action, gint value);
gint gtk_radio_action_get_current_value (GtkRadioAction *action);

typedef struct {
	const gchar *name;
	const gchar *stock_id;
	const gchar *label;
	const gchar *accelerator;
	const gchar *tooltip;
	GCallback callback;
} GtkActionEntry;

typedef struct {
	const gchar *name;
	const gchar *stock_id;
	const gchar *label;
	const gchar *accelerator;
	const gchar *tooltip;
	GCallback callback;
	gboolean is_active;
} GtkToggleActionEntry;

typedef struct {
	const gchar *name;
	const gchar *stock_id;
	const gchar *label;
	const gchar *accelerator;
	const gchar *tooltip;
	gint value;
} GtkRadioActionEntry;

#define GTK_TYPE_ACTION_GROUP (gtk_action_group_get_type ())
#define GTK_ACTION_GROUP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ACTION_GROUP, GtkActionGroup))
#define GTK_IS_ACTION_GROUP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ACTION_GROUP))
typedef struct _GtkActionGroup GtkActionGroup;
typedef struct _GtkActionGroupClass { GObjectClass parent_class; } GtkActionGroupClass;
GType gtk_action_group_get_type (void);
GtkActionGroup *gtk_action_group_new (const gchar *name);
const gchar *gtk_action_group_get_name (GtkActionGroup *group);
GtkAction *gtk_action_group_get_action (GtkActionGroup *group, const gchar *action_name);
GList *gtk_action_group_list_actions (GtkActionGroup *group);
void gtk_action_group_add_action (GtkActionGroup *group, GtkAction *action);
void gtk_action_group_add_action_with_accel (GtkActionGroup *group, GtkAction *action, const gchar *accelerator);
void gtk_action_group_remove_action (GtkActionGroup *group, GtkAction *action);
void gtk_action_group_add_actions (GtkActionGroup *group, const GtkActionEntry *entries, guint n_entries, gpointer user_data);
void gtk_action_group_add_toggle_actions (GtkActionGroup *group, const GtkToggleActionEntry *entries, guint n_entries, gpointer user_data);
void gtk_action_group_add_radio_actions (GtkActionGroup *group, const GtkRadioActionEntry *entries, guint n_entries, gint value, GCallback on_change, gpointer user_data);
void gtk_action_group_set_sensitive (GtkActionGroup *group, gboolean sensitive);
gboolean gtk_action_group_get_sensitive (GtkActionGroup *group);
void gtk_action_group_set_visible (GtkActionGroup *group, gboolean visible);
void gtk_action_group_set_translation_domain (GtkActionGroup *group, const gchar *domain);
gchar *(*gtk_action_group_get_translate_func (GtkActionGroup *group)) (const gchar *, gpointer);
void gtk_action_group_set_translate_func (GtkActionGroup *group, GtkTranslateFunc func, gpointer data, GDestroyNotify notify);

/* --- GtkUIManager --- */
#define GTK_TYPE_UI_MANAGER (gtk_ui_manager_get_type ())
#define GTK_UI_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_UI_MANAGER, GtkUIManager))
#define GTK_IS_UI_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_UI_MANAGER))
typedef struct _GtkUIManager GtkUIManager;
typedef struct _GtkUIManagerClass { GObjectClass parent_class; } GtkUIManagerClass;

typedef enum {
	GTK_UI_MANAGER_AUTO = 0,
	GTK_UI_MANAGER_MENUBAR = 1 << 0,
	GTK_UI_MANAGER_MENU = 1 << 1,
	GTK_UI_MANAGER_TOOLBAR = 1 << 2,
	GTK_UI_MANAGER_PLACEHOLDER = 1 << 3,
	GTK_UI_MANAGER_POPUP = 1 << 4,
	GTK_UI_MANAGER_MENUITEM = 1 << 5,
	GTK_UI_MANAGER_TOOLITEM = 1 << 6,
	GTK_UI_MANAGER_SEPARATOR = 1 << 7,
	GTK_UI_MANAGER_ACCELERATOR = 1 << 8,
	GTK_UI_MANAGER_POPUP_WITH_ACCELS = 1 << 9
} GtkUIManagerItemType;

GType gtk_ui_manager_get_type (void);
GtkUIManager *gtk_ui_manager_new (void);
void gtk_ui_manager_insert_action_group (GtkUIManager *self, GtkActionGroup *group, gint pos);
void gtk_ui_manager_remove_action_group (GtkUIManager *self, GtkActionGroup *group);
GList *gtk_ui_manager_get_action_groups (GtkUIManager *self);
guint gtk_ui_manager_add_ui_from_string (GtkUIManager *self, const gchar *buffer, gssize length, GError **error);
guint gtk_ui_manager_add_ui_from_resource (GtkUIManager *self, const gchar *path, GError **error);
guint gtk_ui_manager_add_ui_from_file (GtkUIManager *self, const gchar *filename, GError **error);
void gtk_ui_manager_add_ui (GtkUIManager *self, guint merge_id, const gchar *path, const gchar *name, const gchar *action, GtkUIManagerItemType type, gboolean top);
void gtk_ui_manager_remove_ui (GtkUIManager *self, guint merge_id);
guint gtk_ui_manager_new_merge_id (GtkUIManager *self);
GtkWidget *gtk_ui_manager_get_widget (GtkUIManager *self, const gchar *path);
GtkAction *gtk_ui_manager_get_action (GtkUIManager *self, const gchar *path);
gpointer gtk_ui_manager_get_accel_group (GtkUIManager *self);
void gtk_ui_manager_ensure_update (GtkUIManager *self);
gchar *gtk_ui_manager_get_ui (GtkUIManager *self);
void gtk_ui_manager_set_add_tearoffs (GtkUIManager *self, gboolean add);

/* --- menus / toolbar / statusbar / bin / eventbox / misc / layout --- */
#define GTK_TYPE_MENU (gtk_menu_get_type ())
#define GTK_MENU(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU, GtkMenu))
#define GTK_IS_MENU(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_MENU))
typedef struct _GtkMenu GtkMenu;
GType gtk_menu_get_type (void);
GtkWidget *gtk_menu_new (void);
void gtk_menu_popup_at_pointer (GtkMenu *menu, const GdkEvent *trigger);
void gtk_menu_popup_at_widget (GtkMenu *menu, GtkWidget *widget, GdkGravity widget_anchor, GdkGravity menu_anchor, const GdkEvent *trigger);
void gtk_menu_popup (GtkMenu *menu, GtkWidget *parent_menu_shell, GtkWidget *parent_menu_item, gpointer func, gpointer data, guint button, guint32 activate_time);
void gtk_menu_popdown (GtkMenu *menu);
void gtk_menu_shell_append (gpointer menu_shell, GtkWidget *child);
void gtk_menu_shell_prepend (gpointer menu_shell, GtkWidget *child);
void gtk_menu_shell_insert (gpointer menu_shell, GtkWidget *child, gint position);
void gtk_menu_attach_to_widget (GtkMenu *menu, GtkWidget *attach, gpointer detacher);
GtkWidget *gtk_menu_get_attach_widget (GtkMenu *menu);
GtkWidget *gtk_menu_get_box (GtkMenu *menu);

#define GTK_TYPE_MENU_BAR (gtk_menu_bar_get_type ())
#define GTK_MENU_BAR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU_BAR, GtkMenuBar))
#define GTK_IS_MENU_BAR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_MENU_BAR))
typedef struct _GtkMenuBar GtkMenuBar;
GType gtk_menu_bar_get_type (void);
GtkWidget *gtk_menu_bar_new (void);

#define GTK_TYPE_MENU_ITEM (gtk_menu_item_get_type ())
#define GTK_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU_ITEM, GtkMenuItem))
#define GTK_IS_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_MENU_ITEM))
#define GTK_TYPE_IMAGE_MENU_ITEM GTK_TYPE_MENU_ITEM
#define GTK_IMAGE_MENU_ITEM GTK_MENU_ITEM
#define GTK_IS_IMAGE_MENU_ITEM GTK_IS_MENU_ITEM
typedef struct _GtkMenuItem GtkMenuItem;
GType gtk_menu_item_get_type (void);
GtkWidget *gtk_menu_item_new (void);
GtkWidget *gtk_menu_item_new_with_label (const gchar *label);
GtkWidget *gtk_menu_item_new_with_mnemonic (const gchar *label);
void gtk_menu_item_set_submenu (GtkMenuItem *item, GtkWidget *submenu);
GtkWidget *gtk_menu_item_get_submenu (GtkMenuItem *item);
void gtk_menu_item_set_label (GtkMenuItem *item, const gchar *label);
const gchar *gtk_menu_item_get_label (GtkMenuItem *item);
void gtk_image_menu_item_set_image (GtkMenuItem *item, GtkWidget *image);
GtkWidget *gtk_image_menu_item_get_image (GtkMenuItem *item);
GtkWidget *gtk_image_menu_item_new_with_label (const gchar *label);
GtkWidget *gtk_image_menu_item_new_from_stock (const gchar *stock_id, gpointer accel_group);
void gtk_image_menu_item_set_always_show_image (GtkMenuItem *item, gboolean always_show);

GtkWidget *gtk_separator_menu_item_new (void);
#define GTK_TYPE_SEPARATOR_MENU_ITEM (gtk_separator_menu_item_get_type ())
#define GTK_IS_SEPARATOR_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SEPARATOR_MENU_ITEM))
GType gtk_separator_menu_item_get_type (void);

#define GTK_TYPE_CHECK_MENU_ITEM (gtk_check_menu_item_get_type ())
#define GTK_CHECK_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CHECK_MENU_ITEM, GtkCheckMenuItem))
#define GTK_IS_CHECK_MENU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CHECK_MENU_ITEM))
typedef struct _GtkCheckMenuItem GtkCheckMenuItem;
GType gtk_check_menu_item_get_type (void);
GtkWidget *gtk_check_menu_item_new_with_mnemonic (const gchar *label);
void gtk_check_menu_item_set_active (GtkCheckMenuItem *item, gboolean is_active);
gboolean gtk_check_menu_item_get_active (GtkCheckMenuItem *item);

#define GTK_TYPE_TOOLBAR (gtk_toolbar_get_type ())
#define GTK_TOOLBAR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TOOLBAR, GtkToolbar))
#define GTK_IS_TOOLBAR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TOOLBAR))
typedef struct _GtkToolbar GtkToolbar;
GType gtk_toolbar_get_type (void);
GtkWidget *gtk_toolbar_new (void);
void gtk_toolbar_insert (GtkToolbar *toolbar, GtkWidget *item, gint pos);
void gtk_toolbar_set_icon_size (GtkToolbar *toolbar, GtkIconSize size);
void gtk_toolbar_set_style (GtkToolbar *toolbar, gint style);
void gtk_toolbar_set_show_arrow (GtkToolbar *toolbar, gboolean show);

#define GTK_TYPE_STATUSBAR (gtk_statusbar_get_type ())
#define GTK_STATUSBAR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_STATUSBAR, GtkStatusbar))
#define GTK_IS_STATUSBAR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_STATUSBAR))
typedef struct _GtkStatusbar GtkStatusbar;
GType gtk_statusbar_get_type (void);
GtkWidget *gtk_statusbar_new (void);
guint gtk_statusbar_get_context_id (GtkStatusbar *bar, const gchar *context_description);
guint gtk_statusbar_push (GtkStatusbar *bar, guint context_id, const gchar *text);
void gtk_statusbar_pop (GtkStatusbar *bar, guint context_id);
void gtk_statusbar_remove_all (GtkStatusbar *bar, guint context_id);
GtkWidget *gtk_statusbar_get_message_area (GtkStatusbar *bar);

#define GTK_TYPE_BIN (gtk_bin_get_type ())
#define GTK_BIN(obj) ((GtkBin *)(obj))
#define GTK_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_BIN, GtkBinClass))
#define GTK_IS_BIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_BIN))
#define GTK_IS_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_BIN))
typedef struct _GtkBin { GtkContainer parent; GtkWidget *child; } GtkBin;
typedef struct _GtkBinClass { GtkContainerClass parent_class; } GtkBinClass;
GType gtk_bin_get_type (void);
GtkWidget *gtk_bin_get_child (GtkBin *bin);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkBin, g_object_unref)

#define GTK_TYPE_EVENT_BOX (gtk_event_box_get_type ())
#define GTK_EVENT_BOX(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_EVENT_BOX, GtkEventBox))
#define GTK_IS_EVENT_BOX(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_EVENT_BOX))
typedef struct _GtkEventBox GtkEventBox;
GType gtk_event_box_get_type (void);
GtkWidget *gtk_event_box_new (void);
void gtk_event_box_set_visible_window (GtkEventBox *box, gboolean visible);
void gtk_event_box_set_above_child (GtkEventBox *box, gboolean above);

#define GTK_TYPE_MISC (verne_misc_get_type ())
#define GTK_MISC(obj) ((GtkMisc *)(obj))
#define GTK_MISC_CLASS(klass) ((GtkMiscClass *)(klass))
#define GTK_IS_MISC(obj) (GTK_IS_WIDGET (obj))
typedef struct _GtkMisc { GtkWidget parent; gfloat xalign, yalign; guint xpad, ypad; } GtkMisc;
typedef struct _GtkMiscClass { GtkWidgetClass parent_class; } GtkMiscClass;
GType verne_misc_get_type (void);
#define gtk_misc_get_type verne_misc_get_type
void gtk_misc_set_alignment (GtkMisc *misc, gfloat xalign, gfloat yalign);
void gtk_misc_get_alignment (GtkMisc *misc, gfloat *xalign, gfloat *yalign);
void gtk_misc_set_padding (GtkMisc *misc, gint xpad, gint ypad);

#define GTK_TYPE_LAYOUT (gtk_layout_get_type ())
#define GTK_LAYOUT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_LAYOUT, GtkLayout))
#define GTK_LAYOUT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_LAYOUT, GtkLayoutClass))
#define GTK_IS_LAYOUT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_LAYOUT))
typedef struct _GtkLayout { GtkWidget parent; } GtkLayout;
typedef struct _GtkLayoutClass { GtkWidgetClass parent_class; } GtkLayoutClass;
GType gtk_layout_get_type (void);
GtkWidget *gtk_layout_new (GtkAdjustment *hadjustment, GtkAdjustment *vadjustment);
void gtk_layout_put (GtkLayout *layout, GtkWidget *child, gint x, gint y);
void gtk_layout_move (GtkLayout *layout, GtkWidget *child, gint x, gint y);
void gtk_layout_set_size (GtkLayout *layout, guint width, guint height);
void gtk_layout_get_size (GtkLayout *layout, guint *width, guint *height);
GtkAdjustment *gtk_layout_get_hadjustment (GtkLayout *layout);
GtkAdjustment *gtk_layout_get_vadjustment (GtkLayout *layout);
void gtk_layout_set_hadjustment (GtkLayout *layout, GtkAdjustment *adj);
void gtk_layout_set_vadjustment (GtkLayout *layout, GtkAdjustment *adj);
GdkSurface *gtk_layout_get_bin_window (GtkLayout *layout);

/* GtkHandleBox / GtkTable / GtkAlignment stubs */
#define GTK_TYPE_TABLE GTK_TYPE_GRID
#define GTK_TABLE(w) GTK_GRID(w)
GtkWidget *gtk_table_new (guint rows, guint columns, gboolean homogeneous);
void gtk_table_attach_defaults (GtkGrid *table, GtkWidget *child, guint left, guint right, guint top, guint bottom);
GtkWidget *gtk_alignment_new (gfloat xalign, gfloat yalign, gfloat xscale, gfloat yscale);
void gtk_alignment_set_padding (GtkWidget *alignment, guint top, guint bottom, guint left, guint right);
#define GTK_ALIGNMENT(w) (w)

/* clipboard */
typedef struct _GtkClipboardClass { GObjectClass parent_class; } GtkClipboardClass;
#define GTK_TYPE_CLIPBOARD (gtk_clipboard_get_type ())
GType gtk_clipboard_get_type (void);
#define GDK_NONE ((GdkAtom)0)
#define GDK_SELECTION_CLIPBOARD ((GdkAtom)g_intern_static_string("CLIPBOARD"))
#define GDK_SELECTION_PRIMARY ((GdkAtom)g_intern_static_string("PRIMARY"))
GdkAtom gdk_atom_intern (const gchar *atom_name, gboolean only_if_exists);
gchar *gdk_atom_name (GdkAtom atom);

typedef struct {
	gchar *target;
	guint flags;
	guint info;
} GtkTargetEntry;

typedef struct _GtkTargetList GtkTargetList;
GtkTargetList *gtk_target_list_new (const GtkTargetEntry *targets, guint ntarget);
void gtk_target_list_ref (GtkTargetList *list);
void gtk_target_list_unref (GtkTargetList *list);
void gtk_target_list_add (GtkTargetList *list, GdkAtom target, guint flags, guint info);
void gtk_target_list_add_uri_targets (GtkTargetList *list, guint info);
void gtk_target_list_add_text_targets (GtkTargetList *list, guint info);
void gtk_target_list_add_image_targets (GtkTargetList *list, guint info, gboolean writable);

GtkClipboard *gtk_clipboard_get (GdkAtom selection);
GtkClipboard *gtk_clipboard_get_for_display (GdkDisplay *display, GdkAtom selection);
void gtk_clipboard_set_text (GtkClipboard *clipboard, const gchar *text, gint len);
void gtk_clipboard_set_with_data (GtkClipboard *clipboard, const GtkTargetEntry *targets, guint n_targets,
				  GtkClipboardGetFunc get_func, GtkClipboardClearFunc clear_func, gpointer user_data);
void gtk_clipboard_set_with_owner (GtkClipboard *clipboard, const GtkTargetEntry *targets, guint n_targets,
				   GtkClipboardGetFunc get_func, GtkClipboardClearFunc clear_func, GObject *owner);
void gtk_clipboard_set_can_store (GtkClipboard *clipboard, const GtkTargetEntry *targets, gint n_targets);
void gtk_clipboard_clear (GtkClipboard *clipboard);
void gtk_clipboard_request_contents (GtkClipboard *clipboard, GdkAtom target, GtkClipboardReceivedFunc cb, gpointer data);
void gtk_clipboard_request_text (GtkClipboard *clipboard, GtkClipboardTextReceivedFunc cb, gpointer data);
void gtk_clipboard_request_targets (GtkClipboard *clipboard, GtkClipboardTargetsReceivedFunc cb, gpointer data);
GObject *gtk_clipboard_get_owner (GtkClipboard *clipboard);
GtkSelectionData *gtk_clipboard_wait_for_contents (GtkClipboard *clipboard, GdkAtom target);
gchar *gtk_clipboard_wait_for_text (GtkClipboard *clipboard);

const guchar *gtk_selection_data_get_data (const GtkSelectionData *s);
gint gtk_selection_data_get_length (const GtkSelectionData *s);
GdkAtom gtk_selection_data_get_target (const GtkSelectionData *s);
GdkAtom gtk_selection_data_get_data_type (const GtkSelectionData *s);
gint gtk_selection_data_get_format (const GtkSelectionData *s);
void gtk_selection_data_set (GtkSelectionData *s, GdkAtom type, gint format, const guchar *data, gint length);
void gtk_selection_data_set_uris (GtkSelectionData *s, gchar **uris);
gchar **gtk_selection_data_get_uris (const GtkSelectionData *s);
gboolean gtk_selection_data_targets_include_text (const GtkSelectionData *s);
gboolean gtk_selection_data_targets_include_uri (const GtkSelectionData *s);
GtkSelectionData *gtk_selection_data_copy (const GtkSelectionData *s);
void gtk_selection_data_free (GtkSelectionData *s);

/* drag and drop (minimal GTK3 API) */
#define GDK_ACTION_DEFAULT ((GdkDragAction)0)
#define GTK_DEST_DEFAULT_ALL ((GtkDestDefaults)15)
typedef enum {
	GTK_DEST_DEFAULT_MOTION = 1 << 0,
	GTK_DEST_DEFAULT_HIGHLIGHT = 1 << 1,
	GTK_DEST_DEFAULT_DROP = 1 << 2
} GtkDestDefaults;

void gtk_drag_dest_set (GtkWidget *widget, GtkDestDefaults flags, const GtkTargetEntry *targets, gint n_targets, GdkDragAction actions);
void gtk_drag_dest_unset (GtkWidget *widget);
void gtk_drag_dest_set_target_list (GtkWidget *widget, GtkTargetList *list);
GtkTargetList *gtk_drag_dest_get_target_list (GtkWidget *widget);
void gtk_drag_source_set (GtkWidget *widget, GdkModifierType start_button_mask, const GtkTargetEntry *targets, gint n_targets, GdkDragAction actions);
void gtk_drag_source_unset (GtkWidget *widget);
void gtk_drag_source_set_target_list (GtkWidget *widget, GtkTargetList *list);
void gtk_drag_finish (gpointer context, gboolean success, gboolean del, guint32 time);
GdkDragContext *gtk_drag_begin_with_coordinates (GtkWidget *widget, GtkTargetList *targets, GdkDragAction actions, gint button, GdkEvent *event, gint x, gint y);
void verne_dnd_gesture_end (GtkWidget *widget);
void verne_dnd_local_motion (GtkWidget *widget);
void gtk_drag_set_icon_pixbuf (GdkDragContext *context, GdkPixbuf *pixbuf, gint hot_x, gint hot_y);
void gtk_drag_set_icon_name (GdkDragContext *context, const gchar *name, gint hot_x, gint hot_y);
void gtk_drag_set_icon_default (GdkDragContext *context);
void gtk_drag_set_icon_widget (GdkDragContext *context, GtkWidget *widget, gint hot_x, gint hot_y);
GtkWidget *gtk_drag_get_source_widget (GdkDragContext *context);

/* icon theme */
GtkIconTheme *gtk_icon_theme_get_for_screen (GdkDisplay *screen);
#define gtk_icon_theme_get_default() gtk_icon_theme_get_for_display (gdk_display_get_default ())
GdkPixbuf *gtk_icon_theme_load_icon (GtkIconTheme *theme, const gchar *name, gint size, GtkIconLookupFlags flags, GError **error);

/* style / css */
static inline GtkStyleContext *
verne_gtk_widget_get_style_context (GtkWidget *widget)
{
	GtkStyleContext *ctx = (gtk_widget_get_style_context) (widget);
	g_object_set_data (G_OBJECT (ctx), "verne-widget", widget);
	return ctx;
}
#define gtk_widget_get_style_context(w) verne_gtk_widget_get_style_context (w)

static inline void
verne_gtk_style_context_add_class (GtkStyleContext *ctx, const char *cls)
{
	GtkWidget *w = ctx ? g_object_get_data (G_OBJECT (ctx), "verne-widget") : NULL;
	if (w)
		gtk_widget_add_css_class (w, cls);
}
#undef gtk_style_context_add_class
#define gtk_style_context_add_class(ctx, cls) verne_gtk_style_context_add_class (ctx, cls)

static inline void
verne_gtk_style_context_remove_class (GtkStyleContext *ctx, const char *cls)
{
	GtkWidget *w = ctx ? g_object_get_data (G_OBJECT (ctx), "verne-widget") : NULL;
	if (w)
		gtk_widget_remove_css_class (w, cls);
}
#define gtk_style_context_remove_class(ctx, cls) verne_gtk_style_context_remove_class (ctx, cls)

GtkWidget *gtk_style_context_get_widget_or_null (GtkStyleContext *ctx);
void verne_style_context_bind_widget (GtkWidget *widget);
void gtk_widget_class_install_style_property (GtkWidgetClass *klass, GParamSpec *pspec);
void gtk_widget_style_get (GtkWidget *widget, const gchar *first_property_name, ...);

/* window helpers */
#define gtk_window_set_wmclass(w,a,b) ((void)0)
#define gtk_window_resize(w,width,height) gtk_window_set_default_size(w,width,height)
void gtk_window_move (GtkWindow *window, gint x, gint y);
void gtk_window_get_position (GtkWindow *window, gint *x, gint *y);
#define gtk_window_parse_geometry(w,g) FALSE
#define gtk_window_set_has_resize_grip(w,b) ((void)0)
#define gtk_window_reshow_with_initial_size(w) ((void)0)
#define gtk_window_present_with_time(w,t) gtk_window_present(w)
#define gtk_window_set_startup_id(w,id) ((void)0)

GtkAccelGroup *gtk_accel_group_new (void);
void gtk_window_add_accel_group (GtkWindow *window, GtkAccelGroup *accel);
void gtk_window_remove_accel_group (GtkWindow *window, GtkAccelGroup *accel);

void gtk_widget_add_accelerator (GtkWidget *widget, const gchar *accel_signal, GtkAccelGroup *accel_group,
				 guint accel_key, GdkModifierType accel_mods, GtkAccelFlags accel_flags);

#ifndef GtkAccelFlags_DEFINED
#define GtkAccelFlags_DEFINED
#endif

/* color / visual */
#define gdk_screen_get_rgba_visual(s) NULL
#define gtk_widget_set_visual(w,v) ((void)0)
#define gdk_screen_get_system_visual(s) NULL

/* show_uri */
gboolean gtk_show_uri_on_window (GtkWindow *parent, const char *uri, guint32 timestamp, GError **error);
#define gtk_show_uri(screen, uri, timestamp, error) gtk_show_uri_on_window (NULL, uri, timestamp, error)

/* icon factory / stock no-ops */
#define gtk_icon_factory_add_default(f) ((void)0)
#define gtk_icon_source_new() NULL
#define gtk_icon_set_new() NULL

/* GtkIconSize leftover */
gint gtk_icon_size_lookup (GtkIconSize size, gint *width, gint *height);

/* accessibility stubs */
gpointer gtk_widget_get_accessible (GtkWidget *widget);

/* widget class GTK3 vfunc shims */
typedef gboolean (*VerneButtonEvent) (GtkWidget *widget, GdkEventButton *event);
typedef gboolean (*VerneMotionEvent) (GtkWidget *widget, GdkEventMotion *event);
typedef gboolean (*VerneKeyEvent) (GtkWidget *widget, GdkEventKey *event);
typedef gboolean (*VerneScrollEvent) (GtkWidget *widget, GdkEventScroll *event);
typedef gboolean (*VerneCrossingEvent) (GtkWidget *widget, GdkEventCrossing *event);
typedef gboolean (*VerneFocusEvent) (GtkWidget *widget, GdkEventFocus *event);
typedef gboolean (*VerneDrawEvent) (GtkWidget *widget, cairo_t *cr);
typedef void (*VerneSizeAllocate) (GtkWidget *widget, GtkAllocation *allocation);
typedef void (*VerneDestroyFunc) (GtkWidget *widget);
typedef void (*VernePreferredSize) (GtkWidget *widget, gint *minimum, gint *natural);
typedef gboolean (*VerneDeleteEvent) (GtkWidget *widget, GdkEventAny *event);
typedef gboolean (*VernePopupMenu) (GtkWidget *widget);
typedef void (*VerneStyleUpdated) (GtkWidget *widget);
typedef void (*VerneGrabNotify) (GtkWidget *widget, gboolean was_grabbed);
typedef void (*VerneShowFunc) (GtkWidget *widget);
typedef gboolean (*VerneWindowStateEvent) (GtkWidget *widget, GdkEventWindowState *event);
typedef void (*VerneCellRenderFunc) (GtkCellRenderer *cell, cairo_t *cr, GtkWidget *widget,
				     const GdkRectangle *background_area, const GdkRectangle *cell_area,
				     GtkCellRendererState flags);

void verne_widget_class_set_button_press_event (GtkWidgetClass *klass, VerneButtonEvent handler);
void verne_widget_class_set_button_release_event (GtkWidgetClass *klass, VerneButtonEvent handler);
void verne_widget_class_set_motion_notify_event (GtkWidgetClass *klass, VerneMotionEvent handler);
void verne_widget_class_set_key_press_event (GtkWidgetClass *klass, VerneKeyEvent handler);
void verne_widget_class_set_key_release_event (GtkWidgetClass *klass, VerneKeyEvent handler);
void verne_widget_class_set_scroll_event (GtkWidgetClass *klass, VerneScrollEvent handler);
void verne_widget_class_set_enter_notify_event (GtkWidgetClass *klass, VerneCrossingEvent handler);
void verne_widget_class_set_leave_notify_event (GtkWidgetClass *klass, VerneCrossingEvent handler);
void verne_widget_class_set_focus_in_event (GtkWidgetClass *klass, VerneFocusEvent handler);
void verne_widget_class_set_focus_out_event (GtkWidgetClass *klass, VerneFocusEvent handler);
void verne_widget_class_set_draw (GtkWidgetClass *klass, VerneDrawEvent handler);
void verne_widget_class_set_size_allocate (GtkWidgetClass *klass, VerneSizeAllocate handler);
void verne_widget_class_set_destroy (GtkWidgetClass *klass, VerneDestroyFunc handler);
void verne_widget_class_set_get_preferred_width (GtkWidgetClass *klass, VernePreferredSize handler);
void verne_widget_class_set_get_preferred_height (GtkWidgetClass *klass, VernePreferredSize handler);
void verne_widget_class_set_delete_event (GtkWidgetClass *klass, VerneDeleteEvent handler);
void verne_widget_class_set_popup_menu (GtkWidgetClass *klass, VernePopupMenu handler);
void verne_widget_class_set_style_updated (GtkWidgetClass *klass, VerneStyleUpdated handler);
void verne_widget_class_set_grab_notify (GtkWidgetClass *klass, VerneGrabNotify handler);
void verne_widget_class_set_show (GtkWidgetClass *klass, VerneShowFunc handler);
void verne_widget_class_set_configure_event (GtkWidgetClass *klass, gpointer handler);
void verne_widget_class_set_get_accessible (GtkWidgetClass *klass, gpointer handler);
void verne_widget_class_set_window_state_event (GtkWidgetClass *klass, VerneWindowStateEvent handler);
void verne_widget_class_set_hide (GtkWidgetClass *klass, VerneShowFunc handler);
void verne_cell_renderer_class_set_render (GtkCellRendererClass *klass, VerneCellRenderFunc handler);

gboolean verne_widget_chain_button_press (gpointer parent_class, GtkWidget *widget, GdkEventButton *event);
gboolean verne_widget_chain_button_release (gpointer parent_class, GtkWidget *widget, GdkEventButton *event);
gboolean verne_widget_chain_motion (gpointer parent_class, GtkWidget *widget, GdkEventMotion *event);
gboolean verne_widget_chain_key_press (gpointer parent_class, GtkWidget *widget, GdkEventKey *event);
gboolean verne_widget_chain_key_release (gpointer parent_class, GtkWidget *widget, GdkEventKey *event);
gboolean verne_widget_chain_scroll (gpointer parent_class, GtkWidget *widget, GdkEventScroll *event);
gboolean verne_widget_chain_draw (gpointer parent_class, GtkWidget *widget, cairo_t *cr);
void verne_widget_chain_size_allocate (gpointer parent_class, GtkWidget *widget, GtkAllocation *allocation);
void verne_widget_chain_destroy (gpointer parent_class, GtkWidget *widget);
void verne_widget_invoke_destroy (GtkWidget *widget);
void verne_widget_chain_show (gpointer parent_class, GtkWidget *widget);
gboolean verne_widget_chain_focus_in (gpointer parent_class, GtkWidget *widget, GdkEventFocus *event);

void verne_compat_init (void);

/* color selection leftover */
void gtk_widget_override_background_color (GtkWidget *widget, GtkStateFlags state, const GdkRGBA *color);
#define gtk_widget_override_color(w,s,c) ((void)0)
#define gtk_widget_override_font(w,f) ((void)0)

#define gtk_widget_size_request(w,r) gtk_widget_get_preferred_size(w,r,NULL)

/* GtkWindowGroup still exists in GTK4 */

/* GTK3 GtkAccessible was AtkObject; GTK4 GtkAccessible is an interface. */
#undef GTK_TYPE_ACCESSIBLE
#define GTK_TYPE_ACCESSIBLE ATK_TYPE_GOBJECT_ACCESSIBLE
#undef GTK_ACCESSIBLE
#define GTK_ACCESSIBLE ATK_GOBJECT_ACCESSIBLE
#undef GTK_IS_ACCESSIBLE
#define GTK_IS_ACCESSIBLE ATK_IS_GOBJECT_ACCESSIBLE
#undef GTK_ACCESSIBLE_CLASS
#define GTK_ACCESSIBLE_CLASS ATK_GOBJECT_ACCESSIBLE_CLASS
void gtk_accessible_set_widget (gpointer accessible, GtkWidget *widget);
GtkWidget *verne_gtk_accessible_get_widget (gpointer accessible);
#define gtk_accessible_get_widget(a) verne_gtk_accessible_get_widget (a)

/* boxes / paned / separators from GTK2/3 names */
#define gtk_hbox_new(hom, spacing) gtk_box_new (GTK_ORIENTATION_HORIZONTAL, (spacing))
#define gtk_vbox_new(hom, spacing) gtk_box_new (GTK_ORIENTATION_VERTICAL, (spacing))
#define gtk_hpaned_new() gtk_paned_new (GTK_ORIENTATION_HORIZONTAL)
#define gtk_vpaned_new() gtk_paned_new (GTK_ORIENTATION_VERTICAL)
#define gtk_hseparator_new() gtk_separator_new (GTK_ORIENTATION_HORIZONTAL)
#define gtk_vseparator_new() gtk_separator_new (GTK_ORIENTATION_VERTICAL)
#define gtk_hbutton_box_new() gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6)
#define gtk_vbutton_box_new() gtk_box_new (GTK_ORIENTATION_VERTICAL, 6)
#define GTK_BUTTON_BOX(w) (w)
#define GTK_BUTTONBOX_SPREAD 0
#define GTK_BUTTONBOX_EDGE 1
#define GTK_BUTTONBOX_START 2
#define GTK_BUTTONBOX_END 3
#define GTK_BUTTONBOX_CENTER 4
void gtk_button_box_set_layout (gpointer box, gint layout_style);
GtkWidget *gtk_dialog_get_action_area (GtkDialog *dialog);

static inline GtkWidget *
verne_gtk_widget_get_toplevel (GtkWidget *widget)
{
	GtkRoot *root = gtk_widget_get_root (widget);
	return root ? GTK_WIDGET (root) : widget;
}
#define gtk_widget_get_toplevel(w) verne_gtk_widget_get_toplevel (w)
#define gtk_widget_is_toplevel(w) GTK_IS_ROOT (w)
#define gtk_widget_get_screen(w) gtk_widget_get_display (w)
#define gtk_window_get_screen(w) gtk_widget_get_display (GTK_WIDGET (w))

static inline GtkWidget *
verne_gtk_button_new_from_icon_name (const char *name, int size)
{
	(void) size;
	return (gtk_button_new_from_icon_name) (verne_map_icon_name (name));
}
#define gtk_button_new_from_icon_name(name, size) verne_gtk_button_new_from_icon_name (name, size)

static inline gboolean
verne_css_provider_load_from_data (GtkCssProvider *p, const char *data, gssize len, GError **error)
{
	(void) error;
	(gtk_css_provider_load_from_data) (p, data, len);
	return TRUE;
}
#define gtk_css_provider_load_from_data(p, d, l, e) verne_css_provider_load_from_data (p, d, l, e)

#ifndef GDK_EXPOSURE_MASK
typedef guint GdkEventMask;
#define GDK_EXPOSURE_MASK ((GdkEventMask) 0)
#define GDK_POINTER_MOTION_MASK ((GdkEventMask) 0)
#define GDK_POINTER_MOTION_HINT_MASK ((GdkEventMask) 0)
#define GDK_BUTTON_MOTION_MASK ((GdkEventMask) 0)
#define GDK_BUTTON1_MOTION_MASK ((GdkEventMask) 0)
#define GDK_BUTTON2_MOTION_MASK ((GdkEventMask) 0)
#define GDK_BUTTON3_MOTION_MASK ((GdkEventMask) 0)
#define GDK_BUTTON_PRESS_MASK ((GdkEventMask) 0)
#define GDK_BUTTON_RELEASE_MASK ((GdkEventMask) 0)
#define GDK_KEY_PRESS_MASK ((GdkEventMask) 0)
#define GDK_KEY_RELEASE_MASK ((GdkEventMask) 0)
#define GDK_ENTER_NOTIFY_MASK ((GdkEventMask) 0)
#define GDK_LEAVE_NOTIFY_MASK ((GdkEventMask) 0)
#define GDK_FOCUS_CHANGE_MASK ((GdkEventMask) 0)
#define GDK_STRUCTURE_MASK ((GdkEventMask) 0)
#define GDK_PROPERTY_CHANGE_MASK ((GdkEventMask) (1 << 4))
#define GDK_VISIBILITY_NOTIFY_MASK ((GdkEventMask) 0)
#define GDK_PROXIMITY_IN_MASK ((GdkEventMask) 0)
#define GDK_PROXIMITY_OUT_MASK ((GdkEventMask) 0)
#define GDK_SUBSTRUCTURE_MASK ((GdkEventMask) 0)
#define GDK_SCROLL_MASK ((GdkEventMask) 0)
#define GDK_TOUCH_MASK ((GdkEventMask) 0)
#define GDK_SMOOTH_SCROLL_MASK ((GdkEventMask) 0)
#define GDK_ALL_EVENTS_MASK ((GdkEventMask) 0)
#endif
void gdk_window_set_events (GdkSurface *window, GdkEventMask event_mask);
#define gdk_window_get_events(w) ((GdkEventMask) 0)

typedef struct {
	guint32 pixel;
	guint16 red;
	guint16 green;
	guint16 blue;
} GdkColor;
GType gdk_color_get_type (void);
#define GDK_TYPE_COLOR (gdk_color_get_type ())
GdkColor *gdk_color_copy (const GdkColor *color);
void gdk_color_free (GdkColor *color);

#ifndef GtkStateType
#define GtkStateType GtkStateFlags
#endif
#ifndef GTK_STATE_NORMAL
#define GTK_STATE_NORMAL GTK_STATE_FLAG_NORMAL
#define GTK_STATE_ACTIVE GTK_STATE_FLAG_ACTIVE
#define GTK_STATE_PRELIGHT GTK_STATE_FLAG_PRELIGHT
#define GTK_STATE_SELECTED GTK_STATE_FLAG_SELECTED
#define GTK_STATE_INSENSITIVE GTK_STATE_FLAG_INSENSITIVE
#define GTK_STATE_INCONSISTENT GTK_STATE_FLAG_INCONSISTENT
#define GTK_STATE_FOCUSED GTK_STATE_FLAG_FOCUSED
#endif

typedef enum {
	GDK_WINDOW_TYPE_HINT_NORMAL,
	GDK_WINDOW_TYPE_HINT_DIALOG,
	GDK_WINDOW_TYPE_HINT_MENU,
	GDK_WINDOW_TYPE_HINT_TOOLBAR,
	GDK_WINDOW_TYPE_HINT_SPLASHSCREEN,
	GDK_WINDOW_TYPE_HINT_UTILITY,
	GDK_WINDOW_TYPE_HINT_DOCK,
	GDK_WINDOW_TYPE_HINT_DESKTOP,
	GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU,
	GDK_WINDOW_TYPE_HINT_POPUP_MENU,
	GDK_WINDOW_TYPE_HINT_TOOLTIP,
	GDK_WINDOW_TYPE_HINT_NOTIFICATION,
	GDK_WINDOW_TYPE_HINT_COMBO,
	GDK_WINDOW_TYPE_HINT_DND
} GdkWindowTypeHint;

void gtk_window_set_type_hint (GtkWindow *window, GdkWindowTypeHint hint);
GdkWindowTypeHint gtk_window_get_type_hint (GtkWindow *window);
void gtk_window_set_skip_taskbar_hint (GtkWindow *window, gboolean setting);
void gtk_window_set_skip_pager_hint (GtkWindow *window, gboolean setting);
void gtk_grab_add (GtkWidget *widget);
void gtk_grab_remove (GtkWidget *widget);
gboolean gtk_widget_hide_on_delete (GtkWidget *widget);
#define gtk_button_set_relief(b, r) ((void)0)
#define GTK_RELIEF_NONE 0
#define GTK_RELIEF_NORMAL 1
#define gtk_button_set_focus_on_click(b, f) gtk_widget_set_focus_on_click (GTK_WIDGET (b), f)


typedef struct _GtkBindingSet {
	gchar *name;
	GtkWidgetClass *klass;
	GPtrArray *entries;
} GtkBindingSet;

GtkBindingSet *gtk_binding_set_by_class (gpointer class_struct);
GtkBindingSet *gtk_binding_set_find (const gchar *name);
void gtk_binding_entry_add_signal (GtkBindingSet *binding_set, guint keyval, GdkModifierType modifiers,
				   const gchar *signal_name, guint n_args, ...);
void gtk_binding_entry_remove (GtkBindingSet *binding_set, guint keyval, GdkModifierType modifiers);

void verne_widget_class_set_realize (GtkWidgetClass *klass, void (*handler) (GtkWidget *));
void verne_widget_class_set_unrealize (GtkWidgetClass *klass, void (*handler) (GtkWidget *));
void verne_widget_class_set_state_changed (GtkWidgetClass *klass, void (*handler) (GtkWidget *, GtkStateType));
void verne_widget_class_set_screen_changed (GtkWidgetClass *klass, gpointer handler);

#ifndef GDK_MOD1_MASK
#define GDK_MOD1_MASK GDK_ALT_MASK
#endif
#define GTK_STYLE_CLASS_ENTRY "entry"
#define GTK_STYLE_PROPERTY_FONT "font"

typedef enum {
	GDK_GRAB_SUCCESS = 0,
	GDK_GRAB_ALREADY_GRABBED,
	GDK_GRAB_INVALID_TIME,
	GDK_GRAB_NOT_VIEWABLE,
	GDK_GRAB_FROZEN
} GdkGrabStatus;
#define GDK_OWNERSHIP_NONE 0
#define GdkDeviceManager GdkSeat
#define gdk_display_get_device_manager(d) gdk_display_get_default_seat (d)
#define gdk_device_manager_get_client_pointer(m) gdk_seat_get_pointer (m)
#define gdk_device_grab(dev, win, own, owner_events, mask, cursor, time) GDK_GRAB_SUCCESS
#define gdk_device_ungrab(dev, time) ((void)0)
#define gdk_pointer_grab(w, own, mask, confine, cursor, time) GDK_GRAB_SUCCESS
#define gdk_pointer_ungrab(time) ((void)0)
#define gdk_keyboard_grab(w, own, time) GDK_GRAB_SUCCESS
#define gdk_keyboard_ungrab(time) ((void)0)
#define gdk_window_move_resize(w, x, y, width, height) ((void)0)
#define gdk_app_launch_context_set_screen(c, s) ((void)0)
#define gtk_widget_get_parent_window(w) ((w) && gtk_widget_get_parent (w) ? gtk_widget_get_window (gtk_widget_get_parent (w)) : gtk_widget_get_window (w))
gint gdk_window_get_origin (GdkSurface *window, gint *x, gint *y);
#define gdk_window_get_toplevel(w) (w)
#define gdk_cairo_get_clip_rectangle(cr, r) verne_gdk_cairo_get_clip_rectangle (cr, r)
#define gtk_style_context_get_background_color(ctx, state, rgba) G_STMT_START { if (rgba) { (rgba)->red=1; (rgba)->green=1; (rgba)->blue=1; (rgba)->alpha=1; } } G_STMT_END
#define gtk_widget_set_realized(w, b) ((void)0)
#define gtk_widget_get_visual(w) NULL
#define gtk_widget_set_window(w, win) ((void)0)
#define gtk_style_context_set_background(ctx, win) ((void)0)
#define gtk_im_context_set_client_window(c, w) ((void)0)
gboolean verne_im_context_filter_keypress (GtkIMContext *context, GdkEvent *event);
#define gtk_im_context_filter_keypress(c, e) verne_im_context_filter_keypress ((c), (GdkEvent *) (e))
#define GTK_MENU_SHELL(x) ((gpointer)(x))
#define gtk_expander_set_spacing(e, s) ((void)0)
#define gtk_label_set_line_wrap(l, b) gtk_label_set_wrap (l, b)
#define gtk_label_set_line_wrap_mode(l, m) gtk_label_set_wrap_mode (l, m)
#define gtk_window_get_gravity(w) GDK_GRAVITY_NORTH_WEST
#define gtk_tree_view_get_bin_window(tv) gtk_widget_get_window (GTK_WIDGET (tv))
#define gtk_widget_queue_draw_area(w, x, y, width, height) gtk_widget_queue_draw (w)
#define gtk_widget_queue_draw_region(w, r) gtk_widget_queue_draw (w)

#ifndef GDK_INPUT_OUTPUT
#define GDK_INPUT_OUTPUT 0
#endif
#ifndef GDK_INPUT_ONLY
#define GDK_INPUT_ONLY 1
#endif
#ifndef GDK_WINDOW_CHILD
#define GDK_WINDOW_CHILD 0
#endif
#ifndef GDK_WA_X
#define GDK_WA_X (1 << 2)
#define GDK_WA_Y (1 << 3)
#define GDK_WA_VISUAL (1 << 6)
#define GDK_WA_WMCLASS (1 << 7)
#define GDK_WA_NOREDIR (1 << 8)
#define GDK_WA_TYPE_HINT (1 << 9)
#define GDK_WA_CURSOR (1 << 4)
#endif
#ifndef GDK_XTERM
#define GDK_XTERM 152
#endif

typedef struct {
	gint x, y, width, height, wclass, window_type, event_mask;
	gpointer visual;
	GdkCursor *cursor;
	gchar *title;
	gchar *wmclass_name;
	gchar *wmclass_class;
	gboolean override_redirect;
	GdkWindowTypeHint type_hint;
} GdkWindowAttr;

GdkSurface *gdk_window_new (GdkSurface *parent, GdkWindowAttr *attributes, gint attributes_mask);
void gdk_window_destroy (GdkSurface *window);
void gdk_window_show (GdkSurface *window);
void gdk_window_hide (GdkSurface *window);
void gdk_window_set_user_data (GdkSurface *window, gpointer user_data);
void gdk_window_get_user_data (GdkSurface *window, gpointer *data);
void gdk_window_invalidate_rect (GdkSurface *window, const GdkRectangle *rect, gboolean invalidate_children);
void gdk_window_get_geometry (GdkSurface *window, gint *x, gint *y, gint *width, gint *height);
void gdk_window_get_position (GdkSurface *window, gint *x, gint *y);
void gdk_window_move (GdkSurface *window, gint x, gint y);
void gdk_window_resize (GdkSurface *window, gint width, gint height);
void gdk_window_raise (GdkSurface *window);
void gdk_window_lower (GdkSurface *window);
void gdk_window_focus (GdkSurface *window, guint32 timestamp);
void gdk_window_set_cursor (GdkSurface *window, GdkCursor *cursor);
GdkCursor *gdk_cursor_new (gint cursor_type);
GdkCursor *gdk_cursor_new_for_display (GdkDisplay *display, gint cursor_type);
void gtk_window_get_size (GtkWindow *window, gint *width, gint *height);
void gtk_menu_popup_at_rect (GtkMenu *menu, GdkSurface *rect_window, const GdkRectangle *rect,
			     GdkGravity rect_anchor, GdkGravity menu_anchor, const GdkEvent *trigger);
gboolean verne_gdk_cairo_get_clip_rectangle (cairo_t *cr, cairo_rectangle_int_t *rect);
static inline GdkSurface *
verne_gdk_window_get_device_position (GdkSurface *surface, GdkDevice *device, gint *x, gint *y, GdkModifierType *mask)
{
	double dx = 0, dy = 0;
	GdkModifierType m = 0;
	if (surface && device)
		gdk_surface_get_device_position (surface, device, &dx, &dy, &m);
	if (x) *x = (gint) dx;
	if (y) *y = (gint) dy;
	if (mask) *mask = m;
	return surface;
}
#define gdk_window_get_device_position verne_gdk_window_get_device_position

void gtk_selection_data_set_text (GtkSelectionData *s, const gchar *str, gint len);
GtkTargetEntry *gtk_target_table_new_from_list (GtkTargetList *list, gint *n_targets);
void gtk_target_table_free (GtkTargetEntry *targets, gint n_targets);

void gtk_misc_get_padding (GtkMisc *misc, gint *xpad, gint *ypad);
void gtk_style_context_get (GtkStyleContext *context, GtkStateFlags state, ...) G_GNUC_NULL_TERMINATED;

typedef struct _GdkKeymap GdkKeymap;
GType gdk_keymap_get_type (void);
#define GDK_TYPE_KEYMAP (gdk_keymap_get_type ())
#define GDK_IS_KEYMAP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_KEYMAP))
GdkKeymap *gdk_keymap_get_default (void);
GdkKeymap *gdk_keymap_get_for_display (GdkDisplay *display);
PangoDirection gdk_keymap_get_direction (GdkKeymap *keymap);

static inline GtkClipboard *
verne_gtk_widget_get_clipboard (GtkWidget *widget, GdkAtom selection)
{
	(void) widget;
	return gtk_clipboard_get (selection);
}
#if defined(gtk_widget_get_clipboard)
#undef gtk_widget_get_clipboard
#endif
#define gtk_widget_get_clipboard(w, sel) verne_gtk_widget_get_clipboard (w, sel)

#define gtk_im_multicontext_append_menuitems(ctx, shell) verne_im_multicontext_append_menuitems ((ctx), (shell))
void verne_im_multicontext_append_menuitems (gpointer context, gpointer menushell);
void gtk_menu_shell_select_first (gpointer shell, gboolean search_sensitive);
void verne_set_current_event (GtkWidget *widget, const GdkEvent *event);
void verne_clear_current_event (void);
GdkEvent *gtk_get_current_event (void);
GtkWidget *gtk_get_event_widget (GdkEvent *event);
gboolean gtk_get_current_event_state (GdkModifierType *state);
guint32 gtk_get_current_event_time (void);

/* Synthesized GTK3 GdkEvent* structs store type at offset 0. GTK4's
 * gdk_event_get_event_type() expects a GdkEvent GObject and SIGSEGVs. */
static inline gboolean
verne_gdk_event_is_synth (const GdkEvent *event)
{
	return event != NULL && (guint) event->type < 2048u;
}

static inline GdkEventType
verne_gdk_event_get_event_type (const GdkEvent *event)
{
	if (event == NULL)
		return (GdkEventType) 0;
	return ((const GdkEventAny *) event)->type;
}
#if defined(gdk_event_get_event_type)
#undef gdk_event_get_event_type
#endif
#define gdk_event_get_event_type(e) verne_gdk_event_get_event_type ((const GdkEvent *) (e))

static inline guint32
verne_gdk_event_get_time (const GdkEvent *event)
{
	if (event == NULL)
		return GDK_CURRENT_TIME;
	if (verne_gdk_event_is_synth (event)) {
		guint type = (guint) event->type;
		if (type == GDK_BUTTON_PRESS || type == GDK_BUTTON_RELEASE ||
		    type == (guint) GDK_2BUTTON_PRESS || type == (guint) GDK_3BUTTON_PRESS)
			return event->button.time;
		if (type == GDK_KEY_PRESS || type == GDK_KEY_RELEASE)
			return event->key.time;
		if (type == GDK_MOTION_NOTIFY)
			return event->motion.time;
		if (type == GDK_SCROLL)
			return event->scroll.time;
		if (type == GDK_ENTER_NOTIFY || type == GDK_LEAVE_NOTIFY)
			return event->crossing.time;
		return GDK_CURRENT_TIME;
	}
	return (gdk_event_get_time) ((GdkEvent *) event);
}
#if defined(gdk_event_get_time)
#undef gdk_event_get_time
#endif
#define gdk_event_get_time(e) verne_gdk_event_get_time ((const GdkEvent *) (e))

static inline void
verne_gtk_style_context_get_color (GtkStyleContext *context, GtkStateFlags state, GdkRGBA *color)
{
	(void) state;
	(gtk_style_context_get_color) (context, color);
}
#define gtk_style_context_get_color(c, s, r) verne_gtk_style_context_get_color (c, s, r)

#ifndef GdkPoint
typedef struct { gint x; gint y; } GdkPoint;
#endif

typedef struct {
	GtkBoxClass parent_class;
	gint scrollbar_spacing;
	gboolean (* scroll_child) (GtkScrolledWindow *sw, GtkScrollType scroll, gboolean horizontal);
	void (* move_focus_out) (GtkScrolledWindow *sw, GtkDirectionType direction);
} GtkScrolledWindowClass;

#define GTK_SCROLLED_WINDOW_CLASS(k) ((GtkScrolledWindowClass *)(k))

typedef struct {
	GtkBox parent;
	GtkWidget *inner;
} VerneScrolledWindow;
typedef GtkScrolledWindowClass VerneScrolledWindowClass;
GType verne_scrolled_window_get_type (void);
#define VERNE_TYPE_SCROLLED_WINDOW (verne_scrolled_window_get_type ())
#define VERNE_IS_SCROLLED_WINDOW(w) (G_TYPE_CHECK_INSTANCE_TYPE ((w), VERNE_TYPE_SCROLLED_WINDOW))
#define VERNE_SCROLLED_WINDOW(w) ((VerneScrolledWindow *)(w))
GtkWidget *verne_scrolled_window_get_inner (gpointer widget);

static inline GtkScrolledWindow *
verne_to_gtk_sw (gpointer widget)
{
	if (widget != NULL && VERNE_IS_SCROLLED_WINDOW (widget))
		return (GtkScrolledWindow *) VERNE_SCROLLED_WINDOW (widget)->inner;
	return (GtkScrolledWindow *) widget;
}
#undef GTK_SCROLLED_WINDOW
#define GTK_SCROLLED_WINDOW(w) verne_to_gtk_sw (w)
#undef GTK_IS_SCROLLED_WINDOW
#define GTK_IS_SCROLLED_WINDOW(w) ((w) != NULL && (VERNE_IS_SCROLLED_WINDOW (w) || G_TYPE_CHECK_INSTANCE_TYPE ((w), (gtk_scrolled_window_get_type) ())))

#define gtk_scrolled_window_set_policy(sw, h, v) (gtk_scrolled_window_set_policy) (verne_to_gtk_sw (sw), h, v)
#define gtk_scrolled_window_set_hadjustment(sw, a) (gtk_scrolled_window_set_hadjustment) (verne_to_gtk_sw (sw), a)
#define gtk_scrolled_window_set_vadjustment(sw, a) (gtk_scrolled_window_set_vadjustment) (verne_to_gtk_sw (sw), a)
#define gtk_scrolled_window_get_hscrollbar(sw) (gtk_scrolled_window_get_hscrollbar) (verne_to_gtk_sw (sw))
#define gtk_scrolled_window_get_vscrollbar(sw) (gtk_scrolled_window_get_vscrollbar) (verne_to_gtk_sw (sw))
#define gtk_scrolled_window_set_child(sw, c) (gtk_scrolled_window_set_child) (verne_to_gtk_sw (sw), c)
#define gtk_scrolled_window_get_child(sw) (gtk_scrolled_window_get_child) (verne_to_gtk_sw (sw))

typedef struct {
	GtkBox parent;
	GtkWidget *inner;
	GtkWidget *content_area;
	GtkWidget *action_area;
} VerneInfoBar;
typedef struct { GtkBoxClass parent_class; } VerneInfoBarClass;
GType verne_info_bar_get_type (void);
#define VERNE_TYPE_INFO_BAR (verne_info_bar_get_type ())
#define VERNE_IS_INFO_BAR(w) (G_TYPE_CHECK_INSTANCE_TYPE ((w), VERNE_TYPE_INFO_BAR))
#define VERNE_INFO_BAR(w) ((VerneInfoBar *)(w))
GtkWidget *verne_info_bar_get_inner (gpointer widget);

static inline GtkInfoBar *
verne_to_gtk_ib (gpointer widget)
{
	if (widget != NULL && VERNE_IS_INFO_BAR (widget))
		return (GtkInfoBar *) VERNE_INFO_BAR (widget)->inner;
	return (GtkInfoBar *) widget;
}

/* NemoTrashBar and siblings are VerneInfoBar (GtkBox) wrappers. The GTK4
 * GTK_INFO_BAR() cast must not type-check them as GtkInfoBar, and
 * get_content_area must still see the wrapper, so this is a plain cast —
 * native GtkInfoBar methods go through verne_to_gtk_ib(). */
#undef GTK_INFO_BAR
#define GTK_INFO_BAR(w) ((GtkInfoBar *)(gpointer)(w))
#undef GTK_IS_INFO_BAR
#define GTK_IS_INFO_BAR(w) ((w) != NULL && (VERNE_IS_INFO_BAR (w) || G_TYPE_CHECK_INSTANCE_TYPE ((w), (gtk_info_bar_get_type) ())))

GtkWidget *verne_info_bar_new (void);
#define gtk_info_bar_new() verne_info_bar_new ()

#define gtk_info_bar_add_button(b, t, r) (gtk_info_bar_add_button) (verne_to_gtk_ib (b), t, r)
#define gtk_info_bar_set_message_type(b, t) (gtk_info_bar_set_message_type) (verne_to_gtk_ib (b), t)
#define gtk_info_bar_set_response_sensitive(b, r, s) (gtk_info_bar_set_response_sensitive) (verne_to_gtk_ib (b), r, s)
#define gtk_info_bar_add_child(b, w) (gtk_info_bar_add_child) (verne_to_gtk_ib (b), w)
#define gtk_info_bar_add_action_widget(b, w, r) (gtk_info_bar_add_action_widget) (verne_to_gtk_ib (b), w, r)

/* VerneInfoBar is a GtkBox, so the GTK3 GtkInfoBarClass stub must be at least
 * GtkBoxClass-sized. Subclassing it with GtkWidgetClass made NemoTrashBar
 * (and sibling bars) fail GType class-size checks on trash:///. */
typedef struct { GtkBoxClass parent_class; } GtkInfoBarClass;
#define GTK_INFO_BAR_CLASS(k) ((GtkInfoBarClass *)(k))

struct _GtkMenuItem {
	GtkButton parent;
	GtkWidget *submenu;
	GtkWidget *image;
	gchar *label;
};
typedef struct _GtkMenuItemClass {
	GtkButtonClass parent_class;
	void (* activate) (GtkMenuItem *item);
	void (* activate_item) (GtkMenuItem *item);
	void (* toggle_size_request) (GtkMenuItem *item, gint *requisition);
	void (* toggle_size_allocate) (GtkMenuItem *item, gint allocation);
	void (* set_label) (GtkMenuItem *item, const gchar *label);
	const gchar *(* get_label) (GtkMenuItem *item);
	void (* select) (GtkMenuItem *item);
	void (* deselect) (GtkMenuItem *item);
} GtkMenuItemClass;

#ifndef GTK_SHADOW_NONE
typedef enum {
	GTK_SHADOW_NONE,
	GTK_SHADOW_IN,
	GTK_SHADOW_OUT,
	GTK_SHADOW_ETCHED_IN,
	GTK_SHADOW_ETCHED_OUT
} GtkShadowType;
#endif

#define GTK_JUNCTION_TOP (1 << 3)
#define GTK_JUNCTION_LEFT (1 << 0)
#define GTK_JUNCTION_RIGHT (1 << 1)
#define GTK_JUNCTION_BOTTOM (1 << 2)
typedef guint GtkJunctionSides;

#define gtk_entry_set_text(e, t) gtk_editable_set_text (GTK_EDITABLE (e), t)
#define gtk_entry_get_text(e) gtk_editable_get_text (GTK_EDITABLE (e))
#define gtk_widget_set_margin_left(w, m) gtk_widget_set_margin_start (w, m)
#define gtk_widget_set_margin_right(w, m) gtk_widget_set_margin_end (w, m)
#define gtk_widget_push_composite_child() ((void)0)
#define gtk_widget_pop_composite_child() ((void)0)
#define gtk_widget_reset_style(w) ((void)0)
#define gtk_size_group_set_ignore_hidden(g, b) ((void)0)
#define gtk_frame_set_shadow_type(f, s) ((void)0)
#define gtk_scrolled_window_set_shadow_type(sw, s) ((void)0)
#define gtk_window_set_position(w, p) ((void)0)
#define gtk_window_set_screen(w, s) ((void)0)
#define gtk_menu_set_screen(m, s) ((void)0)
#define gtk_style_context_invalidate(c) ((void)0)
#define gtk_style_context_set_junction_sides(c, s) ((void)0)
#define gtk_style_context_reset_widgets(s) ((void)0)
#define gtk_style_context_add_provider_for_screen(s, p, pri) gtk_style_context_add_provider_for_display (gdk_display_get_default (), p, pri)
#define gtk_style_context_remove_provider_for_screen(s, p) gtk_style_context_remove_provider_for_display (gdk_display_get_default (), p)
#define gtk_settings_get_for_screen(s) gtk_settings_get_default ()
#define gtk_button_clicked(b) g_signal_emit_by_name (b, "clicked")
#define gtk_image_menu_item_new_with_mnemonic(l) gtk_menu_item_new_with_mnemonic (l)
#define gtk_check_menu_item_new_with_label(l) gtk_check_menu_item_new_with_mnemonic (l)
#define gtk_icon_theme_append_search_path(t, p) gtk_icon_theme_add_search_path (t, p)
#define gtk_widget_new(type, ...) g_object_new (type, __VA_ARGS__)
static inline GOptionGroup *
verne_gtk_get_option_group (gboolean open_default_display)
{
	/* GTK3's gtk_get_option_group(TRUE) initialized GTK during parse. */
	if (open_default_display) {
		verne_compat_init ();
		adw_init ();
		(gtk_init) ();
	}
	return g_option_group_new ("gtk", "GTK Options", "Show GTK Options", NULL, NULL);
}
#define gtk_get_option_group(open) verne_gtk_get_option_group (open)
#define gtk_css_provider_get_named(name, variant) gtk_css_provider_new ()
#define gtk_message_dialog_set_image(d, i) ((void)0)

GtkWidget *gtk_info_bar_get_content_area (GtkInfoBar *bar);
GtkWidget *gtk_info_bar_get_action_area (GtkInfoBar *bar);
void gtk_window_activate_default (GtkWindow *window);
void gtk_window_set_icon (GtkWindow *window, GdkPixbuf *pixbuf);
void gtk_main (void);
void gtk_main_quit (void);
gboolean gtk_main_iteration (void);
gboolean gtk_main_iteration_do (gboolean blocking);
guint gtk_main_level (void);
void gtk_widget_get_preferred_width (GtkWidget *widget, gint *minimum, gint *natural);
void gtk_widget_get_preferred_height (GtkWidget *widget, gint *minimum, gint *natural);
cairo_surface_t *gdk_cairo_surface_create_from_pixbuf (const GdkPixbuf *pixbuf, int scale, GdkSurface *for_surface);
void gdk_screen_get_monitor_geometry (GdkScreen *screen, int monitor, GdkRectangle *dest);
int gdk_screen_get_monitor_scale_factor (GdkScreen *screen, int monitor);
gchar *gdk_screen_get_monitor_plug_name (GdkScreen *screen, int monitor);
void gdk_monitor_get_workarea (GdkMonitor *monitor, GdkRectangle *workarea);
GdkEvent *gdk_event_copy (const GdkEvent *event);
void gdk_event_free (GdkEvent *event);
GdkEvent *gdk_event_new (GdkEventType type);
GdkAtom gtk_drag_dest_find_target (GtkWidget *widget, GdkDragContext *context, GtkTargetList *list);
void gtk_drag_get_data (GtkWidget *widget, GdkDragContext *context, GdkAtom target, guint32 time);
void gtk_drag_highlight (GtkWidget *widget);
void gtk_drag_unhighlight (GtkWidget *widget);
gpointer gtk_drag_begin (GtkWidget *widget, GtkTargetList *targets, GdkDragAction actions, gint button, GdkEvent *event);
void gtk_drag_set_icon_surface (GdkDragContext *context, cairo_surface_t *surface);
void gtk_drag_dest_set_track_motion (GtkWidget *widget, gboolean track);
GtkTargetList *gtk_drag_source_get_target_list (GtkWidget *widget);
GdkDragAction gdk_drag_context_get_selected_action (GdkDragContext *context);
GdkDragAction gdk_drag_context_get_suggested_action (GdkDragContext *context);
GdkDragAction gdk_drag_context_get_actions (GdkDragContext *context);
GdkSurface *gdk_drag_context_get_source_window (GdkDragContext *context);
void gdk_drag_status (GdkDragContext *context, GdkDragAction action, guint32 time);
gboolean gtk_target_list_find (GtkTargetList *list, GdkAtom target, guint *info);
gboolean gtk_targets_include_text (GdkAtom *targets, gint n_targets);
gboolean gtk_targets_include_uri (GdkAtom *targets, gint n_targets);
guchar *gtk_selection_data_get_text (const GtkSelectionData *s);
void gtk_entry_set_icon_from_stock (GtkEntry *entry, GtkEntryIconPosition pos, const gchar *stock_id);
GtkWidget *gtk_tool_item_new (void);
void gtk_tool_item_set_expand (gpointer item, gboolean expand);
#define GTK_TOOL_ITEM(w) (w)
#define GTK_ACTIVATABLE(w) (w)
#define GTK_IS_ACTIVATABLE(w) FALSE
void gtk_activatable_set_related_action (gpointer activatable, GtkAction *action);
GtkAction *gtk_activatable_get_related_action (gpointer activatable);
void verne_cell_renderer_set_pixbuf (GtkCellRenderer *cell, GdkPixbuf *pixbuf);
void verne_tree_view_column_set_attributes (GtkTreeViewColumn *tree_column,
					    GtkCellRenderer *cell,
					    ...) G_GNUC_NULL_TERMINATED;
#undef gtk_tree_view_column_set_attributes
#define gtk_tree_view_column_set_attributes verne_tree_view_column_set_attributes
void gtk_activatable_set_use_action_appearance (gpointer activatable, gboolean use);
gboolean gtk_activatable_get_use_action_appearance (gpointer activatable);
gboolean gtk_bindings_activate_event (GObject *object, GdkEventKey *event);
void gtk_propagate_event (GtkWidget *widget, GdkEvent *event);
#define GTK_RADIO_BUTTON(w) (GTK_CHECK_BUTTON (w))
#define gtk_radio_button_get_group(b) NULL
GtkWidget *gtk_image_new_from_surface (cairo_surface_t *surface);
void gtk_image_set_from_surface (GtkImage *image, cairo_surface_t *surface);
void gtk_render_icon_surface (GtkStyleContext *context, cairo_t *cr, cairo_surface_t *surface, gdouble x, gdouble y);
void verne_gtk_render_layout (GtkStyleContext *context, cairo_t *cr, double x, double y, PangoLayout *layout);
void verne_gtk_render_background (GtkStyleContext *context, cairo_t *cr, double x, double y, double width, double height);
void verne_gtk_render_frame (GtkStyleContext *context, cairo_t *cr, double x, double y, double width, double height);
void verne_gtk_render_focus (GtkStyleContext *context, cairo_t *cr, double x, double y, double width, double height);
#ifdef gtk_render_layout
#undef gtk_render_layout
#endif
#ifdef gtk_render_background
#undef gtk_render_background
#endif
#ifdef gtk_render_frame
#undef gtk_render_frame
#endif
#ifdef gtk_render_focus
#undef gtk_render_focus
#endif
#define gtk_render_layout verne_gtk_render_layout
#define gtk_render_background verne_gtk_render_background
#define gtk_render_frame verne_gtk_render_frame
#define gtk_render_focus verne_gtk_render_focus
gpointer gtk_icon_theme_lookup_icon_for_scale (GtkIconTheme *theme, const gchar *name, gint size, gint scale, GtkIconLookupFlags flags);
gpointer gtk_icon_theme_lookup_by_gicon_for_scale (GtkIconTheme *theme, GIcon *icon, gint size, gint scale, GtkIconLookupFlags flags);
GdkPixbuf *gtk_icon_info_load_icon (gpointer info, GError **error);
const gchar *gtk_icon_info_get_filename (gpointer info);
GtkIconSize gtk_icon_size_from_name (const gchar *name);
GtkWidget *gtk_menu_shell_get_selected_item (gpointer menu_shell);
void gtk_menu_shell_select_first (gpointer menu_shell, gboolean search_sensitive);
typedef enum {
	GTK_FILE_FILTER_FILENAME = 1 << 0,
	GTK_FILE_FILTER_URI = 1 << 1,
	GTK_FILE_FILTER_DISPLAY_NAME = 1 << 2,
	GTK_FILE_FILTER_MIME_TYPE = 1 << 3
} GtkFileFilterFlags;
typedef struct {
	GtkFileFilterFlags contains;
	const gchar *filename;
	const gchar *uri;
	const gchar *display_name;
	const gchar *mime_type;
	gboolean contains_image;
} GtkFileFilterInfo;
typedef gboolean (*GtkFileFilterFunc) (const GtkFileFilterInfo *info, gpointer data);
void gtk_file_filter_add_custom (GtkFileFilter *filter, GtkFileFilterFlags needed, GtkFileFilterFunc func, gpointer data, GDestroyNotify notify);
gboolean verne_file_filter_accepts_file (GtkFileFilter *filter, GFile *file);
gboolean verne_file_filter_has_custom (GtkFileFilter *filter);
gboolean verne_is_file_chooser (gpointer widget);
GtkWidget *verne_file_chooser_dialog_new (const char *title, GtkWindow *parent,
					  GtkFileChooserAction action,
					  const char *first_button_text, ...) G_GNUC_NULL_TERMINATED;
GFile *verne_file_chooser_get_file (gpointer chooser);
gchar *verne_file_chooser_get_uri (gpointer chooser);
void verne_file_chooser_add_filter (gpointer chooser, GtkFileFilter *filter);
GtkFileFilter *verne_file_chooser_get_filter (gpointer chooser);
gchar *gtk_file_chooser_get_filename (GtkFileChooser *chooser);
#undef gtk_file_chooser_dialog_new
#define gtk_file_chooser_dialog_new verne_file_chooser_dialog_new
#undef gtk_file_chooser_get_file
#define gtk_file_chooser_get_file(c) verne_file_chooser_get_file (c)
#undef gtk_file_chooser_add_filter
#define gtk_file_chooser_add_filter(c, f) verne_file_chooser_add_filter ((c), (f))
#undef gtk_file_chooser_get_filter
#define gtk_file_chooser_get_filter(c) verne_file_chooser_get_filter (c)
GdkSurface *gdk_device_get_window_at_position (GdkDevice *device, gint *x, gint *y);
void gdk_window_set_background_rgba (GdkSurface *window, const GdkRGBA *rgba);
void gdk_window_set_transient_for (GdkSurface *window, GdkSurface *parent);
GdkWindowTypeHint gdk_window_get_type_hint (gpointer window);
cairo_surface_t *gdk_window_create_similar_surface (GdkSurface *window, cairo_content_t content, int w, int h);
cairo_surface_t *gdk_window_create_similar_image_surface (GdkSurface *window, cairo_format_t format, int w, int h, int scale);
void gdk_window_move_to_rect (GdkSurface *window, const GdkRectangle *rect, GdkGravity rect_anchor, GdkGravity window_anchor, GdkAnchorHints hints, int dx, int dy);
void gdk_window_remove_filter (GdkSurface *window, gpointer func, gpointer data);
gboolean gdk_property_get (GdkSurface *window, GdkAtom property, GdkAtom type, gulong offset, gulong length, gint pdelete, GdkAtom *actual_type, gint *actual_format, gint *actual_length, guchar **data);
void gdk_property_change (GdkSurface *window, GdkAtom property, GdkAtom type, gint format, gint mode, const guchar *data, gint nelements);
GdkSurface *gdk_selection_owner_get (GdkAtom selection);
gboolean gdk_display_supports_selection_notification (GdkDisplay *display);
gboolean gdk_screen_get_setting (GdkScreen *screen, const gchar *name, GValue *value);
GList *gdk_screen_get_window_stack (GdkScreen *screen);
unsigned long gdk_x11_get_xatom_by_name (const gchar *name);
struct passwd *gnome_desktop_get_session_user_pwent (void);
void gtk_builder_add_callback_symbols (GtkBuilder *builder, const char *first, ...) G_GNUC_NULL_TERMINATED;
gboolean verne_gtk_builder_add_from_string (GtkBuilder *builder, const gchar *buffer, gssize length, GError **error);
gboolean verne_gtk_builder_add_from_file (GtkBuilder *builder, const gchar *filename, GError **error);
gboolean verne_gtk_builder_add_from_resource (GtkBuilder *builder, const gchar *path, GError **error);
void verne_paint_desktop_wallpaper (GtkWidget *widget, GtkSnapshot *snapshot, int width, int height);
void verne_paint_desktop_wallpaper_cairo (GtkWidget *widget, cairo_t *cr, int width, int height);
gboolean verne_desktop_canvas_snapshot (GtkWidget *widget, GtkSnapshot *snapshot, int width, int height,
					VerneDrawEvent draw,
					void (*emit_draw) (GtkWidget *, cairo_t *));
void verne_gtk_widget_queue_draw (GtkWidget *widget);
#undef gtk_widget_queue_draw
#define gtk_widget_queue_draw(w) verne_gtk_widget_queue_draw (w)
void verne_window_keep_native (GtkWindow *window);
void verne_window_present_keep (GtkWindow *window);
GtkWidget *verne_adw_window_from_body (GtkWidget *body, const char *title, int width, int height);
#define gtk_builder_add_from_string(b, buf, len, err) verne_gtk_builder_add_from_string ((b), (buf), (len), (err))
#define gtk_builder_add_from_file(b, f, err) verne_gtk_builder_add_from_file ((b), (f), (err))
#define gtk_builder_add_from_resource(b, p, err) verne_gtk_builder_add_from_resource ((b), (p), (err))
void verne_accel_group_connect_action (GtkAccelGroup *group, GtkAction *action, const gchar *accelerator);
void verne_accel_group_disconnect_action (GtkAccelGroup *group, GtkAction *action);
void verne_action_group_bind_accels (GtkActionGroup *group, GtkAccelGroup *accel);
void verne_action_group_unbind_accels (GtkActionGroup *group, GtkAccelGroup *accel);
void gtk_style_context_get_border_color (GtkStyleContext *context, GtkStateFlags state, GdkRGBA *color);
void gtk_style_context_get_style (GtkStyleContext *context, ...) G_GNUC_NULL_TERMINATED;

typedef enum {
	GDK_FILTER_CONTINUE,
	GDK_FILTER_TRANSLATE,
	GDK_FILTER_REMOVE
} GdkFilterReturn;
typedef void GdkXEvent;
typedef GdkFilterReturn (*GdkFilterFunc) (GdkXEvent *xevent, GdkEvent *event, gpointer data);
typedef struct { GdkEventType type; } GdkEventOwnerChange;
typedef struct { GdkEventType type; GdkAtom selection; } GdkEventSelection;
typedef struct { gint dummy; } GtkStyle;
typedef struct { gint dummy; } GtkIconInfo;
typedef struct { gint dummy; } GtkActivatable;
typedef struct {
	GTypeInterface g_iface;
	void (* update) (GtkActivatable *activatable, GtkAction *action, const gchar *property_name);
	void (* sync_action_properties) (GtkActivatable *activatable, GtkAction *action);
} GtkActivatableIface;
typedef struct { gint dummy; } GdkVisual;
typedef GtkCheckButton GtkRadioButton;
typedef GtkWidget GtkToolItem;
typedef GtkWidget GtkMenuShell;

#define gtk_widget_path_unref(p) ((void)0)
#define gtk_container_class_handle_border_width(c) ((void)0)
#define gtk_menu_reposition(m) ((void)0)
#define gtk_menu_item_get_reserve_indicator(i) FALSE
#define gdk_monitor_is_primary(m) TRUE
#define gdk_error_trap_push() ((void)0)
#define gdk_error_trap_pop_ignored() ((void)0)
#define GTK_STYLE_CLASS_BUTTON "button"
#define GTK_STYLE_CLASS_RUBBERBAND "rubberband"
#define GTK_STYLE_CLASS_DND "dnd"
#define GDK_TOP_LEFT_CORNER 1
#define GDK_BOTTOM_LEFT_CORNER 2
#define GDK_TOP_RIGHT_CORNER 3
#define GDK_BOTTOM_RIGHT_CORNER 4
#define GTK_TYPE_SEPARATOR_TOOL_ITEM (gtk_separator_get_type ())
#define GTK_TYPE_ACTIVATABLE (gtk_activatable_get_type ())
#define GTK_PACK_DIRECTION_RTL 1
#define GTK_PACK_DIRECTION_BTT 2
GType gtk_activatable_get_type (void);

static inline void
verne_gtk_init (int *argc, char ***argv)
{
	(void) argc; (void) argv;
	verne_compat_init ();
	adw_init ();
	(gtk_init) ();
}
#define gtk_init(argc, argv) verne_gtk_init (argc, argv)

#define gtk_container_add_with_properties(c, child, ...) gtk_container_add (c, child)
gboolean verne_toggle_button_get_active (gpointer button);
void verne_toggle_button_set_active (gpointer button, gboolean active);
#define gtk_toggle_button_get_active(b) verne_toggle_button_get_active (b)
#define gtk_toggle_button_set_active(b, v) verne_toggle_button_set_active ((b), (v))
#define gtk_toggle_button_get_inconsistent(b) FALSE
#define gtk_toggle_button_set_inconsistent(b, v) ((void)0)
/* GTK4 GtkCheckButton is no longer a GtkToggleButton. Nemo still casts. */
#undef GTK_TOGGLE_BUTTON
#define GTK_TOGGLE_BUTTON(o) ((GtkToggleButton *) (gpointer) (o))
#undef GTK_IS_TOGGLE_BUTTON
#define GTK_IS_TOGGLE_BUTTON(o) \
	((o) != NULL && (G_TYPE_CHECK_INSTANCE_TYPE ((o), gtk_toggle_button_get_type ()) || \
			  G_TYPE_CHECK_INSTANCE_TYPE ((o), gtk_check_button_get_type ())))
gboolean gtk_window_propagate_key_event (GtkWindow *window, GdkEventKey *event);
#define gtk_icon_size_register(name, w, h) GTK_ICON_SIZE_INHERIT
void gtk_file_chooser_set_local_only (GtkFileChooser *chooser, gboolean local_only);
gboolean gtk_file_chooser_get_local_only (GtkFileChooser *chooser);
#define gtk_file_chooser_get_uri(c) verne_file_chooser_get_uri (c)
GdkSurface *gdk_screen_get_root_window (GdkScreen *screen);
void gdk_window_add_filter (GdkSurface *window, GdkFilterFunc func, gpointer data);
unsigned long verne_gdk_root_xid (void);
#define GDK_ROOT_WINDOW() verne_gdk_root_xid ()
#define gdk_flush() ((void)0)
#define gdk_display_get_n_monitors(d) verne_screen_n_monitors ()
static inline GdkMonitor *
verne_display_get_monitor (GdkDisplay *d, int n)
{
	GListModel *list;
	GdkMonitor *m;
	if (d == NULL)
		d = gdk_display_get_default ();
	list = d ? gdk_display_get_monitors (d) : NULL;
	if (list == NULL || n < 0 || (guint) n >= g_list_model_get_n_items (list))
		return NULL;
	m = g_list_model_get_item (list, (guint) n);
	if (m)
		g_object_unref (m);
	return m;
}
#define gdk_display_get_monitor(d, n) verne_display_get_monitor (d, n)
#define gdk_device_manager_list_devices(m, t) NULL
#define gdk_screen_get_number(s) 0
#define gtk_tree_view_set_rules_hint(t, b) ((void)0)

static inline gboolean
verne_gtk_tree_view_get_tooltip_context (GtkTreeView *tree_view, gint *x, gint *y, gboolean keyboard_tip,
					 GtkTreeModel **model, GtkTreePath **path, GtkTreeIter *iter)
{
	return (gtk_tree_view_get_tooltip_context) (tree_view, x ? *x : 0, y ? *y : 0,
						    keyboard_tip, model, path, iter);
}
#define gtk_tree_view_get_tooltip_context(tv, x, y, kb, model, path, iter) \
	verne_gtk_tree_view_get_tooltip_context (tv, x, y, kb, model, path, iter)
#define gtk_paned_get_child1(p) gtk_paned_get_start_child (p)
#define gtk_paned_get_child2(p) gtk_paned_get_end_child (p)
void verne_gtk_paned_set_start_child (GtkPaned *paned, GtkWidget *child);
void verne_gtk_paned_set_end_child (GtkPaned *paned, GtkWidget *child);
#define gtk_paned_set_start_child(p, c) verne_gtk_paned_set_start_child ((p), (c))
#define gtk_paned_set_end_child(p, c) verne_gtk_paned_set_end_child ((p), (c))
#define gtk_menu_bar_get_child_pack_direction(m) 0
#define gtk_image_menu_item_get_always_show_image(i) TRUE
void gtk_builder_connect_signals (GtkBuilder *builder, gpointer user_data);
#define gtk_action_set_always_show_image(a, b) ((void)0)
#define gdk_window_get_state(w) 0
gboolean gdk_event_get_scroll_deltas (const GdkEvent *event, gdouble *delta_x, gdouble *delta_y);
gboolean gtk_widget_send_focus_change (GtkWidget *widget, GdkEvent *event);
void gtk_menu_item_set_accel_path (GtkMenuItem *item, const gchar *accel_path);
#define gtk_button_box_new(o) gtk_box_new (o, 6)
#define gtk_activatable_sync_action_properties(a, act) ((void)0)
#define gtk_activatable_do_set_related_action(a, act) gtk_activatable_set_related_action (a, act)
#define GTK_STYLE_CLASS_VIEW "view"
#define GTK_STYLE_CLASS_TOOLBAR "toolbar"
#define GTK_STYLE_CLASS_SIDEBAR "sidebar"
#define GTK_STYLE_CLASS_PRIMARY_TOOLBAR "primary-toolbar"
#define GTK_STYLE_CLASS_LINKED "linked"
#define GTK_STYLE_CLASS_FLAT "flat"
#define GTK_TARGET_SAME_WIDGET (1 << 0)
#define GTK_TARGET_SAME_APP (1 << 1)
#define GTK_TARGET_OTHER_APP (1 << 2)
#define GDK_WATCH 150
#define GDK_HAND2 60
#define GDK_DEVICE_TYPE_MASTER 0
#define GTK_PACK_DIRECTION_LTR 0
#define GTK_ICON_LOOKUP_FORCE_SIZE 0
#define GDK_PROP_MODE_REPLACE 0
#define GDK_PROP_MODE_PREPEND 1
#define GDK_PROP_MODE_APPEND 2
unsigned long verne_x11_get_xid (gpointer window);
#ifndef GTK_WINDOW_TOPLEVEL
#define GTK_WINDOW_TOPLEVEL 0
#define GTK_WINDOW_POPUP 1
#endif
#define GTK_TYPE_TOOL_BUTTON G_TYPE_INVALID
#define GtkImageMenuItem GtkMenuItem
#define GtkPackDirection int
#define GtkWidgetPath GtkWidget
#define gtk_widget_path_new() NULL
#define gtk_widget_path_copy(p) (p)
#define gtk_widget_path_append_with_siblings(p, s, i) 0
#define gtk_widget_path_append_for_widget(p, w) 0
#define gtk_widget_get_path(w) NULL

static inline GtkWidget *
verne_gtk_window_new (int type)
{
	(void) type;
	return (gtk_window_new) ();
}
#if defined(gtk_window_new)
#undef gtk_window_new
#endif
#define gtk_window_new(...) verne_gtk_window_new (0)

G_END_DECLS

#endif /* !VERNE_GTK_COMPAT_SKIP */
#endif /* VERNE_GTK_COMPAT_H */
