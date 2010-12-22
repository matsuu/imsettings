/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-client.h
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
#ifndef __IMSETTINGS_IMSETTINGS_CLIENT_H__
#define __IMSETTINGS_IMSETTINGS_CLIENT_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <imsettings/imsettings-info.h>


G_BEGIN_DECLS

#define IMSETTINGS_TYPE_CLIENT			(imsettings_client_get_type())
#define IMSETTINGS_CLIENT(_o_)			(G_TYPE_CHECK_INSTANCE_CAST ((_o_), IMSETTINGS_TYPE_CLIENT, IMSettingsClient))
#define IMSETTINGS_CLIENT_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), IMSETTINGS_TYPE_CLIENT, IMSettingsClientClass))
#define IMSETTINGS_CLIENT_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), IMSETTINGS_TYPE_CLIENT, IMSettingsClientClass))
#define IMSETTINGS_IS_CLIENT(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), IMSETTINGS_TYPE_CLIENT))
#define IMSETTINGS_IS_CLIENT_CLASS(_c_)		(G_TYPE_CHECK_CLASS_TYPE ((_c_), IMSETTINGS_TYPE_CLIENT))

typedef struct _IMSettingsClientClass	IMSettingsClientClass;
typedef struct _IMSettingsClient	IMSettingsClient;

struct _IMSettingsClientClass {
	GDBusProxyClass parent_class;

	void (*reserved1) (void);
	void (*reserved2) (void);
	void (*reserved3) (void);
	void (*reserved4) (void);
};
struct _IMSettingsClient {
	GDBusProxy  parent_instance;
	gchar      *locale;
};


GType             imsettings_client_get_type  (void) G_GNUC_CONST;
IMSettingsClient *imsettings_client_new       (const gchar       *locale,
					       GCancellable      *cancellable,
					       GError           **error);
gboolean          imsettings_client_set_locale(IMSettingsClient  *client,
					       const gchar       *locale);
const gchar      *imsettings_client_get_locale(IMSettingsClient  *client);

/* synchronous methods */

guint           imsettings_client_get_version         (IMSettingsClient  *client,
                                                       GCancellable      *cancellable,
                                                       GError           **error);
GVariant       *imsettings_client_get_info_variants   (IMSettingsClient  *client,
                                                       GCancellable      *cancellable,
                                                       GError           **error);
GVariant       *imsettings_client_get_info_variant    (IMSettingsClient  *client,
                                                       const gchar       *module,
                                                       GCancellable      *cancellable,
                                                       GError           **error);
gchar          *imsettings_client_get_user_im         (IMSettingsClient  *client,
                                                       GCancellable      *cancellable,
                                                       GError           **error);
gchar          *imsettings_client_get_system_im       (IMSettingsClient  *client,
                                                       GCancellable      *cancellable,
                                                       GError           **error);
IMSettingsInfo *imsettings_client_get_active_im_info  (IMSettingsClient  *client,
                                                       GCancellable      *cancellable,
                                                       GError           **error);
gboolean        imsettings_client_im_is_system_default(IMSettingsClient  *client,
                                                       const gchar       *module,
                                                       GCancellable      *cancellable,
                                                       GError           **error);
gboolean        imsettings_client_im_is_user_default  (IMSettingsClient  *client,
                                                       const gchar       *module,
                                                       GCancellable      *cancellable,
                                                       GError           **error);
gboolean        imsettings_client_im_is_xim           (IMSettingsClient  *client,
                                                       const gchar       *module,
                                                       GCancellable      *cancellable,
                                                       GError           **error);
gboolean        imsettings_client_switch_im           (IMSettingsClient  *client,
                                                       const gchar       *module,
                                                       gboolean           update_xinputrc,
                                                       GCancellable      *cancellable,
                                                       GError           **error);
gboolean        imsettings_client_reload              (IMSettingsClient  *client,
						       gboolean           send_signal,
                                                       GCancellable      *cancellable,
                                                       GError           **error);

/* asynchronous methods */

void      imsettings_client_get_info_variants_start (IMSettingsClient     *client,
						     GCancellable         *cancellable,
						     GAsyncReadyCallback   callback,
						     gpointer              user_data);
gpointer  imsettings_client_get_info_variants_finish(IMSettingsClient     *client,
						     GAsyncResult         *result,
						     GError              **error);
void      imsettings_client_get_info_variant_start  (IMSettingsClient     *client,
						     const gchar          *module,
						     GCancellable         *cancellable,
						     GAsyncReadyCallback   callback,
						     gpointer              user_data);
GVariant *imsettings_client_get_info_variant_finish (IMSettingsClient     *client,
						     GAsyncResult         *result,
						     GError              **error);
void      imsettings_client_get_user_im_start       (IMSettingsClient     *client,
						     GCancellable         *cancellable,
						     GAsyncReadyCallback   callback,
						     gpointer              user_data);
gchar    *imsettings_client_get_user_im_finish      (IMSettingsClient     *client,
						     GAsyncResult         *result,
						     GError              **error);
void      imsettings_client_get_system_im_start     (IMSettingsClient     *client,
						     GCancellable         *cancellable,
						     GAsyncReadyCallback   callback,
						     gpointer              user_data);
gchar    *imsettings_client_get_system_im_finish    (IMSettingsClient     *client,
						     GAsyncResult         *result,
						     GError              **error);
void      imsettings_client_switch_im_start         (IMSettingsClient     *client,
						     const gchar          *module,
						     gboolean              update_xinputrc,
						     GCancellable         *cancellable,
						     GAsyncReadyCallback   callback,
						     gpointer              user_data);
gboolean  imsettings_client_switch_im_finish        (IMSettingsClient     *client,
						     GAsyncResult         *result,
						     GError              **error);

G_END_DECLS

#endif /* __IMSETTINGS_IMSETTINGS_CLIENT_H__ */
