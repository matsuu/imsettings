/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-observer.c
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
#include "imsettings.h"
#include "imsettings-observer.h"
#include "imsettings-utils.h"
#include "imsettings-marshal.h"

#define IMSETTINGS_OBSERVER_GET_PRIVATE(_o_)	(G_TYPE_INSTANCE_GET_PRIVATE ((_o_), IMSETTINGS_TYPE_OBSERVER, IMSettingsObserverPrivate))
#ifdef GNOME_ENABLE_DEBUG
#define d(e)	e
#else
#define d(e)
#endif


typedef gboolean (* IMSettingsSignalFunction) (IMSettingsObserver *imsettings,
					       DBusMessage        *message,
					       const gchar        *interface,
					       const gchar        *signal_name,
					       guint               class_offset,
					       GError            **error);

typedef struct _IMSettingsObserverPrivate {
	DBusGConnection *connection;
	gchar           *module_name;
	gchar           *service;
	gchar           *owner;
	gboolean         replace;
} IMSettingsObserverPrivate;

typedef struct _IMSettingsObserverSignal {
	const gchar              *interface;
	const gchar              *signal_name;
	guint                     class_offset;
	IMSettingsSignalFunction  callback;
} IMSettingsObserverSignal;


enum {
	PROP_0,
	PROP_MODULE,
	PROP_REPLACE,
	PROP_CONNECTION,
	LAST_PROP
};
enum {
	DISCONNECTED,
	RELOAD,
	STARTIM,
	STOPIM,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (IMSettingsObserver, imsettings_observer, G_TYPE_OBJECT);

/*
 * Private functions
 */
static gboolean
imsettings_start_im(GObject      *object,
		    const gchar  *lang,
		    const gchar  *module,
		    gboolean      update_xinputrc,
		    gboolean     *ret,
		    GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);

	*ret = FALSE;
	d(g_print("Starting IM `%s' with `%s' language\n", module, lang));
	if (klass->start_im) {
		*ret = klass->start_im(IMSETTINGS_OBSERVER (object),
				       lang,
				       module,
				       update_xinputrc,
				       error);
	}

	return *ret;
}

static gboolean
imsettings_stop_im(GObject      *object,
		   const gchar  *module,
		   gboolean      update_xinputrc,
		   gboolean      force,
		   gboolean     *ret,
		   GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);

	*ret = FALSE;
	d(g_print("Stopping IM `%s'%s\n", module, (force ? " forcibly" : "")));
	if (klass->stop_im) {
		*ret = klass->stop_im(IMSETTINGS_OBSERVER (object),
				      module,
				      update_xinputrc,
				      force,
				      error);
	}

	return *ret;
}

static gboolean
imsettings_what_input_method_is_running(GObject      *object,
					const gchar **ret,
					GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	const gchar *module;
	gboolean retval = FALSE;

	d(g_print("Getting current IM running\n"));
	if (klass->what_im_is_running) {
		module = klass->what_im_is_running(IMSETTINGS_OBSERVER (object),
						   error);
		if (*error == NULL) {
			*ret = g_strdup(module);
			retval = TRUE;
		} else {
			*ret = NULL;
		}
	}

	return retval;
}

static gboolean
imsettings_get_list(GObject     *object,
		    const gchar *lang,
		    gchar     ***ret,
		    GError     **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	GPtrArray *list;
	gboolean retval = FALSE;
	gint i;

	d(g_print("Getting list for `%s' language\n", lang));
	if (klass->get_list) {
		list = klass->get_list(IMSETTINGS_OBSERVER (object), lang, error);
		if (*error == NULL) {
			*ret = g_strdupv((gchar **)list->pdata);
			retval = TRUE;
		}
		if (list) {
			for (i = 0; i < list->len; i++) {
				g_free(g_ptr_array_index(list, i));
			}
			g_ptr_array_free(list, TRUE);
		}
	}

	return retval;
}

static gboolean
imsettings_get_info_objects(GObject      *object,
			    const gchar  *lang,
			    GPtrArray   **ret,
			    GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	GPtrArray *retval = NULL;
	gsize i;

	d(g_print("Getting IMInfo Objects...\n"));
	if (klass->get_info_objects) {
		retval = klass->get_info_objects(IMSETTINGS_OBSERVER (object), lang, error);
	} else {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_NOT_AVAILABLE,
			    "No GetObjects method is supported");
	}
	if (*error == NULL) {
		GValueArray *varray;
		GValue *val;

		*ret = g_ptr_array_sized_new(retval->len);
		for (i = 0; i < retval->len; i++) {
			IMSettingsObject *o;
			GString *s;
			GArray *v;

			varray = g_value_array_new(2);
			g_ptr_array_add(*ret, varray);

			o = IMSETTINGS_OBJECT (g_ptr_array_index(retval, i));
			s = imsettings_object_dump(o);

			g_value_array_append(varray, NULL);
			val = g_value_array_get_nth(varray, 0);
			g_value_init(val, G_TYPE_UINT);
			g_value_set_uint(val, s->len);

			g_value_array_append(varray, NULL);
			val = g_value_array_get_nth(varray, 1);
			g_value_init(val, dbus_g_type_get_collection("GArray", G_TYPE_UCHAR));
			v = g_array_sized_new(FALSE, TRUE, sizeof (guchar), s->len);
			g_array_append_vals(v, s->str, s->len);
			g_value_set_boxed(val, v);

			g_array_free(v, TRUE);
			g_string_free(s, TRUE);
		}
	}

	return *error == NULL;
}

static gboolean
imsettings_get_info_object(GObject      *object,
			   const gchar  *name,
			   GValueArray **ret,
			   GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	IMSettingsInfo *info = NULL;

	d(g_print("Getting IMInfo Object...\n"));
	if (klass->get_info) {
		info = klass->get_info(IMSETTINGS_OBSERVER (object), name, error);
	} else {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_NOT_AVAILABLE,
			    "No GetInfoObject method is supported");
	}
	if (*error == NULL) {
		GValue *val;
		GString *s;
		GArray *v;

		*ret = g_value_array_new(2);

		s = imsettings_object_dump(IMSETTINGS_OBJECT (info));

		g_value_array_append(*ret, NULL);
		val = g_value_array_get_nth(*ret, 0);
		g_value_init(val, G_TYPE_UINT);
		g_value_set_uint(val, s->len);

		g_value_array_append(*ret, NULL);
		val = g_value_array_get_nth(*ret, 1);
		g_value_init(val, dbus_g_type_get_collection("GArray", G_TYPE_UCHAR));
		v = g_array_sized_new(FALSE, TRUE, sizeof (guchar), s->len);
		g_array_append_vals(v, s->str, s->len);
		g_value_set_boxed(val, v);

		g_array_free(v, TRUE);
		g_string_free(s, TRUE);
	}

	return *error == NULL;
}

static gboolean
imsettings_get_current_user_im(GObject      *object,
			       const gchar **ret,
			       GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	gboolean retval = FALSE;
	const gchar *name;

	d(g_print("Getting current user IM\n"));
	if (klass->get_current_user_im) {
		name = klass->get_current_user_im(IMSETTINGS_OBSERVER (object), error);
		if (*error == NULL) {
			*ret = g_strdup(name);
			retval = TRUE;
		}
	}

	return retval;
}

static gboolean
imsettings_get_current_system_im(GObject      *object,
				 const gchar **ret,
				 GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	gboolean retval = FALSE;
	const gchar *name;

	d(g_print("Getting current system IM\n"));
	if (klass->get_current_system_im) {
		name = klass->get_current_system_im(IMSETTINGS_OBSERVER (object), error);
		if (*error == NULL) {
			*ret = g_strdup(name);
			retval = TRUE;
		}
	}

	return retval;
}

static gboolean
imsettings_get_xinput_filename(GObject      *object,
			       const gchar  *module,
			       const gchar **ret,
			       GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	IMSettingsInfo *info;
	gboolean retval = FALSE;

	if (klass->get_info) {
		info = klass->get_info(IMSETTINGS_OBSERVER (object), module, error);
		if (*error == NULL) {
			*ret = imsettings_info_get_filename(info);
			retval = TRUE;
		}
	}

	return retval;
}

static gboolean
imsettings_get_im_module_name(GObject      *object,
			      const gchar  *imname,
			      guint32       type,
			      const gchar **ret,
			      GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	IMSettingsInfo *info;
	gboolean retval = FALSE;

	if (klass->get_info) {
		info = klass->get_info(IMSETTINGS_OBSERVER (object), imname, error);
		if (*error == NULL) {
			retval = TRUE;
			switch (type) {
			    case IMSETTINGS_IMM_GTK:
				    *ret = imsettings_info_get_gtkimm(info);
				    break;
			    case IMSETTINGS_IMM_QT:
				    *ret = imsettings_info_get_qtimm(info);
				    break;
			    case IMSETTINGS_IMM_XIM:
				    *ret = imsettings_info_get_xim(info);
				    break;
			    default:
				    g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_INVALID_IMM,
						_("Invalid IM module type: %d"), type);
				    retval = FALSE;
				    break;
			}
		}
	}

	return retval;
}

static gboolean
imsettings_get_xim_program(GObject      *object,
			   const gchar  *module,
			   const gchar **progname,
			   const gchar **progargs,
			   GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	IMSettingsInfo *info;
	gboolean retval = FALSE;

	if (klass->get_info) {
		info = klass->get_info(IMSETTINGS_OBSERVER (object), module, error);
		if (*error == NULL) {
			*progname = imsettings_info_get_xim_program(info);
			*progargs = imsettings_info_get_xim_args(info);
			retval = TRUE;
		}
	}

	return retval;
}

static gboolean
imsettings_get_preferences_program(GObject      *object,
				   const gchar  *module,
				   const gchar **progname,
				   const gchar **progargs,
				   GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	IMSettingsInfo *info;
	gboolean retval = FALSE;

	if (klass->get_info) {
		info = klass->get_info(IMSETTINGS_OBSERVER (object), module, error);
		if (*error == NULL) {
			*progname = imsettings_info_get_prefs_program(info);
			*progargs = imsettings_info_get_prefs_args(info);
			retval = TRUE;
		}
	}

	return retval;
}

static gboolean
imsettings_get_auxiliary_program(GObject      *object,
				 const gchar  *module,
				 const gchar **progname,
				 const gchar **progargs,
				 GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	IMSettingsInfo *info;
	gboolean retval = FALSE;

	if (klass->get_info) {
		info = klass->get_info(IMSETTINGS_OBSERVER (object), module, error);
		if (*error == NULL) {
			*progname = imsettings_info_get_aux_program(info);
			*progargs = imsettings_info_get_aux_args(info);
			retval = TRUE;
		}
	}

	return retval;
}

static gboolean
imsettings_get_short_description(GObject      *object,
				 const gchar  *module,
				 const gchar **ret,
				 GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	IMSettingsInfo *info;
	gboolean retval = FALSE;

	if (klass->get_info) {
		info = klass->get_info(IMSETTINGS_OBSERVER (object), module, error);
		if (*error == NULL) {
			*ret = imsettings_info_get_short_desc(info);
			retval = TRUE;
		}
	}

	return retval;
}

static gboolean
imsettings_get_long_description(GObject      *object,
				const gchar  *module,
				const gchar **ret,
				GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	IMSettingsInfo *info;
	gboolean retval = FALSE;

	if (klass->get_info) {
		info = klass->get_info(IMSETTINGS_OBSERVER (object), module, error);
		if (*error == NULL) {
			*ret = imsettings_info_get_long_desc(info);
			retval = TRUE;
		}
	}

	return retval;
}

static gboolean
imsettings_is_system_default(GObject      *object,
			     const gchar  *module,
			     gboolean     *ret,
			     GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	IMSettingsInfo *info;
	gboolean retval = FALSE;

	if (klass->get_info) {
		info = klass->get_info(IMSETTINGS_OBSERVER (object), module, error);
		if (*error == NULL) {
			*ret = imsettings_info_is_system_default(info);
			retval = TRUE;
		}
	}

	return retval;
}

static gboolean
imsettings_is_user_default(GObject      *object,
			   const gchar  *module,
			   gboolean     *ret,
			   GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	IMSettingsInfo *info;
	gboolean retval = FALSE;

	if (klass->get_info) {
		info = klass->get_info(IMSETTINGS_OBSERVER (object), module, error);
		if (*error == NULL) {
			*ret = imsettings_info_is_user_default(info);
			retval = TRUE;
		}
	}

	return retval;
}

static gboolean
imsettings_is_xim(GObject      *object,
		  const gchar  *module,
		  gboolean     *ret,
		  GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	IMSettingsInfo *info;
	gboolean retval = FALSE;

	if (klass->get_info) {
		info = klass->get_info(IMSETTINGS_OBSERVER (object), module, error);
		if (*error == NULL) {
			*ret = imsettings_info_is_xim(info);
			retval = TRUE;
		}
	}

	return retval;
}

static gboolean
imsettings_get_supported_language(GObject      *object,
				  const gchar  *module,
				  const gchar **ret,
				  GError      **error)
{
	IMSettingsObserverClass *klass = IMSETTINGS_OBSERVER_GET_CLASS (object);
	IMSettingsInfo *info;
	gboolean retval = FALSE;

	if (klass->get_info) {
		info = klass->get_info(IMSETTINGS_OBSERVER (object), module, error);
		if (*error == NULL) {
			*ret = imsettings_info_get_supported_language(info);
			retval = TRUE;
		}
	}

	return retval;
}

#include "imsettings-glib-glue.h"


static void
imsettings_observer_set_property(GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	IMSettingsObserverPrivate *priv = IMSETTINGS_OBSERVER_GET_PRIVATE (object);

	switch (prop_id) {
	    case PROP_MODULE:
		    g_free(priv->module_name);
		    priv->module_name = g_strdup(g_value_get_string(value));
		    break;
	    case PROP_REPLACE:
		    priv->replace = g_value_get_boolean(value);
		    break;
	    case PROP_CONNECTION:
		    /* XXX: do we need to close the connection here? */
		    priv->connection = g_value_get_boxed(value);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_observer_get_property(GObject    *object,
				 guint       prop_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	IMSettingsObserverPrivate *priv = IMSETTINGS_OBSERVER_GET_PRIVATE (object);

	switch (prop_id) {
	    case PROP_MODULE:
		    g_value_set_string(value, priv->module_name);
		    break;
	    case PROP_REPLACE:
		    g_value_set_boolean(value, priv->replace);
		    break;
	    case PROP_CONNECTION:
		    g_value_set_boxed(value, priv->connection);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_observer_finalize(GObject *object)
{
	IMSettingsObserverPrivate *priv;

	priv = IMSETTINGS_OBSERVER_GET_PRIVATE (object);

	g_free(priv->module_name);
	g_free(priv->service);
	g_free(priv->owner);
	/* XXX: do we need to unref the dbus connection here? */

	if (G_OBJECT_CLASS (imsettings_observer_parent_class)->finalize)
		G_OBJECT_CLASS (imsettings_observer_parent_class)->finalize(object);
}

static gboolean
imsettings_observer_signal_disconnected(IMSettingsObserver *imsettings,
					DBusMessage        *message,
					const gchar        *interface,
					const gchar        *signal_name,
					guint               class_offset,
					GError            **error)
{
	IMSettingsObserverPrivate *priv = IMSETTINGS_OBSERVER_GET_PRIVATE (imsettings);

	d(g_print("***\n*** Disconnected\n***\n"));
	priv->connection = NULL;

	g_signal_emit(imsettings, signals[DISCONNECTED], 0, NULL);

	return TRUE;
}

static gboolean
imsettings_observer_signal_name_owner_changed(IMSettingsObserver *imsettings,
					      DBusMessage        *message,
					      const gchar        *interface,
					      const gchar        *signal_name,
					      guint               class_offset,
					      GError            **error)
{
	IMSettingsObserverPrivate *priv = IMSETTINGS_OBSERVER_GET_PRIVATE (imsettings);
	gchar *service, *old_owner, *new_owner;
	gboolean retval = FALSE;
	DBusError derror;

	dbus_error_init(&derror);
	if (dbus_message_get_args(message, &derror,
				  DBUS_TYPE_STRING, &service,
				  DBUS_TYPE_STRING, &old_owner,
				  DBUS_TYPE_STRING, &new_owner,
				  DBUS_TYPE_INVALID)) {
		if (strcmp(service, priv->service) == 0) {
			d(g_print("OwnerChanged: `%s'->`%s' for %s\n", old_owner, new_owner, service));
			if (priv->owner == NULL) {
				priv->owner = g_strdup(new_owner);
			}
			if (old_owner && strcmp(old_owner, priv->owner) == 0) {
				retval = imsettings_observer_signal_disconnected(imsettings, message, interface, signal_name, class_offset, error);
			}
		}
	} else {
		dbus_set_g_error(error, &derror);
	}

	return retval;
}

static gboolean
imsettings_observer_signal_reload(IMSettingsObserver *imsettings,
				  DBusMessage        *message,
				  const gchar        *interface,
				  const gchar        *signal_name,
				  guint               class_offset,
				  GError            **error)
{
	gboolean force = FALSE;

	dbus_message_get_args(message, NULL, DBUS_TYPE_BOOLEAN, &force, DBUS_TYPE_INVALID);

	d(g_print("Reloading%s for `%s'\n", (force ? " forcibly" : ""), interface));

	g_signal_emit(imsettings, signals[RELOAD], 0, force, NULL);

	return TRUE;
}

static DBusHandlerResult
imsettings_observer_real_message_filter(DBusConnection *connection,
					DBusMessage    *message,
					void           *data)
{
	IMSettingsObserver *imsettings = IMSETTINGS_OBSERVER (data);
	DBusError derror;
	GError *error = NULL;
	IMSettingsObserverSignal signal_table[] = {
		{DBUS_INTERFACE_LOCAL, "Disconnected",
		 0,
		 imsettings_observer_signal_disconnected},
		{DBUS_INTERFACE_DBUS, "NameOwnerChanged",
		 0,
		 imsettings_observer_signal_name_owner_changed},
		{IMSETTINGS_INTERFACE_DBUS, "Reload",
		 0,
		 imsettings_observer_signal_reload},
		{IMSETTINGS_INFO_INTERFACE_DBUS, "Reload",
		 0,
		 imsettings_observer_signal_reload},
		{NULL, NULL, 0, NULL}
	};
	gint i;

	dbus_error_init(&derror);

	for (i = 0; signal_table[i].interface != NULL; i++) {
		if (dbus_message_is_signal(message,
					   signal_table[i].interface,
					   signal_table[i].signal_name)) {
			if (signal_table[i].callback(imsettings, message,
						     signal_table[i].interface,
						     signal_table[i].signal_name,
						     signal_table[i].class_offset,
						     &error)) {
				return DBUS_HANDLER_RESULT_HANDLED;
			} else {
				if (error) {
					g_warning("Failed to deal with a signal `%s':  %s",
						  signal_table[i].signal_name,
						  error->message);
					g_error_free(error);

					return DBUS_HANDLER_RESULT_HANDLED;
				}
			}
			break;
		}
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static gboolean
imsettings_observer_real_start_im(IMSettingsObserver *imsettings,
				  const gchar        *lang,
				  const gchar        *module,
				  gboolean            update_xinputrc,
				  GError            **error)
{
	g_signal_emit(imsettings, signals[STARTIM], 0, module, update_xinputrc, NULL);

	return TRUE;
}

static gboolean
imsettings_observer_real_stop_im(IMSettingsObserver *imsettings,
				 const gchar        *module,
				 gboolean            update_xinputrc,
				 gboolean            force,
				 GError            **error)
{
	g_signal_emit(imsettings, signals[STOPIM], 0, module, update_xinputrc, force, NULL);

	return TRUE;
}

static void
imsettings_observer_class_init(IMSettingsObserverClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private(klass, sizeof (IMSettingsObserverPrivate));

	object_class->set_property = imsettings_observer_set_property;
	object_class->get_property = imsettings_observer_get_property;
	object_class->finalize = imsettings_observer_finalize;

	klass->message_filter       = imsettings_observer_real_message_filter;
	klass->start_im             = imsettings_observer_real_start_im;
	klass->stop_im              = imsettings_observer_real_stop_im;

	/* properties */
	g_object_class_install_property(object_class, PROP_MODULE,
					g_param_spec_string("module",
							    _("IM module name"),
							    _("A target IM module name that to observe signals"),
							    NULL,
							    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property(object_class, PROP_REPLACE,
					g_param_spec_boolean("replace",
							     _("Replace"),
							     _("Replace the running settings daemon with new instance."),
							     FALSE,
							     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property(object_class, PROP_CONNECTION,
					g_param_spec_boxed("connection",
							   _("DBus connection"),
							   _("An object to be a DBus connection"),
							   DBUS_TYPE_G_CONNECTION,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/* signals */
	signals[DISCONNECTED] = g_signal_new("disconnected",
					     G_OBJECT_CLASS_TYPE (klass),
					     G_SIGNAL_RUN_FIRST,
					     G_STRUCT_OFFSET (IMSettingsObserverClass, disconnected),
					     NULL, NULL,
					     g_cclosure_marshal_VOID__VOID,
					     G_TYPE_NONE, 0);
	signals[RELOAD] = g_signal_new("reload",
				       G_OBJECT_CLASS_TYPE (klass),
				       G_SIGNAL_RUN_FIRST,
				       G_STRUCT_OFFSET (IMSettingsObserverClass, reload),
				       NULL, NULL,
				       g_cclosure_marshal_VOID__BOOLEAN,
				       G_TYPE_NONE, 1,
				       G_TYPE_BOOLEAN);
	signals[STARTIM] = g_signal_new("start_im",
					G_OBJECT_CLASS_TYPE (klass),
					G_SIGNAL_RUN_FIRST,
					G_STRUCT_OFFSET (IMSettingsObserverClass, s_start_im),
					NULL, NULL,
					imsettings_marshal_VOID__STRING_BOOLEAN,
					G_TYPE_NONE, 2,
					G_TYPE_STRING, G_TYPE_BOOLEAN);
	signals[STOPIM] = g_signal_new("stop_im",
				       G_OBJECT_CLASS_TYPE (klass),
				       G_SIGNAL_RUN_FIRST,
				       G_STRUCT_OFFSET (IMSettingsObserverClass, s_stop_im),
				       NULL, NULL,
				       imsettings_marshal_VOID__STRING_BOOLEAN_BOOLEAN,
				       G_TYPE_NONE, 3,
				       G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

	dbus_g_object_type_install_info(IMSETTINGS_TYPE_OBSERVER,
					&dbus_glib_imsettings_object_info);
}

static void
imsettings_observer_init(IMSettingsObserver *observer)
{
	IMSettingsObserverPrivate *priv;

	priv = IMSETTINGS_OBSERVER_GET_PRIVATE (observer);
	priv->replace = FALSE;
}

static gboolean
imsettings_observer_setup_dbus(IMSettingsObserver *imsettings,
			       DBusConnection     *connection,
			       const gchar        *service)
{
	IMSettingsObserverPrivate *priv = IMSETTINGS_OBSERVER_GET_PRIVATE (imsettings);
	gint flags, ret;
	DBusError derror;

	g_free(priv->service);
	priv->service = g_strdup(service);

	dbus_error_init(&derror);
	priv = IMSETTINGS_OBSERVER_GET_PRIVATE (imsettings);

	flags = DBUS_NAME_FLAG_ALLOW_REPLACEMENT | DBUS_NAME_FLAG_DO_NOT_QUEUE;
	if (priv->replace) {
		flags |= DBUS_NAME_FLAG_REPLACE_EXISTING;
	}

	ret = dbus_bus_request_name(connection, service, flags, &derror);
	if (dbus_error_is_set(&derror)) {
		g_printerr("Failed to acquire IMSettings service for %s:\n  %s\n", service, derror.message);
		dbus_error_free(&derror);

		return FALSE;
	}
	if (ret == DBUS_REQUEST_NAME_REPLY_EXISTS) {
		g_printerr("IMSettings service for %s already running. exiting.\n", service);

		return FALSE;
	} else if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		g_printerr("Not primary owner of the service, exiting.\n");

		return FALSE;
	}

	return TRUE;
}

/*
 * Public functions
 */
IMSettingsObserver *
imsettings_observer_new(DBusGConnection *connection,
			const gchar     *module)
{
	g_return_val_if_fail (connection != NULL, NULL);

	return (IMSettingsObserver *)g_object_new(IMSETTINGS_TYPE_OBSERVER,
						  "module", module,
						  "connection", connection,
						  NULL);
}

gboolean
imsettings_observer_setup(IMSettingsObserver *imsettings,
			  const gchar        *service)
{
	IMSettingsObserverPrivate *priv;
	DBusError derror;
	DBusConnection *conn;
	gchar *s, *path;

	g_return_val_if_fail (IMSETTINGS_IS_OBSERVER (imsettings), FALSE);

	dbus_error_init(&derror);
	priv = IMSETTINGS_OBSERVER_GET_PRIVATE (imsettings);

	conn = dbus_g_connection_get_connection(priv->connection);

	if (!imsettings_observer_setup_dbus(imsettings, conn, service))
		return FALSE;

	dbus_bus_add_match(conn,
			   "type='signal',"
			   "interface='" DBUS_INTERFACE_DBUS "',"
			   "sender='" DBUS_SERVICE_DBUS "'",
			   &derror);

	s = g_strdup_printf("type='signal',interface='%s'", service);
	dbus_bus_add_match(conn, s, &derror);
	g_free(s);
	dbus_connection_add_filter(conn,
				   IMSETTINGS_OBSERVER_GET_CLASS (imsettings)->message_filter,
				   imsettings,
				   NULL);
	path = imsettings_generate_dbus_path_from_interface(service);
	dbus_g_connection_register_g_object(priv->connection, path, G_OBJECT (imsettings));
	g_free(path);

	return TRUE;
}
