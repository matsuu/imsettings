/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-request.h
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
#ifndef __IMSETTINGS_IMSETTINGS_REQUEST_H__
#define __IMSETTINGS_IMSETTINGS_REQUEST_H__

#include <glib-object.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <imsettings/imsettings-glib-bindings.h>
#include <imsettings/imsettings-info.h>

G_BEGIN_DECLS

#define IMSETTINGS_TYPE_REQUEST			(imsettings_request_get_type())
#define IMSETTINGS_REQUEST(_o_)			(G_TYPE_CHECK_INSTANCE_CAST ((_o_), IMSETTINGS_TYPE_REQUEST, IMSettingsRequest))
#define IMSETTINGS_REQUEST_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), IMSETTINGS_TYPE_REQUEST, IMSettingsRequestClass))
#define IMSETTINGS_IS_REQUEST(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), IMSETTINGS_TYPE_REQUEST))
#define IMSETTINGS_IS_REQUEST_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), IMSETTINGS_TYPE_REQUEST))
#define IMSETTINGS_REQUEST_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), IMSETTINGS_TYPE_REQUEST, IMSettingsRequestClass))


typedef struct _IMSettingsRequestClass	IMSettingsRequestClass;
typedef struct _IMSettingsRequest	IMSettingsRequest;

struct _IMSettingsRequestClass {
	GObjectClass parent_class;

	void (* reserved1) (void);
	void (* reserved2) (void);
	void (* reserved3) (void);
	void (* reserved4) (void);
};
struct _IMSettingsRequest {
	GObject parent_instance;
};


GType              imsettings_request_get_type               (void) G_GNUC_CONST;
IMSettingsRequest *imsettings_request_new                    (DBusConnection     *connection,
                                                              const gchar        *interface);
void               imsettings_request_set_locale             (IMSettingsRequest  *imsettings,
                                                              const gchar        *locale);
gchar*            *imsettings_request_get_im_list            (IMSettingsRequest  *imsettings,
                                                              GError            **error);
gchar             *imsettings_request_get_current_user_im    (IMSettingsRequest  *imsettings,
                                                              GError            **error);
gchar             *imsettings_request_get_current_system_im  (IMSettingsRequest  *imsettings,
                                                              GError            **error);
GPtrArray         *imsettings_request_get_info_objects       (IMSettingsRequest  *imsettings,
                                                              GError            **error);
IMSettingsInfo    *imsettings_request_get_info_object        (IMSettingsRequest  *imsettings,
							      const gchar        *module,
							      GError            **error);
gboolean           imsettings_request_start_im               (IMSettingsRequest  *imsettings,
                                                              const gchar        *module,
                                                              gboolean            update_xinputrc,
                                                              GError            **error);
gboolean           imsettings_request_stop_im                (IMSettingsRequest  *imsettings,
                                                              const gchar        *module,
                                                              gboolean            update_xinputrc,
                                                              gboolean            force,
                                                              GError            **error);
gchar             *imsettings_request_what_im_is_running     (IMSettingsRequest  *imsettings,
                                                              GError            **error);
gboolean           imsettings_request_reload                 (IMSettingsRequest  *imsettings,
                                                              gboolean            force);
gboolean           imsettings_request_change_to              (IMSettingsRequest  *imsettings,
                                                              const gchar        *module,
                                                              GError            **error);
gboolean           imsettings_request_change_to_with_signal  (IMSettingsRequest  *imsettings,
                                                              const gchar        *module);
guint              imsettings_request_get_version            (IMSettingsRequest  *imsettings,
							      GError            **error);

gboolean imsettings_request_get_im_list_async(IMSettingsRequest                           *imsettings,
                                              com_redhat_imsettings_IMInfo_get_list_reply  callback,
                                              gpointer                                     user_data);
gboolean imsettings_request_start_im_async   (IMSettingsRequest                           *imsettings,
                                              const gchar                                 *module,
                                              gboolean                                     update_xinputrc,
                                              com_redhat_imsettings_start_im_reply         callback,
                                              gpointer                                     user_data);
gboolean imsettings_request_stop_im_async    (IMSettingsRequest                           *imsettings,
                                              const gchar                                 *module,
                                              gboolean                                     update_xinputrc,
                                              gboolean                                     force,
                                              com_redhat_imsettings_stop_im_reply          callback,
                                              gpointer                                     user_data);


G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_REQUEST_H__ */
