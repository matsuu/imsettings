/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-xsettings.c
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

#include <gdk/gdkx.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include "xsettings-common.h"
#include "xsettings-manager.h"
#include "imsettings-xsettings.h"

#ifdef GNOME_ENABLE_DEBUG
#define d(x)	x
#else
#define d(x)
#endif /* GNOME_ENABLE_DEBUG */


struct _IMSettingsXSettings {
	GSource            instance;
	GPollFD            poll_fd;
	GdkDisplay        *display;
	XSettingsManager **managers;
	GHashTable        *translation_table;
	GPtrArray         *array;
};
typedef struct _IMSettingsGConfNotify {
	gchar                 *gconf_key;
	gchar                 *xsettings_key;
	GConfClientNotifyFunc  func;
} IMSettingsGConfNotify;

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

/*
 * Private functions
 */
static gboolean
_source_prepare(GSource *source,
		gint    *timeout)
{
	IMSettingsXSettings *s = (IMSettingsXSettings *)source;

	/* just waiting for polling fd */
	*timeout = -1;

	return XPending(GDK_DISPLAY_XDISPLAY (s->display)) > 0;
}

static gboolean
_source_check(GSource *source)
{
	IMSettingsXSettings *s = (IMSettingsXSettings *)source;
	gboolean retval = FALSE;

	if (s->poll_fd.revents & G_IO_IN)
		retval = XPending(GDK_DISPLAY_XDISPLAY (s->display)) > 0;

	return retval;
}

static gboolean
_source_dispatch(GSource     *source,
		 GSourceFunc  callback,
		 gpointer     data)
{
	IMSettingsXSettings *s = (IMSettingsXSettings *)source;
	gint c = 0, i;

	while (XPending(GDK_DISPLAY_XDISPLAY (s->display))) {
		XEvent xevent;
		gint n_screens = gdk_display_get_n_screens(s->display);

		XNextEvent(GDK_DISPLAY_XDISPLAY (s->display), &xevent);

		for (i = 0; i < n_screens; i++) {
			if (xsettings_manager_process_event(s->managers[i], &xevent))
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
	gint i, n_screens = gdk_display_get_n_screens(settings->display);
	const gchar *key = gconf_entry_get_key(entry);
	GConfValue *value = gconf_entry_get_value(entry);

	for (i = 0; i < n_screens; i++) {
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
	for (i = 0; i < n_screens; i++) {
		xsettings_manager_notify(settings->managers[i]);
	}
}

/*
 * Public functions
 */
gboolean
imsettings_xsettings_is_available(GdkDisplay *dpy)
{
	g_return_val_if_fail (GDK_IS_DISPLAY (dpy), FALSE);

	return xsettings_manager_check_running(GDK_DISPLAY_XDISPLAY (dpy),
					       gdk_screen_get_number(gdk_display_get_default_screen(dpy)));
}

IMSettingsXSettings *
imsettings_xsettings_new(GdkDisplay *display,
			 GFreeFunc   term_func,
			 gpointer    data)
{
	IMSettingsXSettings *retval;
	IMSettingsGConfNotify notifications[] = {
		{"/desktop/gnome/interface/gtk-im-module", "Gtk/IMModule", _gtk_im_module_cb},
		{NULL, NULL}
	};
	IMSettingsGConfNotify *n;
	GConfClient *client;
	gint i, n_screens;

	g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
	g_return_val_if_fail (!imsettings_xsettings_is_available(display), NULL);

	retval = (IMSettingsXSettings *)g_source_new(&source_funcs, sizeof (IMSettingsXSettings));
	retval->display = g_object_ref(display);

	n_screens = gdk_display_get_n_screens(display);
	retval->managers = g_new0(XSettingsManager *, n_screens);
	retval->translation_table = g_hash_table_new_full(g_str_hash,
							  g_str_equal,
							  g_free,
							  g_free);
	for (i = 0; i < n_screens; i++) {
		retval->managers[i] = xsettings_manager_new(GDK_DISPLAY_XDISPLAY (display), i,
							    term_func,
							    data);
		if (retval->managers[i] == NULL) {
			g_printerr("Could not create xsettings manager for screen %d", i);
			goto fail;
		}
	}

	client = gconf_client_get_default();
	gconf_client_add_dir(client,
			     "/desktop/gnome/interface",
			     GCONF_CLIENT_PRELOAD_NONE,
			     NULL);
	retval->array = g_ptr_array_new();
	for (n = notifications; n->gconf_key != NULL; n++) {
		GError *error = NULL;
		guint id = gconf_client_notify_add(client, n->gconf_key, n->func, retval, NULL, &error);
		GConfEntry *entry;

		g_hash_table_insert(retval->translation_table,
				    g_strdup(n->gconf_key),
				    g_strdup(n->xsettings_key));
		g_ptr_array_add(retval->array, GUINT_TO_POINTER (id));
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
		_set_value(retval, entry);
	}

	retval->poll_fd.fd = ConnectionNumber(GDK_DISPLAY_XDISPLAY (display));
	retval->poll_fd.events = G_IO_IN;

	g_source_add_poll((GSource *)retval, &retval->poll_fd);
	g_source_attach((GSource *)retval, NULL);

	g_object_unref(G_OBJECT (client));

	return retval;
  fail:
	imsettings_xsettings_free(retval);

	return NULL;
}

void
imsettings_xsettings_free(IMSettingsXSettings *xsettings)
{
	gint i;
	GConfClient *client;

	g_return_if_fail (xsettings != NULL);

	client = gconf_client_get_default();

	if (xsettings->array) {
		for (i = 0; i < xsettings->array->len; i++) {
			guint id = GPOINTER_TO_UINT (g_ptr_array_index(xsettings->array, i));

			gconf_client_notify_remove(client, id);
		}
		g_ptr_array_free(xsettings->array, TRUE);
	}
	if (xsettings->managers) {
		for (i = 0; i < gdk_display_get_n_screens(xsettings->display); i++) {
			xsettings_manager_destroy(xsettings->managers[i]);
		}
		g_free(xsettings->managers);
	}
	if (xsettings->translation_table)
		g_hash_table_destroy(xsettings->translation_table);
	if (xsettings->display)
		g_object_unref(xsettings->display);
	g_source_remove_poll((GSource *)xsettings, &xsettings->poll_fd);
	g_source_destroy((GSource *)xsettings);
}
