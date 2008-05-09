/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * factory_info.c
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

#include <fam.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib/gthread.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-info-private.h"
#include "imsettings/imsettings-marshal.h"
#include "imsettings/imsettings-observer.h"
#include "imsettings/imsettings-utils.h"


typedef struct _IMSettingsInfoManagerClass	IMSettingsInfoManagerClass;
typedef struct _IMSettingsInfoManager		IMSettingsInfoManager;
typedef struct _IMSettingsInfoManagerPrivate	IMSettingsInfoManagerPrivate;


#define IMSETTINGS_TYPE_INFO_MANAGER			(imsettings_info_manager_get_type())
#define IMSETTINGS_INFO_MANAGER(_o_)			(G_TYPE_CHECK_INSTANCE_CAST ((_o_), IMSETTINGS_TYPE_INFO_MANAGER, IMSettingsInfoManager))
#define IMSETTINGS_INFO_MANAGER_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), IMSETTINGS_TYPE_INFO_MANAGER, IMSettingsInfoManagerClass))
#define IMSETTINGS_IS_INFO_MANAGER(_o_)			(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), IMSETTINGS_TYPE_INFO_MANAGER))
#define IMSETTINGS_IS_INFO_MANAGER_CLASS(_c_)		(G_TYPE_CHECK_CLASS_TYPE ((_c_), IMSETTINGS_TYPE_INFO_MANAGER))
#define IMSETTINGS_INFO_MANAGER_GET_CLASS(_o_)		(G_TYPE_INSTANCE_GET_CLASS ((_o_), IMSETTINGS_TYPE_INFO_MANAGER, IMSettingsInfoManagerClass))
#define IMSETTINGS_INFO_MANAGER_GET_PRIVATE(_o_)	(G_TYPE_INSTANCE_GET_PRIVATE ((_o_), IMSETTINGS_TYPE_INFO_MANAGER, IMSettingsInfoManagerPrivate))

enum {
	FAM_MONITOR_START = 0,
	FAM_MONITOR_USER_XINPUTRC,
	FAM_MONITOR_SYSTEM_XINPUTRC,
	FAM_MONITOR_XINPUTD,
	FAM_MONITOR_END
};
enum {
	FAM_STAT_UNKNOWN,
	FAM_STAT_IN_PROGRESS,
	FAM_STAT_UPDATED,
	FAM_STAT_END
};
enum {
	STATUS_CHANGED,
	LAST_SIGNAL
};

struct _IMSettingsInfoManagerClass {
	IMSettingsObserverClass parent_class;

	void (* status_changed) (IMSettingsInfoManager *manager,
				 const gchar           *filename);
};
struct _IMSettingsInfoManager {
	IMSettingsObserver parent_instance;
};
struct _IMSettingsInfoManagerPrivate {
	GHashTable     *im_info_from_name;
	GHashTable     *im_info_from_filename;
	DBusConnection *req_conn;
	FAMConnection   fam_conn;
	FAMRequest      fam_req[FAM_MONITOR_END];
	guint           fam_status[FAM_MONITOR_END];
	guint           fam_source_id;
	gchar          *current_user_im;
	gchar          *current_system_im;
};

static void imsettings_info_manager_init_fam(IMSettingsInfoManager *manager);
static void imsettings_info_manager_tini_fam(IMSettingsInfoManager *manager);
GType imsettings_info_manager_get_type(void) G_GNUC_CONST;


guint signals[LAST_SIGNAL] = { 0 };

G_LOCK_DEFINE_STATIC(imsettings_info_manager);
G_DEFINE_TYPE (IMSettingsInfoManager, imsettings_info_manager, IMSETTINGS_TYPE_OBSERVER);


/*
 * Private functions
 */
static void
disconnected_cb(IMSettingsObserver *observer,
		GMainLoop          *loop)
{
	g_main_loop_quit(loop);
}

static void
reload_cb(IMSettingsObserver *observer,
	  gboolean            force)
{
	if (force) {
		GMainLoop *loop = g_object_get_data(G_OBJECT (observer), "imsettings-daemon-main");

		g_main_loop_quit(loop);
	} else {
		IMSettingsInfoManager *manager = IMSETTINGS_INFO_MANAGER (observer);

		g_print(_("Reloading...\n"));
		imsettings_info_manager_tini_fam(manager);
		imsettings_info_manager_init_fam(manager);
	}
}


static gint
imsettings_info_manager_get_fam_target(IMSettingsInfoManagerPrivate *priv,
				       FAMRequest                   *req)
{
	gint i, retval = 0;

	for (i = FAM_MONITOR_START; i < FAM_MONITOR_END; i++) {
		if (FAMREQUEST_GETREQNUM (&priv->fam_req[i]) == FAMREQUEST_GETREQNUM (req)) {
			retval = i;
			break;
		}
	}

	return retval;
}

static IMSettingsInfo *
imsettings_info_manager_add_info(IMSettingsInfoManagerPrivate *priv,
				 const gchar                  *filename,
				 gboolean                      is_xinputrc)
{
	IMSettingsInfo *info, *ret;
	const gchar *name;

	info = imsettings_info_new(filename);
	if (info != NULL) {
		name = imsettings_info_get_short_desc(info);

		G_LOCK (imsettings_info_manager);

		if ((ret = g_hash_table_lookup(priv->im_info_from_name, name)) == NULL) {
			g_hash_table_insert(priv->im_info_from_name,
					    g_strdup(name),
					    info);
			g_hash_table_insert(priv->im_info_from_filename,
					    g_strdup(filename),
					    g_object_ref(info));
		} else {
			if (strcmp(name, IMSETTINGS_NONE_CONF) == 0 ||
			    strcmp(name, IMSETTINGS_USER_SPECIFIC_SHORT_DESC) == 0) {
				/* We deal with none.conf specially here.
				 * It has to be registered as is anyway.
				 */
				g_hash_table_replace(priv->im_info_from_name,
						     g_strdup(name),
						     info);
				g_hash_table_replace(priv->im_info_from_filename,
						     g_strdup(filename),
						     g_object_ref(info));
			} else if (is_xinputrc) {
				/* just return the object in the hash */
				g_object_unref(info);
				info = ret;
			} else {
				g_warning(_("Duplicate entry `%s' from %s. SHORT_DESC has to be unique."),
					  name, filename);
				g_object_unref(info);
				info = NULL;
			}
		}

		G_UNLOCK (imsettings_info_manager);
	}

	return info;
}

static gboolean
imsettings_info_manager_remove_info(IMSettingsInfoManagerPrivate *priv,
				    const gchar                  *filename,
				    gboolean                      is_xinputrc)
{
	IMSettingsInfo *info;
	const gchar *name;
	gboolean retval = FALSE;

	G_LOCK (imsettings_info_manager);

	if ((info = g_hash_table_lookup(priv->im_info_from_filename, filename))) {
		name = imsettings_info_get_short_desc(info);
		g_hash_table_remove(priv->im_info_from_filename, filename);
		if (!is_xinputrc ||
		    strcmp(name, IMSETTINGS_USER_SPECIFIC_SHORT_DESC) == 0)
			g_hash_table_remove(priv->im_info_from_name, name);
		retval = TRUE;
	}

	G_UNLOCK (imsettings_info_manager);

	return retval;
}

static gboolean
imsettings_info_manager_fam_event_loop(gpointer data)
{
	IMSettingsInfoManager *manager = IMSETTINGS_INFO_MANAGER (data);
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (manager);
	IMSettingsInfo *info, *old_info;
	FAMEvent ev;
	gchar *filename = NULL;
	gsize suffix_len = strlen(XINPUT_SUFFIX), path_len = strlen(XINPUT_PATH);
	gsize len;
	gint target;
	gboolean proceeded = FALSE;

	while (FAMPending(&priv->fam_conn) > 0) {
		FAMNextEvent(&priv->fam_conn, &ev);
		target = imsettings_info_manager_get_fam_target(priv, &ev.fr);
		switch (target) {
		    case FAM_MONITOR_USER_XINPUTRC:
		    case FAM_MONITOR_SYSTEM_XINPUTRC:
			    filename = g_strdup(ev.filename);
			    switch (ev.code) {
				case FAMChanged:
					imsettings_info_manager_remove_info(priv, filename, TRUE);
				case FAMCreated:
				case FAMExists:
					if (ev.code == FAMExists)
						priv->fam_status[target] = FAM_STAT_IN_PROGRESS;
					info = imsettings_info_manager_add_info(priv, filename, TRUE);

					G_LOCK (imsettings_info_manager);

					if (target == FAM_MONITOR_USER_XINPUTRC) {
						if (priv->current_user_im &&
						    (old_info = g_hash_table_lookup(priv->im_info_from_name,
										    priv->current_user_im)))
							g_object_set(G_OBJECT (old_info),
								     "is_user_default",
								     FALSE,
								     NULL);
						g_object_set(G_OBJECT (info), "is_user_default", TRUE, NULL);
						g_free(priv->current_user_im);
						priv->current_user_im = g_strdup(imsettings_info_get_short_desc(info));
					} else {
						if (priv->current_system_im &&
						    (old_info = g_hash_table_lookup(priv->im_info_from_name,
										    priv->current_system_im)))
							g_object_set(G_OBJECT (info),
								     "is_system_default",
								     FALSE,
								     NULL);
						g_object_set(G_OBJECT (info), "is_system_default", TRUE, NULL);
						g_free(priv->current_system_im);
						priv->current_system_im = g_strdup(imsettings_info_get_short_desc(info));
					}
					g_hash_table_replace(priv->im_info_from_filename,
							     g_strdup(filename),
							     g_object_ref(info));

					G_UNLOCK (imsettings_info_manager);

					proceeded = TRUE;
					break;
				case FAMEndExist:
					priv->fam_status[target] = FAM_STAT_UPDATED;
					break;
				case FAMDeleted:
					imsettings_info_manager_remove_info(priv, filename, TRUE);
					proceeded = TRUE;
					break;
				case FAMStartExecuting:
				case FAMStopExecuting:
				case FAMMoved:
				case FAMAcknowledge:
				default:
					g_warning("Status has been changed on %s with %d", filename, ev.code);
					break;
			    }
			    break;
		    case FAM_MONITOR_XINPUTD:
			    if (ev.code == FAMEndExist)
				    priv->fam_status[target] = FAM_STAT_UPDATED;

			    len = strlen(ev.filename);

			    /* XXX: it may assume that XINPUT_PATH is valid path
			     *      and trying to exclude it because it's just reported
			     *      at the beginning in the transaction, but just contains
			     *      its filename for each entries.
			     */
			    if (len > suffix_len &&
				strcmp(&ev.filename[len - suffix_len], XINPUT_SUFFIX) == 0 &&
				strncmp(ev.filename, XINPUT_PATH, path_len) != 0)
				    filename = g_build_filename(XINPUT_PATH, ev.filename, NULL);

			    if (filename == NULL)
				    break;
			    switch (ev.code) {
				case FAMChanged:
					/* better recreating an instance */
					imsettings_info_manager_remove_info(priv, filename, FALSE);
				case FAMCreated:
				case FAMExists:
					if (ev.code == FAMExists)
						priv->fam_status[target] = FAM_STAT_IN_PROGRESS;
					imsettings_info_manager_add_info(priv, filename, FALSE);
					proceeded = TRUE;
					break;
				case FAMDeleted:
					imsettings_info_manager_remove_info(priv, filename, FALSE);
					proceeded = TRUE;
					break;
				case FAMStartExecuting:
				case FAMStopExecuting:
				case FAMMoved:
				case FAMAcknowledge:
				default:
					g_warning("Status has been changed on %s with %d", filename, ev.code);
					break;
			    }
			    break;
		    default:
			    g_warning("Unknown monitoring type: %d\n", target);
			    break;
		}
		if (proceeded)
			g_signal_emit(manager, signals[STATUS_CHANGED], 0,
				      filename, NULL);
		g_free(filename);
		filename = NULL;

		g_main_context_iteration(g_main_context_default(), FALSE);
	}
	sleep(1);

	return TRUE;
}

static void
imsettings_info_manager_real_finalize(GObject *object)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (object);

	if (priv->im_info_from_name)
		g_hash_table_destroy(priv->im_info_from_name);
	if (priv->im_info_from_filename)
		g_hash_table_destroy(priv->im_info_from_filename);
	dbus_connection_unref(priv->req_conn);
	imsettings_info_manager_tini_fam(IMSETTINGS_INFO_MANAGER (object));
	FAMClose(&priv->fam_conn);

	if (G_OBJECT_CLASS (imsettings_info_manager_parent_class)->finalize)
		G_OBJECT_CLASS (imsettings_info_manager_parent_class)->finalize(object);
}

typedef struct _IMSettingsManagerCollectList {
	GQueue      *q;
	const gchar *lang;
	gchar       *legacy_im;
} IMSettingsManagerCollectList;

static void
_collect_im_list(gpointer key,
		 gpointer val,
		 gpointer data)
{
	IMSettingsManagerCollectList *v = data;
	IMSettingsInfo *info = IMSETTINGS_INFO (val);

	if (!imsettings_info_is_xim(info)) {
		if (imsettings_info_is_visible(info)) {
			if (imsettings_info_is_system_default(info))
				g_queue_push_head(v->q, g_strdup(key));
			else
				g_queue_push_tail(v->q, g_strdup(key));
		}
	} else {
		/* need to update the short description with the lang */
		g_object_set(G_OBJECT (info), "language", v->lang, NULL);
		if (imsettings_info_is_visible(info)) {
			if (v->legacy_im)
				g_return_if_reached();
			v->legacy_im = g_strdup(imsettings_info_get_short_desc(info));
		}
	}
}

static gboolean
imsettings_info_manager_pending_init(IMSettingsInfoManagerPrivate *priv)
{
	gint i;
	gboolean retval = TRUE;
	GTimer *timer;

	timer = g_timer_new();
	g_timer_start(timer);
	for (i = FAM_MONITOR_START; i < FAM_MONITOR_END; i++) {
		while (retval && priv->fam_status[i] != FAM_STAT_UPDATED) {
			sleep(1);
			g_main_context_iteration(g_main_context_default(), FALSE);
			if (g_timer_elapsed(timer, NULL) > 10L)
				retval = FALSE;
		}
	}
	g_timer_destroy(timer);

	return retval;
}

static GPtrArray *
imsettings_info_manager_real_get_list(IMSettingsObserver  *observer,
				      const gchar         *lang,
				      GError             **error)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (observer);
	IMSettingsManagerCollectList v;
	GPtrArray *array = g_ptr_array_new();
	gchar *s;

	if (!imsettings_info_manager_pending_init(priv)) {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_FAILED,
			    _("Timed out to initialize %s services."),
			    IMSETTINGS_INFO_INTERFACE_DBUS);
		return NULL;
	}

	v.q = g_queue_new();
	v.lang = lang;
	v.legacy_im = NULL;
	g_queue_init(v.q);

	G_LOCK (imsettings_info_manager);

	g_hash_table_foreach(priv->im_info_from_name, _collect_im_list, &v);

	G_UNLOCK (imsettings_info_manager);

	while ((s = g_queue_pop_head(v.q)))
		g_ptr_array_add(array, s);
	if (v.legacy_im)
		g_ptr_array_add(array, v.legacy_im);
	g_queue_free(v.q);
	if (array->len == 0) {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_NOT_AVAILABLE,
			    _("No input methods available on your system."));
	}
	g_ptr_array_add(array, NULL);

	return array;
}

static const gchar *
imsettings_info_manager_real_get_current_user_im(IMSettingsObserver  *observer,
						 GError             **error)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (observer);

	if (!imsettings_info_manager_pending_init(priv)) {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_FAILED,
			    _("Timed out to initialize %s services."),
			    IMSETTINGS_INFO_INTERFACE_DBUS);
		return NULL;
	}

	return (priv->current_user_im ? priv->current_user_im : priv->current_system_im);
}

static const gchar *
imsettings_info_manager_real_get_current_system_im(IMSettingsObserver  *observer,
						   GError             **error)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (observer);

	if (!imsettings_info_manager_pending_init(priv)) {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_FAILED,
			    _("Timed out to initialize %s services."),
			    IMSETTINGS_INFO_INTERFACE_DBUS);
		return NULL;
	}

	return priv->current_system_im;
}

static IMSettingsInfo *
imsettings_info_manager_real_get_info(IMSettingsObserver  *observer,
				      const gchar         *module,
				      GError             **error)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (observer);
	IMSettingsInfo *info;

	if (!imsettings_info_manager_pending_init(priv)) {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_FAILED,
			    _("Timed out to initialize %s services."),
			    IMSETTINGS_INFO_INTERFACE_DBUS);
		return NULL;
	}

	info = g_hash_table_lookup(priv->im_info_from_name, module);
	if (info == NULL) {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_IM_NOT_FOUND,
			    _("No such input method on your system: %s"), module);
	}

	return info;
}

static void
imsettings_info_manager_init_fam(IMSettingsInfoManager *manager)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (manager);
	const gchar *homedir;
	gchar *file;
	gint i;

	g_hash_table_remove_all(priv->im_info_from_filename);
	g_hash_table_remove_all(priv->im_info_from_name);

	/* initialize FAM */
	priv->fam_status[FAM_MONITOR_START] = FAM_STAT_UPDATED;
	for (i = FAM_MONITOR_START + 1; i < FAM_MONITOR_END; i++) {
		priv->fam_status[i] = FAM_STAT_UNKNOWN;
	}
	FAMOpen(&priv->fam_conn);

	FAMMonitorDirectory(&priv->fam_conn, XINPUT_PATH, &priv->fam_req[FAM_MONITOR_XINPUTD], NULL);
	homedir = g_get_home_dir();
	if (homedir != NULL) {
		file = g_build_filename(homedir, IMSETTINGS_USER_XINPUT_CONF, NULL);
		FAMMonitorFile(&priv->fam_conn, file, &priv->fam_req[FAM_MONITOR_USER_XINPUTRC], NULL);
		g_free(file);
	}
	file = g_build_filename(XINPUTRC_PATH, IMSETTINGS_GLOBAL_XINPUT_CONF, NULL);
	FAMMonitorFile(&priv->fam_conn, file, &priv->fam_req[FAM_MONITOR_SYSTEM_XINPUTRC], NULL);
	g_free(file);

	priv->fam_source_id = g_idle_add(imsettings_info_manager_fam_event_loop,
					 manager);
}

static void
imsettings_info_manager_tini_fam(IMSettingsInfoManager *manager)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (manager);

	g_idle_remove_by_data(manager);
	FAMCancelMonitor(&priv->fam_conn, &priv->fam_req[FAM_MONITOR_USER_XINPUTRC]);
	FAMCancelMonitor(&priv->fam_conn, &priv->fam_req[FAM_MONITOR_SYSTEM_XINPUTRC]);
	FAMCancelMonitor(&priv->fam_conn, &priv->fam_req[FAM_MONITOR_XINPUTD]);
}

static void
imsettings_info_manager_class_init(IMSettingsInfoManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	IMSettingsObserverClass *observer_class = IMSETTINGS_OBSERVER_CLASS (klass);

	g_type_class_add_private(klass, sizeof (IMSettingsInfoManagerPrivate));

	object_class->finalize = imsettings_info_manager_real_finalize;

	observer_class->get_list = imsettings_info_manager_real_get_list;
	observer_class->get_current_user_im = imsettings_info_manager_real_get_current_user_im;
	observer_class->get_current_system_im = imsettings_info_manager_real_get_current_system_im;
	observer_class->get_info = imsettings_info_manager_real_get_info;

	/* signals */
	signals[STATUS_CHANGED] = g_signal_new("status-changed",
					       G_OBJECT_CLASS_TYPE (klass),
					       G_SIGNAL_RUN_FIRST,
					       G_STRUCT_OFFSET (IMSettingsInfoManagerClass, status_changed),
					       NULL, NULL,
					       imsettings_marshal_VOID__STRING_INT,
					       G_TYPE_NONE, 2,
					       G_TYPE_STRING, G_TYPE_INT);
}

static void
imsettings_info_manager_init(IMSettingsInfoManager *manager)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (manager);

	priv->req_conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	priv->im_info_from_name = g_hash_table_new_full(g_str_hash,
							g_str_equal,
							g_free,
							g_object_unref);
	priv->im_info_from_filename = g_hash_table_new_full(g_str_hash,
							    g_str_equal,
							    g_free,
							    g_object_unref);

	imsettings_info_manager_init_fam(manager);
}

static IMSettingsInfoManager *
imsettings_info_manager_new(DBusGConnection *conn,
			    gboolean         replace)
{
	g_return_val_if_fail (conn != NULL, NULL);

	return IMSETTINGS_INFO_MANAGER (g_object_new(IMSETTINGS_TYPE_INFO_MANAGER,
						     "replace", replace,
						     "connection", conn,
						     NULL));
}

/*
 * Public functions
 */
int
main(int    argc,
     char **argv)
{
	GError *error = NULL;
	GMainLoop *loop;
	IMSettingsInfoManager *manager;
	gboolean arg_replace = FALSE;
	GOptionContext *ctx = g_option_context_new(NULL);
	GOptionEntry entries[] = {
		{"replace", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_replace, N_("Replace the running settings daemon with new instance."), NULL},
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	DBusGConnection *gconn;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, IMSETTINGS_LOCALEDIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif /* HAVE_BIND_TEXTDOMAIN_CODESET */
	textdomain (GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */

	g_type_init();
	g_thread_init(NULL);

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
	manager = imsettings_info_manager_new(gconn, arg_replace);
	if (manager == NULL)
		exit(1);

	if (!imsettings_observer_setup(IMSETTINGS_OBSERVER (manager), IMSETTINGS_INFO_SERVICE_DBUS)) {
		exit(1);
	}

	loop = g_main_loop_new(NULL, FALSE);
	g_object_set_data(G_OBJECT (manager), "imsettings-daemon-main", loop);

	g_signal_connect(manager, "disconnected",
			 G_CALLBACK (disconnected_cb),
			 loop);
	g_signal_connect(manager, "reload",
			 G_CALLBACK (reload_cb),
			 NULL);

	g_main_loop_run(loop);

	g_object_unref(manager);
	dbus_g_connection_unref(gconn);

	return 0;
}
