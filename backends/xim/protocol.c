/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * protocol.c
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
#include <X11/Xproto.h>
#include "imsettings/imsettings-marshal.h"
#include "connection.h"
#include "utils.h"
#include "protocol.h"


#define XIM_PROTOCOL_GET_PRIVATE(_o_)	(G_TYPE_INSTANCE_GET_PRIVATE ((_o_), XIM_TYPE_PROTOCOL, XIMProtocolPrivate))
#define PAD4(_n_)			((4 - ((_n_) % 4)) % 4)
#define XIM_ORDER_MSB			0x42
#define XIM_ORDER_LSB			0x6c

#ifdef GNOME_ENABLE_DEBUG
#define d(e)	e
#else
#define d(e)
#endif

typedef struct _XIMProtocolPrivate {
	XIMAtoms      *atoms;
	XIMConnection *conn;
	guint8         host_byte_order;
	guint8         byte_order;
	guint32        major_version;
	guint32        minor_version;
} XIMProtocolPrivate;

enum {
	PROP_0,
	PROP_DISPLAY,
	PROP_VERBOSE,
	PROP_CONN,
	LAST_PROP
};
enum {
	BEGIN_SIGNAL = LAST_XIM_EVENTS,
	LAST_SIGNAL
};


static gboolean xim_protocol_real_connect                   (XIMProtocol    *proto,
							     guint16         major_version,
							     guint16         minor_version,
							     const GSList   *list);
static gboolean xim_protocol_real_connect_reply             (XIMProtocol    *proto,
							     guint16         major_version,
							     guint16         minor_version);
static gboolean xim_protocol_real_disconnect                (XIMProtocol    *proto);
static gboolean xim_protocol_real_disconnect_reply          (XIMProtocol    *proto);
static gboolean xim_protocol_real_auth_required             (XIMProtocol    *proto,
							     const gchar    *auth_data,
							     gsize           length);
static gboolean xim_protocol_real_auth_reply                (XIMProtocol    *proto,
							     const gchar    *auth_data,
							     gsize           length);
static gboolean xim_protocol_real_auth_next                 (XIMProtocol    *proto,
							     const gchar    *auth_data,
							     gsize           length);
static gboolean xim_protocol_real_auth_setup                (XIMProtocol    *proto,
							     const GSList   *list);
static gboolean xim_protocol_real_auth_ng                   (XIMProtocol    *proto);
static gboolean xim_protocol_real_error                     (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     guint16         mask,
							     guint16         error_code);
static gboolean xim_protocol_real_open                      (XIMProtocol    *proto,
							     const gchar    *locale);
static gboolean xim_protocol_real_open_reply                (XIMProtocol    *proto,
							     const GSList   *imattrs,
							     const GSList   *icattrs);
static gboolean xim_protocol_real_close                     (XIMProtocol    *proto,
							     guint16         imid);
static gboolean xim_protocol_real_close_reply               (XIMProtocol    *proto,
							     guint16         imid);
static gboolean xim_protocol_real_register_triggerkeys      (XIMProtocol    *proto,
							     guint16         imid,
							     const GSList   *onkeys,
							     const GSList   *offkeys);
static gboolean xim_protocol_real_trigger_notify            (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     guint32         flag,
							     guint32         index,
							     guint32         mask);
static gboolean xim_protocol_real_trigger_notify_reply      (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_set_event_mask            (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     guint32         forward_event_mask,
							     guint32         synchronous_event_mask);
static gboolean xim_protocol_real_encoding_negotiation      (XIMProtocol    *proto,
							     const GSList   *encodings,
							     const GSList   *details);
static gboolean xim_protocol_real_encoding_negotiation_reply(XIMProtocol    *proto,
							     guint16         imid,
							     guint16         category,
							     gint16          index);
static gboolean xim_protocol_real_query_extension           (XIMProtocol    *proto,
							     guint16         imid,
							     const GSList   *extensions);
static gboolean xim_protocol_real_query_extension_reply     (XIMProtocol    *proto,
							     guint16         imid,
							     const GSList   *extensions);
static gboolean xim_protocol_real_set_im_values             (XIMProtocol    *proto,
							     guint16         imid,
							     const GSList   *attributes);
static gboolean xim_protocol_real_set_im_values_reply       (XIMProtocol    *proto,
							     guint16         imid);
static gboolean xim_protocol_real_get_im_values             (XIMProtocol    *proto,
							     guint16         imid,
							     const GSList   *attr_id);
static gboolean xim_protocol_real_get_im_values_reply       (XIMProtocol    *proto,
							     guint16         imid,
							     const GSList   *attributes);
static gboolean xim_protocol_real_create_ic                 (XIMProtocol    *proto,
							     guint16         imid,
							     const GSList   *attributes);
static gboolean xim_protocol_real_create_ic_reply           (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_destroy_ic                (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_destroy_ic_reply          (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_set_ic_values             (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     const GSList   *attributes);
static gboolean xim_protocol_real_set_ic_values_reply       (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_get_ic_values             (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     const GSList   *attr_id);
static gboolean xim_protocol_real_get_ic_values_reply       (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     const GSList   *attributes);
static gboolean xim_protocol_real_set_ic_focus              (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_unset_ic_focus            (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_forward_event             (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     guint16         flag,
							     XEvent         *event);
static gboolean xim_protocol_real_sync                      (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_sync_reply                (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_commit                    (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     guint16         flag,
							     const gchar    *packets,
							     gsize          *length);
static gboolean xim_protocol_real_reset_ic                  (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_reset_ic_reply            (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     GString        *string);
static gboolean xim_protocol_real_geometry                  (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_str_conversion            (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     guint16         position,
							     guint32         caret_direction,
							     guint16         factor,
							     guint16         operation,
							     gint16          type_length);
static gboolean xim_protocol_real_str_conversion_reply      (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     guint32         feedback,
							     XIMStrConvText *text);
static gboolean xim_protocol_real_preedit_start             (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_preedit_start_reply       (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     gint32          return_value);
static gboolean xim_protocol_real_preedit_draw              (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     gint32          caret,
							     gint32          chg_first,
							     gint32          chg_length,
							     guint32         status,
							     const GString  *string,
							     const GSList   *feedbacks);
static gboolean xim_protocol_real_preedit_caret             (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     gint32          position,
							     guint32         direction,
							     guint32         style);
static gboolean xim_protocol_real_preedit_caret_reply       (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     guint32         position);
static gboolean xim_protocol_real_preedit_done              (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_status_start              (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_status_draw               (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     guint32         type,
							     const gchar    *packets,
							     gsize          *length);
static gboolean xim_protocol_real_status_done               (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid);
static gboolean xim_protocol_real_preeditstate              (XIMProtocol    *proto,
							     guint16         imid,
							     guint16         icid,
							     guint32         mask);


static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (XIMProtocol, xim_protocol, G_TYPE_OBJECT);

/*
 * Private functions
 */
static gboolean
_signal_accumulator(GSignalInvocationHint *hints,
		    GValue                *return_accu,
		    const GValue          *handler_return,
		    gpointer               data)
{
	gboolean ret = g_value_get_boolean(handler_return);

	/* actually the return value doesn't take any effects */
	g_value_set_boolean(return_accu, ret);

	return !ret;
}

static void
xim_protocol_real_forward_raw_packets(XIMProtocol *proto,
				      const gchar *packets,
				      gsize       *length)
{
	GString *p = g_string_sized_new(*length);
	gsize i;

	for (i = 0; i < *length; i++) {
		g_string_append_c(p, packets[i]);
	}
	xim_protocol_send_packets(proto, p);
	*length = 0;
}

static gboolean
xim_protocol_real_connect(XIMProtocol  *proto,
			  guint16       major_version,
			  guint16       minor_version,
			  const GSList *list)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: ->: PROT: XIM_CONNECT [major %d, minor %d]\n",
			comm_window, major_version, minor_version);
	}
	/* No errors should happens here. */
	xim_protocol_send(proto, XIM_CONNECT_REPLY, 0,
			  2,
			  XIM_TYPE_WORD, 2,
			  XIM_TYPE_WORD, 0);

	return TRUE;
}

static gboolean
xim_protocol_real_connect_reply(XIMProtocol *proto,
				guint16      major_version,
				guint16      minor_version)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_CONNECT_REPLY [major %d, minor %d]\n",
			comm_window, major_version, minor_version);
	}
	/* Nothing to do here */

	return TRUE;
}

static gboolean
xim_protocol_real_disconnect(XIMProtocol *proto)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: ->: PROT: XIM_DISCONNECT\n",
			comm_window);
	}
	/* No errors should happens */
	xim_protocol_send(proto, XIM_DISCONNECT_REPLY, 0, 0);

	return TRUE;
}

static gboolean
xim_protocol_real_disconnect_reply(XIMProtocol *proto)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_DISCONNECT_REPLY\n",
			comm_window);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_auth_required(XIMProtocol *proto,
				const gchar *auth_data,
				gsize        length)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_AUTH_REQUIRED: ***FIXME***\n",
			comm_window);
	}

	/* XXX */

	return TRUE;
}

static gboolean
xim_protocol_real_auth_reply(XIMProtocol *proto,
			     const gchar *auth_data,
			     gsize        length)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: ->: PROT: XIM_AUTH_REPLY: ***FIXME***\n",
			comm_window);
	}

	/* XXX */

	return TRUE;
}

static gboolean
xim_protocol_real_auth_next(XIMProtocol *proto,
			    const gchar *auth_data,
			    gsize        length)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_AUTH_NEXT: ***FIXME***\n",
			comm_window);
	}

	/* XXX */

	return TRUE;
}

static gboolean
xim_protocol_real_auth_setup(XIMProtocol  *proto,
			     const GSList *list)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_AUTH_SETUP: ***FIXME***\n",
			comm_window);
	}

	/* XXX */

	return TRUE;
}

static gboolean
xim_protocol_real_auth_ng(XIMProtocol *proto)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_AUTH_NG: ***FIXME***\n",
			comm_window);
	}

	/* XXX */

	return TRUE;
}

static gboolean
xim_protocol_real_error(XIMProtocol *proto,
			guint16      imid,
			guint16      icid,
			guint16      mask,
			guint16      error_code)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <->: PROT: XIM_ERROR: ***FIXME***\n",
			comm_window);
	}

	/* XXX */

	return TRUE;
}

static gboolean
xim_protocol_real_open(XIMProtocol *proto,
		       const gchar *locale)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	static const gchar message[] = "No valid real XIM server available.";
	static size_t len = 0;

	if (len == 0)
		len = strlen(message);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: ->: PROT: XIM_OPEN: [locale: %s]\n",
			comm_window, locale);
	}

	xim_protocol_send(proto, XIM_ERROR, 0,
			  8,
			  XIM_TYPE_WORD, 0,
			  XIM_TYPE_WORD, 0,
			  XIM_TYPE_WORD, 0, /* both imid and icid are invalid */
			  XIM_TYPE_WORD, XIM_ERR_LocaleNotSupported,
			  XIM_TYPE_WORD, len,
			  XIM_TYPE_WORD, 0, /* reserved area */
			  XIM_TYPE_CHAR, message,
			  XIM_TYPE_BYTE, PAD4 (len));

	return TRUE;
}

static gboolean
xim_protocol_real_open_reply(XIMProtocol  *proto,
			     const GSList *imattrs,
			     const GSList *icattrs)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;
		const GSList *l;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_OPEN_REPLY:\n",
			comm_window);
		for (l = imattrs; l != NULL; l = g_slist_next(l)) {
			XIMAttr *a = l->data;

			g_print("	XIMATTR: [id: %d, type: %d, attr: %s]\n",
				a->id, a->type, a->name);
		}
		for (l = icattrs; l != NULL; l = g_slist_next(l)) {
			XICAttr *a = l->data;

			g_print("	XICATTR: [id: %d, type: %d, attr: %s]\n",
				a->id, a->type, a->name);
		}
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_close(XIMProtocol *proto,
			guint16      imid)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	static const gchar message[] = "No valid real XIM server available.";
	static size_t len = 0;

	if (len == 0)
		len = strlen(message);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: ->: PROT: XIM_CLOSE: [imid: %d]\n",
			comm_window, imid);
	}

	xim_protocol_send(proto, XIM_ERROR, 0,
			  7,
			  XIM_TYPE_WORD, imid,
			  XIM_TYPE_WORD, 0,
			  XIM_TYPE_WORD, 1, /* imid is valid */
			  XIM_TYPE_WORD, 999, /* BadSomething */
			  XIM_TYPE_WORD, len,
			  XIM_TYPE_CHAR, message,
			  XIM_TYPE_BYTE, PAD4 (len));

	return TRUE;
}

static gboolean
xim_protocol_real_close_reply(XIMProtocol *proto,
			      guint16      imid)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_CLOSE_REPLY: [imid: %d]\n",
			comm_window, imid);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_register_triggerkeys(XIMProtocol    *proto,
				       guint16         imid,
				       const GSList   *onkeys,
				       const GSList   *offkeys)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_trigger_notify(XIMProtocol    *proto,
				 guint16         imid,
				 guint16         icid,
				 guint32         flag,
				 guint32         index,
				 guint32         mask)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_trigger_notify_reply(XIMProtocol *proto,
				       guint16      imid,
				       guint16      icid)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_CLOSE_REPLY: [imid: %d, icid: %d]\n",
			comm_window, imid, icid);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_set_event_mask(XIMProtocol    *proto,
				 guint16         imid,
				 guint16         icid,
				 guint32         forward_event_mask,
				 guint32         synchronous_event_mask)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_encoding_negotiation(XIMProtocol    *proto,
				       const GSList   *encodings,
				       const GSList   *details)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_encoding_negotiation_reply(XIMProtocol *proto,
					     guint16      imid,
					     guint16      category,
					     gint16       index)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_ENCODING_NEGOTIATION_REPLY: [imid: %d, category: %s, index: %d]\n",
			comm_window, imid, (category == 0 ? "name" : "detailed data"), index);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_query_extension(XIMProtocol    *proto,
				  guint16         imid,
				  const GSList   *extensions)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_query_extension_reply(XIMProtocol  *proto,
					guint16       imid,
					const GSList *extensions)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;
		const GSList *l;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_QUERY_EXTENSION_REPLY: [imid: %d]\n",
			comm_window, imid);
		for (l = extensions; l != NULL; l = g_slist_next(l)) {
			XIMExt *e = l->data;

			g_print("	EXT: [major: %d, minor: %d, name: %s]\n",
				e->major_opcode, e->minor_opcode, e->name->str);
		}
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_set_im_values(XIMProtocol    *proto,
				guint16         imid,
				const GSList   *attributes)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_set_im_values_reply(XIMProtocol *proto,
				      guint16      imid)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_SET_IM_VALUES_REPLY: [imid: %d]\n",
			comm_window, imid);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_get_im_values(XIMProtocol    *proto,
				guint16         imid,
				const GSList   *attr_id)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_get_im_values_reply(XIMProtocol  *proto,
				      guint16       imid,
				      const GSList *attributes)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;
		const GSList *l;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_GET_IM_VALUES_REPLY: [imid: %d]\n",
			comm_window, imid);
		for (l = attributes; l != NULL; l = g_slist_next(l)) {
			XIMAttribute *a = l->data;

			g_print("	XIMATTRIBUTE: [id: %d, name: %s]\n",
				a->id, a->value);
		}
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_create_ic(XIMProtocol    *proto,
			    guint16         imid,
			    const GSList   *attributes)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_create_ic_reply(XIMProtocol *proto,
				  guint16      imid,
				  guint16      icid)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_CREATE_IC_REPLY: [imid: %d, icid: %d]\n",
			comm_window, imid, icid);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_destroy_ic(XIMProtocol    *proto,
			     guint16         imid,
			     guint16         icid)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_destroy_ic_reply(XIMProtocol *proto,
				   guint16      imid,
				   guint16      icid)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_DESTROY_IC_REPLY: [imid: %d, icid: %d]\n",
			comm_window, imid, icid);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_set_ic_values(XIMProtocol    *proto,
				guint16         imid,
				guint16         icid,
				const GSList   *attributes)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_set_ic_values_reply(XIMProtocol *proto,
				      guint16      imid,
				      guint16      icid)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_SET_IC_VALUES_REPLY: [imid: %d, icid: %d]\n",
			comm_window, imid, icid);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_get_ic_values(XIMProtocol    *proto,
				guint16         imid,
				guint16         icid,
				const GSList   *attr_id)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_get_ic_values_reply(XIMProtocol  *proto,
				      guint16       imid,
				      guint16       icid,
				      const GSList *attributes)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;
		const GSList *l;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_GET_IC_VALUES_REPLY: [imid: %d, icid: %d]\n",
			comm_window, imid, icid);
		for (l = attributes; l != NULL; l = g_slist_next(l)) {
			XICAttribute *a = l->data;

			g_print("	XICATTRIBUTE: [id: %d, name: %s]\n",
				a->id, a->value);
		}
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_set_ic_focus(XIMProtocol    *proto,
			       guint16         imid,
			       guint16         icid)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_unset_ic_focus(XIMProtocol    *proto,
				 guint16         imid,
				 guint16         icid)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_forward_event(XIMProtocol    *proto,
				guint16         imid,
				guint16         icid,
				guint16         flag,
				XEvent         *event)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_sync(XIMProtocol    *proto,
		       guint16         imid,
		       guint16         icid)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_sync_reply(XIMProtocol *proto,
			     guint16      imid,
			     guint16      icid)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_SYNC_REPLY: [imid: %d, icid: %d]\n",
			comm_window, imid, icid);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_commit(XIMProtocol    *proto,
			 guint16         imid,
			 guint16         icid,
			 guint16         flag,
			 const gchar    *packets,
			 gsize          *length)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_reset_ic(XIMProtocol    *proto,
			   guint16         imid,
			   guint16         icid)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_reset_ic_reply(XIMProtocol *proto,
				 guint16      imid,
				 guint16      icid,
				 GString     *string)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_RESET_IC_REPLY: [imid: %d, icid: %d, preedit: %s]\n",
			comm_window, imid, icid, string->str);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_geometry(XIMProtocol    *proto,
			   guint16         imid,
			   guint16         icid)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_str_conversion(XIMProtocol    *proto,
				 guint16         imid,
				 guint16         icid,
				 guint16         position,
				 guint32         caret_direction,
				 guint16         factor,
				 guint16         operation,
				 gint16          type_length)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_str_conversion_reply(XIMProtocol    *proto,
				       guint16         imid,
				       guint16         icid,
				       guint32         feedback,
				       XIMStrConvText *text)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_STR_CONVERSION_REPLY: [imid: %d, icid: %d, feedback: %d, text: [feedback: %d, string:%s]]\n",
			comm_window, imid, icid, feedback,
			text->feedback, text->string);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_preedit_start(XIMProtocol    *proto,
				guint16         imid,
				guint16         icid)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_preedit_start_reply(XIMProtocol *proto,
				      guint16      imid,
				      guint16      icid,
				      gint32       return_value)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_PREEDIT_START_REPLY: [imid: %d, icid: %d, return_value: %d]\n",
			comm_window, imid, icid, return_value);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_preedit_draw(XIMProtocol    *proto,
			       guint16         imid,
			       guint16         icid,
			       gint32          caret,
			       gint32          chg_first,
			       gint32          chg_length,
			       guint32         status,
			       const GString  *string,
			       const GSList   *feedbacks)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_preedit_caret(XIMProtocol    *proto,
				guint16         imid,
				guint16         icid,
				gint32          position,
				guint32         direction,
				guint32         style)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_preedit_caret_reply(XIMProtocol *proto,
				      guint16      imid,
				      guint16      icid,
				      guint32      position)
{
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (proto->verbose) {
		Window comm_window;

		g_object_get(priv->conn, "comm_window", &comm_window, NULL);
		g_print("%ld: <-: PROT: XIM_PREEDIT_CARET_REPLY: [imid: %d, icid: %d, position: %d]\n",
			comm_window, imid, icid, position);
	}

	/* Nothing to do */

	return TRUE;
}

static gboolean
xim_protocol_real_preedit_done(XIMProtocol    *proto,
			       guint16         imid,
			       guint16         icid)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_status_start(XIMProtocol    *proto,
			       guint16         imid,
			       guint16         icid)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_status_draw(XIMProtocol    *proto,
			      guint16         imid,
			      guint16         icid,
			      guint32         type,
			      const gchar    *packets,
			      gsize          *length)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_status_done(XIMProtocol    *proto,
			      guint16         imid,
			      guint16         icid)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static gboolean
xim_protocol_real_preeditstate(XIMProtocol    *proto,
			       guint16         imid,
			       guint16         icid,
			       guint32         mask)
{
	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), TRUE);

	return TRUE;
}

static void
xim_protocol_set_property(GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
	XIMProtocol *proto = XIM_PROTOCOL (object);
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	switch (prop_id) {
	    case PROP_DISPLAY:
		    proto->dpy = g_value_get_pointer(value);
		    priv->atoms = xim_get_atoms(proto->dpy);
		    break;
	    case PROP_VERBOSE:
		    proto->verbose = g_value_get_boolean(value);
		    break;
	    case PROP_CONN:
		    priv->conn = g_value_get_object(value);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
xim_protocol_get_property(GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
	XIMProtocol *proto = XIM_PROTOCOL (object);
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (object);

	switch (prop_id) {
	    case PROP_DISPLAY:
		    g_value_set_pointer(value, proto->dpy);
		    break;
	    case PROP_VERBOSE:
		    g_value_set_boolean(value, proto->verbose);
		    break;
	    case PROP_CONN:
		    g_value_set_object(value, priv->conn);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
xim_protocol_finalize(GObject *object)
{
	if (G_OBJECT_CLASS (xim_protocol_parent_class)->finalize)
		G_OBJECT_CLASS (xim_protocol_parent_class)->finalize(object);
}

static void
xim_protocol_class_init(XIMProtocolClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private(klass, sizeof (XIMProtocolPrivate));

	object_class->set_property = xim_protocol_set_property;
	object_class->get_property = xim_protocol_get_property;
	object_class->finalize     = xim_protocol_finalize;

	klass->forward_raw_packets        = xim_protocol_real_forward_raw_packets;
	klass->connect                    = xim_protocol_real_connect;
	klass->connect_reply              = xim_protocol_real_connect_reply;
	klass->disconnect                 = xim_protocol_real_disconnect;
	klass->disconnect_reply           = xim_protocol_real_disconnect_reply;
	klass->auth_required              = xim_protocol_real_auth_required;
	klass->auth_reply                 = xim_protocol_real_auth_reply;
	klass->auth_next                  = xim_protocol_real_auth_next;
	klass->auth_setup                 = xim_protocol_real_auth_setup;
	klass->auth_ng                    = xim_protocol_real_auth_ng;
	klass->error                      = xim_protocol_real_error;
	klass->open                       = xim_protocol_real_open;
	klass->open_reply                 = xim_protocol_real_open_reply;
	klass->close                      = xim_protocol_real_close;
	klass->close_reply                = xim_protocol_real_close_reply;
	klass->register_triggerkeys       = xim_protocol_real_register_triggerkeys;
	klass->trigger_notify             = xim_protocol_real_trigger_notify;
	klass->trigger_notify_reply       = xim_protocol_real_trigger_notify_reply;
	klass->set_event_mask             = xim_protocol_real_set_event_mask;
	klass->encoding_negotiation       = xim_protocol_real_encoding_negotiation;
	klass->encoding_negotiation_reply = xim_protocol_real_encoding_negotiation_reply;
	klass->query_extension            = xim_protocol_real_query_extension;
	klass->query_extension_reply      = xim_protocol_real_query_extension_reply;
	klass->set_im_values              = xim_protocol_real_set_im_values;
	klass->set_im_values_reply        = xim_protocol_real_set_im_values_reply;
	klass->get_im_values              = xim_protocol_real_get_im_values;
	klass->get_im_values_reply        = xim_protocol_real_get_im_values_reply;
	klass->create_ic                  = xim_protocol_real_create_ic;
	klass->create_ic_reply            = xim_protocol_real_create_ic_reply;
	klass->destroy_ic                 = xim_protocol_real_destroy_ic;
	klass->destroy_ic_reply           = xim_protocol_real_destroy_ic_reply;
	klass->set_ic_values              = xim_protocol_real_set_ic_values;
	klass->set_ic_values_reply        = xim_protocol_real_set_ic_values_reply;
	klass->get_ic_values              = xim_protocol_real_get_ic_values;
	klass->get_ic_values_reply        = xim_protocol_real_get_ic_values_reply;
	klass->set_ic_focus               = xim_protocol_real_set_ic_focus;
	klass->unset_ic_focus             = xim_protocol_real_unset_ic_focus;
	klass->forward_event              = xim_protocol_real_forward_event;
	klass->sync                       = xim_protocol_real_sync;
	klass->sync_reply                 = xim_protocol_real_sync_reply;
	klass->commit                     = xim_protocol_real_commit;
	klass->reset_ic                   = xim_protocol_real_reset_ic;
	klass->reset_ic_reply             = xim_protocol_real_reset_ic_reply;
	klass->geometry                   = xim_protocol_real_geometry;
	klass->str_conversion             = xim_protocol_real_str_conversion;
	klass->str_conversion_reply       = xim_protocol_real_str_conversion_reply;
	klass->preedit_start              = xim_protocol_real_preedit_start;
	klass->preedit_start_reply        = xim_protocol_real_preedit_start_reply;
	klass->preedit_draw               = xim_protocol_real_preedit_draw;
	klass->preedit_caret              = xim_protocol_real_preedit_caret;
	klass->preedit_caret_reply        = xim_protocol_real_preedit_caret_reply;
	klass->preedit_done               = xim_protocol_real_preedit_done;
	klass->status_start               = xim_protocol_real_status_start;
	klass->status_draw                = xim_protocol_real_status_draw;
	klass->status_done                = xim_protocol_real_status_done;
	klass->preeditstate               = xim_protocol_real_preeditstate;

	/* properties */
	g_object_class_install_property(object_class, PROP_DISPLAY,
					g_param_spec_pointer("display",
							     _("X display"),
							     _("X display to use"),
							     G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
	g_object_class_install_property(object_class, PROP_VERBOSE,
					g_param_spec_boolean("verbose",
							     _("Verbose flag"),
							     _("Output a lot of helpful messages to debug"),
							     FALSE,
							     G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_CONN,
					g_param_spec_object("connection",
							    _("Connection object"),
							    _("XIMConnection Gobject"),
							    XIM_TYPE_CONNECTION,
							    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/* signals */
	signals[XIM_CONNECT] = g_signal_new("connect",
					    G_OBJECT_CLASS_TYPE (klass),
					    G_SIGNAL_RUN_LAST,
					    G_STRUCT_OFFSET (XIMProtocolClass, connect),
					    _signal_accumulator, NULL,
					    imsettings_marshal_BOOLEAN__UINT_UINT_POINTER,
					    G_TYPE_BOOLEAN, 3,
					    G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_CONNECT_REPLY] = g_signal_new("connect_reply",
						  G_OBJECT_CLASS_TYPE (klass),
						  G_SIGNAL_RUN_LAST,
						  G_STRUCT_OFFSET (XIMProtocolClass, connect_reply),
						  _signal_accumulator, NULL,
						  imsettings_marshal_BOOLEAN__UINT_UINT,
						  G_TYPE_BOOLEAN, 2,
						  G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_DISCONNECT] = g_signal_new("disconnect",
					       G_OBJECT_CLASS_TYPE (klass),
					       G_SIGNAL_RUN_LAST,
					       G_STRUCT_OFFSET (XIMProtocolClass, disconnect),
					       _signal_accumulator, NULL,
					       imsettings_marshal_BOOLEAN__VOID,
					       G_TYPE_BOOLEAN, 0);
	signals[XIM_DISCONNECT_REPLY] = g_signal_new("disconnect_reply",
						     G_OBJECT_CLASS_TYPE (klass),
						     G_SIGNAL_RUN_LAST,
						     G_STRUCT_OFFSET (XIMProtocolClass, disconnect_reply),
						     _signal_accumulator, NULL,
						     imsettings_marshal_BOOLEAN__VOID,
						     G_TYPE_BOOLEAN, 0);
	signals[XIM_AUTH_REQUIRED] = g_signal_new("auth_required",
						  G_OBJECT_CLASS_TYPE (klass),
						  G_SIGNAL_RUN_LAST,
						  G_STRUCT_OFFSET (XIMProtocolClass, auth_required),
						  _signal_accumulator, NULL,
						  imsettings_marshal_BOOLEAN__STRING_UINT,
						  G_TYPE_BOOLEAN, 2,
						  G_TYPE_STRING, G_TYPE_UINT);
	signals[XIM_AUTH_REPLY] = g_signal_new("auth_reply",
					       G_OBJECT_CLASS_TYPE (klass),
					       G_SIGNAL_RUN_LAST,
					       G_STRUCT_OFFSET (XIMProtocolClass, auth_reply),
					       _signal_accumulator, NULL,
					       imsettings_marshal_BOOLEAN__STRING_UINT,
					       G_TYPE_BOOLEAN, 2,
					       G_TYPE_STRING, G_TYPE_UINT);
	signals[XIM_AUTH_SETUP] = g_signal_new("auth_setup",
					       G_OBJECT_CLASS_TYPE (klass),
					       G_SIGNAL_RUN_LAST,
					       G_STRUCT_OFFSET (XIMProtocolClass, auth_setup),
					       _signal_accumulator, NULL,
					       imsettings_marshal_BOOLEAN__POINTER,
					       G_TYPE_BOOLEAN, 1,
					       G_TYPE_POINTER);
	signals[XIM_AUTH_NG] = g_signal_new("auth_ng",
					    G_OBJECT_CLASS_TYPE (klass),
					    G_SIGNAL_RUN_LAST,
					    G_STRUCT_OFFSET (XIMProtocolClass, auth_ng),
					    _signal_accumulator, NULL,
					    imsettings_marshal_BOOLEAN__VOID,
					    G_TYPE_BOOLEAN, 0);
	signals[XIM_ERROR] = g_signal_new("error",
					  G_OBJECT_CLASS_TYPE (klass),
					  G_SIGNAL_RUN_LAST,
					  G_STRUCT_OFFSET (XIMProtocolClass, error),
					  _signal_accumulator, NULL,
					  imsettings_marshal_BOOLEAN__UINT_UINT_UINT_UINT,
					  G_TYPE_BOOLEAN, 4,
					  G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_OPEN] = g_signal_new("open",
					 G_OBJECT_CLASS_TYPE (klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET (XIMProtocolClass, open),
					 _signal_accumulator, NULL,
					 imsettings_marshal_BOOLEAN__STRING,
					 G_TYPE_BOOLEAN, 1,
					 G_TYPE_STRING);
	signals[XIM_OPEN_REPLY] = g_signal_new("open_reply",
					       G_OBJECT_CLASS_TYPE (klass),
					       G_SIGNAL_RUN_LAST,
					       G_STRUCT_OFFSET (XIMProtocolClass, open_reply),
					       _signal_accumulator, NULL,
					       imsettings_marshal_BOOLEAN__POINTER_POINTER,
					       G_TYPE_BOOLEAN, 2,
					       G_TYPE_POINTER, G_TYPE_POINTER);
	signals[XIM_CLOSE] = g_signal_new("close",
					  G_OBJECT_CLASS_TYPE (klass),
					  G_SIGNAL_RUN_LAST,
					  G_STRUCT_OFFSET (XIMProtocolClass, close),
					  _signal_accumulator, NULL,
					  imsettings_marshal_BOOLEAN__UINT,
					  G_TYPE_BOOLEAN, 1,
					  G_TYPE_UINT);
	signals[XIM_CLOSE_REPLY] = g_signal_new("close_reply",
						G_OBJECT_CLASS_TYPE (klass),
						G_SIGNAL_RUN_LAST,
						G_STRUCT_OFFSET (XIMProtocolClass, close_reply),
						_signal_accumulator, NULL,
						imsettings_marshal_BOOLEAN__UINT,
						G_TYPE_BOOLEAN, 1,
						G_TYPE_UINT);
	signals[XIM_REGISTER_TRIGGERKEYS] = g_signal_new("register_triggerkeys",
							 G_OBJECT_CLASS_TYPE (klass),
							 G_SIGNAL_RUN_LAST,
							 G_STRUCT_OFFSET (XIMProtocolClass, register_triggerkeys),
							 _signal_accumulator, NULL,
							 imsettings_marshal_BOOLEAN__UINT_POINTER_POINTER,
							 G_TYPE_BOOLEAN, 3,
							 G_TYPE_UINT, G_TYPE_POINTER, G_TYPE_POINTER);
	signals[XIM_TRIGGER_NOTIFY] = g_signal_new("trigger_notify",
						   G_OBJECT_CLASS_TYPE (klass),
						   G_SIGNAL_RUN_LAST,
						   G_STRUCT_OFFSET (XIMProtocolClass, trigger_notify),
						   _signal_accumulator, NULL,
						   imsettings_marshal_BOOLEAN__UINT_UINT_UINT_UINT_UINT,
						   G_TYPE_BOOLEAN, 5,
						   G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_TRIGGER_NOTIFY_REPLY] = g_signal_new("trigger_notify_reply",
							 G_OBJECT_CLASS_TYPE (klass),
							 G_SIGNAL_RUN_LAST,
							 G_STRUCT_OFFSET (XIMProtocolClass, trigger_notify_reply),
							 _signal_accumulator, NULL,
							 imsettings_marshal_BOOLEAN__UINT_UINT,
							 G_TYPE_BOOLEAN, 2,
							 G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_SET_EVENT_MASK] = g_signal_new("set_event_mask",
						   G_OBJECT_CLASS_TYPE (klass),
						   G_SIGNAL_RUN_LAST,
						   G_STRUCT_OFFSET (XIMProtocolClass, set_event_mask),
						   _signal_accumulator, NULL,
						   imsettings_marshal_BOOLEAN__UINT_UINT_UINT_UINT,
						   G_TYPE_BOOLEAN, 4,
						   G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_ENCODING_NEGOTIATION] = g_signal_new("encoding_negotiation",
							 G_OBJECT_CLASS_TYPE (klass),
							 G_SIGNAL_RUN_LAST,
							 G_STRUCT_OFFSET (XIMProtocolClass, encoding_negotiation),
							 _signal_accumulator, NULL,
							 imsettings_marshal_BOOLEAN__POINTER_POINTER,
							 G_TYPE_BOOLEAN, 2,
							 G_TYPE_POINTER, G_TYPE_POINTER);
	signals[XIM_ENCODING_NEGOTIATION_REPLY] = g_signal_new("encoding_negotiation_reply",
							       G_OBJECT_CLASS_TYPE (klass),
							       G_SIGNAL_RUN_LAST,
							       G_STRUCT_OFFSET (XIMProtocolClass, encoding_negotiation_reply),
							       _signal_accumulator, NULL,
							       imsettings_marshal_BOOLEAN__UINT_UINT_INT,
							       G_TYPE_BOOLEAN, 3,
							       G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INT);
	signals[XIM_QUERY_EXTENSION] = g_signal_new("query_extension",
						    G_OBJECT_CLASS_TYPE (klass),
						    G_SIGNAL_RUN_LAST,
						    G_STRUCT_OFFSET (XIMProtocolClass, query_extension),
						    _signal_accumulator, NULL,
						    imsettings_marshal_BOOLEAN__UINT_POINTER,
						    G_TYPE_BOOLEAN, 2,
						    G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_QUERY_EXTENSION_REPLY] = g_signal_new("query_extension_reply",
							  G_OBJECT_CLASS_TYPE (klass),
							  G_SIGNAL_RUN_LAST,
							  G_STRUCT_OFFSET (XIMProtocolClass, query_extension_reply),
							  _signal_accumulator, NULL,
							  imsettings_marshal_BOOLEAN__UINT_POINTER,
							  G_TYPE_BOOLEAN, 2,
							  G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_SET_IM_VALUES] = g_signal_new("set_im_values",
						  G_OBJECT_CLASS_TYPE (klass),
						  G_SIGNAL_RUN_LAST,
						  G_STRUCT_OFFSET (XIMProtocolClass, set_im_values),
						  _signal_accumulator, NULL,
						  imsettings_marshal_BOOLEAN__UINT_POINTER,
						  G_TYPE_BOOLEAN, 2,
						  G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_SET_IM_VALUES_REPLY] = g_signal_new("set_im_values_reply",
							G_OBJECT_CLASS_TYPE (klass),
							G_SIGNAL_RUN_LAST,
							G_STRUCT_OFFSET (XIMProtocolClass, set_im_values_reply),
							_signal_accumulator, NULL,
							imsettings_marshal_BOOLEAN__UINT,
							G_TYPE_BOOLEAN, 1,
							G_TYPE_UINT);
	signals[XIM_GET_IM_VALUES] = g_signal_new("get_im_values",
						  G_OBJECT_CLASS_TYPE (klass),
						  G_SIGNAL_RUN_LAST,
						  G_STRUCT_OFFSET (XIMProtocolClass, get_im_values),
						  _signal_accumulator, NULL,
						  imsettings_marshal_BOOLEAN__UINT_POINTER,
						  G_TYPE_BOOLEAN, 2,
						  G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_GET_IM_VALUES_REPLY] = g_signal_new("get_im_values_reply",
							G_OBJECT_CLASS_TYPE (klass),
							G_SIGNAL_RUN_LAST,
							G_STRUCT_OFFSET (XIMProtocolClass, get_im_values_reply),
							_signal_accumulator, NULL,
							imsettings_marshal_BOOLEAN__UINT_POINTER,
							G_TYPE_BOOLEAN, 2,
							G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_CREATE_IC] = g_signal_new("create_ic",
					      G_OBJECT_CLASS_TYPE (klass),
					      G_SIGNAL_RUN_LAST,
					      G_STRUCT_OFFSET (XIMProtocolClass, create_ic),
					      _signal_accumulator, NULL,
					      imsettings_marshal_BOOLEAN__UINT_POINTER,
					      G_TYPE_BOOLEAN, 2,
					      G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_CREATE_IC_REPLY] = g_signal_new("create_ic_reply",
						    G_OBJECT_CLASS_TYPE (klass),
						    G_SIGNAL_RUN_LAST,
						    G_STRUCT_OFFSET (XIMProtocolClass, create_ic_reply),
						    _signal_accumulator, NULL,
						    imsettings_marshal_BOOLEAN__UINT_UINT,
						    G_TYPE_BOOLEAN, 2,
						    G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_DESTROY_IC] = g_signal_new("destroy_ic",
					       G_OBJECT_CLASS_TYPE (klass),
					       G_SIGNAL_RUN_LAST,
					       G_STRUCT_OFFSET (XIMProtocolClass, destroy_ic),
					       _signal_accumulator, NULL,
					       imsettings_marshal_BOOLEAN__UINT_UINT,
					       G_TYPE_BOOLEAN, 2,
					       G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_DESTROY_IC_REPLY] = g_signal_new("destroy_ic_reply",
						     G_OBJECT_CLASS_TYPE (klass),
						     G_SIGNAL_RUN_LAST,
						     G_STRUCT_OFFSET (XIMProtocolClass, destroy_ic_reply),
						     _signal_accumulator, NULL,
						     imsettings_marshal_BOOLEAN__UINT_UINT,
						     G_TYPE_BOOLEAN, 2,
						     G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_SET_IC_VALUES] = g_signal_new("set_ic_values",
						  G_OBJECT_CLASS_TYPE (klass),
						  G_SIGNAL_RUN_LAST,
						  G_STRUCT_OFFSET (XIMProtocolClass, set_ic_values),
						  _signal_accumulator, NULL,
						  imsettings_marshal_BOOLEAN__UINT_UINT_POINTER,
						  G_TYPE_BOOLEAN, 3,
						  G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_SET_IC_VALUES_REPLY] = g_signal_new("set_ic_values_reply",
							G_OBJECT_CLASS_TYPE (klass),
							G_SIGNAL_RUN_LAST,
							G_STRUCT_OFFSET (XIMProtocolClass, set_ic_values_reply),
							_signal_accumulator, NULL,
							imsettings_marshal_BOOLEAN__UINT_UINT,
							G_TYPE_BOOLEAN, 2,
							G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_GET_IC_VALUES] = g_signal_new("get_ic_values",
						  G_OBJECT_CLASS_TYPE (klass),
						  G_SIGNAL_RUN_LAST,
						  G_STRUCT_OFFSET (XIMProtocolClass, get_ic_values),
						  _signal_accumulator, NULL,
						  imsettings_marshal_BOOLEAN__UINT_UINT_POINTER,
						  G_TYPE_BOOLEAN, 3,
						  G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_GET_IC_VALUES_REPLY] = g_signal_new("get_ic_values_reply",
							G_OBJECT_CLASS_TYPE (klass),
							G_SIGNAL_RUN_LAST,
							G_STRUCT_OFFSET (XIMProtocolClass, get_ic_values_reply),
							_signal_accumulator, NULL,
							imsettings_marshal_BOOLEAN__UINT_UINT_POINTER,
							G_TYPE_BOOLEAN, 3,
							G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_SET_IC_FOCUS] = g_signal_new("set_ic_focus",
						 G_OBJECT_CLASS_TYPE (klass),
						 G_SIGNAL_RUN_LAST,
						 G_STRUCT_OFFSET (XIMProtocolClass, set_ic_focus),
						 _signal_accumulator, NULL,
						 imsettings_marshal_BOOLEAN__UINT_UINT,
						 G_TYPE_BOOLEAN, 2,
						 G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_UNSET_IC_FOCUS] = g_signal_new("unset_ic_focus",
						   G_OBJECT_CLASS_TYPE (klass),
						   G_SIGNAL_RUN_LAST,
						   G_STRUCT_OFFSET (XIMProtocolClass, unset_ic_focus),
						   _signal_accumulator, NULL,
						   imsettings_marshal_BOOLEAN__UINT_UINT,
						   G_TYPE_BOOLEAN, 2,
						   G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_FORWARD_EVENT] = g_signal_new("forward_event",
						  G_OBJECT_CLASS_TYPE (klass),
						  G_SIGNAL_RUN_LAST,
						  G_STRUCT_OFFSET (XIMProtocolClass, forward_event),
						  _signal_accumulator, NULL,
						  imsettings_marshal_BOOLEAN__UINT_UINT_UINT_POINTER,
						  G_TYPE_BOOLEAN, 4,
						  G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_SYNC] = g_signal_new("sync",
					 G_OBJECT_CLASS_TYPE (klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET (XIMProtocolClass, sync),
					 _signal_accumulator, NULL,
					 imsettings_marshal_BOOLEAN__UINT_UINT,
					 G_TYPE_BOOLEAN, 2,
					 G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_SYNC_REPLY] = g_signal_new("sync_reply",
					       G_OBJECT_CLASS_TYPE (klass),
					       G_SIGNAL_RUN_LAST,
					       G_STRUCT_OFFSET (XIMProtocolClass, sync_reply),
					       _signal_accumulator, NULL,
					       imsettings_marshal_BOOLEAN__UINT_UINT,
					       G_TYPE_BOOLEAN, 2,
					       G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_COMMIT] = g_signal_new("commit",
					   G_OBJECT_CLASS_TYPE (klass),
					   G_SIGNAL_RUN_LAST,
					   G_STRUCT_OFFSET (XIMProtocolClass, commit),
					   _signal_accumulator, NULL,
					   imsettings_marshal_BOOLEAN__UINT_UINT_UINT_POINTER_POINTER,
					   G_TYPE_BOOLEAN, 5,
					   G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER, G_TYPE_POINTER);
	signals[XIM_RESET_IC] = g_signal_new("reset_ic",
					     G_OBJECT_CLASS_TYPE (klass),
					     G_SIGNAL_RUN_LAST,
					     G_STRUCT_OFFSET (XIMProtocolClass, reset_ic),
					     _signal_accumulator, NULL,
					     imsettings_marshal_BOOLEAN__UINT_UINT,
					     G_TYPE_BOOLEAN, 2,
					     G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_RESET_IC_REPLY] = g_signal_new("reset_ic_reply",
						   G_OBJECT_CLASS_TYPE (klass),
						   G_SIGNAL_RUN_LAST,
						   G_STRUCT_OFFSET (XIMProtocolClass, reset_ic_reply),
						   _signal_accumulator, NULL,
						   imsettings_marshal_BOOLEAN__UINT_UINT_POINTER,
						   G_TYPE_BOOLEAN, 3,
						   G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_GEOMETRY] = g_signal_new("geometry",
					     G_OBJECT_CLASS_TYPE (klass),
					     G_SIGNAL_RUN_LAST,
					     G_STRUCT_OFFSET (XIMProtocolClass, geometry),
					     _signal_accumulator, NULL,
					     imsettings_marshal_BOOLEAN__UINT_UINT,
					     G_TYPE_BOOLEAN, 2,
					     G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_STR_CONVERSION] = g_signal_new("str_conversion",
						   G_OBJECT_CLASS_TYPE (klass),
						   G_SIGNAL_RUN_LAST,
						   G_STRUCT_OFFSET (XIMProtocolClass, str_conversion),
						   _signal_accumulator, NULL,
						   imsettings_marshal_BOOLEAN__UINT_UINT_UINT_UINT_UINT_UINT_INT,
						   G_TYPE_BOOLEAN, 7,
						   G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INT);
	signals[XIM_STR_CONVERSION_REPLY] = g_signal_new("str_conversion_reply",
							 G_OBJECT_CLASS_TYPE (klass),
							 G_SIGNAL_RUN_LAST,
							 G_STRUCT_OFFSET (XIMProtocolClass, str_conversion_reply),
							 _signal_accumulator, NULL,
							 imsettings_marshal_BOOLEAN__UINT_UINT_UINT_POINTER,
							 G_TYPE_BOOLEAN, 4,
							 G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER);
	signals[XIM_PREEDIT_START] = g_signal_new("preedit_start",
						  G_OBJECT_CLASS_TYPE (klass),
						  G_SIGNAL_RUN_LAST,
						  G_STRUCT_OFFSET (XIMProtocolClass, preedit_start),
						  _signal_accumulator, NULL,
						  imsettings_marshal_BOOLEAN__UINT_UINT,
						  G_TYPE_BOOLEAN, 2,
						  G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_PREEDIT_START_REPLY] = g_signal_new("preedit_start_reply",
							G_OBJECT_CLASS_TYPE (klass),
							G_SIGNAL_RUN_LAST,
							G_STRUCT_OFFSET (XIMProtocolClass, preedit_start_reply),
							_signal_accumulator, NULL,
							imsettings_marshal_BOOLEAN__UINT_UINT_INT,
							G_TYPE_BOOLEAN, 3,
							G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INT);
	signals[XIM_PREEDIT_DRAW] = g_signal_new("preedit_draw",
						 G_OBJECT_CLASS_TYPE (klass),
						 G_SIGNAL_RUN_LAST,
						 G_STRUCT_OFFSET (XIMProtocolClass, preedit_draw),
						 _signal_accumulator, NULL,
						 imsettings_marshal_BOOLEAN__UINT_UINT_INT_INT_INT_UINT_POINTER_POINTER,
						 G_TYPE_BOOLEAN, 8,
						 G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_UINT, G_TYPE_POINTER, G_TYPE_POINTER);
	signals[XIM_PREEDIT_CARET] = g_signal_new("preedit_caret",
						  G_OBJECT_CLASS_TYPE (klass),
						  G_SIGNAL_RUN_LAST,
						  G_STRUCT_OFFSET (XIMProtocolClass, preedit_caret),
						  _signal_accumulator, NULL,
						  imsettings_marshal_BOOLEAN__UINT_UINT_INT_UINT_UINT,
						  G_TYPE_BOOLEAN, 5,
						  G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INT, G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_PREEDIT_CARET_REPLY] = g_signal_new("preedit_caret_reply",
							G_OBJECT_CLASS_TYPE (klass),
							G_SIGNAL_RUN_LAST,
							G_STRUCT_OFFSET (XIMProtocolClass, preedit_caret_reply),
							_signal_accumulator, NULL,
							imsettings_marshal_BOOLEAN__UINT_UINT_UINT,
							G_TYPE_BOOLEAN, 3,
							G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_PREEDIT_DONE] = g_signal_new("preedit_done",
						 G_OBJECT_CLASS_TYPE (klass),
						 G_SIGNAL_RUN_LAST,
						 G_STRUCT_OFFSET (XIMProtocolClass, preedit_done),
						 _signal_accumulator, NULL,
						 imsettings_marshal_BOOLEAN__UINT_UINT,
						 G_TYPE_BOOLEAN, 2,
						 G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_STATUS_START] = g_signal_new("status_start",
						 G_OBJECT_CLASS_TYPE (klass),
						 G_SIGNAL_RUN_LAST,
						 G_STRUCT_OFFSET (XIMProtocolClass, status_start),
						 _signal_accumulator, NULL,
						 imsettings_marshal_BOOLEAN__UINT_UINT,
						 G_TYPE_BOOLEAN, 2,
						 G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_STATUS_DRAW] = g_signal_new("status_draw",
						G_OBJECT_CLASS_TYPE (klass),
						G_SIGNAL_RUN_LAST,
						G_STRUCT_OFFSET (XIMProtocolClass, status_draw),
						_signal_accumulator, NULL,
						imsettings_marshal_BOOLEAN__UINT_UINT_UINT_POINTER_POINTER,
						G_TYPE_BOOLEAN, 5,
						G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_POINTER, G_TYPE_POINTER);
	signals[XIM_STATUS_DONE] = g_signal_new("status_done",
						G_OBJECT_CLASS_TYPE (klass),
						G_SIGNAL_RUN_LAST,
						G_STRUCT_OFFSET (XIMProtocolClass, status_done),
						_signal_accumulator, NULL,
						imsettings_marshal_BOOLEAN__UINT_UINT,
						G_TYPE_BOOLEAN, 2,
						G_TYPE_UINT, G_TYPE_UINT);
	signals[XIM_PREEDITSTATE] = g_signal_new("preeditstate",
						 G_OBJECT_CLASS_TYPE (klass),
						 G_SIGNAL_RUN_LAST,
						 G_STRUCT_OFFSET (XIMProtocolClass, preeditstate),
						 _signal_accumulator, NULL,
						 imsettings_marshal_BOOLEAN__UINT_UINT_UINT,
						 G_TYPE_BOOLEAN, 3,
						 G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
}

static void
xim_protocol_init(XIMProtocol *proto)
{
	gint i = 1;
	gchar *p = (gchar *)&i;
	XIMProtocolPrivate *priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	if (*p == 1)
		priv->host_byte_order = XIM_ORDER_LSB;
	else
		priv->host_byte_order = XIM_ORDER_MSB;
}

/*
 * Public functions
 */
XIMProtocol *
xim_protocol_new(const Display       *dpy,
		 const XIMConnection *conn)
{
	g_return_val_if_fail (dpy != NULL, NULL);
	g_return_val_if_fail (conn != NULL, NULL);

	return XIM_PROTOCOL (g_object_new(XIM_TYPE_PROTOCOL,
					  "display", dpy,
					  "connection", conn,
					  NULL));
}

void
xim_protocol_process_packets(XIMProtocol *proto,
			     const gchar *packets,
			     gsize       *length)
{
	g_return_if_fail (XIM_IS_PROTOCOL (proto));
	g_return_if_fail (packets != NULL);
	g_return_if_fail (length != NULL);

	XIM_PROTOCOL_GET_CLASS (proto)->forward_raw_packets (proto, packets, length);
}

void
xim_protocol_process_event(XIMProtocol *proto,
			   const gchar *packets,
			   gsize       *length)
{
	XIMProtocolClass *klass;
	XIMProtocolPrivate *priv;
	gsize len = *length;
	gboolean ret = FALSE;
	guint8 major_opcode, minor_opcode;
	guint16 packlen;

	g_return_if_fail (XIM_IS_PROTOCOL (proto));
	g_return_if_fail (packets != NULL);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	klass = XIM_PROTOCOL_GET_CLASS (proto);

	/* no need to rely on the byte order here */
	major_opcode = packets[0];
	minor_opcode = packets[1];
	if (major_opcode == XIM_CONNECT)
		priv->byte_order = packets[4];
	len -= 2;
	packlen = xim_protocol_get_card16(proto, &packets[*length - len], &len);

	switch (major_opcode) {
	    case XIM_CONNECT:
		    G_STMT_START {
			    guint16 major, minor, nitems, i;
			    GSList *list = NULL, *ll;

			    /* byte order + unused area */
			    len -= 2;

			    major = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    minor = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nitems = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    for (i = 0; i < nitems; i++) {
				    GString *buf;

				    buf = xim_protocol_get_string(proto, &packets[*length - len], &len);
				    list = g_slist_append(list, buf);
			    }

			    g_signal_emit(proto, signals[XIM_CONNECT], 0, major, minor, list, &ret);

			    for (ll = list; ll != NULL;) {
				    g_string_free(ll->data, TRUE);
				    ll = g_slist_delete_link(ll, ll);
			    }
		    } G_STMT_END;
		    break;
	    case XIM_CONNECT_REPLY:
		    G_STMT_START {
			    guint16 major, minor;

			    major = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    minor = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_CONNECT_REPLY], 0, major, minor, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_DISCONNECT:
		    g_signal_emit(proto, signals[XIM_DISCONNECT], 0, &ret);
		    break;
	    case XIM_DISCONNECT_REPLY:
		    g_signal_emit(proto, signals[XIM_DISCONNECT_REPLY], 0, &ret);
		    break;
	    case XIM_AUTH_REQUIRED:
		    G_STMT_START {
			    guint8 index;
			    guint16 l, i;
			    gchar *buf;

			    index = xim_protocol_get_card8(proto, &packets[*length - len], &len);
			    /* unused */
			    len -= 3;
			    l = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    len -= 2;
			    buf = g_new(gchar, len + 1);
			    for (i = 0; i < l; i++) {
				    buf[i] = xim_protocol_get_card8(proto, &packets[*length - len], &len);
			    }
			    len -= PAD4 (l);

			    g_signal_emit(proto, signals[XIM_AUTH_REQUIRED], 0, buf, l, &ret);

			    g_free(buf);
		    } G_STMT_END;
		    break;
	    case XIM_AUTH_REPLY:
		    G_STMT_START {
			    guint16 l, i;
			    gchar *buf;

			    l = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    len -= 2;
			    /* Why the length-of-authentication-data appears twice? */
			    len -= 4;
			    buf = g_new(gchar, l + 1);
			    for (i = 0; i < l; i++) {
				    buf[i] = xim_protocol_get_card8(proto, &packets[*length - len], &len);
			    }
			    len -= PAD4 (l);

			    g_signal_emit(proto, signals[XIM_AUTH_REPLY], 0, buf, l, &ret);

			    g_free(buf);
		    } G_STMT_END;
		    break;
	    case XIM_AUTH_NEXT:
		    G_STMT_START {
			    guint16 l, i;
			    gchar *buf;

			    l = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    len -= 2;
			    buf = g_new(gchar, l + 1);
			    for (i = 0; i < l; i++) {
				    buf[i] = xim_protocol_get_card8(proto, &packets[*length - len], &len);
			    }
			    len -= PAD4 (l);

			    g_signal_emit(proto, signals[XIM_AUTH_NEXT], 0, buf, l, &ret);

			    g_free(buf);
		    } G_STMT_END;
		    break;
	    case XIM_AUTH_SETUP:
		    G_STMT_START {
			    guint16 nitems, i;
			    GSList *list = NULL, *ll;

			    nitems = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    len -= 2;
			    for (i = 0; i < nitems; i++) {
				    GString *buf;

				    buf = xim_protocol_get_string(proto, &packets[*length - len], &len);
				    list = g_slist_append(list, buf);
			    }

			    g_signal_emit(proto, signals[XIM_AUTH_SETUP], 0, list, &ret);

			    for (ll = list; ll != NULL;) {
				    g_string_free(ll->data, TRUE);
				    ll = g_slist_delete_link(ll, ll);
			    }
		    } G_STMT_END;
		    break;
	    case XIM_AUTH_NG:
		    g_signal_emit(proto, signals[XIM_AUTH_NG], 0);
		    break;
	    case XIM_ERROR:
		    G_STMT_START {
			    guint16 imid, icid, mask, error;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    mask = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    error = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_ERROR], 0, imid, icid, mask, error, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_OPEN:
		    G_STMT_START {
			    GString *locale;
			    gsize t = len;

			    locale = xim_protocol_get_str(proto, &packets[*length - len], &len);
			    /* padding unused */
			    len -= PAD4 (t - len);
			    g_signal_emit(proto, signals[XIM_OPEN], 0, locale->str, &ret);
			    g_string_free(locale, TRUE);
		    } G_STMT_END;
		    break;
	    case XIM_OPEN_REPLY:
		    G_STMT_START {
			    guint16 imid, nimattrs, nicattrs, i;
			    XIMAttr *imattr;
			    XICAttr *icattr;
			    GSList *lim = NULL, *lic = NULL, *ll;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nimattrs = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    for (i = nimattrs; i > 0;) {
				    gsize t = len;

				    imattr = xim_protocol_get_ximattr(proto, &packets[*length - len], &len);
				    lim = g_slist_append(lim, imattr);
				    i -= (t - len);
			    }
			    nicattrs = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    /* padding unused */
			    len -= 2;
			    for (i = nicattrs; i > 0;) {
				    gsize t = len;

				    icattr = xim_protocol_get_xicattr(proto, &packets[*length - len], &len);
				    lic = g_slist_append(lic, icattr);
				    i -= (t - len);
			    }

			    g_signal_emit(proto, signals[XIM_OPEN_REPLY], 0, lim, lic, &ret);

			    for (ll = lim; ll != NULL;) {
				    xim_ximattr_free(ll->data);
				    ll = g_slist_delete_link(ll, ll);
			    }
			    for (ll = lic; ll != NULL;) {
				    xim_xicattr_free(ll->data);
				    ll = g_slist_delete_link(ll, ll);
			    }
		    } G_STMT_END;
		    break;
	    case XIM_CLOSE:
		    G_STMT_START {
			    guint16 imid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    len -= 2;

			    g_signal_emit(proto, signals[XIM_CLOSE], 0, imid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_CLOSE_REPLY:
		    G_STMT_START {
			    guint16 imid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    len -= 2;

			    g_signal_emit(proto, signals[XIM_CLOSE_REPLY], 0, imid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_REGISTER_TRIGGERKEYS:
		    G_STMT_START {
			    guint16 imid;
			    guint32 nonkeys, noffkeys, i;
			    XIMTriggerKey *key;
			    GSList *lonkeys = NULL, *loffkeys = NULL, *ll;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    /* padding unused */
			    len -= 2;
			    nonkeys = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    for (i = nonkeys; i > 0;) {
				    gsize t = len;

				    key = xim_protocol_get_ximtriggerkey(proto, &packets[*length - len], &len);
				    lonkeys = g_slist_append(lonkeys, key);
				    i -= (t - len);
			    }
			    noffkeys = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    for (i = noffkeys; i > 0;) {
				    gsize t = len;

				    key = xim_protocol_get_ximtriggerkey(proto, &packets[*length - len], &len);
				    loffkeys = g_slist_append(loffkeys, key);
				    i -= (t - len);
			    }

			    g_signal_emit(proto, signals[XIM_REGISTER_TRIGGERKEYS], 0, imid, lonkeys, loffkeys, &ret);

			    for (ll = lonkeys; ll != NULL;) {
				    g_free(ll->data);
				    ll = g_slist_delete_link(ll, ll);
			    }
			    for (ll = loffkeys; ll != NULL;) {
				    g_free(ll->data);
				    ll = g_slist_delete_link(ll, ll);
			    }
		    } G_STMT_END;
		    break;
	    case XIM_TRIGGER_NOTIFY:
		    G_STMT_START {
			    guint16 imid, icid;
			    guint32 flag, index, mask;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    flag = xim_protocol_get_card32(proto, &packets[*length - len], &len);
			    index = xim_protocol_get_card32(proto, &packets[*length - len], &len);
			    mask = xim_protocol_get_card32(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_TRIGGER_NOTIFY], 0, imid, icid, flag, index, mask, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_TRIGGER_NOTIFY_REPLY:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_TRIGGER_NOTIFY_REPLY], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_SET_EVENT_MASK:
		    G_STMT_START {
			    guint16 imid, icid;
			    guint32 fmask, smask;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    fmask = xim_protocol_get_card32(proto, &packets[*length - len], &len);
			    smask = xim_protocol_get_card32(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_SET_EVENT_MASK], 0, imid, icid, fmask, smask, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_ENCODING_NEGOTIATION:
		    G_STMT_START {
			    guint16 imid, nname, ndata, i;
			    GSList *lenc = NULL, *ldata = NULL, *ll;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nname = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    for (i = nname; i > 0;) {
				    gsize t = len;
				    GString *str = xim_protocol_get_str(proto, &packets[*length - len], &len);

				    lenc = g_slist_append(lenc, str);
				    i -= (t - len);
			    }
			    len -= PAD4 (nname);
			    ndata = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    /* padding unused */
			    len -= 2;
			    for (i = ndata; i > 0;) {
				    gsize t = len;
				    GString *str = xim_protocol_get_encodinginfo(proto, &packets[*length - len], &len);

				    ldata = g_slist_append(ldata, str);
				    i -= (t - len);
			    }

			    g_signal_emit(proto, signals[XIM_ENCODING_NEGOTIATION], 0, lenc, ldata, &ret);

			    for (ll = lenc; ll != NULL;) {
				    g_string_free(ll->data, TRUE);
				    ll = g_slist_delete_link(ll, ll);
			    }
			    for (ll = ldata; ll != NULL;) {
				    g_string_free(ll->data, TRUE);
				    ll = g_slist_delete_link(ll, ll);
			    }
		    } G_STMT_END;
		    break;
	    case XIM_ENCODING_NEGOTIATION_REPLY:
		    G_STMT_START {
			    guint16 imid, category;
			    gint16  index;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    category = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    index = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    /* padding unused */
			    len -= 2;

			    g_signal_emit(proto, signals[XIM_ENCODING_NEGOTIATION_REPLY], 0, imid, category, index, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_QUERY_EXTENSION:
		    G_STMT_START {
			    guint16 imid, nbytes, i;
			    GSList *lext = NULL, *ll;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nbytes = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    for (i = nbytes; i > 0;) {
				    gsize t = len;
				    GString *str = xim_protocol_get_str(proto, &packets[*length - len], &len);

				    lext = g_slist_append(lext, str);
				    i -= (t - len);
			    }
			    /* padding unused */
			    len -= PAD4 (nbytes);

			    g_signal_emit(proto, signals[XIM_QUERY_EXTENSION], 0, imid, lext, &ret);

			    for (ll = lext; ll != NULL;) {
				    g_string_free(ll->data, TRUE);
				    ll = g_slist_delete_link(ll, ll);
			    }
		    } G_STMT_END;
		    break;
	    case XIM_QUERY_EXTENSION_REPLY:
		    G_STMT_START {
			    guint16 imid, nbytes, i;
			    GSList *lext = NULL, *ll;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nbytes = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    for (i = nbytes; i > 0;) {
				    gsize t = len;
				    XIMExt *e;

				    e = xim_protocol_get_ext(proto, &packets[*length - len], &len);
				    lext = g_slist_append(lext, e);
				    i -= (t - len);
			    }

			    g_signal_emit(proto, signals[XIM_QUERY_EXTENSION_REPLY], 0, imid, lext, &ret);

			    for (ll = lext; ll != NULL;) {
				    xim_ext_free(ll->data);
				    ll = g_slist_delete_link(ll, ll);
			    }
		    } G_STMT_END;
		    break;
	    case XIM_SET_IM_VALUES:
		    G_STMT_START {
			    guint16 imid, nbytes, i;
			    GSList *lattrs = NULL, *ll;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nbytes = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    for (i = nbytes; i > 0;) {
				    gsize t = len;
				    XIMAttribute *a = xim_protocol_get_ximattribute(proto,
										    &packets[*length - len],
										    &len);

				    lattrs = g_slist_append(lattrs, a);
				    i -= (t - len);
			    }

			    g_signal_emit(proto, signals[XIM_SET_IM_VALUES], 0, imid, lattrs, &ret);

			    for (ll = lattrs; ll != NULL;) {
				    xim_ximattribute_free(ll->data);
				    ll = g_slist_delete_link(ll, ll);
			    }
		    } G_STMT_END;
		    break;
	    case XIM_SET_IM_VALUES_REPLY:
		    G_STMT_START {
			    guint16 imid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    len -= 2;

			    g_signal_emit(proto, signals[XIM_SET_IM_VALUES_REPLY], 0, imid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_GET_IM_VALUES:
		    G_STMT_START {
			    guint16 imid, nbytes, i;
			    GSList *lattrid = NULL;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nbytes = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    for (i = nbytes; i > 0;) {
				    gsize t = len;
				    guint16 id = xim_protocol_get_card16(proto, &packets[*length - len], &len);

				    lattrid = g_slist_append(lattrid, GUINT_TO_POINTER ((guint32)id));
				    i -= (t - len);
			    }
			    len -= PAD4 (nbytes);

			    g_signal_emit(proto, signals[XIM_GET_IM_VALUES], 0, imid, lattrid, &ret);

			    g_slist_free(lattrid);
		    } G_STMT_END;
		    break;
	    case XIM_GET_IM_VALUES_REPLY:
		    G_STMT_START {
			    guint16 imid, nbytes, i;
			    GSList *lattrs = NULL, *ll;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nbytes = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    for (i = nbytes; i > 0;) {
				    gsize t = len;
				    XIMAttribute *a = xim_protocol_get_ximattribute(proto,
										    &packets[*length - len],
										    &len);

				    lattrs = g_slist_append(lattrs, a);
				    i -= (t - len);
			    }

			    g_signal_emit(proto, signals[XIM_GET_IM_VALUES_REPLY], 0, imid, lattrs, &ret);

			    for (ll = lattrs; ll != NULL;) {
				    xim_ximattribute_free(ll->data);
				    ll = g_slist_delete_link(ll, ll);
			    }
		    } G_STMT_END;
		    break;
	    case XIM_CREATE_IC:
		    G_STMT_START {
			    guint16 imid, nbytes, i;
			    GSList *lattrs = NULL, *ll;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nbytes = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    for (i = nbytes; i > 0;) {
				    XICAttribute *a = xim_protocol_get_xicattribute(proto,
										    &packets[*length - len],
										    &len);

				    lattrs = g_slist_append(lattrs, a);
			    }

			    g_signal_emit(proto, signals[XIM_CREATE_IC], 0, imid, lattrs, &ret);

			    for (ll = lattrs; ll != NULL;) {
				    xim_xicattribute_free(ll->data);
				    ll = g_slist_delete_link(ll, ll);
			    }
		    } G_STMT_END;
		    break;
	    case XIM_CREATE_IC_REPLY:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_CREATE_IC_REPLY], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_DESTROY_IC:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_DESTROY_IC], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_DESTROY_IC_REPLY:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_DESTROY_IC_REPLY], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_SET_IC_VALUES:
		    G_STMT_START {
			    guint16 imid, icid, nbytes, i;
			    GSList *lattrs = NULL, *ll;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nbytes = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    /* padding unused */
			    len -= 2;
			    for (i = nbytes; i > 0;) {
				    gsize t = len;
				    XICAttribute *a = xim_protocol_get_xicattribute(proto,
										    &packets[*length - len],
										    &len);

				    lattrs = g_slist_append(lattrs, a);
				    i -= (t - len);
			    }

			    g_signal_emit(proto, signals[XIM_SET_IC_VALUES], 0, imid, icid, lattrs, &ret);

			    for (ll = lattrs; ll != NULL;) {
				    xim_xicattribute_free(ll->data);
				    ll = g_slist_delete_link(ll, ll);
			    }
		    } G_STMT_END;
		    break;
	    case XIM_SET_IC_VALUES_REPLY:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_SET_IC_VALUES_REPLY], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_GET_IC_VALUES:
		    G_STMT_START {
			    guint16 imid, icid, nbytes, i;
			    GSList *lattrid = NULL;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nbytes = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    for (i = nbytes; i > 0;) {
				    gsize t = len;
				    guint16 id = xim_protocol_get_card16(proto, &packets[*length - len], &len);

				    lattrid = g_slist_append(lattrid, GUINT_TO_POINTER ((guint32)id));
				    i -= (t - len);
			    }
			    len -= PAD4 (2 + nbytes);

			    g_signal_emit(proto, signals[XIM_GET_IC_VALUES], 0, imid, icid, lattrid, &ret);

			    g_slist_free(lattrid);
		    } G_STMT_END;
		    break;
	    case XIM_GET_IC_VALUES_REPLY:
		    G_STMT_START {
			    guint16 imid, icid, nbytes, i;
			    GSList *lattrs = NULL, *ll;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nbytes = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    /* padding unused */
			    len -= 2;
			    for (i = nbytes; i > 0;) {
				    gsize t = len;
				    XICAttribute *a = xim_protocol_get_xicattribute(proto,
										    &packets[*length - len],
										    &len);

				    lattrs = g_slist_append(lattrs, a);
				    i -= (t - len);
			    }

			    g_signal_emit(proto, signals[XIM_GET_IC_VALUES_REPLY], 0, imid, icid, lattrs, &ret);

			    for (ll = lattrs; ll != NULL;) {
				    xim_xicattribute_free(ll->data);
				    ll = g_slist_delete_link(ll, ll);
			    }
		    } G_STMT_END;
		    break;
	    case XIM_SET_IC_FOCUS:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_SET_IC_FOCUS], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_UNSET_IC_FOCUS:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_UNSET_IC_FOCUS], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_FORWARD_EVENT:
		    G_STMT_START {
			    guint16 imid, icid, flag, serial;
			    XEvent *e;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    flag = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    serial = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    e = xim_protocol_get_xevent(proto, &packets[*length - len], &len);
			    e->xany.serial |= (serial << 16);

			    g_signal_emit(proto, signals[XIM_FORWARD_EVENT], 0, imid, icid, flag, e, &ret);

			    g_free(e);
		    } G_STMT_END;
		    break;
	    case XIM_SYNC:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_SYNC], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_SYNC_REPLY:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_SYNC_REPLY], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_COMMIT:
		    G_STMT_START {
			    guint16 imid, icid, flag;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    flag = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_COMMIT], 0, imid, icid, flag, &packets[*length - len], &len, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_RESET_IC:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_RESET_IC], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_RESET_IC_REPLY:
		    G_STMT_START {
			    guint16 imid, icid, nbytes, i;
			    GString *str;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    nbytes = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    str = g_string_sized_new(nbytes);
			    for (i = 0; i < nbytes; i++) {
				    guint8 c = xim_protocol_get_card8(proto, &packets[*length - len], &len);

				    g_string_append_c(str, c);
			    }
			    len -= PAD4 (2 + nbytes);

			    g_signal_emit(proto, signals[XIM_RESET_IC_REPLY], 0, imid, icid, str, &ret);

			    g_string_free(str, TRUE);
		    } G_STMT_END;
		    break;
	    case XIM_GEOMETRY:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_GEOMETRY], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_STR_CONVERSION:
		    G_STMT_START {
			    guint16 imid, icid, pos, factor, op;
			    gint16 l;
			    guint32 direction;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    pos = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    /* padding unused */
			    len -= 2;
			    direction = xim_protocol_get_card32(proto, &packets[*length - len], &len);
			    factor = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    op = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    l = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_STR_CONVERSION], 0, imid, icid, pos, direction, factor, op, l, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_STR_CONVERSION_REPLY:
		    G_STMT_START {
			    guint16 imid, icid;
			    guint32 feedback;
			    XIMStrConvText *text;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    feedback = xim_protocol_get_card32(proto, &packets[*length - len], &len);
			    text = xim_protocol_get_strconvtext(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_STR_CONVERSION_REPLY], 0, imid, icid, feedback, text, &ret);

			    xim_strconvtext_free(text);
		    } G_STMT_END;
		    break;
	    case XIM_PREEDIT_START:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_PREEDIT_START], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_PREEDIT_START_REPLY:
		    G_STMT_START {
			    guint16 imid, icid;
			    gint32 return_value;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    return_value = xim_protocol_get_card32(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_PREEDIT_START_REPLY], 0, imid, icid, return_value, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_PREEDIT_DRAW:
		    G_STMT_START {
			    guint16 imid, icid, l, nbytes, i;
			    gint32 caret, chg_first, chg_length;
			    guint32 mask;
			    GString *string;
			    GSList *lfeedback = NULL;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    caret = xim_protocol_get_card32(proto, &packets[*length - len], &len);
			    chg_first = xim_protocol_get_card32(proto, &packets[*length - len], &len);
			    chg_length = xim_protocol_get_card32(proto, &packets[*length - len], &len);
			    mask = xim_protocol_get_card32(proto, &packets[*length - len], &len);
			    l = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    string = g_string_sized_new(l);
			    for (i = 0; i < l; i++) {
				    guint8 c = xim_protocol_get_card8(proto, &packets[*length - len], &len);

				    g_string_append_c(string, c);
			    }
			    len -= PAD4 (2 + l);
			    nbytes = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    /* padding unused */
			    len -= 2;
			    for (i = 0; i < nbytes; i += 4) {
				    guint32 feedback;

				    feedback = xim_protocol_get_card32(proto, &packets[*length - len], &len);
				    lfeedback = g_slist_append(lfeedback, GUINT_TO_POINTER (feedback));
			    }

			    g_signal_emit(proto, signals[XIM_PREEDIT_DRAW], 0, imid, icid, caret, chg_first, chg_length, mask, string, lfeedback, &ret);

			    g_string_free(string, TRUE);
			    g_slist_free(lfeedback);
		    } G_STMT_END;
		    break;
	    case XIM_PREEDIT_CARET:
		    G_STMT_START {
			    guint16 imid, icid;
			    gint32 position;
			    guint32 direction, style;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    position = xim_protocol_get_card32(proto, &packets[*length - len], &len);
			    direction = xim_protocol_get_card32(proto, &packets[*length - len], &len);
			    style = xim_protocol_get_card32(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_PREEDIT_CARET], 0, imid, icid, position, direction, style, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_PREEDIT_CARET_REPLY:
		    G_STMT_START {
			    guint16 imid, icid;
			    guint32 position;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    position = xim_protocol_get_card32(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_PREEDIT_CARET_REPLY], 0, imid, icid, position, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_PREEDIT_DONE:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_PREEDIT_DONE], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_STATUS_START:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_STATUS_START], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_STATUS_DRAW:
		    G_STMT_START {
			    guint16 imid, icid;
			    guint32 type;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    type = xim_protocol_get_card32(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_STATUS_DRAW], 0, imid, icid, type, &packets[*length - len], &len, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_STATUS_DONE:
		    G_STMT_START {
			    guint16 imid, icid;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_STATUS_DONE], 0, imid, icid, &ret);
		    } G_STMT_END;
		    break;
	    case XIM_PREEDITSTATE:
		    G_STMT_START {
			    guint16 imid, icid;
			    guint32 mask;

			    imid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    icid = xim_protocol_get_card16(proto, &packets[*length - len], &len);
			    mask = xim_protocol_get_card32(proto, &packets[*length - len], &len);

			    g_signal_emit(proto, signals[XIM_PREEDITSTATE], 0, imid, icid, mask, &ret);
		    } G_STMT_END;
		    break;
	    default:
		    g_warning("Unknown packets received on XIM protocol: major opcode: %02X, minor opcode: %02X, length: %d", major_opcode, minor_opcode, packlen);
		    break;
	}
	if (ret)
		*length = len;
}

guint8
xim_protocol_get_card8(XIMProtocol  *proto,
		       const gchar  *packets,
		       gsize        *length)
{
	XIMProtocolPrivate *priv;
	guint8 retval = 0;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), 0);
	g_return_val_if_fail (packets != NULL, 0);
	g_return_val_if_fail (length != NULL, 0);
	g_return_val_if_fail (*length > 0, 0);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	if (priv->byte_order == XIM_ORDER_MSB) {
		retval = packets[0];
		(*length)--;
	} else if (priv->byte_order == XIM_ORDER_LSB) {
		retval = packets[0];
		(*length)--;
	} else {
		g_warning("Failed to get CARD8 due to the unknown byte order.");
	}

	return retval;
}

void
xim_protocol_put_card8(XIMProtocol *proto,
		       GString     *packets,
		       guint8       data)
{
	XIMProtocolPrivate *priv;

	g_return_if_fail (XIM_IS_PROTOCOL (proto));
	g_return_if_fail (packets != NULL);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	if (priv->byte_order == XIM_ORDER_MSB) {
		g_string_append_c(packets, data);
	} else if (priv->byte_order == XIM_ORDER_LSB) {
		g_string_append_c(packets, data);
	} else {
		g_warning("Failed to put CARD8 due to the unknown byte order.");
	}
}

guint16
xim_protocol_get_card16(XIMProtocol  *proto,
			const gchar  *packets,
			gsize        *length)
{
	XIMProtocolPrivate *priv;
	guint16 retval = 0;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), 0);
	g_return_val_if_fail (packets != NULL, 0);
	g_return_val_if_fail (length != NULL, 0);
	g_return_val_if_fail (*length > 1, 0);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	if (priv->byte_order == XIM_ORDER_MSB) {
		retval = (packets[0] << 8) + packets[1];
		*length -= 2;
	} else if (priv->byte_order == XIM_ORDER_LSB) {
		retval = (packets[1] << 8) + packets[0];
		*length -= 2;
	} else {
		g_warning("Failed to get CARD16 due to the unknown byte order.");
	}

	return retval;
}

void
xim_protocol_put_card16(XIMProtocol *proto,
			GString     *packets,
			guint16      data)
{
	XIMProtocolPrivate *priv;

	g_return_if_fail (XIM_IS_PROTOCOL (proto));
	g_return_if_fail (packets != NULL);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	if (priv->byte_order == XIM_ORDER_MSB) {
		g_string_append_c(packets, (data >> 8) & 0xff);
		g_string_append_c(packets, data & 0xff);
	} else if (priv->byte_order == XIM_ORDER_LSB) {
		g_string_append_c(packets, data & 0xff);
		g_string_append_c(packets, (data >> 8) & 0xff);
	} else {
		g_warning("Failed to put CARD16 due to the unknown byte order.");
	}
}

guint32
xim_protocol_get_card32(XIMProtocol  *proto,
			const gchar  *packets,
			gsize        *length)
{
	XIMProtocolPrivate *priv;
	guint32 retval = 0;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), 0);
	g_return_val_if_fail (packets != NULL, 0);
	g_return_val_if_fail (length != NULL, 0);
	g_return_val_if_fail (*length > 3, 0);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	if (priv->byte_order == XIM_ORDER_MSB) {
		retval = (packets[0] << 24) + (packets[1] << 16) + (packets[2] << 8) + packets[3];
		*length -= 4;
	} else if (priv->byte_order == XIM_ORDER_LSB) {
		retval = (packets[3] << 24) + (packets[2] << 16) + (packets[1] << 8) + packets[0];
		*length -= 4;
	} else {
		g_warning("Failed to get CARD32 due to the unknown byte order.");
	}

	return retval;
}

void
xim_protocol_put_card32(XIMProtocol *proto,
			GString     *packets,
			guint32      data)
{
	XIMProtocolPrivate *priv;

	g_return_if_fail (XIM_IS_PROTOCOL (proto));
	g_return_if_fail (packets != NULL);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	if (priv->byte_order == XIM_ORDER_MSB) {
		g_string_append_c(packets, (data >> 24) & 0xff);
		g_string_append_c(packets, (data >> 16) & 0xff);
		g_string_append_c(packets, (data >> 8) & 0xff);
		g_string_append_c(packets, data & 0xff);
	} else if (priv->byte_order == XIM_ORDER_LSB) {
		g_string_append_c(packets, data & 0xff);
		g_string_append_c(packets, (data >> 8) & 0xff);
		g_string_append_c(packets, (data >> 16) & 0xff);
		g_string_append_c(packets, (data >> 24) & 0xff);
	} else {
		g_warning("Failed to put CARD32 due to the unknown byte order.");
	}
}

guint16
xim_protocol_swap16(XIMProtocol *proto,
		    guint16      val)
{
	XIMProtocolPrivate *priv;
	guint16 retval = 0;
	gchar *p;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), 0);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	p = (gchar *)&val;
	if (priv->byte_order == XIM_ORDER_LSB) {
		retval = (p[0] << 8) + p[1];
	} else if (priv->byte_order == XIM_ORDER_MSB) {
		retval = (p[1] << 8) + p[0];
	} else {
		g_warning("Failed to swap a value due to the unknown byte order.");
	}

	return retval;
}

guint32
xim_protocol_swap32(XIMProtocol *proto,
		    guint32      val)
{
	XIMProtocolPrivate *priv;
	guint32 retval = 0;
	gchar *p;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), 0);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	p = (gchar *)&val;
	if (priv->byte_order == XIM_ORDER_LSB) {
		retval = (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
	} else if (priv->byte_order == XIM_ORDER_MSB) {
		retval = (p[3] << 24) + (p[2] << 16) + (p[1] << 8) + p[0];
	} else {
		g_warning("Failed to swap a value due to the unknown byte order.");
	}

	return retval;
}

guint8
xim_protocol_get_host_byte_order(XIMProtocol *proto)
{
	XIMProtocolPrivate *priv;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), 0);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	return priv->host_byte_order;
}

GString *
xim_protocol_get_string(XIMProtocol *proto,
			const gchar *packets,
			gsize       *length)
{
	guint16 len, i;
	gsize l;
	GString *retval;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), NULL);
	g_return_val_if_fail (packets != NULL, NULL);
	g_return_val_if_fail (length != NULL, NULL);

	l = *length;
	len = xim_protocol_get_card16(proto, &packets[*length - l], &l);
	if (len > *length) {
		g_warning("Invalid packet detected during peeking a STRING");
		return NULL;
	}

	retval = g_string_sized_new(len);
	for (i = 0; i < len; i++) {
		guint8 d = xim_protocol_get_card8(proto, &packets[*length - l], &l);

		g_string_append_c(retval, d);
	}
	l -= PAD4 (2 + len);
	if (l < 0) {
		g_warning("Invalid packet detected during padding of STRING");
		g_string_free(retval, TRUE);

		return NULL;
	}
	*length = l;

	return retval;
}

GString *
xim_protocol_get_str(XIMProtocol *proto,
		     const gchar *packets,
		     gsize       *length)
{
	guint8 len, i;
	gsize l;
	GString *retval;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), NULL);
	g_return_val_if_fail (packets != NULL, NULL);
	g_return_val_if_fail (length != NULL, NULL);

	l = *length;
	len = xim_protocol_get_card8(proto, &packets[*length - l], &l);
	if (len > *length) {
		g_warning("Invalid packet detected during peeking a STR");
		return NULL;
	}

	retval = g_string_sized_new(len);
	for (i = 0; i < len; i++) {
		guint8 d = xim_protocol_get_card8(proto, &packets[*length - l], &l);

		g_string_append_c(retval, d);
	}
	*length = l;

	return retval;
}

XIMAttr *
xim_protocol_get_ximattr(XIMProtocol *proto,
			 const gchar *packets,
			 gsize       *length)
{
	XIMAttr *retval;
	gsize len;
	guint16 i;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), NULL);
	g_return_val_if_fail (packets != NULL, NULL);
	g_return_val_if_fail (length != NULL, NULL);

	len = *length;
	retval = g_new(XIMAttr, 1);
	if (retval == NULL)
		return NULL;

	retval->id = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	retval->type = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	retval->length = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	if ((retval->length + PAD4 (2 + retval->length)) > *length) {
		g_warning("Invalid packet detected during peeking XIMATTR.");
		g_free(retval);

		return NULL;
	}
	retval->name = g_new(gchar, retval->length + 1);
	for (i = 0; i < retval->length; i++) {
		retval->name[i] = xim_protocol_get_card8(proto, &packets[*length - len], &len);
	}
	retval->name[i] = 0;
	/* padding unused */
	len -= PAD4 (2 + retval->length);
	*length = len;

	return retval;
}

XICAttr *
xim_protocol_get_xicattr(XIMProtocol *proto,
			 const gchar *packets,
			 gsize       *length)
{
	XICAttr *retval;
	gsize len;
	guint16 i;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), NULL);
	g_return_val_if_fail (packets != NULL, NULL);
	g_return_val_if_fail (length != NULL, NULL);

	len = *length;
	retval = g_new(XICAttr, 1);
	if (retval == NULL)
		return NULL;

	retval->id = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	retval->type = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	retval->length = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	if ((retval->length + PAD4 (2 + retval->length)) > *length) {
		g_warning("Invalid packet detected during peeking XICATTR.");
		g_free(retval);

		return NULL;
	}
	retval->name = g_new(gchar, retval->length + 1);
	for (i = 0; i < retval->length; i++) {
		retval->name[i] = xim_protocol_get_card8(proto, &packets[*length - len], &len);
	}
	retval->name[i] = 0;
	/* padding unused */
	len -= PAD4 (2 + retval->length);
	*length = len;

	return retval;
}

XIMTriggerKey *
xim_protocol_get_ximtriggerkey(XIMProtocol *proto,
			       const gchar *packets,
			       gsize       *length)
{
	XIMTriggerKey *retval;
	gsize len;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), NULL);
	g_return_val_if_fail (packets != NULL, NULL);
	g_return_val_if_fail (length != NULL, NULL);
	g_return_val_if_fail (*length > sizeof (XIMTriggerKey), NULL);

	len = *length;
	retval = g_new(XIMTriggerKey, 1);
	retval->keysym = xim_protocol_get_card32(proto, &packets[*length - len], &len);
	retval->modifier = xim_protocol_get_card32(proto, &packets[*length - len], &len);
	retval->mask = xim_protocol_get_card32(proto, &packets[*length - len], &len);
	*length = len;

	return retval;
}

GString *
xim_protocol_get_encodinginfo(XIMProtocol *proto,
			      const gchar *packets,
			      gsize       *length)
{
	guint16 l, i;
	GString *retval;
	gsize len;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), NULL);
	g_return_val_if_fail (packets != NULL, NULL);
	g_return_val_if_fail (length != NULL, NULL);

	len = *length;
	l = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	if ((l + PAD4 (2 + l)) > len) {
		g_warning("Invalid packets detected during peeking ENCODINGINFO");

		return NULL;
	}
	retval = g_string_sized_new(l);
	for (i = 0; i < l; i++) {
		guint8 c = xim_protocol_get_card8(proto, &packets[*length - len], &len);

		g_string_append_c(retval, c);
	}
	len -= PAD4 (2 + l);
	*length = len;

	return retval;
}

XIMExt *
xim_protocol_get_ext(XIMProtocol *proto,
		     const gchar *packets,
		     gsize       *length)
{
	XIMExt *retval;
	gsize len;
	guint16 n, i;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), NULL);
	g_return_val_if_fail (packets != NULL, NULL);
	g_return_val_if_fail (length != NULL, NULL);

	len = *length;
	retval = g_new(XIMExt, 1);
	retval->major_opcode = xim_protocol_get_card8(proto, &packets[*length - len], &len);
	retval->minor_opcode = xim_protocol_get_card8(proto, &packets[*length - len], &len);
	n = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	if ((n + PAD4 (n)) > len) {
		g_warning("Invalid packets detected during peeking EXT");
		g_free(retval);
	}
	retval->name = g_string_sized_new(n);
	for (i = 0; i < n; i++) {
		guint8 c = xim_protocol_get_card8(proto, &packets[*length - len], &len);

		g_string_append_c(retval->name, c);
	}
	len -= PAD4 (n);
	*length = len;

	return retval;
}

XIMAttribute *
xim_protocol_get_ximattribute(XIMProtocol *proto,
			      const gchar *packets,
			      gsize       *length)
{
	XIMAttribute *retval;
	guint16 i;
	gsize len;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), NULL);
	g_return_val_if_fail (packets != NULL, NULL);
	g_return_val_if_fail (length != NULL, NULL);

	len = *length;
	retval = g_new(XIMAttribute, 1);
	retval->id = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	retval->length = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	if ((retval->length + PAD4 (retval->length)) > len) {
		g_warning("Invalid packets detected during peeking XIMATTRIBUTE.");
		g_free(retval);

		return NULL;
	}
	retval->value = g_new(gchar, retval->length + 1);
	for (i = 0; i < retval->length; i++) {
		retval->value[i] = xim_protocol_get_card8(proto, &packets[*length - len], &len);
	}
	retval->value[i] = 0;
	len -= PAD4 (retval->length);
	*length = len;

	return retval;
}

XICAttribute *
xim_protocol_get_xicattribute(XIMProtocol *proto,
			      const gchar *packets,
			      gsize       *length)
{
	XICAttribute *retval;
	guint16 i;
	gsize len;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), NULL);
	g_return_val_if_fail (packets != NULL, NULL);
	g_return_val_if_fail (length != NULL, NULL);

	len = *length;
	retval = g_new(XICAttribute, 1);
	retval->id = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	retval->length = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	if ((retval->length + PAD4 (retval->length)) > len) {
		g_warning("Invalid packets detected during peeking XICATTRIBUTE.");
		g_free(retval);

		return NULL;
	}
	retval->value = g_new(gchar, retval->length + 1);
	for (i = 0; i < retval->length; i++) {
		retval->value[i] = xim_protocol_get_card8(proto, &packets[*length - len], &len);
	}
	retval->value[i] = 0;
	len -= PAD4 (retval->length);
	*length = len;

	return retval;
}

XEvent *
xim_protocol_get_xevent(XIMProtocol *proto,
			const gchar *packets,
			gsize       *length)
{
	XIMProtocolPrivate *priv;
	xEvent raw;
	XEvent *retval;
	gchar *p;
	gsize i, len;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), NULL);
	g_return_val_if_fail (packets != NULL, NULL);
	g_return_val_if_fail (length != NULL, NULL);
	g_return_val_if_fail (*length >= sizeof (xEvent), NULL);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	len = *length;
	p = (gchar *)&raw;
	for (i = 0; i < sizeof (xEvent); i++) {
		p[i] = xim_protocol_get_card8(proto, &packets[*length - len], &len);
	}
	retval = g_new0(XEvent, 1);

#define _SWAP32(_v_)	(priv->host_byte_order != priv->byte_order ? xim_protocol_swap32(proto, (_v_)) : (_v_))
#define _SWAP16(_v_)	(priv->host_byte_order != priv->byte_order ? xim_protocol_swap16(proto, (_v_)) : (_v_))

	retval->type = raw.u.u.type & 0x7f;
	retval->xany.serial = _SWAP16 (raw.u.u.sequenceNumber);
	retval->xany.display = proto->dpy;
	retval->xany.send_event = raw.u.u.type > 127;

	switch (retval->type) {
	    case KeyPress:
	    case KeyRelease:
		    retval->xkey.window = _SWAP32 (raw.u.keyButtonPointer.event);
		    retval->xkey.root = _SWAP32 (raw.u.keyButtonPointer.root);
		    retval->xkey.subwindow = _SWAP32 (raw.u.keyButtonPointer.child);
		    retval->xkey.time = _SWAP32 (raw.u.keyButtonPointer.time);
		    retval->xkey.x = _SWAP16 (raw.u.keyButtonPointer.eventX);
		    retval->xkey.y = _SWAP16 (raw.u.keyButtonPointer.eventY);
		    retval->xkey.x_root = _SWAP16 (raw.u.keyButtonPointer.rootX);
		    retval->xkey.y_root = _SWAP16 (raw.u.keyButtonPointer.rootY);
		    retval->xkey.state = _SWAP16 (raw.u.keyButtonPointer.state);
		    retval->xkey.keycode = raw.u.u.detail;
		    retval->xkey.same_screen = raw.u.keyButtonPointer.sameScreen;
		    break;
	    default:
		    g_warning("Unknown/Unsupported type of XEvent in the packet: %d", retval->type);
		    break;
	}
	*length = len;

	return retval;
}

XIMStrConvText *
xim_protocol_get_strconvtext(XIMProtocol *proto,
			     const gchar *packets,
			     gsize       *length)
{
	XIMStrConvText *retval;
	guint16 nbytes, i;
	gsize len;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), NULL);
	g_return_val_if_fail (packets != NULL, NULL);
	g_return_val_if_fail (length != NULL, NULL);

	len = *length;
	retval = g_new(XIMStrConvText, 1);
	retval->feedback = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	retval->length = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	retval->string = g_new(gchar, retval->length + 1);
	for (i = 0; i < retval->length; i++) {
		retval->string[i] = xim_protocol_get_card8(proto, &packets[*length - len], &len);
	}
	retval->string[i] = 0;
	nbytes = xim_protocol_get_card16(proto, &packets[*length - len], &len);
	if ((*length - len - nbytes) < 0) {
		g_warning("Invalid packets detected during peeking XIMSTRCONVTEXT");

		xim_strconvtext_free(retval);

		return NULL;
	}

	return retval;
}

gboolean
xim_protocol_send_packets(XIMProtocol   *proto,
			  const GString *packets)
{
	XIMProtocolPrivate *priv;
	gboolean retval = FALSE;
	guint send_by = 0;
	static const gchar *methods[] = {
		"???",
		"only-CM",
		"Property-with-CM",
		"multi-CM",
		"PropertyNotify"
	};

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), FALSE);
	g_return_val_if_fail (packets != NULL, FALSE);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);

	switch (priv->major_version) {
	    case 0:
		    switch (priv->minor_version) {
			case 0:
				if (packets->len > TRANSPORT_SIZE) {
					/* Send data via Property */
					retval = xim_connection_send_via_property(priv->conn,
										  packets);
					send_by = 2;
				} else {
					/* Send data via ClientMessage */
					retval = xim_connection_send_via_cm(priv->conn,
									    TRANSPORT_SIZE,
									    packets);
					send_by = 1;
				}
				break;
			case 1:
				/* Send data via ClientMessage or multiple ClientMessages */
				retval = xim_connection_send_via_cm(priv->conn,
								    TRANSPORT_SIZE,
								    packets);
				if (packets->len > TRANSPORT_SIZE)
					send_by = 3;
				else
					send_by = 1;
				break;
			case 2:
				if (packets->len > TRANSPORT_MAX) {
					/* Send data via Property */
					retval = xim_connection_send_via_property(priv->conn,
										  packets);
					send_by = 2;
				} else {
					/* Send data via ClientMessage */
					retval = xim_connection_send_via_cm(priv->conn,
									    TRANSPORT_SIZE,
									    packets);
					if (packets->len > TRANSPORT_SIZE)
						send_by = 3;
					else
						send_by = 1;
				}
				break;
			default:
				g_warning("Unsupported protocol version: major: %d minor %d",
					  priv->major_version, priv->minor_version);
				break;
		    }
		    break;
	    case 1:
		    switch (priv->minor_version) {
			case 0:
				/* Send data via Property to PrioertyNotify */
				retval = xim_connection_send_via_property_notify(priv->conn,
										 packets);
				send_by = 4;
				break;
			default:
				g_warning("Unsupported protocol version: major: %d minor %d",
					  priv->major_version, priv->minor_version);
				break;
		    }
		    break;
	    case 2:
		    switch (priv->minor_version) {
			case 0:
				if (packets->len > TRANSPORT_SIZE) {
					/* Send data via PropertyNotify */
					retval = xim_connection_send_via_property_notify(priv->conn,
											 packets);
					send_by = 4;
				} else {
					/* Send data via ClientMessage */
					retval = xim_connection_send_via_cm(priv->conn,
									    TRANSPORT_SIZE,
									    packets);
					if (packets->len > TRANSPORT_SIZE)
						send_by = 3;
					else
						send_by = 1;
				}
				break;
			case 1:
				if (packets->len > TRANSPORT_MAX) {
					/* Send data via Property */
					retval = xim_connection_send_via_property_notify(priv->conn,
											 packets);
					send_by = 4;
				} else {
					/* Send data via ClientMessage */
					retval = xim_connection_send_via_cm(priv->conn,
									    TRANSPORT_SIZE,
									    packets);
					if (packets->len > TRANSPORT_SIZE)
						send_by = 3;
					else
						send_by = 1;
				}
				break;
			default:
				g_warning("Unsupported protocol version: major: %d minor %d",
					  priv->major_version, priv->minor_version);
				break;
		    }
		    break;
	    default:
		    g_warning("Unsupported protocol version: major: %d minor %d",
			      priv->major_version, priv->minor_version);
		    break;
	}
	if (proto->verbose) {
		Window comm_window;
		Atom selection;

		g_object_get(priv->conn,
			     "comm_window", &comm_window,
			     "selection", &selection,
			     NULL);
		g_print("%ld: %s: %s [sent? %s via %s (major: %d, minor: %d)]\n",
			comm_window, selection == None ? "<-" : "->",
			xim_protocol_name(packets->str[0]),
			retval ? "true" : "false",
			methods[send_by],
			priv->major_version, priv->minor_version);
	}

	return retval;
}

gboolean
xim_protocol_send(XIMProtocol *proto,
		  guint8       major_opcode,
		  guint8       minor_opcode,
		  guint        nitems,
		  ...)
{
	gboolean retval = FALSE;
	GString *packets;
	va_list ap;
	guint i;
	XIMProtocolPrivate *priv;

	g_return_val_if_fail (XIM_IS_PROTOCOL (proto), FALSE);

	priv = XIM_PROTOCOL_GET_PRIVATE (proto);
	va_start(ap, nitems);

	packets = g_string_new(NULL);
	g_string_append_c(packets, major_opcode);
	g_string_append_c(packets, minor_opcode);
	g_string_append_c(packets, 0); /* dummy for CARD16 */
	g_string_append_c(packets, 0); /* dummy for CARD16 */

	for (i = 0; i < nitems; i++) {
		XIMValueType type;
		gchar *p;

		type = va_arg(ap, XIMValueType);
		switch (type) {
		    case XIM_TYPE_BYTE:
			    xim_protocol_put_card8(proto, packets, (guint8)va_arg(ap, int));
			    break;
		    case XIM_TYPE_WORD:
			    xim_protocol_put_card16(proto, packets, (guint16)va_arg(ap, int));
			    break;
		    case XIM_TYPE_LONG:
			    xim_protocol_put_card32(proto, packets, va_arg(ap, guint32));
			    break;
		    case XIM_TYPE_CHAR:
			    p = va_arg(ap, gchar *);
			    for (; p != NULL && *p != 0; p++) {
				    xim_protocol_put_card8(proto, packets, *p);
			    }
			    break;
		    case XIM_TYPE_WINDOW:
			    xim_protocol_put_card32(proto, packets, va_arg(ap, Window));
			    break;
		    case XIM_TYPE_XIMSTYLES:
		    case XIM_TYPE_XRECTANGLE:
		    case XIM_TYPE_XPOINT:
		    case XIM_TYPE_XFONTSET:
		    case XIM_TYPE_HOTKEYTRIGGERS:
		    case XIM_TYPE_HOTKEYSTATE:
		    case XIM_TYPE_STRINGCONVERSION:
		    case XIM_TYPE_PREEDITSTATE:
		    case XIM_TYPE_RESETSTATE:
		    case XIM_TYPE_SEPARATOR:
		    case XIM_TYPE_NESTEDLIST:
		    default:
			    g_warning("Unknown/unsupported value type to send packets: %d", type);
			    break;
		}
	}

	/* Update the packet size */
	G_STMT_START {
		GString *s = g_string_new(NULL);

		if (packets->len % 4)
			g_warning("Bad padding: the number of packets: %" G_GSIZE_FORMAT, packets->len);

		xim_protocol_put_card16(proto, s, (packets->len - 4) / 4);
		packets->str[2] = s->str[0];
		packets->str[3] = s->str[1];

		g_string_free(s, TRUE);
	} G_STMT_END;

	retval = xim_protocol_send_packets(proto, packets);

	va_end(ap);
	g_string_free(packets, TRUE);

	return retval;
}
