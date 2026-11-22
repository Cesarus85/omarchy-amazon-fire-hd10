/* GTK3 module loaded by the tablet's file portal. Keep header controls
 * reachable when the compositor fits a dialog to the portrait work area. */
#include <gtk/gtk.h>
#include <gmodule.h>

static void fit_child(GtkWidget *widget, gpointer unused)
{
    if (GTK_IS_ENTRY(widget)) {
        GtkEntry *entry = GTK_ENTRY(widget);
        if (gtk_entry_get_width_chars(entry) > 12)
            gtk_entry_set_width_chars(entry, 12);
    }

    if (GTK_IS_SCROLLED_WINDOW(widget) &&
        g_strcmp0(gtk_buildable_get_name(GTK_BUILDABLE(widget)), "browse_files_swin") == 0) {
        GtkScrolledWindow *files = GTK_SCROLLED_WINDOW(widget);
        GtkPolicyType horizontal, vertical;
        gtk_scrolled_window_get_policy(files, &horizontal, &vertical);
        if (horizontal == GTK_POLICY_NEVER)
            gtk_scrolled_window_set_policy(files, GTK_POLICY_AUTOMATIC, vertical);
    }

    if (GTK_IS_CONTAINER(widget))
        gtk_container_forall(GTK_CONTAINER(widget), fit_child, unused);
}

static gboolean dialog_show(GSignalInvocationHint *hint, guint n_values,
                            const GValue *values, gpointer data)
{
    GtkWidget *window = g_value_get_object(values);
    if (GTK_IS_FILE_CHOOSER_DIALOG(window))
        fit_child(window, NULL);
    return TRUE;
}

/* GTK creates the location entry on demand when switching from breadcrumbs. */
static gboolean entry_map(GSignalInvocationHint *hint, guint n_values,
                          const GValue *values, gpointer data)
{
    GtkWidget *widget = g_value_get_object(values);
    if (GTK_IS_ENTRY(widget) && GTK_IS_FILE_CHOOSER_DIALOG(gtk_widget_get_toplevel(widget)))
        fit_child(widget, NULL);
    return TRUE;
}

G_MODULE_EXPORT void gtk_module_init(gint *argc, gchar ***argv)
{
    /* GTK loads modules before it necessarily initializes the widget class. */
    gpointer widget_class = g_type_class_ref(GTK_TYPE_WIDGET);
    g_signal_add_emission_hook(g_signal_lookup("show", GTK_TYPE_WIDGET), 0,
                               dialog_show, NULL, NULL);
    g_signal_add_emission_hook(g_signal_lookup("map", GTK_TYPE_WIDGET), 0,
                               entry_map, NULL, NULL);
    g_type_class_unref(widget_class);
}
