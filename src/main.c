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
#include <glib.h>
#include <gio/gio.h>

#include <gst/gst.h>

#include <libxfce4util/libxfce4util.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "player.h"
#include "utils.h"
#include "dbus.h"

static void
parole_send_files (gchar **filenames)
{
    GFile *file;
    DBusGConnection *bus;
    DBusGProxy *proxy;
    gchar **out_paths;
    GError *error = NULL;
    guint i;
    
    bus = parole_g_session_bus_get ();
    
    out_paths = g_new (gchar *, g_strv_length (filenames));
    
    for ( i = 0; filenames && filenames[i]; i++)
    {
	file = g_file_new_for_commandline_arg (filenames[i]);
	out_paths[i] = g_file_get_path (file);
	g_object_unref (file);
    }
    
    proxy = dbus_g_proxy_new_for_name (bus, 
				       PAROLE_DBUS_NAME,
				       PAROLE_DBUS_PATH,
				       PAROLE_DBUS_INTERFACE);
				       
    dbus_g_proxy_call (proxy, "AddFiles", &error,
		       G_TYPE_STRV, out_paths,
		       G_TYPE_INVALID,
		       G_TYPE_INVALID);
		       
    if ( error )
    {
	g_critical ("Unable to send media files to Parole: %s", error->message);
	g_error_free (error);
    }
    
    g_object_unref (proxy);
    dbus_g_connection_unref (bus);
    g_strfreev (out_paths);
}

int main (int argc, char **argv)
{
    ParolePlayer *player;
    GError *error = NULL;
    gchar **filenames = NULL;
    
    GOptionEntry option_entries[] = 
    {
	{G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, N_("Media to play"), NULL},
        { NULL, },
    };
    
    if ( !g_thread_supported () )
	g_thread_init (NULL);

    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");
    
    gst_init (&argc, &argv);
    
    if ( !gtk_init_with_args (&argc, &argv, (gchar *)"", option_entries, (gchar *)PACKAGE, &error))
    {
	if (G_LIKELY (error) ) 
        {
            g_printerr ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_printerr (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_printerr ("\n");
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    if ( parole_dbus_name_has_owner (PAROLE_DBUS_NAME) )
    {
	TRACE ("Parole is already running");
	if ( filenames && filenames[0] != NULL )
	    parole_send_files (filenames);
    }
    else
    {
	parole_dbus_register_name (PAROLE_DBUS_NAME);
	
	player = parole_player_new ();

	if ( filenames && filenames[0] != NULL )
	{
	    ParoleMediaList *list;
	    list = parole_player_get_media_list (player);
	    parole_media_list_add_files (list, filenames);
	}
	    
	gdk_notify_startup_complete ();
	gtk_main ();
	parole_dbus_release_name (PAROLE_DBUS_NAME);
    }

    gst_deinit ();
    return 0;
}
