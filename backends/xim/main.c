/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * main.c
 * Copyright (C) 2008-2011 Red Hat, Inc. All rights reserved.
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

#include <locale.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <libgxim/gximmessage.h>
#include <libgxim/gximmisc.h>
#include "imsettings.h"
#include "imsettings-client.h"
#include "client.h"
#include "proxy.h"
#include "utils.h"

typedef struct _Proxy {
	GMainLoop     *loop;
	GDBusServer   *server;
	GDBusNodeInfo *introspection_data;
	XimProxy      *proxy;
	gchar         *xim_name;
} Proxy;

static void handle_method_call(GDBusConnection       *connection,
			       const gchar           *sender,
			       const gchar           *object_path,
			       const gchar           *interface_name,
			       const gchar           *method_name,
			       GVariant              *parameters,
			       GDBusMethodInvocation *invocation,
			       gpointer               user_data);

static const gchar introspection_xml[] =
	"<node>"
	"  <interface name='com.redhat.imsettings.xim'>"
	"    <method name='SwitchXIM'>"
	"      <arg type='s' name='name' direction='in'/>"
	"      <arg type='b' name='ret' direction='out'/>"
	"    </method>"
	"  </interface>"
	"</node>";
static const GDBusInterfaceVTable __iface_vtable = {
	handle_method_call,
	NULL,
	NULL,
};

/*< private >*/
static void
_quit_cb(XimProxy *proxy,
	 gpointer  data)
{
	GMainLoop *loop = data;

	g_print("*** Exiting...\n");

	g_main_loop_quit(loop);
}

static gboolean
new_connection_cb(GDBusServer     *server,
		  GDBusConnection *connection,
		  gpointer         user_data)
{
	Proxy *proxy = user_data;
	guint registration_id;

	g_object_ref(connection);
	registration_id = g_dbus_connection_register_object(connection,
							    IMSETTINGS_XIM_PATH_DBUS,
							    proxy->introspection_data->interfaces[0],
							    &__iface_vtable,
							    proxy, /* user_data */
							    NULL, /* user_data_free_func */
							    NULL /* GError */);

	return TRUE;
}

static void
handle_method_call(GDBusConnection       *connection,
		   const gchar           *sender,
		   const gchar           *object_path,
		   const gchar           *interface_name,
		   const gchar           *method_name,
		   GVariant              *parameters,
		   GDBusMethodInvocation *invocation,
		   gpointer               user_data)
{
	Proxy *proxy = user_data;

	if (g_strcmp0(method_name, "StopService") == 0) {
		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("(b)", TRUE));
		g_main_loop_quit(proxy->loop);
	} else if (g_strcmp0(method_name, "SwitchXIM") == 0) {
		const gchar *im;

		g_variant_get(parameters, "(&s)", &im);
		g_xim_message_debug(G_XIM_CORE (proxy->proxy)->message, "dbus/event",
				    "Changing XIM server: '%s'->'%s'\n",
				    proxy->xim_name, im);
		g_free(proxy->xim_name);
		proxy->xim_name = g_strdup(im);
		g_object_set(proxy->proxy, "connect_to", im, NULL);
		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("(b)", TRUE));
	}
}

/*< public >*/
int
main(int    argc,
     char **argv)
{
	gchar *arg_display_name = NULL, *arg_xim = NULL, *dpy_name, *arg_address = NULL;
	gboolean arg_replace = FALSE, arg_verbose = FALSE;
	GOptionContext *ctx = g_option_context_new(NULL);
	GOptionEntry entries[] = {
		{"address", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_STRING, &arg_address, N_("D-Bus address to use"), N_("ADDRESS")},
		{"display", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &arg_display_name, N_("X display to use"), N_("DISPLAY")},
		{"replace", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_replace, N_("Replace the running XIM server with new instance."), NULL},
		{"verbose", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_verbose, N_("Output the debugging logs"), NULL},
		{"xim", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &arg_xim, N_("XIM server connects to, for debugging purpose only"), N_("XIM")},
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	GError *err = NULL;
	gchar *guid;
	GdkDisplay *dpy;
	Proxy *proxy;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, IMSETTINGS_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif /* HAVE_BIND_TEXTDOMAIN_CODESET */
	textdomain (GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */

	setlocale(LC_ALL, "");

	g_type_init();
	gdk_init(&argc, &argv);
	g_xim_init();

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

	dpy_name = xim_substitute_display_name(arg_display_name);
	dpy = gdk_display_open(dpy_name);
	if (dpy == NULL) {
		g_printerr("Can't open a X display: %s\n", dpy_name);
		exit(1);
	}
	g_free(dpy_name);
	if (arg_address == NULL) {
		g_printerr("No D-Bus address\n");
		exit(1);
	}

	proxy = g_new0(Proxy, 1);
	proxy->loop = g_main_loop_new(NULL, FALSE);
	proxy->xim_name = g_strdup("none");
	proxy->proxy = xim_proxy_new(dpy, "imsettings", proxy->xim_name);
	if (!proxy->proxy)
		goto finalize;
	g_signal_connect(proxy->proxy, "destroy",
			 G_CALLBACK (_quit_cb),
			 proxy->loop);
	if (!xim_proxy_take_ownership(proxy->proxy, arg_replace, &err)) {
		g_printerr("Unable to take an ownership: %s\n",
			   err->message);
		g_error_free(err);
		goto finalize;
	}

	proxy->introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
	guid = g_dbus_generate_guid();
	proxy->server = g_dbus_server_new_sync(arg_address,
					       G_DBUS_SERVER_FLAGS_NONE,
					       guid,
					       NULL,
					       NULL,
					       &err);
	g_dbus_server_start(proxy->server);
	g_free(guid);

	if (!proxy->server) {
		g_printerr("Unable to create server at address %s: %s\n",
			   arg_address,
			   err->message);
		g_error_free(err);
		goto finalize;
	}
	g_print("IMSETTINGS_XIM_ADDRESS=%s\n", g_dbus_server_get_client_address(proxy->server));
	g_signal_connect(proxy->server, "new-connection",
			 G_CALLBACK (new_connection_cb),
			 proxy);

	g_main_loop_run(proxy->loop);

  finalize:
	if (proxy->server)
		g_object_unref(proxy->server);
	if (proxy->proxy)
		g_object_unref(proxy->proxy);
	if (proxy->introspection_data)
		g_object_unref(proxy->introspection_data);
	g_free(proxy->xim_name);
	g_free(proxy);

	gdk_display_close(dpy);

	g_xim_finalize();

	return 0;
}
