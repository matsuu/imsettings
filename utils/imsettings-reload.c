/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-reload.c
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-request.h"

int
main(int    argc,
     char **argv)
{
	IMSettingsRequest *req, *req_gconf;
	gboolean arg_force = FALSE;
	GOptionContext *ctx = g_option_context_new(NULL);
	GOptionEntry entries[] = {
		{"force", 'f', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_force, N_("Force reloading the configuration, including restarting the process.")},
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	GError *error = NULL;
	DBusConnection *connection;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, IMSETTINGS_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif /* HAVE_BIND_TEXTDOMAIN_CODESET */
	textdomain (GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */

	g_type_init();

	g_option_context_add_main_entries(ctx, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse(ctx, &argc, &argv, &error)) {
		if (error != NULL) {
			g_printerr("%s\n", error->message);
		} else {
			g_warning(_("Unknown error in parsing the command lines."));
		}
		exit(1);
	}
	g_option_context_free(ctx);

	connection = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	if (connection == NULL) {
		g_printerr("Failed to get a session bus.\n");
		return 1;
	}
	req = imsettings_request_new(connection, IMSETTINGS_INTERFACE_DBUS);
	req_gconf = imsettings_request_new(connection, IMSETTINGS_GCONF_INTERFACE_DBUS);
	imsettings_request_reload(req, arg_force);
	imsettings_request_reload(req_gconf, arg_force);
	sleep(1);

	g_print("Reloaded.\n");

	g_object_unref(req);
	g_object_unref(req_gconf);
	dbus_connection_unref(connection);

	return 0;
}
