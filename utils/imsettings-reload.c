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
#include "imsettings.h"
#include "imsettings-client.h"

int
main(int    argc,
     char **argv)
{
	IMSettingsClient *client;
	IMSettingsInfo *info = NULL;
	gboolean arg_force = FALSE;
	GOptionContext *ctx = g_option_context_new(NULL);
	GOptionEntry entries[] = {
		{"force", 'f', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_force, N_("Force reloading imsettings-daemon (deprecated)."), NULL},
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	GError *error = NULL;
	gint retval = 0;

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

	client = imsettings_client_new(NULL, NULL, &error);
	if (error)
		goto error;
	if (imsettings_client_get_version(client, NULL, &error) != IMSETTINGS_SETTINGS_API_VERSION) {
		imsettings_client_reload(client, TRUE, NULL, &error);
	} else {
		const gchar *module, *lang;

		info = imsettings_client_get_active_im_info(client, NULL, &error);
		module = imsettings_info_get_short_desc(info);
		lang = imsettings_info_get_language(info);
		imsettings_client_reload(client, FALSE, NULL, &error);
		if (error)
			goto error;
		if (g_strcmp0(module, IMSETTINGS_NONE_CONF) != 0) {
			/* this instance isn't valid anymore */
			g_object_unref(client);
			client = imsettings_client_new(lang, NULL, &error);
			imsettings_client_switch_im(client, module, FALSE, NULL, &error);
			if (error) {
			  error:
				g_object_unref(info);
				g_printerr("E: %s\n", error->message);
				retval = 1;
				goto end;
			}
		}
		g_object_unref(info);
	}
	g_print(_("Reloaded.\n"));
  end:
	g_object_unref(client);

	return retval;
}
