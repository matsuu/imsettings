/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * main.c
 * Copyright (C) 2008 Red Hat, Inc. All rights reserved.
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gconf/gconf.h>
#include "imsettings/imsettings.h"

#ifdef GNOME_ENABLE_DEBUG
#define d(e)	e
#else
#define d(e)
#endif


typedef struct _IMSettingsGConfClass	IMSettingsGConfClass;
typedef struct _IMSettingsGConf		IMSettingsGConf;

struct _IMSettingsGConfClass {
	GObjectClass parent_class;
};

struct _IMSettingsGConf {
	GObject  parent_instance;
	gchar   *owner;
};


GType            imsettings_gconf_get_type(void) G_GNUC_CONST;
IMSettingsGConf *imsettings_gconf_new(void);


G_DEFINE_TYPE (IMSettingsGConf, imsettings_gconf, G_TYPE_OBJECT);

/*
 * Private functions
 */
static gboolean
gconf_imsettings_change_to(GObject      *object,
			   const gchar  *module,
			   gboolean     *ret,
			   GError      **error)
{
	GConfEngine *engine;
	GConfValue *gval;

	engine = g_object_get_data(object, "imsettings-gconf-engine");
	gval = gconf_value_new_from_string(GCONF_VALUE_STRING, module, error);
	if (gval == NULL) {
		*ret = FALSE;
	} else {
		d(g_print("setting up gconf key to %s\n", module));
		if (!gconf_engine_set(engine, "/desktop/gnome/interface/gtk-im-module", gval, error)) {
			*ret = FALSE;
		} else {
			*ret = TRUE;
		}
		gconf_value_free(gval);
	}

	return *ret;
}

#include "gconf-imsettings-glue.h"

static void
imsettings_gconf_class_init(IMSettingsGConfClass *klass)
{
	dbus_g_object_type_install_info(imsettings_gconf_get_type(),
					&dbus_glib_gconf_imsettings_object_info);
}

static void
imsettings_gconf_init(IMSettingsGConf *gconf)
{
}

static void
_disconnected(IMSettingsGConf *gconf)
{
	GMainLoop *loop;

	d(g_print("***\n*** Disconnected\n***\n"));
	loop = g_object_get_data(G_OBJECT (gconf), "imsettings-gconf-loop-main");
	g_main_loop_quit(loop);
}

static DBusHandlerResult
imsettings_gconf_message_filter(DBusConnection *connection,
				DBusMessage    *message,
				void           *data)
{
	IMSettingsGConf *gconf = G_TYPE_CHECK_INSTANCE_CAST (data, imsettings_gconf_get_type(), IMSettingsGConf);
	GMainLoop *loop;

	if (dbus_message_is_signal(message, DBUS_INTERFACE_LOCAL, "Disconnected")) {
		_disconnected(gconf);
	} else if (dbus_message_is_signal(message, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
		gchar *service, *old_owner, *new_owner;

		dbus_message_get_args(message, NULL,
				      DBUS_TYPE_STRING, &service,
				      DBUS_TYPE_STRING, &old_owner,
				      DBUS_TYPE_STRING, &new_owner,
				      DBUS_TYPE_INVALID);
		if (strcmp(service, IMSETTINGS_GCONF_INTERFACE_DBUS) == 0) {
			d(g_print("OwnerChanged: `%s'->`%s' for %s\n", old_owner, new_owner, IMSETTINGS_GCONF_INTERFACE_DBUS));
			if (gconf->owner == NULL) {
				gconf->owner = g_strdup(new_owner);
			}
			if (old_owner && strcmp(old_owner, gconf->owner) == 0) {
				_disconnected(gconf);
			}

			return DBUS_HANDLER_RESULT_HANDLED;
		}
	} else if (dbus_message_is_signal(message, IMSETTINGS_GCONF_INTERFACE_DBUS, "Reload")) {
		gboolean force = FALSE;

		dbus_message_get_args(message, NULL, DBUS_TYPE_BOOLEAN, &force, DBUS_TYPE_INVALID);
		d(g_print("Reloading%s\n", (force ? " forcibly" : "")));
		if (force) {
			loop = g_object_get_data(G_OBJECT (gconf), "imsettings-gconf-loop-main");
			g_main_loop_quit(loop);
		}

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/*
 * Public functions
 */
IMSettingsGConf *
imsettings_gconf_new(void)
{
	return (IMSettingsGConf *)g_object_new(imsettings_gconf_get_type(), NULL);
}

int
main(int    argc,
     char **argv)
{
	DBusError derror;
	DBusConnection *conn;
	DBusGConnection *gconn;
	IMSettingsGConf *gconf;
	GMainLoop *loop;
	gint flags, ret;
	GConfEngine *gengine;
	gboolean arg_replace = FALSE;
	GOptionContext *ctx = g_option_context_new(NULL);
	GOptionEntry entries[] = {
		{"replace", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_replace, N_("Replace the running settings daemon with new instance."), NULL},
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	GError *error = NULL;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, IMSETTINGS_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif /* HAVE_BIND_TEXTDOMAIN_CODESET */
	textdomain (GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */

	g_type_init();
	dbus_error_init(&derror);

	/* deal with the arguments */
	g_option_context_add_main_entries(ctx, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse(ctx, &argc, &argv, &error)) {
		if (error != NULL) {
			g_print("%s\n", error->message);
		} else {
			g_warning(_("Unknown error in parsing the command lines."));
		}
		exit(1);
	}
	g_option_context_free(ctx);

	gengine = gconf_engine_get_default();

	gconn = dbus_g_bus_get(DBUS_BUS_SESSION, NULL);
	conn = dbus_g_connection_get_connection(gconn);

	flags = DBUS_NAME_FLAG_ALLOW_REPLACEMENT | DBUS_NAME_FLAG_DO_NOT_QUEUE;
	if (arg_replace) {
		flags |= DBUS_NAME_FLAG_REPLACE_EXISTING;
	}

	ret = dbus_bus_request_name(conn, IMSETTINGS_GCONF_SERVICE_DBUS, flags, &derror);
	if (dbus_error_is_set(&derror)) {
		g_printerr("Failed to acquire IMSettings service for %s:\n  %s\n", IMSETTINGS_GCONF_SERVICE_DBUS, derror.message);
		dbus_error_free(&derror);

		return 1;
	}
	if (ret == DBUS_REQUEST_NAME_REPLY_EXISTS) {
		g_printerr("IMSettings service for %s already running. exiting.\n", IMSETTINGS_GCONF_SERVICE_DBUS);

		return 1;
	} else if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		g_printerr("Not primary owner of the service, exiting.\n");

		return 1;
	}

	gconf = imsettings_gconf_new();

	g_object_set_data(G_OBJECT (gconf), "imsettings-gconf-engine", gengine);

	dbus_bus_add_match(conn,
			   "type='signal',"
			   "interface='" DBUS_INTERFACE_DBUS "',"
			   "sender='" DBUS_SERVICE_DBUS "'",
			   &derror);
	dbus_bus_add_match(conn, "type='signal',interface='" IMSETTINGS_GCONF_INTERFACE_DBUS "'", &derror);
	dbus_connection_add_filter(conn, imsettings_gconf_message_filter, gconf, NULL);

	dbus_g_connection_register_g_object(gconn, IMSETTINGS_GCONF_PATH_DBUS, G_OBJECT (gconf));

	loop = g_main_loop_new(NULL, FALSE);
	g_object_set_data(G_OBJECT (gconf), "imsettings-gconf-loop-main", loop);
	g_main_loop_run(loop);

	g_main_loop_unref(loop);
	dbus_g_connection_unref(gconn);
	g_object_unref(gconf);

	return 0;
}
