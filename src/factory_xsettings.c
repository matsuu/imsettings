/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * factory_xsettings.c
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

#include <glib.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include "xsettings-manager.h"

#ifdef GNOME_ENABLE_DEBUG
#define d(x)	x
#else
#define d(x)
#endif /* GNOME_ENABLE_DEBUG */


typedef struct _IMSettingsXSettings {
	Display           *dpy;
	XSettingsManager **managers;
	GMainLoop         *loop;
	GHashTable        *translation_table;
} IMSettingsXSettings;
typedef struct _IMSettingsGConfNotify {
	gchar                 *gconf_key;
	gchar                 *xsettings_key;
	GConfClientNotifyFunc  func;
} IMSettingsGConfNotify;
typedef struct _IMSettingsSource {
	GSource              instance;
	GPollFD              poll_fd;
	IMSettingsXSettings *settings;
} IMSettingsSource;


static gboolean _source_prepare (GSource             *source,
                                 gint                *timeout);
static gboolean _source_check   (GSource             *source);
static gboolean _source_dispatch(GSource             *source,
                                 GSourceFunc          callback,
                                 gpointer             data);
static void     _source_finalize(GSource             *source);
static void     _set_value      (IMSettingsXSettings *settings,
                                 GConfEntry          *entry);


static GSourceFuncs source_funcs = {
	_source_prepare,
	_source_check,
	_source_dispatch,
	_source_finalize
};

static gboolean
_source_prepare(GSource *source,
		gint    *timeout)
{
	IMSettingsSource *s = (IMSettingsSource *)source;

	/* just waiting for polling fd */
	*timeout = -1;

	return XPending(s->settings->dpy) > 0;
}

static gboolean
_source_check(GSource *source)
{
	IMSettingsSource *s = (IMSettingsSource *)source;
	gboolean retval = FALSE;

	if (s->poll_fd.revents & G_IO_IN)
		retval = XPending(s->settings->dpy) > 0;

	return retval;
}

static gboolean
_source_dispatch(GSource     *source,
		 GSourceFunc  callback,
		 gpointer     data)
{
	IMSettingsSource *s = (IMSettingsSource *)source;
	gint c = 0, i;

	while (XPending(s->settings->dpy)) {
		XEvent xevent;

		XNextEvent(s->settings->dpy, &xevent);

		for (i = 0; i < ScreenCount(s->settings->dpy); i++) {
			if (xsettings_manager_process_event(s->settings->managers[i], &xevent))
				c++;
		}
		if (c > 0)
			break;
	}

	return TRUE;
}

static void
_source_finalize(GSource *source)
{
}

static void
_gtk_im_module_cb(GConfClient *conf,
		  guint        cnxn_id,
		  GConfEntry  *entry,
		  gpointer     user_data)
{
	IMSettingsXSettings *settings = user_data;

	_set_value(settings, entry);
}

static void
_set_value(IMSettingsXSettings *settings,
	   GConfEntry          *entry)
{
	gint i;
	const gchar *key = gconf_entry_get_key(entry);
	GConfValue *value = gconf_entry_get_value(entry);

	for (i = 0; i < ScreenCount(settings->dpy); i++) {
		if (value == NULL) {
			xsettings_manager_delete_setting(settings->managers[i], key);
			continue;
		}
		switch (value->type) {
		    case GCONF_VALUE_STRING:
			    d(g_print("Set %s = %s\n", key, gconf_value_get_string(value)));
			    xsettings_manager_set_string(settings->managers[i],
							 g_hash_table_lookup(settings->translation_table, key),
							 gconf_value_get_string(value));
			    break;
		    case GCONF_VALUE_INT:
			    d(g_print("Set %s = %d\n", key, gconf_value_get_int(value)));
			    xsettings_manager_set_int(settings->managers[i],
						      g_hash_table_lookup(settings->translation_table, key),
						      gconf_value_get_int(value));
			    break;
		    case GCONF_VALUE_FLOAT:
		    case GCONF_VALUE_BOOL:
		    case GCONF_VALUE_SCHEMA:
		    case GCONF_VALUE_LIST:
		    case GCONF_VALUE_PAIR:
		    default:
			    g_warning("Unsupported value type: %d", value->type);
		}
	}
	for (i = 0; i < ScreenCount(settings->dpy); i++) {
		xsettings_manager_notify(settings->managers[i]);
	}
}

static void
terminate_cb(gpointer data)
{
	IMSettingsXSettings *settings = data;

	g_main_loop_quit(settings->loop);
}

int
main(int    argc,
     char **argv)
{
	GConfClient *client;
	IMSettingsXSettings *settings;
	Display *dpy;
	int i;
	IMSettingsGConfNotify notifications[] = {
		{"/desktop/gnome/interface/gtk-im-module", "Gtk/IMModule", _gtk_im_module_cb},
		{NULL, NULL}
	};
	IMSettingsGConfNotify *n;
	GPtrArray *array;
	IMSettingsSource *s;

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		g_print("Unable to open a X display\n");
		return 1;
	}
	if (xsettings_manager_check_running(dpy, DefaultScreen(dpy))) {
		g_print("XSETTINGS manager is already running.\n");
		return 1;
	}
	settings = g_new0(IMSettingsXSettings, 1);
	settings->dpy = dpy;
	settings->managers = g_new0(XSettingsManager *, ScreenCount(dpy));
	settings->loop = g_main_loop_new(NULL, FALSE);
	settings->translation_table = g_hash_table_new_full(g_str_hash,
							    g_str_equal,
							    g_free,
							    g_free);
	for (i = 0; i < ScreenCount(dpy); i++) {
		settings->managers[i] = xsettings_manager_new(dpy, i,
							      terminate_cb,
							      NULL);
		if (settings->managers[i] == NULL) {
			g_error("Could not create xsettings manager for screen %d", i);
			return 1;
		}
	}

	client = gconf_client_get_default();
	gconf_client_add_dir(client,
			     "/desktop/gnome/interface",
			     GCONF_CLIENT_PRELOAD_NONE,
			     NULL);
	array = g_ptr_array_new();
	for (n = notifications; n->gconf_key != NULL; n++) {
		GError *error = NULL;
		guint id = gconf_client_notify_add(client, n->gconf_key, n->func, settings, NULL, &error);
		GConfEntry *entry;

		g_hash_table_insert(settings->translation_table,
				    g_strdup(n->gconf_key),
				    g_strdup(n->xsettings_key));
		g_ptr_array_add(array, GUINT_TO_POINTER (id));
		if (error) {
			g_warning("%s", error->message);
			g_error_free(error);
			continue;
		}

		entry = gconf_client_get_entry(client,
					       n->gconf_key,
					       NULL,
					       TRUE,
					       &error);
		if (error) {
			g_warning("%s", error->message);
			g_error_free(error);
			continue;
		}
		_set_value(settings, entry);
	}

	s = (IMSettingsSource *)g_source_new(&source_funcs, sizeof (IMSettingsSource));
	s->poll_fd.fd = ConnectionNumber(dpy);
	s->poll_fd.events = G_IO_IN;
	s->settings = settings;

	g_source_add_poll((GSource *)s, &s->poll_fd);
	g_source_attach((GSource *)s, NULL);

	g_main_loop_run(settings->loop);

	for (i = 0; i < array->len; i++) {
		guint id = GPOINTER_TO_UINT (g_ptr_array_index(array, i));

		gconf_client_notify_remove(client, id);
	}
	for (i = 0; i < ScreenCount(dpy); i++) {
		xsettings_manager_destroy(settings->managers[i]);
	}
	g_main_loop_unref(settings->loop);
	g_object_unref(s);
	g_ptr_array_free(array, TRUE);
	g_free(settings->managers);
	g_free(settings);

	XCloseDisplay(dpy);

	return 0;
}
