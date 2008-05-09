/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * connection.h
 * Copyright (C) 2008 Red Hat, Inc. All rights reserved.
 * 
 * Authors:
 *   Akira TAGOH  <tagoh@redhat.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __XIM_CONNECTION_H__
#define __XIM_CONNECTION_H__

#include <glib.h>
#include <glib-object.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

#define XIM_TYPE_CONNECTION		(xim_connection_get_type())
#define XIM_CONNECTION(_o_)		(G_TYPE_CHECK_INSTANCE_CAST ((_o_), XIM_TYPE_CONNECTION, XIMConnection))
#define XIM_CONNECTION_CLASS(_c_)	(G_TYPE_CHECK_CLASS_CAST ((_c_), XIM_TYPE_CONNECTION, XIMConnectionClass))
#define XIM_IS_CONNECTION(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), XIM_TYPE_CONNECTION))
#define XIM_IS_CONNECTION_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), XIM_TYPE_CONNECTION))
#define XIM_CONNECTION_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), XIM_TYPE_CONNECTION, XIMConnectionClass))


typedef struct _XIMConnectionClass	XIMConnectionClass;
typedef struct _XIMConnection		XIMConnection;


struct _XIMConnectionClass {
	GObjectClass parent_class;
};

struct _XIMConnection {
	GObject parent_instance;
};


GType          xim_connection_get_type                (void) G_GNUC_CONST;
XIMConnection *xim_connection_new                     (Display       *dpy,
						       GType          protocol_type,
                                                       Window         client_window);
void           xim_connection_forward_event           (XIMConnection *from,
						       XIMConnection *to,
                                                       XEvent        *event);
gboolean       xim_connection_send_via_cm             (XIMConnection *conn,
                                                       gsize          threshold,
                                                       const GString *packets);
gboolean       xim_connection_send_via_property       (XIMConnection *conn,
                                                       const GString *packets);
gboolean       xim_connection_send_via_property_notify(XIMConnection *conn,
                                                       const GString *packets);


G_END_DECLS

#endif /* __XIM_CONNECTION_H__ */
