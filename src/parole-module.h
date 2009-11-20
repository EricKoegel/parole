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

#ifndef __PAROLE_MODULE_H
#define __PAROLE_MODULE_H

#include <glib-object.h>

#include "parole.h"
#include "parole-plugin.h"

G_BEGIN_DECLS

#define PAROLE_TYPE_MODULE        (parole_module_get_type () )
#define PAROLE_MODULE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PAROLE_TYPE_MODULE, ParoleModule))
#define PAROLE_IS_MODULE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAROLE_TYPE_MODULE))

typedef struct ParoleModulePrivate ParoleModulePrivate;

typedef struct
{
    GTypeModule       	     parent;
    
    GModule		    *mod;
    ParolePlugin            *plugin;
    ParolePluginDesc        *desc;
    gboolean		     enabled;
    
    ParolePlugin	   *(*constructor)		   (void);
    
    ParolePluginDesc	   *(*get_plugin_description)	   (void);
    
} ParoleModule;

typedef struct
{
    GTypeModuleClass 	     parent_class;
    
} ParoleModuleClass;

GType        		     parole_module_get_type        (void) G_GNUC_CONST;

ParoleModule       	    *parole_module_new             (const gchar *filename);

void			     parole_module_set_active      (ParoleModule *module,
							    gboolean active);

G_END_DECLS

#endif /* __PAROLE_MODULE_H */