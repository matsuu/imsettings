/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * proxy.h
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
#ifndef __XIM_PROXY_H__
#define __XIM_PROXY_H__

#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <libgxim/gximsrvconn.h>
#include <libgxim/gximsrvtmpl.h>

G_BEGIN_DECLS

#define XIM_TYPE_PROXY			(xim_proxy_get_type())
#define XIM_PROXY(_o_)			(G_TYPE_CHECK_INSTANCE_CAST ((_o_), XIM_TYPE_PROXY, XimProxy))
#define XIM_PROXY_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), XIM_TYPE_PROXY, XimProxyClass))
#define XIM_IS_PROXY(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), XIM_TYPE_PROXY))
#define XIM_IS_PROXY_CLASS(_c_)		(G_TYPE_CHECK_CLASS_TYPE ((_c_), XIM_TYPE_PROXY))
#define XIM_PROXY_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), XIM_TYPE_PROXY, XimProxyClass))

#define XIM_TYPE_PROXY_CONNECTION		(xim_proxy_connection_get_type())
#define XIM_PROXY_CONNECTION(_o_)		(G_TYPE_CHECK_INSTANCE_CAST ((_o_), XIM_TYPE_PROXY_CONNECTION, XimProxyConnection))
#define XIM_PROXY_CONNECTION_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), XIM_TYPE_PROXY_CONNECTION, XimProxyConnectionClass))
#define XIM_IS_PROXY_CONNECTION(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), XIM_TYPE_PROXY_CONNECTION))
#define XIM_IS_PROXY_CONNECTION_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), XIM_TYPE_PROXY_CONNECTION))
#define XIM_PROXY_CONNECTION_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), XIM_TYPE_PROXY_CONNECTION, XimProxyConnectionClass))


typedef struct _XimProxyClass		XimProxyClass;
typedef struct _XimProxy		XimProxy;

typedef struct _XimProxyConnectionClass	XimProxyConnectionClass;
typedef struct _XimProxyConnection	XimProxyConnection;

struct _XimProxyClass {
	GXimServerTemplateClass  parent_class;

	/* signals */
	gboolean (* locales_request)   (XimProxy         *proxy,
					GdkEventSelection *event);
	gboolean (* transport_request) (XimProxy         *proxy,
					GdkEventSelection *event);
};

struct _XimProxy {
	GXimServerTemplate       parent_instance;

	GHashTable              *client_table;
	GHashTable              *selection_table;
	GHashTable              *comm_table;
	GHashTable              *sconn_table;
	GHashTable              *simattr_table;
	GHashTable              *cimattr_table;
	GHashTable              *sicattr_table;
	GHashTable              *cicattr_table;
	gchar                   *connect_to;
	GXimLazySignalConnector *client_proto_signals;
	GXimServerTemplate      *default_server;
	guint16                 *simid_table;
	guint16                 *cimid_table;
	guint16                  latest_imid;
};

struct _XimProxyConnectionClass {
	GXimServerConnectionClass  parent_class;
};

struct _XimProxyConnection {
	GXimServerConnection  parent_instance;
	GXimStr              *locale;
	GQueue               *pendingq;
};


GType     xim_proxy_get_type      (void) G_GNUC_CONST;
XimProxy *xim_proxy_new           (GdkDisplay   *dpy,
                                   const gchar  *server_name,
				   const gchar  *connect_to);
gboolean  xim_proxy_take_ownership(XimProxy     *proxy,
                                   gboolean      force,
                                   GError      **error);
gboolean  xim_proxy_is_pending    (XimProxy     *proxy);
void      xim_proxy_disconnect_all(XimProxy     *proxy);

GType xim_proxy_connection_get_type(void) G_GNUC_CONST;


G_END_DECLS

#endif /* __XIM_PROXY_H__ */
