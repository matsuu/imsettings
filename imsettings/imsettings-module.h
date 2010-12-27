/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-module.h
 * Copyright (C) 2010 Red Hat, Inc. All rights reserved.
 * 
 * Authors:
 *   Akira TAGOH  <tagoh@redhat.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __IMSETTINGS_IMSETTINGS_MODULE_H__
#define __IMSETTINGS_IMSETTINGS_MODULE_H__

#include <glib.h>
#include <glib-object.h>
#include <imsettings/imsettings-info.h>

G_BEGIN_DECLS

#define IMSETTINGS_TYPE_MODULE			(imsettings_module_get_type())
#define IMSETTINGS_MODULE(_o_)			(G_TYPE_CHECK_INSTANCE_CAST ((_o_), IMSETTINGS_TYPE_MODULE, IMSettingsModule))
#define IMSETTINGS_MODULE_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), IMSETTINGS_TYPE_MODULE, IMSettingsModuleClass))
#define IMSETTINGS_MODULE_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), IMSETTINGS_TYPE_MODULE, IMSettingsModuleClass))
#define IMSETTINGS_IS_MODULE(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), IMSETTINGS_TYPE_MODULE))
#define IMSETTINGS_IS_MODULE_CLASS(_c_)		(G_TYPE_CHECK_CLASS_TYPE ((_c_), IMSETTINGS_TYPE_MODULE))


typedef struct _IMSettingsModuleClass		IMSettingsModuleClass;
typedef struct _IMSettingsModule		IMSettingsModule;
typedef struct _IMSettingsModulePrivate		IMSettingsModulePrivate;

typedef void (* IMSettingsModuleCallback) (IMSettingsInfo *info);

struct _IMSettingsModuleClass {
	GObjectClass parent_class;
};
struct _IMSettingsModule {
	GObject                  parent_instance;
	IMSettingsModulePrivate *priv;
};


GType             imsettings_module_get_type (void) G_GNUC_CONST;
IMSettingsModule *imsettings_module_new      (const gchar *name);
const gchar      *imsettings_module_get_name (IMSettingsModule *module);
gboolean          imsettings_module_load     (IMSettingsModule *module);
void              imsettings_module_switch_im(IMSettingsModule *module,
					      IMSettingsInfo   *info);

G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_MODULE_H__ */
