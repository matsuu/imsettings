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
#include <X11/Xlib.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <libgxim/gximattr.h>
#include <libgxim/gximmessage.h>
#include <libgxim/gximmisc.h>
#include <libgxim/gximprotocol.h>
#include <libgxim/gximtransport.h>
#include "loopback.h"

/*
 * Borrow an idea from IsModifierKey() in Xutil.h
 */
#define IS_MODIFIER_KEY(x)						\
	((((x) >= GDK_Shift_L) && ((x) <= GDK_Hyper_R)) ||		\
	 (((x) >= GDK_ISO_Lock) && ((x) <= GDK_ISO_Group_Lock)) ||	\
	 ((x) == GDK_Mode_switch) ||					\
	 ((x) == GDK_Num_Lock))

typedef struct _XimLoopbackQueueContainer {
	GXimProtocol *proto;
	guint16       imid;
	guint16       icid;
	guint16       flag;
	GdkEvent     *event;
} XimLoopbackQueueContainer;

enum {
	PROP_0,
	PROP_SYNCHRONOUS,
	LAST_PROP
};
enum {
	SIGNAL_0,
	LAST_SIGNAL
};

static gboolean xim_loopback_real_xim_disconnect          (GXimProtocol  *proto,
                                                           gpointer       data);
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
                                                           GSList        *attributes,
                                                           gpointer       data);
static gboolean xim_loopback_real_xim_set_ic_values       (GXimProtocol  *proto,
                                                           guint16        imid,
                                                           guint16        icid,
                                                           const GSList  *attributes,
                                                           gpointer       data);
static gboolean xim_loopback_real_xim_get_ic_values       (GXimProtocol  *proto,
							   guint16        imid,
							   guint16        icid,
							   const GSList  *attr_id,
							   gpointer       data);
static gboolean xim_loopback_real_xim_set_ic_focus        (GXimProtocol  *proto,
                                                           guint16        imid,
                                                           guint16        icid,
                                                           gpointer       data);
static gboolean xim_loopback_real_xim_unset_ic_focus      (GXimProtocol  *proto,
                                                           guint16        imid,
                                                           guint16        icid,
                                                           gpointer       data);
static gboolean xim_loopback_real_xim_forward_event       (GXimProtocol  *proto,
                                                           guint16        imid,
                                                           guint16        icid,
                                                           guint16        flag,
                                                           GdkEvent      *event,
                                                           gpointer       data);
static gboolean xim_loopback_real_xim_sync_reply          (GXimProtocol  *proto,
							   guint16        imid,
							   guint16        icid,
							   gpointer       data);
static gboolean _process_keyevent                         (gpointer       data);

//static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (XimLoopback, xim_loopback, G_TYPE_XIM_SRV_TMPL);
G_DEFINE_TYPE (XimLoopbackConnection, xim_loopback_connection, G_TYPE_XIM_SERVER_CONNECTION);

/*
 * Private functions
 */
static XimLoopbackIC *
xim_loopback_ic_new(void)
{
	XimLoopbackIC *retval = g_new0(XimLoopbackIC, 1);

	G_XIM_CHECK_ALLOC (retval, NULL);
	retval->keyeventq = g_queue_new();

	return retval;
}

static void
xim_loopback_ic_free(gpointer data)
{
	XimLoopbackIC *ic = data;

	if (ic) {
		g_queue_free(ic->keyeventq);
		g_free(ic);
	}
}

static void
xim_loopback_real_set_property(GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	XimLoopback *loopback = XIM_LOOPBACK (object);

	switch (prop_id) {
	    case PROP_SYNCHRONOUS:
		    loopback->sync_on_forward = g_value_get_boolean(value);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
xim_loopback_real_get_property(GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
	XimLoopback *loopback = XIM_LOOPBACK (object);

	switch (prop_id) {
	    case PROP_SYNCHRONOUS:
		    g_value_set_boolean(value, loopback->sync_on_forward);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

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

	object_class->set_property = xim_loopback_real_set_property;
	object_class->get_property = xim_loopback_real_get_property;
	object_class->finalize     = xim_loopback_real_finalize;

	core_class->setup_connection = xim_loopback_real_setup_connection;

	/* properties */
	g_object_class_install_property(object_class, PROP_SYNCHRONOUS,
					g_param_spec_boolean("synchronous",
							     _("Synchronous"),
							     _("Request to send a key event synchronously"),
							     TRUE,
							     G_PARAM_READWRITE));

	/* signals */
}

static void
xim_loopback_init(XimLoopback *loopback)
{
	GXimLazySignalConnector sigs[] = {
		{"XIM_DISCONNECT", G_CALLBACK (xim_loopback_real_xim_disconnect), loopback},
		{"XIM_OPEN", G_CALLBACK (xim_loopback_real_xim_open), loopback},
		{"XIM_CLOSE", G_CALLBACK (xim_loopback_real_xim_close), loopback},
		{"XIM_ENCODING_NEGOTIATION", G_CALLBACK (xim_loopback_real_xim_encoding_negotiation), loopback},
		{"XIM_GET_IM_VALUES", G_CALLBACK (xim_loopback_real_xim_get_im_values), loopback},
		{"XIM_CREATE_IC", G_CALLBACK (xim_loopback_real_xim_create_ic), loopback},
		{"XIM_SET_IC_VALUES", G_CALLBACK (xim_loopback_real_xim_set_ic_values), loopback},
		{"XIM_GET_IC_VALUES", G_CALLBACK (xim_loopback_real_xim_get_ic_values), loopback},
		{"XIM_SET_IC_FOCUS", G_CALLBACK (xim_loopback_real_xim_set_ic_focus), loopback},
		{"XIM_UNSET_IC_FOCUS", G_CALLBACK (xim_loopback_real_xim_unset_ic_focus), loopback},
		{"XIM_FORWARD_EVENT", G_CALLBACK (xim_loopback_real_xim_forward_event), loopback},
		{"XIM_SYNC_REPLY", G_CALLBACK (xim_loopback_real_xim_sync_reply), loopback},
		{NULL, NULL, NULL}
	};

	g_object_set(loopback, "proto_signals", sigs, NULL);

	loopback->conn_table = g_hash_table_new(g_direct_hash, g_direct_equal);
	loopback->latest_imid = 1;
}

static void
xim_loopback_connection_real_finalize(GObject *object)
{
	XimLoopbackConnection *conn = XIM_LOOPBACK_CONNECTION (object);

	compose_free(conn->composer);
	g_hash_table_destroy(conn->ic_table);
}

static void
xim_loopback_connection_class_init(XimLoopbackConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = xim_loopback_connection_real_finalize;
}

static void
xim_loopback_connection_init(XimLoopbackConnection *conn)
{
	conn->composer = compose_new();
	conn->ic_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, xim_loopback_ic_free);
	conn->latest_icid = 1;
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

static guint
xim_loopback_connection_find_icid(XimLoopbackConnection *conn)
{
	gboolean overflowed = FALSE;
	guint begin_id = conn->latest_icid;

	while (1) {
		if (g_hash_table_lookup(conn->ic_table, GUINT_TO_POINTER (conn->latest_icid)) == NULL)
			break;
		if (overflowed && conn->latest_icid >= begin_id)
			return 0;
		conn->latest_icid++;
		if (conn->latest_icid == 0) {
			overflowed = TRUE;
			conn->latest_icid = 1;
		}
	}

	return conn->latest_icid;
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
	XimLoopbackConnection *lconn = XIM_LOOPBACK_CONNECTION (proto);
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
		if (compose_open(lconn->composer, g_xim_str_get_string(locale))) {
			if (!compose_parse(lconn->composer)) {
				g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
						      "Unable to parse a compose file for %s. disabling.",
						      g_xim_str_get_string(locale));
			}
			compose_close(lconn->composer);
		} else {
			g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
					      "Unable to open a compose file for %s. disabling.",
					      g_xim_str_get_string(locale));
		}
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

	if ((conn = g_hash_table_lookup(loopback->conn_table, GUINT_TO_POINTER ((guint)imid))) == NULL) {
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
	g_hash_table_remove(loopback->conn_table, GUINT_TO_POINTER ((guint)imid));

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

	/* We only support COMPOUND_TEXT */
	/* XXX: need to get rid of hard-coded encoding support */
	l = g_slist_find_custom((GSList *)encodings, "COMPOUND_TEXT", _cmp_str);
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

	conn = g_hash_table_lookup(loopback->conn_table, GUINT_TO_POINTER ((guint)imid));
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
				GSList       *attributes,
				gpointer      data)
{
	XimLoopback *loopback = XIM_LOOPBACK (data);
	XimLoopbackConnection *lconn = XIM_LOOPBACK_CONNECTION (proto);
	XimLoopbackIC *ic;
	GXimConnection *conn;
	guint icid;

	conn = g_hash_table_lookup(loopback->conn_table, GUINT_TO_POINTER ((guint)imid));
	if (!conn) {
		g_xim_connection_cmd_error(G_XIM_CONNECTION (proto),
					   imid, 0, G_XIM_EMASK_VALID_IMID,
					   G_XIM_ERR_BadProtocol,
					   0, "Invalid connection");
	} else {
		icid = xim_loopback_connection_find_icid(lconn);
		if (icid > 0) {
			if (g_xim_server_connection_cmd_create_ic_reply(G_XIM_SERVER_CONNECTION (proto),
									imid, icid)) {
				GSList *list, *l;
				GXimAttr *attr = G_XIM_ATTR (G_XIM_CONNECTION (proto)->default_icattr);

				ic = xim_loopback_ic_new();
				G_XIM_CHECK_ALLOC (ic, FALSE);

				ic->icattr = g_xim_ic_attr_new(XNInputStyle "," \
							       XNClientWindow "," \
							       XNFocusWindow "," \
							       XNPreeditAttributes "," \
							       XNForeground ","	\
							       XNBackground ","	\
							       XNSpotLocation "," \
							       XNFontSet "," \
							       XNArea "," \
							       XNLineSpace "," \
							       XNStatusAttributes "," \
							       XNAreaNeeded ","	\
							       XNColormap "," \
							       XNStdColormap "," \
							       XNBackgroundPixmap "," \
							       XNCursor "," \
							       XNFilterEvents "," \
							       XNSeparatorofNestedList);

				list = g_xim_attr_get_supported_attributes(attr);
				for (l = list; l != NULL; l = g_slist_next(l)) {
					if (l->data && g_xim_attr_attribute_is_enabled(attr, l->data)) {
						guint id;
						GType gtype;
						GXimValueType vtype;
						GString *s;
						GXimRawAttr *raw;

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

						g_xim_attr_set_raw_attr(G_XIM_ATTR (ic->icattr), raw);

						g_xim_raw_attr_free(raw);
					}
					g_free(l->data);
				}
				g_slist_free(list);

				for (l = attributes; l != NULL; l = g_slist_next(l)) {
					GXimAttribute *a = l->data;
					gchar *name;

					name = g_xim_attr_get_attribute_name(G_XIM_ATTR (ic->icattr), a->id);
					g_object_set(ic->icattr, name, a->v.pointer, NULL);
				}
				g_hash_table_insert(lconn->ic_table,
						    GUINT_TO_POINTER (icid),
						    ic);
				/* XXX: need to get rid of X specific event mask? */
				g_xim_server_connection_cmd_set_event_mask(G_XIM_SERVER_CONNECTION (proto),
									   imid, icid,
									   KeyPressMask | KeyReleaseMask,
									   loopback->sync_on_forward ? ~NoEventMask : ~(KeyPressMask | KeyReleaseMask));
			} else {
				g_xim_connection_cmd_error(G_XIM_CONNECTION (proto),
							   imid, 0, G_XIM_EMASK_VALID_IMID,
							   G_XIM_ERR_BadSomething,
							   0, "Unable to deliver XIM_CREATE_IC_REPLY");
			}
		} else {
			g_xim_connection_cmd_error(G_XIM_CONNECTION (proto),
						   imid, 0, G_XIM_EMASK_VALID_IMID,
						   G_XIM_ERR_BadSomething,
						   0, "Unable to assign input-context ID");
		}
	}

	return TRUE;
}

static gboolean
xim_loopback_real_xim_set_ic_values(GXimProtocol *proto,
				    guint16       imid,
				    guint16       icid,
				    const GSList *attributes,
				    gpointer      data)
{
	XimLoopbackConnection *lconn = XIM_LOOPBACK_CONNECTION (proto);
	XimLoopbackIC *ic = g_hash_table_lookup(lconn->ic_table, GUINT_TO_POINTER ((guint)icid));
	const GSList *l;

	if (ic == NULL) {
		gchar *msg = g_strdup_printf("Invalid input-context ID: [%d,%d]", imid, icid);
		gboolean retval;

		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      msg);
		retval = g_xim_connection_cmd_error(G_XIM_CONNECTION (proto),
						    imid, icid, G_XIM_EMASK_VALID_IMID | G_XIM_EMASK_VALID_ICID,
						    G_XIM_ERR_BadProtocol,
						    0, msg);
		g_free(msg);

		return retval;
	}
	for (l = attributes; l != NULL; l = g_slist_next(l)) {
		GXimAttribute *a = l->data;
		gchar *name;

		name = g_xim_attr_get_attribute_name(G_XIM_ATTR (ic->icattr), a->id);
		g_object_set(ic->icattr, name, a->v.pointer, NULL);
	}

	return g_xim_server_connection_cmd_set_ic_values_reply(G_XIM_SERVER_CONNECTION (proto), imid, icid);
}

static gboolean
xim_loopback_real_xim_get_ic_values(GXimProtocol *proto,
				    guint16       imid,
				    guint16       icid,
				    const GSList *attr_id,
				    gpointer      data)
{
	XimLoopbackConnection *lconn = XIM_LOOPBACK_CONNECTION (proto);
	XimLoopbackIC *ic = g_hash_table_lookup(lconn->ic_table, GUINT_TO_POINTER ((guint)icid));
	const GSList *l;
	GSList *list = NULL;
	gboolean retval = FALSE;
	GXimNestedList *nested = NULL;
	guint16 nid = 0;
	GXimAttribute *a;

	if (ic == NULL) {
		gchar *msg = g_strdup_printf("Invalid input-context ID: [%d,%d]", imid, icid);
		gboolean retval;

		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      msg);
		retval = g_xim_connection_cmd_error(G_XIM_CONNECTION (proto),
						    imid, icid, G_XIM_EMASK_VALID_IMID | G_XIM_EMASK_VALID_ICID,
						    G_XIM_ERR_BadProtocol,
						    0, msg);
		g_free(msg);

		return retval;
	}
	for (l = attr_id; l != NULL; l = g_slist_next(l)) {
		guint16 id = GPOINTER_TO_UINT (l->data);
		GType gtype;
		GXimValueType vtype;
		gpointer data;

		gtype = g_xim_attr_get_gtype_by_id(G_XIM_ATTR (ic->icattr), id);
		vtype = g_xim_gtype_to_value_type(gtype);
		if (vtype == G_XIM_TYPE_NESTEDLIST) {
			nid = id;
			nested = g_xim_nested_list_new(G_XIM_ATTR (ic->icattr), 0);
			continue;
		} else if (nested && vtype == G_XIM_TYPE_SEPARATOR) {
			g_xim_nested_list_append(nested, XNSeparatorofNestedList, NULL);
			a = g_xim_attribute_new_with_value(nid, G_XIM_TYPE_NESTEDLIST, nested);
			nested = NULL;
			nid = 0;
		} else if (nested) {
			gchar *name = g_xim_attr_get_attribute_name(G_XIM_ATTR (ic->icattr),
								    id);

			data = g_xim_attr_get_value_by_name(G_XIM_ATTR (ic->icattr), name);
			g_xim_nested_list_append(nested, name, data);
			g_free(name);
			continue;
		} else {
			data = g_xim_attr_get_value_by_id(G_XIM_ATTR (ic->icattr), id);
			a = g_xim_attribute_new_with_value(id, vtype, data);
		}
		list = g_slist_append(list, a);
	}
	if (nested) {
		a = g_xim_attribute_new_with_value(nid, G_XIM_TYPE_NESTEDLIST, nested);
		list = g_slist_append(list, a);
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "No separator found in NESTEDLIST. is might be highly likely a bug in the client application.");
	}
	retval = g_xim_server_connection_cmd_get_ic_values_reply(G_XIM_SERVER_CONNECTION (proto),
								 imid, icid, list);

	g_slist_foreach(list, (GFunc)g_xim_attribute_free, NULL);
	g_slist_free(list);

	return retval;
}

static gboolean
xim_loopback_real_xim_set_ic_focus(GXimProtocol *proto,
				   guint16       imid,
				   guint16       icid,
				   gpointer      data)
{
	/* Nothing to do */
	return TRUE;
}

static gboolean
xim_loopback_real_xim_unset_ic_focus(GXimProtocol *proto,
				     guint16       imid,
				     guint16       icid,
				     gpointer      data)
{
	/* Nothing to do */
	return TRUE;
}

static gboolean
xim_loopback_real_xim_forward_event(GXimProtocol *proto,
				    guint16       imid,
				    guint16       icid,
				    guint16       flag,
				    GdkEvent     *event,
				    gpointer      data)
{
	XimLoopbackConnection *lconn = XIM_LOOPBACK_CONNECTION (proto);
	XimLoopbackIC *ic = g_hash_table_lookup(lconn->ic_table, GUINT_TO_POINTER ((guint)icid));
	XimLoopback *loopback = XIM_LOOPBACK (data);
	GdkDisplay *dpy = g_xim_core_get_display(G_XIM_CORE (loopback));
	gchar *string = NULL;
	gulong keysym = 0;
	guint16 sflag = 0;
	gboolean retval = FALSE;

	if (ic == NULL) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Invalid input-context ID: %d",
				      icid);
		goto end;
	}
	if (!ic->resend &&
	    (ic->wait_for_reply || g_queue_get_length(ic->keyeventq) > 0)) {
		XimLoopbackQueueContainer *c = g_new0(XimLoopbackQueueContainer, 1);

		c->proto = g_object_ref(proto);
		c->imid = imid;
		c->icid = icid;
		c->flag = flag;
		c->event = gdk_event_copy(event);
		g_queue_push_tail(ic->keyeventq, c);
		g_xim_message_debug(G_XIM_PROTOCOL_GET_IFACE (proto)->message, "loopback/proto/event",
				    "Queueing a keyevent. (imid: %d, icid: %d, type: %s, keyval: %X)",
				    imid, icid,
				    event->type == GDK_KEY_PRESS ? "KeyPress" : "KeyRelease",
				    event->key.keyval);

		return TRUE;
	}

	if (IS_MODIFIER_KEY (event->key.keyval))
		goto end;
	if (event->type == GDK_KEY_RELEASE)
		goto end;

	if ((flag & G_XIM_Event_Synchronous) == 0)
		sflag = G_XIM_Event_Synchronous;
		
	d(g_print("\n**** %s: %d\n\n", gdk_keyval_name(event->key.keyval), event->key.state));
	if (compose_lookup(lconn->composer, &ic->sequence_state,
			   event->key.keyval, event->key.state,
			   &string, &keysym)) {
		g_xim_message_debug(G_XIM_PROTOCOL_GET_IFACE (proto)->message, "loopback/proto/event",
				    "Entering the compose sequence: %s",
				    gdk_keyval_name(event->key.keyval));
		if (ic->sequence_state->candidates == NULL) {
			GString *s = g_string_new(NULL);
			guchar *ctext = NULL;
			gint len = 0;
			GXimLookupType lookup_type = G_XIM_XLookupChars;

			if (string) {
				gdk_string_to_compound_text_for_display(dpy, string, NULL, NULL, &ctext, &len);
				g_string_append_len(s, (gchar *)ctext, len);
				g_free(ctext);
			}

			/* XXX: need to look at the keymap? */
			if (string == NULL)
				lookup_type = G_XIM_XLookupKeySym;
			retval = g_xim_server_connection_cmd_commit(G_XIM_SERVER_CONNECTION (proto),
								    imid, icid,
								    (sflag ? G_XIM_XLookupSynchronous : 0) | lookup_type,
								    keysym, s);
			/* Ensure that we'll try to find out a sequence from the beginning next time */
			ic->sequence_state = NULL;

			d(g_print("result: %s [%s]: 0x%x\n", gdk_keyval_name(keysym), string, event->key.hardware_keycode));

			g_string_free(s, TRUE);
		} else {
			event->key.state = 0;
			event->key.keyval = GDK_VoidSymbol;
			event->key.hardware_keycode = 0;
		}
		g_free(string);
	} else {
		ic->sequence_state = NULL;
	}

  end:
	if (!retval)
		retval = g_xim_connection_cmd_forward_event(G_XIM_CONNECTION (proto),
							    imid, icid, sflag, event);
	if (flag & G_XIM_Event_Synchronous) {
		g_xim_connection_cmd_sync_reply(G_XIM_CONNECTION (proto), imid, icid);
		/* sending XIM_SYNC_REPLY usually means synchronization is done. */
		ic->wait_for_reply = FALSE;
	} else if (sflag & G_XIM_Event_Synchronous) {
		ic->wait_for_reply = TRUE;
	}
	if (!ic->wait_for_reply && g_queue_get_length(ic->keyeventq)) {
		g_idle_add_full(G_PRIORITY_HIGH_IDLE, _process_keyevent, ic->keyeventq, NULL);
	}

	return retval;
}

static gboolean
xim_loopback_real_xim_sync_reply(GXimProtocol *proto,
				 guint16       imid,
				 guint16       icid,
				 gpointer      data)
{
	XimLoopbackConnection *lconn = XIM_LOOPBACK_CONNECTION (proto);
	XimLoopbackIC *ic = g_hash_table_lookup(lconn->ic_table, GUINT_TO_POINTER ((guint)icid));
	gboolean flag = g_atomic_int_get(&ic->wait_for_reply);

  retry:
	if (flag) {
		if (!g_atomic_int_compare_and_exchange(&ic->wait_for_reply, flag, FALSE))
			goto retry;
	}
	if (g_queue_get_length(ic->keyeventq) > 0) {
		g_idle_add_full(G_PRIORITY_HIGH_IDLE, _process_keyevent, ic->keyeventq, NULL);
	}

	return TRUE;
}

static gboolean
_process_keyevent(gpointer data)
{
	GQueue *q = data;
	XimLoopbackQueueContainer *c;
	XimLoopbackConnection *lconn;
	XimLoopbackIC *ic;
	gboolean retval;
	GClosure *closure;

	c = g_queue_pop_head(q);
	closure = (GClosure *)g_xim_protocol_lookup_protocol_by_id(c->proto, G_XIM_FORWARD_EVENT, 0);
	if (closure == NULL) {
		g_xim_message_bug(G_XIM_PROTOCOL_GET_IFACE (c->proto)->message,
				  "No closure to re-send back a XIM_FORWARD_EVENT.");
	} else {
		lconn = XIM_LOOPBACK_CONNECTION (c->proto);
		ic = g_hash_table_lookup(lconn->ic_table, GUINT_TO_POINTER ((guint)c->icid));
		ic->resend = TRUE;
		g_xim_message_debug(G_XIM_PROTOCOL_GET_IFACE (c->proto)->message, "loopback/proto/event",
				    "Re-processing XIM_FORWARD_EVENT (imid: %d, icid: %d, type: %s, keyval: %X)",
				    c->imid, c->icid,
				    c->event->type == GDK_KEY_PRESS ? "KeyPress" : "KeyRelease",
				    c->event->key.keyval);
		retval = g_xim_protocol_closure_emit_signal((GXimProtocolClosure *)closure,
							    c->proto,
							    c->imid, c->icid, c->flag, c->event);
		ic->resend = FALSE;
		if (!retval) {
			g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (c->proto)->message,
					      "Unable to re-send back a XIM_FORWARD_EVENT. this event will be discarded.");
		}
	}
	g_object_unref(c->proto);
	gdk_event_free(c->event);
	g_free(c);

	return FALSE;
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
					    "connection_gtype", XIM_TYPE_LOOPBACK_CONNECTION,
					    NULL));

	return retval;
}
