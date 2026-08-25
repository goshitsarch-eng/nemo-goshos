/* Verne GTK4 / Adwaita compatibility — GTK3 APIs implemented on GTK4.
 * Internal symbols stay Nemo* for extension parity; UI branding is Verne.
 */
#ifndef VERNE_GTK_COMPAT_H
#define VERNE_GTK_COMPAT_H

#include <gtk/gtk.h>
#include <adwaita.h>
#include <atk/atk.h>
#include <gdk/gdk.h>
#include <cairo.h>
#include <pango/pango.h>

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

#ifndef GDK_WINDOW_STATE_MAXIMIZED
#define GDK_WINDOW_STATE_MAXIMIZED GDK_TOPLEVEL_STATE_MAXIMIZED
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
} VerneGdkEvent;

#define GdkWindow GdkSurface
#define GDK_WINDOW GDK_SURFACE
#define GDK_IS_WINDOW GDK_IS_SURFACE
#define GDK_TYPE_WINDOW GDK_TYPE_SURFACE
#define gdk_window_get_display gdk_surface_get_display
#define gdk_window_get_width gdk_surface_get_width
#define gdk_window_get_height gdk_surface_get_height
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
void gtk_container_add (GtkWidget *container, GtkWidget *child);
void gtk_container_remove (GtkWidget *container, GtkWidget *child);
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
	gpointer (* get_path_for_child) (GtkContainer *container, GtkWidget *child);
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
#define gtk_widget_event(w,e) FALSE

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

/* image helpers: GTK4 dropped icon-size argument */
static inline GtkWidget *
verne_gtk_image_new_from_icon_name (const char *name, int size)
{
	GtkWidget *image = (gtk_image_new_from_icon_name) (name);
	if (size >= 48)
		gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
	else if (size > 0)
		gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_NORMAL);
	return image;
}
#define gtk_image_new_from_icon_name(name, size) verne_gtk_image_new_from_icon_name (name, size)

GtkWidget *gtk_image_new_from_stock (const gchar *stock_id, int size);
void gtk_image_set_from_stock (GtkImage *image, const gchar *stock_id, int size);
void gtk_button_set_image (GtkButton *button, GtkWidget *image);
GtkWidget *gtk_button_get_image (GtkButton *button);
GtkWidget *gtk_button_new_from_stock (const gchar *stock_id);

#ifndef GTK_ICON_SIZE_MENU
#define GTK_ICON_SIZE_MENU GTK_ICON_SIZE_INHERIT
#define GTK_ICON_SIZE_BUTTON GTK_ICON_SIZE_NORMAL
#define GTK_ICON_SIZE_SMALL_TOOLBAR GTK_ICON_SIZE_NORMAL
#define GTK_ICON_SIZE_LARGE_TOOLBAR GTK_ICON_SIZE_LARGE
#define GTK_ICON_SIZE_DND GTK_ICON_SIZE_LARGE
#define GTK_ICON_SIZE_DIALOG GTK_ICON_SIZE_LARGE
#endif

/* dialog run (nested loop, GTK3 behavior) */
gint gtk_dialog_run (GtkDialog *dialog);

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
};

struct _GtkActionClass {
	GObjectClass parent_class;
	void (* activate) (GtkAction *action);
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
#define GTK_BIN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_BIN, GtkBin))
#define GTK_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_BIN, GtkBinClass))
#define GTK_IS_BIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_BIN))
#define GTK_IS_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_BIN))
typedef struct _GtkBin { GtkContainer parent; GtkWidget *child; } GtkBin;
typedef struct _GtkBinClass { GtkContainerClass parent_class; } GtkBinClass;
GType gtk_bin_get_type (void);
GtkWidget *gtk_bin_get_child (GtkBin *bin);

#define GTK_TYPE_EVENT_BOX (gtk_event_box_get_type ())
#define GTK_EVENT_BOX(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_EVENT_BOX, GtkEventBox))
#define GTK_IS_EVENT_BOX(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_EVENT_BOX))
typedef struct _GtkEventBox GtkEventBox;
GType gtk_event_box_get_type (void);
GtkWidget *gtk_event_box_new (void);
void gtk_event_box_set_visible_window (GtkEventBox *box, gboolean visible);
void gtk_event_box_set_above_child (GtkEventBox *box, gboolean above);

#define GTK_TYPE_MISC (gtk_misc_get_type ())
#define GTK_MISC(obj) ((GtkMisc *)(obj))
#define GTK_MISC_CLASS(klass) ((GtkMiscClass *)(klass))
#define GTK_IS_MISC(obj) (GTK_IS_WIDGET (obj))
typedef struct _GtkMisc { GtkWidget parent; gfloat xalign, yalign; guint xpad, ypad; } GtkMisc;
typedef struct _GtkMiscClass { GtkWidgetClass parent_class; } GtkMiscClass;
GType gtk_misc_get_type (void);
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
void gtk_drag_source_set (GtkWidget *widget, GdkModifierType start_button_mask, const GtkTargetEntry *targets, gint n_targets, GdkDragAction actions);
void gtk_drag_source_unset (GtkWidget *widget);
void gtk_drag_finish (gpointer context, gboolean success, gboolean del, guint32 time);
GdkDragContext *gtk_drag_begin_with_coordinates (GtkWidget *widget, GtkTargetList *targets, GdkDragAction actions, gint button, GdkEvent *event, gint x, gint y);
void gtk_drag_set_icon_pixbuf (GdkDragContext *context, GdkPixbuf *pixbuf, gint hot_x, gint hot_y);
void gtk_drag_set_icon_name (GdkDragContext *context, const gchar *name, gint hot_x, gint hot_y);
void gtk_drag_set_icon_default (GdkDragContext *context);
void gtk_drag_set_icon_widget (GdkDragContext *context, GtkWidget *widget, gint hot_x, gint hot_y);
GtkWidget *gtk_drag_get_source_widget (GdkDragContext *context);
gboolean gtk_drag_check_threshold (GtkWidget *widget, gint start_x, gint start_y, gint current_x, gint current_y);

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
#define gtk_window_move(w,x,y) ((void)0)
#define gtk_window_get_position(w,x,y) G_STMT_START { if (x) *(x)=0; if (y) *(y)=0; } G_STMT_END
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

gboolean verne_widget_chain_button_press (gpointer parent_class, GtkWidget *widget, GdkEventButton *event);
gboolean verne_widget_chain_button_release (gpointer parent_class, GtkWidget *widget, GdkEventButton *event);
gboolean verne_widget_chain_motion (gpointer parent_class, GtkWidget *widget, GdkEventMotion *event);
gboolean verne_widget_chain_key_press (gpointer parent_class, GtkWidget *widget, GdkEventKey *event);
gboolean verne_widget_chain_key_release (gpointer parent_class, GtkWidget *widget, GdkEventKey *event);
gboolean verne_widget_chain_draw (gpointer parent_class, GtkWidget *widget, cairo_t *cr);
void verne_widget_chain_size_allocate (gpointer parent_class, GtkWidget *widget, GtkAllocation *allocation);
void verne_widget_chain_destroy (gpointer parent_class, GtkWidget *widget);
void verne_widget_chain_show (gpointer parent_class, GtkWidget *widget);

void verne_compat_init (void);

/* color selection leftover */
#define gtk_widget_override_background_color(w,s,c) ((void)0)
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
	return (gtk_button_new_from_icon_name) (name);
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
#define GDK_PROPERTY_CHANGE_MASK ((GdkEventMask) 0)
#define GDK_VISIBILITY_NOTIFY_MASK ((GdkEventMask) 0)
#define GDK_PROXIMITY_IN_MASK ((GdkEventMask) 0)
#define GDK_PROXIMITY_OUT_MASK ((GdkEventMask) 0)
#define GDK_SUBSTRUCTURE_MASK ((GdkEventMask) 0)
#define GDK_SCROLL_MASK ((GdkEventMask) 0)
#define GDK_TOUCH_MASK ((GdkEventMask) 0)
#define GDK_SMOOTH_SCROLL_MASK ((GdkEventMask) 0)
#define GDK_ALL_EVENTS_MASK ((GdkEventMask) 0)
#endif
#define gdk_window_set_events(w, e) ((void) 0)
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
#define gtk_widget_get_parent_window(w) gtk_widget_get_window (gtk_widget_get_parent (w))
#define gdk_window_get_origin(w, x, y) G_STMT_START { if (x) *(x)=0; if (y) *(y)=0; } G_STMT_END
#define gdk_window_get_toplevel(w) (w)
#define gdk_cairo_get_clip_rectangle(cr, r) (cairo_clip_extents (cr, NULL, NULL, NULL, NULL), TRUE)
#define gtk_style_context_get_background_color(ctx, state, rgba) ((void)0)
#define gtk_widget_set_realized(w, b) ((void)0)
#define gtk_widget_get_visual(w) NULL
#define GDK_INPUT_OUTPUT 0
#define GDK_WINDOW_CHILD 0
#define GDK_WA_X 0
#define GDK_WA_Y 0
#define GDK_WA_VISUAL 0
typedef struct { gint x, y, width, height, wclass, window_type, event_mask; gpointer visual; } GdkWindowAttr;

void gtk_misc_get_padding (GtkMisc *misc, gint *xpad, gint *ypad);
void gtk_style_context_get (GtkStyleContext *context, GtkStateFlags state, ...) G_GNUC_NULL_TERMINATED;
#define gtk_render_insertion_cursor(ctx, cr, x, y, layout, index, dir) ((void)0)
#define gdk_keymap_get_default() NULL
#define gdk_keymap_get_direction(k) PANGO_DIRECTION_LTR

static inline void
verne_gtk_style_context_get_color (GtkStyleContext *context, GtkStateFlags state, GdkRGBA *color)
{
	(void) state;
	(gtk_style_context_get_color) (context, color);
}
#define gtk_style_context_get_color(c, s, r) verne_gtk_style_context_get_color (c, s, r)

G_END_DECLS

#endif /* VERNE_GTK_COMPAT_H */
