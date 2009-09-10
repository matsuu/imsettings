/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-info.c
 * Copyright (C) 2008-2009 Red Hat, Inc. All rights reserved.
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
#include <unistd.h>
#include <locale.h>
#include <glib/gi18n.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-request.h"

int
main(int    argc,
     char **argv)
{
	IMSettingsRequest *req;
	IMSettingsInfo *info;
	DBusConnection *connection;
	const gchar *file, *gtkimm, *qtimm, *xim;
	const gchar *xim_prog, *xim_args;
	const gchar *prefs_prog, *prefs_args;
	const gchar *aux_prog, *aux_args;
	const gchar *short_desc, *long_desc;
	const gchar *icon;
	const gchar *locale;
	gchar *module = NULL;
	gboolean is_system_default, is_user_default, is_xim;
	GError *error = NULL;
	int retval = 0;

	g_type_init();
	setlocale(LC_ALL, "");

	connection = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	if (connection == NULL) {
		g_printerr(_("Failed to get a session bus.\n"));
		return 1;
	}
	req = imsettings_request_new(connection, IMSETTINGS_INTERFACE_DBUS);

	if (imsettings_request_get_version(req, NULL) != IMSETTINGS_SETTINGS_API_VERSION) {
		g_printerr(_("Currently a different version of imsettings is running.\nRunning \"imsettings-reload -f\" may help but it will restart the Input Method\n"));
		retval = 1;
		goto end;
	}

	locale = setlocale(LC_CTYPE, NULL);
	imsettings_request_set_locale(req, locale);
	if (argc < 2) {
		module = imsettings_request_whats_input_method_running(req, &error);
		if (error) {
			g_printerr("%s\n", error->message);
			g_error_free(error);
			retval = 1;
			goto end;
		}
		if (module == NULL || module[0] == 0) {
			g_print(_("No Input Method running. please specify Input Method name explicitly if necessary.\n"));
			retval = 1;
			goto end;
		}
	} else {
		module = g_strdup(argv[1]);
	}
	info = imsettings_request_get_info_object(req, module, &error);
	if (error) {
		g_printerr(_("Unable to obtain an Input Method Information."));
		retval = 1;
		g_clear_error(&error);
	} else {
		file = imsettings_info_get_filename(info);
		gtkimm = imsettings_info_get_gtkimm(info);
		qtimm = imsettings_info_get_qtimm(info);
		xim = imsettings_info_get_xim(info);
		xim_prog = imsettings_info_get_xim_program(info);
		xim_args = imsettings_info_get_xim_args(info);
		prefs_prog = imsettings_info_get_prefs_program(info);
		prefs_args = imsettings_info_get_prefs_args(info);
		aux_prog = imsettings_info_get_aux_program(info);
		aux_args = imsettings_info_get_aux_args(info);
		short_desc = imsettings_info_get_short_desc(info);
		long_desc = imsettings_info_get_long_desc(info);
		is_system_default = imsettings_info_is_system_default(info);
		is_user_default = imsettings_info_is_user_default(info);
		is_xim = imsettings_info_is_xim(info);
		icon = imsettings_info_get_icon_file(info);

		g_print("Xinput file: %s\n"
			"GTK+ immodule: %s\n"
			"Qt immodule: %s\n"
			"XMODIFIERS: @im=%s\n"
			"XIM server: %s %s\n"
			"Preferences: %s %s\n"
			"Auxiliary: %s %s\n"
			"Short Description: %s\n"
			"Long Description: %s\n"
			"Icon file: %s\n"
			"Is system default: %s\n"
			"Is user default: %s\n"
			"Is XIM server: %s\n",
			file, gtkimm, qtimm, xim,
			xim_prog, xim_args ? xim_args : "",
			prefs_prog ? prefs_prog : "",
			prefs_args ? prefs_args : "",
			aux_prog ? aux_prog : "",
			aux_args ? aux_args : "",
			short_desc, long_desc ? long_desc : "",
			icon ? icon : "",
			(is_system_default ? "TRUE" : "FALSE"),
			(is_user_default ? "TRUE" : "FALSE"),
			(is_xim ? "TRUE" : "FALSE"));
	}
  end:
	g_free(module);
	g_object_unref(req);
	dbus_connection_unref(connection);

	return retval;
}
