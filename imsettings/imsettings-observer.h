/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-observer.h
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
#ifndef __IMSETTINGS_IMSETTINGS_OBSERVER_H__
#define __IMSETTINGS_IMSETTINGS_OBSERVER_H__

#include <glib.h>
#include <glib-object.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <imsettings/imsettings-info.h>

G_BEGIN_DECLS

#define IMSETTINGS_TYPE_OBSERVER		(imsettings_observer_get_type())
#define IMSETTINGS_OBSERVER(_o_)		(G_TYPE_CHECK_INSTANCE_CAST ((_o_), IMSETTINGS_TYPE_OBSERVER, IMSettingsObserver))
#define IMSETTINGS_OBSERVER_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), IMSETTINGS_TYPE_OBSERVER, IMSettingsObserverClass))
#define IMSETTINGS_IS_OBSERVER(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), IMSETTINGS_TYPE_OBSERVER))
#define IMSETTINGS_IS_OBSERVER_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), IMSETTINGS_TYPE_OBSERVER))
#define IMSETTINGS_OBSERVER_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), IMSETTINGS_TYPE_OBSERVER, IMSettingsObserverClass))


typedef struct _IMSettingsObserverClass		IMSettingsObserverClass;
typedef struct _IMSettingsObserver		IMSettingsObserver;


struct _IMSettingsObserverClass {
	GObjectClass parent_class;

	/* dbus handlers */
	void              (* unregistered_handler) (DBusConnection      *connection,
						    void                *data);
	DBusHandlerResult (* message_handler)      (DBusConnection      *connection,
						    DBusMessage         *message,
						    void                *data);
	DBusHandlerResult (* message_filter)       (DBusConnection      *connection,
						    DBusMessage         *message,
						    void                *data);

	void              (* dh_reserved1)         (void);
	void              (* dh_reserved2)         (void);
	void              (* dh_reserved3)         (void);
	void              (* dh_reserved4)         (void);
	void              (* dh_reserved5)         (void);

	/* dbus signals */
	void              (* disconnected)         (IMSettingsObserver  *imsettings);

	void              (* ds_reserved1)         (void);
	void              (* ds_reserved2)         (void);
	void              (* ds_reserved3)         (void);
	void              (* ds_reserved4)         (void);
	void              (* ds_reserved5)         (void);
	void              (* ds_reserved6)         (void);
	void              (* ds_reserved7)         (void);

	/* dbus methods */
	gboolean          (* reload)               (IMSettingsObserver  *imsettings,
						    gboolean             force,
						    GError             **error);
	GPtrArray       * (* get_list)             (IMSettingsObserver  *imsettings,
						    const gchar         *lang,
						    GError             **error);
	const gchar     * (* get_current_user_im)  (IMSettingsObserver  *imsettings,
						    GError             **error);
	const gchar     * (* get_current_system_im)(IMSettingsObserver  *imsettings,
						    GError             **error);
	GPtrArray       * (* get_info_objects)     (IMSettingsObserver  *imsettings,
						    const gchar         *locale,
						    GError             **error);
	IMSettingsInfo  * (* get_info)             (IMSettingsObserver  *imsettings,
						    const gchar         *locale,
						    const gchar         *module,
						    GError             **error);
	gboolean          (* start_im)             (IMSettingsObserver  *imsettings,
						    const gchar         *lang,
						    const gchar         *module,
						    gboolean             update_xinputrc,
						    GError             **error);
	gboolean          (* stop_im)              (IMSettingsObserver  *imsettings,
						    const gchar         *module,
						    gboolean             update_xinputrc,
						    gboolean             force,
						    GError             **error);
	const gchar     * (* what_im_is_running)   (IMSettingsObserver  *imsettings,
						    GError             **error);
	guint             (* get_version)          (IMSettingsObserver  *imsettings,
						    GError             **error);

	void              (* dm_reserved1)         (void);
	void              (* dm_reserved2)         (void);
	void              (* dm_reserved3)         (void);
	void              (* dm_reserved4)         (void);
	void              (* dm_reserved5)         (void);
	void              (* dm_reserved6)         (void);

	/* signals */
	void              (* s_start_im)           (IMSettingsObserver  *imsettings,
						    const gchar         *module,
						    gboolean             update_xinputrc);
	void              (* s_stop_im)            (IMSettingsObserver  *imsettings,
						    const gchar         *module,
						    gboolean             update_xinputrc,
						    gboolean             force);

	void              (* s_reserved1)          (void);
	void              (* s_reserved2)          (void);

	void              (* reserved1)            (void);
	void              (* reserved2)            (void);
	void              (* reserved3)            (void);
	void              (* reserved4)            (void);
};

struct _IMSettingsObserver {
	GObject parent_instance;
};


GType               imsettings_observer_get_type(void) G_GNUC_CONST;
IMSettingsObserver *imsettings_observer_new     (DBusGConnection    *connection,
						 const gchar        *module);
gboolean            imsettings_observer_setup   (IMSettingsObserver *imsettings,
						 const gchar        *service);


G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_OBSERVER_H__ */
