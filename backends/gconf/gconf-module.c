/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gconf-module.c
 * Copyright (C) 2008-2010 Red Hat, Inc. All rights reserved.
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

#include <gconf/gconf-client.h>
#include "imsettings-info.h"

void module_switch_im(IMSettingsInfo *info);

/*< private >*/

/*< public >*/
void
module_switch_im(IMSettingsInfo *info)
{
	GConfEngine *engine;
	GConfValue *val = NULL;
	const gchar *gtkimm = imsettings_info_get_gtkimm(info);
	GError *err = NULL;

	engine = gconf_engine_get_default();
	if (!engine) {
		g_warning("Unable to obtain GConfEngine instance.");
		return;
	}
	if (!gtkimm || gtkimm[0] == 0) {
		g_warning("Invalid gtk immodule in: %s",
			  imsettings_info_get_filename(info));
		goto finalize;
	}

	val = gconf_value_new_from_string(GCONF_VALUE_STRING,
					  gtkimm, &err);
	if (err)
		goto error;
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
	      "Setting up %s as gtk+ immodule",
	      gtkimm);
	gconf_engine_set(engine,
			 "/desktop/gnome/interface/gtk-im-module",
			 val, &err);
	if (err) {
	  error:
		g_warning("%s", err->message);
		g_error_free(err);
	}
  finalize:
	if (val)
		gconf_value_free(val);
	if (engine) {
		gconf_engine_unref(engine);
	}
	g_print("%d\n", gconf_debug_shutdown());
}
