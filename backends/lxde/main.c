/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * main.c
 * Copyright (C) 2009-2010 Red Hat, Inc. All rights reserved.
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
#include <errno.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-utils.h"


typedef struct _IMSettingsLxdeClass	IMSettingsLxdeClass;
typedef struct _IMSettingsLxde		IMSettingsLxde;

struct _IMSettingsLxdeClass {
	GObjectClass parent_class;
};

struct _IMSettingsLxde {
	GObject  parent_instance;
	gchar   *owner;
};


GType           imsettings_lxde_get_type(void) G_GNUC_CONST;
IMSettingsLxde *imsettings_lxde_new(void);


G_DEFINE_TYPE (IMSettingsLxde, imsettings_lxde, G_TYPE_OBJECT);

/*
 * Private functions
 */
static gboolean
lxde_imsettings_change_to(GObject      *object,
			  const gchar  *module,
			  gboolean     *ret,
			  GError      **error)
{
	GKeyFile *key = g_key_file_new();
	gchar *confdir = g_build_filename(g_get_user_config_dir(), "lxde", NULL);
	gchar *conf = g_build_filename(confdir, "config", NULL);
	gchar *s;
	gsize len = 0;

	*ret = FALSE;
	if (!g_key_file_load_from_file(key, conf, 0, NULL)) {
		if (!g_key_file_load_from_file(key, LXDE_CONFIGDIR G_DIR_SEPARATOR_S "config", 0, NULL)) {
			g_warning("Unable to load the lxde configuration file.");
			goto end;
		}
	}

	g_key_file_set_string(key, "GTK", "sGtk/IMModule", module);

	if ((s = g_key_file_to_data(key, &len, NULL)) != NULL) {
		if (g_mkdir_with_parents(confdir, 0700) != 0) {
			int save_errno = errno;

			g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(save_errno),
				    "Failed to create the user config dir: %s",
				    g_strerror(save_errno));
			g_free(s);
			goto end;
		}
		if (g_file_set_contents(conf, s, len, error)) {
			if (g_spawn_command_line_sync("lxde-settings-daemon reload", NULL, NULL, NULL, error)) {
				*ret = TRUE;
			}
		} else {
			g_warning("Unable to store the configuration into %s", conf);
		}
	} else {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_FAILED,
			    "Unable to obtain the configuration from the instance.");
	}
	g_free(s);
  end:
	g_free(conf);
	g_free(confdir);
	g_key_file_free(key);

	return *ret;
}

#include "lxde-imsettings-glue.h"

static void
imsettings_lxde_class_init(IMSettingsLxdeClass *klass)
{
	dbus_g_object_type_install_info(imsettings_lxde_get_type(),
					&dbus_glib_lxde_imsettings_object_info);
}

static void
imsettings_lxde_init(IMSettingsLxde *lxde)
{
}

static void
_disconnected(IMSettingsLxde *lxde)
{
	GMainLoop *loop;

	d(g_print("***\n*** Disconnected\n***\n"));
	loop = g_object_get_data(G_OBJECT (lxde), "imsettings-lxde-loop-main");
	g_main_loop_quit(loop);
}

static DBusHandlerResult
imsettings_lxde_message_filter(DBusConnection *connection,
			       DBusMessage    *message,
			       void           *data)
{
	IMSettingsLxde *lxde = G_TYPE_CHECK_INSTANCE_CAST (data, imsettings_lxde_get_type(), IMSettingsLxde);
	GMainLoop *loop;

	if (dbus_message_is_signal(message, DBUS_INTERFACE_LOCAL, "Disconnected")) {
		_disconnected(lxde);
	} else if (dbus_message_is_signal(message, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
		gchar *service, *old_owner, *new_owner;

		dbus_message_get_args(message, NULL,
				      DBUS_TYPE_STRING, &service,
				      DBUS_TYPE_STRING, &old_owner,
				      DBUS_TYPE_STRING, &new_owner,
				      DBUS_TYPE_INVALID);
		if (strcmp(service, IMSETTINGS_LXDE_INTERFACE_DBUS) == 0) {
			d(g_print("OwnerChanged: `%s'->`%s' for %s\n", old_owner, new_owner, IMSETTINGS_LXDE_INTERFACE_DBUS));
			if (lxde->owner == NULL) {
				lxde->owner = g_strdup(new_owner);
			}
			if (old_owner && strcmp(old_owner, lxde->owner) == 0) {
				_disconnected(lxde);
			}

			return DBUS_HANDLER_RESULT_HANDLED;
		}
	} else if (dbus_message_is_signal(message, IMSETTINGS_LXDE_INTERFACE_DBUS, "Reload")) {
		gboolean force = FALSE;

		dbus_message_get_args(message, NULL, DBUS_TYPE_BOOLEAN, &force, DBUS_TYPE_INVALID);
		d(g_print("Reloading%s\n", (force ? " forcibly" : "")));
		if (force) {
			loop = g_object_get_data(G_OBJECT (lxde), "imsettings-lxde-loop-main");
			g_main_loop_quit(loop);
		}

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/*
 * Public functions
 */
IMSettingsLxde *
imsettings_lxde_new(void)
{
	return (IMSettingsLxde *)g_object_new(imsettings_lxde_get_type(), NULL);
}

int
main(int    argc,
     char **argv)
{
	DBusError derror;
	DBusConnection *conn;
	DBusGConnection *gconn;
	IMSettingsLxde *lxde;
	GMainLoop *loop;
	gint flags, ret;
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

	gconn = dbus_g_bus_get(DBUS_BUS_SESSION, NULL);
	conn = dbus_g_connection_get_connection(gconn);

	flags = DBUS_NAME_FLAG_ALLOW_REPLACEMENT | DBUS_NAME_FLAG_DO_NOT_QUEUE;
	if (arg_replace) {
		flags |= DBUS_NAME_FLAG_REPLACE_EXISTING;
	}

	ret = dbus_bus_request_name(conn, IMSETTINGS_LXDE_SERVICE_DBUS, flags, &derror);
	if (dbus_error_is_set(&derror)) {
		g_printerr("Failed to acquire IMSettings service for %s:\n  %s\n", IMSETTINGS_LXDE_SERVICE_DBUS, derror.message);
		dbus_error_free(&derror);

		return 1;
	}
	if (ret == DBUS_REQUEST_NAME_REPLY_EXISTS) {
		g_printerr("IMSettings service for %s already running. exiting.\n", IMSETTINGS_LXDE_SERVICE_DBUS);

		return 1;
	} else if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		g_printerr("Not primary owner of the service, exiting.\n");

		return 1;
	}

	lxde = imsettings_lxde_new();

	dbus_bus_add_match(conn,
			   "type='signal',"
			   "interface='" DBUS_INTERFACE_DBUS "',"
			   "sender='" DBUS_SERVICE_DBUS "'",
			   &derror);
	dbus_bus_add_match(conn, "type='signal',interface='" IMSETTINGS_LXDE_INTERFACE_DBUS "'", &derror);
	dbus_connection_add_filter(conn, imsettings_lxde_message_filter, lxde, NULL);

	dbus_g_connection_register_g_object(gconn, IMSETTINGS_LXDE_PATH_DBUS, G_OBJECT (lxde));

	loop = g_main_loop_new(NULL, FALSE);
	g_object_set_data(G_OBJECT (lxde), "imsettings-lxde-loop-main", loop);
	g_main_loop_run(loop);

	g_main_loop_unref(loop);
	dbus_g_connection_unref(gconn);
	g_object_unref(lxde);

	return 0;
}
