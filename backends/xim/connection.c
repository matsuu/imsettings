/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * connection.c
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include <X11/Xatom.h>
#include "protocol.h"
#include "utils.h"
#include "connection.h"


#define XIM_CONNECTION_GET_PRIVATE(_o_)	(G_TYPE_INSTANCE_GET_PRIVATE ((_o_), XIM_TYPE_CONNECTION, XIMConnectionPrivate))

#ifdef GNOME_ENABLE_DEBUG
#define d(e)	e
#else
#define d(e)
#endif

typedef struct _XIMConnectionPrivate {
	XIMProtocol *proto;
	Display     *dpy;
	XIMAtoms    *atoms;
	Window       client_window;
	Window       comm_window;
	Atom         selection;
	gulong       major_transport;
	gulong       minor_transport;
	gboolean     verbose;
	GString     *packet_cache;
} XIMConnectionPrivate;

enum {
	PROP_0,
	PROP_DISPLAY,
	PROP_CLIENT_WINDOW,
	PROP_COMM_WINDOW,
	PROP_SELECTION,
	PROP_VERBOSE,
	PROP_PROTOCOL,
	PROP_PROTOCOL_OBJECT,
	LAST_PROP
};
enum {
	LAST_SIGNAL
};


//static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (XIMConnection, xim_connection, G_TYPE_OBJECT);

/*
 * Private functions
 */
static void
xim_connection_create_agent(XIMConnection *connection)
{
	XIMConnectionPrivate *priv = XIM_CONNECTION_GET_PRIVATE (connection);

	if (priv->comm_window != 0)
		XDestroyWindow(priv->dpy, priv->comm_window);

	priv->comm_window = XCreateSimpleWindow(priv->dpy,
						DefaultRootWindow(priv->dpy),
						0, 0, 1, 1, 0, 0, 0);

	XSelectInput(priv->dpy, DefaultRootWindow(priv->dpy), 0);
	XSync(priv->dpy, False);
}

static void
xim_connection_set_property(GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
	XIMConnectionPrivate *priv = XIM_CONNECTION_GET_PRIVATE (object);

	switch (prop_id) {
	    case PROP_DISPLAY:
		    priv->dpy = g_value_get_pointer(value);
		    if (priv->client_window != 0)
			    xim_connection_create_agent(XIM_CONNECTION (object));
		    priv->atoms = xim_get_atoms(priv->dpy);
		    break;
	    case PROP_CLIENT_WINDOW:
		    priv->client_window = g_value_get_ulong(value);
		    if (priv->dpy)
			    xim_connection_create_agent(XIM_CONNECTION (object));
		    break;
	    case PROP_VERBOSE:
		    priv->verbose = g_value_get_boolean(value);
		    g_object_set(priv->proto, "verbose", priv->verbose, NULL);
		    break;
	    case PROP_SELECTION:
		    priv->selection = g_value_get_uint(value);
		    break;
	    case PROP_PROTOCOL:
		    priv->proto = XIM_PROTOCOL (g_object_new(g_value_get_gtype(value),
							     "display", priv->dpy,
							     "connection", object,
							     "verbose", priv->verbose,
							     NULL));
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
xim_connection_get_property(GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
	XIMConnectionPrivate *priv = XIM_CONNECTION_GET_PRIVATE (object);

	switch (prop_id) {
	    case PROP_DISPLAY:
		    g_value_set_pointer(value, priv->dpy);
		    break;
	    case PROP_CLIENT_WINDOW:
		    g_value_set_ulong(value, priv->client_window);
		    break;
	    case PROP_COMM_WINDOW:
		    g_value_set_ulong(value, priv->comm_window);
		    break;
	    case PROP_SELECTION:
		    g_value_set_uint(value, priv->selection);
		    break;
	    case PROP_VERBOSE:
		    g_value_set_boolean(value, priv->verbose);
		    break;
	    case PROP_PROTOCOL_OBJECT:
		    g_value_set_object(value, priv->proto);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
xim_connection_finalize(GObject *object)
{
	XIMConnectionPrivate *priv = XIM_CONNECTION_GET_PRIVATE (object);

	if (priv->comm_window)
		XDestroyWindow(priv->dpy, priv->comm_window);
	g_string_free(priv->packet_cache, TRUE);
	g_object_unref(priv->proto);

	if (G_OBJECT_CLASS (xim_connection_parent_class)->finalize)
		G_OBJECT_CLASS (xim_connection_parent_class)->finalize(object);
}

static void
xim_connection_class_init(XIMConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private(klass, sizeof (XIMConnectionPrivate));

	object_class->set_property = xim_connection_set_property;
	object_class->get_property = xim_connection_get_property;
	object_class->finalize     = xim_connection_finalize;

	/* properties */
	g_object_class_install_property(object_class, PROP_DISPLAY,
					g_param_spec_pointer("display",
							     _("X display"),
							     _("X display to use"),
							     G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
	g_object_class_install_property(object_class, PROP_CLIENT_WINDOW,
					g_param_spec_ulong("client_window",
							   _("Window ID"),
							   _("Window ID for the client"),
							   0,
							   G_MAXULONG,
							   0,
							   G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
	g_object_class_install_property(object_class, PROP_COMM_WINDOW,
					g_param_spec_ulong("comm_window",
							   _("Communication Window ID"),
							   _("Window ID to communicate the client"),
							   0,
							   G_MAXULONG,
							   0,
							   G_PARAM_READABLE));
	g_object_class_install_property(object_class, PROP_SELECTION,
					g_param_spec_uint("selection",
							  _("Selection Atom"),
							  _("Atom for selection"),
							  0,
							  G_MAXUINT,
							  0,
							  G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_VERBOSE,
					g_param_spec_boolean("verbose",
							     _("Verbose flag"),
							     _("Output a lot of helpful messages to debug"),
							     FALSE,
							     G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_PROTOCOL,
					g_param_spec_gtype("protocol",
							   _("Protocol"),
							   _("GType to deal with the XIM protocol"),
							   XIM_TYPE_PROTOCOL,
							   G_PARAM_WRITABLE|G_PARAM_CONSTRUCT));
	g_object_class_install_property(object_class, PROP_PROTOCOL_OBJECT,
					g_param_spec_object("protocol_object",
							    _("XIMProtocol"),
							    _("GObject of GObject"),
							    XIM_TYPE_PROTOCOL,
							    G_PARAM_READABLE));

	/* signals */
}

static void
xim_connection_init(XIMConnection *connection)
{
	XIMConnectionPrivate *priv = XIM_CONNECTION_GET_PRIVATE (connection);

	priv->atoms = NULL;
	priv->major_transport = 0;
	priv->minor_transport = 0;
	priv->proto = NULL;
	priv->packet_cache = g_string_new(NULL);
}

static void
xim_connection_forward_client_message_event(XIMConnection       *from,
					    XIMConnection       *to,
					    XClientMessageEvent *event)
{
	XIMConnectionPrivate *fpriv, *tpriv;
	static const gchar *sdirection[] = {
		"->", "<-"
	};
	gint direction, i;
	Window destination, orig;
	gsize packlen;
	Atom atom_type;
	int format;
	gulong nitems, bytes;
	unsigned char *prop;

	fpriv = XIM_CONNECTION_GET_PRIVATE (from);
	tpriv = XIM_CONNECTION_GET_PRIVATE (to);
	orig = event->window;

	if (fpriv->selection == None) {
		direction = 0; /* to the server */
		destination = event->window = tpriv->client_window;
	} else {
		direction = 1; /* to the client */
		if (event->window != fpriv->comm_window)
			g_warning("Invalid event. we were expecting it should be a reply.");
	}
	destination = event->window = tpriv->client_window;

	if (event->message_type == fpriv->atoms->atom_xim_xconnect) {
		if (orig == fpriv->comm_window /* do we really need to check this? */ &&
		    fpriv->selection != None) {
			/* have to look at the transport version to pick up
			 * and deliver Property too perhaps
			 */
			tpriv->major_transport = fpriv->major_transport = event->data.l[1];
			tpriv->major_transport = fpriv->minor_transport = event->data.l[2];
			fpriv->client_window = (Window)event->data.l[0];

			event->data.l[0] = tpriv->comm_window;
		} else {
			/* the client should be set the transport version to 0:0. */
			XSelectInput(fpriv->dpy,
				     fpriv->comm_window,
				     StructureNotifyMask);
			XSelectInput(tpriv->dpy,
				     tpriv->comm_window,
				     StructureNotifyMask);

			event->data.l[0] = tpriv->comm_window;
		}
		d(g_print("%ld: %s: _XIM_XCONNECT from %ld to %ld\n",
			  fpriv->comm_window,
			  sdirection[direction],
			  orig,
			  event->window));

		if (!XSendEvent(fpriv->dpy,
				destination,
				False,
				NoEventMask,
				(XEvent *)event)) {
			if (direction == 0) {
				g_warning("FIXME: We have to send back something to the client here then. otherwise the client applications will be frozen because the XIM specification assumes this is synchronous event.");
			} else {
				g_warning("The client application may be terminated.");
			}
		}
	} else if (event->message_type == fpriv->atoms->atom_xim_moredata) {
		for (i = 0; i < TRANSPORT_SIZE; i++) {
			g_string_append_c(tpriv->packet_cache, event->data.b[i]);
		}
	} else if (event->message_type == fpriv->atoms->atom_xim_protocol) {
		XIMEventType op = LAST_XIM_EVENTS;

		if (event->format == 32) {
			XGetWindowProperty(fpriv->dpy, orig,
					   event->data.l[1],
					   0, event->data.l[0],
					   True, AnyPropertyType,
					   &atom_type, &format,
					   &nitems, &bytes, &prop);
			if (prop) {
				for (i = 0; i < nitems; i++) {
					g_string_append_c(tpriv->packet_cache, prop[i]);
				}
			}
			packlen = tpriv->packet_cache->len;
			xim_protocol_process_packets(tpriv->proto, tpriv->packet_cache->str, &packlen);
			op = tpriv->packet_cache->str[0];
			g_string_erase(tpriv->packet_cache, 0, tpriv->packet_cache->len - packlen);
			if (tpriv->packet_cache->len > 0) {
				g_warning("There may be unprocessed packets in the cache: %" G_GSIZE_FORMAT " packets.",
					  tpriv->packet_cache->len);
			};
		} else if (event->format == 8) {
			for (i = 0; i < TRANSPORT_SIZE; i++) {
				g_string_append_c(tpriv->packet_cache, event->data.b[i]);
			}
			packlen = tpriv->packet_cache->len;
			xim_protocol_process_packets(tpriv->proto, tpriv->packet_cache->str, &packlen);
			op = tpriv->packet_cache->str[0];
			g_string_erase(tpriv->packet_cache, 0, tpriv->packet_cache->len - packlen);
			if (tpriv->packet_cache->len > 0) {
				g_warning("There may be unprocessed packets in the cache: %" G_GSIZE_FORMAT " packets.",
					  tpriv->packet_cache->len);
			};
		} else {
			g_warning("Invalid packet.");
		}
		if (op == XIM_DISCONNECT_REPLY) {
			/* all the connections should be closed here */
			g_object_unref(from);
		}
	} else {
		gchar *s = XGetAtomName(fpriv->dpy, event->message_type);

		g_warning("non XIM message: %s", s);
		XFree(s);
		destination = 0;
	}
}

/*
 * Public functions
 */
XIMConnection *
xim_connection_new(Display     *dpy,
		   GType        protocol_type,
		   Window       client_window)
{
	g_return_val_if_fail (dpy != NULL, NULL);

	return XIM_CONNECTION (g_object_new(XIM_TYPE_CONNECTION,
					    "display", dpy,
					    "client_window", client_window,
					    "protocol", protocol_type,
					    NULL));
}

void
xim_connection_forward_event(XIMConnection *from,
			     XIMConnection *to,
			     XEvent        *event)
{
	XIMConnectionPrivate *fpriv, *tpriv;
	XSelectionRequestEvent *esr;
	XSelectionEvent *es;
	Window destination = 0;
	static const gchar *event_names[] = {
		"Reserved in the protocol",
		"Reserved in the protocol",
		"KeyPress",
		"KeyRelease",
		"ButtonPress",
		"ButtonRelease",
		"MotionNotify",
		"EnterNotify",
		"LeaveNotify",
		"FocusIn",
		"FocusOut",
		"KeymapNotify",
		"Expose",
		"GraphicsExpose",
		"NoExpose",
		"VisibilityNotify",
		"CreateNotify",
		"DestroyNotify",
		"UnmapNotify",
		"MapNotify",
		"MapRequest",
		"ReparentNotify",
		"ConfigureNotify",
		"ConfigureRequest",
		"GravityNotify",
		"ResizeRequest",
		"CirculateNotify",
		"CirculateRequest",
		"PropertyNotify",
		"SelectionClear",
		"SelectionRequest",
		"SelectionNotify",
		"ColormapNotify",
		"ClientMessage",
		"MappingNotify",
		NULL
	};
	Atom atom_type;
	int format;
	gulong nitems, bytes;
	unsigned char *prop;


	g_return_if_fail (XIM_IS_CONNECTION (from));
	g_return_if_fail (XIM_IS_CONNECTION (to));
	g_return_if_fail (event != NULL);
	g_return_if_fail (event->type <= MappingNotify);

	fpriv = XIM_CONNECTION_GET_PRIVATE (from);
	tpriv = XIM_CONNECTION_GET_PRIVATE (to);

	switch (event->type) {
	    case SelectionRequest:
		    /* from the client */
		    esr = (XSelectionRequestEvent *)event;
		    if (tpriv->selection == None) {
			    g_warning("Getting SelectionRequest from the XIM server side is unlikely.\n"
				      "    From:   comm_window: %ld\n"
				      "          client_window: %ld\n"
				      "              selection: %ld\n"
				      "      To:   comm_window: %ld\n"
				      "          client_window: %ld\n"
				      "              selection: %ld\n"
				      "   Event:     requestor: %ld\n"
				      "                  owner: %ld",
				      fpriv->comm_window, fpriv->client_window, fpriv->selection,
				      tpriv->comm_window, tpriv->client_window, tpriv->selection,
				      esr->requestor, esr->owner);
			    return;
		    }
		    if (fpriv->verbose) {
			    g_print("%ld: FWD: %s: %ld(%ld)->%ld(%ld)[->%ld]\n",
				    fpriv->comm_window,
				    event_names[event->type],
				    esr->requestor, fpriv->client_window,
				    esr->owner, tpriv->comm_window,
				    tpriv->client_window);
		    }
		    destination = esr->owner = tpriv->client_window;
		    esr->requestor = tpriv->comm_window;
		    esr->selection = tpriv->selection;
		    break;
	    case SelectionNotify:
		    /* from the XIM server */
		    if (fpriv->selection == None) {
			    g_warning("Getting SelectionNotify from the client side is unlikely.");
			    return;
		    }
		    es = (XSelectionEvent *)event;
		    if (fpriv->verbose) {
			    g_print("%ld: FWD: %s: %ld->%ld(%ld)[->%ld]\n",
				    fpriv->comm_window,
				    event_names[event->type],
				    fpriv->client_window,
				    es->requestor, fpriv->comm_window,
				    tpriv->client_window);
		    }
		    destination = es->requestor = tpriv->client_window;

		    XGetWindowProperty(fpriv->dpy, fpriv->comm_window,
				       es->property, 0, 1000000L, True, es->target,
				       &atom_type, &format, &nitems, &bytes, &prop);
		    XChangeProperty(fpriv->dpy, tpriv->client_window,
				    es->property, es->target, 8, PropModeReplace,
				    prop, nitems);
		    XFree(prop);
		    break;
	    case ClientMessage:
		    xim_connection_forward_client_message_event(from, to, &event->xclient);
		    break;
	    default:
		    g_warning("Unsupported event type %s", event_names[event->type]);
		    break;
	}
	if (destination) {
		if (!XSendEvent(fpriv->dpy,
				destination,
				False,
				NoEventMask,
				event)) {
			g_warning("Failed to send event to %ld", destination);
		}
	}
}

gboolean
xim_connection_send_via_cm(XIMConnection *conn,
			   gsize          threshold,
			   const GString *packets)
{
	XIMConnectionPrivate *priv;
	XClientMessageEvent ev;
	gsize length, offset = 0;

	g_return_val_if_fail (XIM_IS_CONNECTION (conn), FALSE);
	g_return_val_if_fail (packets != NULL, FALSE);

	priv = XIM_CONNECTION_GET_PRIVATE (conn);
	length = packets->len;

	while (length > threshold) {
		ev.type = ClientMessage;
		ev.display = priv->dpy;
		ev.window = priv->client_window;
		ev.message_type = priv->atoms->atom_xim_moredata;
		ev.format = 8;
		memcpy(ev.data.b, &packets->str[offset], TRANSPORT_SIZE);
		XSendEvent(priv->dpy, priv->client_window, False,
			   NoEventMask, (XEvent *)&ev);
		offset += TRANSPORT_SIZE;
		length -= TRANSPORT_SIZE;
	}
	ev.type = ClientMessage;
	ev.display = priv->dpy;
	ev.window = priv->client_window;
	ev.message_type = priv->atoms->atom_xim_protocol;
	ev.format = 8;
	memset(ev.data.b, 0, TRANSPORT_SIZE);
	memcpy(ev.data.b, &packets->str[offset], length);

	return XSendEvent(priv->dpy, priv->client_window, False,
			  NoEventMask, (XEvent *)&ev) != 0;
}

gboolean
xim_connection_send_via_property(XIMConnection *conn,
				 const GString *packets)
{
	XIMConnectionPrivate *priv;
	XClientMessageEvent ev;

	g_return_val_if_fail (XIM_IS_CONNECTION (conn), FALSE);
	g_return_val_if_fail (packets != NULL, FALSE);

	priv = XIM_CONNECTION_GET_PRIVATE (conn);

	ev.type = ClientMessage;
	ev.display = priv->dpy;
	ev.window = priv->client_window;
	ev.message_type = priv->atoms->atom_xim_protocol;
	ev.format = 32;
	ev.data.l[0] = packets->len;
	ev.data.l[1] = priv->atoms->atom_imsettings_comm;
	XChangeProperty(priv->dpy, priv->client_window,
			priv->atoms->atom_imsettings_comm, XA_STRING, 8,
			PropModeAppend,
			(unsigned char *)packets->str,
			packets->len);

	return XSendEvent(priv->dpy, priv->client_window, False,
			  NoEventMask, (XEvent *)&ev) != 0;
}

gboolean
xim_connection_send_via_property_notify(XIMConnection *conn,
					const GString *packets)
{
	XIMConnectionPrivate *priv;
	XPropertyEvent ev;
	Atom a;
	static gint32 count = 0;
	gchar *name;

	g_return_val_if_fail (XIM_IS_CONNECTION (conn), FALSE);
	g_return_val_if_fail (packets != NULL, FALSE);

	priv = XIM_CONNECTION_GET_PRIVATE (conn);

	name = g_strdup_printf("_imsettings%ld_%d", priv->client_window, count++);
	a = XInternAtom(priv->dpy, name, False);
	g_free(name);

	ev.type = PropertyNotify;
	ev.display = priv->dpy;
	ev.window = priv->client_window;
	ev.atom = a;
	ev.time = CurrentTime;
	ev.state = PropertyNewValue;
	XChangeProperty(priv->dpy, priv->client_window,
			a, XA_STRING, 8,
			PropModeAppend,
			(unsigned char *)packets->str,
			packets->len);

	return XSendEvent(priv->dpy, priv->client_window, False,
			  NoEventMask, (XEvent *)&ev) != 0;
}
