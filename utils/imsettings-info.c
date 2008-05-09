/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-info.c
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
#include <stdlib.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-request.h"

int
main(int    argc,
     char **argv)
{
	IMSettingsRequest *imsettings;
	DBusConnection *connection;
	gchar *file, *gtkimm, *qtimm, *xim;
	gchar *xim_prog, *xim_args, *prefs_prog, *prefs_args, *aux_prog, *aux_args;
	gchar *short_desc, *long_desc;
	gboolean is_system_default, is_user_default, is_xim;

	g_type_init();

	if (argc < 2) {
		gchar *progname = g_path_get_basename(argv[0]);

		g_print("Usage: %s <module name>\n", progname);

		g_free(progname);

		exit(1);
	}
	connection = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	imsettings = imsettings_request_new(connection, IMSETTINGS_INFO_INTERFACE_DBUS);

	file = imsettings_request_get_xinput_filename(imsettings, argv[1]);
	if (file == NULL) {
		g_printerr("Failed to get an IM info.\n");
	} else {
		gtkimm = imsettings_request_get_im_module_name(imsettings, argv[1], IMSETTINGS_IMM_GTK);
		qtimm = imsettings_request_get_im_module_name(imsettings, argv[1], IMSETTINGS_IMM_QT);
		xim = imsettings_request_get_im_module_name(imsettings, argv[1], IMSETTINGS_IMM_XIM);
		imsettings_request_get_xim_program(imsettings, argv[1], &xim_prog, &xim_args);
		imsettings_request_get_preferences_program(imsettings, argv[1], &prefs_prog, &prefs_args);
		imsettings_request_get_auxiliary_program(imsettings, argv[1], &aux_prog, &aux_args);
		short_desc = imsettings_request_get_short_description(imsettings, argv[1]);
		long_desc = imsettings_request_get_long_description(imsettings, argv[1]);
		is_system_default = imsettings_request_is_system_default(imsettings, argv[1]);
		is_user_default = imsettings_request_is_user_default(imsettings, argv[1]);
		is_xim = imsettings_request_is_xim(imsettings, argv[1]);

		g_print("Xinput file: %s\n"
			"GTK+ immodule: %s\n"
			"Qt immodule: %s\n"
			"XMODIFIERS: @im=%s\n"
			"XIM server: %s %s\n"
			"preferences: %s %s\n"
			"auxiliary: %s %s\n"
			"Short Description: %s\n"
			"Long Description: %s\n"
			"Is system default: %s\n"
			"Is user default: %s\n"
			"Is XIM server: %s\n",
			file, gtkimm, qtimm, xim,
			xim_prog, xim_args,
			prefs_prog, prefs_args,
			aux_prog, aux_args,
			short_desc, long_desc,
			(is_system_default ? "TRUE" : "FALSE"),
			(is_user_default ? "TRUE" : "FALSE"),
			(is_xim ? "TRUE" : "FALSE"));
	}
	g_object_unref(imsettings);
	dbus_connection_unref(connection);

	return 0;
}
