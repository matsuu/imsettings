/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * loopback.h
 * Copyright (C) 2008 Red Hat, Inc. All rights reserved.
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
#ifndef __LOOPBACK_H__
#define __LOOPBACK_H__

#include <glib.h>
#include <glib-object.h>
#include <libgxim/gximsrvconn.h>
#include <libgxim/gximsrvtmpl.h>

G_BEGIN_DECLS

#define XIM_TYPE_LOOPBACK		(xim_loopback_get_type())
#define XIM_LOOPBACK(_o_)		(G_TYPE_CHECK_INSTANCE_CAST ((_o_), XIM_TYPE_LOOPBACK, XimLoopback))
#define XIM_LOOPBACK_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_o_), XIM_TYPE_LOOPBACK, XimLoopbackClass))
#define XIM_IS_LOOPBACK(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), XIM_TYPE_LOOPBACK))
#define XIM_IS_LOOPBACK_CLASS(_o_)	(G_TYPE_CHECK_CLASS_TYPE ((_o_), XIM_TYPE_LOOPBACK))
#define XIM_LOOPBACK_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), XIM_TYPE_LOOPBACK, XimLoopbackClass))

#define XIM_TYPE_LOOPBACK_CONNECTION	(xim_loopback_connection_get_type())


typedef struct _XimLoopbackClass	XimLoopbackClass;
typedef struct _XimLoopback		XimLoopback;

typedef struct _XimLoopbackConnectionClass	XimLoopbackConnectionClass;
typedef struct _XimLoopbackConnection		XimLoopbackConnection;

struct _XimLoopbackClass {
	GXimServerTemplateClass  parent_class;
};

struct _XimLoopback {
	GXimServerTemplate  parent_instance;

	GHashTable         *conn_table;
	guint               latest_imid;
};

struct _XimLoopbackConnectionClass {
	GXimServerConnectionClass  parent_class;
};

struct _XimLoopbackConnection {
	GXimServerConnection  parent_instance;
};


GType        xim_loopback_get_type(void) G_GNUC_CONST;
XimLoopback *xim_loopback_new     (GdkDisplay *dpy);

GType xim_loopback_connection_get_type(void) G_GNUC_CONST;


G_END_DECLS

#endif /* __LOOPBACK_H__ */
