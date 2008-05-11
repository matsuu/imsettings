/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-utils.c
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

#include "imsettings-utils.h"


GQuark
imsettings_g_error_quark(void)
{
	static GQuark quark = 0;

	if (quark == 0)
		quark = g_quark_from_static_string("imsettings-error-quark");

	return quark;
}

gchar *
imsettings_generate_dbus_path_from_interface(const gchar *interface)
{
	GString *string = g_string_new(NULL);
	gint i;

	g_string_printf(string, "/%s", interface);
	for (i = 0; i < string->len; i++) {
		if (string->str[i] == '.')
			string->str[i] = '/';
	}

	return g_string_free(string, FALSE);
}
