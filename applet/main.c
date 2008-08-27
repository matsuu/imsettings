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

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gconf/gconf.h>
#include <libnotify/notify.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-info.h"
#include "imsettings/imsettings-request.h"
#include "radiomenuitem.h"

#ifdef GNOME_ENABLE_DEBUG
#define d(x)	x
#else
#define d(x)
#endif

typedef struct _IMApplet {
	GtkStatusIcon      *status_icon;
	DBusConnection     *conn;
	IMSettingsRequest  *req;
	IMSettingsRequest  *req_info;
	NotifyNotification *notify;
	gchar              *current_im;
	gchar              *process_im;
	gboolean            need_notification;
	gboolean            updated;
	gboolean            is_enabled;
	/* configurable */
	gboolean            update_xinputrc;
	KeyCode             keycode;
	guint               mods;
} IMApplet;


static void _update_icon(IMApplet *applet);


/*
 * Private functions
 */
static void
notify_notification(IMApplet    *applet,
		    const gchar *summary,
		    const gchar *body,
		    gint         timeout)
{
	notify_notification_update(applet->notify,
				   _(summary),
				   _(body),
				   NULL);
	notify_notification_clear_actions(applet->notify);
	notify_notification_set_timeout(applet->notify, timeout * 1000);
	notify_notification_set_category(applet->notify, "x-imsettings-notice");
	notify_notification_show(applet->notify, NULL);
}

static gboolean
_check_version(IMApplet *applet)
{
	gint n_retry = 0;
	GError *error = NULL;

  retry:
	if (imsettings_request_get_version(applet->req, &error) != IMSETTINGS_SETTINGS_DAEMON_VERSION) {
		if (n_retry > 0) {
			gchar *body = g_strdup_printf(N_("Mismatch the version of im-settings-daemon.\ndetails: %s"),
						      error ? error->message : _("None"));

			notify_notification(applet, N_("Error"), body, 2);
			g_free(body);
			if (error)
				g_error_free(error);

			return FALSE;
		}
		/* version is inconsistent. try to reload the process */
		imsettings_request_reload(applet->req, TRUE);
		/* XXX */
		sleep(1);
		n_retry++;
		goto retry;
	}

	return TRUE;
}

static void
_start_process_cb(DBusGProxy *proxy,
		  gboolean    ret,
		  GError     *error,
		  gpointer    data)
{
	IMApplet *applet = data;

	if (!ret) {
		gchar *body = g_strdup_printf(N_("Unable to start %s.\nDetails: %s"),
					      applet->process_im,
					      error ? error->message : N_("None"));

		notify_notification(applet, N_("Error"), body, 2);
		g_free(body);
	} else {
		gchar *body;

		body = g_strdup_printf(N_("Connected to %s"), applet->process_im);
		g_free(applet->current_im);
		applet->current_im = g_strdup(applet->process_im);
		applet->is_enabled = TRUE;
		_update_icon(applet);
		notify_notification(applet, N_("Information"), body, 2);
		g_free(body);
	}
}

static void
_stop_process_cb(DBusGProxy *proxy,
		 gboolean    ret,
		 GError     *error,
		 gpointer    data)
{
	IMApplet *applet = data;

	if (!ret) {
		gchar *body = g_strdup_printf(N_("Unable to stop %s.\nDetails: %s"),
					      applet->process_im,
					      error ? error->message : N_("None"));

		notify_notification(applet, N_("Error"), body, 2);
		g_free(body);
	} else {
		g_free(applet->current_im);
		applet->current_im = NULL;
		applet->is_enabled = FALSE;

		if (applet->need_notification) {
			gchar *body;

			body = g_strdup(N_("Disconnected from Input Method"));
			applet->current_im = g_strdup("none");
			_update_icon(applet);
			notify_notification(applet, N_("Information"), body, 2);
			g_free(body);
		}
	}
}

static gboolean
_start_process(IMApplet *applet)
{
	if (applet->process_im &&
	    strcmp(applet->process_im, "none") == 0)
		return TRUE;

	if (!_check_version(applet))
		return FALSE;

	if (!imsettings_request_start_im_async(applet->req,
					       applet->process_im,
					       applet->update_xinputrc,
					       _start_process_cb, applet)) {
		gchar *body = g_strdup_printf(N_("Unable to start %s. maybe DBus related issue."),
					      applet->process_im);

		notify_notification(applet, N_("Error"), body, 2);
		g_free(body);

		return FALSE;
	}
	applet->need_notification = TRUE;

	return TRUE;
}

static gboolean
_stop_process(IMApplet *applet,
	      gboolean  need_notification)
{
	if (!_check_version(applet))
		return FALSE;

	if (!imsettings_request_stop_im_async(applet->req,
					      applet->process_im,
					      applet->update_xinputrc,
					      TRUE,
					      _stop_process_cb, applet)) {
		gchar *body = g_strdup_printf(N_("Unable to stop %s. maybe DBus related issue."),
					      applet->process_im);

		notify_notification(applet, N_("Error"), body, 2);
		g_free(body);

		return FALSE;
	}
	applet->need_notification = need_notification;

	return TRUE;
}

static void
_toggled(GtkCheckMenuItem *item,
	 gpointer          data)
{
	IMApplet *applet = data;
	gchar *name;

	name = g_object_get_data(G_OBJECT (item), "short_desc");
	if (gtk_check_menu_item_get_active(item)) {
		if (!applet->current_im ||
		    strcmp(name, applet->current_im)) {
			g_free(applet->process_im);
			applet->process_im = g_strdup(name);

			_start_process(applet);
		}
	} else {
		if (applet->current_im &&
		    strcmp(name, applet->current_im) == 0) {
			g_free(applet->process_im);
			applet->process_im = g_strdup(name);

			_stop_process(applet, TRUE);
		}
	}
}

static void
_popup_menu(GtkStatusIcon *status_icon,
	    guint          button,
	    guint32        activate_time,
	    gpointer       data)
{
	IMApplet *applet = data;
	GPtrArray *array;
	gint i;
	GtkWidget *menu, *item, *prev_item;
	gchar *current_im;

	menu = gtk_menu_new();

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);

	array = imsettings_request_get_info_objects(applet->req_info, NULL);
	if (applet->current_im)
		current_im = g_strdup(applet->current_im);
	else
		current_im = imsettings_request_what_im_is_running(applet->req, NULL);

	prev_item = item = imsettings_radio_menu_item_new_from_stock(NULL,
								     GTK_STOCK_DISCONNECT,
								     NULL);
	gtk_label_set_text(GTK_LABEL (gtk_bin_get_child(GTK_BIN (item))),
			   _("Disconnected"));
	g_object_set_data_full(G_OBJECT (item), "short_desc", g_strdup("none"), g_free);
	g_signal_connect(item, "toggled",
			 G_CALLBACK (_toggled),
			 applet);
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
	for (i = 0; array && i < array->len; i++) {
		GtkWidget *item, *image;
		IMSettingsInfo *info;
		const gchar *name, *iconfile;
		GdkPixbuf *pixbuf = NULL;

		info = IMSETTINGS_INFO (g_ptr_array_index(array, i));
		name = imsettings_info_get_short_desc(info);
		iconfile = imsettings_info_get_icon_file(info);
		if (iconfile && g_file_test(iconfile, G_FILE_TEST_EXISTS)) {
			pixbuf = gdk_pixbuf_new_from_file_at_scale(iconfile,
								   18, 18,
								   TRUE, NULL);
		} else {
			gchar *p = g_build_filename(PIXMAP_PATH, "unknown.png", NULL);

			pixbuf = gdk_pixbuf_new_from_file_at_scale(p, 18, 18, TRUE, NULL);
		}
		image = gtk_image_new_from_pixbuf(pixbuf);
		item = imsettings_radio_menu_item_new_from_widget_with_image(IMSETTINGS_RADIO_MENU_ITEM (prev_item),
									     name,
									     image);
		g_object_set_data_full(G_OBJECT (item), "short_desc", g_strdup(name), g_free);
		g_signal_connect(item, "toggled",
				 G_CALLBACK (_toggled),
				 applet);
		prev_item = item;
		gtk_menu_shell_append(GTK_MENU_SHELL (menu), item);
		if (strcmp(name, current_im) == 0) {
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM (item), TRUE);
			applet->current_im = g_strdup(name);
		}
		g_object_unref(info);
	}
	g_ptr_array_free(array, TRUE);
	gtk_widget_show_all(menu);
	gtk_menu_popup(GTK_MENU (menu), NULL, NULL,
		       gtk_status_icon_position_menu, applet->status_icon,
		       button, activate_time);

	g_free(current_im);
}

static void
_update_icon(IMApplet *applet)
{
	if (strcmp(applet->current_im, "none") == 0) {
		gtk_status_icon_set_from_stock(applet->status_icon, GTK_STOCK_DISCONNECT);
		gtk_status_icon_set_tooltip(applet->status_icon,
					    _("Click to connect to Input Method"));
	} else {
		gtk_status_icon_set_from_stock(applet->status_icon, GTK_STOCK_CONNECT);
		gtk_status_icon_set_tooltip(applet->status_icon,
					    _("Click to disconnect from Input Method"));
	}
}

static void
_activate(GtkStatusIcon *status_icon,
	  gpointer       data)
{
	IMApplet *applet = data;
	gchar *name;

	name = imsettings_request_what_im_is_running(applet->req, NULL);
	if (!applet->updated && name && strcmp(name, "none")) {
		applet->updated = TRUE;
		applet->current_im = g_strdup(name);
	}
	if (applet->updated && applet->is_enabled) {
		applet->updated = TRUE;
		g_free(applet->process_im);
		applet->process_im = g_strdup("none");

		_stop_process(applet, TRUE);
	} else {
		gchar *current_im = imsettings_request_get_current_user_im(applet->req_info, NULL);

		applet->updated = TRUE;
		g_free(applet->process_im);
		applet->process_im = current_im;

		_start_process(applet);
	}
	g_free(name);
}

static GdkFilterReturn
filter_func(GdkXEvent *gdk_xevent,
	    GdkEvent  *event,
	    gpointer   data)
{
	IMApplet *applet = data;
	GdkFilterReturn retval = GDK_FILTER_CONTINUE;
	XEvent *xevent = (XEvent *)gdk_xevent;
	guint event_mods;

	switch (xevent->type) {
	    case KeyPress:
		    event_mods = xevent->xkey.state & ~LockMask;
		    g_print("Pressed shortcut key\n");
		    if (xevent->xkey.keycode == applet->keycode &&
			event_mods == applet->mods) {
			    _activate(applet->status_icon, applet);
		    }
		    break;
	}

	return retval;
}

static void
_do_not_show_tips_again_cb(NotifyNotification *notify,
			   gchar              *action,
			   gpointer            user_data)
{
	GConfEngine *engine;
	GConfValue *val;

	engine = gconf_engine_get_default();
	val = gconf_value_new(GCONF_VALUE_BOOL);
	gconf_value_set_bool(val, FALSE);
	gconf_engine_set(engine, "/apps/imsettings-applet/notify_tips", val, NULL);
	gconf_value_free(val);
	gconf_engine_unref(engine);
	d(g_print("disabled notification\n"));
}

static gboolean
_delay_notify(gpointer data)
{
	IMApplet *applet = data;
	GConfEngine *engine = gconf_engine_get_default();
	GConfValue *val;

	val = gconf_engine_get(engine, "/apps/imsettings-applet/notify_tips", NULL);
	if (val) {
		if (!gconf_value_get_bool(val)) {
			d(g_print("No notification\n"));
			return FALSE;
		}
	}
	notify_notification_update(applet->notify,
				   _("Tips"),
				   _("Press Alt+F10 or Left-click on the icon to connect to/disconnect from Input Method.\nRight-click to show up the Input Method menu."),
				   NULL);
	notify_notification_clear_actions(applet->notify);
	notify_notification_set_timeout(applet->notify, 10*1000);
	notify_notification_set_category(applet->notify, "x-imsettings-tips-shortcutkey");
	notify_notification_add_action(applet->notify,
				       "notips",
				       _("Do not show this again"),
				       _do_not_show_tips_again_cb,
				       applet,
				       NULL);
	notify_notification_show(applet->notify, NULL);

	return FALSE;
}

static IMApplet *
_create_applet(void)
{
	IMApplet *applet;
	gchar *name;

	applet = g_new0(IMApplet, 1);
	applet->is_enabled = FALSE;
	applet->updated = FALSE;
	applet->update_xinputrc = FALSE;
	applet->status_icon = gtk_status_icon_new_from_stock(GTK_STOCK_DISCONNECT);
	applet->conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	if (applet->conn == NULL) {
		g_printerr("Unable to establish the dbus session bus.\n");

		return NULL;
	}
	applet->req = imsettings_request_new(applet->conn, IMSETTINGS_INTERFACE_DBUS);
	applet->req_info = imsettings_request_new(applet->conn, IMSETTINGS_INFO_INTERFACE_DBUS);

	g_signal_connect(applet->status_icon, "popup_menu",
			 G_CALLBACK (_popup_menu),
			 applet);
	g_signal_connect(applet->status_icon, "activate",
			 G_CALLBACK (_activate),
			 applet);

	name = imsettings_request_what_im_is_running(applet->req, NULL);
	if (!applet->current_im)
		applet->current_im = g_strdup(name);
	g_free(name);
	_update_icon(applet);

	/* setup shortcut key */
	G_STMT_START {
//		GdkKeymap *keymap = gdk_keymap_get_default();
		GdkWindow *rootwin = gdk_get_default_root_window();

		applet->keycode = XKeysymToKeycode(GDK_WINDOW_XDISPLAY (rootwin),
						   XK_F10);
		applet->mods = Mod1Mask;
		XGrabKey(GDK_WINDOW_XDISPLAY (rootwin),
			 applet->keycode, applet->mods,
			 GDK_WINDOW_XWINDOW (rootwin),
			 False,
			 GrabModeAsync,
			 GrabModeAsync);

		gdk_window_add_filter(rootwin,
				      filter_func,
				      applet);
	} G_STMT_END;

	notify_init("imsettings-applet");
	applet->notify = notify_notification_new_with_status_icon("foo", "bar", NULL, applet->status_icon);
	g_timeout_add_seconds(1, _delay_notify, applet);

	return applet;
}

static void
_destroy_applet(IMApplet *applet)
{
	notify_notification_close(applet->notify, NULL);
	g_object_unref(applet->req_info);
	g_object_unref(applet->req);
	if (applet->conn)
		dbus_connection_unref(applet->conn);
	g_object_unref(applet->status_icon);
	g_free(applet);
}

/*
 * Public functions
 */
int
main(int    argc,
     char **argv)
{
	IMApplet *applet;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, IMAPPLET_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif /* HAVE_BIND_TEXTDOMAIN_CODESET */
	textdomain (GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */

	gtk_init(&argc, &argv);

	applet = _create_applet();
	if (applet == NULL)
		goto end;

	gtk_main();

  end:
	_destroy_applet(applet);

	return 0;
}
