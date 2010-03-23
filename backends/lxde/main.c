/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * main.c
 * Copyright (C) 2009-2010 Red Hat, Inc. All rights reserved.
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
#include <string.h>
#include <errno.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include "imsettings/imsettings-utils.h"


/*
 * Private functions
 */
static void
_notify_cb(GConfClient *client,
	   guint        ctxt_id,
	   GConfEntry  *entry,
	   gpointer     user_data)
{
	GConfValue *gval = gconf_entry_get_value(entry);
	const gchar *val;
	GKeyFile *key = g_key_file_new();
	gchar *confdir = g_build_filename(g_get_user_config_dir(), "lxde", NULL);
	gchar *conf = g_build_filename(confdir, "config", NULL);
	gchar *s;
	gsize len = 0;

	val = gconf_value_get_string(gval);
	if (!g_key_file_load_from_file(key, conf, 0, NULL)) {
		if (!g_key_file_load_from_file(key, LXDE_CONFIGDIR G_DIR_SEPARATOR_S "config", 0, NULL)) {
			g_warning("Unable to load the lxde configuration file.");
			goto end;
		}
	}

	g_key_file_set_string(key, "GTK", "sGtk/IMModule", val);

	if ((s = g_key_file_to_data(key, &len, NULL)) != NULL) {
		if (g_mkdir_with_parents(confdir, 0700) != 0) {
			int save_errno = errno;

			g_warning("Failed to create the user config dir: %s", g_strerror(save_errno));
			g_free(s);
			goto end;
		}
		if (g_file_set_contents(conf, s, len, NULL)) {
			if (!g_spawn_command_line_sync("lxde-settings-daemon reload", NULL, NULL, NULL, NULL)) {
				g_warning("Unable to reload the LXDE settings");
			}
		} else {
			g_warning("Unable to store the configuration into %s", conf);
		}
	} else {
		g_warning("Unable to obtain the configuration from the instance.");
	}
	g_free(s);
  end:
	g_free(conf);
	g_free(confdir);
	g_key_file_free(key);
}

/*
 * Public functions
 */
int
main(int    argc,
     char **argv)
{
	GError *error = NULL;
	GConfClient *client = NULL;
	GMainLoop *loop;
	GConfEntry *entry;
	guint ctxt_id;

	client = gconf_client_get_default();
	if (client == NULL) {
		g_printerr("Failed to obtain the default GConfClient instance.\n");
		goto end;
	}
	gconf_client_add_dir(client, "/desktop/gnome/interface",
			     GCONF_CLIENT_PRELOAD_ONELEVEL, &error);
	if (error) {
		g_printerr("Failed to listen on the gconf dir: %s\n",
			   error->message);
		goto end;
	}
	if ((ctxt_id = gconf_client_notify_add(client, "/desktop/gnome/interface/gtk-im-module",
					       _notify_cb, NULL, NULL, &error)) == 0) {
		g_printerr("Failed to listen on the gconf key: %s\n",
			   error->message);
		goto end;
	}

	/* set a initial value */
	entry = gconf_client_get_entry(client, "/desktop/gnome/interface/gtk-im-module", NULL, TRUE, &error);
	if (error) {
		g_printerr("Failed to obtain gtk-im-module from gconf: %s\n",
			   error->message);
	} else {
		_notify_cb(client, ctxt_id, entry, NULL);
	}
	loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
  end:
	if (client)
		g_object_unref(client);

	return 0;
}
