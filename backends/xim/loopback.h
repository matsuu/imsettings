/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * loopback.h
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
#ifndef __XIM_LOOPBACK_H__
#define __XIM_LOOPBACK_H__

#include <glib.h>
#include <glib-object.h>
#include "protocol.h"

G_BEGIN_DECLS

#define XIM_TYPE_LOOPBACK		(xim_loopback_get_type())
#define XIM_LOOPBACK(_o_)		(G_TYPE_CHECK_INSTANCE_CAST ((_o_), XIM_TYPE_LOOPBACK, XIMLoopback))
#define XIM_LOOPBACK_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), XIM_TYPE_LOOPBACK, XIMLoopbackClass))
#define XIM_IS_LOOPBACK(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), XIM_TYPE_LOOPBACK))
#define XIM_IS_LOOPBACK_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), XIM_TYPE_LOOPBACK))
#define XIM_LOOPBACK_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), XIM_TYPE_LOOPBACK, XIMLoopbackClass))


typedef struct _XIMLoopbackClass	XIMLoopbackClass;
typedef struct _XIMLoopback		XIMLoopback;

struct _XIMLoopbackClass {
	XIMProtocolClass parent_class;
};

struct _XIMLoopback {
	XIMProtocol parent_instance;
};


GType        xim_loopback_get_type(void) G_GNUC_CONST;
XIMLoopback *xim_loopback_new     (const gchar *display_name);
gboolean     xim_loopback_setup   (XIMLoopback *loop);


G_END_DECLS

#endif /* __XIM_LOOPBACK_H__ */
