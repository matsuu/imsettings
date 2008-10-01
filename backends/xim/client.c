/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * client.c
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gdk/gdk.h>
#include <libgxim/gximmisc.h>
#include "client.h"

enum {
	PROP_0,
	PROP_IM_NAME,
	LAST_PROP
};
enum {
	SIGNAL_0,
	LAST_SIGNAL
};


//static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (XimClient, xim_client, G_TYPE_XIM_CL_TMPL);
G_DEFINE_TYPE (XimClientConnection, xim_client_connection, G_TYPE_XIM_CLIENT_CONNECTION);

/*
 * Private functions
 */
static void
xim_client_real_set_property(GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	switch (prop_id) {
	    case PROP_IM_NAME:
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
xim_client_real_get_property(GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	switch (prop_id) {
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
xim_client_real_finalize(GObject *object)
{
	XimClient *client = XIM_CLIENT (object);

	g_queue_foreach(client->pendingq, (GFunc)g_free, NULL);
	g_queue_free(client->pendingq);

	if (G_OBJECT_CLASS (xim_client_parent_class)->finalize)
		(* G_OBJECT_CLASS (xim_client_parent_class)->finalize) (object);
}

static void
xim_client_real_setup_connection(GXimCore       *core,
				 GXimConnection *conn)
{
}

static void
xim_client_class_init(XimClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GXimCoreClass *core_class = G_XIM_CORE_CLASS (klass);

	object_class->set_property = xim_client_real_set_property;
	object_class->get_property = xim_client_real_get_property;
	object_class->finalize     = xim_client_real_finalize;

	core_class->setup_connection = xim_client_real_setup_connection;

	/* properties */

	/* signals */
}

static void
xim_client_init(XimClient *client)
{
	client->pendingq = g_queue_new();
}

static void
xim_client_connection_real_finalize(GObject *object)
{
	XimClientConnection *conn = XIM_CLIENT_CONNECTION (object);

	g_queue_foreach(conn->pendingq, (GFunc)g_free, NULL);
	g_queue_free(conn->pendingq);

	if (G_OBJECT_CLASS (xim_client_connection_parent_class)->finalize)
		(* G_OBJECT_CLASS (xim_client_connection_parent_class)->finalize) (object);
}

static void
xim_client_connection_class_init(XimClientConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = xim_client_connection_real_finalize;
}

static void
xim_client_connection_init(XimClientConnection *conn)
{
	conn->pendingq = g_queue_new();
}

/*
 * Public functions
 */
XimClient *
xim_client_new(GdkDisplay  *dpy,
	       const gchar *im_name)
{
	XimClient *retval;
	GdkAtom atom_server;

	g_return_val_if_fail (GDK_IS_DISPLAY (dpy), NULL);
	g_return_val_if_fail (im_name != NULL, NULL);

	atom_server = g_xim_get_server_atom(im_name);
	retval = XIM_CLIENT (g_object_new(XIM_TYPE_CLIENT,
					  "display", dpy,
					  "connection_gtype", XIM_TYPE_CLIENT_CONNECTION,
					  "atom_server", atom_server,
					  NULL));

	return retval;
}
