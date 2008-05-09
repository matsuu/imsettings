/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-stop.c
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

#include <stdlib.h>
#include <glib/gi18n.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-request.h"

int
main(int    argc,
     char **argv)
{
	IMSettingsRequest *imsettings;
	DBusConnection *connection;
	gboolean arg_force = FALSE, arg_no_update = FALSE;
	GOptionContext *ctx = g_option_context_new(NULL);
	GOptionEntry entries[] = {
		{"force", 'f', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_force, N_("Force reloading the configuration, including restarting the process.")},
		{"no-update", 'n', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_no_update, N_("Do not update .xinputrc.")},
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	GError *error = NULL;

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
	if (argc < 2) {
		gchar *help = g_option_context_get_help(ctx, TRUE, NULL);

		g_print("%s\n", help);
		g_free(help);

		exit(1);
	}
	g_option_context_free(ctx);

	connection = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	imsettings = imsettings_request_new(connection, IMSETTINGS_INTERFACE_DBUS);
	if (imsettings_request_stop_im(imsettings, argv[1], !arg_no_update, arg_force)) {
		g_print(_("Stopped %s\n"), argv[1]);
	} else {
		g_printerr(_("Failed to stop IM process `%s'\n"), argv[1]);
		exit(1);
	}
	g_object_unref(imsettings);
	dbus_connection_unref(connection);

	return 0;
}
