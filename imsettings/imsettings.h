/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsetting.h
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
#ifndef __IMSETTINGS_IMSETTINGS_H__
#define __IMSETTINGS_IMSETTINGS_H__

#include <glib.h>

G_BEGIN_DECLS

#define IMSETTINGS_SERVICE_DBUS		"com.redhat.imsettings"
#define IMSETTINGS_PATH_DBUS		"/com/redhat/imsettings"
#define IMSETTINGS_INTERFACE_DBUS	"com.redhat.imsettings"
#define IMSETTINGS_XIM_SERVICE_DBUS	"com.redhat.imsettings.xim"
#define IMSETTINGS_XIM_PATH_DBUS	"/com/redhat/imsettings/xim"
#define IMSETTINGS_XIM_INTERFACE_DBUS	"com.redhat.imsettings.xim"

#define IMSETTINGS_SETTINGS_API_VERSION	4

#ifdef GNOME_ENABLE_DEBUG
#define d(e)	e
#else
#define d(e)
#endif

/* global config file */
#define IMSETTINGS_GLOBAL_XINPUT_CONF		"xinputrc"
/* user config file */
#define IMSETTINGS_USER_XINPUT_CONF		".xinputrc"
/* config file use to be determined that IM is never used no matter what */
#define IMSETTINGS_NONE_CONF			"none"
/* config file use to be determined that IM is always used no matter what */
#define IMSETTINGS_XIM_CONF			"xim"
#define IMSETTINGS_USER_SPECIFIC_SHORT_DESC	N_("User Specific")
#define IMSETTINGS_USER_SPECIFIC_LONG_DESC	N_("xinputrc was modified by the user")

enum IMSettingsIMModuleType {
	IMSETTINGS_IMM_GTK,
	IMSETTINGS_IMM_QT,
	IMSETTINGS_IMM_XIM
};

G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_H__ */
