/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * main.c
 * Copyright (C) 2008-2010 Red Hat, Inc. All rights reserved.
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
#include <glib/gi18n.h>
#include "imsettings-server.h"
#include "imsettings-utils.h"

static void _sig_handler(int signum);

static IMSettingsServer *server = NULL;
static gboolean running = TRUE;

/*< private >*/
static int
_setup_signal(int signum)
{
	struct sigaction sa;

	sa.sa_handler = _sig_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	return sigaction(signum, &sa, NULL);
}

static void
_sig_handler(int signum)
{
	if (!server) {
		g_print("\nReceived a signal %d. exiting...\n", signum);
		exit(1);
	}
	switch (signum) {
	    case SIGTERM:
		    _setup_signal(SIGTERM);
		    break;
	    case SIGINT:
		    running = FALSE;
		    _setup_signal(SIGINT);
		    break;
	    default:
		    g_print("\nUnknown signal (%d) received. ignoring.\n", signum);
		    break;
	}
}

static void
_disconnected_cb(IMSettingsServer *server)
{
	running = FALSE;
}

static gboolean
_loop_cb(gpointer data)
{
	if (!running) {
		GMainLoop *loop = data;

		g_main_loop_quit(loop);

		return FALSE;
	}

	return TRUE;
}

/*< public >*/
int
main(int argc, char **argv)
{
	GError *err = NULL;
	gboolean arg_replace = FALSE, arg_no_logfile = FALSE;
	gchar *arg_xinputrcdir = NULL, *arg_xinputdir = NULL, *arg_homedir = NULL, *arg_moduledir = NULL;
	GMainLoop *loop;
	GOptionContext *ctx = g_option_context_new(NULL);
	GOptionEntry entries[] = {
		{"replace", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_replace, N_("Replace the instance of the imsettings daemon."), NULL},
		{"xinputrcdir", 0, G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &arg_xinputrcdir, N_("Set the system-wide xinputrc directory (for debugging purpose)"), N_("DIR")},
		{"xinputdir", 0, G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &arg_xinputdir, N_("Set the IM configuration directory (for debugging purpose)"), N_("DIR")},
		{"homedir", 0, G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &arg_homedir, N_("Set a home directory (for debugging purpose)"), N_("DIR")},
		{"moduledir", 0, G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &arg_moduledir, N_("Set the imsettings module directory (for debugging purpose)"), N_("DIR")},
		{"no-logfile", 0, G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_no_logfile, N_("Do not create a log file."), NULL},
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	GDBusConnection *connection;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, IMSETTINGS_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif /* HAVE_BIND_TEXTDOMAIN_CODESET */
	textdomain (GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */

	g_type_init();

	/* deal with the arguments */
	g_option_context_add_main_entries(ctx, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse(ctx, &argc, &argv, &err)) {
		if (err != NULL) {
			g_print("%s\n", err->message);
		} else {
			g_warning(_("Unknown error in parsing the command lines."));
		}
		exit(1);
	}
	g_option_context_free(ctx);

	connection = g_bus_get_sync(G_BUS_TYPE_SESSION,
				    NULL,
				    &err);
	if (err) {
		g_printerr("%s\n", err->message);
		exit(1);
	}

	g_dbus_error_register_error(IMSETTINGS_GERROR, 3, "com.redhat.imsettings.Error");

	_setup_signal(SIGTERM);
	_setup_signal(SIGINT);

	server = imsettings_server_new(connection,
				       arg_homedir,
				       arg_xinputrcdir,
				       arg_xinputdir,
				       arg_moduledir);
	if (!server) {
		g_printerr("Unable to create a server instance.\n");
		exit(1);
	}
	loop = g_main_loop_new(NULL, FALSE);

	g_signal_connect(server, "disconnected",
			 G_CALLBACK (_disconnected_cb),
			 NULL);

	g_object_set(G_OBJECT (server),
		     "logging", !arg_no_logfile,
		     NULL);

	g_idle_add(_loop_cb, loop);
	imsettings_server_start(server, arg_replace);
	g_main_loop_run(loop);

	g_print("\nExiting...\n");

	g_object_unref(server);
	g_object_unref(connection);
	g_main_loop_unref(loop);

	/* invoking _exit(2) instead of just returning or invoking exit(2)
	 * to avoid segfault in a function added by atexit(3) in GConf.
	 * Since GConf is dlopen'd, the function pointer isn't a valid
	 * at atexit(2) anymore.
	 */
	_exit(0);
}
