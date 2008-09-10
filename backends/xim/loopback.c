/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * loopback.c
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <glib/gi18n-lib.h>
#include <libgxim/gximattr.h>
#include <libgxim/gximmessage.h>
#include <libgxim/gximmisc.h>
#include <libgxim/gximprotocol.h>
#include <libgxim/gximtransport.h>
#include "loopback.h"

enum {
	PROP_0,
	LAST_PROP
};
enum {
	SIGNAL_0,
	LAST_SIGNAL
};

static gboolean xim_loopback_real_xim_open                (GXimProtocol  *proto,
                                                           const GXimStr *locale,
                                                           gpointer       data);
static gboolean xim_loopback_real_xim_close               (GXimProtocol  *proto,
                                                           guint16        imid,
                                                           gpointer       data);
static gboolean xim_loopback_real_xim_encoding_negotiation(GXimProtocol  *proto,
                                                           guint          imid,
                                                           const GSList  *encodings,
                                                           const GSList  *details,
                                                           gpointer       data);
static gboolean xim_loopback_real_xim_get_im_values       (GXimProtocol  *proto,
                                                           guint16        imid,
                                                           const GSList  *attributes,
                                                           gpointer       data);
static gboolean xim_loopback_real_xim_create_ic           (GXimProtocol  *proto,
                                                           guint16        imid,
                                                           const GSList  *attributes,
                                                           gpointer       data);
static gboolean xim_loopback_real_xim_disconnect          (GXimProtocol  *proto,
							   gpointer       data);

//static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (XimLoopback, xim_loopback, G_TYPE_XIM_SRV_TMPL);

/*
 * Private functions
 */
static void
xim_loopback_real_finalize(GObject *object)
{
	XimLoopback *loopback = XIM_LOOPBACK (object);

	g_hash_table_destroy(loopback->conn_table);

	if (G_OBJECT_CLASS (xim_loopback_parent_class)->finalize)
		(* G_OBJECT_CLASS (xim_loopback_parent_class)->finalize) (object);
}

static void
xim_loopback_real_setup_connection(GXimCore       *core,
				   GXimConnection *conn)
{
}

static void
xim_loopback_class_init(XimLoopbackClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GXimCoreClass *core_class = G_XIM_CORE_CLASS (klass);

	object_class->finalize     = xim_loopback_real_finalize;

	core_class->setup_connection = xim_loopback_real_setup_connection;

	/* properties */

	/* signals */
}

static void
xim_loopback_init(XimLoopback *loopback)
{
	GXimLazySignalConnector sigs[] = {
		{"XIM_OPEN", G_CALLBACK (xim_loopback_real_xim_open), loopback},
		{"XIM_CLOSE", G_CALLBACK (xim_loopback_real_xim_close), loopback},
		{"XIM_ENCODING_NEGOTIATION", G_CALLBACK (xim_loopback_real_xim_encoding_negotiation), loopback},
		{"XIM_GET_IM_VALUES", G_CALLBACK (xim_loopback_real_xim_get_im_values), loopback},
		{"XIM_CREATE_IC", G_CALLBACK (xim_loopback_real_xim_create_ic), loopback},
		{"XIM_DISCONNECT", G_CALLBACK (xim_loopback_real_xim_disconnect), loopback},
		{NULL, NULL, NULL}
	};

	g_object_set(loopback, "proto_signals", sigs, NULL);

	loopback->conn_table = g_hash_table_new(g_direct_hash, g_direct_equal);
	loopback->latest_imid = 1;
}

static guint
xim_loopback_find_imid(XimLoopback *loopback)
{
	gboolean overflowed = FALSE;
	guint begin_id = loopback->latest_imid;

	while (1) {
		if (g_hash_table_lookup(loopback->conn_table, GUINT_TO_POINTER (loopback->latest_imid)) == NULL)
			break;
		if (overflowed && loopback->latest_imid >= begin_id)
			return 0;
		loopback->latest_imid++;
		if (loopback->latest_imid == 0) {
			overflowed = TRUE;
			loopback->latest_imid = 1;
		}
	}

	return loopback->latest_imid;
}

static void
xim_loopback_set_default_im_values(XimLoopback *loopback,
				   GXimIMAttr  *attr)
{
	GSList *attributes, *l;

	attributes = g_xim_attr_get_supported_attributes(G_XIM_ATTR (attr));

	for (l = attributes; l != NULL; l = g_slist_next(l)) {
		if (l->data != NULL) {
			if (strcmp(l->data, XNQueryInputStyle) == 0) {
				GXimStyles *styles = g_xim_styles_new();
				GXimStyle default_im_styles[] = {
					G_XIM_PreeditNothing | G_XIM_StatusNothing,
					G_XIM_PreeditPosition | G_XIM_StatusNothing,
					G_XIM_PreeditPosition | G_XIM_StatusArea,
					G_XIM_PreeditCallbacks | G_XIM_StatusNothing,
					G_XIM_PreeditCallbacks | G_XIM_StatusCallbacks,
					0
				}, i;
				GError *error = NULL;

				for (i = 0; default_im_styles[i] != 0; i++) {
					g_xim_styles_append(styles,
							    default_im_styles[i],
							    &error);
					if (error) {
						g_xim_message_gerror(G_XIM_CORE (loopback)->message, error);
						g_clear_error(&error);
					}
				}
				g_object_set(attr, l->data, styles, NULL);
				g_xim_styles_free(styles);
			} else {
				g_xim_message_bug(G_XIM_CORE (loopback)->message,
						  "Unsupported IM attribute: %s",
						  (gchar *)l->data);
			}
		}
	}
	g_slist_foreach(attributes,
			(GFunc)g_free,
			NULL);
	g_slist_free(attributes);
}

static void
xim_loopback_set_default_ic_values(XimLoopback *loopback,
				   GXimICAttr  *attr)
{
}

/* protocol callbacks */
static gboolean
xim_loopback_real_xim_open(GXimProtocol  *proto,
			   const GXimStr *locale,
			   gpointer       data)
{
	GXimAttr *attr;
	GXimIMAttr *imattr = NULL;
	GXimICAttr *icattr = NULL;
	guint imid = 0;
	XimLoopback *loopback = XIM_LOOPBACK (data);
	GXimConnection *conn = G_XIM_CONNECTION (proto);
	GdkNativeWindow client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	GSList *l, *imlist = NULL, *iclist = NULL, *list;
	GXimRawAttr *raw;

	g_return_val_if_fail (conn->imid == 0, FALSE);

	imid = xim_loopback_find_imid(loopback);
	if (imid > 0) {
		g_xim_message_debug(G_XIM_PROTOCOL_GET_IFACE (proto)->message, "loopback/proto/event",
				    "XIM_OPEN[%s] from %p - imid: %d",
				    g_xim_str_get_string(locale),
				    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window),
				    imid);
		imattr = g_xim_im_attr_new(XNQueryInputStyle);
		icattr = g_xim_ic_attr_new(XNInputStyle ","		\
					   XNClientWindow ","		\
					   XNFocusWindow ","		\
					   XNPreeditAttributes ","	\
					   XNForeground ","		\
					   XNBackground ","		\
					   XNSpotLocation ","		\
					   XNFontSet ","		\
					   XNArea ","			\
					   XNLineSpace ","		\
					   XNStatusAttributes ","	\
					   XNAreaNeeded ","		\
					   XNColormap ","		\
					   XNStdColormap ","		\
					   XNBackgroundPixmap ","	\
					   XNCursor ","			\
					   XNFilterEvents ","		\
					   XNSeparatorofNestedList);
		conn->imid = imid;
		conn->imattr = imattr;
		conn->default_icattr = icattr;

		xim_loopback_set_default_im_values(loopback, imattr);
		xim_loopback_set_default_ic_values(loopback, icattr);

		attr = G_XIM_ATTR (imattr);
		list = g_xim_attr_get_supported_attributes(attr);
		for (l = list; l != NULL; l = g_slist_next(l)) {
			if (l->data && g_xim_attr_attribute_is_enabled(attr, l->data)) {
				guint id;
				GType gtype;
				GXimValueType vtype;
				GString *s;

				id = g_xim_attr_get_attribute_id(attr, l->data);
				gtype = g_xim_attr_get_gtype_by_name(attr, l->data);
				vtype = g_xim_gtype_to_value_type(gtype);
				if (vtype == G_XIM_TYPE_INVALID) {
					g_xim_message_bug(G_XIM_CORE (loopback)->message,
							  "Unable to compose a XIMATTR for %s",
							  (gchar *)l->data);
					continue;
				}
				s = g_string_new(l->data);
				G_XIM_CHECK_ALLOC (s, FALSE);

				raw = g_xim_raw_attr_new_with_value(id, s, vtype);
				G_XIM_CHECK_ALLOC (raw, FALSE);

				imlist = g_slist_append(imlist, raw);
			}
			g_free(l->data);
		}
		g_slist_free(list);

		attr = G_XIM_ATTR (icattr);
		list = g_xim_attr_get_supported_attributes(attr);
		for (l = list; l != NULL; l = g_slist_next(l)) {
			if (l->data && g_xim_attr_attribute_is_enabled(attr, l->data)) {
				guint id;
				GType gtype;
				GXimValueType vtype;
				GString *s;

				id = g_xim_attr_get_attribute_id(attr, l->data);
				gtype = g_xim_attr_get_gtype_by_name(attr, l->data);
				vtype = g_xim_gtype_to_value_type(gtype);
				if (vtype == G_XIM_TYPE_INVALID) {
					g_xim_message_bug(G_XIM_CORE (loopback)->message,
							  "Unable to compose a XICATTR for %s",
							  (gchar *)l->data);
					continue;
				}
				s = g_string_new(l->data);
				G_XIM_CHECK_ALLOC (s, FALSE);

				raw = g_xim_raw_attr_new_with_value(id, s, vtype);
				G_XIM_CHECK_ALLOC (raw, FALSE);

				iclist = g_slist_append(iclist, raw);
			}
			g_free(l->data);
		}
		g_slist_free(list);

		if (g_xim_server_connection_cmd_open_reply(G_XIM_SERVER_CONNECTION (proto),
							   imid, imlist, iclist)) {
			g_hash_table_insert(loopback->conn_table,
					    GUINT_TO_POINTER (imid),
					    conn);
		}

		g_slist_foreach(imlist, (GFunc)g_xim_raw_attr_free, NULL);
		g_slist_foreach(iclist, (GFunc)g_xim_raw_attr_free, NULL);
		g_slist_free(imlist);
		g_slist_free(iclist);
	} else {
		g_xim_message_warning(G_XIM_CORE (loopback)->message,
				      "No imid available for %p.",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
	}

	return imid > 0;
}

static gboolean
xim_loopback_real_xim_close(GXimProtocol  *proto,
			    guint16        imid,
			    gpointer       data)
{
	XimLoopback *loopback = XIM_LOOPBACK (data);
	GXimConnection *conn;
	GdkNativeWindow client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));

	if ((conn = g_hash_table_lookup(loopback->conn_table, GUINT_TO_POINTER (imid))) == NULL) {
		g_xim_message_warning(G_XIM_CORE (loopback)->message,
				      "Invalid imid `%d' from %p to close the connection.",
				      imid,
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	g_xim_message_debug(G_XIM_PROTOCOL_GET_IFACE (proto)->message, "loopback/proto/event",
			    "XIM_CLOSE[imid:%d] from %p",
			    imid,
			    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));

	g_xim_server_connection_cmd_close_reply(G_XIM_SERVER_CONNECTION (proto),
						imid);
	g_hash_table_remove(loopback->conn_table, GUINT_TO_POINTER (imid));

	return TRUE;
}

static gint
_cmp_str(gconstpointer a,
	 gconstpointer b)
{
	const GXimStr *ss = a;
	const gchar *s = g_xim_str_get_string(ss);

	return g_ascii_strcasecmp(s, b);
}

static gboolean
xim_loopback_real_xim_encoding_negotiation(GXimProtocol  *proto,
					   guint          imid,
					   const GSList  *encodings,
					   const GSList  *details,
					   gpointer       data)
{
	GSList *l;

	/* XXX: need to get rid of hard-coded encoding support */
	l = g_slist_find_custom((GSList *)encodings, "UTF-8", _cmp_str);
	if (l == NULL)
		return FALSE;

	return g_xim_server_connection_cmd_encoding_negotiation_reply(G_XIM_SERVER_CONNECTION (proto),
								      imid,
								      0,
								      g_slist_position((GSList *)encodings, l));
}

static gboolean
xim_loopback_real_xim_get_im_values(GXimProtocol  *proto,
				    guint16        imid,
				    const GSList  *attributes,
				    gpointer       data)
{
	XimLoopback *loopback = XIM_LOOPBACK (data);
	GXimConnection *conn;
	GXimIMAttr *attr = NULL;
	const GSList *l;
	GSList *list = NULL;
	GXimAttribute *a;
	GType gtype;
	GXimValueType vtype;
	gpointer value;
	GdkNativeWindow client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	gboolean retval;

	conn = g_hash_table_lookup(loopback->conn_table, GUINT_TO_POINTER (imid));
	if (!conn)
		return FALSE;

	attr = conn->imattr;
	for (l = attributes; l != NULL; l = g_slist_next(l)) {
		gtype = g_xim_attr_get_gtype_by_id(G_XIM_ATTR (attr),
						   GPOINTER_TO_INT (l->data));
		vtype = g_xim_gtype_to_value_type(gtype);
		if (vtype == G_XIM_TYPE_INVALID) {
			g_xim_message_warning(G_XIM_CORE (loopback)->message,
					      "Invalid attribute ID %d received from %p",
					      GPOINTER_TO_INT (l->data),
					      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
			continue;
		}
		value = g_xim_attr_get_value_by_id(G_XIM_ATTR (attr),
						   GPOINTER_TO_INT (l->data));
		g_xim_message_debug(G_XIM_CORE (loopback)->message, "loopback/proto/attr",
				    "IM Attribute %d: %p",
				    GPOINTER_TO_INT (l->data),
				    value);
		a = g_xim_attribute_new_with_value(GPOINTER_TO_INT (l->data), vtype, value);
		G_XIM_CHECK_ALLOC (a, FALSE);

		list = g_slist_append(list, a);
	}
	retval = g_xim_server_connection_cmd_get_im_values_reply(G_XIM_SERVER_CONNECTION (conn), imid, list);

	g_slist_foreach(list, (GFunc)g_xim_attribute_free, NULL);
	g_slist_free(list);

	return retval;
}

static gboolean
xim_loopback_real_xim_create_ic(GXimProtocol *proto,
				guint16       imid,
				const GSList *attributes,
				gpointer      data)
{
	XimLoopback *loopback = XIM_LOOPBACK (data);
	GXimConnection *conn;

	conn = g_hash_table_lookup(loopback->conn_table, GUINT_TO_POINTER (imid));
	if (!conn) {
		g_xim_connection_cmd_error(G_XIM_CONNECTION (proto),
					   imid, 0, G_XIM_EMASK_VALID_IMID,
					   G_XIM_ERR_BadProtocol,
					   0, "Invalid connection");
	} else {
		g_xim_connection_cmd_error(G_XIM_CONNECTION (proto),
					   imid, 0, G_XIM_EMASK_VALID_IMID,
					   G_XIM_ERR_BadSomething,
					   0, "Not supported.");
	}

	return TRUE;
}

static gboolean
xim_loopback_real_xim_disconnect(GXimProtocol *proto,
				 gpointer      data)
{
	GXimProtocolPrivate *priv;

	g_xim_server_connection_cmd_disconnect_reply(G_XIM_SERVER_CONNECTION (proto));

	priv = g_xim_protocol_get_private(proto);
	/* We have to set a flag explicitly, because the loopback class
	 * is working together with the proxy class in the same process.
	 * Which means the refcount of GdkWindow is kept. thus, DestroyNotify
	 * won't be raised.
	 */
	priv->is_disconnected = TRUE;

	return TRUE;
}

/*
 * Public functions
 */
XimLoopback *
xim_loopback_new(GdkDisplay  *dpy)
{
	XimLoopback *retval;

	g_return_val_if_fail (GDK_IS_DISPLAY (dpy), NULL);

	retval = XIM_LOOPBACK (g_object_new(XIM_TYPE_LOOPBACK,
					    "display", dpy,
					    "server_name", "loopback",
					    "connection_gtype", G_TYPE_XIM_SERVER_CONNECTION,
					    NULL));

	return retval;
}
