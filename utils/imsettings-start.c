/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-start.c
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

#include <locale.h>
#include <stdlib.h>
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
	gchar *locale, *module = NULL;
	gboolean arg_no_update = FALSE, arg_cond_start = FALSE, arg_quiet = FALSE;
	GOptionContext *ctx = g_option_context_new(_("[Input Method name|xinput.conf]"));
	GOptionEntry entries[] = {
		{"no-update", 'n', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_no_update, N_("Do not update .xinputrc."), NULL},
		{"cond-start", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_cond_start, N_("Start Input Method if not yet running."), NULL},
		{"quiet", 'q', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_quiet, N_("Shut up the extra messages."), NULL},
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	GError *error = NULL;
	int retval = 0;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, IMSETTINGS_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif /* HAVE_BIND_TEXTDOMAIN_CODESET */
	textdomain (GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */

	setlocale(LC_ALL, "");
	locale = setlocale(LC_CTYPE, NULL);

	g_type_init();

	g_option_context_add_main_entries(ctx, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse(ctx, &argc, &argv, &error)) {
		if (error != NULL) {
			g_printerr("%s\n", error->message);
		} else {
			g_warning(_("Unknown error in parsing the command lines."));
		}
		return 1;
	}

	connection = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	if (connection == NULL) {
		g_printerr(_("Failed to get a session bus.\n"));
		return 1;
	}
	req = imsettings_request_new(connection, IMSETTINGS_INTERFACE_DBUS);
	imsettings_request_set_locale(req, locale);

	if (imsettings_request_get_version(req, NULL) != IMSETTINGS_SETTINGS_API_VERSION) {
		g_printerr(_("Currently a different version of imsettings is running.\nRunning \"imsettings-reload -f\" may help but it will restart the Input Method\n"));
		retval = 1;
		goto end;
	}

	if (argc < 2) {
		const gchar *env = g_getenv("IMSETTINGS_MODULE");

		if (env != NULL && env[0] != 0) {
			module = g_strdup(env);
		} else {
			module = imsettings_request_get_current_user_im(req, &error);
			if (error) {
				g_printerr("%s\n", error->message);
				g_error_free(error);
				retval = 1;
				goto end;
			}
			if (module == NULL || module[0] == 0) {
				g_print(_("No Input Method available\n"));
				retval = 1;
				goto end;
			}
		}
	} else {
		module = g_strdup(argv[1]);
	}
	g_option_context_free(ctx);

	if (arg_cond_start) {
		gchar *name = imsettings_request_whats_input_method_running(req, &error);

		if (name && strcmp(name, module) == 0) {
			g_free(name);
			goto end;
		}
		g_free(name);
	}
	if (imsettings_request_start_im(req, module, !arg_no_update, &error)) {
		if (!arg_quiet)
			g_print(_("Started %s\n"), module);
	} else {
		g_printerr(_("Failed to start IM process `%s'\n"), module);
		retval = 1;
		g_error_free(error);
	}
  end:
	g_free(module);
	g_object_unref(req);
	dbus_connection_unref(connection);

	return retval;
}
