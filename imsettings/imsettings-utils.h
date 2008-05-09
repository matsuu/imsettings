/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-utils.h
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
#ifndef __IMSETTINGS_IMSETTINGS_UTILS_H__
#define __IMSETTINGS_IMSETTINGS_UTILS_H__

#include <glib.h>
#include <dbus/dbus.h>

G_BEGIN_DECLS

#define IMSETTINGS_GERROR	(imsettings_g_error_quark())


enum {
	IMSETTINGS_GERROR_NOT_YET_IMPLEMENTED,
	IMSETTINGS_GERROR_UNKNOWN,
	IMSETTINGS_GERROR_NOT_AVAILABLE,
	IMSETTINGS_GERROR_IM_NOT_FOUND,
	IMSETTINGS_GERROR_INVALID_IMM,
	IMSETTINGS_GERROR_UNABLE_TO_TRACK_IM,
	IMSETTINGS_GERROR_OOM,
	IMSETTINGS_GERROR_FAILED,
};

GQuark  imsettings_g_error_quark                    (void);
gchar  *imsettings_generate_dbus_path_from_interface(const gchar    *interface) G_GNUC_MALLOC;

G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_UTILS_H__ */
