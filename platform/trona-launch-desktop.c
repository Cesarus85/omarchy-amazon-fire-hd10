// SPDX-License-Identifier: GPL-2.0-only
#include <gdk/gdk.h>
#include <gio/gdesktopappinfo.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    GMainLoop *loop;
    GError *error;
} Launch;

static void launched(GObject *app, GAsyncResult *result, gpointer data)
{
    Launch *launch = data;
    g_app_info_launch_uris_finish(G_APP_INFO(app), result, &launch->error);
    g_main_loop_quit(launch->loop);
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    if (argc != 2 || !g_str_has_suffix(argv[1], ".desktop") || strchr(argv[1], '/')) {
        fputs("Expected one desktop entry ID including .desktop\n", stderr);
        return 2;
    }
    g_autoptr(GDesktopAppInfo) app = g_desktop_app_info_new(argv[1]);
    if (!app || !gdk_init_check(NULL, NULL)) {
        fprintf(stderr, "Could not resolve %s or connect to the display\n", argv[1]);
        return 1;
    }
    GdkDisplay *display = gdk_display_get_default();
    g_autoptr(GdkAppLaunchContext) context = gdk_display_get_app_launch_context(display);
    g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);
    Launch launch = { .loop = loop };
    g_app_info_launch_uris_async(G_APP_INFO(app), NULL, G_APP_LAUNCH_CONTEXT(context),
                                 NULL, launched, &launch);
    g_main_loop_run(loop);
    if (launch.error) {
        fprintf(stderr, "%s: %s\n", argv[1], launch.error->message);
        g_error_free(launch.error);
        return 1;
    }
    gdk_display_flush(display);
    return 0;
}
