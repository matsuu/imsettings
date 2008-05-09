/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * utils.h
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
#ifndef __XIM_UTILS_H__
#define __XIM_UTILS_H__

#include <glib.h>
#include <X11/Xlib.h>
#include "protocol.h"

G_BEGIN_DECLS

#define TRANSPORT_MAX			20
#define TRANSPORT_SIZE			20

typedef struct _XIMAtoms	XIMAtoms;

struct _XIMAtoms {
	Atom atom_xim_servers;
	Atom atom_locales;
	Atom atom_transport;
	Atom atom_xim_xconnect;
	Atom atom_xim_protocol;
	Atom atom_xim_moredata;
	Atom atom_imsettings_comm;
};
enum {
	XIM_G_FAILED
};

void         xim_init             (Display      *dpy);
gboolean     xim_is_initialized   (Display      *dpy);
void         xim_destroy          (Display      *dpy);
XIMAtoms    *xim_get_atoms        (Display      *dpy);
GQuark       xim_g_error_quark    (void);
Atom         xim_lookup_atom      (Display      *dpy,
                                   const gchar  *xim_server_name);
void         xim_ximattr_free     (gpointer      data);
void         xim_xicattr_free     (gpointer      data);
void         xim_ext_free         (gpointer      data);
void         xim_ximattribute_free(gpointer      data);
void         xim_xicattribute_free(gpointer      data);
void         xim_strconvtext_free (gpointer      data);
const gchar *xim_protocol_name    (XIMEventType  major_opcode);
gchar       *xim_substitute_display_name(const gchar *display_name) G_GNUC_MALLOC;


G_END_DECLS

#endif /* __XIM_UTILS_H__ */
