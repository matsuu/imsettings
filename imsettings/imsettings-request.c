/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-request.c
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib-bindings.h>
#include "imsettings-request.h"
#include "imsettings.h"
#include "imsettings-utils.h"

#define IMSETTINGS_REQUEST_GET_PRIVATE(_o_)	(G_TYPE_INSTANCE_GET_PRIVATE ((_o_), IMSETTINGS_TYPE_REQUEST, IMSettingsRequestPrivate))

typedef struct _IMSettingsRequestPrivate {
	DBusConnection *connection;
	DBusGProxy     *proxy;
	gchar          *interface;
	gchar          *path;
	gchar          *locale;
} IMSettingsRequestPrivate;

enum {
	PROP_0,
	PROP_INTERFACE,
	PROP_CONNECTION,
	PROP_LOCALE,
	LAST_PROP
};


G_DEFINE_TYPE (IMSettingsRequest, imsettings_request, G_TYPE_OBJECT);

/*
 * Private functions
 */
static void
imsettings_request_connect_to(IMSettingsRequest *imsettings)
{
	IMSettingsRequestPrivate *priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);

	if (!priv->connection || !priv->interface)
		return;
	if (priv->proxy) {
		g_object_unref(priv->proxy);
	}
	dbus_connection_setup_with_g_main(priv->connection, NULL);

	priv->proxy = dbus_g_proxy_new_for_name(dbus_connection_get_g_connection(priv->connection),
						priv->interface,
						priv->path,
						priv->interface);
	dbus_g_connection_register_g_object(dbus_connection_get_g_connection(priv->connection),
					    priv->path,
					    G_OBJECT (imsettings));
}

static void
imsettings_request_set_property(GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	IMSettingsRequestPrivate *priv = IMSETTINGS_REQUEST_GET_PRIVATE (object);

	switch (prop_id) {
	    case PROP_INTERFACE:
		    g_free(priv->interface);
		    g_free(priv->path);
		    priv->interface = g_strdup(g_value_get_string(value));
		    priv->path = imsettings_generate_dbus_path_from_interface(priv->interface);
		    imsettings_request_connect_to(IMSETTINGS_REQUEST (object));
		    break;
	    case PROP_CONNECTION:
		    /* XXX: do we need to close the dbus connection here? */
		    priv->connection = (DBusConnection *)g_value_get_boxed(value);
		    imsettings_request_connect_to(IMSETTINGS_REQUEST (object));
		    break;
	    case PROP_LOCALE:
		    imsettings_request_set_locale(IMSETTINGS_REQUEST (object),
						  g_value_get_string(value));
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_request_get_property(GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
	IMSettingsRequestPrivate *priv = IMSETTINGS_REQUEST_GET_PRIVATE (object);

	switch (prop_id) {
	    case PROP_INTERFACE:
		    g_value_set_string(value, priv->interface);
		    break;
	    case PROP_CONNECTION:
		    g_value_set_boxed(value, priv->connection);
		    break;
	    case PROP_LOCALE:
		    g_value_set_string(value, priv->locale);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_request_finalize(GObject *object)
{
	IMSettingsRequestPrivate *priv = IMSETTINGS_REQUEST_GET_PRIVATE (object);

	g_free(priv->interface);
	g_free(priv->path);
	g_free(priv->locale);
	if (priv->proxy)
		g_object_unref(priv->proxy);
	/* XXX: do we need to unref the dbus connection here? */

	if (G_OBJECT_CLASS (imsettings_request_parent_class)->finalize)
		G_OBJECT_CLASS (imsettings_request_parent_class)->finalize(object);
}

static void
imsettings_request_class_init(IMSettingsRequestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	static const DBusGObjectInfo dummy_info = {
		0, NULL, 0, "\0", "\0", "\0"
	};

	g_type_class_add_private(klass, sizeof (IMSettingsRequestPrivate));

	object_class->set_property = imsettings_request_set_property;
	object_class->get_property = imsettings_request_get_property;
	object_class->finalize = imsettings_request_finalize;

	/* properties */
	g_object_class_install_property(object_class, PROP_INTERFACE,
					g_param_spec_string("interface",
							    _("DBus Interface"),
							    _("Interface name to connect to DBus"),
							    IMSETTINGS_INTERFACE_DBUS,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_CONNECTION,
					g_param_spec_boxed("connection",
							   _("DBus connection"),
							   _("An object to be a DBus connection"),
							   DBUS_TYPE_CONNECTION,
							   G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_LOCALE,
					g_param_spec_string("locale",
							    _("Locale"),
							    _("Locale to get the imsettings information"),
							    NULL,
							    G_PARAM_READWRITE));

	dbus_g_object_type_install_info(IMSETTINGS_TYPE_REQUEST,
					&dummy_info);
}

static void
imsettings_request_init(IMSettingsRequest *imsettings)
{
}

/*
 * Public functions
 */
IMSettingsRequest *
imsettings_request_new(DBusConnection *connection,
		       const gchar    *interface)
{
	g_return_val_if_fail (connection != NULL, NULL);
	g_return_val_if_fail (interface != NULL, NULL);

	return (IMSettingsRequest *)g_object_new(IMSETTINGS_TYPE_REQUEST,
						 "interface", interface,
						 "connection", connection,
						 NULL);
}

void
imsettings_request_set_locale(IMSettingsRequest *imsettings,
			      const gchar       *locale)
{
	IMSettingsRequestPrivate *priv;

	g_return_if_fail (IMSETTINGS_IS_REQUEST (imsettings));

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	g_free(priv->locale);
	priv->locale = NULL;
	if (locale)
		priv->locale = g_strdup(locale);

	g_object_notify(G_OBJECT (imsettings), "locale");
}

gchar **
imsettings_request_get_im_list(IMSettingsRequest *imsettings)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gchar **retval = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), NULL);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_get_list(priv->proxy, priv->locale, &retval, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "GetList", error->message);
		g_error_free(error);
	}

	return retval;
}

gboolean
imsettings_request_get_im_list_async(IMSettingsRequest                           *imsettings,
				     com_redhat_imsettings_IMInfo_get_list_reply  callback,
				     gpointer                                     user_data)
{
	IMSettingsRequestPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);

	return com_redhat_imsettings_IMInfo_get_list_async(priv->proxy, priv->locale, callback, user_data) != NULL;
}

gchar *
imsettings_request_get_current_user_im(IMSettingsRequest *imsettings)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gchar *retval = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), NULL);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_get_current_user_im(priv->proxy, &retval, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "GetCurrentUserIM", error->message);
		g_error_free(error);
	}

	return retval;
}

gchar *
imsettings_request_get_current_system_im(IMSettingsRequest *imsettings)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gchar *retval = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), NULL);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_get_current_system_im(priv->proxy, &retval, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "GetCurrentSystemIM", error->message);
		g_error_free(error);
	}

	return retval;
}

gchar *
imsettings_request_get_xinput_filename(IMSettingsRequest *imsettings,
				       const gchar       *module)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gchar *retval = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), NULL);
	g_return_val_if_fail (module != NULL && module[0] != 0, NULL);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_get_xinput_filename(priv->proxy, module, &retval, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "GetFilename", error->message);
		g_error_free(error);
	}

	return retval;
}

gchar *
imsettings_request_get_im_module_name(IMSettingsRequest *imsettings,
				      const gchar       *module,
				      guint32            type)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gchar *retval = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), NULL);
	g_return_val_if_fail (module != NULL && module[0] != 0, NULL);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_get_im_module_name(priv->proxy, module, type, &retval, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "GetIMModuleName", error->message);
		g_error_free(error);
	}

	return retval;
}

gboolean
imsettings_request_get_xim_program(IMSettingsRequest *imsettings,
				   const gchar       *module,
				   gchar            **out_prog,
				   gchar            **out_prog_args)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gboolean retval = TRUE;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (module != NULL && module[0] != 0, FALSE);
	g_return_val_if_fail (out_prog != NULL, FALSE);
	g_return_val_if_fail (out_prog_args != NULL, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_get_xim_program(priv->proxy, module, out_prog, out_prog_args, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "GetXimProgram", error->message);
		g_error_free(error);
		retval = FALSE;
	}

	return retval;
}

gboolean
imsettings_request_get_preferences_program(IMSettingsRequest *imsettings,
					   const gchar       *module,
					   gchar            **out_prog,
					   gchar            **out_prog_args)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gboolean retval = TRUE;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (module != NULL && module[0] != 0, FALSE);
	g_return_val_if_fail (out_prog != NULL, FALSE);
	g_return_val_if_fail (out_prog_args != NULL, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_get_preferences_program(priv->proxy, module, out_prog, out_prog_args, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "GetPreferencesProgram", error->message);
		g_error_free(error);
		retval = FALSE;
	}

	return retval;
}

gboolean
imsettings_request_get_auxiliary_program(IMSettingsRequest *imsettings,
					 const gchar       *module,
					 gchar            **out_prog,
					 gchar            **out_prog_args)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gboolean retval = TRUE;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (module != NULL && module[0] != 0, FALSE);
	g_return_val_if_fail (out_prog != NULL, FALSE);
	g_return_val_if_fail (out_prog_args != NULL, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_get_auxiliary_program(priv->proxy, module, out_prog, out_prog_args, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "GetAuxiliaryProgram", error->message);
		g_error_free(error);
		retval = FALSE;
	}

	return retval;
}

gchar *
imsettings_request_get_short_description(IMSettingsRequest *imsettings,
					 const gchar       *module)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gchar *retval = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), NULL);
	g_return_val_if_fail (module != NULL && module[0] != 0, NULL);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_get_short_description(priv->proxy, module, &retval, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "GetShortDescription", error->message);
		g_error_free(error);
	}

	return retval;
}

gchar *
imsettings_request_get_long_description(IMSettingsRequest *imsettings,
					const gchar       *module)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gchar *retval = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), NULL);
	g_return_val_if_fail (module != NULL && module[0] != 0, NULL);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_get_long_description(priv->proxy, module, &retval, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "GetLongDescription", error->message);
		g_error_free(error);
	}

	return retval;
}

gboolean
imsettings_request_is_system_default(IMSettingsRequest *imsettings,
				     const gchar       *module)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gboolean retval = FALSE;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (module != NULL && module[0] != 0, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_is_system_default(priv->proxy, module, &retval, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "IsSystemDefault", error->message);
		g_error_free(error);
	}

	return retval;
}

gboolean
imsettings_request_is_user_default(IMSettingsRequest *imsettings,
				   const gchar       *module)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gboolean retval = FALSE;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (module != NULL && module[0] != 0, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_is_user_default(priv->proxy, module, &retval, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "IsUserDefault", error->message);
		g_error_free(error);
	}

	return retval;
}

gboolean
imsettings_request_is_xim(IMSettingsRequest *imsettings,
			  const gchar       *module)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gboolean retval = FALSE;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (module != NULL && module[0] != 0, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_is_xim(priv->proxy, module, &retval, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "IsXim", error->message);
		g_error_free(error);
	}

	return retval;
}

gchar *
imsettings_request_get_supported_language(IMSettingsRequest *imsettings,
					  const gchar       *module)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gchar *retval = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), NULL);
	g_return_val_if_fail (module != NULL && module[0] != 0, NULL);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_IMInfo_get_supported_language(priv->proxy, module, &retval, &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "GetSupportedLanguage", error->message);
		g_error_free(error);
	}

	return retval;
}

gboolean
imsettings_request_start_im(IMSettingsRequest *imsettings,
			    const gchar       *module,
			    gboolean           update_xinputrc)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gboolean retval = FALSE;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (module != NULL && module[0] != 0, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_start_im(priv->proxy,
					    priv->locale,
					    module,
					    update_xinputrc,
					    &retval,
					    &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "StartIM", error->message);
		g_error_free(error);
	}

	return retval;
}

gboolean
imsettings_request_start_im_async(IMSettingsRequest                    *imsettings,
				  const gchar                          *module,
				  gboolean                              update_xinputrc,
				  com_redhat_imsettings_start_im_reply  callback,
				  gpointer                              user_data)
{
	IMSettingsRequestPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (module != NULL && module[0] != 0, FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);

	return com_redhat_imsettings_start_im_async(priv->proxy,
						    priv->locale,
						    module,
						    update_xinputrc,
						    callback,
						    user_data) != NULL;
}

gboolean
imsettings_request_stop_im(IMSettingsRequest  *imsettings,
			   const gchar        *module,
			   gboolean            update_xinputrc,
			   gboolean            force)
{
	IMSettingsRequestPrivate *priv;
	gboolean retval = FALSE;
	GError *error = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (module != NULL && module[0] != 0, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_stop_im(priv->proxy,
					   module,
					   update_xinputrc,
					   force,
					   &retval,
					   &error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "StopIM", error->message);
		g_error_free(error);

		if (force)
			return TRUE;
	}

	return retval;
}

gboolean
imsettings_request_stop_im_async(IMSettingsRequest                   *imsettings,
				 const gchar                         *module,
				 gboolean                             update_xinputrc,
				 gboolean                             force,
				 com_redhat_imsettings_stop_im_reply  callback,
				 gpointer                             user_data)
{
	IMSettingsRequestPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (module != NULL && module[0] != 0, FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);

	return com_redhat_imsettings_stop_im_async(priv->proxy,
						   module,
						   update_xinputrc,
						   force,
						   callback,
						   user_data) != NULL;
}

gchar *
imsettings_request_what_im_is_running(IMSettingsRequest *imsettings)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gchar *retval;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!com_redhat_imsettings_what_input_method_is_running(priv->proxy,
								&retval,
								&error)) {
		g_warning(_("Failed to invoke a method `%s':\n  %s"), "WhatInputMethodIsRunning", error->message);
		g_error_free(error);

		return NULL;
	}

	return retval;
}

gboolean
imsettings_request_reload(IMSettingsRequest *imsettings,
			  gboolean           force)
{
	IMSettingsRequestPrivate *priv;
	DBusMessage *message;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	message = dbus_message_new_signal(priv->path, priv->interface, "Reload");
	dbus_message_append_args(message,
				 DBUS_TYPE_BOOLEAN, &force,
				 DBUS_TYPE_INVALID);
	dbus_connection_send(priv->connection, message, NULL);
	dbus_message_unref(message);

	return TRUE;
}

gboolean
imsettings_request_change_to(IMSettingsRequest *imsettings,
			     const gchar       *module)
{
	IMSettingsRequestPrivate *priv;
	GError *error = NULL;
	gboolean retval = FALSE;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (module != NULL, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	if (!dbus_g_proxy_call(priv->proxy, "ChangeTo", &error,
			       G_TYPE_STRING, module,
			       G_TYPE_INVALID,
			       G_TYPE_BOOLEAN, &retval,
			       G_TYPE_INVALID)) {
		g_warning(_("Failed to invoke a method `%s' on %s:\n  %s"),
			  "ChangeTo",
			  dbus_g_proxy_get_interface(priv->proxy),
			  error->message);
		g_error_free(error);
	}

	return retval;
}

gboolean
imsettings_request_change_to_with_signal(IMSettingsRequest *imsettings,
					 const gchar       *module)
{
	IMSettingsRequestPrivate *priv;
	DBusMessage *message;

	g_return_val_if_fail (IMSETTINGS_IS_REQUEST (imsettings), FALSE);
	g_return_val_if_fail (module != NULL, FALSE);

	priv = IMSETTINGS_REQUEST_GET_PRIVATE (imsettings);
	message = dbus_message_new_signal(priv->path, priv->interface, "ChangeTo");
	dbus_message_append_args(message,
				 DBUS_TYPE_STRING, &module,
				 DBUS_TYPE_INVALID);
	dbus_connection_send(priv->connection, message, NULL);
	dbus_message_unref(message);

	return TRUE;
}
