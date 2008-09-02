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
#include <unistd.h>
#include <locale.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-request.h"

int
main(int    argc,
     char **argv)
{
	IMSettingsRequest *req_settings, *req_info;
	IMSettingsInfo *info;
	DBusConnection *connection;
	const gchar *file, *gtkimm, *qtimm, *xim;
	const gchar *xim_prog, *xim_args;
	const gchar *prefs_prog, *prefs_args;
	const gchar *aux_prog, *aux_args;
	const gchar *short_desc, *long_desc;
	const gchar *icon;
	const gchar *locale;
	gchar *module;
	gboolean is_system_default, is_user_default, is_xim;
	GError *error = NULL;
	guint n_retry = 0;
	int retval = 0;

	g_type_init();

	connection = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	if (connection == NULL) {
		g_printerr("Failed to get a session bus.\n");
		return 1;
	}
	req_settings = imsettings_request_new(connection, IMSETTINGS_INTERFACE_DBUS);
	req_info = imsettings_request_new(connection, IMSETTINGS_INFO_INTERFACE_DBUS);

  retry:
	if (imsettings_request_get_version(req_settings, NULL) != IMSETTINGS_SETTINGS_DAEMON_VERSION) {
		if (n_retry > 0) {
			g_printerr("Mismatch the version of im-settings-daemon.");
			exit(1);
		}
		/* version is inconsistent. try to reload the process */
		imsettings_request_reload(req_settings, TRUE);
		g_print("Waiting for reloading the process...\n");
		/* XXX */
		sleep(1);
		n_retry++;
		goto retry;
	}
	n_retry = 0;
  retry2:
	if (imsettings_request_get_version(req_info, NULL) != IMSETTINGS_IMINFO_DAEMON_VERSION) {
		if (n_retry > 0) {
			g_printerr("Mismatch the version of im-info-daemon.");
			exit(1);
		}
		/* version is inconsistent. try to reload the process */
		imsettings_request_reload(req_info, TRUE);
		g_print("Waiting for reloading the process...\n");
		/* XXX */
		sleep(1);
		n_retry++;
		goto retry2;
	}

	locale = setlocale(LC_CTYPE, NULL);
	imsettings_request_set_locale(req_settings, locale);
	imsettings_request_set_locale(req_info, locale);
	if (argc < 2) {
		module = imsettings_request_what_im_is_running(req_settings, &error);
		if (error) {
			g_printerr("%s\n", error->message);
			g_error_free(error);
			retval = 1;
			goto end;
		}
		if (module == NULL || module[0] == 0) {
			g_print("No IM is running. please specify the IM name explicitly.\n");
			retval = 1;
			goto end;
		}
	} else {
		module = g_strdup(argv[1]);
	}
	info = imsettings_request_get_info_object(req_info, module, &error);
	if (error) {
		g_printerr("Unable to get an IM info.\n");
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
	g_object_unref(req_settings);
	g_object_unref(req_info);
	dbus_connection_unref(connection);

	return 0;
}
