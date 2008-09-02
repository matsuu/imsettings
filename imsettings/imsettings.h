/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsetting.h
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
#ifndef __IMSETTINGS_IMSETTINGS_H__
#define __IMSETTINGS_IMSETTINGS_H__

#include <glib.h>

G_BEGIN_DECLS

#define IMSETTINGS_SERVICE_DBUS		"com.redhat.imsettings"
#define IMSETTINGS_PATH_DBUS		"/com/redhat/imsettings"
#define IMSETTINGS_INTERFACE_DBUS	"com.redhat.imsettings"
#define IMSETTINGS_INFO_SERVICE_DBUS	"com.redhat.imsettings.IMInfo"
#define IMSETTINGS_INFO_PATH_DBUS	"/com/redhat/imsettings/IMInfo"
#define IMSETTINGS_INFO_INTERFACE_DBUS	"com.redhat.imsettings.IMInfo"
#define IMSETTINGS_GCONF_SERVICE_DBUS	"com.redhat.imsettings.GConf"
#define IMSETTINGS_GCONF_PATH_DBUS	"/com/redhat/imsettings/GConf"
#define IMSETTINGS_GCONF_INTERFACE_DBUS	"com.redhat.imsettings.GConf"
#define IMSETTINGS_QT_SERVICE_DBUS	"com.redhat.imsettings.Qt"
#define IMSETTINGS_QT_PATH_DBUS		"/com/redhat/imsettings/Qt"
#define IMSETTINGS_QT_INTERFACE_DBUS	"com.redhat.imsettings.Qt"
#define IMSETTINGS_XIM_SERVICE_DBUS	"com.redhat.imsettings.XIM"
#define IMSETTINGS_XIM_PATH_DBUS	"/com/redhat/imsettings/XIM"
#define IMSETTINGS_XIM_INTERFACE_DBUS	"com.redhat.imsettings.XIM"

#define IMSETTINGS_SETTINGS_DAEMON_VERSION	2
#define IMSETTINGS_IMINFO_DAEMON_VERSION	2

enum IMSettingsIMModuleType {
	IMSETTINGS_IMM_GTK,
	IMSETTINGS_IMM_QT,
	IMSETTINGS_IMM_XIM
};

G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_H__ */
