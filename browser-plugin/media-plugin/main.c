/*
 * * Copyright (C) 2008-2009 Ali <aliov@xfce.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>

#include <gtk/gtk.h>

#include <libxfcegui4/libxfcegui4.h>

#include <gst/gst.h>

#include "parole-plugin-player.h"

#include "dbus/parole-dbus.h"

static gulong exit_source_id = 0;

static void G_GNUC_NORETURN
force_exit (gpointer data)
{
    g_debug ("Forcing exit");
    exit (0);
}

static void
posix_signal_handler (gint sig, ParolePluginPlayer *player)
{
    parole_plugin_player_exit (player);
    
    exit_source_id = g_timeout_add_seconds (4, (GSourceFunc) force_exit, NULL);
}

int main (int argc, char **argv)
{
    ParolePluginPlayer *player;
    GdkNativeWindow socket_id = 0;
    gchar *url = NULL;
    GOptionContext *ctx;
    GOptionGroup *gst_option_group;
    GError *error = NULL;
    gchar *dbus_name;
    GtkWidget *plug;
    
    GOptionEntry option_entries[] = 
    {
        { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &socket_id, N_("socket"), N_("SOCKET ID") },
	{ "url", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &url, N_("url to play"), N_("URL") },
        { NULL, },
    };
    
    if ( !g_thread_supported () )
	g_thread_init (NULL);
	
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

    textdomain (GETTEXT_PACKAGE);
  
    gtk_init (&argc, &argv);
    
    ctx = g_option_context_new (NULL);
    
    gst_option_group = gst_init_get_option_group ();
    g_option_context_add_main_entries (ctx, option_entries, GETTEXT_PACKAGE);
    g_option_context_set_translation_domain (ctx, GETTEXT_PACKAGE);
    g_option_context_add_group (ctx, gst_option_group);

    g_option_context_add_group (ctx, gtk_get_option_group (TRUE));
    
    if ( !g_option_context_parse (ctx, &argc, &argv, &error) ) 
    {
	g_print ("%s\n", error->message);
	g_print ("Type %s --help to list all available command line options", argv[0]);
	g_error_free (error);
	g_option_context_free (ctx);
	return EXIT_FAILURE;
    }
    g_option_context_free (ctx);
    
    dbus_name = g_strdup_printf ("org.Parole.Media.Plugin%d", socket_id);
    parole_dbus_register_name (dbus_name);
    
    g_assert (url != NULL);
    
    plug = gtk_plug_new (socket_id);
	
    player = parole_plugin_player_new (plug, url);
    gtk_widget_show_all (plug);
    
    if ( xfce_posix_signal_handler_init (&error)) 
    {
        xfce_posix_signal_handler_set_handler (SIGKILL,
                                               (XfcePosixSignalHandler) posix_signal_handler,
                                               player, NULL);
    } 
    else 
    {
        g_warning ("Unable to set up POSIX signal handlers: %s", error->message);
        g_error_free (error);
    }

    parole_plugin_player_play (player);
    
    gtk_main ();
    g_object_unref (player);
    gtk_widget_destroy (plug);
    parole_dbus_release_name (dbus_name);
    g_free (dbus_name);

    if ( exit_source_id != 0 )
	g_source_remove (exit_source_id);

    g_debug ("Exiting");
    gst_deinit ();

    return EXIT_SUCCESS;
}
