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
#include <gdk/gdkkeysyms.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <libnotify/notify.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-info.h"
#include "imsettings/imsettings-request.h"
#ifdef ENABLE_XIM
#include <libgxim/gximmessage.h>
#include <libgxim/gximmisc.h>
#include "client.h"
#include "proxy.h"
#include "utils.h"
#endif
#ifdef ENABLE_XSETTINGS
#include "imsettings/imsettings-xsettings.h"
#endif
#include "eggaccelerators.h"
#include "radiomenuitem.h"


#ifndef d
#ifdef GNOME_ENABLE_DEBUG
#define d(x)	x
#else
#define d(x)
#endif /* GNOME_ENABLE_DEBUG */
#endif


typedef struct _IMApplet {
	GtkStatusIcon       *status_icon;
	GtkWidget           *dialog;
	GtkWidget           *entry_grabkey;
	GtkWidget           *close_button;
	DBusConnection      *conn;
	IMSettingsRequest   *req;
	IMSettingsRequest   *req_info;
	NotifyNotification  *notify;
	gchar               *current_im;
	gchar               *process_im;
	gboolean             need_notification;
	gboolean             is_enabled;
	gboolean             need_update_xinputrc;
	GtkWidget           *checkbox_showicon;
#ifdef ENABLE_XIM
	XimProxy            *server;
	gchar               *xim_server;
#endif
#ifdef ENABLE_XSETTINGS
	IMSettingsXSettings *xsettings;
	GtkWidget           *checkbox_xsettings;
	gboolean             is_xsettings_manager_enabled;
	gboolean             is_another_xsettings_manager_running;
#endif
	KeyCode              keycode;
	gboolean             watch_accel;
	/* configurable */
	guint                keyval;
	guint                modifiers;
} IMApplet;


static void   _update_icon            (IMApplet *applet);
static gchar *_get_acceleration_key   (IMApplet *applet);
static void   _preference_update_entry(IMApplet *applet);

#ifdef ENABLE_XIM
static GQuark quark_applet = 0;
#endif
static guint num_lock_mask, caps_lock_mask, scroll_lock_mask;

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
			gchar *body = g_strdup_printf("%s",
						      error ? error->message : N_("No detailed information"));

			g_printerr("%s\n", body);
			notify_notification(applet, N_("Mismatch the version of im-settings-daemon"), body, 5);
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
		gchar *header = g_strdup_printf(_("Unable to start %s"),
						applet->process_im);
		gchar *body = g_strdup_printf("%s",
					      error ? error->message : N_("No detailed information"));

		g_printerr("%s: %s\n", header, body);
		notify_notification(applet, header, body, 5);
		g_free(header);
		g_free(body);
	} else {
		gchar *body, *notice;

		if (applet->need_update_xinputrc) {
			notice = g_strdup(N_(" and the default Input Method has been changed. if you need to change that to anything else, please use im-chooser."));
			applet->need_update_xinputrc = FALSE;
		} else {
			notice = g_strdup("");
		}
		body = g_strdup_printf(N_("Connected to %s.%s"), applet->process_im, notice);
		g_free(applet->current_im);
		applet->current_im = g_strdup(applet->process_im);
		applet->is_enabled = TRUE;
		_update_icon(applet);
		notify_notification(applet, N_("Information"), body, 5);
		g_free(notice);
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
		gchar *header = g_strdup_printf(_("Unable to stop %s"),
						applet->process_im);
		gchar *body = g_strdup_printf("%s",
					      error ? error->message : N_("No detailed information"));

		g_printerr("%s: %s\n", header, body);
		notify_notification(applet, header, body, 5);
		g_free(header);
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
			notify_notification(applet, N_("Information"), body, 5);
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
					       applet->need_update_xinputrc,
					       _start_process_cb, applet)) {
		gchar *header = g_strdup_printf(_("Unable to start %s"),
						applet->process_im);

		g_printerr("%s: maybe DBus related issue.\n", header);
		notify_notification(applet, header, N_("maybe DBus related issue."), 5);
		g_free(header);

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
					      FALSE,
					      TRUE,
					      _stop_process_cb, applet)) {
		gchar *header = g_strdup_printf(_("Unable to stop %s"),
						applet->process_im);

		g_printerr("%s: maybe DBus related issue.\n", header);
		notify_notification(applet, header, N_("maybe DBus related issue."), 5);
		g_free(header);

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
_preference_showicon_toggled(GtkToggleButton *button,
			     gpointer         data)
{
	IMApplet *applet = data;
	GConfClient *client = gconf_client_get_default();
	GConfValue *val;
	GError *error = NULL;

	val = gconf_value_new(GCONF_VALUE_BOOL);
	if (gtk_toggle_button_get_active(button)) {
		gconf_value_set_bool(val, TRUE);
	} else {
		gconf_value_set_bool(val, FALSE);
	}
	gconf_client_set(client, "/apps/imsettings-applet/show_icon",
			 val, &error);
	if (error) {
		notify_notification(applet, N_("Unable to store a value to GConf"), error->message, 5);
		g_error_free(error);
	}
	gconf_value_free(val);
	g_object_unref(client);
}

static void
_gconf_show_icon_cb(GConfClient *conf,
		    guint        cnxn_id,
		    GConfEntry  *entry,
		    gpointer     user_data)
{
	IMApplet *applet = user_data;
	GConfValue *val = gconf_entry_get_value(entry);

	gtk_status_icon_set_visible(applet->status_icon, gconf_value_get_bool(val));
}

#ifdef ENABLE_XSETTINGS
static void
_xsettings_terminated(gpointer data)
{
	IMApplet *applet = data;

	imsettings_xsettings_free(applet->xsettings);
	applet->xsettings = NULL;
	applet->is_xsettings_manager_enabled = FALSE;
}

static void
_xsettings_update(IMApplet *applet)
{
	GConfClient *client = gconf_client_get_default();
	GConfValue *val;
	GError *error = NULL;

	val = gconf_client_get(client, "/apps/imsettings-applet/xsettings_manager", &error);
	if (error) {
		notify_notification(applet, N_("Unable to get a value from GConf"), error->message, 5);
		g_error_free(error);

		return;
	}
	if (g_ascii_strcasecmp(gconf_value_get_string(val), "auto") == 0) {
		applet->is_xsettings_manager_enabled = TRUE;
		if (applet->xsettings == NULL) {
			if (imsettings_xsettings_is_available(gdk_display_get_default())) {
				applet->is_another_xsettings_manager_running = TRUE;
			} else {
				applet->is_another_xsettings_manager_running = FALSE;
				applet->xsettings = imsettings_xsettings_new(gdk_display_get_default(),
									     _xsettings_terminated,
									     applet);
			}
		}
	} else {
		applet->is_xsettings_manager_enabled = FALSE;
		if (applet->xsettings) {
			imsettings_xsettings_free(applet->xsettings);
			applet->xsettings = NULL;
			applet->is_another_xsettings_manager_running = FALSE;
		}
			
	}
	gconf_value_free(val);
	g_object_unref(G_OBJECT (client));
}

static void
_preference_xsettings_toggled(GtkToggleButton *button,
			      gpointer         data)
{
	IMApplet *applet = data;
	GConfClient *client = gconf_client_get_default();
	GConfValue *val;
	GError *error = NULL;

	val = gconf_value_new(GCONF_VALUE_STRING);
	if (gtk_toggle_button_get_active(button)) {
		gconf_value_set_string(val, "auto");
	} else {
		gconf_value_set_string(val, "disabled");
	}
	gconf_client_set(client, "/apps/imsettings-applet/xsettings_manager",
			 val, &error);
	if (error) {
		notify_notification(applet, N_("Unable to store a value to GConf"), error->message, 5);
		g_error_free(error);
	}
	gconf_value_free(val);
	g_object_unref(G_OBJECT (client));

	_xsettings_update(applet);
}

static void
_gconf_xsettings_cb(GConfClient *client,
		    guint        cnxn_id,
		    GConfEntry  *entry,
		    gpointer     user_data)
{
	IMApplet *applet = user_data;

	_xsettings_update(applet);
}
#endif

static void
_lookup_ignorable_modifiers(GdkKeymap *keymap)
{
	egg_keymap_resolve_virtual_modifiers(keymap,
					     EGG_VIRTUAL_LOCK_MASK,
					     &caps_lock_mask);
	egg_keymap_resolve_virtual_modifiers(keymap,
					     EGG_VIRTUAL_NUM_LOCK_MASK,
					     &num_lock_mask);
	egg_keymap_resolve_virtual_modifiers(keymap,
					     EGG_VIRTUAL_SCROLL_LOCK_MASK,
					     &scroll_lock_mask);
}

static void
_grab_ungrab_with_ignorable_modifiers(IMApplet  *applet,
				      GdkWindow *rootwin,
				      gboolean   grab)
{
	guint mod_masks[] = {
		0, /* modifier only */
		num_lock_mask,
		caps_lock_mask,
		scroll_lock_mask,
		num_lock_mask | caps_lock_mask,
		num_lock_mask | scroll_lock_mask,
		caps_lock_mask | scroll_lock_mask,
		num_lock_mask | caps_lock_mask | scroll_lock_mask
	};
	guint i;

	for (i = 0; i < G_N_ELEMENTS (mod_masks); i++) {
		if (grab) {
			XGrabKey(GDK_WINDOW_XDISPLAY (rootwin),
				 applet->keycode, applet->modifiers | mod_masks[i],
				 GDK_WINDOW_XWINDOW (rootwin),
				 False,
				 GrabModeAsync,
				 GrabModeAsync);
		} else {
			XUngrabKey(GDK_WINDOW_XDISPLAY (rootwin),
				   applet->keycode, applet->modifiers | mod_masks[i],
				   GDK_WINDOW_XWINDOW (rootwin));
		}
	}
}

static void
_setup_acceleration_key(IMApplet *applet)
{
	GdkWindow *rootwin = gdk_get_default_root_window();
	gchar *key;

	applet->keycode = XKeysymToKeycode(GDK_WINDOW_XDISPLAY (rootwin),
					   applet->keyval);

	key = _get_acceleration_key(applet);
	gdk_error_trap_push();

	_grab_ungrab_with_ignorable_modifiers(applet, rootwin, TRUE);
	gdk_flush();

	if (gdk_error_trap_pop()) {
		gchar *escaped_key = g_markup_escape_text(key, -1);
		gchar *body = g_strdup_printf(_("The acceleration key %s may be invalid. disabled it temporarily."), escaped_key);

		notify_notification(applet, N_("Unable to set up the acceleration key"), body, 5);
		g_free(body);
		g_free(escaped_key);

		applet->keyval = GDK_VoidSymbol;
		applet->modifiers = 0;
		applet->watch_accel = FALSE;
		g_print("Acceleration key: disabled\n");
	} else {
		applet->watch_accel = TRUE;
		g_print("Acceleration key: %s\n", key);
	}

	g_free(key);
}

static void
_keymap_changed(GdkKeymap *map,
		gpointer   data)
{
	IMApplet *applet = data;
	GdkKeymap *keymap = gdk_keymap_get_default();
	GdkWindow *rootwin = gdk_get_default_root_window();

	_grab_ungrab_with_ignorable_modifiers(applet, rootwin, FALSE);
	_lookup_ignorable_modifiers(keymap);
	_setup_acceleration_key(applet);
}

static void
_gconf_trigger_key_cb(GConfClient *conf,
		      guint        cnxn_id,
		      GConfEntry  *entry,
		      gpointer     user_data)
{
	IMApplet *applet = user_data;
	GdkWindow *rootwin = gdk_get_default_root_window();
	GConfValue *val;
	const gchar *key;
	GdkKeymap *keymap = gdk_keymap_get_default();
	EggVirtualModifierType virtual_mods = 0;

	if (applet->watch_accel) {
		_grab_ungrab_with_ignorable_modifiers(applet, rootwin, FALSE);
		applet->watch_accel = FALSE;
	}
	val = gconf_entry_get_value(entry);
	key = gconf_value_get_string(val);
	if (key) {
		egg_accelerator_parse_virtual(key, &applet->keyval, &virtual_mods);
		egg_keymap_resolve_virtual_modifiers(keymap, virtual_mods, &applet->modifiers);
	}

	_setup_acceleration_key(applet);
	_preference_update_entry(applet);
}

static gchar *
_get_acceleration_key(IMApplet *applet)
{
	const gchar *key = gdk_keyval_name(applet->keyval);
	GString *s = g_string_new(NULL);

	if (key == NULL || strcmp(key, "VoidSymbol") == 0)
		g_string_append(s, "disabled");
	else
		g_string_append(s, key);

	if (applet->modifiers & GDK_SHIFT_MASK)
		g_string_prepend(s, "<Shift>");
	if (applet->modifiers & GDK_CONTROL_MASK)
		g_string_prepend(s, "<Control>");
	if (applet->modifiers & GDK_LOCK_MASK)
		g_string_prepend(s, "<Lock>");
	if (applet->modifiers & GDK_MOD1_MASK)
		g_string_prepend(s, "<Alt>");
	if (applet->modifiers & GDK_SUPER_MASK)
		g_string_prepend(s, "<Super>");
	if (applet->modifiers & GDK_HYPER_MASK)
		g_string_prepend(s, "<Hyper>");
	if (applet->modifiers & GDK_META_MASK)
		g_string_prepend(s, "<Meta>");

	return g_string_free(s, FALSE);
}

static void
_preference_update_entry(IMApplet *applet)
{
	gchar *key = _get_acceleration_key(applet);

	gtk_editable_set_editable(GTK_EDITABLE (applet->entry_grabkey), TRUE);
	gtk_entry_set_text(GTK_ENTRY (applet->entry_grabkey), key);
	gtk_editable_set_editable(GTK_EDITABLE (applet->entry_grabkey), FALSE);

	g_free(key);
}

static gboolean
_preference_grabbed(GtkWidget   *widget,
		    GdkEventKey *event,
		    gpointer     data)
{
	IMApplet *applet = data;

	if (!event->is_modifier &&
	    event->keyval != GDK_Hyper_L &&
	    event->keyval != GDK_Hyper_R &&
	    event->keyval != GDK_Super_L &&
	    event->keyval != GDK_Super_R) {
		GdkWindow *rootwin = gdk_get_default_root_window();
		GConfClient *client = gconf_client_get_default();
		GConfValue *val;
		GError *error = NULL;
		gchar *key;

		gtk_grab_remove(applet->entry_grabkey);
		gdk_keyboard_ungrab(GDK_CURRENT_TIME);
		g_signal_handlers_disconnect_by_func(applet->entry_grabkey,
						     G_CALLBACK (_preference_grabbed),
						     applet);
		if (applet->watch_accel) {
			_grab_ungrab_with_ignorable_modifiers(applet, rootwin, FALSE);
			applet->watch_accel = FALSE;
		}

		if (event->keyval == GDK_BackSpace) {
			applet->keyval = GDK_VoidSymbol;
			applet->modifiers = 0;
			applet->watch_accel = FALSE;
		} else {
			applet->keyval = event->keyval;
			applet->modifiers = event->state;
		}
		key = _get_acceleration_key(applet);
		val = gconf_value_new(GCONF_VALUE_STRING);
		gconf_value_set_string(val, key);
		gconf_client_set(client, "/apps/imsettings-applet/trigger_key",
				 val, &error);
		if (error) {
			notify_notification(applet, N_("Unable to store the acceleration key into GConf"), error->message, 5);
			g_error_free(error);
		}
		g_free(key);
		gconf_value_free(val);
		g_object_unref(G_OBJECT (client));

		_preference_update_entry(applet);
	}

	return TRUE;
}

static void
_preference_grabkey(GtkButton *button,
		    gpointer   data)
{
	IMApplet *applet = data;

	gtk_editable_set_editable(GTK_EDITABLE (applet->entry_grabkey), TRUE);
	gtk_entry_set_text(GTK_ENTRY (applet->entry_grabkey),
			   _("Please press the acceleration key..."));
	gtk_editable_set_editable(GTK_EDITABLE (applet->entry_grabkey), FALSE);
	while (g_main_context_pending(NULL))
		g_main_context_iteration(NULL, TRUE);

	if (gdk_keyboard_grab(applet->entry_grabkey->window, FALSE,
				   GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS) {
		_preference_update_entry(applet);
		return;
	}
	g_signal_connect(applet->entry_grabkey, "key_press_event",
			 G_CALLBACK (_preference_grabbed), applet);
	gtk_grab_add(applet->entry_grabkey);
}

static void
_preference_closed(GtkButton *button,
		   gpointer   data)
{
	IMApplet *applet = data;

	gtk_widget_hide(applet->dialog);
}

static void
_preference_activated(GtkMenuItem *item,
		      gpointer     data)
{
	IMApplet *applet = data;

	if (applet->dialog == NULL) {
		gchar *iconfile;
#ifdef ENABLE_XSETTINGS
		GtkWidget *align_xsettings;
#endif
		GtkWidget *align_showicon;
		GtkWidget *button_trigger_grab;
		GtkWidget *vbox_item_trigger, *vbox_item_trigger_value;
		GtkWidget *hbox_item_trigger_value_entry, *hbox_item_trigger_value_notice;
		GtkWidget *align_trigger, *align_trigger_value, *align_trigger_value_notice;
		GtkWidget *label_trigger, *label_trigger_notice;
		GtkWidget *image_trigger_notice;

		applet->dialog = gtk_dialog_new();
		gtk_window_set_title(GTK_WINDOW (applet->dialog), _("IMSettings Applet Preferences"));
		gtk_window_set_resizable(GTK_WINDOW (applet->dialog), FALSE);
		iconfile = g_build_filename(ICONDIR, "imsettings-applet.png", NULL);
		gtk_container_set_border_width(GTK_CONTAINER (applet->dialog), 4);
		gtk_container_set_border_width(GTK_CONTAINER (GTK_DIALOG (applet->dialog)->vbox), 0);

		applet->close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
		gtk_dialog_add_action_widget(GTK_DIALOG (applet->dialog), applet->close_button, GTK_RESPONSE_OK);
		gtk_dialog_set_has_separator(GTK_DIALOG (applet->dialog), FALSE);

		/* show icon */
		align_showicon = gtk_alignment_new(0, 0, 0, 0);
		applet->checkbox_showicon = gtk_check_button_new_with_mnemonic(_("_Show the status icon"));
		gtk_container_add(GTK_CONTAINER (align_showicon), applet->checkbox_showicon);
		gtk_alignment_set_padding(GTK_ALIGNMENT (align_showicon), 9, 6, 6, 6);
		g_signal_connect(applet->checkbox_showicon, "toggled",
				 G_CALLBACK (_preference_showicon_toggled), applet);

#ifdef ENABLE_XSETTINGS
		/* xsettings manager */
		align_xsettings = gtk_alignment_new(0, 0, 0, 0);
		applet->checkbox_xsettings = gtk_check_button_new_with_mnemonic(_("_Run XSETTINGS manager as needed"));
		gtk_container_add(GTK_CONTAINER (align_xsettings), applet->checkbox_xsettings);
		gtk_alignment_set_padding(GTK_ALIGNMENT (align_xsettings), 9, 6, 6, 6);
		g_signal_connect(applet->checkbox_xsettings, "toggled",
				 G_CALLBACK (_preference_xsettings_toggled), applet);
#endif /* ENABLE_XSETTINGS */

		/* trigger key */
		vbox_item_trigger = gtk_vbox_new(FALSE, 0);

		/* trigger key: label */
		align_trigger = gtk_alignment_new(0, 0, 0, 0);
		label_trigger = gtk_label_new(_("<b>Trigger key:</b>"));
		gtk_label_set_use_markup(GTK_LABEL (label_trigger), TRUE);
		gtk_container_add(GTK_CONTAINER (align_trigger), label_trigger);
		gtk_alignment_set_padding(GTK_ALIGNMENT (align_trigger), 9, 6, 6, 6);
		/* trigger key: value */
		align_trigger_value = gtk_alignment_new(0.1, 0, 1.0, 1.0);
		vbox_item_trigger_value = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER (align_trigger_value), vbox_item_trigger_value);
		gtk_alignment_set_padding(GTK_ALIGNMENT (align_trigger_value), 0, 6, 18, 6);
		/* trigger key: value - entry box */
		hbox_item_trigger_value_entry = gtk_hbox_new(FALSE, 0);
		applet->entry_grabkey = gtk_entry_new();
		button_trigger_grab = gtk_button_new_with_mnemonic(_("_Grab key"));

		GTK_WIDGET_UNSET_FLAGS(applet->entry_grabkey, GTK_CAN_FOCUS);
		g_signal_connect(button_trigger_grab, "clicked",
				 G_CALLBACK (_preference_grabkey), applet);
		gtk_box_pack_start(GTK_BOX (hbox_item_trigger_value_entry), applet->entry_grabkey, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX (hbox_item_trigger_value_entry), button_trigger_grab, FALSE, TRUE, 0);

		/* trigger key: value - notice */
		hbox_item_trigger_value_notice = gtk_hbox_new(FALSE, 0);
		image_trigger_notice = gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO,
								GTK_ICON_SIZE_DIALOG);
		align_trigger_value_notice = gtk_alignment_new(0.05, 0.5, 0, 0);
		gtk_alignment_set_padding(GTK_ALIGNMENT (align_trigger_value_notice), 6, 6, 0, 0);
		label_trigger_notice = gtk_label_new(_("Please click Grab key button and press the key combinations you want to assign for the acceleration key. Press Backspace key to disable this feature."));
		gtk_label_set_line_wrap(GTK_LABEL (label_trigger_notice), TRUE);

		gtk_container_add(GTK_CONTAINER (align_trigger_value_notice), label_trigger_notice);
		gtk_box_pack_start(GTK_BOX (hbox_item_trigger_value_notice), image_trigger_notice, FALSE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX (hbox_item_trigger_value_notice), align_trigger_value_notice, TRUE, TRUE, 0);

		/* trigger key: value / packing */
		gtk_box_pack_start(GTK_BOX (vbox_item_trigger_value), hbox_item_trigger_value_entry, FALSE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX (vbox_item_trigger_value), hbox_item_trigger_value_notice, FALSE, TRUE, 0);
		/* trigger key / packing */
		gtk_box_pack_start(GTK_BOX (vbox_item_trigger), align_trigger, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX (vbox_item_trigger), align_trigger_value, TRUE, TRUE, 0);

		/* items / packing */
		gtk_box_pack_start(GTK_BOX (GTK_DIALOG (applet->dialog)->vbox), align_showicon, TRUE, TRUE, 0);
#ifdef ENABLE_XSETTINGS
		gtk_box_pack_start(GTK_BOX (GTK_DIALOG (applet->dialog)->vbox), align_xsettings, TRUE, TRUE, 0);
#endif
		gtk_box_pack_start(GTK_BOX (GTK_DIALOG (applet->dialog)->vbox), vbox_item_trigger, TRUE, TRUE, 0);

		/* */
		g_signal_connect(applet->close_button, "clicked",
				 G_CALLBACK (_preference_closed), applet);
		g_object_add_weak_pointer(G_OBJECT (applet->dialog), (gpointer *)&applet->dialog);

		gtk_widget_show_all(GTK_DIALOG (applet->dialog)->vbox);
		_preference_update_entry(applet);
	}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (applet->checkbox_showicon),
				     gtk_status_icon_get_visible(applet->status_icon));
#ifdef ENABLE_XSETTINGS
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (applet->checkbox_xsettings),
				     applet->is_xsettings_manager_enabled);
	gtk_widget_set_sensitive(applet->checkbox_xsettings,
				 !applet->is_another_xsettings_manager_running);
#endif
	gtk_editable_set_editable(GTK_EDITABLE (applet->entry_grabkey), FALSE);
	gtk_widget_show(applet->dialog);
	gtk_widget_grab_focus(applet->close_button);
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
	g_signal_connect(item, "activate",
			 G_CALLBACK (_preference_activated),
			 applet);
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
			   _("Disconnect"));
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
			gchar *p = g_build_filename(ICONDIR, "imsettings-unknown.png", NULL);

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
	if (applet->current_im == NULL ||
	    applet->current_im[0] == 0 ||
	    strcmp(applet->current_im, "none") == 0) {
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
	if (applet->is_enabled) {
		g_free(applet->process_im);
		applet->process_im = g_strdup(applet->current_im);

		_stop_process(applet, TRUE);
	} else {
		gchar *current_im = imsettings_request_get_current_user_im(applet->req_info, NULL);

		if (current_im == NULL || current_im[0] == 0 ||
		    strcmp(current_im, "none") == 0) {
			notify_notification(applet, N_("Information"),
					    N_("The default Input Method isn't yet configured. To get this working, you need to set up that on im-chooser or select one from the menu which appears by the right click first."),
					    3);
			return;
		}
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
		    event_mods = xevent->xkey.state & ~(num_lock_mask | caps_lock_mask | scroll_lock_mask);
		    d(g_print("Pressed shortcut key\n"));
		    if (xevent->xkey.keycode == applet->keycode &&
			event_mods == applet->modifiers) {
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
	GConfClient *client;
	GConfValue *val;

	client = gconf_client_get_default();
	val = gconf_value_new(GCONF_VALUE_BOOL);
	gconf_value_set_bool(val, FALSE);
	gconf_client_set(client, "/apps/imsettings-applet/notify_tips", val, NULL);
	gconf_value_free(val);

	g_object_unref(G_OBJECT (client));
	d(g_print("disabled notification\n"));
}

static gboolean
_delay_notify(gpointer data)
{
	IMApplet *applet = data;
	GConfClient *client = gconf_client_get_default();
	GConfValue *val;
	gchar *notice_key, *body, *key, *escaped_key;

	val = gconf_client_get(client, "/apps/imsettings-applet/notify_tips", NULL);
	if (val) {
		if (!gconf_value_get_bool(val)) {
			d(g_print("No notification\n"));
			return FALSE;
		}
	}
	key = _get_acceleration_key(applet);
	escaped_key = g_markup_escape_text(key, -1);
	if (strcmp(key, "disabled") == 0) {
		notice_key = g_strdup("");
	} else {
		notice_key = g_strdup_printf(_("Press %s or "),
					     escaped_key);
	}
	body = g_strdup_printf(_("%sLeft-click on the icon to connect to/disconnect from Input Method.\nRight-click to show up the Input Method menu."),
			       notice_key);
	notify_notification_update(applet->notify, _("Tips"), body, NULL);
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
	g_free(body);
	g_free(key);
	g_free(escaped_key);
	gconf_value_free(val);
	g_object_unref(G_OBJECT (client));

	return FALSE;
}

#ifdef ENABLE_XIM
static void
_quit_cb(XimProxy *proxy,
	 gpointer  data)
{
	g_print("*** XIM server is exiting...\n");

	gtk_main_quit();
}

static XimProxy *
_create_proxy(IMApplet   *applet,
	      GdkDisplay *dpy,
	      gboolean    replace)
{
	XimProxy *retval;
	GError *error = NULL;

	retval = xim_proxy_new(dpy, "imsettings", applet->xim_server);
	if (retval == NULL)
		return NULL;
	g_signal_connect(retval, "destroy",
			 G_CALLBACK (_quit_cb),
			 NULL);
	if (!xim_proxy_take_ownership(retval, replace, &error)) {
		gchar *body;

		body = g_strdup_printf("XIM feature will be turned off.\nDetails: %s",
				       error->message);
		g_printerr("%s\n", body);
		notify_notification(applet, N_("Unable to take an ownership for XIM server"), body, 5);
		g_free(body);

		return NULL;
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
		IMApplet *applet;

		applet = g_object_get_qdata(G_OBJECT (server), quark_applet);
		dbus_message_get_args(message, NULL,
				      DBUS_TYPE_STRING, &module,
				      DBUS_TYPE_INVALID);
		if (strcmp(module, applet->xim_server) == 0) {
			/* No changes */
			return DBUS_HANDLER_RESULT_HANDLED;
		}
		g_xim_message_debug(G_XIM_CORE (server)->message, "dbus/event",
				    "Changing XIM server: `%s'->`%s'\n",
				    (applet->xim_server && applet->xim_server[0] == 0 ? "none" : applet->xim_server),
				    (module && module[0] == 0 ? "none" : module));
		g_free(applet->xim_server);
		applet->xim_server = g_strdup(module);
		g_object_set(applet->server, "connect_to", module, NULL);

		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal(message, IMSETTINGS_XIM_INTERFACE_DBUS, "Changed")) {
		gchar *module;
		IMApplet *applet;

		applet = g_object_get_qdata(G_OBJECT (server), quark_applet);
		dbus_message_get_args(message, NULL,
				      DBUS_TYPE_STRING, &module,
				      DBUS_TYPE_INVALID);

		if (strcmp(applet->current_im, module) != 0) {
			if (strcmp(module, "none") == 0) {
				applet->is_enabled = FALSE;
				applet->need_update_xinputrc = TRUE;
			} else {
				applet->is_enabled = TRUE;
				applet->need_update_xinputrc = FALSE;
			}
			g_free(applet->current_im);
			applet->current_im = g_strdup(module);

			_update_icon(applet);
		}

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
#endif /* ENABLE_XIM */

static IMApplet *
_create_applet(void)
{
	IMApplet *applet;
	gchar *name;
	const gchar *locale, *key;
	GConfClient *client;
	GConfValue *val;
	GError *error = NULL;
#ifdef ENABLE_XIM
	DBusError derror;
	const gchar *xim;
	IMSettingsInfo *info;
#endif
	GdkWindow *rootwin = gdk_get_default_root_window();
	GdkKeymap *keymap = gdk_keymap_get_default();

	applet = g_new0(IMApplet, 1);
	applet->is_enabled = FALSE;
	applet->status_icon = gtk_status_icon_new_from_stock(GTK_STOCK_DISCONNECT);
	applet->conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	if (applet->conn == NULL) {
		g_printerr("Unable to establish the dbus session bus.\n");

		return NULL;
	}
	applet->req = imsettings_request_new(applet->conn, IMSETTINGS_INTERFACE_DBUS);
	applet->req_info = imsettings_request_new(applet->conn, IMSETTINGS_INFO_INTERFACE_DBUS);

	locale = setlocale(LC_CTYPE, "");
	imsettings_request_set_locale(applet->req, locale);
	imsettings_request_set_locale(applet->req_info, locale);

	g_signal_connect(applet->status_icon, "popup_menu",
			 G_CALLBACK (_popup_menu),
			 applet);
	g_signal_connect(applet->status_icon, "activate",
			 G_CALLBACK (_activate),
			 applet);

	name = imsettings_request_what_im_is_running(applet->req, NULL);
	if (name == NULL || name[0] == 0) {
		g_free(name);
		name = g_strdup("none");
	}
	if (!applet->current_im)
		applet->current_im = g_strdup(name);
	g_free(name);
	_update_icon(applet);

	notify_init("imsettings-applet");
	applet->notify = notify_notification_new_with_status_icon("foo", "bar", NULL, applet->status_icon);

	client = gconf_client_get_default();

	/* setup shortcut key */
	_lookup_ignorable_modifiers(keymap);
	g_signal_connect(keymap, "keys_changed",
			 G_CALLBACK (_keymap_changed), applet);

	/* setup gconf */
	gconf_client_add_dir(client, "/apps/imsettings-applet",
			     GCONF_CLIENT_PRELOAD_NONE,
			     NULL);
	gconf_client_notify_add(client, "/apps/imsettings-applet/trigger_key",
				_gconf_trigger_key_cb, applet, NULL, &error);
#ifdef ENABLE_XSETTINGS
	gconf_client_notify_add(client, "/apps/imsettings-applet/xsettings_manager",
				_gconf_xsettings_cb, applet, NULL, &error);
#endif
	gconf_client_notify_add(client, "/apps/imsettings-applet/show_icon",
				_gconf_show_icon_cb, applet, NULL, &error);

	val = gconf_client_get(client, "/apps/imsettings-applet/trigger_key", NULL);
	key = gconf_value_get_string(val);
	if (key)
		gtk_accelerator_parse(key, &applet->keyval, &applet->modifiers);
	gconf_value_free(val);

	_setup_acceleration_key(applet);
	gdk_window_add_filter(rootwin, filter_func, applet);

	val = gconf_client_get(client, "/apps/imsettings-applet/show_icon", NULL);
	if (val == NULL || gconf_value_get_bool(val)) {
		g_timeout_add_seconds(1, _delay_notify, applet);
	} else {
		gtk_status_icon_set_visible(applet->status_icon, FALSE);
	}
	gconf_value_free(val);

	g_object_unref(client);

#ifdef ENABLE_XIM
	dbus_error_init(&derror);

	quark_applet = g_quark_from_static_string("imsettings-applet-xim");

	if (strcmp(applet->current_im, "none") == 0) {
		applet->need_update_xinputrc = TRUE;
	} else {
		applet->need_update_xinputrc = FALSE;
		applet->is_enabled = TRUE;
	}
	info = imsettings_request_get_info_object(applet->req_info,
						  applet->need_update_xinputrc ? "none" : applet->current_im,
						  NULL);
	xim = imsettings_info_get_xim(info);

	applet->xim_server = g_strdup(xim && xim[0] != 0 ? xim : "none");
	applet->server = _create_proxy(applet, gdk_display_get_default(), TRUE);
	if (applet->server)
		g_object_set_qdata(G_OBJECT (applet->server), quark_applet, applet);

	dbus_bus_add_match(applet->conn,
			   "type='signal',"
			   "interface='" IMSETTINGS_XIM_INTERFACE_DBUS "'",
			   &derror);
	dbus_connection_add_filter(applet->conn, imsettings_xim_message_filter, applet->server, NULL);
#endif

#ifdef ENABLE_XSETTINGS
	_xsettings_update(applet);
#endif /* ENABLE_XSETTINGS */

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
	if (applet->dialog)
		g_object_unref(applet->dialog);
#ifdef ENABLE_XIM
	g_object_unref(applet->server);
	g_free(applet->xim_server);
#endif
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
#ifdef ENABLE_XIM
	g_xim_init();
#endif

	applet = _create_applet();
	if (applet == NULL)
		goto end;

	gtk_main();

  end:
	_destroy_applet(applet);
#ifdef ENABLE_XIM
	g_xim_finalize();
#endif

	return 0;
}
