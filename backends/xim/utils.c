/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * utils.c
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

#include <glib.h>
#include <string.h>
#include <X11/Xatom.h>
#include "protocol.h"
#include "utils.h"


void xim_atoms_free(gpointer data);


static GHashTable *atom_tables = NULL;

/*
 * Private functions
 */
void
xim_atoms_free(gpointer data)
{
	XIMAtoms *atoms = data;

	g_free(atoms);
}

/*
 * Public functions
 */
void
xim_init(Display *dpy)
{
	gchar *dpy_name;
	XIMAtoms *atoms;

	g_return_if_fail (dpy != NULL);

	if (atom_tables == NULL)
		atom_tables = g_hash_table_new_full(g_str_hash, g_str_equal,
						    g_free, xim_atoms_free);
	if (xim_is_initialized(dpy))
		return;

	atoms = g_new(XIMAtoms, 1);
	atoms->atom_xim_servers = XInternAtom(dpy, "XIM_SERVERS", False);
	atoms->atom_locales = XInternAtom(dpy, "LOCALES", False);
	atoms->atom_transport = XInternAtom(dpy, "TRANSPORT", False);
	atoms->atom_xim_xconnect = XInternAtom(dpy, "_XIM_XCONNECT", False);
	atoms->atom_xim_protocol = XInternAtom(dpy, "_XIM_PROTOCOL", False);
	atoms->atom_xim_moredata = XInternAtom(dpy, "_XIM_MOREDATA", False);
	atoms->atom_imsettings_comm = XInternAtom(dpy, "_IMSETTINGS_COMM", False);
	XFlush(dpy);

	dpy_name = g_strdup(DisplayString (dpy));
	g_hash_table_insert(atom_tables, dpy_name, atoms);
}

gboolean
xim_is_initialized(Display *dpy)
{
	return xim_get_atoms(dpy) != NULL;
}

void
xim_destroy(Display *dpy)
{
	g_hash_table_remove(atom_tables, DisplayString (dpy));
}

XIMAtoms *
xim_get_atoms(Display *dpy)
{
	g_return_val_if_fail (dpy != NULL, NULL);

	return g_hash_table_lookup(atom_tables, DisplayString (dpy));
}

GQuark
xim_g_error_quark(void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string("xim-error-quark");

	return quark;
}

Atom
xim_lookup_atom(Display     *dpy,
		const gchar *xim_server_name)
{
	XIMAtoms *a;
	Atom atom_type, *atoms, retval = None;
	int format;
	unsigned long nitems, bytes, i;
	unsigned char *prop;
	gchar *s1 = NULL, *s2 = NULL;

	g_return_val_if_fail (dpy != NULL, None);

	if (xim_server_name == NULL)
		return None;

	XSync(dpy, False);
	s1 = g_strdup_printf("@server=%s", xim_server_name);
	a = xim_get_atoms(dpy);
	XGetWindowProperty(dpy, DefaultRootWindow(dpy), a->atom_xim_servers,
			   0, 1000000L, False, XA_ATOM,
			   &atom_type, &format, &nitems, &bytes, &prop);
	atoms = (Atom *)prop;

	for (i = 0; i < nitems; i++) {
		s2 = XGetAtomName(dpy, atoms[i]);
		if (strcmp(s1, s2) == 0) {
			retval = atoms[i];
			break;
		}
	}

	XFree(atoms);
	g_free(s1);
	if (s2)
		XFree(s2);

	return retval;
}

void
xim_ximattr_free(gpointer data)
{
	XIMAttr *x = data;

	if (data == NULL)
		return;

	g_free(x->name);
	g_free(x);
}

void
xim_xicattr_free(gpointer data)
{
	XICAttr *x = data;

	if (data == NULL)
		return;

	g_free(x->name);
	g_free(x);
}

void
xim_ext_free(gpointer data)
{
	XIMExt *e = data;

	if (data == NULL)
		return;

	g_string_free(e->name, TRUE);
	g_free(e);
}

void
xim_ximattribute_free(gpointer data)
{
	XIMAttribute *a = data;

	if (data == NULL)
		return;

	g_free(a->value);
	g_free(a);
}

void
xim_xicattribute_free(gpointer data)
{
	XICAttribute *a = data;

	if (data == NULL)
		return;

	g_free(a->value);
	g_free(a);
}

void
xim_strconvtext_free(gpointer data)
{
	XIMStrConvText *p = data;

	if (data == NULL)
		return;

	g_free(p->string);
	g_free(p);
}

const gchar *
xim_protocol_name(XIMEventType major_opcode)
{
	static gchar __xim_event_names[] = {
		"\0"
		"XIM_CONNECT\0"
		"XIM_CONNECT_REPLY\0"
		"XIM_DISCONNECT\0"
		"XIM_DISCONNECT_REPLY\0"
		"XIM_AUTH_REQUIRED\0"
		"XIM_AUTH_REPLY\0"
		"XIM_AUTH_NEXT\0"
		"XIM_AUTH_SETUP\0"
		"XIM_AUTH_NG\0"
		"XIM_ERROR\0"
		"XIM_OPEN\0"
		"XIM_OPEN_REPLY\0"
		"XIM_CLOSE\0"
		"XIM_CLOSE_REPLY\0"
		"XIM_REGISTER_TRIGGERKEYS\0"
		"XIM_TRIGGER_NOTIFY\0"
		"XIM_TRIGGER_NOTIFY_REPLY\0"
		"XIM_SET_EVENT_MASK\0"
		"XIM_ENCODING_NEGOTIATION\0"
		"XIM_ENCODING_NEGOTIATION_REPLY\0"
		"XIM_QUERY_EXTENSION\0"
		"XIM_QUERY_EXTENSION_REPLY\0"
		"XIM_SET_IM_VALUES\0"
		"XIM_SET_IM_VALUES_REPLY\0"
		"XIM_GET_IM_VALUES\0"
		"XIM_GET_IM_VALUES_REPLY\0"
		"XIM_CREATE_IC\0"
		"XIM_CREATE_IC_REPLY\0"
		"XIM_DESTROY_IC\0"
		"XIM_DESTROY_IC_REPLY\0"
		"XIM_SET_IC_VALUES\0"
		"XIM_SET_IC_VALUES_REPLY\0"
		"XIM_GET_IC_VALUES\0"
		"XIM_GET_IC_VALUES_REPLY\0"
		"XIM_SET_IC_FOCUS\0"
		"XIM_UNSET_IC_FOCUS\0"
		"XIM_FORWARD_EVENT\0"
		"XIM_SYNC\0"
		"XIM_SYNC_REPLY\0"
		"XIM_COMMIT\0"
		"XIM_RESET_IC\0"
		"XIM_RESET_IC_REPLY\0"
		"XIM_GEOMETRY\0"
		"XIM_STR_CONVERSION\0"
		"XIM_STR_CONVERSION_REPLY\0"
		"XIM_PREEDIT_START\0"
		"XIM_PREEDIT_START_REPLY\0"
		"XIM_PREEDIT_DRAW\0"
		"XIM_PREEDIT_CARET\0"
		"XIM_PREEDIT_CARET_REPLY\0"
		"XIM_PREEDIT_DONE\0"
		"XIM_STATUS_START\0"
		"XIM_STATUS_DRAW\0"
		"XIM_STATUS_DONE\0"
		"XIM_PREEDITSTATE\0"
	};
	static const gint __xim_event_map[] = {
		0,
		1, /* XIM_CONNECT = 1 */
		13, /* XIM_CONNECT_REPLY */
		31, /* XIM_DISCONNECT */
		46, /* XIM_DISCONNECT_REPLY */
		0, /* ??? = 5 */
		0,
		0,
		0,
		0,
		67, /* XIM_AUTH_REQUIRED = 10 */
		85, /* XIM_AUTH_REPLY */
		100, /* XIM_AUTH_NEXT */
		114, /* XIM_AUTH_SETUP */
		129, /* XIM_AUTH_NG */
		0, /* ??? = 15 */
		0,
		0,
		0,
		0,
		141, /* XIM_ERROR = 20 */
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		151, /* XIM_OPEN = 30 */
		160, /* XIM_OPEN_REPLY */
		175, /* XIM_CLOSE */
		185, /* XIM_CLOSE_REPLY */
		201, /* XIM_REGISTER_TRIGGERKEYS */
		226, /* XIM_TRIGGER_NOTIFY */
		245, /* XIM_TRIGGER_NOTIFY_REPLY */
		270, /* XIM_SET_EVENT_MASK */
		289, /* XIM_ENCODING_NEGOTIATION */
		314, /* XIM_ENCODING_NEGOTIATION_REPLY */
		345, /* XIM_QUERY_EXTENSION */
		365, /* XIM_QUERY_EXTENSION_REPLY */
		391, /* XIM_SET_IM_VALUES */
		409, /* XIM_SET_IM_VALUES_REPLY */
		433, /* XIM_GET_IM_VALUES */
		451, /* XIM_GET_IM_VALUES_REPLY */
		0, /* ??? = 46 */
		0,
		0,
		0,
		475, /* XIM_CREATE_IC = 50 */
		489, /* XIM_CREATE_IC_REPLY */
		509, /* XIM_DESTROY_IC */
		524, /* XIM_DESTROY_IC_REPLY */
		545, /* XIM_SET_IC_VALUES */
		563, /* XIM_SET_IC_VALUES_REPLY */
		587, /* XIM_GET_IC_VALUES */
		605, /* XIM_GET_IC_VALUES_REPLY */
		629, /* XIM_SET_IC_FOCUS */
		646, /* XIM_UNSET_IC_FOCUS */
		665, /* XIM_FORWARD_EVENT */
		683, /* XIM_SYNC */
		692, /* XIM_SYNC_REPLY */
		707, /* XIM_COMMIT */
		718, /* XIM_RESET_IC */
		731, /* XIM_RESET_IC_REPLY */
		0, /* ??? = 66 */
		0,
		0,
		0,
		750, /* XIM_GEOMETRY = 70 */
		763, /* XIM_STR_CONVERSION */
		782, /* XIM_STR_CONVERSION_REPLY */
		807, /* XIM_PREEDIT_START */
		825, /* XIM_PREEDIT_START_REPLY */
		849, /* XIM_PREEDIT_DRAW */
		866, /* XIM_PREEDIT_CARET */
		884, /* XIM_PREEDIT_CARET_REPLY */
		908, /* XIM_PREEDIT_DONE */
		925, /* XIM_STATUS_START */
		942, /* XIM_STATUS_DRAW */
		958, /* XIM_STATUS_DONE */
		974, /* XIM_PREEDITSTATE */
		991
	};
	static const gchar *unknown = "???";

	if (major_opcode >= LAST_XIM_EVENTS) {
		return unknown;
	} else {
		return &__xim_event_names[__xim_event_map[major_opcode]];
	}
}

gchar *
xim_substitute_display_name(const gchar *display_name)
{
	GString *str;
	gchar *p;

	if (display_name == NULL)
		display_name = g_getenv("DISPLAY");
	if (display_name == NULL)
		return NULL;

	str = g_string_new(display_name);
	p = strrchr(str->str, '.');
	if (p && p > strchr(str->str, ':'))
		g_string_truncate(str, p - str->str);

	/* Quote:
	 * 3.  Default Preconnection Convention
	 *
	 * IM Servers are strongly encouraged to register their sym-
	 * bolic names as the ATOM names into the IM Server directory
	 * property, XIM_SERVERS, on the root window of the screen_num-
	 * ber 0.
	 */
	g_string_append_printf(str, ".0");

	return g_string_free(str, FALSE);
}
