/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "plugin.h"
#include "plugin_setup.h"
#include "plugin_types.h"

NPError NS_PluginInitialize();
void NS_PluginShutdown();
NPError NS_PluginGetValue(NPPVariable aVariable, void *aValue);

//////////////////////////////////////
//
// general initialization and shutdown
//
NPError NS_PluginInitialize()
{
    return NPERR_NO_ERROR;
}

void NS_PluginShutdown()
{
}

// get values per plugin
NPError NS_PluginGetValue(NPPVariable aVariable, void *aValue)
{
    return PluginGetValue(aVariable, aValue);
}

////////////////////////////////////////
//
// CPlugin class implementation
//
CPlugin::CPlugin (NPP pNPInstance)
{
    g_debug ("Constructor");
    
    mInstance = pNPInstance;
    mInitialized = TRUE;
    window_set = FALSE;
    child_spawned = FALSE;
    url = NULL;
    bus = NULL;
    proxy = NULL;
}

void kill_child (GPid child_pid)
{
    /*
    gchar *command;
	    
    command = g_strdup_printf ("kill -9 %d", child_pid);
    g_spawn_command_line_async (command, NULL);
    g_free (command);
    */
}

CPlugin::~CPlugin()
{
    g_debug ("Destructor");
    
    if ( !proxy )
	GetProxy ();
    
    if ( proxy )
    {
	GError *error = NULL;
	
	dbus_g_proxy_call (proxy, "Quit", &error,
			   G_TYPE_INVALID,
			   G_TYPE_INVALID);
		
	/*
	 * This might happen if the browser unload the plugin quickly
	 * while the process didn't get the dbus name.
	 */
	if ( error )
	{
#ifdef DEBUG
	    g_debug ("Failed to stop the backend via D-Bus killing the process %d", child_pid);
#endif
	    kill_child (child_pid);
	}
    }   
    else if ( child_spawned )
    {
	kill_child (child_pid);
    }
    
    if ( bus )
	dbus_g_connection_unref (bus);
    
    if ( url )
	g_free (url);
    mInstance = NULL;
}

NPBool CPlugin::init(NPWindow * pNPWindow)
{
    if (pNPWindow == NULL)
        return FALSE;

    m_Window = pNPWindow;
    mInitialized = TRUE;

    return mInitialized;
}

NPError CPlugin::SetWindow(NPWindow * aWindow)
{
    g_debug ("SetWindow");
    
    if ( aWindow == NULL )
	return FALSE;

    if ( !window_set )
    {
	window = (Window) aWindow->window;
	
	window_set = TRUE;
    }
    return NPERR_NO_ERROR;
}

void CPlugin::shut()
{
    g_debug ("shut");
    
    if ( !proxy )
	GetProxy ();
    
    if ( proxy )
    {
        dbus_g_proxy_call_no_reply (proxy, "Terminate",
                                    G_TYPE_INVALID,
                                    G_TYPE_INVALID);
    }   
}

NPBool CPlugin::isInitialized()
{
    return mInitialized;
}

void CPlugin::GetProxy ()
{
    g_return_if_fail (proxy == NULL);
    
    if ( child_spawned )
    {
	g_return_if_fail (bus != NULL);
	
	gchar *dbus_name;
	dbus_name = g_strdup_printf ("org.Parole.Media.Plugin%ld", window);
    
	proxy = dbus_g_proxy_new_for_name (bus, 
					   dbus_name,
					   "/org/Parole/Media/Plugin",
					   "org.Parole.Media.Plugin");
	if ( !proxy ) 
	    g_critical ("Unable to create proxy for %s", dbus_name);
	    
	g_free (dbus_name);
    }
}

NPError CPlugin::NewStream(NPMIMEType type, NPStream * stream, NPBool seekable, uint16 * stype)
{
    g_debug ("New stream callback %s", stream->url);
    
    if ( !url )
    {
	gchar *command[6];
	gchar *socket;
	gchar *app;
	GError *error = NULL;
	
	url = g_strdup (stream->url);
	
	socket = g_strdup_printf ("%ld", window);
#ifdef PAROLE_ENABLE_DEBUG
	app = g_strdup ("media-plugin/parole-media-plugin");
#else
	app = g_build_filename (LIBEXECDIR, "parole-media-plugin", NULL);
#endif

	command[0] = app;
	command[1] = (gchar *)"--socket-id";
	command[2] = socket;
	command[3] = (gchar *)"--url";
	command[4] = url;
	command[5] = NULL;
	
	if ( !g_spawn_async (NULL, 
			     command,
			     NULL,
			     (GSpawnFlags) 0,
			     NULL, NULL,
			     &child_pid, 
			     &error) )
	{
	    g_critical ("Failed to spawn command : %s", error->message);
	    g_error_free (error);
	    return NPERR_GENERIC_ERROR;
	}
	child_spawned = TRUE;
	g_free (socket);
	g_free (app);
	
	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    
	if ( error )
	{
	    g_critical ("%s : ", error->message);
	    g_error_free (error);
	    return NPERR_GENERIC_ERROR;
	}
    }
    return NPERR_NO_ERROR;
}

NPError CPlugin::DestroyStream(NPStream * stream, NPError reason)
{
    g_debug ("Destroy stream %s reason %i ", stream->url, reason);
    
    shut ();
    
    return NPERR_NO_ERROR;
}
