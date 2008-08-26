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
#include "utils.h"


void xim_atoms_free(gpointer data);


/*
 * Private functions
 */

/*
 * Public functions
 */
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
