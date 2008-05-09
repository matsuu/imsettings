/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * loopback.c
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <glib/gi18n-lib.h>
#include "utils.h"
#include "loopback.h"

#define XIM_LOOPBACK_GET_PRIVATE(_o_)		(G_TYPE_INSTANCE_GET_PRIVATE ((_o_), XIM_TYPE_LOOPBACK, XIMLoopbackPrivate))
#define XIM_TYPE_LOOPBACK_CONNECTION		(xim_loopback_connection_get_type())
#define XIM_LOOPBACK_CONNECTION(_o_)		(G_TYPE_CHECK_INSTANCE_CAST ((_o_), XIM_TYPE_LOOPBACK_CONNECTION, XIMLoopbackConnection))
#define XIM_LOOPBACK_CONNECTION_CLASS(_c_)	(G_TYPE_CHECK_CLASS_CAST ((_c_), XIM_TYPE_LOOPBACK_CONNECTION, XIMLoopbackConnectionClass))
#define XIM_IS_LOOPBACK_CONNECTION(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), XIM_TYPE_LOOPBACK_CONNECTION))
#define XIM_IS_LOOPBACK_CONNECTION_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), XIM_TYPE_LOOPBACK_CONNECTION))
#define XIM_LOOPBACK_CONNECTION_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), XIM_TYPE_LOOPBACK_CONNECTION, XIMLoopbackConnectionClass))

#ifdef GNOME_ENABLE_DEBUG
#define d(e)	e
#else
#define d(e)
#endif

typedef struct _XIMLoopbackSource {
	GSource      instance;
	GPollFD      poll_fd;
	Display     *dpy;
	XIMLoopback *loopback;
} XIMLoopbackSource;

typedef struct _XIMLoopbackPrivate {
	XIMLoopbackSource *event_loop;
	XIMAtoms          *atoms;
	GHashTable        *comm_table;
	Window             selection_window;
	Atom               atom_selection;
	Atom               atom_property;
} XIMLoopbackPrivate;

typedef struct _XIMLoopbackConnectionClass {
	XIMConnectionClass parent_class;
} XIMLoopbackConnectionClass;

typedef struct _XIMLoopbackConnection {
	XIMConnection  parent_instance;
	Display       *dpy;
	Window         server_window;
	Window         comm_window;
	GString       *packet_cache;
} XIMLoopbackConnection;

enum {
	PROP_0,
	PROP_SELECTION_WINDOW,
	PROP_SELECTION_ATOM,
	LAST_PROP
};

static gboolean _source_prepare                       (GSource                *source,
                                                       gint                   *timeout);
static gboolean _source_check                         (GSource                *source);
static gboolean _source_dispatch                      (GSource                *source,
                                                       GSourceFunc             callback,
                                                       gpointer                data);
static void     _source_finalize                      (GSource                *source);
static gboolean xim_loopback_send_selection_notify    (XIMLoopback            *loopback,
                                                       Display                *dpy,
                                                       XSelectionRequestEvent *xevent,
                                                       const gchar            *data,
                                                       gsize                   length);
static gboolean xim_loopback_real_connect             (XIMProtocol            *proto,
                                                       guint16                 major_version,
                                                       guint16                 minor_version,
                                                       const GSList           *list);
static gboolean xim_loopback_real_auth_reply          (XIMProtocol            *proto,
                                                       const gchar            *auth_data,
                                                       gsize                   length);
static gboolean xim_loopback_real_disconnect          (XIMProtocol            *proto);
static gboolean xim_loopback_real_open                (XIMProtocol            *proto,
                                                       const gchar            *locale);
static gboolean xim_loopback_real_close               (XIMProtocol            *proto,
                                                       guint16                 imid);
static gboolean xim_loopback_real_trigger_notify      (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       guint16                 icid,
                                                       guint32                 flag,
                                                       guint32                 index,
                                                       guint32                 mask);
static gboolean xim_loopback_real_encoding_negotiation(XIMProtocol            *proto,
                                                       const GSList           *encodings,
                                                       const GSList           *details);
static gboolean xim_loopback_real_query_extension     (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       const GSList           *extensions);
static gboolean xim_loopback_real_set_im_values       (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       const GSList           *attributes);
static gboolean xim_loopback_real_get_im_values       (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       const GSList           *attr_id);
static gboolean xim_loopback_real_create_ic           (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       const GSList           *attributes);
static gboolean xim_loopback_real_destroy_ic          (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       guint16                 icid);
static gboolean xim_loopback_real_set_ic_values       (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       guint16                 icid,
                                                       const GSList           *attributes);
static gboolean xim_loopback_real_get_ic_values       (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       guint16                 icid,
                                                       const GSList           *attr_id);
static gboolean xim_loopback_real_set_ic_focus        (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       guint16                 icid);
static gboolean xim_loopback_real_unset_ic_focus      (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       guint16                 icid);
static gboolean xim_loopback_real_reset_ic            (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       guint16                 icid);
static gboolean xim_loopback_real_str_conversion_reply(XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       guint16                 icid,
                                                       guint32                 feedback,
                                                       XIMStrConvText         *text);
static gboolean xim_loopback_real_preedit_start_reply (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       guint16                 icid,
                                                       gint32                  return_value);
static gboolean xim_loopback_real_preedit_caret_reply (XIMProtocol            *proto,
                                                       guint16                 imid,
                                                       guint16                 icid,
                                                       guint32                 position);

static GType                  xim_loopback_connection_get_type(void) G_GNUC_CONST;
static XIMLoopbackConnection *xim_loopback_connection_new     (Display *dpy,
							       Window   server_window,
							       Window   client_window);


static GSourceFuncs source_funcs = {
	_source_prepare,
	_source_check,
	_source_dispatch,
	_source_finalize
};
static const gchar *locales[] = {
	"aa_DJ",
	"aa_ER",
	"aa_ET",
	"af_ZA",
	"am_ET",
	"an_ES",
	"ar_AE",
	"ar_BH",
	"ar_DZ",
	"ar_EG",
	"ar_IN",
	"ar_IQ",
	"ar_JO",
	"ar_KW",
	"ar_LB",
	"ar_LY",
	"ar_MA",
	"ar_OM",
	"ar_QA",
	"ar_SA",
	"ar_SD",
	"ar_SY",
	"ar_TN",
	"ar_YE",
	"as_IN",
	"ast_ES",
	"az_AZ",
	"be_BY",
	"ber_DZ",
	"ber_MA",
	"bg_BG",
	"bn_BD",
	"bn_IN",
	"br_FR",
	"bs_BA",
	"byn_ER",
	"ca_AD",
	"ca_ES",
	"ca_FR",
	"ca_IT",
	"crh_UA",
	"cs_CZ",
	"csb_PL",
	"cy_GB",
	"da_DK",
	"de_AT",
	"de_BE",
	"de_CH",
	"de_DE",
	"de_LI",
	"de_LU",
	"dz_BT",
	"el_CY",
	"el_GR",
	"en_AU",
	"en_BW",
	"en_CA",
	"en_DK",
	"en_GB",
	"en_HK",
	"en_IE",
	"en_IN",
	"en_NG",
	"en_NZ",
	"en_PH",
	"en_SG",
	"en_US",
	"en_ZA",
	"en_ZW",
	"eo",
	"es_AR",
	"es_BO",
	"es_CL",
	"es_CO",
	"es_CR",
	"es_DO",
	"es_EC",
	"es_ES",
	"es_GT",
	"es_HN",
	"es_MX",
	"es_NI",
	"es_PA",
	"es_PE",
	"es_PR",
	"es_PY",
	"es_SV",
	"es_US",
	"es_UY",
	"es_VE",
	"et_EE",
	"eu_ES",
	"eu_FR",
	"fa_IR",
	"fi_FI",
	"fil_PH",
	"fo_FO",
	"fr_BE",
	"fr_CA",
	"fr_CH",
	"fr_FR",
	"fr_LU",
	"fur_IT",
	"fy_DE",
	"fy_NL",
	"ga_IE",
	"gd_GB",
	"gez_ER",
	"gez_ET",
	"gl_ES",
	"gu_IN",
	"gv_GB",
	"ha_NG",
	"he_IL",
	"hi_IN",
	"hr_HR",
	"hsb_DE",
	"hu_HU",
	"hy_AM",
	"ia",
	"id_ID",
	"ig_NG",
	"ik_CA",
	"is_IS",
	"it_CH",
	"it_IT",
	"iu_CA",
	"iw_IL",
	"ja_JP",
	"ka_GE",
	"kk_KZ",
	"kl_GL",
	"km_KH",
	"kn_IN",
	"ko_KR",
	"ks_IN",
	"ku_TR",
	"kw_GB",
	"ky_KG",
	"lg_UG",
	"li_BE",
	"li_NL",
	"lo_LA",
	"lt_LT",
	"lv_LV",
	"mai_IN",
	"mg_MG",
	"mi_NZ",
	"mk_MK",
	"ml_IN",
	"mn_MN",
	"mr_IN",
	"ms_MY",
	"mt_MT",
	"nb_NO",
	"nds_DE",
	"nds_NL",
	"ne_NP",
	"nl_BE",
	"nl_NL",
	"nn_NO",
	"nr_ZA",
	"nso_ZA",
	"oc_FR",
	"om_ET",
	"om_KE",
	"or_IN",
	"pa_IN",
	"pa_PK",
	"pap_AN",
	"pl_PL",
	"pt_BR",
	"pt_PT",
	"ro_RO",
	"ru_RU",
	"ru_UA",
	"rw_RW",
	"sa_IN",
	"sc_IT",
	"se_NO",
	"si_LK",
	"sid_ET",
	"sk_SK",
	"sl_SI",
	"so_DJ",
	"so_ET",
	"so_KE",
	"so_SO",
	"sq_AL",
	"sr_ME",
	"sr_RS",
	"ss_ZA",
	"st_ZA",
	"sv_FI",
	"sv_SE",
	"ta_IN",
	"te_IN",
	"tg_TJ",
	"th_TH",
	"ti_ER",
	"ti_ET",
	"tig_ER",
	"tk_TM",
	"tl_PH",
	"tn_ZA",
	"tr_CY",
	"tr_TR",
	"ts_ZA",
	"tt_RU",
	"ug_CN",
	"uk_UA",
	"ur_PK",
	"uz_UZ",
	"ve_ZA",
	"vi_VN",
	"wa_BE",
	"wo_SN",
	"xh_ZA",
	"yi_US",
	"yo_NG",
	"zh_CN",
	"zh_HK",
	"zh_SG",
	"zh_TW",
	"zu_ZA",
	NULL
};

G_DEFINE_TYPE (XIMLoopback, xim_loopback, XIM_TYPE_PROTOCOL);
G_DEFINE_TYPE (XIMLoopbackConnection, xim_loopback_connection, XIM_TYPE_CONNECTION);

/*
 * Private functions
 */
static void
_weak_notify_connection_cb(gpointer  data,
			   GObject  *object)
{
	GHashTable *conn_table = data;
	gconstpointer w;
	gboolean verbose;

	w = (gconstpointer)XIM_LOOPBACK_CONNECTION (object)->comm_window;
	g_hash_table_remove(conn_table, w);
	g_object_get(object, "verbose", &verbose, NULL);
	if (verbose)
		g_print("%ld: LOOP: EOL'd an instance\n", (gulong)w);
}

static gboolean
_source_prepare(GSource *source,
		gint    *timeout)
{
	XIMLoopbackSource *s = (XIMLoopbackSource *)source;

	/* just waiting for polling fd */
	*timeout = -1;

	return XPending(s->dpy) > 0;
}

static gboolean
_source_check(GSource *source)
{
	XIMLoopbackSource *s = (XIMLoopbackSource *)source;
	gboolean retval = FALSE;

	if (s->poll_fd.revents & G_IO_IN)
		retval = XPending(s->dpy) > 0;

	return retval;
}

static gboolean
_source_dispatch(GSource     *source,
		 GSourceFunc  callback,
		 gpointer     data)
{
	XIMLoopbackSource *s = (XIMLoopbackSource *)source;
	XIMLoopbackPrivate *priv = XIM_LOOPBACK_GET_PRIVATE (s->loopback);

	while (XPending(s->dpy)) {
		XEvent xevent;
		XClientMessageEvent xcev;
		Atom atom;

		XNextEvent(s->dpy, &xevent);

		switch (xevent.type) {
		    case SelectionRequest:
			    atom = xevent.xselectionrequest.property;
			    if (atom == priv->atoms->atom_locales) {
				    gchar *supported_locales, *p;

				    if (XIM_PROTOCOL (s->loopback)->verbose)
					    g_print("D: LOOP: LOCALES on SelectionRequest\n");

				    supported_locales = g_strjoinv(",", (gchar **)locales);
				    p = g_strdup_printf("@locale=%s", supported_locales);
				    xim_loopback_send_selection_notify(s->loopback,
								       s->dpy,
								       (XSelectionRequestEvent *)&xevent.xselectionrequest,
								       p, strlen(p) + 1);
				    g_free(p);
				    g_free(supported_locales);
			    } else if (atom == priv->atoms->atom_transport) {
				    const gchar *transport = "@transport=X/";

				    if (XIM_PROTOCOL (s->loopback)->verbose)
					    g_print("D: LOOP: TRANSPORT on SelectionRequest\n");

				    xim_loopback_send_selection_notify(s->loopback,
								       s->dpy,
								       (XSelectionRequestEvent *)&xevent.xselectionrequest,
								       transport, strlen(transport) + 1);
			    } else {
				    char *str = XGetAtomName(s->dpy, xevent.xselectionrequest.property);

				    g_warning("Unknown property `%s' is requested", str);
				    XFree(str);
			    }
			    break;
		    case SelectionNotify:
			    d(g_print("EV: SelectionNotify\n"));
			    break;
		    case Expose:
			    d(g_print("EV: Expose\n"));
			    break;
		    case ConfigureNotify:
			    d(g_print("EV: ConfigureNotify\n"));
			    break;
		    case DestroyNotify:
			    d(g_print("EV: DestroyNotify\n"));
			    break;
		    case ClientMessage:
			    if (xevent.xclient.message_type == priv->atoms->atom_xim_xconnect) {
				    XIMLoopbackConnection *conn;
				    Window client_window;

				    client_window = xevent.xclient.data.l[0];
				    conn = xim_loopback_connection_new(s->dpy,
								       xevent.xclient.window,
								       client_window);
				    if (conn == NULL) {
					    g_warning("Failed to create a loopback connection");
					    break;
				    }
				    g_object_set(G_OBJECT (conn),
						 "verbose", XIM_PROTOCOL (s->loopback)->verbose,
						 NULL);

				    if (XIM_PROTOCOL (s->loopback)->verbose)
					    g_print("%ld: LOOP: _XIM_XCONNECT [comm_window: %ld]\n",
						    client_window, conn->comm_window);

				    g_hash_table_insert(priv->comm_table,
							(gpointer)conn->comm_window,
							conn);
				    g_object_weak_ref(G_OBJECT (conn),
						      _weak_notify_connection_cb,
						      priv->comm_table);

				    xcev.type = ClientMessage;
				    xcev.window = client_window;
				    xcev.message_type = xevent.xclient.message_type;
				    xcev.format = 32;
				    xcev.data.l[0] = conn->comm_window;
				    xcev.data.l[1] = 0;
				    xcev.data.l[2] = 0;
				    xcev.data.l[3] = TRANSPORT_MAX;

				    XSendEvent(s->dpy, client_window, False, NoEventMask, (XEvent *)&xcev);
			    } else {
				    XIMLoopbackConnection *conn;
				    XIMProtocol *proto;
				    gint i;

				    conn = g_hash_table_lookup(priv->comm_table,
							       (gpointer)xevent.xclient.window);
				    if (conn == NULL) {
					    g_warning("No loopback connection for %ld",
						      xevent.xclient.window);
					    break;
				    }
				    g_object_get(G_OBJECT (conn), "protocol_object", &proto, NULL);

				    if (xevent.xclient.message_type == priv->atoms->atom_xim_moredata) {
					    for (i = 0; i < TRANSPORT_SIZE; i++) {
						    g_string_append_c(conn->packet_cache,
								      xevent.xclient.data.b[i]);
					    }
				    } else if (xevent.xclient.message_type == priv->atoms->atom_xim_protocol) {
					    Atom atom_type;
					    int format;
					    unsigned long nitems, bytes;
					    unsigned char *prop;
					    gsize packlen;
					    XIMEventType op = LAST_XIM_EVENTS;

					    if (xevent.xclient.format == 32) {
						    XGetWindowProperty(s->dpy,
								       xevent.xclient.window,
								       xevent.xclient.data.l[1],
								       0, xevent.xclient.data.l[0],
								       True, AnyPropertyType,
								       &atom_type, &format,
								       &nitems, &bytes, &prop);
						    if (prop) {
							    for (i = 0; i < nitems; i++) {
								    g_string_append_c(conn->packet_cache,
										      prop[i]);
							    }
						    }
						    packlen = conn->packet_cache->len;
						    xim_protocol_process_packets(proto,
										 conn->packet_cache->str,
										 &packlen);
						    op = conn->packet_cache->str[0];
						    g_string_erase(conn->packet_cache,
								   0,
								   conn->packet_cache->len - packlen);
						    if (conn->packet_cache->len > 0) {
							    g_warning("There may be unprocessed packets in the cache: %" G_GSIZE_FORMAT " packets.",
								      conn->packet_cache->len);
						    };
					    } else if (xevent.xclient.format == 8) {
						    for (i = 0; i < TRANSPORT_SIZE; i++) {
							    g_string_append_c(conn->packet_cache,
									      xevent.xclient.data.b[i]);
						    }
						    packlen = conn->packet_cache->len;
						    xim_protocol_process_packets(proto,
										 conn->packet_cache->str,
										 &packlen);
						    op = conn->packet_cache->str[0];
						    /* here is likely to see unprocessed packets
						     * since no way of communicating how much packets
						     * in the protocol is available.
						     */
						    g_string_erase(conn->packet_cache, 0, -1);
					    } else {
						    g_warning("Invalid packet.");
					    }
					    if (op == XIM_DISCONNECT_REPLY) {
						    /* all the connections should be closed here */
						    g_object_unref(conn);
					    }
				    } else {
					    char *str = XGetAtomName(s->dpy, xevent.xclient.message_type);

					    g_warning("non XIM message: %s", str);
					    XFree(s);
				    }
			    }
			    break;
		    case MappingNotify:
			    d(g_print("EV: MappingNotify\n"));
			    break;
		    default:
			    break;
		}
	}

	return TRUE;
}

static void
_source_finalize(GSource *source)
{
}

static gboolean
xim_loopback_send_selection_notify(XIMLoopback            *loopback,
				   Display                *dpy,
				   XSelectionRequestEvent *xevent,
				   const gchar            *data,
				   gsize                   length)
{
	XSelectionEvent ev;

	ev.type = SelectionNotify;
	ev.requestor = xevent->requestor;
	ev.selection = xevent->selection;
	ev.target    = xevent->target;
	ev.property  = xevent->property;

	XChangeProperty(dpy, ev.requestor,
			ev.property,
			ev.target,
			8, PropModeReplace,
			(unsigned char *)data, length);
	XSendEvent(dpy, ev.requestor, 0, 0, (XEvent *)&ev);
	XFlush(dpy);

	return TRUE;
}

static void
xim_loopback_set_property(GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
	switch (prop_id) {
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
xim_loopback_get_property(GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
	XIMLoopbackPrivate *priv = XIM_LOOPBACK_GET_PRIVATE (object);

	switch (prop_id) {
	    case PROP_SELECTION_WINDOW:
		    g_value_set_ulong(value, priv->selection_window);
		    break;
	    case PROP_SELECTION_ATOM:
		    g_value_set_ulong(value, priv->atom_selection);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
xim_loopback_finalize(GObject *object)
{
	XIMLoopbackPrivate *priv = XIM_LOOPBACK_GET_PRIVATE (object);
	XIMProtocol *proto = XIM_PROTOCOL (object);

	if (priv->selection_window)
		XDestroyWindow(proto->dpy, priv->selection_window);
	g_hash_table_destroy(priv->comm_table);
	g_source_destroy((GSource *)priv->event_loop);

	if (G_OBJECT_CLASS (xim_loopback_parent_class)->finalize)
		G_OBJECT_CLASS (xim_loopback_parent_class)->finalize(object);
}

static void
xim_loopback_connection_finalize(GObject *object)
{
	XIMLoopbackConnection *conn = XIM_LOOPBACK_CONNECTION (object);

	if (conn->comm_window)
		XDestroyWindow(conn->dpy, conn->comm_window);
	g_string_free(conn->packet_cache, TRUE);

	if (G_OBJECT_CLASS (xim_loopback_connection_parent_class)->finalize)
		G_OBJECT_CLASS (xim_loopback_connection_parent_class)->finalize(object);
}

static void
xim_loopback_real_forward_raw_packets(XIMProtocol *proto,
				      const gchar *packets,
				      gsize       *length)
{
	xim_protocol_process_event(proto, packets, length);
}

static gboolean
xim_loopback_real_connect(XIMProtocol  *proto,
			  guint16       major_version,
			  guint16       minor_version,
			  const GSList *list)
{
	/* We don't need any authentications here
	 * just sending a XIM_CONNECT_REPLY
	 */
	xim_protocol_send(proto,
			  XIM_CONNECT_REPLY, 0,
			  2,
			  XIM_TYPE_WORD, 1,
			  XIM_TYPE_WORD, 0);

	return TRUE;
}

static gboolean
xim_loopback_real_auth_reply(XIMProtocol *proto,
			     const gchar *auth_data,
			     gsize        length)
{
	return FALSE;
}

static gboolean
xim_loopback_real_disconnect(XIMProtocol *proto)
{
	return FALSE;
}

static gboolean
xim_loopback_real_open(XIMProtocol *proto,
		       const gchar *locale)
{
	xim_protocol_send(proto, XIM_OPEN_REPLY, 0,
			  4,
			  XIM_TYPE_WORD, 0,
			  XIM_TYPE_WORD, 0,
			  XIM_TYPE_WORD, 0,
			  XIM_TYPE_WORD, 0);
	return TRUE;
	g_print("%s: %s\n", __FUNCTION__, locale);
	return FALSE;
}

static gboolean
xim_loopback_real_close(XIMProtocol *proto,
			guint16      imid)
{
	return FALSE;
}

static gboolean
xim_loopback_real_trigger_notify(XIMProtocol *proto,
				 guint16      imid,
				 guint16      icid,
				 guint32      flag,
				 guint32      index,
				 guint32      mask)
{
	return FALSE;
}

static gboolean
xim_loopback_real_encoding_negotiation(XIMProtocol  *proto,
				       const GSList *encodings,
				       const GSList *details)
{
	return FALSE;
}

static gboolean
xim_loopback_real_query_extension(XIMProtocol  *proto,
				  guint16       imid,
				  const GSList *extensions)
{
	return FALSE;
}

static gboolean
xim_loopback_real_set_im_values(XIMProtocol  *proto,
				guint16       imid,
				const GSList *attributes)
{
	return FALSE;
}

static gboolean
xim_loopback_real_get_im_values(XIMProtocol  *proto,
				guint16       imid,
				const GSList *attr_id)
{
	return FALSE;
}

static gboolean
xim_loopback_real_create_ic(XIMProtocol  *proto,
			    guint16       imid,
			    const GSList *attributes)
{
	return FALSE;
}

static gboolean
xim_loopback_real_destroy_ic(XIMProtocol *proto,
			     guint16      imid,
			     guint16      icid)
{
	return FALSE;
}

static gboolean
xim_loopback_real_set_ic_values(XIMProtocol  *proto,
				guint16       imid,
				guint16       icid,
				const GSList *attributes)
{
	return FALSE;
}

static gboolean
xim_loopback_real_get_ic_values(XIMProtocol  *proto,
				guint16       imid,
				guint16       icid,
				const GSList *attr_id)
{
	return FALSE;
}

static gboolean
xim_loopback_real_set_ic_focus(XIMProtocol *proto,
			       guint16      imid,
			       guint16      icid)
{
	return FALSE;
}

static gboolean
xim_loopback_real_unset_ic_focus(XIMProtocol *proto,
				 guint16      imid,
				 guint16      icid)
{
	return FALSE;
}

static gboolean
xim_loopback_real_reset_ic(XIMProtocol *proto,
			   guint16      imid,
			   guint16      icid)
{
	return FALSE;
}

static gboolean
xim_loopback_real_str_conversion_reply(XIMProtocol    *proto,
				       guint16         imid,
				       guint16         icid,
				       guint32         feedback,
				       XIMStrConvText *text)
{
	return FALSE;
}

static gboolean
xim_loopback_real_preedit_start_reply(XIMProtocol *proto,
				      guint16      imid,
				      guint16      icid,
				      gint32       return_value)
{
	return FALSE;
}

static gboolean
xim_loopback_real_preedit_caret_reply(XIMProtocol *proto,
				      guint16      imid,
				      guint16      icid,
				      guint32      position)
{
	return FALSE;
}


static void
xim_loopback_class_init(XIMLoopbackClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	XIMProtocolClass *proto_class = XIM_PROTOCOL_CLASS (klass);

	g_type_class_add_private(klass, sizeof (XIMLoopbackPrivate));

	object_class->set_property = xim_loopback_set_property;
	object_class->get_property = xim_loopback_get_property;
	object_class->finalize     = xim_loopback_finalize;

	proto_class->forward_raw_packets  = xim_loopback_real_forward_raw_packets;

	/* properties */
	g_object_class_install_property(object_class, PROP_SELECTION_WINDOW,
					g_param_spec_ulong("selection_window",
							   _("Selection Window ID"),
							   _("Window ID to communicate the client"),
							   0,
							   G_MAXULONG,
							   0,
							   G_PARAM_READABLE));
	g_object_class_install_property(object_class, PROP_SELECTION_ATOM,
					g_param_spec_ulong("selection_atom",
							   _("Selection Atom"),
							   _("Atom for selection"),
							   0,
							   G_MAXULONG,
							   0,
							   G_PARAM_READABLE));

	/* signals */
}

static void
xim_loopback_connection_class_init(XIMLoopbackConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = xim_loopback_connection_finalize;
}

static void
xim_loopback_init(XIMLoopback *loop)
{
	XIMLoopbackPrivate *priv = XIM_LOOPBACK_GET_PRIVATE (loop);

	priv->comm_table = g_hash_table_new_full(g_direct_hash, g_direct_equal,
						 NULL, g_object_unref);
	g_signal_connect(loop, "connect",
			 G_CALLBACK (xim_loopback_real_connect), NULL);
	g_signal_connect(loop, "auth_reply",
			 G_CALLBACK (xim_loopback_real_auth_reply), NULL);
	g_signal_connect(loop, "disconnect",
			 G_CALLBACK (xim_loopback_real_disconnect), NULL);
	g_signal_connect(loop, "open",
			 G_CALLBACK (xim_loopback_real_open), NULL);
	g_signal_connect(loop, "close",
			 G_CALLBACK (xim_loopback_real_close), NULL);
	g_signal_connect(loop, "trigger_notify",
			 G_CALLBACK (xim_loopback_real_trigger_notify), NULL);
	g_signal_connect(loop, "encoding_negotiation",
			 G_CALLBACK (xim_loopback_real_encoding_negotiation), NULL);
	g_signal_connect(loop, "query_extension",
			 G_CALLBACK (xim_loopback_real_query_extension), NULL);
	g_signal_connect(loop, "set_im_values",
			 G_CALLBACK (xim_loopback_real_set_im_values), NULL);
	g_signal_connect(loop, "get_im_values",
			 G_CALLBACK (xim_loopback_real_get_im_values), NULL);
	g_signal_connect(loop, "create_ic",
			 G_CALLBACK (xim_loopback_real_create_ic), NULL);
	g_signal_connect(loop, "destroy_ic",
			 G_CALLBACK (xim_loopback_real_destroy_ic), NULL);
	g_signal_connect(loop, "set_ic_values",
			 G_CALLBACK (xim_loopback_real_set_ic_values), NULL);
	g_signal_connect(loop, "get_ic_values",
			 G_CALLBACK (xim_loopback_real_get_ic_values), NULL);
	g_signal_connect(loop, "set_ic_focus",
			 G_CALLBACK (xim_loopback_real_set_ic_focus), NULL);
	g_signal_connect(loop, "unset_ic_focus",
			 G_CALLBACK (xim_loopback_real_unset_ic_focus), NULL);
	g_signal_connect(loop, "reset_ic",
			 G_CALLBACK (xim_loopback_real_reset_ic), NULL);
	g_signal_connect(loop, "str_conversion_reply",
			 G_CALLBACK (xim_loopback_real_str_conversion_reply), NULL);
	g_signal_connect(loop, "preedit_start_reply",
			 G_CALLBACK (xim_loopback_real_preedit_start_reply), NULL);
	g_signal_connect(loop, "preedit_caret_reply",
			 G_CALLBACK (xim_loopback_real_preedit_caret_reply), NULL);
}

static void
xim_loopback_connection_init(XIMLoopbackConnection *conn)
{
	conn->packet_cache = g_string_new(NULL);
}

static XIMLoopbackConnection *
xim_loopback_connection_new(Display *dpy,
			    Window   server_window,
			    Window   client_window)
{
	XIMLoopbackConnection *retval;

	g_return_val_if_fail (dpy != NULL, NULL);

	retval = XIM_LOOPBACK_CONNECTION (g_object_new(XIM_TYPE_LOOPBACK_CONNECTION,
						       "display", dpy,
						       "client_window", client_window,
						       "protocol", XIM_TYPE_LOOPBACK,
						       NULL));
	if (retval) {
		/* lazy hack */
		retval->dpy = dpy;
		retval->server_window = server_window;
		retval->comm_window = XCreateSimpleWindow(dpy, server_window,
							  0, 0, 1, 1, 0, 0, 0);
	}

	return retval;
}

/*
 * Public functions
 */
XIMLoopback *
xim_loopback_new(const gchar *display_name)
{
	Display *dpy;
	gchar *dpy_name;

	dpy_name = xim_substitute_display_name(display_name);
	/* We have own display here to manage the separate event loop */
	dpy = XOpenDisplay(dpy_name);
	if (dpy == NULL) {
		g_printerr("Can't open a X display: %s\n", dpy_name);
		return NULL;
	}
	return XIM_LOOPBACK (g_object_new(XIM_TYPE_LOOPBACK,
					  "display", dpy,
					  NULL));
}

gboolean
xim_loopback_setup(XIMLoopback *loop)
{
	XIMLoopbackPrivate *priv;
	XIMProtocol *proto;
	Window owner;

	g_return_val_if_fail (XIM_IS_LOOPBACK (loop), FALSE);

	proto = XIM_PROTOCOL (loop);
	priv = XIM_LOOPBACK_GET_PRIVATE (loop);
	if (priv->selection_window)
		return TRUE;

	priv->atoms = xim_get_atoms(proto->dpy);
	priv->atom_selection = XInternAtom(proto->dpy, "@server=none", False);
	if ((owner = XGetSelectionOwner(proto->dpy, priv->atom_selection)) != None) {
		g_warning("Loopback instance already exists.");

		return FALSE;
	}
	priv->selection_window = XCreateSimpleWindow(proto->dpy,
						     DefaultRootWindow(proto->dpy),
						     0, 0, 1, 1, 0, 0, 0);
	XSetSelectionOwner(proto->dpy,
			   priv->atom_selection,
			   priv->selection_window,
			   CurrentTime);
	XSelectInput(proto->dpy, DefaultRootWindow(proto->dpy), 0);
	XSync(proto->dpy, False);

	priv->event_loop = (XIMLoopbackSource *)g_source_new(&source_funcs, sizeof (XIMLoopbackSource));
	priv->event_loop->dpy = proto->dpy;
	priv->event_loop->loopback = loop;
	priv->event_loop->poll_fd.fd = ConnectionNumber(proto->dpy);
	priv->event_loop->poll_fd.events = G_IO_IN;

	g_source_add_poll((GSource *)priv->event_loop, &priv->event_loop->poll_fd);
	g_source_set_can_recurse((GSource *)priv->event_loop, TRUE);
	g_source_attach((GSource *)priv->event_loop, NULL);

	return TRUE;
}
