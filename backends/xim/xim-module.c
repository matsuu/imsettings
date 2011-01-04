/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * xim-module.c
 * Copyright (C) 2008-2011 Red Hat, Inc. All rights reserved.
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

#include <gio/gio.h>
#include "imsettings.h"
#include "imsettings-info.h"

void module_switch_im(IMSettingsInfo *info);

static gchar *__client_address = NULL;

/*< private >*/

/*< public >*/
void
module_switch_im(IMSettingsInfo *info)
{
	GDBusConnection *connection;
	GError *err = NULL;
	GVariant *value;
	gboolean ret = FALSE;
	const gchar *xim = imsettings_info_get_xim(info);

	if (!__client_address) {
		g_spawn_command_line_async("imsettings-xim --address=unix:abstract=/tmp/imsettings-xim --replace", &err);
		if (err) {
			g_warning("Unable to spawn XIM server: %s", err->message);
			g_error_free(err);
			return;
		}
		__client_address = g_strdup("unix:abstract=/tmp/imsettings-xim");
		g_usleep(3 * G_USEC_PER_SEC);
	}
	connection = g_dbus_connection_new_for_address_sync(__client_address,
							    G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
							    NULL,
							    NULL,
							    &err);
	if (!connection) {
		g_warning("Unable to connect to %s: %s",
			  __client_address,
			  err->message);
		g_error_free(err);
		return;
	}

	value = g_dbus_connection_call_sync(connection,
					    NULL,
					    IMSETTINGS_XIM_PATH_DBUS,
					    IMSETTINGS_XIM_INTERFACE_DBUS,
					    "SwitchXIM",
					    g_variant_new("(s)",
							  xim),
					    G_VARIANT_TYPE ("(b)"),
					    G_DBUS_CALL_FLAGS_NONE,
					    -1,
					    NULL,
					    &err);
	if (value)
		g_variant_get(value, "(b)", &ret);
	if (!ret) {
		g_warning("Unable to update XIM settings: %s",
			  err ? err->message : "unknown");
	} else {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		      "Setting up %s as XIM", xim);
	}

	if (value)
		g_variant_unref(value);
	g_object_unref(connection);
}
