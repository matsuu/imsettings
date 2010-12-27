/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * xfce-module.c
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

#include <xfconf/xfconf.h>
#include "imsettings-info.h"

void module_switch_im(IMSettingsInfo *info);

/*< private >*/

/*< public >*/
void
module_switch_im(IMSettingsInfo *info)
{
	XfconfChannel *ch;
	GError *err = NULL;
	const gchar *gtkimm = imsettings_info_get_gtkimm(info);

	if (!gtkimm || gtkimm[0] == 0) {
		g_warning("Invalid gtk immodule in: %s",
			  imsettings_info_get_filename(info));
		return;
	}
	if (!xfconf_init(&err)) {
		g_warning("Unable to connect to XFconf daemon: %s",
			  err->message);
		g_error_free(err);
		return;
	}
	ch = xfconf_channel_new("xsettings");
	if (!xfconf_channel_set_string(ch, "/Gtk/IMModule", gtkimm)) {
		g_warning("Unable to set %s to xfconf.", gtkimm);
	} else {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		      "Setting up %s as gtk+ immodule",
		      gtkimm);
	}
	g_object_unref(ch);

	xfconf_shutdown();
}
