/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings_plugin.c
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

#include <gconf/gconf.h>
#include <libxfce4mcs/mcs-manager.h>
#include <libxfce4util/libxfce4util.h>
#include <xfce-mcs-manager/manager-plugin.h>

#define CHANNEL		"settings"

static GConfEngine *gengine = NULL;
static guint connection_id = 0;

/*
 * Private functions
 */
static void
_notify_cb(GConfEngine *engine,
	   guint        connection_id,
	   GConfEntry  *entry,
	   gpointer     data)
{
	McsPlugin *plugin = data;
	GConfValue *gval = gconf_entry_get_value(entry);
	const gchar *val;

	g_return_if_fail (gval != NULL && gval->type == GCONF_VALUE_STRING);

	val = gconf_value_get_string(gval);
	mcs_manager_set_string(plugin->manager, "Gtk/IMModule", CHANNEL, val);
	mcs_manager_notify(plugin->manager, CHANNEL);
}

static void
run_dialog(McsPlugin *plugin)
{
	gchar *argv[] = {
		"im-chooser",
		NULL
	};

	if (g_spawn_async(g_get_tmp_dir(),
			  argv, NULL,
			  G_SPAWN_SEARCH_PATH |
			  G_SPAWN_STDOUT_TO_DEV_NULL |
			  G_SPAWN_STDERR_TO_DEV_NULL,
			  NULL, NULL, NULL, NULL) == FALSE) {
	}
}

/*
 * Public functions
 */
McsPluginInitResult
mcs_plugin_init(McsPlugin *plugin)
{
	gchar *val, *iconfile;
	GError *error = NULL;

	xfce_textdomain(GETTEXT_PACKAGE, IMSETTINGS_LOCALEDIR, "UTF-8");

	iconfile = g_build_filename(ICONDIR, "im-chooser.png", NULL);
	plugin->plugin_name = g_strdup("imsettings");
	plugin->caption = g_strdup(Q_("Button Label|Input Method"));
	plugin->run_dialog = run_dialog;
	plugin->icon = gdk_pixbuf_new_from_file_at_scale(iconfile, 48, 48, TRUE, NULL);

	mcs_manager_add_channel(plugin->manager, CHANNEL);

	/* Sorry to looking up GConf in Xfce. however this is the standard way
	 * (so far) to share the configuration among GTK+/GNOME applications
	 * and Xfce for gtk+ immodule.
	 */
	gengine = gconf_engine_get_default();
	if ((connection_id = gconf_engine_notify_add(gengine, "/desktop/gnome/interface/gtk-im-module",
						     _notify_cb, plugin, NULL)) == 0)
		return MCS_PLUGIN_INIT_ERROR;

	val = gconf_engine_get_string(gengine, "/desktop/gnome/interface/gtk-im-module", &error);
	/* Deliver the value to XSETTINGS regardless of its value */
	mcs_manager_set_string(plugin->manager, "Gtk/IMModule", CHANNEL, val);
	mcs_manager_notify(plugin->manager, CHANNEL);

	return MCS_PLUGIN_INIT_OK;
}

MCS_PLUGIN_CHECK_INIT
