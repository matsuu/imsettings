/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-client.c
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include "imsettings.h"
#include "imsettings-utils.h"
#include "imsettings-client.h"

#define IMSETTINGS_CLIENT_GET_PRIVATE(_o_)	(G_TYPE_INSTANCE_GET_PRIVATE ((_o_), IMSETTINGS_TYPE_CLIENT, IMSettingsClientPrivate))


G_DEFINE_TYPE (IMSettingsClient, imsettings_client, G_TYPE_OBJECT);

struct _IMSettingsClientPrivate {
	GDBusProxy *proxy;
	gchar      *locale;
};
enum {
	PROP_0,
	PROP_LOCALE,
	LAST_PROP
};

/*< private >*/
static GDBusProxy *
imsettings_client_get_proxy(IMSettingsClient *client)
{
	IMSettingsClientPrivate *priv = client->priv;
	GDBusConnection *connection;
	GError *err = NULL;

	if (priv->proxy) {
		connection = g_dbus_proxy_get_connection(priv->proxy);
		if (g_dbus_connection_is_closed(connection)) {
			g_object_unref(priv->proxy);
			goto create;
		}
	} else {
	  create:
		priv->proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
							    G_DBUS_PROXY_FLAGS_NONE,
							    imsettings_get_interface_info(),
							    IMSETTINGS_SERVICE_DBUS,
							    IMSETTINGS_PATH_DBUS,
							    IMSETTINGS_INTERFACE_DBUS,
							    NULL,
							    &err);
	}
	if (err) {
		g_warning("%s", err->message);
		g_error_free(err);
	}

	return priv->proxy;
}

static void
imsettings_client_set_property(GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	switch (prop_id) {
	    case PROP_LOCALE:
		    imsettings_client_set_locale(IMSETTINGS_CLIENT (object),
						 g_value_get_string(value));
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_client_get_property(GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
	switch (prop_id) {
	    case PROP_LOCALE:
		    g_value_set_string(value,
				       imsettings_client_get_locale(IMSETTINGS_CLIENT (object)));
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_client_finalize(GObject *object)
{
	IMSettingsClient *client = IMSETTINGS_CLIENT (object);
	IMSettingsClientPrivate *priv = client->priv;

	if (priv->proxy)
		g_object_unref(priv->proxy);
	g_free(priv->locale);

	if (G_OBJECT_CLASS (imsettings_client_parent_class)->finalize)
		G_OBJECT_CLASS (imsettings_client_parent_class)->finalize(object);
}

static void
imsettings_client_class_init(IMSettingsClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private(klass, sizeof (IMSettingsClientPrivate));

	object_class->set_property = imsettings_client_set_property;
	object_class->get_property = imsettings_client_get_property;
	object_class->finalize     = imsettings_client_finalize;

	/* properties */
	g_object_class_install_property(object_class, PROP_LOCALE,
					g_param_spec_string("locale",
							    _("Locale"),
							    _("Locale to get the imsettings information"),
							    NULL,
							    G_PARAM_READWRITE));
}

static void
imsettings_client_init(IMSettingsClient *client)
{
	IMSettingsClientPrivate *priv;

	priv = client->priv = IMSETTINGS_CLIENT_GET_PRIVATE (client);

	priv->proxy = NULL;
	priv->locale = NULL;
}

G_INLINE_FUNC gboolean
imsettings_client_async_result_boolean(IMSettingsClient  *client,
				       GAsyncResult      *result,
				       GError           **error)
{
	GDBusProxy *proxy;
	gboolean retval = FALSE;
	GVariant *value;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_finish(proxy, result, error);
	if (value != NULL) {
		g_variant_get(value, "(b)", &retval);
		g_variant_unref(value);
	}

	return retval;
}

G_INLINE_FUNC gchar *
imsettings_client_async_result_string(IMSettingsClient  *client,
				      GAsyncResult      *result,
				      GError           **error)
{
	GDBusProxy *proxy;
	gchar *retval = NULL;
	GVariant *value;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_finish(proxy, result, error);
	if (value != NULL) {
		g_variant_get(value, "(s)", &retval);
		g_variant_unref(value);
	}

	return retval;
}

G_INLINE_FUNC GVariant *
imsettings_client_async_result_variant(IMSettingsClient  *client,
				       GAsyncResult      *result,
				       GError           **error)
{
	GDBusProxy *proxy;
	GVariant *value, *retval = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_finish(proxy, result, error);
	if (value != NULL) {
		retval = g_variant_get_child_value(value, 0);
		g_variant_unref(value);
	}

	return retval;
}

/*< public >*/

/**
 * imsettings_client_new:
 * @locale:
 *
 * FIXME
 *
 * Returns:
 */
IMSettingsClient *
imsettings_client_new(const gchar   *locale)
{
	return IMSETTINGS_CLIENT (g_object_new(IMSETTINGS_TYPE_CLIENT,
					       "locale", locale, NULL));
}

/**
 * imsettings_client_set_locale:
 * @client:
 * @locale:
 *
 * FIXME
 *
 * Returns:
 */
gboolean
imsettings_client_set_locale(IMSettingsClient *client,
			     const gchar      *locale)
{
	IMSettingsClientPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), FALSE);

	priv = client->priv;
	if (locale) {
		gchar *cl = g_strdup(setlocale(LC_CTYPE, NULL));

		if (setlocale(LC_CTYPE, locale) == NULL) {
			g_free(cl);
			return FALSE;
		}

		setlocale(LC_CTYPE, cl);
		g_free(cl);
	}
	g_free(priv->locale);
	priv->locale = g_strdup(locale);

	g_object_notify(G_OBJECT (client), "locale");

	return TRUE;
}

/**
 * imsettings_client_get_locale:
 * @client:
 *
 * FIXME
 *
 * Returns:
 */
const gchar *
imsettings_client_get_locale(IMSettingsClient *client)
{
	IMSettingsClientPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), NULL);

	priv = client->priv;
	if (!priv->locale)
		return setlocale(LC_CTYPE, NULL);

	return priv->locale;
}

/**
 * imsettings_client_get_version:
 * @client:
 * @cancellable:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
guint
imsettings_client_get_version(IMSettingsClient  *client,
			      GCancellable      *cancellable,
			      GError           **error)
{
	GDBusProxy *proxy;
	guint retval = 0;
	GVariant *value;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), 0);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_sync(proxy,
				       "GetVersion",
				       NULL,
				       G_DBUS_CALL_FLAGS_NONE,
				       -1,
				       cancellable,
				       error);
	if (value != NULL) {
		g_variant_get(value, "(u)", &retval);
		g_variant_unref(value);
	}

	return retval;
}

/**
 * imsettings_client_get_info_variants:
 * @client:
 * @cancellable:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
GVariant *
imsettings_client_get_info_variants(IMSettingsClient  *client,
				    GCancellable      *cancellable,
				    GError           **error)
{
	GDBusProxy *proxy;
	GVariant *value, *retval = NULL;
	GError *err = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), NULL);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_sync(proxy,
				       "GetInfoVariants",
				       g_variant_new("(s)",
						     imsettings_client_get_locale(client)),
				       G_DBUS_CALL_FLAGS_NONE,
				       -1,
				       cancellable,
				       &err);
	if (value != NULL) {
		retval = g_variant_get_child_value(value, 0);
		g_variant_unref(value);
	}
	if (err) {
		if (error) {
			*error = g_error_copy(err);
		} else {
			g_warning("%s", err->message);
		}
		g_error_free(err);
	}

	return retval;
}

/**
 * imsettings_client_get_info_variants_start:
 * @client:
 * @cancellable:
 * @callback:
 * @user_data:
 *
 * FIXME
 */
void
imsettings_client_get_info_variants_start(IMSettingsClient    *client,
					  GCancellable        *cancellable,
					  GAsyncReadyCallback  callback,
					  gpointer             user_data)
{
	GDBusProxy *proxy;

	g_return_if_fail (IMSETTINGS_IS_CLIENT (client));

	proxy = imsettings_client_get_proxy(client);
	g_dbus_proxy_call(proxy,
			  "GetInfoVariants",
			  g_variant_new("(s)",
					imsettings_client_get_locale(client)),
			  G_DBUS_CALL_FLAGS_NONE,
			  -1,
			  cancellable,
			  callback,
			  user_data);
}

/**
 * imsettings_client_get_info_variants_finish:
 * @client:
 * @result:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gpointer
imsettings_client_get_info_variants_finish(IMSettingsClient  *client,
					   GAsyncResult      *result,
					   GError           **error)
{
	return imsettings_client_async_result_variant(client, result, error);
}

/**
 * imsettings_client_get_info_object:
 * @client:
 * @module:
 * @cancellable:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
IMSettingsInfo *
imsettings_client_get_info_object(IMSettingsClient  *client,
				  const gchar       *module,
				  GCancellable      *cancellable,
				  GError           **error)
{
	IMSettingsInfo *retval = NULL;
	GVariant *v;

	v = imsettings_client_get_info_variant(client, module, cancellable, error);
	if (v) {
		retval = imsettings_info_new(v);
		g_variant_unref(v);
	}

	return retval;
}

/**
 * imsettings_client_get_info_variant:
 * @client:
 * @module:
 * @cancellable:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
GVariant *
imsettings_client_get_info_variant(IMSettingsClient  *client,
				   const gchar       *module,
				   GCancellable      *cancellable,
				   GError           **error)
{
	GDBusProxy *proxy;
	GVariant *value, *retval = NULL;
	GError *err = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), NULL);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_sync(proxy,
				       "GetInfoVariant",
				       g_variant_new("(ss)",
						     imsettings_client_get_locale(client),
						     module),
				       G_DBUS_CALL_FLAGS_NONE,
				       -1,
				       cancellable,
				       &err);
	if (value != NULL) {
		retval = g_variant_get_child_value(value, 0);
		g_variant_unref(value);
	}
	if (err) {
		if (error) {
			*error = g_error_copy(err);
		} else {
			g_warning("%s", err->message);
		}
		g_error_free(err);
	}

	return retval;
}

/**
 * imsettings_client_get_info_variant_start:
 * @client:
 * @module:
 * @cancellable:
 * @callback:
 * @user_data:
 *
 * FIXME
 */
void
imsettings_client_get_info_variant_start(IMSettingsClient    *client,
					 const gchar         *module,
					 GCancellable        *cancellable,
					 GAsyncReadyCallback  callback,
					 gpointer             user_data)
{
	GDBusProxy *proxy;

	g_return_if_fail (IMSETTINGS_IS_CLIENT (client));

	proxy = imsettings_client_get_proxy(client);
	g_dbus_proxy_call(proxy,
			  "GetInfoVariant",
			  g_variant_new("(ss)",
					imsettings_client_get_locale(client),
					module),
			  G_DBUS_CALL_FLAGS_NONE,
			  -1,
			  cancellable,
			  callback,
			  user_data);
}

/**
 * imsettings_client_get_info_variant_finish:
 * @client:
 * @result:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
GVariant *
imsettings_client_get_info_variant_finish(IMSettingsClient  *client,
					  GAsyncResult      *result,
					  GError           **error)
{
	return imsettings_client_async_result_variant(client, result, error);
}

/**
 * imsettings_client_get_user_im:
 * @client:
 * @cancellable:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gchar *
imsettings_client_get_user_im(IMSettingsClient  *client,
			      GCancellable      *cancellable,
			      GError           **error)
{
	GDBusProxy *proxy;
	GVariant *value;
	gchar *retval = NULL;
	GError *err = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), NULL);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_sync(proxy,
				       "GetUserIM",
				       g_variant_new("(s)",
						     imsettings_client_get_locale(client)),
				       G_DBUS_CALL_FLAGS_NONE,
				       -1,
				       cancellable,
				       &err);
	if (value != NULL) {
		g_variant_get(value, "(s)", &retval);
		g_variant_unref(value);
	}
	if (err) {
		if (error) {
			*error = g_error_copy(err);
		} else {
			g_warning("%s", err->message);
		}
		g_error_free(err);
	}

	return retval;
}

/**
 * imsettings_client_get_user_im_start:
 * @client:
 * @cancellable:
 * @callback:
 * @user_data:
 *
 * FIXME
 */
void
imsettings_client_get_user_im_start(IMSettingsClient    *client,
				    GCancellable        *cancellable,
				    GAsyncReadyCallback  callback,
				    gpointer             user_data)
{
	GDBusProxy *proxy;

	g_return_if_fail (IMSETTINGS_IS_CLIENT (client));

	proxy = imsettings_client_get_proxy(client);
	g_dbus_proxy_call(proxy,
			  "GetUserIM",
			  g_variant_new("(s)",
					imsettings_client_get_locale(client)),
			  G_DBUS_CALL_FLAGS_NONE,
			  -1,
			  cancellable,
			  callback,
			  user_data);
}

/**
 * imsettings_client_get_user_im_finish:
 * @client:
 * @result:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gchar *
imsettings_client_get_user_im_finish(IMSettingsClient  *client,
				     GAsyncResult      *result,
				     GError           **error)
{
	return imsettings_client_async_result_string(client, result, error);
}

/**
 * imsettings_client_get_system_im:
 * @client:
 * @cancellable:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gchar *
imsettings_client_get_system_im(IMSettingsClient  *client,
				GCancellable      *cancellable,
				GError           **error)
{
	GDBusProxy *proxy;
	GVariant *value;
	gchar *retval = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), NULL);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_sync(proxy,
				       "GetSystemIM",
				       g_variant_new("(s)",
						     imsettings_client_get_locale(client)),
				       G_DBUS_CALL_FLAGS_NONE,
				       -1,
				       cancellable,
				       error);
	if (value != NULL) {
		g_variant_get(value, "(s)", &retval);
		g_variant_unref(value);
	}

	return retval;
}

/**
 * imsettings_client_get_system_im_start:
 * @client:
 * @cancellable:
 * @callback:
 * @user_data:
 *
 * FIXME
 */
void
imsettings_client_get_system_im_start(IMSettingsClient    *client,
				      GCancellable        *cancellable,
				      GAsyncReadyCallback  callback,
				      gpointer             user_data)
{
	GDBusProxy *proxy;

	g_return_if_fail (IMSETTINGS_IS_CLIENT (client));

	proxy = imsettings_client_get_proxy(client);
	g_dbus_proxy_call(proxy,
			  "GetSystemIM",
			  g_variant_new("(s)",
					imsettings_client_get_locale(client)),
			  G_DBUS_CALL_FLAGS_NONE,
			  -1,
			  cancellable,
			  callback,
			  user_data);
}

/**
 * imsettings_client_get_system_im_finish:
 * @client:
 * @result:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gchar *
imsettings_client_get_system_im_finish(IMSettingsClient  *client,
				       GAsyncResult      *result,
				       GError           **error)
{
	return imsettings_client_async_result_string(client, result, error);
}

/**
 * imsettings_client_switch_im:
 * @client:
 * @module:
 * @update_xinputrc:
 * @cancellable:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gboolean
imsettings_client_switch_im(IMSettingsClient  *client,
			    const gchar       *module,
			    gboolean           update_xinputrc,
			    GCancellable      *cancellable,
			    GError           **error)
{
	GDBusProxy *proxy;
	gboolean retval = FALSE;
	GVariant *value;
	gchar *m;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), FALSE);

	if (module == NULL || module[0] == 0)
		m = g_strdup(IMSETTINGS_NONE_CONF);
	else
		m = g_strdup(module);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_sync(proxy,
				       "SwitchIM",
				       g_variant_new("(ssb)",
						     imsettings_client_get_locale(client),
						     m, update_xinputrc),
				       G_DBUS_CALL_FLAGS_NONE,
				       -1,
				       cancellable,
				       error);
	if (value != NULL) {
		g_variant_get(value, "(b)", &retval);
		g_variant_unref(value);
	}
	g_free(m);

	return retval;

}

/**
 * imsettings_client_switch_im_start:
 * @client:
 * @module:
 * @update_xinputrc:
 * @cancellable:
 * @callback:
 * @user_data
 *
 * FIXME
 */
void
imsettings_client_switch_im_start(IMSettingsClient    *client,
				  const gchar         *module,
				  gboolean             update_xinputrc,
				  GCancellable        *cancellable,
				  GAsyncReadyCallback  callback,
				  gpointer             user_data)
{
	GDBusProxy *proxy;
	gchar *m;

	g_return_if_fail (IMSETTINGS_IS_CLIENT (client));

	if (module == NULL || module[0] == 0)
		m = g_strdup(IMSETTINGS_NONE_CONF);
	else
		m = g_strdup(module);

	proxy = imsettings_client_get_proxy(client);
	g_dbus_proxy_call(proxy,
			  "SwitchIM",
			  g_variant_new("(ssb)",
					imsettings_client_get_locale(client),
					m, update_xinputrc),
			  G_DBUS_CALL_FLAGS_NONE,
			  -1,
			  cancellable,
			  callback,
			  user_data);
	g_free(m);
}

/**
 * imsettings_client_switch_im_finish:
 * @client:
 * @result:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gboolean
imsettings_client_switch_im_finish(IMSettingsClient  *client,
				   GAsyncResult      *result,
				   GError           **error)
{
	return imsettings_client_async_result_boolean(client, result, error);
}

/**
 * imsettings_client_get_active_im_info:
 * @client:
 * @cancellable:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
IMSettingsInfo *
imsettings_client_get_active_im_info(IMSettingsClient  *client,
				     GCancellable      *cancellable,
				     GError           **error)
{
	GDBusProxy *proxy;
	IMSettingsInfo *retval = NULL;
	GVariant *value, *v;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), NULL);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_sync(proxy,
				       "GetActiveVariant",
				       NULL,
				       G_DBUS_CALL_FLAGS_NONE,
				       -1,
				       cancellable,
				       error);
	if (value != NULL) {
		v = g_variant_get_child_value(value, 0);
		g_variant_unref(value);
		retval = imsettings_info_new(v);
	}

	return retval;
}

/**
 * imsettings_client_im_is_system_default:
 * @client:
 * @module:
 * @cancellable:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gboolean
imsettings_client_im_is_system_default(IMSettingsClient  *client,
				       const gchar       *module,
				       GCancellable      *cancellable,
				       GError           **error)
{
	GDBusProxy *proxy;
	gboolean retval = FALSE;
	GVariant *value;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (module != NULL, FALSE);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_sync(proxy,
				       "IsSystemDefault",
				       g_variant_new("(ss)",
						     imsettings_client_get_locale(client),
						     module),
				       G_DBUS_CALL_FLAGS_NONE,
				       -1,
				       cancellable,
				       error);
	if (value != NULL) {
		g_variant_get(value, "(b)", &retval);
		g_variant_unref(value);
	}

	return retval;
}

/**
 * imsettings_client_im_is_user_default:
 * @client:
 * @module:
 * @cancellable:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gboolean
imsettings_client_im_is_user_default(IMSettingsClient  *client,
				     const gchar       *module,
				     GCancellable      *cancellable,
				     GError           **error)
{
	GDBusProxy *proxy;
	gboolean retval = FALSE;
	GVariant *value;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (module != NULL, FALSE);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_sync(proxy,
				       "IsUserDefault",
				       g_variant_new("(ss)",
						     imsettings_client_get_locale(client),
						     module),
				       G_DBUS_CALL_FLAGS_NONE,
				       -1,
				       cancellable,
				       error);
	if (value != NULL) {
		g_variant_get(value, "(b)", &retval);
		g_variant_unref(value);
	}

	return retval;
}

/**
 * imsettings_client_im_is_xim:
 * @client:
 * @module:
 * @cancellable:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gboolean
imsettings_client_im_is_xim(IMSettingsClient  *client,
			    const gchar       *module,
			    GCancellable      *cancellable,
			    GError           **error)
{
	GDBusProxy *proxy;
	gboolean retval = FALSE;
	GVariant *value;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (module != NULL, FALSE);

	proxy = imsettings_client_get_proxy(client);
	value = g_dbus_proxy_call_sync(proxy,
				       "IsXIM",
				       g_variant_new("(ss)",
						     imsettings_client_get_locale(client),
						     module),
				       G_DBUS_CALL_FLAGS_NONE,
				       -1,
				       cancellable,
				       error);
	if (value != NULL) {
		g_variant_get(value, "(b)", &retval);
		g_variant_unref(value);
	}

	return retval;
}

/**
 * imsettings_client_reload:
 * @client:
 * @cancellable:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gboolean
imsettings_client_reload(IMSettingsClient  *client,
			 gboolean           send_signal,
			 GCancellable      *cancellable,
			 GError           **error)
{
	GDBusProxy *proxy;

	g_return_val_if_fail (IMSETTINGS_IS_CLIENT (client), FALSE);

	proxy = imsettings_client_get_proxy(client);
	if (send_signal) {
		GDBusConnection *connection;

		g_clear_error(error);
		/* try to send a signal only. */
		connection = g_dbus_proxy_get_connection(proxy);
		if (!g_dbus_connection_emit_signal(connection,
						   IMSETTINGS_SERVICE_DBUS,
						   IMSETTINGS_PATH_DBUS,
						   IMSETTINGS_INTERFACE_DBUS,
						   "Reload",
						   g_variant_new("(b)", TRUE),
						   error))
			return FALSE;
		if (!g_dbus_connection_emit_signal(connection,
						   "com.redhat.imsettings.GConf",
						   "/com/redhat/imsettings/GConf",
						   "com.redhat.imsettings.GConf",
						   "Reload",
						   g_variant_new("(b)", TRUE),
						   error))
			return FALSE;
		sleep(3);
	} else {
		GVariant *value;
		gboolean retval = FALSE;

		value = g_dbus_proxy_call_sync(proxy,
					       "StopService", NULL,
					       G_DBUS_CALL_FLAGS_NONE,
					       -1,
					       cancellable,
					       error);
		if (value != NULL) {
			g_variant_get(value, "(b)", &retval);
			g_variant_unref(value);
		}
		return retval;
	}

	return TRUE;
}
