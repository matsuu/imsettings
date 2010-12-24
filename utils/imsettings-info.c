/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-info.c
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
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <glib/gi18n.h>
#include "imsettings.h"
#include "imsettings-client.h"
#include "imsettings-info.h"

int
main(int    argc,
     char **argv)
{
	IMSettingsClient *client = NULL;
	IMSettingsInfo *info;
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
	GVariant *v = NULL;

	g_type_init();
	setlocale(LC_ALL, "");

	locale = setlocale(LC_CTYPE, NULL);

	client = imsettings_client_new(locale);
	if (!client) {
		g_printerr(_("Unable to create a client instance."));
		goto end;
	}
	if (imsettings_client_get_version(client, NULL, &error) != IMSETTINGS_SETTINGS_API_VERSION) {
		if (error)
			goto error;
		g_printerr(_("Currently a different version of imsettings is running.\nRunning \"imsettings-reload -f\" may help but it will restart the Input Method\n"));
		retval = 1;
		goto end;
	}

	if (argc < 2) {
		info = imsettings_client_get_active_im_info(client, NULL, &error);
		if (error) {
		  error:
			g_printerr("%s\n", error->message);
			g_error_free(error);
			retval = 1;
			goto end;
		}
	} else {
		module = g_strdup(argv[1]);
		v = imsettings_client_get_info_variant(client, module, NULL, &error);
		if (error) {
			g_printerr(_("Unable to obtain an Input Method Information: %s\n"),
				   error->message);
			retval = 1;
			g_clear_error(&error);
			goto end;
		}
		info = imsettings_info_new(v);
	}

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

	g_object_unref(info);
  end:
	g_free(module);
	if (v)
		g_variant_unref(v);
	if (client)
		g_object_unref(client);

	return retval;
}
