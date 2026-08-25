#include "config.h"
#include "verne-gtk-compat.h"
#include <atk/atk.h>
#include <stdarg.h>
#include <string.h>

/* ---------- GdkColor boxed type (removed in GTK4) ---------- */
static GdkColor *
gdk_color_copy_impl (const GdkColor *color)
{
	GdkColor *c = g_new (GdkColor, 1);
	*c = *color;
	return c;
}

GType
gdk_color_get_type (void)
{
	static GType type = 0;
	if (type == 0)
		type = g_boxed_type_register_static ("GdkColor",
						     (GBoxedCopyFunc) gdk_color_copy_impl,
						     (GBoxedFreeFunc) gdk_color_free);
	return type;
}

GdkColor *
gdk_color_copy (const GdkColor *color)
{
	return color ? gdk_color_copy_impl (color) : NULL;
}

void
gdk_color_free (GdkColor *color)
{
	g_free (color);
}

/* ---------- style properties ---------- */
static GHashTable *style_props;

void
gtk_widget_class_install_style_property (GtkWidgetClass *klass, GParamSpec *pspec)
{
	GList *list;
	GType type = G_TYPE_FROM_CLASS (klass);

	if (style_props == NULL)
		style_props = g_hash_table_new (g_direct_hash, g_direct_equal);
	list = g_hash_table_lookup (style_props, GSIZE_TO_POINTER (type));
	list = g_list_prepend (list, pspec);
	g_hash_table_insert (style_props, GSIZE_TO_POINTER (type), list);
}

static GParamSpec *
lookup_style_pspec (GtkWidget *widget, const gchar *name)
{
	GType type = G_OBJECT_TYPE (widget);
	while (type != 0 && type != GTK_TYPE_WIDGET) {
		GList *l;
		if (style_props) {
			for (l = g_hash_table_lookup (style_props, GSIZE_TO_POINTER (type)); l; l = l->next) {
				GParamSpec *p = l->data;
				if (g_strcmp0 (p->name, name) == 0)
					return p;
			}
		}
		type = g_type_parent (type);
	}
	return NULL;
}

void
gtk_widget_style_get (GtkWidget *widget, const gchar *first_property_name, ...)
{
	va_list args;
	const gchar *name;

	va_start (args, first_property_name);
	for (name = first_property_name; name != NULL; name = va_arg (args, const gchar *)) {
		GParamSpec *pspec = lookup_style_pspec (widget, name);
		gpointer dest = va_arg (args, gpointer);
		if (dest == NULL)
			continue;
		if (pspec == NULL) {
			if (g_str_has_suffix (name, "color"))
				*(gpointer *) dest = NULL;
			else
				*(gint *) dest = 0;
			continue;
		}
		if (G_IS_PARAM_SPEC_INT (pspec))
			*(gint *) dest = G_PARAM_SPEC_INT (pspec)->default_value;
		else if (G_IS_PARAM_SPEC_BOOLEAN (pspec))
			*(gboolean *) dest = G_PARAM_SPEC_BOOLEAN (pspec)->default_value;
		else if (G_IS_PARAM_SPEC_BOXED (pspec))
			*(gpointer *) dest = NULL;
		else
			*(gint *) dest = 0;
	}
	va_end (args);
}

/* ---------- accessible widget glue ---------- */
static GQuark
accessible_widget_quark (void)
{
	static GQuark q;
	if (!q)
		q = g_quark_from_static_string ("verne-accessible-widget");
	return q;
}

void
gtk_accessible_set_widget (gpointer accessible, GtkWidget *widget)
{
	if (accessible)
		g_object_set_qdata (G_OBJECT (accessible), accessible_widget_quark (), widget);
}

GtkWidget *
verne_gtk_accessible_get_widget (gpointer accessible)
{
	gpointer obj;

	if (accessible == NULL)
		return NULL;
	if (GTK_IS_WIDGET (accessible))
		return GTK_WIDGET (accessible);
	obj = g_object_get_qdata (G_OBJECT (accessible), accessible_widget_quark ());
	if (GTK_IS_WIDGET (obj))
		return GTK_WIDGET (obj);
	obj = g_object_get_qdata (G_OBJECT (accessible), g_quark_from_string ("object-for-accessible"));
	if (GTK_IS_WIDGET (obj))
		return GTK_WIDGET (obj);
	if (ATK_IS_GOBJECT_ACCESSIBLE (accessible)) {
		obj = atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (accessible));
		if (GTK_IS_WIDGET (obj))
			return GTK_WIDGET (obj);
	}
	return NULL;
}

/* ---------- dialog action area ---------- */
GtkWidget *
gtk_dialog_get_action_area (GtkDialog *dialog)
{
	GtkWidget *area = g_object_get_data (G_OBJECT (dialog), "verne-action-area");
	if (area == NULL) {
		area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_halign (area, GTK_ALIGN_END);
		gtk_widget_set_margin_top (area, 6);
		gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (dialog)), area);
		g_object_set_data (G_OBJECT (dialog), "verne-action-area", area);
	}
	return area;
}

void
gtk_button_box_set_layout (gpointer box, gint layout_style)
{
	(void) box;
	(void) layout_style;
}

/* ---------- grabs / type hints / toplevel helpers ---------- */
void
gtk_grab_add (GtkWidget *widget)
{
	g_object_set_data (G_OBJECT (gdk_display_get_default ()), "verne-grab-widget", widget);
}

void
gtk_grab_remove (GtkWidget *widget)
{
	GdkDisplay *d = gdk_display_get_default ();
	if (g_object_get_data (G_OBJECT (d), "verne-grab-widget") == widget)
		g_object_set_data (G_OBJECT (d), "verne-grab-widget", NULL);
}

void
gtk_window_set_type_hint (GtkWindow *window, GdkWindowTypeHint hint)
{
	g_object_set_data (G_OBJECT (window), "verne-type-hint", GINT_TO_POINTER ((int) hint));
}

GdkWindowTypeHint
gtk_window_get_type_hint (GtkWindow *window)
{
	return (GdkWindowTypeHint) GPOINTER_TO_INT (g_object_get_data (G_OBJECT (window), "verne-type-hint"));
}

void
gtk_container_child_set (GtkWidget *container, GtkWidget *child, const gchar *first_property_name, ...)
{
	va_list args;
	const gchar *name;

	va_start (args, first_property_name);
	for (name = first_property_name; name != NULL; name = va_arg (args, const gchar *)) {
		if (GTK_IS_NOTEBOOK (container) && g_strcmp0 (name, "tab-expand") == 0) {
			gboolean expand = va_arg (args, gboolean);
			GtkNotebookPage *page = gtk_notebook_get_page (GTK_NOTEBOOK (container), child);
			if (page)
				g_object_set (page, "tab-expand", expand, NULL);
		} else {
			/* skip one value */
			(void) va_arg (args, gpointer);
		}
	}
	va_end (args);
}

GtkWidget *
gtk_style_context_get_widget_or_null (GtkStyleContext *ctx)
{
	return ctx ? g_object_get_data (G_OBJECT (ctx), "verne-widget") : NULL;
}

void
verne_style_context_bind_widget (GtkWidget *widget)
{
	if (widget)
		g_object_set_data (G_OBJECT (gtk_widget_get_style_context (widget)), "verne-widget", widget);
}

gboolean
gtk_widget_hide_on_delete (GtkWidget *widget)
{
	gtk_widget_set_visible (widget, FALSE);
	return TRUE;
}

void
gtk_window_activate_default (GtkWindow *window)
{
	GtkWidget *focus = gtk_window_get_focus (window);
	if (GTK_IS_BUTTON (focus))
		g_signal_emit_by_name (focus, "clicked");
}

static GHashTable *binding_sets;

GtkBindingSet *
gtk_binding_set_by_class (gpointer class_struct)
{
	GtkBindingSet *set;

	g_return_val_if_fail (class_struct != NULL, NULL);
	if (binding_sets == NULL)
		binding_sets = g_hash_table_new (g_direct_hash, g_direct_equal);
	set = g_hash_table_lookup (binding_sets, class_struct);
	if (set == NULL) {
		set = g_new0 (GtkBindingSet, 1);
		set->klass = class_struct;
		set->name = g_strdup (G_OBJECT_CLASS_NAME (class_struct));
		g_hash_table_insert (binding_sets, class_struct, set);
		if (set->name)
			g_hash_table_insert (binding_sets, set->name, set);
	}
	return set;
}

GtkBindingSet *
gtk_binding_set_find (const gchar *name)
{
	if (binding_sets == NULL || name == NULL)
		return NULL;
	return g_hash_table_lookup (binding_sets, name);
}

void
gtk_binding_entry_add_signal (GtkBindingSet *binding_set, guint keyval, GdkModifierType modifiers,
			      const gchar *signal_name, guint n_args, ...)
{
	va_list args;
	g_return_if_fail (binding_set != NULL && binding_set->klass != NULL);
	va_start (args, n_args);
	if (n_args == 0) {
		gtk_widget_class_add_binding_signal (binding_set->klass, keyval, modifiers, signal_name, NULL);
	} else if (n_args == 1) {
		GType t1 = va_arg (args, GType);
		if (t1 == G_TYPE_BOOLEAN) {
			gboolean v = va_arg (args, gboolean);
			gtk_widget_class_add_binding_signal (binding_set->klass, keyval, modifiers, signal_name, "b", v);
		} else {
			gtk_widget_class_add_binding_signal (binding_set->klass, keyval, modifiers, signal_name, NULL);
			(void) va_arg (args, gpointer);
		}
	} else {
		gtk_widget_class_add_binding_signal (binding_set->klass, keyval, modifiers, signal_name, NULL);
	}
	va_end (args);
}

void
gtk_binding_entry_remove (GtkBindingSet *binding_set, guint keyval, GdkModifierType modifiers)
{
	(void) binding_set;
	(void) keyval;
	(void) modifiers;
}

void
gtk_style_context_get (GtkStyleContext *context, GtkStateFlags state, ...)
{
	va_list args;
	const char *name;
	GtkWidget *widget = gtk_style_context_get_widget_or_null (context);
	(void) state;

	va_start (args, state);
	while ((name = va_arg (args, const char *)) != NULL) {
		gpointer *out = va_arg (args, gpointer *);
		if (out == NULL)
			break;
		if (g_strcmp0 (name, "font") == 0 && widget) {
			PangoFontDescription *desc = pango_font_description_copy (
				pango_context_get_font_description (gtk_widget_get_pango_context (widget)));
			*out = desc;
		} else {
			*out = NULL;
		}
	}
	va_end (args);
}

