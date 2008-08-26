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

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <libgxim/gximmessage.h>
#include <libgxim/gximmisc.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-request.h"
#include "client.h"
#include "proxy.h"
#include "utils.h"

#ifdef GNOME_ENABLE_DEBUG
#define d(e)	e
#else
#define d(e)
#endif


typedef struct _Proxy {
	GMainLoop  *loop;
	XimProxy   *server;
	GHashTable *client_table;
	GHashTable *window_table;
	gchar      *server_name;
} Proxy;

static void     _quit_cb             (XimProxy          *proxy,
                                      gpointer           data);


static GQuark quark_proxy = 0;


/*
 * Private functions
 */
static void
_quit_cb(XimProxy *proxy,
	 gpointer  data)
{
	GMainLoop *loop = data;

	g_print("*** Exiting...\n");

	g_main_loop_quit(loop);
}

static XimProxy *
_create_proxy(GdkDisplay *dpy,
	      Proxy      *proxy,
	      gboolean    replace)
{
	XimProxy *retval;
	GError *error = NULL;

	retval = xim_proxy_new(dpy, "imsettings", proxy->server_name);
	if (retval == NULL)
		return NULL;
	g_signal_connect(retval, "destroy",
			 G_CALLBACK (_quit_cb),
			 proxy->loop);
	if (!xim_proxy_take_ownership(retval, replace, &error)) {
		g_print("Unable to take an ownership: %s\n",
			error->message);
		exit(1);
	}

	return retval;
}

static DBusHandlerResult
imsettings_xim_message_filter(DBusConnection *connection,
			      DBusMessage    *message,
			      void           *data)
{
	XimProxy *server = XIM_PROXY (data);

	if (dbus_message_is_signal(message, IMSETTINGS_XIM_INTERFACE_DBUS, "ChangeTo")) {
		gchar *module;
		Proxy *proxy;

		proxy = g_object_get_qdata(G_OBJECT (server), quark_proxy);
		dbus_message_get_args(message, NULL,
				      DBUS_TYPE_STRING, &module,
				      DBUS_TYPE_INVALID);
		if (strcmp(module, proxy->server_name) == 0) {
			/* No changes */
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		g_xim_message_debug(G_XIM_CORE (server)->message, "dbus/event",
				    "Changing XIM server: `%s'->`%s'\n",
				    (proxy->server_name && proxy->server_name[0] == 0 ? "none" : proxy->server_name),
				    (module && module[0] == 0 ? "none" : module));
		g_free(proxy->server_name);
		proxy->server_name = g_strdup(module);
		g_object_set(proxy->server, "connect_to", module, NULL);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/*
 * Public functions
 */
int
main(int    argc,
     char **argv)
{
	Proxy *proxy;
	gchar *arg_display_name = NULL, *arg_xim = NULL, *dpy_name;
	gboolean arg_replace = FALSE, arg_verbose = FALSE;
	GOptionContext *ctx = g_option_context_new(NULL);
	GOptionEntry entries[] = {
		{"display", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &arg_display_name, N_("X display to use"), N_("DISPLAY")},
		{"replace", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_replace, N_("Replace the running XIM server with new instance."), NULL},
		{"verbose", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_verbose, N_("Output the debugging logs"), NULL},
		{"xim", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &arg_xim, N_("XIM server connects to, for debugging purpose only"), N_("XIM")},
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	GError *error = NULL;
	GdkDisplay *dpy;
	IMSettingsRequest *req;
	IMSettingsInfo *info;
	DBusConnection *conn;
	gchar *locale, *module;
	const gchar *xim;
	DBusError derror;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, IMSETTINGS_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif /* HAVE_BIND_TEXTDOMAIN_CODESET */
	textdomain (GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */

	setlocale(LC_ALL, "");
	locale = setlocale(LC_CTYPE, NULL);

	g_type_init();
	gdk_init(&argc, &argv);
	g_xim_init();
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

	dpy_name = xim_substitute_display_name(arg_display_name);
	dpy = gdk_display_open(dpy_name);
	if (dpy == NULL) {
		g_print("Can't open a X display: %s\n", dpy_name);
		exit(1);
	}
	g_free(dpy_name);

	quark_proxy = g_quark_from_static_string("imsettings-xim-proxy");

	conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	req = imsettings_request_new(conn, IMSETTINGS_INFO_INTERFACE_DBUS);
	imsettings_request_set_locale(req, locale);
	module = imsettings_request_get_current_user_im(req, &error);
	if (module == NULL) {
		g_print("No default IM is available.\n");
		exit(1);
	}
	info = imsettings_request_get_info_object(req, module, &error);
	if (arg_xim == NULL)
		xim = imsettings_info_get_xim(info);
	else
		xim = arg_xim;
	g_free(module);

	proxy = g_new0(Proxy, 1);
	proxy->loop = g_main_loop_new(NULL, FALSE);
	proxy->client_table = g_hash_table_new_full(g_direct_hash,
						    g_direct_equal,
						    NULL,
						    g_object_unref);
	proxy->window_table = g_hash_table_new(g_direct_hash,
					       g_direct_equal);
	proxy->server_name = g_strdup(xim);
	proxy->server = _create_proxy(dpy, proxy, arg_replace);
	if (proxy->server == NULL)
		exit(1);
	g_object_set_qdata(G_OBJECT (proxy->server), quark_proxy, proxy);

	dbus_bus_add_match(conn,
			   "type='signal',"
			   "interface='" IMSETTINGS_XIM_INTERFACE_DBUS "'",
			   &derror);
	dbus_connection_add_filter(conn, imsettings_xim_message_filter, proxy->server, NULL);

	g_main_loop_run(proxy->loop);

	g_object_unref(info);
	g_hash_table_destroy(proxy->client_table);
	g_hash_table_destroy(proxy->window_table);
	g_main_loop_unref(proxy->loop);
	g_object_unref(proxy->server);
	g_free(proxy->server_name);
	g_free(proxy);
	gdk_display_close(dpy);

	g_xim_finalize();

	return 0;
}
