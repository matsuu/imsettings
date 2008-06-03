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
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-request.h"
#include "server.h"
#include "utils.h"

#ifdef GNOME_ENABLE_DEBUG
#define d(e)	e
#else
#define d(e)
#endif


static XIMServer *_create_server(GMainLoop   *loop,
                                 Display     *dpy,
                                 const gchar *xim_server,
                                 gboolean     replace,
				 gboolean     verbose);
static void       _quit_cb      (XIMServer   *server,
                                 gpointer     data);


static GQuark quark_main_loop = 0;

/*
 * Private functions
 */
static DBusHandlerResult
imsettings_xim_message_filter(DBusConnection *connection,
			      DBusMessage    *message,
			      void           *data)
{
	XIMServer *server = XIM_SERVER (data);

	if (dbus_message_is_signal(message, IMSETTINGS_XIM_INTERFACE_DBUS, "ChangeTo")) {
		gchar *module;
		Display *dpy = server->dpy;
		GMainLoop *loop;
		gboolean verbose;
		const gchar *s;

		loop = g_object_get_qdata(G_OBJECT (server), quark_main_loop);
		dbus_message_get_args(message, NULL,
				      DBUS_TYPE_STRING, &module,
				      DBUS_TYPE_INVALID);
		g_object_get(G_OBJECT (server),
			     "verbose", &verbose,
			     "xim", &s,
			     NULL);
		if (strcmp(module, s) == 0) {
			/* No changes */
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		dbus_connection_remove_filter(connection, imsettings_xim_message_filter, server);
		g_object_unref(G_OBJECT (server));
		if (verbose) {
			g_print("Changing XIM server: `%s'->`%s'\n",
				(s && s[0] == 0 ? "none" : s),
				(module && module[0] == 0 ? "none" : module));
		}
		server = _create_server(loop, dpy, module, TRUE, verbose);
		dbus_connection_add_filter(connection, imsettings_xim_message_filter, server, NULL);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int
_error_cb(Display     *dpy,
	  XErrorEvent *e)
{
	if (e->error_code) {
		char msg[64];

		XGetErrorText(dpy, e->error_code, msg, 63);
		g_printerr("Received an X Window System error - %s\n", msg);
	}

	return 0;
}

static int
_io_error_cb(Display *dpy)
{
	g_printerr("IO Error from X Window System.\n");

	return 0;
}

static XIMServer *
_create_server(GMainLoop   *loop,
	       Display     *dpy,
	       const gchar *xim_server,
	       gboolean     replace,
	       gboolean     verbose)
{
	XIMServer *server;

	server = xim_server_new(dpy, xim_server);
	if (server == NULL)
		goto error;

	g_object_set(G_OBJECT (server),
		     "verbose", verbose,
		     NULL);
	if (xim_server_setup(server, replace) == FALSE ||
	    !xim_server_is_initialized(server))
		goto error;

	g_signal_connect(server, "destroy",
			 G_CALLBACK (_quit_cb),
			 loop);

	return server;

  error:
	g_printerr("Failed to initialize XIM server.\n");
	exit(1);
}

static void
_quit_cb(XIMServer *server,
	 gpointer   data)
{
	GMainLoop *loop = data;

	g_print("*** Exiting...\n");

	g_main_loop_quit(loop);
}

/*
 * Public functions
 */
int
main(int    argc,
     char **argv)
{
	GMainLoop *loop;
	XIMServer *server;
	gchar *arg_display_name = NULL, *dpy_name;
	gboolean arg_replace = FALSE, arg_verbose = FALSE;
	GOptionContext *ctx = g_option_context_new(NULL);
	GOptionEntry entries[] = {
		{"display", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &arg_display_name, N_("X display to use"), N_("DISPLAY")},
		{"replace", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_replace, N_("Replace the running XIM server with new instance."), NULL},
		{"verbose", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_verbose, N_("Output the debugging logs"), NULL},
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	GError *error = NULL;
	Display *dpy;
	IMSettingsRequest *req;
	DBusConnection *conn;
	gchar *locale, *module, *xim;
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
	dpy = XOpenDisplay(dpy_name);
	if (dpy == NULL) {
		g_print("Can't open a X display: %s\n", dpy_name);
		exit(1);
	}
	g_free(dpy_name);

	xim_init(dpy);

	XSetErrorHandler(_error_cb);
	XSetIOErrorHandler(_io_error_cb);

	quark_main_loop = g_quark_from_static_string("xim-server-main-loop");

	conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	req = imsettings_request_new(conn, IMSETTINGS_INFO_INTERFACE_DBUS);
	imsettings_request_set_locale(req, locale);
	module = imsettings_request_get_current_user_im(req, &error);
	if (module == NULL) {
		g_print("No default IM is available.\n");
		exit(1);
	}
	xim = imsettings_request_get_im_module_name(req, module, IMSETTINGS_IMM_XIM, &error);
	g_free(module);

	loop = g_main_loop_new(NULL, FALSE);
	server = _create_server(loop, dpy, xim, arg_replace, arg_verbose);
	if (server == NULL)
		exit(1);
	g_object_set_qdata(G_OBJECT (server), quark_main_loop, loop);

	dbus_bus_add_match(conn,
			   "type='signal',"
			   "interface='" IMSETTINGS_XIM_INTERFACE_DBUS "'",
			   &derror);
	dbus_connection_add_filter(conn, imsettings_xim_message_filter, server, NULL);

	g_main_loop_run(loop);

	g_object_unref(server);
	xim_destroy(dpy);
	XCloseDisplay(dpy);

	return 0;
}
