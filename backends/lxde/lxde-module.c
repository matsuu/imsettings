/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * lxde-module.c
 * Copyright (C) 2009-2010 Red Hat, Inc. All rights reserved.
 * 
 * Authors:
 *   Akira TAGOH  <tagoh@redhat.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include "imsettings-info.h"

void module_switch_im(IMSettingsInfo *info);

/*< private >*/

/*< public >*/
void
module_switch_im(IMSettingsInfo *info)
{
	GKeyFile *key = g_key_file_new();
	gchar *confdir = g_build_filename(g_get_user_config_dir(), "lxde", NULL);
	gchar *conf = g_build_filename(confdir, "config", NULL);
	gchar *s;
	const gchar *gtkimm = imsettings_info_get_gtkimm(info);
	gsize len = 0;

	if (!gtkimm || gtkimm[0] == 0) {
		g_warning("Invalid gtk immodule in: %s",
			  imsettings_info_get_filename(info));
		goto finalize;
	}
	if (!g_key_file_load_from_file(key, conf, 0, NULL)) {
		if (!g_key_file_load_from_file(key, LXDE_CONFIGDIR G_DIR_SEPARATOR_S "config", 0, NULL)) {
			g_warning("Unable to load the lxde configuration file.");
			goto finalize;
		}
	}

	g_key_file_set_string(key, "GTK", "sGtk/IMModule",
			      gtkimm);

	if ((s = g_key_file_to_data(key, &len, NULL)) != NULL) {
		if (g_mkdir_with_parents(confdir, 0700) != 0) {
			int save_errno = errno;

			g_warning("Failed to create the user config dir: %s",
				  g_strerror(save_errno));
			goto finalize;
		}
		if (g_file_set_contents(conf, s, len, NULL)) {
			gchar *p = g_find_program_in_path("lxde-settings-daemon");
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
			      "Setting up %s as gtk+ immodule",
			      gtkimm);
			if (p && !g_spawn_command_line_sync("lxde-settings-daemon reload", NULL, NULL, NULL, NULL)) {
				g_warning("Unable to reload the LXDE settings");
			}
			g_free(p);
		} else {
			g_warning("Unable to store the configuration into %s", conf);
		}
	} else {
		g_warning("Unable to obtain the configuration from the instance.");
	}
	g_free(s);
  finalize:
	g_free(conf);
	g_free(confdir);
	g_key_file_free(key);
}
