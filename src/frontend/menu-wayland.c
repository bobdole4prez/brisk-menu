/*
 * This file is part of brisk-menu.
 *
 * Copyright © 2026 Brisk Menu Developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Wayland support for the Brisk menu windows.
 *
 * Under Wayland a client cannot position its own windows or grab the
 * pointer and keyboard, so when Brisk runs inside mate-panel on a Wayland
 * session the menu window is turned into a wlr-layer-shell surface instead:
 *
 *  - the menu sits in the OVERLAY layer, anchored to the panel's edge, and
 *    asks for exclusive keyboard focus so the search entry works at once;
 *  - a transparent "dismiss" surface in the TOP layer covers the rest of
 *    the usable area of the monitor and hides the menu when clicked, which
 *    replaces the X11 pointer grab used to close the menu on outside clicks.
 *
 * Under X11 every function here is a no-op and the original code paths run.
 */

#define _GNU_SOURCE

#include "util.h"

BRISK_BEGIN_PEDANTIC
#include "menu-private.h"
#ifdef BRISK_WAYLAND
#include <gdk/gdkwayland.h>
/* gtk-layer-shell 0.9 declares a few functions as "f ()" */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <gtk-layer-shell.h>
#pragma GCC diagnostic pop
#endif
BRISK_END_PEDANTIC

gboolean brisk_menu_window_is_wayland(void)
{
#ifdef BRISK_WAYLAND
        GdkDisplay *display = gdk_display_get_default();
        return display != NULL && GDK_IS_WAYLAND_DISPLAY(display);
#else
        return FALSE;
#endif
}

GtkWindowType brisk_menu_window_get_window_type(void)
{
        /* gtk-layer-shell only works on toplevel windows */
        return brisk_menu_window_is_wayland() ? GTK_WINDOW_TOPLEVEL : GTK_WINDOW_POPUP;
}

#ifdef BRISK_WAYLAND
static gboolean brisk_menu_wayland_catcher_draw(__brisk_unused__ GtkWidget *widget, cairo_t *cr,
                                                __brisk_unused__ gpointer v)
{
        /* Fully transparent: the surface only exists to receive clicks */
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
        cairo_paint(cr);
        return GDK_EVENT_STOP;
}

static gboolean brisk_menu_wayland_catcher_press(__brisk_unused__ GtkWidget *widget,
                                                 __brisk_unused__ GdkEvent *event,
                                                 BriskMenuWindow *self)
{
        gtk_widget_hide(GTK_WIDGET(self));
        return GDK_EVENT_STOP;
}

static void brisk_menu_wayland_shown(BriskMenuWindow *self, __brisk_unused__ gpointer v)
{
        if (self->wl_catcher) {
                gtk_widget_show(self->wl_catcher);
        }
}

static void brisk_menu_wayland_hidden(BriskMenuWindow *self, __brisk_unused__ gpointer v)
{
        if (self->wl_catcher) {
                gtk_widget_hide(self->wl_catcher);
        }
}
#endif

/**
 * brisk_menu_window_wayland_setup:
 *
 * Turn the (not yet realized) menu window into a layer-shell surface and
 * create its dismiss surface. Must be called before the window is realized.
 */
void brisk_menu_window_wayland_setup(BriskMenuWindow *self)
{
#ifdef BRISK_WAYLAND
        GtkWindow *window = GTK_WINDOW(self);
        GtkWindow *catcher = NULL;
        GdkVisual *visual = NULL;

        if (!brisk_menu_window_is_wayland()) {
                return;
        }

        gtk_layer_init_for_window(window);
        gtk_layer_set_namespace(window, "brisk-menu");
        gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
        /* Zero: keep clear of the panel's exclusive zone, don't reserve space */
        gtk_layer_set_exclusive_zone(window, 0);

        catcher = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
        gtk_window_set_decorated(catcher, FALSE);
        gtk_layer_init_for_window(catcher);
        gtk_layer_set_namespace(catcher, "brisk-menu-dismiss");
        gtk_layer_set_layer(catcher, GTK_LAYER_SHELL_LAYER_TOP);
        gtk_layer_set_keyboard_mode(catcher, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
        gtk_layer_set_exclusive_zone(catcher, 0);
        for (int i = 0; i < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; i++) {
                gtk_layer_set_anchor(catcher, (GtkLayerShellEdge)i, TRUE);
        }

        visual = gdk_screen_get_rgba_visual(gtk_widget_get_screen(GTK_WIDGET(catcher)));
        if (visual) {
                gtk_widget_set_visual(GTK_WIDGET(catcher), visual);
        }
        gtk_widget_set_app_paintable(GTK_WIDGET(catcher), TRUE);
        gtk_widget_add_events(GTK_WIDGET(catcher), GDK_BUTTON_PRESS_MASK | GDK_TOUCH_MASK);
        g_signal_connect(catcher, "draw", G_CALLBACK(brisk_menu_wayland_catcher_draw), NULL);
        g_signal_connect(catcher,
                         "button-press-event",
                         G_CALLBACK(brisk_menu_wayland_catcher_press),
                         self);

        self->wl_catcher = GTK_WIDGET(catcher);

        g_signal_connect(self, "show", G_CALLBACK(brisk_menu_wayland_shown), NULL);
        g_signal_connect(self, "hide", G_CALLBACK(brisk_menu_wayland_hidden), NULL);
#else
        (void)self;
#endif
}

/**
 * brisk_menu_window_wayland_place:
 * @fill: Whether the window should fill the usable area of the monitor
 *
 * Anchor the menu next to the applet using layer-shell. Under X11 this does
 * nothing and returns FALSE, so the caller carries on with gtk_window_move().
 *
 * Returns: TRUE if the window was handled here (i.e. we're on Wayland)
 */
gboolean brisk_menu_window_wayland_place(BriskMenuWindow *self, gboolean fill)
{
#ifdef BRISK_WAYLAND
        GtkWindow *window = GTK_WINDOW(self);
        GtkWidget *toplevel = NULL;
        GdkWindow *panel_window = NULL;
        GdkMonitor *monitor = NULL;
        GdkRectangle geom = { 0 };
        GtkRequisition req = { 0 };
        gint applet_x = 0, applet_y = 0;
        gint width = 0, height = 0;
        GtkLayerShellEdge edge;

        if (!brisk_menu_window_is_wayland()) {
                return FALSE;
        }

        if (!self->relative_to) {
                return TRUE;
        }

        toplevel = gtk_widget_get_toplevel(self->relative_to);
        panel_window = gtk_widget_get_window(toplevel);

        /* GdkMonitor is GTK 3.22+, which gtk-layer-shell requires anyway;
         * Brisk caps GDK_VERSION_MAX_ALLOWED at 3.18 for the X11 code. */
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        if (panel_window) {
                monitor = gdk_display_get_monitor_at_window(gtk_widget_get_display(toplevel),
                                                            panel_window);
        }
        if (monitor) {
                gtk_layer_set_monitor(window, monitor);
                if (self->wl_catcher) {
                        gtk_layer_set_monitor(GTK_WINDOW(self->wl_catcher), monitor);
                }
                gdk_monitor_get_geometry(monitor, &geom);
        }
        G_GNUC_END_IGNORE_DEPRECATIONS

        for (int i = 0; i < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; i++) {
                gtk_layer_set_anchor(window, (GtkLayerShellEdge)i, fill);
                gtk_layer_set_margin(window, (GtkLayerShellEdge)i, 0);
        }
        if (fill) {
                return TRUE;
        }

        /* Applet offset within the panel. mate-panel's Wayland panels start
         * at the monitor edge, so this doubles as the offset on the monitor. */
        gtk_widget_translate_coordinates(self->relative_to, toplevel, 0, 0, &applet_x, &applet_y);

        gtk_window_get_size(window, &width, &height);
        gtk_widget_get_preferred_size(GTK_WIDGET(self), NULL, &req);
        width = MAX(width, req.width);
        height = MAX(height, req.height);

        switch (self->position) {
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
                edge = self->position == GTK_POS_LEFT ? GTK_LAYER_SHELL_EDGE_LEFT
                                                      : GTK_LAYER_SHELL_EDGE_RIGHT;
                gtk_layer_set_anchor(window, edge, TRUE);
                if (geom.height == 0 || applet_y + height <= geom.height) {
                        gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
                        gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_TOP, applet_y);
                } else {
                        gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
                }
                break;
        case GTK_POS_TOP:
        case GTK_POS_BOTTOM:
        default:
                edge = self->position == GTK_POS_TOP ? GTK_LAYER_SHELL_EDGE_TOP
                                                     : GTK_LAYER_SHELL_EDGE_BOTTOM;
                gtk_layer_set_anchor(window, edge, TRUE);
                if (geom.width == 0 || applet_x + width <= geom.width) {
                        gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
                        gtk_layer_set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT, applet_x);
                } else {
                        gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
                }
                break;
        }

        return TRUE;
#else
        (void)self;
        (void)fill;
        return FALSE;
#endif
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 expandtab:
 * :indentSize=8:tabSize=8:noTabs=true:
 */
