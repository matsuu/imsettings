/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-utils.c
 * Copyright (C) 2008 Red Hat, Inc. All rights reserved.
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

#include "imsettings-utils.h"

static const gchar introspection_xml[] =
	"<node name='/com/redhat/imsettings'>"
	"  <interface name='com.redhat.imsettings'>"
	"    <!-- Information APIs -->"
	"    <method name='GetVersion'>"
	"      <arg type='u' name='version' direction='out' />"
	"    </method>"
	"    <method name='GetInfoVariants'>"
	"      <arg type='s' name='lang' direction='in' />"
	"      <arg type='a{sv}' name='ret' direction='out' />"
	"    </method>"
	"    <method name='GetInfoVariant'>"
	"      <arg type='s' name='lang' direction='in' />"
	"      <arg type='s' name='name' direction='in' />"
	"      <arg type='a{sv}' name='ret' direction='out' />"
	"    </method>"
	"    <method name='GetActiveVariant'>"
	"      <arg type='a{sv}' name='ret' direction='out' />"
	"    </method>"
	"    <method name='GetUserIM'>"
	"      <arg type='s' name='lang' direction='in' />"
	"      <arg type='s' name='ret' direction='out'>"
	"	<annotation name='org.freedesktop.DBus.GLib.Const' value='' />"
	"      </arg>"
	"    </method>"
	"    <method name='GetSystemIM'>"
	"      <arg type='s' name='lang' direction='in' />"
	"      <arg type='s' name='ret' direction='out'>"
	"	<annotation name='org.freedesktop.DBus.GLib.Const' value='' />"
	"      </arg>"
	"    </method>"
	"    <method name='IsSystemDefault'>"
	"      <arg type='s' name='lang' direction='in' />"
	"      <arg type='s' name='imname' direction='in' />"
	"      <arg type='b' name='ret' direction='out' />"
	"    </method>"
	"    <method name='IsUserDefault'>"
	"      <arg type='s' name='lang' direction='in' />"
	"      <arg type='s' name='imname' direction='in' />"
	"      <arg type='b' name='ret' direction='out' />"
	"    </method>"
	"    <method name='IsXIM'>"
	"      <arg type='s' name='lang' direction='in' />"
	"      <arg type='s' name='imname' direction='in' />"
	"      <arg type='b' name='ret' direction='out' />"
	"    </method>"
	"    <!-- Operation APIs -->"
	"    <method name='SwitchIM'>"
	"      <arg type='s' name='lang' direction='in' />"
	"      <arg type='s' name='module' direction='in' />"
	"      <arg type='b' name='update_xinputrc' direction='in' />"
	"      <arg type='b' name='ret' direction='out' />"
	"    </method>"
	"    <signal name='Reload'>"
	"      <arg type='b' name='ret' direction='out' />"
	"    </signal>"
	"    <method name='StopService'>"
	"      <arg type='b' name='ret' direction='out' />"
	"    </method>"
	"  </interface>"
	"</node>";

/*< private >*/

/*< public >*/

/**
 * imsettings_g_error_quark:
 *
 * FIXME
 *
 * Returns:
 */
GQuark
imsettings_g_error_quark(void)
{
	static GQuark quark = 0;

	if (quark == 0)
		quark = g_quark_from_static_string("imsettings-error-quark");

	return quark;
}

/**
 * imsettings_get_interface_info:
 *
 * FIXME
 *
 * Returns:
 */
GDBusInterfaceInfo *
imsettings_get_interface_info(void)
{
	static gsize has_info = 0;
	static GDBusInterfaceInfo *info = NULL;

	if (g_once_init_enter(&has_info)) {
		GError *err = NULL;
		GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &err);

		if (err) {
			g_warning(err->message);
			return NULL;
		}
		info = g_dbus_interface_info_ref(introspection_data->interfaces[0]);
		g_dbus_node_info_unref(introspection_data);

		g_once_init_leave(&has_info, 1);
	}

	return info;
}
