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
#include <gio/gio.h>

G_BEGIN_DECLS

#define imsettings_n_pad4(_n_) ((4 - ((_n_) % 4)) % 4)
#define imsettings_pad4(_s_,_n_)					\
	G_STMT_START {							\
		gint __n__ = imsettings_n_pad4 (_n_);			\
		gint __i__;						\
									\
		for (__i__ = 0; __i__ < __n__; __i__++)			\
			g_data_output_stream_put_byte((_s_), 0, NULL, NULL); \
	} G_STMT_END
#define imsettings_skip_pad4(_s_,_n_)					\
	G_STMT_START {							\
		gint __n__ = imsettings_n_pad4 (_n_);			\
		gint __i__;						\
									\
		for (__i__ = 0; __i__ < __n__; __i__++)			\
			g_data_input_stream_read_byte((_s_), NULL, NULL); \
	} G_STMT_END
#define imsettings_swap16(_o_,_v_)					\
	(imsettings_object_get_byte_order(_o_) != imsettings_get_host_byte_order() ? \
	 (imsettings_get_host_byte_order() == IMSETTINGS_OBJECT_MSB ?	\
	  GINT16_TO_BE (_v_) :						\
	  GINT16_TO_LE (_v_)) :						\
	 (_v_))
#define imsettings_swapu16(_o_,_v_)					\
	(imsettings_object_get_byte_order(_o_) != imsettings_get_host_byte_order() ? \
	 (imsettings_get_host_byte_order() == IMSETTINGS_OBJECT_MSB ?	\
	  GUINT16_TO_BE (_v_) :						\
	  GUINT16_TO_LE (_v_)) :					\
	 (_v_))
#define imsettings_swap32(_o_,_v_)					\
	(imsettings_object_get_byte_order(_o_) != imsettings_get_host_byte_order() ? \
	 (imsettings_get_host_byte_order() == IMSETTINGS_OBJECT_MSB ?	\
	  GINT32_TO_BE (_v_) :						\
	  GINT32_TO_LE (_v_)) :						\
	 (_v_))
#define imsettings_swapu32(_o_,_v_)					\
	(imsettings_object_get_byte_order(_o_) != imsettings_get_host_byte_order() ? \
	 (imsettings_get_host_byte_order() == IMSETTINGS_OBJECT_MSB ?	\
	  GUINT32_TO_BE (_v_) :						\
	  GUINT32_TO_LE (_v_)) :					\
	 (_v_))
#define imsettings_swap64(_o_,_v_)					\
	(imsettings_object_get_byte_order(_o_) != imsettings_get_host_byte_order() ? \
	 (imsettings_get_host_byte_order() == IMSETTINGS_OBJECT_MSB ?	\
	  GINT64_TO_BE (_v_) :						\
	  GINT64_TO_LE (_v_)) :						\
	 (_v_))
#define imsettings_swapu64(_o_,_v_)					\
	(imsettings_object_get_byte_order(_o_) != imsettings_get_host_byte_order() ? \
	 (imsettings_get_host_byte_order() == IMSETTINGS_OBJECT_MSB ?	\
	  GUINT64_TO_BE (_v_) :						\
	  GUINT64_TO_LE (_v_)) :					\
	 (_v_))

#define IMSETTINGS_GERROR	(imsettings_g_error_quark())


enum {
	IMSETTINGS_GERROR_NOT_YET_IMPLEMENTED,
	IMSETTINGS_GERROR_UNKNOWN,
	IMSETTINGS_GERROR_CONFIGURATION_ERROR,
	IMSETTINGS_GERROR_IM_NOT_FOUND,
	IMSETTINGS_GERROR_NOT_AVAILABLE,
	IMSETTINGS_GERROR_INVALID_IMM,
	IMSETTINGS_GERROR_UNABLE_TO_TRACK_IM,
	IMSETTINGS_GERROR_OOM,
	IMSETTINGS_GERROR_FAILED,
};

GQuark              imsettings_g_error_quark                    (void);
GDBusInterfaceInfo *imsettings_get_interface_info               (void);
gchar              *imsettings_generate_dbus_path_from_interface(const gchar *interface) G_GNUC_MALLOC;

G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_UTILS_H__ */
