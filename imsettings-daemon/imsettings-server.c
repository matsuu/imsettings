/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-server.c
 * Copyright (C) 2008-2010 Red Hat, Inc. All rights reserved.
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

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include "imsettings.h"
#include "imsettings-info.h"
#include "imsettings-module.h"
#include "imsettings-proc.h"
#include "imsettings-utils.h"
#include "imsettings-server.h"

#define IMSETTINGS_SERVER_GET_PRIVATE(_o_)	(G_TYPE_INSTANCE_GET_PRIVATE ((_o_), IMSETTINGS_TYPE_SERVER, IMSettingsServerPrivate))

struct _IMSettingsServerPrivate {
	GDBusConnection    *connection;
	NotifyNotification *notify;
	gchar              *homedir;
	gchar              *xinputrcdir;
	gchar              *xinputdir;
	gchar              *moduledir;
	GHashTable         *modules;
	IMSettingsProc     *current_im;
	GLogFunc            old_log_handler;
	guint               id;
	guint               signal_id;
	guint               owner;
	gboolean            active:1;
	gboolean            logging:1;
};
enum {
	PROP_0,
	PROP_CONNECTION,
	PROP_LOGGING,
	PROP_HOMEDIR,
	PROP_XINPUTRCDIR,
	PROP_XINPUTDIR,
	PROP_MODULEDIR,
	LAST_PROP
};
enum {
	SIG_DISCONNECTED,
	LAST_SIGNAL
};

static gchar     *imsettings_server_cb_get_system_im(IMSettingsServer     *server,
						     const gchar          *lang,
						     GError              **error);
static void      imsettings_server_bus_method_call (GDBusConnection       *connection,
                                                    const gchar           *sender,
                                                    const gchar           *object_path,
                                                    const gchar           *interface_name,
                                                    const gchar           *method_name,
                                                    GVariant              *parameters,
                                                    GDBusMethodInvocation *invocation,
                                                    gpointer               user_data);
static GVariant *imsettings_server_bus_get_property(GDBusConnection       *connection,
                                                    const gchar           *sender,
                                                    const gchar           *object_path,
                                                    const gchar           *interface_name,
                                                    const gchar           *property_name,
                                                    GError                **error,
                                                    gpointer               user_data);
static gboolean  imsettings_server_bus_set_property(GDBusConnection       *connection,
                                                    const gchar           *sender,
                                                    const gchar           *object_paht,
                                                    const gchar           *interface_name,
                                                    const gchar           *property_name,
                                                    GVariant              *value,
                                                    GError                **error,
                                                    gpointer               user_data);

static const gchar *imsettings_server_get_homedir    (IMSettingsServer *server);
static const gchar *imsettings_server_get_xinputrcdir(IMSettingsServer *server);
static const gchar *imsettings_server_get_xinputdir  (IMSettingsServer *server);
static const gchar *imsettings_server_get_moduledir  (IMSettingsServer *server);

GDBusInterfaceVTable __iface_vtable = {
	imsettings_server_bus_method_call,
	imsettings_server_bus_get_property,
	imsettings_server_bus_set_property,
};
guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (IMSettingsServer, imsettings_server, G_TYPE_OBJECT);
G_LOCK_DEFINE_STATIC (logger);

/*< private >*/
static void
_notify_cb(IMSettingsProc *proc,
	   NotifyUrgency   urgency,
	   const gchar    *title,
	   const gchar    *message,
	   gint            timeout,
	   gpointer        user_data)
{
	IMSettingsServer *server = IMSETTINGS_SERVER (user_data);
	IMSettingsServerPrivate *priv = server->priv;
	GError *err = NULL;

	notify_notification_set_urgency(priv->notify, urgency);
	notify_notification_update(priv->notify, title, message, NULL);
	notify_notification_clear_actions(priv->notify);
	notify_notification_set_timeout(priv->notify, timeout * 1000);
	notify_notification_set_category(priv->notify, "x-imsettings-notice");
	notify_notification_show(priv->notify, &err);

	if (err) {
		g_warning(err->message);
		g_error_free(err);
	}
}

static void
imsettings_server_logger(IMSettingsServer *server,
			 const gchar      *buffer,
			 gsize             length)
{
	FILE *fp;
	gchar *logfile;
	const gchar *homedir = NULL;
	IMSettingsServerPrivate *priv = server->priv;

	if (!priv->logging)
		return;
	if (length < 0)
		length = strlen(buffer);

	homedir = imsettings_server_get_homedir(server);
	logfile = g_build_filename(homedir, ".imsettings.log", NULL);

	G_LOCK (logger);

	fp = fopen(logfile, "ab");
	if (fp == NULL) {
		g_critical("Unable to open a log file: %s", logfile);
		goto finalize;
	}
	fwrite(buffer, length, sizeof (gchar), fp);
	fclose(fp);

  finalize:
	G_UNLOCK (logger);

	g_free(logfile);
}

static void
imsettings_server_log_handler(const gchar    *log_domain,
			      GLogLevelFlags  log_level,
			      const gchar    *message,
			      gpointer        data)
{
	GString *log = g_string_new(NULL);
	IMSettingsServer *server = IMSETTINGS_SERVER (data);
	GTimeVal val;

	g_get_current_time(&val);
	g_string_append_printf(log, "[% 10ld.%06ld]: ",
			       val.tv_sec, (val.tv_usec * 1000) / 1000);

	if ((log_level & G_LOG_LEVEL_MASK) == G_LOG_LEVEL_MESSAGE) {
	} else {
		if (log_domain) {
			g_string_append(log, log_domain);
		} else {
			g_string_append_printf(log, "(%s)", g_get_prgname());
		}
		g_string_append_printf(log, "[%lu]: ", (gulong)getpid());

		switch (log_level & G_LOG_LEVEL_MASK) {
		    case G_LOG_LEVEL_ERROR:
			    g_string_append(log, "ERROR");
			    break;
		    case G_LOG_LEVEL_CRITICAL:
			    g_string_append(log, "CRITICAL");
			    break;
		    case G_LOG_LEVEL_WARNING:
			    g_string_append(log, "WARNING");
			    break;
		    case G_LOG_LEVEL_MESSAGE:
			    break;
		    case G_LOG_LEVEL_INFO:
			    g_string_append(log, "INFO");
			    break;
		    case G_LOG_LEVEL_DEBUG:
			    g_string_append(log, "DEBUG");
			    break;
		    default:
			    if (log_level) {
				    g_string_append_printf(log, "LOG-0x%x", log_level & G_LOG_LEVEL_MASK);
			    } else {
				    g_string_append(log, "LOG");
			    }
			    break;
		}
		if (log_level & G_LOG_FLAG_RECURSION)
			g_string_append(log, " (recursed)");
		if (log_level & (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING)) {
			g_string_prepend(log, "\n");
			g_string_append(log, " **");
		}
		g_string_append(log, ": ");
	}
	if (message) {
		g_string_append(log, message);
	} else {
		g_string_append(log, "(NULL) message");
	}
	g_string_append(log, "\n");

	g_print("%s", log->str);
	imsettings_server_logger(server, log->str, log->len);

	g_string_free(log, TRUE);
}

static void
imsettings_server_set_property(GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	IMSettingsServer *server = IMSETTINGS_SERVER (object);
	IMSettingsServerPrivate *priv = server->priv;
	const gchar *p;
	GFile *f;

	switch (prop_id) {
	    case PROP_CONNECTION:
		    priv->connection = g_value_dup_object(value);
		    break;
	    case PROP_LOGGING:
		    priv->logging = g_value_get_boolean(value);
		    break;
	    case PROP_HOMEDIR:
		    p = g_value_get_string(value);
		    if (!p) {
			    g_free(priv->homedir);
			    priv->homedir = NULL;
			    break;
		    }
		    if (!g_file_test(p, G_FILE_TEST_IS_DIR)) {
			    g_warning("Invalid parameter for the property %s: %s",
				      g_type_name(G_PARAM_SPEC_TYPE (pspec)),
				      p);
		    } else {
			    f = g_file_new_for_path(p);
			    g_free(priv->homedir);
			    priv->homedir = g_file_get_path(f);
			    g_object_unref(f);
		    }
		    break;
	    case PROP_XINPUTRCDIR:
		    p = g_value_get_string(value);
		    if (!p) {
			    g_free(priv->xinputrcdir);
			    priv->xinputrcdir = NULL;
			    break;
		    }
		    if (!g_file_test(p, G_FILE_TEST_IS_DIR)) {
			    g_warning("Invalid parameter for the property %s: %s",
				      g_type_name(G_PARAM_SPEC_TYPE (pspec)),
				      p);
		    } else {
			    f = g_file_new_for_path(p);
			    g_free(priv->xinputrcdir);
			    priv->xinputrcdir = g_file_get_path(f);
			    g_object_unref(f);
		    }
		    break;
	    case PROP_XINPUTDIR:
		    p = g_value_get_string(value);
		    if (!p) {
			    g_free(priv->xinputdir);
			    priv->xinputdir = NULL;
			    break;
		    }
		    if (!g_file_test(p, G_FILE_TEST_IS_DIR)) {
			    g_warning("Invalid parameter for the property %s: %s",
				      g_type_name(G_PARAM_SPEC_TYPE (pspec)),
				      p);
		    } else {
			    f = g_file_new_for_path(p);
			    g_free(priv->xinputdir);
			    priv->xinputdir = g_file_get_path(f);
			    g_object_unref(f);
		    }
		    break;
	    case PROP_MODULEDIR:
		    p = g_value_get_string(value);
		    if (!p) {
			    g_free(priv->moduledir);
			    priv->moduledir = NULL;
			    break;
		    }
		    priv->moduledir = g_strdup(p);
		    g_setenv("IMSETTINGS_MODULE_PATH", p, TRUE);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_server_get_property(GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
	IMSettingsServer *server = IMSETTINGS_SERVER (object);
	IMSettingsServerPrivate *priv = server->priv;

	switch (prop_id) {
	    case PROP_CONNECTION:
		    g_value_set_object(value, priv->connection);
		    break;
	    case PROP_LOGGING:
		    g_value_set_boolean(value, priv->logging);
		    break;
	    case PROP_HOMEDIR:
		    g_value_set_string(value, imsettings_server_get_homedir(server));
		    break;
	    case PROP_XINPUTRCDIR:
		    g_value_set_string(value, imsettings_server_get_xinputrcdir(server));
		    break;
	    case PROP_XINPUTDIR:
		    g_value_set_string(value, imsettings_server_get_xinputdir(server));
		    break;
	    case PROP_MODULEDIR:
		    g_value_set_string(value, imsettings_server_get_moduledir(server));
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_server_finalize(GObject *object)
{
	IMSettingsServer *server = IMSETTINGS_SERVER (object);
	IMSettingsServerPrivate *priv = server->priv;

	if (priv->signal_id != 0)
		g_dbus_connection_signal_unsubscribe(priv->connection,
						     priv->signal_id);
	if (priv->id != 0)
		g_dbus_connection_unregister_object(priv->connection,
						    priv->id);
	if (priv->owner != 0)
		g_bus_unown_name(priv->owner);
	g_object_unref(priv->notify);
	/* This is quite weird but modules has to be closed
	 * after any instances destroyed that possibly happens
	 * dbus connection.
	 * otherwise the deak lock occurs in pthread.
	 */
	if (priv->modules)
		g_hash_table_destroy(priv->modules);
	if (priv->current_im)
		g_object_unref(priv->current_im);
	g_object_unref(priv->connection);
	g_free(priv->homedir);
	g_free(priv->xinputrcdir);
	g_free(priv->xinputdir);
	g_free(priv->moduledir);

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
	      "imsettings-daemon is shut down.");

	if (priv->old_log_handler)
		g_log_set_default_handler(priv->old_log_handler, NULL);

	if (G_OBJECT_CLASS (imsettings_server_parent_class)->finalize)
		G_OBJECT_CLASS (imsettings_server_parent_class)->finalize(object);
}

static void
imsettings_server_class_init(IMSettingsServerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private(klass, sizeof (IMSettingsServerPrivate));

	object_class->set_property = imsettings_server_set_property;
	object_class->get_property = imsettings_server_get_property;
	object_class->finalize     = imsettings_server_finalize;

	/* properties */

	g_object_class_install_property(object_class, PROP_CONNECTION,
					g_param_spec_object("connection",
							    _("DBus connection"),
							    _("A GObject to be a DBus connection"),
							    G_TYPE_DBUS_CONNECTION,
							    G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
	g_object_class_install_property(object_class, PROP_LOGGING,
					g_param_spec_boolean("logging",
							     _("Logging"),
							     _("A boolean value whether the logging facility is enabled or not."),
							     TRUE,
							     G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_HOMEDIR,
					g_param_spec_string("homedir",
							    _("Home directory"),
							    _("Home directory"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_XINPUTRCDIR,
					g_param_spec_string("xinputrcdir",
							    _("xinputrc directory"),
							    _("xinputrc directory"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_XINPUTDIR,
					g_param_spec_string("xinputdir",
							    _("xinput directory"),
							    _("xinput directory"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_MODULEDIR,
					g_param_spec_string("moduledir",
							    _("module directory"),
							    _("IMSettings module directory"),
							    NULL,
							    G_PARAM_READWRITE));

	/* signals */

	signals[SIG_DISCONNECTED] = g_signal_new("disconnected",
						 G_OBJECT_CLASS_TYPE (klass),
						 G_SIGNAL_RUN_FIRST,
						 G_STRUCT_OFFSET (IMSettingsServerClass, disconnected),
						 NULL, NULL,
						 g_cclosure_marshal_VOID__VOID,
						 G_TYPE_NONE, 0);
}

static void
imsettings_server_init(IMSettingsServer *server)
{
	IMSettingsServerPrivate *priv;

	priv = server->priv = IMSETTINGS_SERVER_GET_PRIVATE (server);

	priv->active = FALSE;
	priv->logging = TRUE;
	priv->modules = g_hash_table_new_full(g_str_hash,
					      g_str_equal,
					      g_free,
					      g_object_unref);
#ifdef HAS_STATUS_ICON
	/* this is deprecated code */
	priv->notify = notify_notification_new("imsettings-daemon notification",
					       "messages from imsettings-daemon",
					       NULL /* no icon */,
					       NULL /* no GtkWidget supported */);
#else
	priv->notify = notify_notification_new("imsettings-daemon notification",
					       "messages from imsettings-daemon",
					       NULL /* no icon */);
#endif
}

static GVariant *
imsettings_server_cb_get_info_variants(IMSettingsServer  *server,
				       const gchar       *lang,
				       GError           **error)
{
	GVariant *value = NULL;
	GFile *file;
	GFileEnumerator *e;

	file = g_file_new_for_path(imsettings_server_get_xinputdir(server));
	e = g_file_enumerate_children(file, "standard::*",
				      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				      NULL, NULL);
	if (e) {
		GFileInfo *finfo;
		GFileType type;
		GPtrArray *a = g_ptr_array_new();
		GVariantBuilder *vb;
		GVariant *v, *sys_v = NULL;
		IMSettingsInfo *info, *sys_info = NULL;
		gchar *path, *conf, *p;
		const gchar *n, *module;
		gsize len, slen = strlen(XINPUT_SUFFIX), i;
		gchar *im = imsettings_server_cb_get_system_im(server, lang, error);

		g_clear_error(error);
		path = g_file_get_path(file);
		while (1) {
			finfo = g_file_enumerator_next_file(e, NULL, error);
			if (!finfo && !*error)
				break;
			if (*error) {
				g_warning("Unable to obtain the file information: %s",
					  (*error)->message);
				g_clear_error(error);
				goto next;
			}
			type = g_file_info_get_file_type(finfo);
			if (type != G_FILE_TYPE_REGULAR &&
			    type != G_FILE_TYPE_SYMBOLIC_LINK &&
			    type != G_FILE_TYPE_SHORTCUT)
				goto next;
			n = g_file_info_get_name(finfo);
			if (!n)
				goto next;
			len = strlen(n);
			if (len <= slen ||
			    strcmp(&n[len - slen], XINPUT_SUFFIX) != 0)
				goto next;

			conf = g_path_get_basename(n);
			p = g_build_filename(path, conf, NULL);
			v = imsettings_info_variant_new(p, lang);
			g_free(p);
			g_free(conf);

			info = imsettings_info_new(v);
			if (imsettings_info_is_visible(info)) {
				if (im && g_ascii_strcasecmp(im, imsettings_info_get_short_desc(info)) == 0) {
					if (sys_v) {
						g_warning("the system default should be only one: [%s vs %s]",
							  imsettings_info_get_short_desc(sys_info),
							  imsettings_info_get_short_desc(info));
						g_ptr_array_add(a, sys_v);
						g_object_unref(sys_info);
					}
					sys_info = g_object_ref(info);
					sys_v = v;
				} else {
					g_ptr_array_add(a, v);
				}
			} else {
				g_variant_unref(v);
			}
			g_object_unref(info);
		  next:
			if (finfo)
				g_object_unref(finfo);
		}
		g_free(im);
		g_free(path);
		g_file_enumerator_close(e, NULL, NULL);
		g_object_unref(e);

		vb = g_variant_builder_new(G_VARIANT_TYPE ("a{sv}"));
		if (sys_info) {
			module = imsettings_info_get_short_desc(sys_info);
			n = imsettings_info_get_filename(sys_info);
			g_variant_builder_add(vb, "{sv}", module, sys_v);
			g_variant_builder_add(vb, "{sv}", n, sys_v);
			g_object_unref(sys_info);
		}
		for (i = 0; i < a->len; i++) {
			v = g_ptr_array_index(a, i);
			info = imsettings_info_new(v);
			module = imsettings_info_get_short_desc(info);
			n = imsettings_info_get_filename(info);
			g_variant_builder_add(vb, "{sv}", module, v);
			g_variant_builder_add(vb, "{sv}", n, v);
			g_object_unref(info);
		}
		p = g_build_filename(imsettings_server_get_homedir(server), IMSETTINGS_USER_XINPUT_CONF, NULL);
		if (g_file_test(p, G_FILE_TEST_EXISTS)) {
			if (!g_file_test(p, G_FILE_TEST_IS_SYMLINK)) {
				v = imsettings_info_variant_new(p, lang);
				info = imsettings_info_new(v);
				module = imsettings_info_get_short_desc(info);
				n = imsettings_info_get_filename(info);
				if (g_strcmp0(module, IMSETTINGS_USER_SPECIFIC_SHORT_DESC) == 0) {
					g_variant_builder_add(vb, "{sv}", module, v);
				}
				g_object_unref(info);
			}
		}
		g_free(p);
		value = g_variant_builder_end(vb);
		g_variant_builder_unref(vb);
		g_ptr_array_free(a, TRUE);
	}

	g_object_unref(file);

	return value;
}

static GVariant *
imsettings_server_cb_get_info_variant(IMSettingsServer  *server,
				      const gchar       *lang,
				      const gchar       *module,
				      GError           **error)
{
	GVariant *v, *value = NULL;

	if (g_strcmp0(module, IMSETTINGS_NONE_CONF) == 0 ||
	    g_strcmp0(module, IMSETTINGS_NONE_CONF XINPUT_SUFFIX) == 0) {
		gchar *p = g_build_filename(imsettings_server_get_xinputdir(server),
					    IMSETTINGS_NONE_CONF XINPUT_SUFFIX, NULL);

		value = imsettings_info_variant_new(p, lang);
		g_free(p);

		return value;
	}
	v = imsettings_server_cb_get_info_variants(server, lang, error);
	if (!v) {
		g_set_error(error, IMSETTINGS_GERROR,
			    IMSETTINGS_GERROR_IM_NOT_FOUND,
			    "No IMs available");
	} else {
		GVariantIter *iter;
		GVariant *vv;
		const gchar *key;
		gchar *conf;

		g_variant_get(v, "a{sv}", &iter);
		while (g_variant_iter_next(iter, "{&sv}", &key, &vv)) {
			conf = g_path_get_basename(key);
			if (g_ascii_strcasecmp(key, module) == 0 ||
			    g_ascii_strcasecmp(conf, module) == 0) {
				g_free(conf);
				value = vv;
				break;
			}
			g_free(conf);
		}
		g_variant_unref(v);
	}

	return value;
}

static gboolean
imsettings_server_cb_switch_im(IMSettingsServer  *server,
			       const gchar       *lang,
			       const gchar       *module,
			       gboolean           update_xinputrc,
			       IMSettingsInfo   **info,
			       GError           **error)
{
	IMSettingsServerPrivate *priv = server->priv;
	GVariant *v;
	gchar *conffile, *backupfile, *p, *n;
	const gchar *homedir, *xinputfile;
	struct stat st;
	gint save_errno;

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
	      "Attempting to switch IM to %s [lang=%s, update=%s]",
	      module, lang, update_xinputrc ? "true" : "false");

	if (g_ascii_strcasecmp(module, IMSETTINGS_NONE_CONF) == 0 ||
	    g_ascii_strcasecmp(module, IMSETTINGS_NONE_CONF XINPUT_SUFFIX) == 0) {
		gchar *f = g_build_filename(imsettings_server_get_xinputdir(server),
					    IMSETTINGS_NONE_CONF XINPUT_SUFFIX,
					    NULL);
		v = imsettings_info_variant_new(f, lang);
		g_free(f);
	} else {
		v = imsettings_server_cb_get_info_variant(server, lang, module, error);
	}
	if (!v) {
		g_clear_error(error);
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_IM_NOT_FOUND,
			    _("No such input method on your system: %s"), module);
		g_warning((*error)->message);

		return FALSE;
	}
	*info = imsettings_info_new(v);
	if (!*info) {
		g_variant_unref(v);
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_OOM,
			    _("Out of memory"));
		g_warning((*error)->message);

		return FALSE;
	}
	g_variant_unref(v);

	if (priv->current_im) {
		imsettings_proc_kill(priv->current_im, error);
		if (*error)
			return FALSE;

		g_object_unref(priv->current_im);
	}

	priv->current_im = imsettings_proc_new(*info);
	g_object_add_weak_pointer(G_OBJECT (priv->current_im), (gpointer *)&priv->current_im);
	g_signal_connect(priv->current_im, "notify_notification",
			 G_CALLBACK (_notify_cb),
			 server);

	homedir = imsettings_server_get_homedir(server);
	conffile = g_build_filename(homedir,
				    IMSETTINGS_USER_XINPUT_CONF,
				    NULL);
	backupfile = g_build_filename(homedir,
				      IMSETTINGS_USER_XINPUT_CONF ".bak",
				      NULL);
	xinputfile = imsettings_info_get_filename(*info);
	p = g_path_get_dirname(xinputfile);
	n = g_path_get_basename(xinputfile);
	if (g_strcmp0(p, homedir) == 0 &&
	    g_strcmp0(n, IMSETTINGS_USER_XINPUT_CONF) == 0) {
		/* do not create/update a symlink */
	} else if (g_strcmp0(p, homedir) == 0 &&
	    g_strcmp0(n, IMSETTINGS_USER_XINPUT_CONF ".bak") == 0) {
		/* try to revert the backup file for the user specific conf file */
		if (g_rename(backupfile, conffile) == -1) {
			save_errno = errno;

			g_set_error(error, G_FILE_ERROR,
				    g_file_error_from_errno(save_errno),
				    _("Failed to revert the backup file: %s"),
				    g_strerror(save_errno));
			goto end;
		}
	} else {
		if (lstat(conffile, &st) == 0) {
			if (!S_ISLNK (st.st_mode)) {
				/* .xinputrc was maybe created by the hand. */
				if (g_rename(conffile, backupfile) == -1) {
					save_errno = errno;

					g_set_error(error, G_FILE_ERROR,
						    g_file_error_from_errno(save_errno),
						    _("Failed to create a backup file: %s"),
						    g_strerror(save_errno));
					goto end;
				}
			} else {
				if (g_unlink(conffile) == -1) {
					save_errno = errno;

					g_set_error(error, G_FILE_ERROR,
						    g_file_error_from_errno(save_errno),
						    _("Failed to remove a .xinputrc file: %s"),
						    g_strerror(save_errno));
					goto end;
				}
			}
		}
		if (symlink(xinputfile, conffile) == -1) {
			save_errno = errno;

			g_set_error(error, G_FILE_ERROR,
				    g_file_error_from_errno(save_errno),
				    _("Failed to create a symlink: %s"),
				    g_strerror(save_errno));
		}
	}
  end:
	g_free(n);
	g_free(p);
	g_free(backupfile);
	g_free(conffile);

	return imsettings_proc_spawn(priv->current_im, error);
}

static gchar *
imsettings_server_cb_get_system_im(IMSettingsServer  *server,
				   const gchar       *lang,
				   GError           **error)
{
	gchar *f, *retval = NULL;
	GVariant *v = NULL;
	IMSettingsInfo *info;

	f = g_build_filename(imsettings_server_get_xinputrcdir(server),
			     IMSETTINGS_GLOBAL_XINPUT_CONF, NULL);
	if (g_file_test(f, G_FILE_TEST_EXISTS))
		v = imsettings_info_variant_new(f, lang);
	g_free(f);
	if (!v) {
		g_set_error(error, IMSETTINGS_GERROR,
			    IMSETTINGS_GERROR_CONFIGURATION_ERROR,
			    _("No system-wide xinputrc available"));
		return NULL;
	}
	info = imsettings_info_new(v);
	if (!info) {
		g_set_error(error, IMSETTINGS_GERROR,
			    IMSETTINGS_GERROR_OOM,
			    _("Out of memory"));
	} else {
		retval = g_strdup(imsettings_info_get_short_desc(info));
		g_object_unref(info);
	}
	g_variant_unref(v);

	return retval;
}

static gchar *
imsettings_server_cb_get_user_im(IMSettingsServer  *server,
				 const gchar       *lang,
				 GError           **error)
{
	gchar *f, *retval = NULL;
	GVariant *v = NULL;
	IMSettingsInfo *info;

	f = g_build_filename(imsettings_server_get_homedir(server),
			     IMSETTINGS_USER_XINPUT_CONF, NULL);
	if (g_file_test(f, G_FILE_TEST_EXISTS))
		v = imsettings_info_variant_new(f, lang);
	g_free(f);
	if (!v) {
		f = g_build_filename(imsettings_server_get_xinputrcdir(server),
				     IMSETTINGS_GLOBAL_XINPUT_CONF, NULL);
		if (g_file_test(f, G_FILE_TEST_EXISTS))
			v = imsettings_info_variant_new(f, lang);
		g_free(f);
		if (!v) {
			g_set_error(error, IMSETTINGS_GERROR,
				    IMSETTINGS_GERROR_CONFIGURATION_ERROR,
				    _("No .xinputrc nor system-wide xinputrc available"));
			return NULL;
		}
	}
	info = imsettings_info_new(v);
	if (!info) {
		g_set_error(error, IMSETTINGS_GERROR,
			    IMSETTINGS_GERROR_OOM,
			    _("Out of memory"));
	} else {
		retval = g_strdup(imsettings_info_get_short_desc(info));
		g_object_unref(info);
	}
	g_variant_unref(v);

	return retval;
}

static gboolean
imsettings_server_cb_load_module(IMSettingsServer *server,
				 const gchar      *modname)
{
	IMSettingsServerPrivate *priv = server->priv;
	IMSettingsModule *module;

	module = imsettings_module_new(modname);
	if (!module)
		return FALSE;

	if (!imsettings_module_load(module)) {
		g_object_unref(module);
		return FALSE;
	}
	g_hash_table_insert(priv->modules,
			    g_strdup(imsettings_module_get_name(module)),
			    module);

	return TRUE;
}

static gboolean
imsettings_server_cb_unload_module(IMSettingsServer *server,
				   const gchar      *modname)
{
	IMSettingsServerPrivate *priv = server->priv;
	IMSettingsModule *module;
	gboolean retval = TRUE;

	if ((module = g_hash_table_lookup(priv->modules, modname)) != NULL) {
		g_hash_table_remove(priv->modules, modname);
	} else {
		retval = FALSE;
	}

	return retval;
}

static void
imsettings_server_bus_signal(GDBusConnection *connection,
			     const gchar     *sender,
			     const gchar     *object_path,
			     const gchar     *interface_name,
			     const gchar     *signal_name,
			     GVariant        *parameters,
			     gpointer         user_data)
{
	d(g_print("%s: sender[%s] path[%s] iface[%s] method[%s]\n", __PRETTY_FUNCTION__, sender, object_path, interface_name, signal_name));

	if (g_strcmp0(signal_name, "Reload") == 0) {
		/* ignore it anyway.
		 * this is just for the backward compatibility.
		 * and is obsolete since it doesn't give any clue
		 * how the action is going on.
		 */
	}
}

static void
imsettings_server_bus_method_call(GDBusConnection       *connection,
				  const gchar           *sender,
				  const gchar           *object_path,
				  const gchar           *interface_name,
				  const gchar           *method_name,
				  GVariant              *parameters,
				  GDBusMethodInvocation *invocation,
				  gpointer               user_data)
{
	GVariant *value = NULL;
	IMSettingsServer *server = IMSETTINGS_SERVER (user_data);
	IMSettingsServerPrivate *priv = server->priv;
	GError *err = NULL;

	d(g_print("%s: sender[%s] path[%s] iface[%s] method[%s]\n", __PRETTY_FUNCTION__, sender, object_path, interface_name, method_name));

	if (g_strcmp0(method_name, "StopService") == 0) {
		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("(b)", TRUE));
		g_signal_emit(server, signals[SIG_DISCONNECTED], 0, NULL);
	} else if (g_strcmp0(method_name, "GetVersion") == 0) {
		value = g_variant_new("(u)",
				      IMSETTINGS_SETTINGS_API_VERSION);
	} else if (g_strcmp0(method_name, "GetInfoVariants") == 0) {
		const gchar *lang;
		GVariant *v;

		g_variant_get(parameters, "(&s)",
			      &lang);

		v = imsettings_server_cb_get_info_variants(server, lang, &err);
		if (!v) {
			g_set_error(&err, IMSETTINGS_GERROR,
				    IMSETTINGS_GERROR_IM_NOT_FOUND,
				    "No IMs available");
		} else {
			value = g_variant_new_tuple(&v, 1);
		}
	} else if (g_strcmp0(method_name, "GetInfoVariant") == 0) {
		const gchar *lang, *module;
		GVariant *v;

		g_variant_get(parameters, "(&s&s)",
			      &lang, &module);

		v = imsettings_server_cb_get_info_variant(server,
							  lang,
							  module,
							  &err);
		if (!v) {
			g_set_error(&err, IMSETTINGS_GERROR,
				    IMSETTINGS_GERROR_IM_NOT_FOUND,
				    "No such input method: %s", module);
		} else {
			value = g_variant_new_tuple(&v, 1);
		}
	} else if (g_strcmp0(method_name, "GetSystemIM") == 0) {
		const gchar *lang;
		gchar *im;

		g_variant_get(parameters, "(&s)", &lang);
		im = imsettings_server_cb_get_system_im(server, lang, &err);
		if (im) {
			value = g_variant_new("(s)", im);
			g_free(im);
		}
	} else if (g_strcmp0(method_name, "GetUserIM") == 0) {
		const gchar *lang;
		gchar *im;

		g_variant_get(parameters, "(&s)", &lang);
		im = imsettings_server_cb_get_user_im(server, lang, &err);
		if (im) {
			value = g_variant_new("(s)", im);
			g_free(im);
		}
	} else if (g_strcmp0(method_name, "IsSystemDefault") == 0) {
		const gchar *lang, *module;
		gchar *im;

		g_variant_get(parameters, "(&s&s)", &lang, &module);
		im = imsettings_server_cb_get_system_im(server, lang, &err);
		if (im) {
			value = g_variant_new("(b)",
					      g_ascii_strcasecmp(im, module) == 0);
			g_free(im);
		}
	} else if (g_strcmp0(method_name, "IsUserDefault") == 0) {
		const gchar *lang, *module;
		gchar *im;

		g_variant_get(parameters, "(&s&s)", &lang, &module);
		im = imsettings_server_cb_get_user_im(server, lang, &err);
		if (im) {
			value = g_variant_new("(b)",
					      g_ascii_strcasecmp(im, module) == 0);
			g_free(im);
		}
	} else if (g_strcmp0(method_name, "IsXIM") == 0) {
		const gchar *lang, *module;
		GVariant *v;
		IMSettingsInfo *info;

		g_variant_get(parameters, "(&s&s)", &lang, &module);

		v = imsettings_server_cb_get_info_variant(server,
							  lang,
							  module,
							  &err);
		if (!v) {
			g_set_error(&err, IMSETTINGS_GERROR,
				    IMSETTINGS_GERROR_IM_NOT_FOUND,
				    _("No such input method: %s"),
				    module);
			goto finalize;
		}
		info = imsettings_info_new(v);
		if (!info) {
			g_set_error(&err, IMSETTINGS_GERROR,
				    IMSETTINGS_GERROR_OOM,
				    _("Out of memory"));
		} else {
			value = g_variant_new("(b)",
					      imsettings_info_is_xim(info));
			g_object_unref(info);
		}
		g_variant_unref(v);
	} else if (g_strcmp0(method_name, "SwitchIM") == 0) {
		const gchar *lang, *module;
		gboolean update, ret;
		IMSettingsInfo *info = NULL;

		g_variant_get(parameters, "(&s&sb)",
			      &lang, &module, &update);
		ret = imsettings_server_cb_switch_im(server, lang, module, update, &info, &err);
		if (ret) {
			GHashTableIter iter;
			gpointer key, val;
			IMSettingsModule *mod;

			g_hash_table_iter_init(&iter, priv->modules);
			while (g_hash_table_iter_next(&iter, &key, &val)) {
				mod = IMSETTINGS_MODULE (val);
				if (mod)
					imsettings_module_switch_im(mod, info);
			}
		}
		if (info)
			g_object_unref(info);

		value = g_variant_new("(b)", ret);
	} else if (g_strcmp0(method_name, "GetActiveVariant") == 0) {
		GVariant *v = NULL;
		gchar *f;

		if (!priv->current_im ||
		    !imsettings_proc_is_alive(priv->current_im)) {
			f = g_build_filename(imsettings_server_get_xinputdir(server),
					     IMSETTINGS_NONE_CONF XINPUT_SUFFIX, NULL);
			if (g_file_test(f, G_FILE_TEST_EXISTS))
				v = imsettings_info_variant_new(f, NULL);
			g_free(f);
		} else {
			IMSettingsInfo *info = imsettings_proc_info(priv->current_im);

			v = imsettings_info_variant_new(imsettings_info_get_filename(info),
							imsettings_info_get_language(info));
		}
		if (!v) {
			g_set_error(&err, IMSETTINGS_GERROR, IMSETTINGS_GERROR_CONFIGURATION_ERROR,
				    "none.conf isn't installed.");
		} else {
			value = g_variant_new_tuple(&v, 1);
		}
	} else if (g_strcmp0(method_name, "LoadModule") == 0) {
		const gchar *modname;
		gboolean ret;

		g_variant_get(parameters, "(&s)", &modname);

		ret = imsettings_server_cb_load_module(server, modname);
		value = g_variant_new("(b)", ret);
	} else if (g_strcmp0(method_name, "UnloadModule") == 0) {
		const gchar *modname;
		gboolean ret;

		g_variant_get(parameters, "(&s)", &modname);

		ret = imsettings_server_cb_unload_module(server, modname);
		value = g_variant_new("(b)", ret);
	}
  finalize:
	if (err) {
		g_dbus_method_invocation_return_error(invocation,
						      IMSETTINGS_GERROR,
						      err->code,
						      err->message);
		g_error_free(err);
		if (value)
			g_variant_unref(value);
	} else {
		if (value) {
			g_dbus_method_invocation_return_value(invocation, value);
			g_variant_unref(value);
		}
	}
}

static GVariant *
imsettings_server_bus_get_property(GDBusConnection  *connection,
				   const gchar      *sender,
				   const gchar      *object_path,
				   const gchar      *interface_name,
				   const gchar      *property_name,
				   GError          **error,
				   gpointer          user_data)
{
	g_print("XXX: %s\n", __PRETTY_FUNCTION__);
	return NULL;
}

static gboolean
imsettings_server_bus_set_property(GDBusConnection  *connection,
				   const gchar      *sender,
				   const gchar      *object_paht,
				   const gchar      *interface_name,
				   const gchar      *property_name,
				   GVariant         *value,
				   GError          **error,
				   gpointer          user_data)
{
	g_print("XXX: %s\n", __PRETTY_FUNCTION__);
	return FALSE;
}

static void
imsettings_server_bus_on_name_acquired(GDBusConnection *connection,
				       const gchar     *name,
				       gpointer         user_data)
{
	IMSettingsServer *server = IMSETTINGS_SERVER (user_data);
	IMSettingsServerPrivate *priv = server->priv;
	GError *err = NULL;
	GList *l, *ll;
	GString *s;

	priv->id = g_dbus_connection_register_object(priv->connection,
						     IMSETTINGS_PATH_DBUS,
						     imsettings_get_interface_info(),
						     &__iface_vtable,
						     server, /* user_data */
						     NULL, /* user_data_free_func */
						     &err);
	if (err) {
		g_error("%s", err->message);
	}
	priv->signal_id = g_dbus_connection_signal_subscribe(connection, NULL, IMSETTINGS_INTERFACE_DBUS, NULL, IMSETTINGS_PATH_DBUS, NULL, G_DBUS_SIGNAL_FLAGS_NONE, imsettings_server_bus_signal, server, NULL);

	priv->active = TRUE;
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
	      "Starting imsettings-daemon...");
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
	      "  [HOME=%s]", imsettings_server_get_homedir(server));
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
	      "  [XINPUTRCDIR=%s]", imsettings_server_get_xinputrcdir(server));
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
	      "  [XINPUTDIR=%s]\n", imsettings_server_get_xinputdir(server));
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
	      "  [MODULEDIR=%s]\n", imsettings_server_get_moduledir(server));

	l = g_hash_table_get_keys(priv->modules);
	s = g_string_new(NULL);
	for (ll = l; ll != NULL; ll = g_list_next(ll)) {
		gchar *mn = ll->data;

		if (s->len > 0)
			g_string_append(s, ", ");
		g_string_append(s, mn);
	}
	g_list_free(l);
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
	      "  [MODULES=%s]\n", s->str);
	g_string_free(s, TRUE);
}

static void
imsettings_server_bus_on_name_lost(GDBusConnection *connection,
				   const gchar     *name,
				   gpointer         user_data)
{
	IMSettingsServer *server = IMSETTINGS_SERVER (user_data);
	IMSettingsServerPrivate *priv = server->priv;

	if (priv->active) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		      "Release the ownership of %s\n", name);
	} else {
		g_critical("Unable to acquire the ownership of %s.\n", name);
	}
	g_signal_emit(server, signals[SIG_DISCONNECTED], 0, NULL);
}

static const gchar *
imsettings_server_get_homedir(IMSettingsServer *server)
{
	IMSettingsServerPrivate *priv = server->priv;

	if (!priv->homedir) {
		priv->homedir = g_strdup(g_get_home_dir());
	}
	return priv->homedir;
}

static const gchar *
imsettings_server_get_xinputrcdir(IMSettingsServer *server)
{
	IMSettingsServerPrivate *priv = server->priv;

	if (!priv->xinputrcdir) {
		priv->xinputrcdir = g_strdup(XINPUTRC_PATH);
	}
	return priv->xinputrcdir;
}

static const gchar *
imsettings_server_get_xinputdir(IMSettingsServer *server)
{
	IMSettingsServerPrivate *priv = server->priv;

	if (!priv->xinputdir) {
		priv->xinputdir = g_strdup(XINPUT_PATH);
	}
	return priv->xinputdir;
}

static const gchar *
imsettings_server_get_moduledir(IMSettingsServer *server)
{
	IMSettingsServerPrivate *priv = server->priv;

	if (!priv->moduledir) {
		priv->moduledir = g_strdup(IMSETTINGS_MODULE_PATH);
	}
	return priv->moduledir;
}

static void
imsettings_server_load_modules(IMSettingsServer *server)
{
	GFile *file;
	GFileEnumerator *e;
	gchar **path_list;
	gint i;
	gboolean flag = FALSE;

	path_list = g_strsplit(imsettings_server_get_moduledir(server),
			       G_SEARCHPATH_SEPARATOR_S,
			       -1);
	for (i = 0; path_list[i] != NULL; i++) {
		file = g_file_new_for_path(path_list[i]);
		e = g_file_enumerate_children(file, "standard::*",
					      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					      NULL, NULL);
		if (e) {
			GFileInfo *info = NULL;
			GError *err = NULL;
			const gchar *n;
			gchar *filename;

			while (1) {
				info = g_file_enumerator_next_file(e, NULL, &err);
				if (!info && !err)
					break;
				if (err) {
					g_warning("Unable to obtain the module information: %s",
						  err->message);
					g_clear_error(&err);
					goto next;
				}
				n = g_file_info_get_name(info);
				if (!n)
					goto next;
				filename = g_path_get_basename(n);

				flag = imsettings_server_cb_load_module(server, filename);
				g_free(filename);
			  next:
				if (info)
					g_object_unref(info);
			}
			g_file_enumerator_close(e, NULL, NULL);
			g_object_unref(e);
		}
		g_object_unref(file);
	}
}

/*< public >*/

/**
 * imsettings_server_new:
 * @connection:
 * @homedir:
 * @xinputrcdir:
 * @xinputdir:
 *
 * FIXME
 *
 * Returns:
 */
IMSettingsServer *
imsettings_server_new(GDBusConnection *connection,
		      const gchar     *homedir,
		      const gchar     *xinputrcdir,
		      const gchar     *xinputdir,
		      const gchar     *moduledir)
{
	g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);

	return IMSETTINGS_SERVER (g_object_new(IMSETTINGS_TYPE_SERVER,
					       "connection", connection,
					       "homedir", homedir,
					       "xinputrcdir", xinputrcdir,
					       "xinputdir", xinputdir,
					       "moduledir", moduledir,
					       NULL));
}

/**
 * imsettings_server_start:
 * @server:
 * @replace:
 *
 * FIXME
 *
 * Returns:
 */
void
imsettings_server_start(IMSettingsServer *server,
			gboolean          replace)
{
	IMSettingsServerPrivate *priv;
	guint flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;

	g_return_if_fail (IMSETTINGS_IS_SERVER (server));

	priv = server->priv;
	imsettings_server_load_modules(server);

	if (priv->owner == 0) {
		if (replace)
			flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

		priv->owner = g_bus_own_name_on_connection(priv->connection,
							   IMSETTINGS_SERVICE_DBUS,
							   flags,
							   imsettings_server_bus_on_name_acquired,
							   imsettings_server_bus_on_name_lost,
							   server,
							   NULL);
	}
	if (!priv->old_log_handler)
		g_log_set_default_handler(imsettings_server_log_handler, server);
}
