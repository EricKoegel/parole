/*
 * * Copyright (C) 2009 Ali <aliov@xfce.org>
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

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "medialist.h"
#include "mediafile.h"
#include "mediachooser.h"
#include "builder.h"

#define PAROLE_MEDIA_LIST_GET_PRIVATE(o) \
(G_TYPE_INSTANCE_GET_PRIVATE ((o), PAROLE_TYPE_MEDIA_LIST, ParoleMediaListPrivate))

struct ParoleMediaListPrivate
{
    GtkWidget 	  	*view;
    GtkWidget		*box;
    
    GtkWidget		*hide_show;
    gboolean		 hidden;
    
    GtkListStore	*store;
    ParoleMediaChooser 	*chooser;
};

enum
{
    MEDIA_ACTIVATED,
    MEDIA_CURSOR_CHANGED,
    LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ParoleMediaList, parole_media_list, G_TYPE_OBJECT)

static void
parole_media_list_add (ParoleMediaList *list, ParoleMediaFile *file, gboolean emit)
{
    GtkListStore *list_store;
    GtkTreePath *path;
    GtkTreeRowReference *row;
    GtkTreeIter iter;
			      
    list_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (list->priv->view)));
    
    gtk_list_store_append (list_store, &iter);
    
    gtk_list_store_set (list_store, 
			&iter, 
			NAME_COL, parole_media_file_get_display_name (file),
			DATA_COL, file,
			-1);
    if ( emit )
    {
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
	row = gtk_tree_row_reference_new (GTK_TREE_MODEL (list_store), path);
	gtk_tree_path_free (path);
	g_signal_emit (G_OBJECT (list), signals [MEDIA_ACTIVATED], 0, row);
    }
    /*
     * Unref it as the list store will have
     * a reference anyway.
     */
    g_object_unref (file);
}

static void
parole_media_list_file_opened_cb (ParoleMediaChooser *chooser, ParoleMediaFile *file, ParoleMediaList *list)
{
    parole_media_list_add (list, file, TRUE);
}

static void
parole_media_list_files_opened_cb (ParoleMediaChooser *chooser, GSList *files, ParoleMediaList *list)
{
    ParoleMediaFile *file;
    guint len;
    guint i;
    
    len = g_slist_length (files);
    
    for ( i = 0; i < len; i++)
    {
	file = g_slist_nth_data (files, i);
	parole_media_list_add (list, file, FALSE);
    }
}

static void
parole_media_list_add_clicked_cb (ParoleMediaList *list)
{
    parole_media_chooser_open (list->priv->chooser, TRUE);
}

static void
parole_media_list_remove_clicked_cb (ParoleMediaList *list)
{
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    
    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (list->priv->view));
    
    if ( !gtk_tree_selection_get_selected (sel, NULL, &iter) )
	return;
	
    gtk_list_store_remove (list->priv->store,
			   &iter);

    /*
     * Returns the number of children that iter has. 
     * As a special case, if iter is NULL, 
     * then the number of toplevel nodes is returned. Gtk API doc.
     */
    if ( gtk_tree_model_iter_n_children (GTK_TREE_MODEL (list->priv->store), NULL) == 0 )
	/*
	 * Will emit the signal media_cursor_changed with FALSE because there is no any 
	 * row remaining so the player can disable click on the play button.
	 */
	g_signal_emit (G_OBJECT (list), signals [MEDIA_CURSOR_CHANGED], 0, FALSE);
}

static void
parole_media_list_media_down_clicked_cb (ParoleMediaList *list)
{
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    GtkTreeIter *pos_iter;
    
    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (list->priv->view));
    
    if ( !gtk_tree_selection_get_selected (sel, NULL, &iter) )
	return;
    
    /* Save the selected iter to the selected row */
    pos_iter = gtk_tree_iter_copy (&iter);

    /* We are on the last node in the list!*/
    if ( !gtk_tree_model_iter_next (GTK_TREE_MODEL (list->priv->store), &iter) )
    {
	/* Return false if tree is empty*/
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list->priv->store), &iter))
	{
	    gtk_list_store_move_before (GTK_LIST_STORE (list->priv->store), pos_iter, &iter);
	}
    }
    else
    {
	gtk_list_store_move_after (GTK_LIST_STORE (list->priv->store), pos_iter, &iter);
    }
    
    gtk_tree_iter_free (pos_iter);
}

static void
parole_media_list_media_up_clicked_cb (ParoleMediaList *list)
{
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeIter *pos_iter;
    
    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (list->priv->view));
    
    if ( !gtk_tree_selection_get_selected (sel, NULL, &iter) )
	return;
	
    /* Save the selected iter to the selected row */
    pos_iter = gtk_tree_iter_copy (&iter);
    
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (list->priv->store), &iter);
    
    /* We are on the top of the list */
    if ( !gtk_tree_path_prev (path) )
    {
	/* Passing NULL as the last argument will cause this call to move 
	 * the iter to the end of the list, Gtk API doc*/
	gtk_list_store_move_before (GTK_LIST_STORE (list->priv->store), &iter, NULL);
    }
    else
    {
	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (list->priv->store), &iter, path))
	    gtk_list_store_move_before (GTK_LIST_STORE (list->priv->store), pos_iter, &iter);
    }
    
    gtk_tree_path_free (path);
    gtk_tree_iter_free (pos_iter);
}

static void
parole_media_list_row_activated (GtkTreeView *view, GtkTreePath *path, 
			       GtkTreeViewColumn *col, ParoleMediaList *list)
{
    GtkTreeModel *model;
    GtkTreeRowReference *row;
    
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (list->priv->view));

    row = gtk_tree_row_reference_new (gtk_tree_view_get_model (GTK_TREE_VIEW (list->priv->view)), 
				      path);
				      
    g_signal_emit (G_OBJECT (list), signals [MEDIA_ACTIVATED], 0, row);
}

static void
parole_media_list_cursor_changed_cb (GtkTreeView *view, ParoleMediaList *list)
{
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    
    sel = gtk_tree_view_get_selection (view);
    
    if ( !gtk_tree_selection_get_selected (sel, NULL, &iter ) )
    {
	g_signal_emit (G_OBJECT (list), signals [MEDIA_CURSOR_CHANGED], 0, FALSE);
    }
    else
    {
	g_signal_emit (G_OBJECT (list), signals [MEDIA_CURSOR_CHANGED], 0, TRUE);
    }
}

static void
parole_media_list_clear_list (ParoleMediaList *list)
{
    gtk_list_store_clear (GTK_LIST_STORE (list->priv->store));
}

static void
parole_media_list_show_menu (ParoleMediaList *list, guint button, guint activate_time)
{
    GtkWidget *menu, *mi;
    
    menu = gtk_menu_new ();
    
    mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLEAR, NULL);
    gtk_widget_set_sensitive (mi, TRUE);
    gtk_widget_show (mi);
    g_signal_connect_swapped (mi, "activate",
                              G_CALLBACK (parole_media_list_clear_list), list);
			      
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
    
    g_signal_connect_swapped (menu, "selection-done",
                              G_CALLBACK (gtk_widget_destroy), menu);
    
    gtk_menu_popup (GTK_MENU (menu), 
                    NULL, NULL,
                    NULL, NULL,
                    button, activate_time);
}

static gboolean
parole_media_list_button_release_event (GtkWidget *widget, GdkEventButton *ev, ParoleMediaList *list)
{
    
    if ( ev->button == 3 )
    {
	parole_media_list_show_menu (list, ev->button, ev->time);
	return TRUE;
    }
    
    return FALSE;
}

static void
parole_media_list_finalize (GObject *object)
{
    ParoleMediaList *list;

    list = PAROLE_MEDIA_LIST (object);

    g_object_unref (list->priv->chooser);

    G_OBJECT_CLASS (parole_media_list_parent_class)->finalize (object);
}

static void
parole_media_list_class_init (ParoleMediaListClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = parole_media_list_finalize;
    
    signals[MEDIA_ACTIVATED] = 
        g_signal_new ("media-activated",
                      PAROLE_TYPE_MEDIA_LIST,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ParoleMediaListClass, media_activated),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);

    signals[MEDIA_CURSOR_CHANGED] = 
        g_signal_new ("media-cursor-changed",
                      PAROLE_TYPE_MEDIA_LIST,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ParoleMediaListClass, media_cursor_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOOLEAN,
                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

    g_type_class_add_private (klass, sizeof (ParoleMediaListPrivate));
}

static void
parole_media_list_setup_view (ParoleMediaList *list)
{
    GtkListStore *list_store;
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    
    list_store = gtk_list_store_new (COL_NUMBERS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_OBJECT);

    gtk_tree_view_set_model (GTK_TREE_VIEW (list->priv->view), GTK_TREE_MODEL(list_store));
    
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (list->priv->view), TRUE);
    col = gtk_tree_view_column_new ();

    renderer = gtk_cell_renderer_pixbuf_new ();
    
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "pixbuf", PIXBUF_COL, NULL);

    renderer = gtk_cell_renderer_text_new();
    
    gtk_tree_view_column_pack_start(col, renderer, FALSE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", NAME_COL, NULL);
    
    gtk_tree_view_append_column (GTK_TREE_VIEW (list->priv->view), col);
    gtk_tree_view_column_set_title (col, _("Media list"));
    
    g_signal_connect (G_OBJECT (list->priv->view), "row_activated",
		      G_CALLBACK (parole_media_list_row_activated), list);

    g_signal_connect (G_OBJECT (list->priv->view), "cursor_changed",
		      G_CALLBACK (parole_media_list_cursor_changed_cb), list);
    
    g_signal_connect (G_OBJECT (list->priv->view), "button_release_event",
		      G_CALLBACK (parole_media_list_button_release_event), list);
    
    list->priv->store = list_store;
}

static void
parole_media_show_or_hide (ParoleMediaList *list)
{
    GtkWidget *img;
    
    g_object_get (G_OBJECT (list->priv->hide_show),
		  "image", &img,
		  NULL);
		  
    if ( list->priv->hidden )
    {
	g_object_set (G_OBJECT (img),
		      "stock", GTK_STOCK_GO_FORWARD,
		      NULL);
		      
	list->priv->hidden = FALSE;
	gtk_widget_show_all (list->priv->box);
	gtk_widget_set_tooltip_text (GTK_WIDGET (list->priv->hide_show), _("Hide playlist"));
    }
    else
    {
	g_object_set (G_OBJECT (img),
		      "stock", GTK_STOCK_GO_BACK,
		      NULL);
		      
	gtk_widget_hide_all (list->priv->box);
	gtk_widget_set_tooltip_text (GTK_WIDGET (list->priv->hide_show), _("Show playlist"));
	
	list->priv->hidden = TRUE;
    }
    g_object_unref (img);
}

static void
parole_media_list_init (ParoleMediaList *list)
{
    GtkBuilder *builder;
    
    list->priv = PAROLE_MEDIA_LIST_GET_PRIVATE (list);
    
    list->priv->hidden = TRUE;
    
    builder = parole_builder_new ();
    
    list->priv->view = GTK_WIDGET (gtk_builder_get_object (builder, "media-list"));
    list->priv->box = GTK_WIDGET (gtk_builder_get_object (builder, "list-box"));
    
    parole_media_list_setup_view (list);

    g_signal_connect_swapped (gtk_builder_get_object (builder, "add-media"), "clicked",
			      G_CALLBACK (parole_media_list_add_clicked_cb), list);
    
    g_signal_connect_swapped (gtk_builder_get_object (builder, "remove-media"), "clicked",
			      G_CALLBACK (parole_media_list_remove_clicked_cb), list);
    
    g_signal_connect_swapped (gtk_builder_get_object (builder, "media-down"), "clicked",
			      G_CALLBACK (parole_media_list_media_down_clicked_cb), list);
			      
    g_signal_connect_swapped (gtk_builder_get_object (builder, "media-up"), "clicked",
			      G_CALLBACK (parole_media_list_media_up_clicked_cb), list);
    
    list->priv->hide_show = GTK_WIDGET (gtk_builder_get_object (builder, "show-hide-list"));
    
    parole_media_show_or_hide (list);
    
    g_signal_connect_swapped (G_OBJECT (list->priv->hide_show), "clicked",
			      G_CALLBACK (parole_media_show_or_hide), list);
		    

    list->priv->chooser = parole_media_chooser_new ();
    
    g_signal_connect (G_OBJECT (list->priv->chooser), "media_file_opened",
		      G_CALLBACK (parole_media_list_file_opened_cb), list);
		      
    g_signal_connect (G_OBJECT (list->priv->chooser), "media_files_opened",
		      G_CALLBACK (parole_media_list_files_opened_cb), list);
    
    g_object_unref (builder);
}

ParoleMediaList *
parole_media_list_new (void)
{
    ParoleMediaList *list = NULL;
    list = g_object_new (PAROLE_TYPE_MEDIA_LIST, NULL);
    return list;
}

GtkTreeRowReference *parole_media_list_get_next_row (ParoleMediaList *list, GtkTreeRowReference *row)
{
    GtkTreeRowReference *next;
    GtkTreePath *path;
    GtkTreeIter iter;
    
    g_return_val_if_fail (row != NULL, NULL);

    if ( !gtk_tree_row_reference_valid (row) )
	return NULL;
    
    path = gtk_tree_row_reference_get_path (row);
    
    gtk_tree_path_next (path);
    
    if ( gtk_tree_model_get_iter (GTK_TREE_MODEL (list->priv->store), &iter, path))
    {
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (list->priv->store), &iter);
	next = gtk_tree_row_reference_new (GTK_TREE_MODEL (list->priv->store), path);
	gtk_tree_path_free (path);
	return next;
    }
    
    return NULL;
}

GtkTreeRowReference *parole_media_list_get_selected_row (ParoleMediaList *list)
{
    GtkTreeModel *model;
    GtkTreePath	*path;
    GtkTreeSelection *sel;
    GtkTreeRowReference *row;
    GtkTreeIter iter;
    
    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (list->priv->view));
    
    if (!gtk_tree_selection_get_selected (sel, &model, &iter))
	return NULL;
    
    path = gtk_tree_model_get_path (model, &iter);
    
    row = gtk_tree_row_reference_new (model, path);
    
    gtk_tree_path_free (path);

    return row;
}

void parole_media_list_set_row_pixbuf  (ParoleMediaList *list, GtkTreeRowReference *row, GdkPixbuf *pix)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    
    if ( gtk_tree_row_reference_valid (row) )
    {
	path = gtk_tree_row_reference_get_path (row);
	
	if ( gtk_tree_model_get_iter (GTK_TREE_MODEL (list->priv->store), &iter, path) )
	{
	    gtk_list_store_set (list->priv->store, &iter, PIXBUF_COL, pix, -1);
	}
	gtk_tree_path_free (path);
    }
}

/*
 * Called by ParolePlay when going to full screen mode
 */
void parole_media_list_set_visible (ParoleMediaList *list, gboolean visible)
{
    if ( visible )
    {
	if ( !list->priv->hidden )
	    gtk_widget_show_all (GTK_WIDGET (list->priv->box));
	gtk_widget_show (list->priv->hide_show);
    }
    else
    {
	gtk_widget_hide_all (GTK_WIDGET (list->priv->box));
	gtk_widget_hide (list->priv->hide_show);
    }
}
