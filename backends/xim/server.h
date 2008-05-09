/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * server.h
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
#ifndef __XIM_SERVER_H__
#define __XIM_SERVER_H__

#include <glib.h>
#include <glib-object.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

#define XIM_TYPE_SERVER			(xim_server_get_type())
#define XIM_SERVER(_o_)			(G_TYPE_CHECK_INSTANCE_CAST ((_o_), XIM_TYPE_SERVER, XIMServer))
#define XIM_SERVER_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), XIM_TYPE_SERVER, XIMServerClass))
#define XIM_IS_SERVER(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), XIM_TYPE_SERVER))
#define XIM_IS_SERVER_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), XIM_TYPE_SERVER))
#define XIM_SERVER_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), XIM_TYPE_SERVER, XIMServerClass))


typedef struct _XIMServerClass		XIMServerClass;
typedef struct _XIMServer		XIMServer;


struct _XIMServerClass {
	GObjectClass parent_class;

	/* signals */
	void (* destroy) (XIMServer   *server);
};

struct _XIMServer {
	GObject  parent_instance;
	Display *dpy;
};


GType      xim_server_get_type      (void) G_GNUC_CONST;
XIMServer *xim_server_new           (Display     *dpy,
                                     const gchar *xim);
gboolean   xim_server_is_initialized(XIMServer   *server);
gboolean   xim_server_setup         (XIMServer   *server,
				     gboolean     replace);


G_END_DECLS

#endif /* __XIM_SERVER_H__ */
