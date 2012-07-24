/*
 * * Copyright (C) 2009-2011 Ali <aliov@xfce.org>
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

#include <gtk/gtk.h>
#include <glib.h>

#include <src/misc/parole-file.h>

#include "interfaces/mediachooser_ui.h"

#include "parole-mediachooser.h"
#include "parole-builder.h"
#include "parole-filters.h"
#include "parole-rc-utils.h"
#include "parole-utils.h"

#include "common/parole-common.h"

/*
 * GtkBuilder Callbacks
 */
void    parole_media_chooser_add_clicked (GtkWidget *widget,
					  ParoleMediaChooser *chooser);

void    parole_media_chooser_close_clicked (GtkWidget *widget,
					    ParoleMediaChooser *chooser);

void	parole_media_chooser_destroy_cb (GtkWidget *widget,
					 ParoleMediaChooser *chooser);
					 
void	media_chooser_folder_changed_cb (GtkWidget *widget, 
					 gpointer data);

void	media_chooser_file_activate_cb  (GtkFileChooser *filechooser,
					 ParoleMediaChooser *chooser);

struct ParoleMediaChooser
{
    GObject             parent;
    
    ParoleConf          *conf;
    GtkWidget			*window;
    GtkWidget 			*spinner;
    
};

struct ParoleMediaChooserClass
{
    GObjectClass 		 parent_class;
    
    void			 (*media_files_opened)		    (ParoleMediaChooser *chooser,
								     GSList *list);
};

enum
{
    MEDIA_FILES_OPENED,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ParoleMediaChooser, parole_media_chooser, G_TYPE_OBJECT)

void
media_chooser_folder_changed_cb (GtkWidget *widget, gpointer data)
{
    gchar *folder;
    folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (widget));
    
    if ( folder )
    {
	parole_rc_write_entry_string ("media-chooser-folder", PAROLE_RC_GROUP_GENERAL, folder);
	g_free (folder);
    }
}

static void
parole_media_chooser_add (ParoleMediaChooser *chooser, GtkWidget *file_chooser)
{
    GSList *media_files = NULL;
    GSList *files;
    GtkFileFilter *filter;
    gboolean scan_recursive;
    gchar *file;
    guint    i;
    guint len;
    
    files = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (file_chooser));
    filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (file_chooser));
    
    if ( G_UNLIKELY (files == NULL) )
	return;
	
	g_object_get (G_OBJECT (chooser->conf),
		  "scan-recursive", &scan_recursive,
		  NULL);
    
    len = g_slist_length (files);
    
    for ( i = 0; i < len; i++)
    {
	file = g_slist_nth_data (files, i);
	parole_get_media_files (filter, file, scan_recursive, &media_files);
    }
    
    g_signal_emit (G_OBJECT (chooser), signals [MEDIA_FILES_OPENED], 0, media_files);
    g_slist_free (media_files);
    
    g_slist_foreach (files, (GFunc) g_free, NULL);
    g_slist_free (files);
}

static gboolean
parole_media_chooser_add_idle (gpointer data)
{
    ParoleMediaChooser *chooser;
    GtkWidget *file_chooser;
    
    chooser = PAROLE_MEDIA_CHOOSER (data);
    
    file_chooser = GTK_WIDGET (g_object_get_data (G_OBJECT (chooser), "file-chooser"));
    
    parole_media_chooser_add (chooser, file_chooser);
    
    gtk_widget_destroy (chooser->window);
    
    return FALSE;
}

static void
parole_media_chooser_open (ParoleMediaChooser *chooser)
{
    parole_window_busy_cursor (chooser->window->window);

    gtk_widget_show( chooser->spinner );
    
    g_idle_add ((GSourceFunc) parole_media_chooser_add_idle, chooser);
}

void parole_media_chooser_add_clicked (GtkWidget *widget, ParoleMediaChooser *chooser)
{
    parole_media_chooser_open (chooser);
}

void parole_media_chooser_close_clicked (GtkWidget *widget, ParoleMediaChooser *chooser)
{
    gtk_widget_destroy (chooser->window);
}

void parole_media_chooser_destroy_cb (GtkWidget *widget, ParoleMediaChooser *chooser)
{
    g_object_unref (chooser);
}

void media_chooser_file_activate_cb (GtkFileChooser *filechooser, ParoleMediaChooser *chooser)
{
    parole_media_chooser_open (chooser);
}

static void
parole_media_chooser_open_internal (ParoleMediaChooser *media_chooser)
{
    GtkWidget       *file_chooser;
    GtkBuilder      *builder;
    GtkWidget       *recursive;
    GtkWidget       *replace;
    GtkWidget       *play_opened;
    GtkFileFilter   *filter;
    gboolean        scan_recursive;
    gboolean        replace_playlist;
    gboolean        play;
    const gchar    *folder;

    builder = parole_builder_new_from_string (mediachooser_ui, mediachooser_ui_length);
    
    media_chooser->window = GTK_WIDGET (gtk_builder_get_object (builder, "chooser"));
    media_chooser->spinner = GTK_WIDGET (gtk_builder_get_object (builder, "spinner"));
    
    gtk_widget_hide( media_chooser->spinner );
    
    file_chooser = GTK_WIDGET (gtk_builder_get_object (builder, "filechooserwidget"));
    filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (file_chooser));
    
    gtk_file_filter_set_name( filter, _("Supported files") );
    
    gtk_file_filter_add_mime_type (GTK_FILE_FILTER (filter), "audio/*");
    gtk_file_filter_add_mime_type (GTK_FILE_FILTER (filter), "video/*");
    
    GtkFileFilter *all_files;
    all_files = gtk_file_filter_new();
    gtk_file_filter_add_pattern ( all_files, "*");
    
    gtk_file_filter_set_name( all_files, _("All files") );
    
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(file_chooser), filter );
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(file_chooser), all_files );

    folder = parole_rc_read_entry_string ("media-chooser-folder", PAROLE_RC_GROUP_GENERAL, NULL);
    
    if ( folder )
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (file_chooser), folder);
    
    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (file_chooser), TRUE);

    g_object_get (G_OBJECT (media_chooser->conf),
		  "scan-recursive", &scan_recursive,
		  "replace-playlist", &replace_playlist,
		  "play-opened-files", &play,
		  NULL);
    
    recursive = GTK_WIDGET (gtk_builder_get_object (builder, "recursive"));
    replace = GTK_WIDGET (gtk_builder_get_object (builder, "replace"));
    play_opened = GTK_WIDGET (gtk_builder_get_object (builder, "play-added-files"));
    
    g_object_set_data (G_OBJECT (media_chooser), "file-chooser", file_chooser);
    g_object_set_data (G_OBJECT (media_chooser), "recursive", recursive);
    
    gtk_builder_connect_signals (builder, media_chooser);
    
    g_object_unref (builder);
}

static void
parole_media_chooser_finalize (GObject *object)
{
    ParoleMediaChooser *chooser;
    
    chooser = PAROLE_MEDIA_CHOOSER (object);
    
    g_object_unref (chooser->conf);
    
    if ( chooser->window )
	gtk_widget_destroy (chooser->window);
    
    G_OBJECT_CLASS (parole_media_chooser_parent_class)->finalize (object);
}

static void
parole_media_chooser_class_init (ParoleMediaChooserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    signals[MEDIA_FILES_OPENED] = 
        g_signal_new("media-files-opened",
                      PAROLE_TYPE_MEDIA_CHOOSER,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ParoleMediaChooserClass, media_files_opened),
                      NULL, NULL,
		      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, 
		      G_TYPE_POINTER);

    object_class->finalize = parole_media_chooser_finalize;
}

static void
parole_media_chooser_init (ParoleMediaChooser *chooser)
{
    chooser->conf = parole_conf_new ();
}

ParoleMediaChooser *parole_media_chooser_open_local (GtkWidget *parent)
{
    ParoleMediaChooser *chooser;
        
    chooser = g_object_new (PAROLE_TYPE_MEDIA_CHOOSER, NULL);
    
    parole_media_chooser_open_internal (chooser);

    gtk_window_set_modal (GTK_WINDOW (chooser->window), TRUE);
    
    if ( parent )
	gtk_window_set_transient_for (GTK_WINDOW (chooser->window), GTK_WINDOW (parent));
	
    gtk_window_set_position (GTK_WINDOW (chooser->window), GTK_WIN_POS_CENTER_ON_PARENT);

    gtk_widget_show_all (chooser->window);

    return chooser;
}
