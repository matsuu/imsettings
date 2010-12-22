/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-proc.h
 * Copyright (C) 2008-2010 Red Hat, Inc. All rights reserved.
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
#ifndef __IMSETTINGS_IMSETTINGS_PROC_H__
#define __IMSETTINGS_IMSETTINGS_PROC_H__

#include <glib-object.h>
#include <imsettings/imsettings-info.h>


G_BEGIN_DECLS

#define IMSETTINGS_TYPE_PROC		(imsettings_proc_get_type())
#define IMSETTINGS_PROC(_o_)		(G_TYPE_CHECK_INSTANCE_CAST ((_o_), IMSETTINGS_TYPE_PROC, IMSettingsProc))
#define IMSETTINGS_PROC_CLASS(_c_)	(G_TYPE_CHECK_CLASS_CAST ((_c_), IMSETTINGS_TYPE_PROC, IMSettingsProcClass))
#define IMSETTINGS_PROC_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), IMSETTINGS_TYPE_PROC, IMSettingsProcClass))
#define IMSETTINGS_IS_PROC(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), IMSETTINGS_TYPE_PROC))
#define IMSETTINGS_IS_PROC_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), IMSETTINGS_TYPE_PROC))

typedef struct _IMSettingsProcClass	IMSettingsProcClass;
typedef struct _IMSettingsProc		IMSettingsProc;
typedef struct _IMSettingsProcPrivate	IMSettingsProcPrivate;

struct _IMSettingsProcClass {
	GObjectClass parent_class;

	void (*reserved1) (void);
	void (*reserved2) (void);
	void (*reserved3) (void);
	void (*reserved4) (void);
};
struct _IMSettingsProc {
	GObject                parent_instance;

	/*< private >*/
	IMSettingsProcPrivate *priv;
};


GType           imsettings_proc_get_type(void) G_GNUC_CONST;
IMSettingsProc *imsettings_proc_new     (IMSettingsInfo  *info);
gboolean        imsettings_proc_spawn   (IMSettingsProc  *proc,
					 GError         **error);
gboolean        imsettings_proc_kill    (IMSettingsProc  *proc,
					 GError         **error);
gboolean        imsettings_proc_is_alive(IMSettingsProc  *proc);
IMSettingsInfo *imsettings_proc_info    (IMSettingsProc  *proc);

G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_PROC_H__ */
