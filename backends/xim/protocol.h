/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * protocol.h
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
#ifndef __XIM_PROTOCOL_H__
#define __XIM_PROTOCOL_H__

#include <glib.h>
#include <glib-object.h>
#include <X11/Xlib.h>
#include "connection.h"

G_BEGIN_DECLS

#define XIM_TYPE_PROTOCOL		(xim_protocol_get_type())
#define XIM_PROTOCOL(_o_)		(G_TYPE_CHECK_INSTANCE_CAST ((_o_), XIM_TYPE_PROTOCOL, XIMProtocol))
#define XIM_PROTOCOL_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), XIM_TYPE_PROTOCOL, XIMProtocolClass))
#define XIM_IS_PROTOCOL(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), XIM_TYPE_PROTOCOL))
#define XIM_IS_PROTOCOL_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), XIM_TYPE_PROTOCOL))
#define XIM_PROTOCOL_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), XIM_TYPE_PROTOCOL, XIMProtocolClass))


typedef struct _XIMProtocolClass	XIMProtocolClass;
typedef struct _XIMProtocol		XIMProtocol;
typedef struct _XIMAttr			XIMAttr;
typedef struct _XICAttr			XICAttr;
typedef struct _XIMTriggerKey		XIMTriggerKey;
typedef struct _XIMExt			XIMExt;
typedef struct _XIMAttribute		XIMAttribute;
typedef struct _XICAttribute		XICAttribute;
typedef struct _XIMStrConvText		XIMStrConvText;

struct _XIMProtocolClass {
	GObjectClass parent_class;

	void (* forward_raw_packets)            (XIMProtocol    *proto,
						 const gchar    *packets,
						 gsize          *length);

	/* XIM events */
	gboolean (* connect)                    (XIMProtocol    *proto,
						 guint16         major_version,
						 guint16         minor_version,
						 const GSList   *list);
	gboolean (* connect_reply)              (XIMProtocol    *proto,
						 guint16         major_version,
						 guint16         minor_version);
	gboolean (* disconnect)                 (XIMProtocol    *proto);
	gboolean (* disconnect_reply)           (XIMProtocol    *proto);
	gboolean (* auth_required)              (XIMProtocol    *proto,
						 const gchar    *auth_data,
						 gsize           length);
	gboolean (* auth_reply)                 (XIMProtocol    *proto,
						 const gchar    *auth_data,
						 gsize           length);
	gboolean (* auth_next)                  (XIMProtocol    *proto,
						 const gchar    *auth_data,
						 gsize           length);
	gboolean (* auth_setup)                 (XIMProtocol    *proto,
						 const GSList   *list);
	gboolean (* auth_ng)                    (XIMProtocol    *proto);
	gboolean (* error)                      (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 guint16         mask,
						 guint16         error_code);
	gboolean (* open)                       (XIMProtocol    *proto,
						 const gchar    *locale);
	gboolean (* open_reply)                 (XIMProtocol    *proto,
						 const GSList   *imattrs,
						 const GSList   *icattrs);
	gboolean (* close)                      (XIMProtocol    *proto,
						 guint16         imid);
	gboolean (* close_reply)                (XIMProtocol    *proto,
						 guint16         imid);
	gboolean (* register_triggerkeys)       (XIMProtocol    *proto,
						 guint16         imid,
						 const GSList   *onkeys,
						 const GSList   *offkeys);
	gboolean (* trigger_notify)             (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 guint32         flag,
						 guint32         index,
						 guint32         mask);
	gboolean (* trigger_notify_reply)       (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* set_event_mask)             (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 guint32         forward_event_mask,
						 guint32         synchronous_event_mask);
	gboolean (* encoding_negotiation)       (XIMProtocol    *proto,
						 const GSList   *encodings,
						 const GSList   *details);
	gboolean (* encoding_negotiation_reply) (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         category,
						 gint16          index);
	gboolean (* query_extension)            (XIMProtocol    *proto,
						 guint16         imid,
						 const GSList   *extensions);
	gboolean (* query_extension_reply)      (XIMProtocol    *proto,
						 guint16         imid,
						 const GSList   *extensions);
	gboolean (* set_im_values)              (XIMProtocol    *proto,
						 guint16         imid,
						 const GSList   *attributes);
	gboolean (* set_im_values_reply)        (XIMProtocol    *proto,
						 guint16         imid);
	gboolean (* get_im_values)              (XIMProtocol    *proto,
						 guint16         imid,
						 const GSList   *attr_id);
	gboolean (* get_im_values_reply)        (XIMProtocol    *proto,
						 guint16         imid,
						 const GSList   *attributes);
	gboolean (* create_ic)                  (XIMProtocol    *proto,
						 guint16         imid,
						 const GSList   *attributes);
	gboolean (* create_ic_reply)            (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* destroy_ic)                 (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* destroy_ic_reply)           (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* set_ic_values)              (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 const GSList   *attributes);
	gboolean (* set_ic_values_reply)        (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* get_ic_values)              (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 const GSList   *attr_id);
	gboolean (* get_ic_values_reply)        (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 const GSList   *attributes);
	gboolean (* set_ic_focus)               (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* unset_ic_focus)             (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* forward_event)              (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 guint16         flag,
						 XEvent         *event);
	gboolean (* sync)                       (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* sync_reply)                 (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* commit)                     (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 guint16         flag,
						 const gchar    *packets,
						 gsize          *length);
	gboolean (* reset_ic)                   (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* reset_ic_reply)             (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 GString        *string);
	gboolean (* geometry)                   (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* str_conversion)             (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 guint16         position,
						 guint32         caret_direction,
						 guint16         factor,
						 guint16         operation,
						 gint16          type_length);
	gboolean (* str_conversion_reply)       (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 guint32         feedback,
						 XIMStrConvText *text);
	gboolean (* preedit_start)              (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* preedit_start_reply)        (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 gint32          return_value);
	gboolean (* preedit_draw)               (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 gint32          caret,
						 gint32          chg_first,
						 gint32          chg_length,
						 guint32         status,
						 const GString  *string,
						 const GSList   *feedbacks);
	gboolean (* preedit_caret)              (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 gint32          position,
						 guint32         direction,
						 guint32         style);
	gboolean (* preedit_caret_reply)        (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 guint32         position);
	gboolean (* preedit_done)               (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* status_start)               (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* status_draw)                (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 guint32         type,
						 const gchar    *packets,
						 gsize          *length);
	gboolean (* status_done)                (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid);
	gboolean (* preeditstate)               (XIMProtocol    *proto,
						 guint16         imid,
						 guint16         icid,
						 guint32         mask);
};

struct _XIMProtocol {
	GObject  parent_instance;
	Display *dpy;
	gboolean verbose;
};

struct _XIMAttr {
	guint16  id;
	guint16  type;
	guint16  length;
	gchar   *name;
};

struct _XICAttr {
	guint16  id;
	guint16  type;
	guint16  length;
	gchar   *name;
};

struct _XIMTriggerKey {
	guint32 keysym;
	guint32 modifier;
	guint32 mask;
};

struct _XIMExt {
	guint8   major_opcode;
	guint8   minor_opcode;
	GString *name;
};

struct _XIMAttribute {
	guint16  id;
	guint16  length;
	gchar   *value;
};

struct _XICAttribute {
	guint16  id;
	guint16  length;
	gchar   *value;
};

struct _XIMStrConvText {
	guint16  feedback;
	guint16  length;
	gchar   *string;
};

typedef enum {
	XIM_CONNECT = 1,
	XIM_CONNECT_REPLY,
	XIM_DISCONNECT,
	XIM_DISCONNECT_REPLY,
	XIM_AUTH_REQUIRED = 10,
	XIM_AUTH_REPLY,
	XIM_AUTH_NEXT,
	XIM_AUTH_SETUP,
	XIM_AUTH_NG,
	XIM_ERROR = 20,
	XIM_OPEN = 30,
	XIM_OPEN_REPLY,
	XIM_CLOSE,
	XIM_CLOSE_REPLY,
	XIM_REGISTER_TRIGGERKEYS,
	XIM_TRIGGER_NOTIFY,
	XIM_TRIGGER_NOTIFY_REPLY,
	XIM_SET_EVENT_MASK,
	XIM_ENCODING_NEGOTIATION,
	XIM_ENCODING_NEGOTIATION_REPLY,
	XIM_QUERY_EXTENSION,
	XIM_QUERY_EXTENSION_REPLY,
	XIM_SET_IM_VALUES,
	XIM_SET_IM_VALUES_REPLY,
	XIM_GET_IM_VALUES,
	XIM_GET_IM_VALUES_REPLY,
	XIM_CREATE_IC = 50,
	XIM_CREATE_IC_REPLY,
	XIM_DESTROY_IC,
	XIM_DESTROY_IC_REPLY,
	XIM_SET_IC_VALUES,
	XIM_SET_IC_VALUES_REPLY,
	XIM_GET_IC_VALUES,
	XIM_GET_IC_VALUES_REPLY,
	XIM_SET_IC_FOCUS,
	XIM_UNSET_IC_FOCUS,
	XIM_FORWARD_EVENT,
	XIM_SYNC,
	XIM_SYNC_REPLY,
	XIM_COMMIT,
	XIM_RESET_IC,
	XIM_RESET_IC_REPLY,
	XIM_GEOMETRY = 70,
	XIM_STR_CONVERSION,
	XIM_STR_CONVERSION_REPLY,
	XIM_PREEDIT_START,
	XIM_PREEDIT_START_REPLY,
	XIM_PREEDIT_DRAW,
	XIM_PREEDIT_CARET,
	XIM_PREEDIT_CARET_REPLY,
	XIM_PREEDIT_DONE,
	XIM_STATUS_START,
	XIM_STATUS_DRAW,
	XIM_STATUS_DONE,
	XIM_PREEDITSTATE,
	LAST_XIM_EVENTS
} XIMEventType;

typedef enum {
	XIM_TYPE_SEPARATOR = 0,
	XIM_TYPE_BYTE,
	XIM_TYPE_WORD,
	XIM_TYPE_LONG,
	XIM_TYPE_CHAR,
	XIM_TYPE_WINDOW,
	XIM_TYPE_XIMSTYLES = 10,
	XIM_TYPE_XRECTANGLE,
	XIM_TYPE_XPOINT,
	XIM_TYPE_XFONTSET,
	XIM_TYPE_HOTKEYTRIGGERS = 15,
	XIM_TYPE_HOTKEYSTATE,
	XIM_TYPE_STRINGCONVERSION,
	XIM_TYPE_PREEDITSTATE,
	XIM_TYPE_RESETSTATE,
	XIM_TYPE_NESTEDLIST = 0x7fff
} XIMValueType;

typedef enum {
	XIM_ERR_BadAlloc = 1,
	XIM_ERR_BadStyle,
	XIM_ERR_BadClientWindow,
	XIM_ERR_BadFocusWindow,
	XIM_ERR_BadArea,
	XIM_ERR_BadSpotLocation,
	XIM_ERR_BadColormap,
	XIM_ERR_BadAtom,
	XIM_ERR_BadPixel,
	XIM_ERR_BadPixmap,
	XIM_ERR_BadName,
	XIM_ERR_BadCursor,
	XIM_ERR_BadProtocol,
	XIM_ERR_BadForeground,
	XIM_ERR_BadBackground,
	XIM_ERR_LocaleNotSupported,
	XIM_ERR_BadSomething = 999
} XIMErrorCode;

GType           xim_protocol_get_type           (void) G_GNUC_CONST;
XIMProtocol    *xim_protocol_new                (const Display       *dpy,
                                                 const XIMConnection *conn);
void            xim_protocol_process_packets    (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
void            xim_protocol_process_event      (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
guint8          xim_protocol_get_card8          (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
void            xim_protocol_put_card8          (XIMProtocol         *proto,
                                                 GString             *packets,
                                                 guint8               data);
guint16         xim_protocol_get_card16         (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
void            xim_protocol_put_card16         (XIMProtocol         *proto,
                                                 GString             *packets,
                                                 guint16              data);
guint32         xim_protocol_get_card32         (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
void            xim_protocol_put_card32         (XIMProtocol         *proto,
                                                 GString             *packets,
                                                 guint32              data);
guint16         xim_protocol_swap16             (XIMProtocol         *proto,
                                                 guint16              val);
guint32         xim_protocol_swap32             (XIMProtocol         *proto,
                                                 guint32              val);
guint8          xim_protocol_get_host_byte_order(XIMProtocol         *proto);
GString        *xim_protocol_get_string         (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
GString        *xim_protocol_get_str            (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
XIMAttr        *xim_protocol_get_ximattr        (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
XICAttr        *xim_protocol_get_xicattr        (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
XIMTriggerKey  *xim_protocol_get_ximtriggerkey  (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
GString        *xim_protocol_get_encodinginfo   (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
XIMExt         *xim_protocol_get_ext            (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
XIMAttribute   *xim_protocol_get_ximattribute   (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
XICAttribute   *xim_protocol_get_xicattribute   (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
XEvent         *xim_protocol_get_xevent         (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
XIMStrConvText *xim_protocol_get_strconvtext    (XIMProtocol         *proto,
                                                 const gchar         *packets,
                                                 gsize               *length);
gboolean        xim_protocol_send_packets       (XIMProtocol         *proto,
                                                 const GString       *packets);
gboolean        xim_protocol_send               (XIMProtocol         *proto,
						 guint8               major_opcode,
						 guint8               minor_opcode,
						 guint                nitems,
						 ...);


G_END_DECLS

#endif /* __XIM_PROTOCOL_H__ */
