/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * main.c
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

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <xfconf/xfconf.h>

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
	XfconfChannel *ch;

	val = gconf_value_get_string(gval);
	ch = xfconf_channel_new("xsettings");
	if (!xfconf_channel_set_string(ch, "/Gtk/IMModule", val)) {
		g_warning("Failed to set %s to xfconf.", val);
	}
	g_object_unref(ch);
}

/*
 * Public functions
 */
int
main(int argc, char **argv)
{
	GError *error = NULL;
	GConfClient *client = NULL;
	GMainLoop *loop;
	GConfEntry *entry;
	guint ctxt_id;

	if (!xfconf_init(&error)) {
		g_printerr("Failed to connect to XFconf daemon: %s\n",
			   error->message);
		return 1;
	}

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
	xfconf_shutdown();

	return 0;
}
