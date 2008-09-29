/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * proxy.c
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

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <glib/gi18n-lib.h>
#include <gdk/gdkx.h>
#include <libgxim/gximattr.h>
#include <libgxim/gximerror.h>
#include <libgxim/gximmessage.h>
#include <libgxim/gximmisc.h>
#include <libgxim/gximprotocol.h>
#include <libgxim/gximtransport.h>
#include "client.h"
#include "loopback.h"
#include "proxy.h"

#define INC_PENDING(_p_)						\
	G_STMT_START {							\
		if ((_p_)->pending_tasks < G_MAXULONG) {		\
			(_p_)->pending_tasks++;				\
			g_xim_message_debug(G_XIM_CORE (_p_)->message, "proxy/task", \
					    "%s: pending tasks: %ld\n",	\
					    __FUNCTION__, (_p_)->pending_tasks); \
		} else {						\
			g_xim_message_error(G_XIM_CORE (_p_)->message,	\
					    "%s: Pending tasks counter is overflowed", \
					    __FUNCTION__);		\
		}							\
	} G_STMT_END
#define DEC_PENDING(_p_)						\
	G_STMT_START {							\
		if ((_p_)->pending_tasks > 0) {				\
			(_p_)->pending_tasks--;				\
			g_xim_message_debug(G_XIM_CORE (_p_)->message, "proxy/task", \
					    "%s: pending tasks: %ld\n", \
					    __FUNCTION__, (_p_)->pending_tasks); \
		} else {						\
			g_xim_message_error(G_XIM_CORE (_p_)->message,	\
					    "%s: Pending tasks counter is screwed up", \
					    __FUNCTION__);		\
		}							\
	} G_STMT_END

enum {
	PROP_0,
	PROP_CONNECT_TO,
	PROP_CLIENT_PROTO_SIGNALS,
	LAST_PROP
};
enum {
	SIGNAL_0,
	LAST_SIGNAL
};


static GXimServerConnection *_get_server_connection                   (XimProxy             *proxy,
                                                                       GXimProtocol         *proto);
static gboolean              xim_proxy_client_real_notify_locales_cb  (GXimClientTemplate   *client,
                                                                       gchar               **locales,
                                                                       gpointer              user_data);
static gboolean              xim_proxy_client_real_notify_transport_cb(GXimClientTemplate   *client,
                                                                       gchar               **transport,
                                                                       gpointer              user_data);
static gboolean              xim_proxy_client_real_xconnect_cb        (GXimClientTemplate   *client,
                                                                       GdkEventClient       *event,
                                                                       gpointer              data);


//static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (XimProxy, xim_proxy, G_TYPE_XIM_SRV_TMPL);
G_DEFINE_TYPE (XimProxyConnection, xim_proxy_connection, G_TYPE_XIM_SERVER_CONNECTION);

/*
 * Private functions
 */
static guint16
_find_free_imid(XimProxy *proxy,
		guint16   imid)
{
	guint16 retval = imid, tmp;
	gboolean overflowed = FALSE;

	if (proxy->cimid_table[imid] != 0) {
		tmp = proxy->latest_imid;

		while (1) {
			if (proxy->cimid_table[tmp] == 0)
				break;
			if (overflowed && tmp == proxy->latest_imid)
				return 0;
			tmp++;
			if (tmp == 0) {
				overflowed = TRUE;
				tmp = 1;
			}
		}
		retval = proxy->latest_imid = tmp;
	}

	return retval;
}

static void
_link_imid(XimProxy *proxy,
	   guint16   simid,
	   guint16   cimid)
{
	proxy->simid_table[cimid] = simid;
	proxy->cimid_table[simid] = cimid;
	g_xim_message_debug(G_XIM_CORE (proxy)->message, "proxy/proto",
			    "imid link: %d <-> %d\n",
			    simid, cimid);
}

static guint16
_get_server_imid(XimProxy *proxy,
		 guint16   imid)
{
	guint16 retval;

	retval = proxy->simid_table[imid];
	if (retval == 0) {
		g_xim_message_warning(G_XIM_CORE (proxy)->message,
				      "No links for imid %d", imid);

		return 0;
	}

	return retval;
}

static guint16
_get_client_imid(XimProxy *proxy,
		 guint16   imid)
{
	guint16 retval;

	retval = proxy->cimid_table[imid];
	if (retval == 0) {
		g_xim_message_warning(G_XIM_CORE (proxy)->message,
				      "No links for imid %d", imid);

		return 0;
	}

	return retval;
}

static void
_remove_imid(XimProxy *proxy,
	     guint16   imid)
{
	guint16 simid = _get_server_imid(proxy, imid);

	proxy->cimid_table[simid] = 0;
	proxy->simid_table[imid] = 0;
}

static gboolean
_reconnect(XimProxy             *proxy,
	   GXimClientConnection *conn)
{
	GXimServerConnection *sconn;
	gboolean retval = TRUE;
	guint imid;

	switch (XIM_CLIENT_CONNECTION (conn)->reconnection_state) {
	    case 0:
		    XIM_CLIENT_CONNECTION (conn)->is_reconnecting = TRUE;
		    break;
	    case 1:
		    /* XIM_CONNECT */
		    /* XXX */
		    retval = g_xim_client_connection_cmd_connect(conn, 1, 0, NULL, TRUE);
		    break;
	    case 2:
		    /* XIM_OPEN */
		    sconn = _get_server_connection(proxy, G_XIM_PROTOCOL (conn));
		    retval = g_xim_client_connection_cmd_open_im(conn,
								 XIM_PROXY_CONNECTION (sconn)->locale,
								 TRUE);
		    break;
	    case 3:
		    /* XIM_ENCODING_NEGOTIATION */
		    G_STMT_START {
			    GSList *e = NULL, *d = NULL;
			    GValueArray *a;
			    guint i;

			    sconn = _get_server_connection(proxy, G_XIM_PROTOCOL (conn));
			    a = G_XIM_CONNECTION (sconn)->encodings;
			    for (i = 0; i < a->n_values; i++) {
				    GValue *v = g_value_array_get_nth(a, i);
				    gpointer p = g_value_get_boxed(v);

				    e = g_slist_append(e, p);
			    }
			    a = G_XIM_CONNECTION (sconn)->encoding_details;
			    for (i = 0; i < a->n_values; i++) {
				    GValue *v = g_value_array_get_nth(a, i);
				    gpointer p = g_value_get_boxed(v);

				    e = g_slist_append(e, p);
			    }
			    imid = _get_client_imid(proxy, G_XIM_CONNECTION (sconn)->imid);
			    retval = g_xim_client_connection_cmd_encoding_negotiation(conn, imid, e, d, TRUE);

			    g_slist_free(e);
			    g_slist_free(d);
		    } G_STMT_END;
		    break;
	    case 4:
		    /* XIM_SET_IM_VALUES */
#if 0
		    G_STMT_START {
			    GXimAttr *attr;
			    GSList *attrs, *l, *imlist = NULL;

			    sconn = _get_server_connection(proxy, G_XIM_PROTOCOL (conn));
			    attr = G_XIM_ATTR (G_XIM_CONNECTION (sconn)->imattr);
			    attrs = g_xim_attr_get_supported_attributes(attr);
			    for (l = attrs; l != NULL; l = g_slist_next(l)) {
				    if (l->data &&
					g_xim_attr_attribute_is_enabled(attr, l->data)) {
					    guint16 sid, id;
					    GType gtype;
					    GXimValueType vtype;
					    gpointer val;
					    GXimAttribute *a;

					    sid = g_xim_attr_get_attribute_id(attr, l->data);
					    if ((id = GPOINTER_TO_UINT (g_hash_table_lookup(proxy->cimattr_table, l->data))) == 0) {
						    g_xim_message_warning(G_XIM_CORE (proxy)->message,
									  "Unknown IM attribute id %d [%s]",
									  sid, (gchar *)l->data);
						    continue;
					    }
					    gtype = g_xim_attr_get_gtype_by_name(attr, l->data);
					    vtype = g_xim_gtype_to_value_type(gtype);
					    if (vtype == G_XIM_TYPE_INVALID) {
						    g_xim_message_bug(G_XIM_CORE (proxy)->message,
								      "Unable to compose a XIMATTRIBUTE for %s",
								      (gchar *)l->data);
						    continue;
					    }
					    val = g_xim_attr_get_value_by_name(attr, l->data);
					    a = g_xim_attribute_new_with_value(id - 1, vtype, val);
					    if (a)
						    imlist = g_slist_append(imlist, a);
				    }
			    }
			    imid = _get_client_imid(proxy, G_XIM_CONNECTION (sconn)->imid);
			    retval = g_xim_client_connection_set_im_values(conn, imid, imlist, TRUE);

			    g_slist_foreach(attrs,
					    (GFunc)g_free,
					    NULL);
			    g_slist_free(attrs);
		    } G_STMT_END;
		    break;
#endif
	    case 5:
		    XIM_CLIENT_CONNECTION (conn)->is_reconnecting = FALSE;
		    g_xim_message_debug(G_XIM_CORE (proxy)->message, "proxy/event",
					"Reconnection finished");
		    break;
	    default:
		    break;
	}
	if (XIM_CLIENT_CONNECTION (conn)->is_reconnecting)
		XIM_CLIENT_CONNECTION (conn)->reconnection_state++;
	else
		XIM_CLIENT_CONNECTION (conn)->reconnection_state = 0;

	return retval;
}

static XimClient *
_create_client(XimProxy        *proxy,
	       GdkNativeWindow  client_window)
{
	GXimCore *core = G_XIM_CORE (proxy);
	XimClient *client;
	GdkDisplay *dpy = g_xim_core_get_display(core);
	GdkNativeWindow nw;
	const gchar *real_server_name;
	gboolean retried = FALSE;

	if (strcmp(proxy->connect_to, "none") == 0) {
	  fallback:
		real_server_name = G_XIM_SRV_TMPL (proxy->default_server)->server_name;
	} else {
		real_server_name = proxy->connect_to;
	}
	client = xim_client_new(dpy, real_server_name);
	if (client == NULL) {
		g_xim_message_critical(core->message,
				       "Unable to create a client instance.");
		if (retried)
			return NULL;
		else
			goto fallback;
	}
	g_signal_connect(client, "notify_locales",
			 G_CALLBACK (xim_proxy_client_real_notify_locales_cb),
			 proxy);
	g_signal_connect(client, "notify_transport",
			 G_CALLBACK (xim_proxy_client_real_notify_transport_cb),
			 proxy);
	g_xim_message_debug(core->message, "client/conn",
			    "Inserting a client connection %p to the table with %p",
			    client,
			    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
	g_hash_table_insert(proxy->client_table,
			    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window),
			    client);
	nw = GDK_WINDOW_XID (g_xim_core_get_selection_window(G_XIM_CORE (client)));
	g_hash_table_insert(proxy->selection_table,
			    G_XIM_NATIVE_WINDOW_TO_POINTER (nw),
			    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));

	return client;
}

static GXimClientConnection *
_get_client_connection(XimProxy     *proxy,
		       GXimProtocol *proto)
{
	GXimTransport *trans = G_XIM_TRANSPORT (proto);
	GdkNativeWindow client_window;
	XimClient *client;
	GXimClientTemplate *cltmpl;

	client_window = g_xim_transport_get_client_window(trans);
	client = g_hash_table_lookup(proxy->client_table,
				     G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
	if (client == NULL) {
		/* Whether or not the connection is valid has already been
		 * checked in GXimServerTemplate. so this simply usually means
		 * the XIM server was changed.
		 */
		client = _create_client(proxy, client_window);
		g_object_set(G_OBJECT (client), "proto_signals", proxy->client_proto_signals, NULL);
		g_signal_connect(client, "xconnect",
				 G_CALLBACK (xim_proxy_client_real_xconnect_cb),
				 proxy);
	}
	cltmpl = G_XIM_CL_TMPL (client);
	if (client->is_reconnecting) {
		while (!g_xim_cl_tmpl_is_initialized(cltmpl))
			g_main_context_iteration(NULL, TRUE);
		while (XIM_CLIENT_CONNECTION (cltmpl->connection)->is_reconnecting)
			g_main_context_iteration(NULL, TRUE);
	}
	if (!g_xim_cl_tmpl_is_initialized(cltmpl)) {
		GError *error = NULL;
		GdkNativeWindow comm_window;
		GXimCore *core = G_XIM_CORE (proxy);

		client->is_reconnecting = TRUE;
		INC_PENDING (proxy);
		if (!g_xim_cl_tmpl_connect_to_server(cltmpl, &error)) {
			if (error) {
				g_xim_message_gerror(core->message, error);
				g_error_free(error);
			} else {
				g_xim_message_warning(core->message,
						      "Waiting for reconnecting on other thread.");
			}
			DEC_PENDING (proxy);

			return NULL;
		}
		_reconnect(proxy, G_XIM_CLIENT_CONNECTION (cltmpl->connection));
		comm_window = g_xim_transport_get_native_channel(G_XIM_TRANSPORT (cltmpl->connection));
		g_hash_table_insert(proxy->comm_table,
				    G_XIM_NATIVE_WINDOW_TO_POINTER (comm_window),
				    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		g_hash_table_insert(proxy->sconn_table,
				    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window),
				    proto);
		g_xim_message_debug(core->message, "proxy/event",
				    "%p-> XIM_XCONNECT [reconnect]",
				    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		G_XIM_CL_TMPL (client)->is_connection_initialized = GXC_NEGOTIATING;

		while (!g_xim_cl_tmpl_is_initialized(cltmpl))
			g_main_context_iteration(NULL, TRUE);
		while (XIM_CLIENT_CONNECTION (cltmpl->connection)->is_reconnecting)
			g_main_context_iteration(NULL, TRUE);

		client->is_reconnecting = FALSE;
	}

	return G_XIM_CLIENT_CONNECTION (G_XIM_CL_TMPL (client)->connection);
}

static GXimServerConnection *
_get_server_connection(XimProxy     *proxy,
		       GXimProtocol *proto)
{
	GXimTransport *trans;
	GdkNativeWindow nw, comm_window;

	trans = G_XIM_TRANSPORT (proto);
	nw = g_xim_transport_get_native_channel(trans);
	comm_window = G_XIM_POINTER_TO_NATIVE_WINDOW (g_hash_table_lookup(proxy->comm_table,
									  G_XIM_NATIVE_WINDOW_TO_POINTER (nw)));
	return g_hash_table_lookup(proxy->sconn_table,
				   G_XIM_NATIVE_WINDOW_TO_POINTER (comm_window));
}

static gboolean
xim_proxy_client_real_notify_locales_cb(GXimClientTemplate  *client,
					gchar              **locales,
					gpointer             user_data)
{
	XimProxy *proxy = XIM_PROXY (user_data);
	GXimCore *core = G_XIM_CORE (client);
	GdkWindow *w;
	gchar *prop = NULL, *s = NULL;
	gboolean retval = TRUE;
	GdkNativeWindow client_window = 0, selection_window = 0;
	GdkEventSelection ev;

	w = g_xim_core_get_selection_window(core);
	selection_window = GDK_WINDOW_XID (w);
	client_window = G_XIM_POINTER_TO_NATIVE_WINDOW (g_hash_table_lookup(proxy->selection_table,
									    G_XIM_NATIVE_WINDOW_TO_POINTER (selection_window)));
	if (client_window == 0) {
		g_xim_message_warning(core->message,
				      "Received SelectionNotify from unknown sender: %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (selection_window));
		goto end;
	}

	ev.requestor = client_window;
	ev.selection = client->atom_server;
	ev.target = core->atom_locales;
	ev.property = core->atom_locales;
	ev.requestor = client_window;
	if (locales)
		s = g_strjoinv(",", locales);
	prop = g_strdup_printf("@locale=%s", s);
	g_free(s);
	g_xim_message_debug(core->message, "proxy/event",
			    "%p <-%p<- SelectionNotify[%s]",
			    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window),
			    G_XIM_NATIVE_WINDOW_TO_POINTER (selection_window),
			    prop);
	retval = g_xim_srv_tmpl_send_selection_notify(G_XIM_SRV_TMPL (proxy), &ev, prop, strlen(prop) + 1, NULL);
  end:
	g_xim_message_debug(core->message, "client/conn",
			    "Removing a client connection from the table for %p",
			    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
	g_hash_table_remove(proxy->client_table,
			    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
	g_hash_table_remove(proxy->selection_table,
			    G_XIM_NATIVE_WINDOW_TO_POINTER (selection_window));
	g_free(prop);
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_real_notify_transport_cb(GXimClientTemplate  *client,
					  gchar              **transport,
					  gpointer             user_data)
{
	XimProxy *proxy = user_data;
	GXimCore *core = G_XIM_CORE (client);
	GdkWindow *w;
	gchar *prop = NULL, *s = NULL;
	gboolean retval = TRUE;
	GdkNativeWindow client_window = 0, selection_window = 0;
	GdkEventSelection ev;

	w = g_xim_core_get_selection_window(core);
	selection_window = GDK_WINDOW_XID (w);
	client_window = G_XIM_POINTER_TO_NATIVE_WINDOW (g_hash_table_lookup(proxy->selection_table,
									    G_XIM_NATIVE_WINDOW_TO_POINTER (selection_window)));
	if (client_window == 0) {
		g_xim_message_warning(core->message,
				      "Received SelectionNotify from unknown sender: %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (selection_window));
		goto end;
	}

	ev.requestor = client_window;
	ev.selection = client->atom_server;
	ev.target = core->atom_transport;
	ev.property = core->atom_transport;
	ev.requestor = client_window;
	if (transport)
		s = g_strjoinv(",", transport);
	prop = g_strdup_printf("@transport=%s", s);
	g_free(s);
	g_xim_message_debug(core->message, "proxy/event",
			    "%p <-%p<- SelectionNotify[%s]",
			    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window),
			    G_XIM_NATIVE_WINDOW_TO_POINTER (selection_window),
			    prop);
	retval = g_xim_srv_tmpl_send_selection_notify(G_XIM_SRV_TMPL (proxy), &ev, prop, strlen(prop) + 1, NULL);
  end:
	g_xim_message_debug(core->message, "client/conn",
			    "Removing a client connection from the table for %p",
			    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
	g_hash_table_remove(proxy->client_table,
			    G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
	g_hash_table_remove(proxy->selection_table,
			    G_XIM_NATIVE_WINDOW_TO_POINTER (selection_window));
	g_free(prop);
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_real_xconnect_cb(GXimClientTemplate *client,
				  GdkEventClient     *event,
				  gpointer            data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimCore *core = G_XIM_CORE (proxy);
	GXimConnection *cconn, *sconn;
	GXimTransport *ctrans, *strans;
	GdkNativeWindow nw, comm_window;
	GdkDisplay *dpy = g_xim_core_get_display(core);
	GdkEvent *ev;

	nw = GDK_WINDOW_XID (event->window);
	comm_window = G_XIM_POINTER_TO_NATIVE_WINDOW (g_hash_table_lookup(proxy->comm_table,
									  G_XIM_NATIVE_WINDOW_TO_POINTER (nw)));
	if (comm_window == 0) {
		g_xim_message_warning(core->message,
				      "Got a response of XIM_XCONNECT for unknown client: %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (nw));
		return TRUE;
	}
	sconn = g_xim_srv_tmpl_lookup_connection_with_native_window(G_XIM_SRV_TMPL (proxy), comm_window);
	if (sconn == NULL) {
		g_xim_message_warning(core->message,
				      "No connection %p for a response of XIM_XCONNECT",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (comm_window));
		return TRUE;
	}
	cconn = client->connection;
	strans = G_XIM_TRANSPORT (sconn);
	ctrans = G_XIM_TRANSPORT (cconn);
	g_xim_transport_set_version(strans, event->data.l[1], event->data.l[2]);
	g_xim_transport_set_version(ctrans, event->data.l[1], event->data.l[2]);
	g_xim_transport_set_transport_max(strans, event->data.l[3]);
	g_xim_transport_set_transport_max(ctrans, event->data.l[3]);
	g_xim_transport_set_client_window(ctrans, event->data.l[0]);
	g_xim_core_add_client_message_filter(core,
					     g_xim_transport_get_atom(strans));
	g_xim_core_add_client_message_filter(G_XIM_CORE (client),
					     g_xim_transport_get_atom(ctrans));

	if (XIM_CLIENT (client)->is_reconnecting) {
		g_xim_message_debug(core->message, "proxy/event",
				    "%p<- XIM_XCONNECT[%p] [reconnected]",
				    G_XIM_NATIVE_WINDOW_TO_POINTER (nw),
				    G_XIM_NATIVE_WINDOW_TO_POINTER (event->data.l[0]));
		INC_PENDING (proxy);

		_reconnect(proxy, G_XIM_CLIENT_CONNECTION (cconn));
	} else {
		ev = gdk_event_new(GDK_CLIENT_EVENT);
		ev->client.window = g_object_ref(event->window);
		ev->client.message_type = event->message_type;
		ev->client.data_format = 32;
		ev->client.data.l[0] = (long)g_xim_transport_get_native_channel(strans);
		ev->client.data.l[1] = event->data.l[1];
		ev->client.data.l[2] = event->data.l[2];
		ev->client.data.l[3] = event->data.l[3];

		g_xim_message_debug(core->message, "proxy/event",
				    "%p <-%p<- XIM_XCONNECT[%p]",
				    G_XIM_NATIVE_WINDOW_TO_POINTER (comm_window),
				    G_XIM_NATIVE_WINDOW_TO_POINTER (nw),
				    G_XIM_NATIVE_WINDOW_TO_POINTER (event->data.l[0]));
		gdk_event_send_client_message_for_display(dpy, ev, comm_window);

		gdk_event_free(ev);
	}
	client->is_connection_initialized = GXC_ESTABLISHED;
	DEC_PENDING (proxy);

	return TRUE;
}

static gboolean
xim_proxy_client_protocol_real_xim_connect_reply(GXimProtocol *proto,
						 guint16       major_version,
						 guint16       minor_version,
						 gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval;

	conn = _get_server_connection(proxy, proto);

	if (XIM_CLIENT_CONNECTION (proto)->is_reconnecting) {
		INC_PENDING (proxy);

		_reconnect(proxy, G_XIM_CLIENT_CONNECTION (proto));
		retval = TRUE;
	} else {
		retval = g_xim_server_connection_cmd_connect_reply(conn, major_version, minor_version);
	}
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_disconnect_reply(GXimProtocol *proto,
						    gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval;

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_server_connection_cmd_disconnect_reply(conn);
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_auth_ng(GXimProtocol *proto,
					   gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval;

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_connection_cmd_auth_ng(G_XIM_CONNECTION (conn));

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_error(GXimProtocol  *proto,
					 guint16        imid,
					 guint16        icid,
					 GXimErrorMask  flag,
					 GXimErrorCode  error_code,
					 guint16        detail,
					 const gchar   *error_message,
					 gpointer       data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval;
	guint16 simid = _get_server_imid(proxy, imid);

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_connection_cmd_error(G_XIM_CONNECTION (conn), simid, icid, flag, error_code, detail, error_message);
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_open_reply(GXimProtocol *proto,
					      guint16       imid,
					      GXimIMAttr   *imattr,
					      GXimICAttr   *icattr,
					      gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	GXimConnection *c, *sc;
	GdkNativeWindow client_window;
	GXimClientTemplate *client;
	GXimAttr *attr;
	GXimRawAttr *raw;
	GSList *l, *imlist = NULL, *iclist = NULL, *list;
	gboolean retval;
	guint16 simid;

	conn = _get_server_connection(proxy, proto);
	sc = G_XIM_CONNECTION (conn);
	if (sc->imid != 0 &&
	    XIM_CLIENT_CONNECTION (proto)->is_reconnecting) {
		simid = sc->imid;
	} else {
		simid = _find_free_imid(proxy, imid);
		sc->imid = simid;
		sc->imattr = g_object_ref(imattr);
		sc->default_icattr = g_object_ref(icattr);
	}
	_link_imid(proxy, simid, imid);

	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (conn));
	client = g_hash_table_lookup(proxy->client_table,
				     G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));

	c = G_XIM_CONNECTION (client->connection);
	c->imid = imid;
	c->imattr = g_object_ref(imattr);
	c->default_icattr = g_object_ref(icattr);

	attr = G_XIM_ATTR (imattr);
	list = g_xim_attr_get_supported_attributes(attr);
	for (l = list; l != NULL; l = g_slist_next(l)) {
		if (l->data && g_xim_attr_attribute_is_enabled(attr, l->data)) {
			guint id, sid;
			GType gtype;
			GXimValueType vtype;
			GString *s;

			id = g_xim_attr_get_attribute_id(attr, l->data);
			if (id == -1) {
				g_xim_message_warning(G_XIM_CORE (proxy)->message,
						      "Unknown IM attribute %s: no attribute id in the XIM server side is assigned.",
						      (gchar *)l->data);
				continue;
			}
			gtype = g_xim_attr_get_gtype_by_name(attr, l->data);
			vtype = g_xim_gtype_to_value_type(gtype);
			if (vtype == G_XIM_TYPE_INVALID) {
				g_xim_message_bug(G_XIM_CORE (proxy)->message,
						  "Unable to compose a XIMATTR for %s",
						  (gchar *)l->data);
				continue;
			}
			/* We are assuming that the attribute name is
			 * consistent in XIM protocol.
			 */
			sid = g_xim_attr_get_attribute_id(G_XIM_ATTR (sc->imattr), l->data);
			if (sid == -1) {
				g_xim_message_warning(G_XIM_CORE (proxy)->message,
						      "Unknown IM attribute %s: no attribute id in the client side is assigned.",
						      (gchar *)l->data);
				continue;
			}
			g_hash_table_insert(proxy->simattr_table,
					    g_strdup(l->data),
					    GUINT_TO_POINTER (sid + 1));
			g_hash_table_insert(proxy->cimattr_table,
					    g_strdup(l->data),
					    GUINT_TO_POINTER (id + 1));

			s = g_string_new(l->data);
			raw = g_xim_raw_attr_new_with_value(id, s, vtype);
			imlist = g_slist_append(imlist, raw);

			g_xim_message_debug(G_XIM_CORE (proxy)->message, "proxy/proto",
					    "IM Attribute link %s: %d <-> %d",
					    s->str, sid, id);
		}
		g_free(l->data);
	}
	g_slist_free(list);

	attr = G_XIM_ATTR (icattr);
	list = g_xim_attr_get_supported_attributes(attr);
	for (l = list; l != NULL; l = g_slist_next(l)) {
		if (l->data && g_xim_attr_attribute_is_enabled(attr, l->data)) {
			guint id, sid;
			GType gtype;
			GXimValueType vtype;
			GString *s;

			id = g_xim_attr_get_attribute_id(attr, l->data);
			if (id == -1) {
				g_xim_message_warning(G_XIM_CORE (proxy)->message,
						      "Unknown IC attribute %s: no attribute id in the XIM server side is assigned.",
						      (gchar *)l->data);
				continue;
			}
			gtype = g_xim_attr_get_gtype_by_name(attr, l->data);
			vtype = g_xim_gtype_to_value_type(gtype);
			if (vtype == G_XIM_TYPE_INVALID) {
				g_xim_message_bug(G_XIM_CORE (proxy)->message,
						  "Unable to compose a XICATTR for %s",
						  (gchar *)l->data);
				continue;
			}
			/* We are assuming that the attribute name is
			 * consistent in XIM protocol.
			 */
			sid = g_xim_attr_get_attribute_id(G_XIM_ATTR (sc->default_icattr), l->data);
			if (sid == -1) {
				g_xim_message_warning(G_XIM_CORE (proxy)->message,
						      "Unknown IC attribute %s: no attribute id in the client side is assigned.",
						      (gchar *)l->data);
				continue;
			}
			g_hash_table_insert(proxy->sicattr_table,
					    g_strdup(l->data),
					    GUINT_TO_POINTER (sid + 1));
			g_hash_table_insert(proxy->cicattr_table,
					    g_strdup(l->data),
					    GUINT_TO_POINTER (id + 1));

			s = g_string_new(l->data);
			raw = g_xim_raw_attr_new_with_value(id, s, vtype);
			iclist = g_slist_append(iclist, raw);

			g_xim_message_debug(G_XIM_CORE (proxy)->message, "proxy/proto",
					    "IC Attribute link %s: %d <-> %d",
					    s->str, sid, id);
		}
		g_free(l->data);
	}
	g_slist_free(list);

	if (XIM_CLIENT_CONNECTION (proto)->is_reconnecting) {
		_reconnect(proxy, G_XIM_CLIENT_CONNECTION (proto));
		retval = TRUE;
	} else {
		retval = g_xim_server_connection_cmd_open_reply(conn, simid, imlist, iclist);
	}
	DEC_PENDING (proxy);

	g_slist_foreach(imlist, (GFunc)g_xim_raw_attr_free, NULL);
	g_slist_foreach(iclist, (GFunc)g_xim_raw_attr_free, NULL);
	g_slist_free(imlist);
	g_slist_free(iclist);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_close_reply(GXimProtocol *proto,
					       guint16       imid,
					       gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval = FALSE;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		goto end;

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_server_connection_cmd_close_reply(conn, simid);
  end:
	DEC_PENDING (proxy);

	_remove_imid(proxy, imid);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_register_triggerkeys(GXimProtocol *proto,
							guint16       imid,
							const GSList *onkeys,
							const GSList *offkeys,
							gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		return FALSE;

	conn = _get_server_connection(proxy, proto);

	return g_xim_server_connection_cmd_register_triggerkeys(conn, simid, onkeys, offkeys);
}

static gboolean
xim_proxy_client_protocol_real_xim_trigger_notify_reply(GXimProtocol *proto,
							guint16       imid,
							guint16       icid,
							gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	guint16 simid = _get_server_imid(proxy, imid);
	gboolean retval = FALSE;

	if (simid == 0)
		goto end;

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_server_connection_cmd_trigger_notify_reply(conn, simid, icid);
  end:
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_set_event_mask(GXimProtocol *proto,
						  guint16       imid,
						  guint16       icid,
						  guint32       forward_event,
						  guint32       sync_event,
						  gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		return FALSE;

	conn = _get_server_connection(proxy, proto);

	return g_xim_server_connection_cmd_set_event_mask(conn, simid, icid, forward_event, sync_event);
}

static gboolean
xim_proxy_client_protocol_real_xim_encoding_negotiation_reply(GXimProtocol *proto,
							      guint16       imid,
							      guint16       category,
							      gint16        index_,
							      gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval = FALSE;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		goto end;

	conn = _get_server_connection(proxy, proto);
	if (XIM_CLIENT_CONNECTION (proto)->is_reconnecting) {
		const gchar *e1, *e2;
		const GValue *v;

		G_XIM_CONNECTION (proto)->encoding_category = category;
		G_XIM_CONNECTION (proto)->encoding_index = index_;

		if (category == 0) {
			v = g_value_array_get_nth(G_XIM_CONNECTION (conn)->encodings,
						  index_);
			e1 = g_xim_str_get_string(g_value_get_boxed(v));
		} else {
			v = g_value_array_get_nth(G_XIM_CONNECTION (conn)->encoding_details,
						  index_);
			e1 = g_xim_encodinginfo_get_string(g_value_get_boxed(v));
		}
		if (G_XIM_CONNECTION (conn)->encoding_category == 0) {
			v = g_value_array_get_nth(G_XIM_CONNECTION (conn)->encodings,
						  G_XIM_CONNECTION (conn)->encoding_index);
			e2 = g_xim_str_get_string(g_value_get_boxed(v));
		} else {
			v = g_value_array_get_nth(G_XIM_CONNECTION (conn)->encoding_details,
						  G_XIM_CONNECTION (conn)->encoding_index);
			e2 = g_xim_encodinginfo_get_string(g_value_get_boxed(v));
		}
		g_xim_message_debug(G_XIM_CORE (proxy)->message, "proxy/proto",
				    "Encoding link: %s <-> %s",
				    e2, e1);
		_reconnect(proxy, G_XIM_CLIENT_CONNECTION (proto));

		return TRUE;
	}

	retval = g_xim_server_connection_cmd_encoding_negotiation_reply(conn, simid, category, index_);
  end:
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_query_extension_reply(GXimProtocol *proto,
							 guint16       imid,
							 const GSList *extensions,
							 gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval = FALSE;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		goto end;

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_server_connection_cmd_query_extension_reply(conn, simid, extensions);
  end:
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_set_im_values_reply(GXimProtocol *proto,
						       guint16       imid,
						       gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval = FALSE;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		goto end;

	if (XIM_CLIENT_CONNECTION (proto)->is_reconnecting) {
		_reconnect(proxy, G_XIM_CLIENT_CONNECTION (proto));
		retval = TRUE;
	} else {
		conn = _get_server_connection(proxy, proto);

		retval = g_xim_server_connection_cmd_set_im_values_reply(conn, simid);
	  end:
		DEC_PENDING (proxy);
	}

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_get_im_values_reply(GXimProtocol *proto,
						       guint16       imid,
						       const GSList *attributes,
						       gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval = FALSE;
	guint16 simid = _get_server_imid(proxy, imid);
	GSList *alt = NULL, *l;
	GXimAttr *attr;

	if (simid == 0)
		goto end;

	conn = _get_server_connection(proxy, proto);
	attr = G_XIM_ATTR (G_XIM_CONNECTION (proto)->imattr);
	for (l = (GSList *)attributes; l != NULL; l = g_slist_next(l)) {
		GXimAttribute *a, *orig = l->data;
		guint16 id;
		gchar *name = g_xim_attr_get_attribute_name(attr, orig->id);

		/* assertion here is highly unlikely */
		g_return_val_if_fail(name != NULL, FALSE);
		if ((id = GPOINTER_TO_UINT (g_hash_table_lookup(proxy->simattr_table, name))) == 0) {
			g_xim_message_warning(G_XIM_CORE (proxy)->message,
					      "Unknown IM attribute %s: XIM server assigned it to id %d",
					      name, orig->id);
		} else {
			a = g_xim_attribute_copy(l->data);
			a->id = id - 1;
			alt = g_slist_append(alt, a);
			g_object_set(attr, name, a->v.pointer, NULL);
		}
		g_free(name);
	}

	retval = g_xim_server_connection_cmd_get_im_values_reply(conn, simid, alt);
  end:
	DEC_PENDING (proxy);

	g_slist_foreach(alt,
			(GFunc)g_xim_attribute_free,
			NULL);
	g_slist_free(alt);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_create_ic_reply(GXimProtocol *proto,
						   guint16       imid,
						   guint16       icid,
						   gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval = FALSE;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		goto end;

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_server_connection_cmd_create_ic_reply(conn, simid, icid);
  end:
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_destroy_ic_reply(GXimProtocol *proto,
						    guint16       imid,
						    guint16       icid,
						    gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval = FALSE;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		goto end;

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_server_connection_cmd_destroy_ic_reply(conn, simid, icid);
  end:
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_set_ic_values_reply(GXimProtocol *proto,
						       guint16       imid,
						       guint16       icid,
						       gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval = FALSE;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		goto end;

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_server_connection_cmd_set_ic_values_reply(conn, simid, icid);
  end:
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_get_ic_values_reply(GXimProtocol *proto,
						       guint16       imid,
						       guint16       icid,
						       const GSList *attributes,
						       gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval = FALSE;
	guint16 simid = _get_server_imid(proxy, imid);
	GSList *alt = NULL, *l;

	if (simid == 0)
		goto end;

	conn = _get_server_connection(proxy, proto);
	for (l = (GSList *)attributes; l != NULL; l = g_slist_next(l)) {
		GXimAttribute *a, *orig = l->data;
		guint16 id;
		gchar *name = g_xim_attr_get_attribute_name(G_XIM_ATTR (G_XIM_CONNECTION (proto)->default_icattr),
							    orig->id);

		/* assertion here is highly unlikely */
		g_return_val_if_fail(name != NULL, FALSE);
		if ((id = GPOINTER_TO_UINT (g_hash_table_lookup(proxy->sicattr_table, name))) == 0) {
			g_xim_message_warning(G_XIM_CORE (proxy)->message,
					      "Unknown IC attribute %s: XIM server assigned it to id %d",
					      name, orig->id);
		} else {
			a = g_xim_attribute_copy(l->data);
			a->id = id - 1;
			alt = g_slist_append(alt, a);
		}
		g_free(name);
	}

	retval = g_xim_server_connection_cmd_get_ic_values_reply(conn, simid, icid, alt);
  end:
	DEC_PENDING (proxy);

	g_slist_foreach(alt,
			(GFunc)g_xim_attribute_free,
			NULL);
	g_slist_free(alt);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_forward_event(GXimProtocol *proto,
						 guint16       imid,
						 guint16       icid,
						 guint16       flag,
						 GdkEvent     *event,
						 gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		return FALSE;

	conn = _get_server_connection(proxy, proto);
	if (flag & G_XIM_Event_Synchronous)
		INC_PENDING (proxy);

	return g_xim_connection_cmd_forward_event(G_XIM_CONNECTION (conn), simid, icid, flag, event);
}

static gboolean
xim_proxy_client_protocol_real_xim_sync(GXimProtocol *proto,
					guint16       imid,
					guint16       icid,
					gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		return FALSE;

	conn = _get_server_connection(proxy, proto);
	INC_PENDING (proxy);

	return g_xim_server_connection_cmd_sync(conn, simid, icid);
}

static gboolean
xim_proxy_client_protocol_real_xim_sync_reply(GXimProtocol *proto,
					      guint16       imid,
					      guint16       icid,
					      gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimConnection *conn;
	gboolean retval = FALSE;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		goto end;

	conn = G_XIM_CONNECTION (_get_server_connection(proxy, proto));

	retval = g_xim_connection_cmd_sync_reply(conn, simid, icid);
  end:
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_commit(GXimProtocol *proto,
					  guint16       imid,
					  guint16       icid,
					  guint16       flag,
					  guint32       keysym,
					  GString      *string,
					  gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	guint16 simid = _get_server_imid(proxy, imid);
	gboolean retval;

	if (simid == 0)
		return FALSE;

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_server_connection_cmd_commit(conn, simid, icid, flag, keysym, string);
	if (retval && (flag & G_XIM_XLookupSynchronous))
		INC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_reset_ic_reply(GXimProtocol  *proto,
						  guint16        imid,
						  guint16        icid,
						  const GString *preedit_string,
						  gpointer       data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval = FALSE;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		goto end;

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_server_connection_cmd_reset_ic_reply(conn, simid, icid, preedit_string);
  end:
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_preedit_start(GXimProtocol *proto,
						 guint16       imid,
						 guint16       icid,
						 gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		return FALSE;

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_server_connection_cmd_preedit_start(conn, simid, icid);
	INC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_preedit_draw(GXimProtocol    *proto,
						guint16          imid,
						guint16          icid,
						GXimPreeditDraw *draw,
						gpointer         data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		return FALSE;

	conn = _get_server_connection(proxy, proto);

	return g_xim_server_connection_cmd_preedit_draw(conn, simid, icid, draw);
}

static gboolean
xim_proxy_client_protocol_real_xim_preedit_caret(GXimProtocol     *proto,
						 guint16           imid,
						 guint16           icid,
						 GXimPreeditCaret *caret,
						 gpointer          data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	gboolean retval;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		return FALSE;

	conn = _get_server_connection(proxy, proto);

	retval = g_xim_server_connection_cmd_preedit_caret(conn, simid, icid, caret);
	INC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_client_protocol_real_xim_preedit_done(GXimProtocol *proto,
						guint16       imid,
						guint16       icid,
						gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		return FALSE;

	conn = _get_server_connection(proxy, proto);

	return g_xim_server_connection_cmd_preedit_done(conn, simid, icid);
}

static gboolean
xim_proxy_client_protocol_real_xim_status_start(GXimProtocol *proto,
						guint16       imid,
						guint16       icid,
						gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		return FALSE;

	conn = _get_server_connection(proxy, proto);

	return g_xim_server_connection_cmd_status_start(conn, simid, icid);
}

static gboolean
xim_proxy_client_protocol_real_xim_status_draw(GXimProtocol   *proto,
					       guint16         imid,
					       guint16         icid,
					       GXimStatusDraw *draw,
					       gpointer        data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		return FALSE;

	conn = _get_server_connection(proxy, proto);

	return g_xim_server_connection_cmd_status_draw(conn, simid, icid, draw);
}

static gboolean
xim_proxy_client_protocol_real_xim_status_done(GXimProtocol *proto,
					       guint16       imid,
					       guint16       icid,
					       gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimServerConnection *conn;
	guint16 simid = _get_server_imid(proxy, imid);

	if (simid == 0)
		return FALSE;

	conn = _get_server_connection(proxy, proto);

	return g_xim_server_connection_cmd_status_done(conn, simid, icid);
}

static GObject *
xim_proxy_real_constructor(GType                  type,
			   guint                  n_construct_properties,
			   GObjectConstructParam *construct_properties)
{
	GObject *object, *retval = NULL;
	GXimCore *core;
	XimProxy *proxy;
	GdkDisplay *dpy;

	object = G_OBJECT_CLASS (xim_proxy_parent_class)->constructor(type,
								      n_construct_properties,
								      construct_properties);
	if (object) {
		GError *error = NULL;

		core = G_XIM_CORE (object);
		proxy = XIM_PROXY (object);
		dpy = g_xim_core_get_display(core);
		proxy->default_server = G_XIM_SRV_TMPL (xim_loopback_new(dpy));
		if (proxy->default_server == NULL) {
			g_xim_message_critical(core->message,
					       "Unable to create a default server instance.");
			goto end;
		}
		if (!g_xim_srv_tmpl_take_ownership(proxy->default_server,
						   TRUE,
						   &error)) {
			g_xim_message_gerror(core->message, error);
			g_error_free(error);
			goto end;
		}
		retval = object;

	  end:
		if (retval == NULL)
			g_object_unref(object);
	}

	return retval;
}

static void
xim_proxy_real_set_property(GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	XimProxy *proxy = XIM_PROXY (object);
	GXimLazySignalConnector *sigs;
	gsize len, i;

	switch (prop_id) {
	    case PROP_CONNECT_TO:
		    proxy->connect_to = g_strdup(g_value_get_string(value));
		    xim_proxy_disconnect_all(proxy);
		    break;
	    case PROP_CLIENT_PROTO_SIGNALS:
		    if (proxy->client_proto_signals != NULL) {
			    g_xim_message_warning(G_XIM_CORE (object)->message,
						  "Unable to update the signal list.");
			    break;
		    }
		    sigs = g_value_get_pointer(value);
		    for (len = 0; sigs[len].signal_name != NULL; len++);
		    proxy->client_proto_signals = g_new0(GXimLazySignalConnector, len + 1);
		    for (i = 0; sigs[i].signal_name != NULL; i++) {
			    proxy->client_proto_signals[i].signal_name = g_strdup(sigs[i].signal_name);
			    proxy->client_proto_signals[i].function = sigs[i].function;
			    proxy->client_proto_signals[i].data = sigs[i].data;
		    }
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
xim_proxy_real_get_property(GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	XimProxy *proxy = XIM_PROXY (object);

	switch (prop_id) {
	    case PROP_CONNECT_TO:
		    g_value_set_string(value, proxy->connect_to);
		    break;
	    case PROP_CLIENT_PROTO_SIGNALS:
		    g_value_set_pointer(value, proxy->client_proto_signals);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
xim_proxy_real_dispose(GObject *object)
{
	XimProxy *proxy = XIM_PROXY (object);
	GHashTableIter iter;
	gpointer key, val;

	while (xim_proxy_is_pending(proxy))
		g_main_context_iteration(NULL, TRUE);

	g_hash_table_iter_init(&iter, proxy->sconn_table);
	while (g_hash_table_iter_next(&iter, &key, &val)) {
		g_xim_srv_tmpl_remove_connection(G_XIM_SRV_TMPL (proxy),
						 G_XIM_POINTER_TO_NATIVE_WINDOW (key));
		/* hash might be changed */
		g_hash_table_iter_init(&iter, proxy->sconn_table);
	}

	if (G_OBJECT_CLASS (xim_proxy_parent_class)->dispose)
		(* G_OBJECT_CLASS (xim_proxy_parent_class)->dispose) (object);
}

static void
xim_proxy_real_finalize(GObject *object)
{
	XimProxy *proxy = XIM_PROXY (object);
	gint i;

	g_hash_table_destroy(proxy->sconn_table);
	g_hash_table_destroy(proxy->client_table);
	g_hash_table_destroy(proxy->selection_table);
	g_hash_table_destroy(proxy->comm_table);
	g_hash_table_destroy(proxy->simattr_table);
	g_hash_table_destroy(proxy->sicattr_table);
	g_hash_table_destroy(proxy->cimattr_table);
	g_hash_table_destroy(proxy->cicattr_table);
	g_free(proxy->connect_to);
	for (i = 0; proxy->client_proto_signals[i].signal_name != NULL; i++) {
		g_free(proxy->client_proto_signals[i].signal_name);
	}
	g_free(proxy->client_proto_signals);
	g_free(proxy->simid_table);
	g_free(proxy->cimid_table);
	g_object_unref(proxy->default_server);

	if (G_OBJECT_CLASS (xim_proxy_parent_class)->finalize)
		(* G_OBJECT_CLASS (xim_proxy_parent_class)->finalize) (object);
}

static void
xim_proxy_real_setup_connection(GXimCore       *core,
				GXimConnection *conn)
{
}

static gboolean
xim_proxy_real_selection_request_event(GXimCore          *core,
				       GdkEventSelection *event)
{
	XimProxy *proxy = XIM_PROXY (core);
	XimClient *client;
	gboolean retval = TRUE;
	GdkNativeWindow nw;
	gchar *s;
	GError *error = NULL;

	client = _create_client(proxy, event->requestor);
	if (client == NULL)
		return FALSE;
	nw = GDK_WINDOW_XID (g_xim_core_get_selection_window(G_XIM_CORE (client)));
	s = gdk_atom_name(event->property);
	g_xim_message_debug(core->message, "proxy/event",
			    "%p ->%p-> SelectionRequest[%s]",
			    G_XIM_NATIVE_WINDOW_TO_POINTER (event->requestor),
			    G_XIM_NATIVE_WINDOW_TO_POINTER (nw),
			    s);
	g_free(s);
	if (event->property == core->atom_locales) {
		if (!g_xim_cl_tmpl_send_selection_request(G_XIM_CL_TMPL (client),
							  core->atom_locales,
							  &error)) {
			g_xim_message_error(core->message,
					    "Unable to get the supported locales from the server `%s': %s",
					    G_XIM_SRV_TMPL (proxy)->server_name,
					    error->message);
			g_error_free(error);
			/* XXX: fallback to the default behavior wouldn't be ideal.
			 * however no response from the server makes
			 * the applications getting on stuck easily.
			 * That may be a good idea to send back anything.
			 */
			retval = FALSE;
		}
	} else if (event->property == core->atom_transport) {
		if (!g_xim_cl_tmpl_send_selection_request(G_XIM_CL_TMPL (client),
							  core->atom_transport,
							  &error)) {
			g_xim_message_error(core->message,
					    "Unable to get the supported transport from the server `%s': %s",
					    G_XIM_SRV_TMPL (proxy)->server_name,
					    error->message);
			g_error_free(error);
			/* XXX: fallback to the default behavior wouldn't be ideal.
			 * however no response from the server makes
			 * the applications getting on stuck easily.
			 * That may be a good idea to send back anything.
			 */
			retval = FALSE;
		}
	} else {
		gchar *s = gdk_atom_name(event->property);

		g_xim_message_warning(core->message,
				      "%s: Unknown/unsupported Property received: %s",
				      __FUNCTION__, s);
		g_free(s);

		return FALSE;
	}
	if (retval)
		INC_PENDING (proxy);

	return retval;
}

static gchar *
xim_proxy_real_get_supported_locales(GXimServerTemplate *server)
{
	return g_strdup("");
}

static gchar *
xim_proxy_real_get_supported_transport(GXimServerTemplate *server)
{
	return g_strdup("");
}

static void
_weak_notify_conn_cb(gpointer  data,
		     GObject  *object)
{
	GXimServerTemplate *server = data;
	GXimCore *core = G_XIM_CORE (server);
	GdkDisplay *dpy = g_xim_core_get_display(core);
	GdkNativeWindow nw, comm_window;
	GdkWindow *w;
	GXimClientTemplate *client;

	nw = g_xim_transport_get_native_channel(G_XIM_TRANSPORT (object));
	if (nw) {
		g_xim_message_debug(core->message, "server/conn",
				    "Removing a connection from the table for %p",
				    G_XIM_NATIVE_WINDOW_TO_POINTER (nw));
		w = g_xim_get_window(dpy, nw);
		g_xim_core_unwatch_event(core, w);
		g_hash_table_remove(server->conn_table,
				    G_XIM_NATIVE_WINDOW_TO_POINTER (nw));
	}
	nw = g_xim_transport_get_client_window(G_XIM_TRANSPORT (object));
	if (nw) {
		g_xim_message_debug(core->message, "server/conn",
				    "Removing a connection from the table for %p",
				    G_XIM_NATIVE_WINDOW_TO_POINTER (nw));
		w = g_xim_get_window(dpy, nw);
		g_xim_core_unwatch_event(core, w);
		g_hash_table_remove(server->conn_table,
				    G_XIM_NATIVE_WINDOW_TO_POINTER (nw));
		g_hash_table_remove(XIM_PROXY (server)->sconn_table,
				    G_XIM_NATIVE_WINDOW_TO_POINTER (nw));
		g_xim_message_debug(core->message, "client/conn",
				    "Removing a client connection from the table for %p",
				    G_XIM_NATIVE_WINDOW_TO_POINTER (nw));
		client = g_hash_table_lookup(XIM_PROXY (server)->client_table,
					     G_XIM_NATIVE_WINDOW_TO_POINTER (nw));
		comm_window = g_xim_transport_get_native_channel(G_XIM_TRANSPORT (client->connection));
		g_hash_table_remove(XIM_PROXY (server)->client_table,
				    G_XIM_NATIVE_WINDOW_TO_POINTER (nw));
		g_hash_table_remove(XIM_PROXY (server)->comm_table,
				    G_XIM_NATIVE_WINDOW_TO_POINTER (comm_window));
	}
	g_xim_message_debug(core->message, "proxy/conn",
			    "live server connection: %d [%d][%d]\n",
			    g_hash_table_size(server->conn_table),
			    g_hash_table_size(XIM_PROXY (server)->sconn_table),
			    g_hash_table_size(XIM_PROXY (server)->comm_table));
	g_xim_message_debug(core->message, "proxy/conn",
			    "live client connection: %d\n",
			    g_hash_table_size(XIM_PROXY (server)->client_table));
}

static GXimConnection *
xim_proxy_real_xconnect(GXimServerTemplate *server,
			GdkEventClient     *event)
{
	GdkNativeWindow w = event->data.l[0], comm_window;
	GXimConnection *conn = g_xim_srv_tmpl_lookup_connection_with_native_window(server, w);
	GXimCore *core = G_XIM_CORE (server);
	GXimTransport *trans;
	GdkDisplay *dpy = g_xim_core_get_display(core);
	GType gtype;
	XimProxy *proxy = XIM_PROXY (server);
	XimClient *client;
	GError *error = NULL;

	if (conn) {
		g_xim_message_warning(core->message,
				      "Received XIM_XCONNECT event from %p again: duplicate connection",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (w));
		g_object_ref(conn);
		goto client_proc;
	}
	gtype = g_xim_core_get_connection_gtype(core);
	if (!g_type_is_a(gtype, G_TYPE_XIM_CONNECTION)) {
		g_xim_message_bug(core->message,
				  "given GObject type isn't inherited from GXimConnection");
		return NULL;
	}
	conn = G_XIM_CONNECTION (g_object_new(gtype,
					      "proto_signals", g_xim_core_get_protocol_signal_connector(core),
					      NULL));
	g_xim_core_setup_connection(core, conn);
	trans = G_XIM_TRANSPORT (conn);
	g_xim_transport_set_display(trans, dpy);
	g_xim_transport_set_client_window(trans, w);
	/* supposed to do create a comm_window.
	 * but what we really want here is GdkNativeWindow.
	 * so the return value of g_xim_transport_get_channel
	 * won't be used then.
	 */
	g_xim_transport_get_channel(trans, event->window);
	g_xim_core_add_client_message_filter(core,
					     g_xim_transport_get_atom(G_XIM_TRANSPORT (conn)));
	g_xim_srv_tmpl_add_connection(server, conn, w);
	g_object_weak_ref(G_OBJECT (conn),
			  _weak_notify_conn_cb,
			  server);

  client_proc:
	if ((client = g_hash_table_lookup(proxy->client_table,
					  G_XIM_NATIVE_WINDOW_TO_POINTER (w)))) {
		g_xim_message_warning(core->message,
				      "Received XIM_XCONNECT event from %p again: duplicate client",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (w));
		/* XXX: ignore it so far */
	} else {
		client = _create_client(XIM_PROXY (server), w);
		if (client == NULL) {
			return NULL;
		}
		g_object_set(G_OBJECT (client), "proto_signals", proxy->client_proto_signals, NULL);
		g_signal_connect(client, "xconnect",
				 G_CALLBACK (xim_proxy_client_real_xconnect_cb),
				 server);
	}

	if (!g_xim_cl_tmpl_connect_to_server(G_XIM_CL_TMPL (client), &error)) {
		g_xim_message_gerror(core->message, error);
		g_error_free(error);

		return NULL;
	}
	comm_window = g_xim_transport_get_native_channel(G_XIM_TRANSPORT (G_XIM_CL_TMPL (client)->connection));
	g_hash_table_insert(proxy->comm_table,
			    G_XIM_NATIVE_WINDOW_TO_POINTER (comm_window),
			    G_XIM_NATIVE_WINDOW_TO_POINTER (w));
	g_hash_table_insert(proxy->sconn_table,
			    G_XIM_NATIVE_WINDOW_TO_POINTER (w),
			    conn);
	g_xim_message_debug(core->message, "proxy/event",
			    "%p ->%p-> XIM_XCONNECT",
			    G_XIM_NATIVE_WINDOW_TO_POINTER (w),
			    G_XIM_NATIVE_WINDOW_TO_POINTER (comm_window));
	G_XIM_CL_TMPL (client)->is_connection_initialized = GXC_NEGOTIATING;
	INC_PENDING (proxy);

	return conn;
}

static gboolean
xim_proxy_protocol_real_xim_connect(GXimProtocol *proto,
				    guint16       major_version,
				    guint16       minor_version,
				    const GSList *list,
				    gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));

	if (!g_xim_client_connection_cmd_connect(conn,
						 major_version,
						 minor_version,
						 (GSList *)list,
						 TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_CONNECT for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_disconnect(GXimProtocol *proto,
				       gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));

	/* We don't want to unref a client connection by the request from
	 * the XIM server. it should be done by triggering destroying
	 * the server connection.
	 */
	g_object_ref(conn);

	if (!g_xim_client_connection_cmd_disconnect(conn, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_DISCONNECT for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_auth_ng(GXimProtocol *proto,
				    gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));

	if (!g_xim_connection_cmd_auth_ng(G_XIM_CONNECTION (conn))) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_AUTH_NG for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_error(GXimProtocol  *proto,
				  guint16        imid,
				  guint16        icid,
				  GXimErrorMask  flag,
				  GXimErrorCode  error_code,
				  guint16        detail,
				  const gchar   *error_message,
				  gpointer       data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);

	if (cimid == 0)
		return FALSE;

	if (!g_xim_connection_cmd_error(G_XIM_CONNECTION (conn), cimid, icid, flag, error_code, detail, error_message)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_ERROR for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_open(GXimProtocol  *proto,
				 const GXimStr *locale,
				 gpointer       data)
{
	XimProxy *proxy = XIM_PROXY (data);
	XimProxyConnection *pconn = XIM_PROXY_CONNECTION (proto);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));

	g_xim_str_free(pconn->locale);
	pconn->locale = g_xim_str_copy((GXimStr *)locale);
	if (!g_xim_client_connection_cmd_open_im(conn,
						 locale,
						 TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_OPEN for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_close(GXimProtocol *proto,
				  guint16       imid,
				  gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);

	if (cimid == 0)
		return FALSE;

	if (!g_xim_client_connection_cmd_close_im(conn, cimid, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_CLOSE for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_trigger_notify(GXimProtocol *proto,
					   guint16       imid,
					   guint16       icid,
					   guint32       flag,
					   guint32       index_,
					   guint32       event_mask,
					   gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn;
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);

	/* the case when cimid is 0 means new XIM server is being brought up.
	 * So IC is already invalid for new one. we don't need to send this out.
	 */
	if (cimid == 0) {
		return g_xim_connection_cmd_error(G_XIM_CONNECTION (proto), imid, icid, G_XIM_EMASK_VALID_IMID | G_XIM_EMASK_VALID_ICID,
						  G_XIM_ERR_BadProtocol, 0, "input-context id is inavlid due to reconnecting");
	}

	conn = _get_client_connection(proxy, proto);

	if (!g_xim_client_connection_cmd_trigger_notify(conn, cimid, icid, flag, index_, event_mask, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_TRIGGER_NOTIFY for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_encoding_negotiation(GXimProtocol *proto,
						 guint16       imid,
						 GSList       *encodings,
						 GSList       *details,
						 gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);
	GXimConnection *c = G_XIM_CONNECTION (proto);
	const GSList *l;

	if (cimid == 0)
		return FALSE;

	if (c->encodings)
		g_value_array_free(c->encodings);
	if (c->encoding_details)
		g_value_array_free(c->encoding_details);
	c->encodings = g_value_array_new(g_slist_length(encodings));
	for (l = encodings; l != NULL; l = g_slist_next(l)) {
		GValue v = { 0, };

		g_value_init(&v, G_TYPE_XIM_STR);
		g_value_set_boxed(&v, l->data);
		g_value_array_append(c->encodings, &v);
	}
	c->encoding_details = g_value_array_new(g_slist_length(details));
	for (l = details; l != NULL; l = g_slist_next(l)) {
		GValue v = { 0, };

		g_value_init(&v, G_TYPE_XIM_ENCODINGINFO);
		g_value_set_boxed(&v, l->data);
		g_value_array_append(c->encoding_details, &v);
	}
	if (!g_xim_client_connection_cmd_encoding_negotiation(conn, cimid, encodings, details, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_ENCODING_NEGOTIATION for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_query_extension(GXimProtocol *proto,
					    guint16       imid,
					    const GSList *extensions,
					    gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);

	if (cimid == 0)
		return FALSE;

	if (!g_xim_client_connection_cmd_query_extension(conn, cimid, extensions, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_QUERY_EXTENSION for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_set_im_values(GXimProtocol *proto,
					  guint16       imid,
					  const GSList *attributes,
					  gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);
	GSList *alt = NULL, *l;

	if (cimid == 0)
		return FALSE;

	for (l = (GSList *)attributes; l != NULL; l = g_slist_next(l)) {
		GXimAttribute *a, *orig = l->data;
		guint16 id;
		gchar *name = g_xim_attr_get_attribute_name(G_XIM_ATTR (G_XIM_CONNECTION (proto)->imattr),
							    orig->id);

		/* assertion here is highly unlikely */
		g_return_val_if_fail (name != NULL, FALSE);
		if ((id = GPOINTER_TO_UINT (g_hash_table_lookup(proxy->cimattr_table, name))) == 0) {
			g_xim_message_warning(G_XIM_CORE (proxy)->message,
					      "Unknown IM attribute id %d [%s]",
					      orig->id, name);
		} else {
			g_xim_message_debug(G_XIM_CORE (proxy)->message, "proxy/proto",
					    "IM Attribute %d->%d: %s",
					    orig->id, id - 1, name);
			a = g_xim_attribute_copy(l->data);
			a->id = id - 1;
			alt = g_slist_append(alt, a);
		}
		g_free(name);
	}
	if (!g_xim_client_connection_cmd_set_im_values(conn, cimid, alt, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_SET_IM_VALUES for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	g_slist_foreach(alt,
			(GFunc)g_xim_attribute_free,
			NULL);
	g_slist_free(alt);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_get_im_values(GXimProtocol *proto,
					  guint16       imid,
					  const GSList *attr_id,
					  gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);
	GSList *alt = NULL, *l;

	if (cimid == 0)
		return FALSE;

	for (l = (GSList *)attr_id; l != NULL; l = g_slist_next(l)) {
		guint16 id = GPOINTER_TO_UINT (l->data);
		gchar *name = g_xim_attr_get_attribute_name(G_XIM_ATTR (G_XIM_CONNECTION (proto)->imattr),
							    id);
		guint16 sid;

		/* no attribute name is highly unlikely */
		g_return_val_if_fail (name != NULL, FALSE);
		sid = GPOINTER_TO_UINT (g_hash_table_lookup(proxy->cimattr_table, name));
		if (sid == 0) {
			g_xim_message_warning(G_XIM_CORE (proxy)->message,
					      "Unknown IM attribute id %d [%s]",
					      id, name);
		} else {
			alt = g_slist_append(alt, GUINT_TO_POINTER (sid - 1));
		}
		g_free(name);
	}
	if (!g_xim_client_connection_cmd_get_im_values(conn, cimid, alt, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_GET_IM_VALUES for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	g_slist_free(alt);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_create_ic(GXimProtocol *proto,
				      guint16       imid,
				      const GSList *attributes,
				      gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);
	GSList *alt = NULL, *l;

	if (cimid == 0)
		return FALSE;

	for (l = (GSList *)attributes; l != NULL; l = g_slist_next(l)) {
		GXimAttribute *a, *orig = l->data;
		guint16 id;
		gchar *name = g_xim_attr_get_attribute_name(G_XIM_ATTR (G_XIM_CONNECTION (proto)->default_icattr),
							    orig->id);

		/* assertion here is highly unlikely */
		g_return_val_if_fail (name != NULL, FALSE);
		if ((id = GPOINTER_TO_UINT (g_hash_table_lookup(proxy->cicattr_table, name))) == 0) {
			g_xim_message_warning(G_XIM_CORE (proxy)->message,
					      "Unknown IC attribute id %d [%s]",
					      orig->id, name);
		} else {
			g_xim_message_debug(G_XIM_CORE (proxy)->message, "proxy/proto",
					    "IC Attribute %d->%d: %s",
					    orig->id, id - 1, name);
			a = g_xim_attribute_copy(l->data);
			a->id = id - 1;
			alt = g_slist_append(alt, a);
		}
		g_free(name);
	}
	if (!g_xim_client_connection_cmd_create_ic(conn, cimid, alt, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_CREATE_IC for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	g_slist_foreach(alt,
			(GFunc)g_xim_attribute_free,
			NULL);
	g_slist_free(alt);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_destroy_ic(GXimProtocol *proto,
				       guint16       imid,
				       guint16       icid,
				       gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn;
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);

	/* the case when cimid is 0 means new XIM server is being brought up.
	 * So IC is already invalid for new one. we don't need to send this out.
	 */
	if (cimid == 0) {
		/* to avoid the unnecessary error, send back XIM_DESTROY_IC_REPLY here. */
		return g_xim_server_connection_cmd_destroy_ic_reply(G_XIM_SERVER_CONNECTION (proto),
								    imid, icid);
	}

	conn = _get_client_connection(proxy, proto);
	if (!g_xim_client_connection_cmd_destroy_ic(conn, cimid, icid, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_DESTROY_IC for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_set_ic_values(GXimProtocol *proto,
					  guint16       imid,
					  guint16       icid,
					  const GSList *attributes,
					  gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn;
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);
	GSList *alt = NULL, *l;

	/* the case when cimid is 0 means new XIM server is being brought up.
	 * So IC is already invalid for new one. we don't need to send this out.
	 */
	if (cimid == 0) {
		return g_xim_connection_cmd_error(G_XIM_CONNECTION (proto), imid, icid, G_XIM_EMASK_VALID_IMID | G_XIM_EMASK_VALID_ICID,
						  G_XIM_ERR_BadProtocol, 0, "input-context id is inavlid due to reconnecting");
	}

	conn = _get_client_connection(proxy, proto);

	for (l = (GSList *)attributes; l != NULL; l = g_slist_next(l)) {
		GXimAttribute *a, *orig = l->data;
		guint16 id;
		gchar *name = g_xim_attr_get_attribute_name(G_XIM_ATTR (G_XIM_CONNECTION (proto)->default_icattr),
							    orig->id);

		/* assertion here is highly unlikely */
		g_return_val_if_fail(name != NULL, FALSE);
		if ((id = GPOINTER_TO_UINT (g_hash_table_lookup(proxy->cicattr_table, name))) == 0) {
			g_xim_message_warning(G_XIM_CORE (proxy)->message,
					      "Unknown IC attribute id %d [%s]",
					      orig->id, name);
		} else {
			a = g_xim_attribute_copy(l->data);
			a->id = id - 1;
			alt = g_slist_append(alt, a);
		}
		g_free(name);
	}
	if (!g_xim_client_connection_cmd_set_ic_values(conn, cimid, icid, alt, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_SET_IC_VALUES for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	g_slist_foreach(alt,
			(GFunc)g_xim_attribute_free,
			NULL);
	g_slist_free(alt);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_get_ic_values(GXimProtocol *proto,
					  guint16       imid,
					  guint16       icid,
					  const GSList *attr_id,
					  gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn;
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);
	GSList *alt = NULL, *l;

	/* the case when cimid is 0 means new XIM server is being brought up.
	 * So IC is already invalid for new one. we don't need to send this out.
	 */
	if (cimid == 0) {
		return g_xim_connection_cmd_error(G_XIM_CONNECTION (proto), imid, icid, G_XIM_EMASK_VALID_IMID | G_XIM_EMASK_VALID_ICID,
						  G_XIM_ERR_BadProtocol, 0, "input-context id is inavlid due to reconnecting");
	}

	conn = _get_client_connection(proxy, proto);

	for (l = (GSList *)attr_id; l != NULL; l = g_slist_next(l)) {
		guint16 id = GPOINTER_TO_UINT (l->data);
		gchar *name = g_xim_attr_get_attribute_name(G_XIM_ATTR (G_XIM_CONNECTION (proto)->default_icattr),
							    id);
		guint16 sid;

		/* no attribute name is highly unlikely */
		g_return_val_if_fail (name != NULL, FALSE);
		sid = GPOINTER_TO_UINT (g_hash_table_lookup(proxy->cicattr_table, name));
		if (sid == 0) {
			g_xim_message_warning(G_XIM_CORE (proxy)->message,
					      "Unknown IC attribute id %d [%s]",
					      id, name);
		} else {
			alt = g_slist_append(alt, GUINT_TO_POINTER (sid - 1));
		}
		g_xim_message_debug(G_XIM_CORE (proxy)->message, "proxy/proto",
				    "IC Attribute %d->%d: %s",
				    id, sid - 1, name);
		g_free(name);
	}
	if (!g_xim_client_connection_cmd_get_ic_values(conn, cimid, icid, alt, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_GET_IC_VALUES for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	g_slist_free(alt);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_set_ic_focus(GXimProtocol *proto,
					 guint16       imid,
					 guint16       icid,
					 gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn;
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);

	/* the case when cimid is 0 means new XIM server is being brought up.
	 * So IC is already invalid for new one. we don't need to send this out.
	 */
	if (cimid == 0)
		return TRUE;

	conn = _get_client_connection(proxy, proto);
	if (!g_xim_client_connection_cmd_set_ic_focus(conn, cimid, icid)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_SET_IC_FOCUS for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_unset_ic_focus(GXimProtocol *proto,
					   guint16       imid,
					   guint16       icid,
					   gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn;
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);

	/* the case when cimid is 0 means new XIM server is being brought up.
	 * So IC is already invalid for new one. we don't need to send this out.
	 */
	if (cimid == 0)
		return TRUE;

	conn = _get_client_connection(proxy, proto);
	if (!g_xim_client_connection_cmd_unset_ic_focus(conn, cimid, icid)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_UNSET_IC_FOCUS for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_forward_event(GXimProtocol *proto,
					  guint16       imid,
					  guint16       icid,
					  guint16       flag,
					  GdkEvent     *event,
					  gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);

	/* XXX: should we take care of reconnection issue here? */
	if (cimid == 0)
		return FALSE;

	if (!g_xim_connection_cmd_forward_event(G_XIM_CONNECTION (conn), cimid, icid, flag, event)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_FORWARD_EVENT for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		if (flag & G_XIM_Event_Synchronous)
			g_xim_connection_cmd_sync_reply(G_XIM_CONNECTION (proto), imid, icid);

		g_xim_connection_cmd_forward_event(G_XIM_CONNECTION (proto),
						   imid, icid, flag & ~G_XIM_Event_Synchronous, event);

		return TRUE;
	}
	if (flag & G_XIM_Event_Synchronous)
		INC_PENDING (proxy);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_sync(GXimProtocol *proto,
				 guint16       imid,
				 guint16       icid,
				 gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn;
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);

	/* the case when cimid is 0 means new XIM server is being brought up.
	 * So IC is already invalid for new one. we don't need to send this out.
	 */
	if (cimid == 0) {
		return g_xim_connection_cmd_sync_reply(G_XIM_CONNECTION (proto), imid, icid);
	}

	conn = _get_client_connection(proxy, proto);

	if (!g_xim_client_connection_cmd_sync(conn, cimid, icid, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_SYNC for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_sync_reply(GXimProtocol *proto,
				       guint16       imid,
				       guint16       icid,
				       gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimConnection *conn = G_XIM_CONNECTION (_get_client_connection(proxy, proto));
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);
	gboolean retval = FALSE;

	if (cimid == 0)
		goto end;

	if (!(retval = g_xim_connection_cmd_sync_reply(conn, cimid, icid))) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_SYNC_REPLY for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
	}
  end:
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_protocol_real_xim_reset_ic(GXimProtocol *proto,
				     guint16       imid,
				     guint16       icid,
				     gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn;
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);

	/* the case when cimid is 0 means new XIM server is being brought up.
	 * So IC is already invalid for new one. we don't need to send this out.
	 */
	if (cimid == 0) {
		return g_xim_connection_cmd_error(G_XIM_CONNECTION (proto), imid, icid, G_XIM_EMASK_VALID_IMID | G_XIM_EMASK_VALID_ICID,
						  G_XIM_ERR_BadProtocol, 0, "input-context id is inavlid due to reconnecting");
	}

	conn = _get_client_connection(proxy, proto);

	if (!g_xim_client_connection_cmd_reset_ic(conn, cimid, icid, TRUE)) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_RESET_IC for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
		return FALSE;
	}
	INC_PENDING (proxy);

	return TRUE;
}

static gboolean
xim_proxy_protocol_real_xim_preedit_start_reply(GXimProtocol *proto,
						guint16       imid,
						guint16       icid,
						gint32        return_value,
						gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);
	gboolean retval = FALSE;

	if (cimid == 0)
		goto end;

	if (!(retval = g_xim_client_connection_cmd_preedit_start_reply(conn, cimid, icid, return_value))) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_PREEDIT_START_REPLY for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
	}
  end:
	DEC_PENDING (proxy);

	return retval;
}

static gboolean
xim_proxy_protocol_real_xim_preedit_caret_reply(GXimProtocol *proto,
						guint16       imid,
						guint16       icid,
						guint32       position,
						gpointer      data)
{
	XimProxy *proxy = XIM_PROXY (data);
	GXimClientConnection *conn = _get_client_connection(proxy, proto);
	GdkNativeWindow	client_window = g_xim_transport_get_client_window(G_XIM_TRANSPORT (proto));
	guint16 cimid = _get_client_imid(proxy, imid);
	gboolean retval = FALSE;

	if (cimid == 0)
		goto end;

	if (!(retval = g_xim_client_connection_cmd_preedit_caret_reply(conn, cimid, icid, position))) {
		g_xim_message_warning(G_XIM_PROTOCOL_GET_IFACE (proto)->message,
				      "Unable to deliver XIM_PREEDIT_CARET_REPLY for %p",
				      G_XIM_NATIVE_WINDOW_TO_POINTER (client_window));
	}
  end:
	DEC_PENDING (proxy);

	return retval;
}

static void
xim_proxy_class_init(XimProxyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GXimCoreClass *core_class = G_XIM_CORE_CLASS (klass);
	GXimServerTemplateClass *srvtmpl_class = G_XIM_SRV_TMPL_CLASS (klass);

	object_class->constructor  = xim_proxy_real_constructor;
	object_class->set_property = xim_proxy_real_set_property;
	object_class->get_property = xim_proxy_real_get_property;
	object_class->dispose      = xim_proxy_real_dispose;
	object_class->finalize     = xim_proxy_real_finalize;

	core_class->setup_connection        = xim_proxy_real_setup_connection;
	core_class->selection_request_event = xim_proxy_real_selection_request_event;

	srvtmpl_class->get_supported_locales   = xim_proxy_real_get_supported_locales;
	srvtmpl_class->get_supported_transport = xim_proxy_real_get_supported_transport;
	srvtmpl_class->xconnect                = xim_proxy_real_xconnect;

	/* properties */
	g_object_class_install_property(object_class, PROP_CONNECT_TO,
					g_param_spec_string("connect_to",
							    _("A XIM server name"),
							    _("A XIM server name would connects to"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_CLIENT_PROTO_SIGNALS,
					g_param_spec_pointer("client_proto_signals",
							     _("Signals for Protocol class"),
							     _("A structure of signals for Protocol class"),
							     G_PARAM_READWRITE));

	/* signals */
}

static void
xim_proxy_init(XimProxy *proxy)
{
	GXimLazySignalConnector sigs[] = {
		{"XIM_CONNECT", G_CALLBACK (xim_proxy_protocol_real_xim_connect), proxy},
		{"XIM_DISCONNECT", G_CALLBACK (xim_proxy_protocol_real_xim_disconnect), proxy},
//FIXME		{"XIM_AUTH_REQUIRED", G_CALLBACK (xim_proxy_protocol_real_xim_auth_required), proxy},
//FIXME		{"XIM_AUTH_REPLY", G_CALLBACK (xim_proxy_protocol_real_xim_auth_reply), proxy},
//FIXME		{"XIM_AUTH_NEXT", G_CALLBACK (xim_proxy_protocol_real_xim_auth_next), proxy},
		{"XIM_AUTH_NG", G_CALLBACK (xim_proxy_protocol_real_xim_auth_ng), proxy},
		{"XIM_ERROR", G_CALLBACK (xim_proxy_protocol_real_xim_error), proxy},
		{"XIM_OPEN", G_CALLBACK (xim_proxy_protocol_real_xim_open), proxy},
		{"XIM_CLOSE", G_CALLBACK (xim_proxy_protocol_real_xim_close), proxy},
		{"XIM_TRIGGER_NOTIFY", G_CALLBACK (xim_proxy_protocol_real_xim_trigger_notify), proxy},
		{"XIM_ENCODING_NEGOTIATION", G_CALLBACK (xim_proxy_protocol_real_xim_encoding_negotiation), proxy},
		{"XIM_QUERY_EXTENSION", G_CALLBACK (xim_proxy_protocol_real_xim_query_extension), proxy},
		{"XIM_SET_IM_VALUES", G_CALLBACK (xim_proxy_protocol_real_xim_set_im_values), proxy},
		{"XIM_GET_IM_VALUES", G_CALLBACK (xim_proxy_protocol_real_xim_get_im_values), proxy},
		{"XIM_CREATE_IC", G_CALLBACK (xim_proxy_protocol_real_xim_create_ic), proxy},
		{"XIM_DESTROY_IC", G_CALLBACK (xim_proxy_protocol_real_xim_destroy_ic), proxy},
		{"XIM_SET_IC_VALUES", G_CALLBACK (xim_proxy_protocol_real_xim_set_ic_values), proxy},
		{"XIM_GET_IC_VALUES", G_CALLBACK (xim_proxy_protocol_real_xim_get_ic_values), proxy},
		{"XIM_SET_IC_FOCUS", G_CALLBACK (xim_proxy_protocol_real_xim_set_ic_focus), proxy},
		{"XIM_UNSET_IC_FOCUS", G_CALLBACK (xim_proxy_protocol_real_xim_unset_ic_focus), proxy},
		{"XIM_FORWARD_EVENT", G_CALLBACK (xim_proxy_protocol_real_xim_forward_event), proxy},
		{"XIM_SYNC", G_CALLBACK (xim_proxy_protocol_real_xim_sync), proxy},
		{"XIM_SYNC_REPLY", G_CALLBACK (xim_proxy_protocol_real_xim_sync_reply), proxy},
		{"XIM_RESET_IC", G_CALLBACK (xim_proxy_protocol_real_xim_reset_ic), proxy},
//FIXME		{"XIM_STR_CONVERSION_REPLY", G_CALLBACK (xim_proxy_protocol_real_xim_str_conversion_reply), proxy},
		{"XIM_PREEDIT_START_REPLY", G_CALLBACK (xim_proxy_protocol_real_xim_preedit_start_reply), proxy},
		{"XIM_PREEDIT_CARET_REPLY", G_CALLBACK (xim_proxy_protocol_real_xim_preedit_caret_reply), proxy},
		{NULL, NULL, NULL}
	};
	GXimLazySignalConnector csigs[] = {
		{"XIM_CONNECT_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_connect_reply), proxy},
		{"XIM_DISCONNECT_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_disconnect_reply), proxy},
//FIXME		{"XIM_AUTH_REQUIRED", G_CALLBACK (xim_proxy_client_protocol_real_xim_auth_required), proxy},
//FIXME		{"XIM_AUTH_NEXT", G_CALLBACK (xim_proxy_client_protocol_real_xim_auth_next), proxy},
//FIXME		{"XIM_AUTH_SETUP", G_CALLBACK (xim_proxy_client_protocol_real_xim_auth_setup), proxy},
		{"XIM_AUTH_NG", G_CALLBACK (xim_proxy_client_protocol_real_xim_auth_ng), proxy},
		{"XIM_ERROR", G_CALLBACK (xim_proxy_client_protocol_real_xim_error), proxy},
		{"XIM_OPEN_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_open_reply), proxy},
		{"XIM_CLOSE_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_close_reply), proxy},
		{"XIM_REGISTER_TRIGGERKEYS", G_CALLBACK (xim_proxy_client_protocol_real_xim_register_triggerkeys), proxy},
		{"XIM_TRIGGER_NOTIFY_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_trigger_notify_reply), proxy},
		{"XIM_SET_EVENT_MASK", G_CALLBACK (xim_proxy_client_protocol_real_xim_set_event_mask), proxy},
		{"XIM_ENCODING_NEGOTIATION_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_encoding_negotiation_reply), proxy},
		{"XIM_QUERY_EXTENSION_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_query_extension_reply), proxy},
		{"XIM_SET_IM_VALUES_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_set_im_values_reply), proxy},
		{"XIM_GET_IM_VALUES_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_get_im_values_reply), proxy},
		{"XIM_CREATE_IC_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_create_ic_reply), proxy},
		{"XIM_DESTROY_IC_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_destroy_ic_reply), proxy},
		{"XIM_SET_IC_VALUES_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_set_ic_values_reply), proxy},
		{"XIM_GET_IC_VALUES_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_get_ic_values_reply), proxy},
		{"XIM_FORWARD_EVENT", G_CALLBACK (xim_proxy_client_protocol_real_xim_forward_event), proxy},
		{"XIM_SYNC", G_CALLBACK (xim_proxy_client_protocol_real_xim_sync), proxy},
		{"XIM_SYNC_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_sync_reply), proxy},
		{"XIM_COMMIT", G_CALLBACK (xim_proxy_client_protocol_real_xim_commit), proxy},
		{"XIM_RESET_IC_REPLY", G_CALLBACK (xim_proxy_client_protocol_real_xim_reset_ic_reply), proxy},
//FIXME		{"XIM_GEOMETRY", G_CALLBACK (xim_proxy_client_protocol_real_xim_geometry), proxy},
//FIXME		{"XIM_STR_CONVERSION", G_CALLBACK (xim_proxy_client_protocol_real_xim_str_conversion), proxy},
		{"XIM_PREEDIT_START", G_CALLBACK (xim_proxy_client_protocol_real_xim_preedit_start), proxy},
		{"XIM_PREEDIT_DRAW", G_CALLBACK (xim_proxy_client_protocol_real_xim_preedit_draw), proxy},
		{"XIM_PREEDIT_CARET", G_CALLBACK (xim_proxy_client_protocol_real_xim_preedit_caret), proxy},
		{"XIM_PREEDIT_DONE", G_CALLBACK (xim_proxy_client_protocol_real_xim_preedit_done), proxy},
		{"XIM_STATUS_START", G_CALLBACK (xim_proxy_client_protocol_real_xim_status_start), proxy},
		{"XIM_STATUS_DRAW", G_CALLBACK (xim_proxy_client_protocol_real_xim_status_draw), proxy},
		{"XIM_STATUS_DONE", G_CALLBACK (xim_proxy_client_protocol_real_xim_status_done), proxy},
//FIXME		{"XIM_PREEDITSTATE", G_CALLBACK (xim_proxy_client_protocol_real_xim_preeditstate), proxy},
		{NULL, NULL, NULL}
	};

	proxy->client_table = g_hash_table_new_full(g_direct_hash, g_direct_equal,
						    NULL, g_object_unref);
	proxy->selection_table = g_hash_table_new(g_direct_hash, g_direct_equal);
	proxy->comm_table = g_hash_table_new(g_direct_hash, g_direct_equal);
	proxy->sconn_table = g_hash_table_new(g_direct_hash, g_direct_equal);
	proxy->simattr_table = g_hash_table_new_full(g_str_hash, g_str_equal,
						     g_free, NULL);
	proxy->sicattr_table = g_hash_table_new_full(g_str_hash, g_str_equal,
						     g_free, NULL);
	proxy->cimattr_table = g_hash_table_new_full(g_str_hash, g_str_equal,
						     g_free, NULL);
	proxy->cicattr_table = g_hash_table_new_full(g_str_hash, g_str_equal,
						     g_free, NULL);
	proxy->simid_table = g_new0(guint16, G_MAXUINT16 + 1);
	proxy->cimid_table = g_new0(guint16, G_MAXUINT16 + 1);
	proxy->latest_imid = 1;
	g_object_set(proxy, "proto_signals", sigs, NULL);
	g_object_set(proxy, "client_proto_signals", csigs, NULL);
}

static void
xim_proxy_connection_real_finalize(GObject *object)
{
	XimProxyConnection *conn = XIM_PROXY_CONNECTION (object);

	g_xim_str_free(conn->locale);

	if (G_OBJECT_CLASS (xim_proxy_connection_parent_class)->finalize)
		(* G_OBJECT_CLASS (xim_proxy_connection_parent_class)->finalize) (object);
}

static void
xim_proxy_connection_class_init(XimProxyConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = xim_proxy_connection_real_finalize;
}

static void
xim_proxy_connection_init(XimProxyConnection *proto)
{
}

/*
 * Public functions
 */
XimProxy *
xim_proxy_new(GdkDisplay  *dpy,
	      const gchar *server_name,
	      const gchar *connect_to)
{
	XimProxy *retval;

	g_return_val_if_fail (GDK_IS_DISPLAY (dpy), NULL);
	g_return_val_if_fail (server_name != NULL, NULL);
	g_return_val_if_fail (connect_to != NULL, NULL);

	retval = XIM_PROXY (g_object_new(XIM_TYPE_PROXY,
					 "display", dpy,
					 "server_name", server_name,
					 "connection_gtype", XIM_TYPE_PROXY_CONNECTION,
					 "connect_to", connect_to,
					  NULL));

	return retval;
}

gboolean
xim_proxy_take_ownership(XimProxy  *proxy,
			  gboolean    force,
			  GError    **error)
{
	g_return_val_if_fail (XIM_IS_PROXY (proxy), FALSE);

	return g_xim_srv_tmpl_take_ownership(G_XIM_SRV_TMPL (proxy),
					     force,
					     error);
}

gboolean
xim_proxy_is_pending(XimProxy *proxy)
{
	g_return_val_if_fail (XIM_IS_PROXY (proxy), FALSE);

	return proxy->pending_tasks > 0;
}

void
xim_proxy_disconnect_all(XimProxy *proxy)
{
	GHashTableIter iter;
	gpointer key, val;

	g_return_if_fail (XIM_IS_PROXY (proxy));

	while (xim_proxy_is_pending(proxy))
		g_main_context_iteration(NULL, TRUE);

	g_hash_table_iter_init(&iter, proxy->sconn_table);
	while (g_hash_table_iter_next(&iter, &key, &val)) {
		GXimConnection *conn = G_XIM_CONNECTION (val);
		GdkNativeWindow nw = g_xim_transport_get_client_window(G_XIM_TRANSPORT (conn));
		GdkNativeWindow snw;
		XimClient *client;

		client = g_hash_table_lookup(proxy->client_table,
					     G_XIM_NATIVE_WINDOW_TO_POINTER (nw));
		if (client) {
			snw = GDK_WINDOW_XID (g_xim_core_get_selection_window(G_XIM_CORE (client)));
			g_hash_table_remove(proxy->selection_table,
					    G_XIM_NATIVE_WINDOW_TO_POINTER (snw));
			g_hash_table_remove(proxy->client_table,
					    G_XIM_NATIVE_WINDOW_TO_POINTER (nw));
		}
	}
	g_hash_table_remove_all(proxy->selection_table);
	g_hash_table_remove_all(proxy->comm_table);
	g_hash_table_remove_all(proxy->simattr_table);
	g_hash_table_remove_all(proxy->cimattr_table);
	g_hash_table_remove_all(proxy->sicattr_table);
	g_hash_table_remove_all(proxy->cicattr_table);
	memset(proxy->simid_table, 0, sizeof (guint16) * G_MAXUINT16);
	memset(proxy->cimid_table, 0, sizeof (guint16) * G_MAXUINT16);
}
