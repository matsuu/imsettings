/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-list.c
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
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib/gi18n.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-request.h"

int
main(int    argc,
     char **argv)
{
	IMSettingsRequest *req;
	DBusConnection *connection;
	gchar *locale;
	gchar *user_im, *system_im, *running_im;
	gint i;
	GError *error = NULL;
	GPtrArray *array;

	setlocale(LC_ALL, "");
	locale = setlocale(LC_CTYPE, NULL);

	g_type_init();

	connection = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	if (connection == NULL) {
		g_printerr("Failed to get a session bus.\n");
		return 1;
	}
	req = imsettings_request_new(connection, IMSETTINGS_INTERFACE_DBUS);
	imsettings_request_set_locale(req, locale);

	if (imsettings_request_get_version(req, NULL) != IMSETTINGS_SETTINGS_API_VERSION) {
		g_printerr(_("Currently a different version of imsettings is running.\nRunning \"imsettings-reload -f\" may help but it will restart the Input Method\n"));
		exit(1);
	}

	if ((array = imsettings_request_get_info_objects(req, &error)) == NULL) {
		g_printerr("Failed to obtain an Input Method list.\n");
	} else {
		user_im = imsettings_request_get_current_user_im(req, &error);
		system_im = imsettings_request_get_current_system_im(req, &error);
		running_im = imsettings_request_whats_input_method_running(req, &error);
		if (error) {
			g_printerr("%s\n", error->message);
			exit(1);
		}
		for (i = 0; i < array->len; i++) {
			IMSettingsInfo *info = g_ptr_array_index(array, i);
			const gchar *name = imsettings_info_get_short_desc(info);
			gchar *xinput = g_path_get_basename(imsettings_info_get_filename(info));

			g_print("%s %d: %s[%s] %s\n",
				(strcmp(running_im, name) == 0 ? "*" : (strcmp(user_im, name) == 0 ? "-" : " ")),
				i + 1,
				name, xinput,
				(strcmp(system_im, name) == 0 ? "(recommended)" : ""));
			g_object_unref(info);
			g_free(xinput);
		}
		g_ptr_array_free(array, TRUE);
		g_free(user_im);
		g_free(system_im);
		g_free(running_im);
	}
	g_object_unref(req);
	dbus_connection_unref(connection);

	return 0;
}
