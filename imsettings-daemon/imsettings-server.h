/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-server.h
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
#ifndef __IMSETTINGS_IMSETTINGS_SERVER_H__
#define __IMSETTINGS_IMSETTINGS_SERVER_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <imsettings/imsettings-info.h>


G_BEGIN_DECLS

#define IMSETTINGS_TYPE_SERVER			(imsettings_server_get_type())
#define IMSETTINGS_SERVER(_o_)			(G_TYPE_CHECK_INSTANCE_CAST ((_o_), IMSETTINGS_TYPE_SERVER, IMSettingsServer))
#define IMSETTINGS_SERVER_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), IMSETTINGS_TYPE_SERVER, IMSettingsServerClass))
#define IMSETTINGS_SERVER_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), IMSETTINGS_TYPE_SERVER, IMSettingsServerClass))
#define IMSETTINGS_IS_SERVER(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), IMSETTINGS_TYPE_SERVER))
#define IMSETTINGS_IS_SERVER_CLASS(_c_)		(G_TYPE_CHECK_CLASS_TYPE ((_c_), IMSETTINGS_TYPE_SERVER))

typedef struct _IMSettingsServerClass	IMSettingsServerClass;
typedef struct _IMSettingsServer	IMSettingsServer;
typedef struct _IMSettingsServerPrivate	IMSettingsServerPrivate;

struct _IMSettingsServerClass {
	GObjectClass parent_class;

	void (* disconnected) (IMSettingsServer *server);
	void (* changed_im)   (IMSettingsServer *server,
			       IMSettingsInfo   *info);

	void (*reserved1) (void);
	void (*reserved2) (void);
	void (*reserved3) (void);
	void (*reserved4) (void);
};
struct _IMSettingsServer {
	GObject                  parent_instance;

	/*< private >*/
	IMSettingsServerPrivate *priv;
};


GType             imsettings_server_get_type  (void) G_GNUC_CONST;
IMSettingsServer *imsettings_server_new       (GDBusConnection *connection,
					       const gchar     *homedir,
					       const gchar     *xinputrcdir,
					       const gchar     *xinputdir,
					       const gchar     *moduledir);
void              imsettings_server_start     (IMSettingsServer *server,
					       gboolean          replace);

G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_SERVER_H__ */
