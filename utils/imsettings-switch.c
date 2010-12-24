/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-switch.c
 * Copyright (C) 2008-2010 Red Hat, Inc. All rights reserved.
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
#include "imsettings.h"
#include "imsettings-client.h"

int
main(int    argc,
     char **argv)
{
	IMSettingsClient *client;
	IMSettingsInfo *info = NULL;
	gchar *locale, *module = NULL;
	gboolean arg_force = FALSE, arg_no_update = FALSE, arg_restart = FALSE, arg_xinputrc = FALSE, arg_quiet = FALSE;
	GOptionContext *ctx = g_option_context_new(_("[Input Method name|xinput.conf]"));
	GOptionEntry entries[] = {
		/* For translators: this is a translation for the command-line option. */
		{"force", 'f', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_force, N_("Force restarting the IM process regardless of any errors."), NULL},
		/* For translators: this is a translation for the command-line option. */
		{"no-update", 'n', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_no_update, N_("Do not update .xinputrc."), NULL},
		/* For translators: this is a translation for the command-line option. */
		{"quiet", 'q', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_quiet, N_("Shut up the extra messages."), NULL},
		/* For translators: this is a translation for the command-line option. */
		{"restart", 'r', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_restart, N_("Restart input method"), NULL},
		/* For translators: this is a translation for the command-line option. */
		{"read-xinputrc", 'x', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_xinputrc, N_("Read xinputrc to determine the input method"), NULL},
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

	client = imsettings_client_new(locale);
	if (!client) {
		g_printerr(_("Unable to create a client instance."));
		goto end;
	}
	if (imsettings_client_get_version(client, NULL, &error) != IMSETTINGS_SETTINGS_API_VERSION) {
		g_printerr(_("Currently a different version of imsettings is running.\nRunning \"imsettings-reload\" may help but it will restart the Input Method\n"));
		retval = 1;
		goto end;
	}

	if (arg_restart) {
		const gchar *m, *l;

		info = imsettings_client_get_active_im_info(client, NULL, &error);
		if (error)
			goto error;
		m = imsettings_info_get_short_desc(info);
		l = imsettings_info_get_language(info);
		if (m == NULL || m[0] == 0 ||
		    strcmp(m, IMSETTINGS_NONE_CONF) == 0) {
			g_printerr(_("No Input Method running to be restarted.\n"));
			retval = 1;
			goto end;
		}
		imsettings_client_switch_im(client, IMSETTINGS_NONE_CONF, FALSE, NULL, &error);
		if (error)
			goto error;
		imsettings_client_set_locale(client, l);
		imsettings_client_switch_im(client, m, FALSE, NULL, &error);
		if (error)
			goto error;

		if (!arg_quiet)
			g_print(_("Restarted %s"), module);
		goto end;
	}
	if (argc < 2 && !arg_xinputrc) {
		gchar *help = g_option_context_get_help(ctx, FALSE, NULL);

		g_print("%s", help);
		g_free(help);
		goto end;
	}
	if (arg_xinputrc) {
		module = imsettings_client_get_user_im(client, NULL, &error);
	} else {
		module = g_strdup(argv[1]);
	}

	imsettings_client_switch_im(client, module, !arg_no_update, NULL, &error);
	if (error) {
	  error:
		g_printerr("%s\n", error->message);
		retval = 1;
		g_clear_error(&error);
		goto end;
	}
	if (!arg_quiet)
		g_print("Switched input method to %s\n", module);
  end:
	g_free(module);
	if (info)
		g_object_unref(info);
	g_object_unref(client);
	g_option_context_free(ctx);

	return retval;
}
