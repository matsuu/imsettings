/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * client.h
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
#ifndef __XIM_CLIENT_H__
#define __XIM_CLIENT_H__

#include <glib.h>
#include <glib-object.h>
#include <libgxim/gximcltmpl.h>
#include <libgxim/gximclconn.h>

G_BEGIN_DECLS

#define XIM_TYPE_CLIENT			(xim_client_get_type())
#define XIM_CLIENT(_o_)			(G_TYPE_CHECK_INSTANCE_CAST ((_o_), XIM_TYPE_CLIENT, XimClient))
#define XIM_CLIENT_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), XIM_TYPE_CLIENT, XimClientClass))
#define XIM_IS_CLIENT(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), XIM_TYPE_CLIENT))
#define XIM_IS_CLIENT_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), XIM_TYPE_CLIENT))
#define XIM_CLIENT_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), XIM_TYPE_CLIENT, XimClientClass))

#define XIM_TYPE_CLIENT_CONNECTION		(xim_client_connection_get_type())
#define XIM_CLIENT_CONNECTION(_o_)		(G_TYPE_CHECK_INSTANCE_CAST ((_o_), XIM_TYPE_CLIENT_CONNECTION, XimClientConnection))
#define XIM_CLIENT_CONNECTION_CLASS(_c_)	(G_TYPE_CHECK_CLASS_CAST ((_c_), XIM_TYPE_CLIENT_CONNECTION, XimClientConnectionClass))
#define XIM_IS_CLIENT_CONNECTION(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), XIM_TYPE_CLIENT_CONNECTION))
#define XIM_IS_CLIENT_CONNECTION_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), XIM_TYPE_CLIENT_CONNECTION))
#define XIM_CLIENT_CONNECTION_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), XIM_TYPE_CLIENT_CONNECTION, XimClientConnectionClass))


typedef struct _XimClientClass			XimClientClass;
typedef struct _XimClient			XimClient;
typedef struct _XimClientConnectionClass	XimClientConnectionClass;
typedef struct _XimClientConnection		XimClientConnection;

struct _XimClientClass {
	GXimClientTemplateClass  parent_class;

	/* signals */
	gboolean (* locales_notify)   (XimClient         *client,
				       GdkEventSelection *event);
	gboolean (* transport_notify) (XimClient         *client,
				       GdkEventSelection *event);
};

struct _XimClient {
	GXimClientTemplate  parent_instance;
	gboolean            is_reconnecting;
	GQueue             *pendingq;
};

struct _XimClientConnectionClass {
	GXimClientConnectionClass  parent_class;
};

struct _XimClientConnection {
	GXimClientConnection  parent_instance;
	gboolean              is_reconnecting;
	guint                 reconnection_state;
	GQueue               *pendingq;
};


GType      xim_client_get_type(void) G_GNUC_CONST;
XimClient *xim_client_new     (GdkDisplay  *dpy,
			       const gchar *im_name);

GType xim_client_connection_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif /* __XIM_CLIENT_H__ */
