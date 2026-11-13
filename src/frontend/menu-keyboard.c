/*
 * This file is part of brisk-menu.
 *
 * Copyright © 2016-2020 Brisk Menu Developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "util.h"

BRISK_BEGIN_PEDANTIC
#include "menu-private.h"
BRISK_END_PEDANTIC

/**
 * Handle hiding the menu when it comes to the shortcut key only.
 * i.e. the Super_L key.
 */
gboolean brisk_menu_window_key_press(BriskMenuWindow *self, GdkEvent *event,
                                     __brisk_unused__ gpointer v)
{
        autofree(gchar) *accel_name = NULL;
        GdkModifierType lock_masks = GDK_MOD2_MASK | GDK_LOCK_MASK | GDK_MOD5_MASK;
        guint mods;

        if (!self->shortcut) {
                return GDK_EVENT_PROPAGATE;
        }

        /* Unset mask of the lock keys */
        mods = event->key.state & ~(lock_masks);

        accel_name = gtk_accelerator_name(event->key.keyval, mods);
        if (!accel_name || g_ascii_strcasecmp(self->shortcut, accel_name) != 0) {
                return GDK_EVENT_PROPAGATE;
        }

        gtk_widget_hide(GTK_WIDGET(self));
        return GDK_EVENT_STOP;
}

/**
 * Handle the escape key being hit so we can hide again
 */
gboolean brisk_menu_window_key_release(BriskMenuWindow *self, GdkEvent *event,
                                       __brisk_unused__ gpointer v)
{
        if (event->key.keyval == GDK_KEY_Escape) {
                gtk_widget_hide(GTK_WIDGET(self));
                return GDK_EVENT_STOP;
        }
        return GDK_EVENT_PROPAGATE;
}

/**
 * Called in idle once back out of the event
 */
static gboolean toggle_menu(BriskMenuWindow *self)
{
        gboolean vis = !gtk_widget_get_visible(GTK_WIDGET(self));
        if (vis) {
                /* Cheap trick to ensure we reset our active position */
                brisk_menu_window_set_parent_position(self, self->position);
                /* Ensure we're in the appropriate place */
                brisk_menu_window_update_screen_position(self);
        }

        gtk_widget_set_visible(GTK_WIDGET(self), vis);
        return FALSE;
}

extern BriskMenuWindow *BriskMenuWindowInstance;

/**
 * Toggle whichever menu window is current when the idle fires, so a window
 * replaced in the meantime (window-type change) is never touched.
 */
static gboolean toggle_current_menu(__brisk_unused__ gpointer v)
{
        if (BriskMenuWindowInstance) {
                toggle_menu(BriskMenuWindowInstance);
        }
        return FALSE;
}

/**
 * Handle global hotkey press
 */
static void hotkey_cb(__brisk_unused__ GdkEvent *event, __brisk_unused__ gpointer v)
{
        g_idle_add(toggle_current_menu, NULL);
}

#define BRISK_DBUS_NAME "com.solus_project.brisk.Menu"
#define BRISK_DBUS_PATH "/com/solus_project/brisk/Menu"

static const gchar brisk_dbus_xml[] =
    "<node>"
    "  <interface name='" BRISK_DBUS_NAME "'>"
    "    <method name='Toggle'/>"
    "  </interface>"
    "</node>";

static void brisk_dbus_method_call(__brisk_unused__ GDBusConnection *connection,
                                   __brisk_unused__ const gchar *sender,
                                   __brisk_unused__ const gchar *object_path,
                                   __brisk_unused__ const gchar *interface_name,
                                   const gchar *method_name,
                                   __brisk_unused__ GVariant *parameters,
                                   GDBusMethodInvocation *invocation,
                                   __brisk_unused__ gpointer v)
{
        if (g_strcmp0(method_name, "Toggle") == 0) {
                g_idle_add(toggle_current_menu, NULL);
                g_dbus_method_invocation_return_value(invocation, NULL);
                return;
        }
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "Unknown method %s",
                                              method_name);
}

static const GDBusInterfaceVTable brisk_dbus_vtable = { brisk_dbus_method_call, NULL, NULL, { 0 } };

static void brisk_dbus_bus_acquired(GDBusConnection *connection, __brisk_unused__ const gchar *name,
                                    __brisk_unused__ gpointer v)
{
        GDBusNodeInfo *info = NULL;
        GError *error = NULL;

        info = g_dbus_node_info_new_for_xml(brisk_dbus_xml, &error);
        if (!info) {
                g_warning("Brisk: invalid D-Bus introspection data: %s", error->message);
                g_error_free(error);
                return;
        }
        if (!g_dbus_connection_register_object(connection,
                                               BRISK_DBUS_PATH,
                                               info->interfaces[0],
                                               &brisk_dbus_vtable,
                                               NULL,
                                               NULL,
                                               &error)) {
                g_warning("Brisk: cannot export %s: %s", BRISK_DBUS_PATH, error->message);
                g_error_free(error);
        }
        g_dbus_node_info_unref(info);
}

/**
 * Export com.solus_project.brisk.Menu.Toggle on the session bus, once per
 * process. Under Wayland this is how the hotkey reaches us: bind
 * brisk-menu-toggle to Super in the compositor. Harmless under X11.
 */
void brisk_menu_window_export_toggle(void)
{
        static guint owner_id = 0;

        if (owner_id != 0) {
                return;
        }
        owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                                  BRISK_DBUS_NAME,
                                  G_BUS_NAME_OWNER_FLAGS_NONE,
                                  brisk_dbus_bus_acquired,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL);
}

/**
 * Update the applet hotkey in accordance with settings
 */
void brisk_menu_window_update_hotkey(BriskMenuWindow *self, gchar *key)
{
        if (self->shortcut) {
                brisk_key_binder_unbind(self->binder, self->shortcut);
                g_clear_pointer(&self->shortcut, g_free);
        }

        /* Wayland: the compositor owns global keys. Remember the shortcut so
         * pressing it inside the open menu still closes it. */
        if (brisk_menu_window_is_wayland()) {
                self->shortcut = g_strdup(key);
                return;
        }

        if (!brisk_key_binder_bind(self->binder, key, hotkey_cb, self)) {
                g_message("Failed to bind keyboard shortcut");
                return;
        }

        self->shortcut = g_strdup(key);
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
