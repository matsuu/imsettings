/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * monitor.h
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
#ifndef __IMSETTINGS_IMSETTINGS_MONITOR_H__
#define __IMSETTINGS_IMSETTINGS_MONITOR_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <imsettings/imsettings-info.h>

G_BEGIN_DECLS

#define IMSETTINGS_TYPE_MONITOR			(imsettings_monitor_get_type())
#define IMSETTINGS_MONITOR(_o_)			(G_TYPE_CHECK_INSTANCE_CAST ((_o_), IMSETTINGS_TYPE_MONITOR, IMSettingsMonitor))
#define IMSETTINGS_MONITOR_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), IMSETTINGS_TYPE_MONITOR, IMSettingsMonitorClass))
#define IMSETTINGS_IS_MONITOR(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), IMSETTINGS_TYPE_MONITOR))
#define IMSETTINGS_IS_MONITOR_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), IMSETTINGS_TYPE_MONITOR))
#define IMSETTINGS_MONITOR_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), IMSETTINGS_TYPE_MONITOR, IMSettingsMonitorClass))

#define IMSETTINGS_MONITOR_ERROR		(imsettings_monitor_get_error_quark())


typedef struct _IMSettingsMonitorClass		IMSettingsMonitorClass;
typedef struct _IMSettingsMonitor		IMSettingsMonitor;
typedef enum _IMSettingsMonitorError		IMSettingsMonitorError;

struct _IMSettingsMonitorClass {
	GObjectClass  parent_class;

	void (* status_changed) (IMSettingsMonitor *monitor,
				 const gchar       *filename);
};
struct _IMSettingsMonitor {
	GObject       parent_instance;
	gchar        *xinputrcdir;
	gchar        *xinputdir;
	gchar        *homedir;
	gchar        *current_system_im;
	gchar        *current_user_im;
	GFileMonitor *mon_xinputd;
	GHashTable   *im_info_from_name;
	GHashTable   *im_info_from_filename;
	GHashTable   *file_list_for_xinputrc;
	GHashTable   *file_list_for_dot_xinputrc;
	GPtrArray    *mon_xinputrc;
	GPtrArray    *mon_dot_xinputrc;
};
enum _IMSettingsMonitorError {
	IMSETTINGS_MONITOR_ERROR_DANGLING_SYMLINK,
	IMSETTINGS_MONITOR_ERROR_NOT_AVAILABLE,
};


GType              imsettings_monitor_get_type             (void) G_GNUC_CONST;
GQuark             imsettings_monitor_get_error_quark      (void);
IMSettingsMonitor *imsettings_monitor_new                  (const gchar       *xinputrcdir,
                                                            const gchar       *xinputdir,
                                                            const gchar       *homedir);
void               imsettings_monitor_start                (IMSettingsMonitor *monitor);
void               imsettings_monitor_stop                 (IMSettingsMonitor *monitor);
GPtrArray         *imsettings_monitor_foreach              (IMSettingsMonitor *monitor,
                                                            const gchar       *locale);
void               imsettings_monitor_array_free           (GPtrArray         *array);
IMSettingsInfo    *imsettings_monitor_lookup               (IMSettingsMonitor *monitor,
                                                            const gchar       *module,
                                                            const gchar       *locale,
                                                            GError            **error);
gchar             *imsettings_monitor_get_current_user_im  (IMSettingsMonitor *monitor,
                                                            GError            **error);
gchar             *imsettings_monitor_get_current_system_im(IMSettingsMonitor *monitor,
                                                            GError            **error);
gchar             *imsettings_monitor_get_shortname        (IMSettingsMonitor *monitor,
							    const gchar       *filename);

G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_MONITOR_H__ */
