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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib/gthread.h>
#include <gio/gio.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-info-private.h"
#include "imsettings/imsettings-marshal.h"
#include "imsettings/imsettings-observer.h"
#include "imsettings/imsettings-utils.h"


#ifdef GNOME_ENABLE_DEBUG
#define d(e)	e
#else
#define d(e)
#endif


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
	PROP_0 = 0,
	PROP_XINPUTRCDIR,
	PROP_XINPUTDIR,
	PROP_HOMEDIR,
	LAST_PROP
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
	GFileMonitor   *mon_xinputd;
	GFileMonitor   *mon_dot_xinputrc;
	GFileMonitor   *mon_xinputrc;
	gchar          *current_user_im;
	gchar          *current_system_im;
	gchar          *xinputrcdir;
	gchar          *xinputdir;
	gchar          *homedir;
};

static gboolean imsettings_info_manager_init_monitor(IMSettingsInfoManager *manager);
static void     imsettings_info_manager_tini_monitor(IMSettingsInfoManager *manager);
GType           imsettings_info_manager_get_type    (void) G_GNUC_CONST;


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
		imsettings_info_manager_tini_monitor(manager);
		imsettings_info_manager_init_monitor(manager);
	}
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
			d(g_print("Adding %s(%s)...\n", filename, name));
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
				d(g_print("Adding %s(%s)...\n", filename, name));
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
	} else {
		g_warning("Unable to get the file information for `%s'",
			  filename);
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
		d(g_print("Removing %s(%s)...\n", filename, name));
		g_hash_table_remove(priv->im_info_from_filename, filename);
		if (!is_xinputrc ||
		    strcmp(name, IMSETTINGS_USER_SPECIFIC_SHORT_DESC) == 0) {
			d(g_print("Removing %s...\n", name));
			g_hash_table_remove(priv->im_info_from_name, name);
		}
		retval = TRUE;
	}

	G_UNLOCK (imsettings_info_manager);

	return retval;
}

static void
imsettings_info_manager_real_set_property(GObject      *object,
					  guint         prop_id,
					  const GValue *value,
					  GParamSpec   *pspec)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (object);
	const gchar *p;

	switch (prop_id) {
	    case PROP_XINPUTRCDIR:
		    p = g_value_get_string(value);
		    if (p == NULL ||
			!g_file_test(p, G_FILE_TEST_IS_DIR)) {
			    g_warning("Given directory through `xinputrcdir' property is invalid: %s",
				      p);
		    } else {
			    d(g_print("*** changing xinputrc dir from %s to %s\n",
				      priv->xinputrcdir, p));
			    g_free(priv->xinputrcdir);
			    priv->xinputrcdir = g_strdup(p);
		    }
		    break;
	    case PROP_XINPUTDIR:
		    p = g_value_get_string(value);
		    if (p == NULL ||
			!g_file_test(p, G_FILE_TEST_IS_DIR)) {
			    g_warning("Given directory through `xinputdir' property is invalid: %s",
				      p);
		    } else {
			    d(g_print("*** changing xinput dir from %s to %s\n",
				      priv->xinputdir, p));
			    g_free(priv->xinputdir);
			    priv->xinputdir = g_strdup(p);
		    }
		    break;
	    case PROP_HOMEDIR:
		    p = g_value_get_string(value);
		    if (p == NULL ||
			!g_file_test(p, G_FILE_TEST_IS_DIR)) {
			    g_warning("Given directory through `homedir' property is invalid: %s",
				      p);
		    } else {
			    d(g_print("*** changing home dir from %s to %s\n",
				      priv->homedir, p));
			    g_free(priv->homedir);
			    priv->homedir = g_strdup(p);
		    }
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_info_manager_real_get_property(GObject    *object,
					  guint       prop_id,
					  GValue     *value,
					  GParamSpec *pspec)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (object);

	switch (prop_id) {
	    case PROP_XINPUTRCDIR:
		    g_value_set_string(value, priv->xinputrcdir);
		    break;
	    case PROP_XINPUTDIR:
		    g_value_set_string(value, priv->xinputdir);
		    break;
	    case PROP_HOMEDIR:
		    g_value_set_string(value, priv->homedir);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
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
	imsettings_info_manager_tini_monitor(IMSETTINGS_INFO_MANAGER (object));
	g_free(priv->xinputrcdir);
	g_free(priv->xinputdir);
	g_free(priv->homedir);

	if (G_OBJECT_CLASS (imsettings_info_manager_parent_class)->finalize)
		G_OBJECT_CLASS (imsettings_info_manager_parent_class)->finalize(object);
}

static gboolean
imsettings_info_manager_validate_from_file_info(GFileInfo *info)
{
	GFileType type;
	const char *p;
	gsize slen = strlen(XINPUT_SUFFIX), len;

	type = g_file_info_get_file_type(info);
	if (type != G_FILE_TYPE_REGULAR &&
	    type != G_FILE_TYPE_SYMBOLIC_LINK &&
	    type != G_FILE_TYPE_SHORTCUT) {
		/* just ignore them */
		return FALSE;
	}
	p = g_file_info_get_name(info);
	if (p == NULL)
		return FALSE;
	len = strlen(p);
	if (len <= slen ||
	    strcmp(&p[len - slen], XINPUT_SUFFIX) != 0)
		return FALSE;

	return TRUE;
}

static gboolean
imsettings_info_manager_validate_from_file(GFile *file)
{
	GFileInfo *fi;
	gboolean retval;

	fi = g_file_query_info(file,
			       G_FILE_ATTRIBUTE_STANDARD_TYPE ","
			       G_FILE_ATTRIBUTE_STANDARD_NAME,
			       G_FILE_QUERY_INFO_NONE,
			       NULL, NULL);
	if (!fi) {
		gchar *filename = g_file_get_path(file);

		g_warning("Unable to get the file information to validate: `%s'",
			  filename);
		g_free(filename);

		return FALSE;
	}

	retval = imsettings_info_manager_validate_from_file_info(fi);
	g_object_unref(fi);

	return retval;
}

static void
imsettings_info_manager_real_changed_xinputd(GFileMonitor      *monitor,
					     GFile             *file,
					     GFile             *other_file,
					     GFileMonitorEvent  event_type,
					     gpointer           user_data)
{
	IMSettingsInfoManager *manager = IMSETTINGS_INFO_MANAGER (user_data);
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (manager);
	gchar *filename = g_file_get_path(file);
	gsize len = strlen(filename);
	gsize suffix_len = strlen(XINPUT_SUFFIX);
	gboolean proceeded = FALSE;

	switch (event_type) {
	    case G_FILE_MONITOR_EVENT_CHANGED:
		    if (len <= suffix_len ||
			strcmp(&filename[len - suffix_len], XINPUT_SUFFIX) != 0) {
			    /* just ignore this */
			    d(g_print("Ignoring the change of `%s'.\n", filename));
			    break;
		    }
		    imsettings_info_manager_remove_info(priv, filename, FALSE);
	    case G_FILE_MONITOR_EVENT_CREATED:
		    if (!imsettings_info_manager_validate_from_file(file)) {
			    /* ignore them */
			    goto end;
		    }
		    imsettings_info_manager_add_info(priv, filename, FALSE);
		    proceeded = TRUE;
		    break;
	    case G_FILE_MONITOR_EVENT_DELETED:
		    imsettings_info_manager_remove_info(priv, filename, FALSE);
		    proceeded = TRUE;
		    break;
	    default:
		    break;
	}
	if (proceeded)
		g_signal_emit(manager, signals[STATUS_CHANGED], 0,
			      filename, NULL);

  end:
	g_free(filename);
}

static void
imsettings_info_manager_real_changed_dot_xinputrc(GFileMonitor      *monitor,
						  GFile             *file,
						  GFile             *other_file,
						  GFileMonitorEvent  event_type,
						  gpointer           user_data)
{
	IMSettingsInfoManager *manager = IMSETTINGS_INFO_MANAGER (user_data);
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (manager);
	gchar *filename = g_file_get_path(file);
	gboolean proceeded = FALSE;
	IMSettingsInfo *old_info, *info;

	switch (event_type) {
	    case G_FILE_MONITOR_EVENT_CHANGED:
		    imsettings_info_manager_remove_info(priv, filename, TRUE);
	    case G_FILE_MONITOR_EVENT_CREATED:
		    if (!imsettings_info_manager_validate_from_file(file)) {
			    /* ignore them */
			    goto end;
		    }
		    info = imsettings_info_manager_add_info(priv, filename, TRUE);

		    G_LOCK (imsettings_info_manager);

		    if (priv->current_user_im &&
			(old_info = g_hash_table_lookup(priv->im_info_from_name,
							priv->current_user_im))) {
			    g_object_set(G_OBJECT (old_info),
					 "is_user_default",
					 FALSE, NULL);
		    }
		    g_object_set(G_OBJECT (info), "is_user_default", TRUE, NULL);
		    g_free(priv->current_user_im);
		    priv->current_user_im = g_strdup(imsettings_info_get_short_desc(info));
		    g_hash_table_replace(priv->im_info_from_filename,
					 g_strdup(filename),
					 g_object_ref(info));

		    G_UNLOCK (imsettings_info_manager);

		    proceeded = TRUE;
		    break;
	    case G_FILE_MONITOR_EVENT_DELETED:
		    imsettings_info_manager_remove_info(priv, filename, TRUE);
		    proceeded = TRUE;
		    break;
	    default:
		    break;
	}
	if (proceeded)
		g_signal_emit(manager, signals[STATUS_CHANGED], 0,
			      filename, NULL);

  end:
	g_free(filename);
}

static void
imsettings_info_manager_real_changed_xinputrc(GFileMonitor      *monitor,
					      GFile             *file,
					      GFile             *other_file,
					      GFileMonitorEvent  event_type,
					      gpointer           user_data)
{
	IMSettingsInfoManager *manager = IMSETTINGS_INFO_MANAGER (user_data);
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (manager);
	gchar *filename = g_file_get_path(file);
	gboolean proceeded = FALSE;
	IMSettingsInfo *old_info, *info;

	switch (event_type) {
	    case G_FILE_MONITOR_EVENT_CHANGED:
		    imsettings_info_manager_remove_info(priv, filename, TRUE);
	    case G_FILE_MONITOR_EVENT_CREATED:
		    if (!imsettings_info_manager_validate_from_file(file)) {
			    /* ignore them */
			    goto end;
		    }
		    info = imsettings_info_manager_add_info(priv, filename, TRUE);

		    G_LOCK (imsettings_info_manager);

		    if (priv->current_system_im &&
			(old_info = g_hash_table_lookup(priv->im_info_from_name,
							priv->current_system_im))) {
			    g_object_set(G_OBJECT (old_info),
					 "is_system_default",
					 FALSE, NULL);
		    }
		    g_object_set(G_OBJECT (info), "is_system_default", TRUE, NULL);
		    g_free(priv->current_system_im);
		    priv->current_system_im = g_strdup(imsettings_info_get_short_desc(info));
		    g_hash_table_replace(priv->im_info_from_filename,
					 g_strdup(filename),
					 g_object_ref(info));

		    G_UNLOCK (imsettings_info_manager);

		    proceeded = TRUE;
		    break;
	    case G_FILE_MONITOR_EVENT_DELETED:
		    imsettings_info_manager_remove_info(priv, filename, FALSE);
		    proceeded = TRUE;
		    break;
	    default:
		    break;
	}
	if (proceeded)
		g_signal_emit(manager, signals[STATUS_CHANGED], 0,
			      filename, NULL);

  end:
	g_free(filename);
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

typedef struct _IMSettingsManagerCollectObjects {
	GQueue         *q;
	const gchar    *lang;
	IMSettingsInfo *legacy_im;
} IMSettingsManagerCollectObjects;

static void
_collect_info_objects(gpointer key,
		      gpointer val,
		      gpointer data)
{
	IMSettingsManagerCollectObjects *v = data;
	IMSettingsInfo *info = IMSETTINGS_INFO (val);

	if (!imsettings_info_is_xim(info)) {
		if (imsettings_info_is_visible(info)) {
			if (imsettings_info_is_system_default(info))
				g_queue_push_head(v->q, (gpointer)info);
			else
				g_queue_push_tail(v->q, (gpointer)info);
		}
	} else {
		/* need to update the short description with the lang */
		g_object_set(G_OBJECT (info), "language", v->lang, NULL);
		if (imsettings_info_is_visible(info)) {
			if (v->legacy_im)
				g_return_if_reached();
			v->legacy_im = info;
		}
	}
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

static GPtrArray *
imsettings_info_manager_real_get_info_objects(IMSettingsObserver  *observer,
					      const gchar         *locale,
					      GError             **error)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (observer);
	IMSettingsManagerCollectObjects v;
	GPtrArray *array = g_ptr_array_new();
	gpointer p;

	v.q = g_queue_new();
	v.lang = locale;
	v.legacy_im = NULL;
	g_queue_init(v.q);

	G_LOCK (imsettings_info_manager);

	g_hash_table_foreach(priv->im_info_from_name, _collect_info_objects, &v);

	G_UNLOCK (imsettings_info_manager);

	while ((p = g_queue_pop_head(v.q)))
		g_ptr_array_add(array, p);
	if (v.legacy_im)
		g_ptr_array_add(array, v.legacy_im);
	g_queue_free(v.q);
	if (array->len == 0) {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_NOT_AVAILABLE,
			    _("No input methods available on your system."));
	}

	return array;
}

static const gchar *
imsettings_info_manager_real_get_current_user_im(IMSettingsObserver  *observer,
						 GError             **error)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (observer);

	return (priv->current_user_im ? priv->current_user_im : priv->current_system_im);
}

static const gchar *
imsettings_info_manager_real_get_current_system_im(IMSettingsObserver  *observer,
						   GError             **error)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (observer);

	return priv->current_system_im;
}

static IMSettingsInfo *
imsettings_info_manager_real_get_info(IMSettingsObserver  *observer,
				      const gchar         *module,
				      GError             **error)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (observer);
	IMSettingsInfo *info;

	info = g_hash_table_lookup(priv->im_info_from_name, module);
	if (info == NULL) {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_IM_NOT_FOUND,
			    _("No such input method on your system: %s"), module);
	}

	return info;
}

static guint
imsettings_info_manager_real_get_version(IMSettingsObserver  *observer,
					 GError             **error)
{
	return IMSETTINGS_IMINFO_DAEMON_VERSION;
}

static gboolean
imsettings_info_manager_init_monitor(IMSettingsInfoManager *manager)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (manager);
	gchar *file = NULL, *path, *p;
	const gchar *filename;
	GError *error = NULL;
	GFile *f;
	GFileEnumerator *e;
	GFileInfo *i;
	IMSettingsInfo *info, *old_info;

	g_hash_table_remove_all(priv->im_info_from_filename);
	g_hash_table_remove_all(priv->im_info_from_name);
	g_free(priv->current_user_im);
	g_free(priv->current_system_im);
	priv->current_user_im = NULL;
	priv->current_system_im = NULL;

	/* for all xinput configurations */
	f = g_file_new_for_path(priv->xinputdir);
	path = g_file_get_path(f);
	e = g_file_enumerate_children(f, "standard::*",
				      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				      NULL, NULL);
	if (e) {
		while (1) {
			i = g_file_enumerator_next_file(e, NULL, &error);
			if (i == NULL && error == NULL) {
				/* the end of data */
				break;
			} else if (error) {
				g_warning("Unable to get the file information. ignoring");
				continue;
			} else if (!imsettings_info_manager_validate_from_file_info(i)) {
				/* just ignore them */
				continue;
			}
			filename = g_file_info_get_name(i);
			p = g_build_filename(path, filename, NULL);
			imsettings_info_manager_add_info(priv, p, FALSE);
			g_free(p);
			g_object_unref(i);
		}
		g_object_unref(e);
	}
	priv->mon_xinputd = g_file_monitor_directory(f,
						     G_FILE_MONITOR_NONE,
						     NULL,
						     &error);
	if (error) {
		g_warning("Unable to monitor %s: %s",
			  priv->xinputdir, error->message);
		g_clear_error(&error);
	} else {
		g_signal_connect(priv->mon_xinputd, "changed",
				 G_CALLBACK (imsettings_info_manager_real_changed_xinputd),
				 manager);
	}
	g_free(path);
	g_object_unref(f);
	f = NULL;

	/* for user specific xinputrc */
	if (priv->homedir != NULL) {
		file = g_build_filename(priv->homedir, IMSETTINGS_USER_XINPUT_CONF, NULL);
		f = g_file_new_for_path(file);
		if (g_file_test(file, G_FILE_TEST_EXISTS) &&
		    imsettings_info_manager_validate_from_file(f)) {
			info = imsettings_info_manager_add_info(priv, file, TRUE);

			G_LOCK (imsettings_info_manager);

			if (priv->current_user_im &&
			    (old_info = g_hash_table_lookup(priv->im_info_from_name,
							    priv->current_user_im)))
				g_object_set(G_OBJECT (old_info),
					     "is_user_default",
					     FALSE, NULL);
			g_object_set(G_OBJECT (info), "is_user_default", TRUE, NULL);
			g_free(priv->current_user_im);
			priv->current_user_im = g_strdup(imsettings_info_get_short_desc(info));

			G_UNLOCK (imsettings_info_manager);
		}
	}
	priv->mon_dot_xinputrc = g_file_monitor_file(f,
						     G_FILE_MONITOR_NONE,
						     NULL,
						     &error);
	if (error) {
		g_warning("Unable to monitor %s: %s",
			  file, error->message);
		g_clear_error(&error);
	} else {
		g_signal_connect(priv->mon_dot_xinputrc, "changed",
				 G_CALLBACK (imsettings_info_manager_real_changed_dot_xinputrc),
				 manager);
	}
	g_free(file);
	g_object_unref(f);

	/* for system-wide xinputrc */
	file = g_build_filename(priv->xinputrcdir, IMSETTINGS_GLOBAL_XINPUT_CONF, NULL);
	f = g_file_new_for_path(file);
	if (g_file_test(file, G_FILE_TEST_EXISTS) &&
	    imsettings_info_manager_validate_from_file(f)) {
		info = imsettings_info_manager_add_info(priv, file, TRUE);

		G_LOCK (imsettings_info_manager);

		if (priv->current_system_im &&
		    (old_info = g_hash_table_lookup(priv->im_info_from_name,
						    priv->current_system_im)))
			g_object_set(G_OBJECT (old_info),
				     "is_system_default",
				     FALSE, NULL);
		g_object_set(G_OBJECT (info), "is_system_default", TRUE, NULL);
		g_free(priv->current_system_im);
		priv->current_system_im = g_strdup(imsettings_info_get_short_desc(info));

		G_UNLOCK (imsettings_info_manager);
	}
	priv->mon_xinputrc = g_file_monitor_file(f,
						 G_FILE_MONITOR_NONE,
						 NULL,
						 &error);
	if (error) {
		g_warning("Unable to monitor %s: %s",
			  file, error->message);
		g_clear_error(&error);
	} else {
		g_signal_connect(priv->mon_xinputrc, "changed",
				 G_CALLBACK (imsettings_info_manager_real_changed_xinputrc),
				 manager);
	}
	g_free(file);
	g_object_unref(f);

	return TRUE;
}

static void
imsettings_info_manager_tini_monitor(IMSettingsInfoManager *manager)
{
	IMSettingsInfoManagerPrivate *priv = IMSETTINGS_INFO_MANAGER_GET_PRIVATE (manager);

	g_signal_handlers_disconnect_by_func(priv->mon_xinputd,
					     G_CALLBACK (imsettings_info_manager_real_changed_xinputd),
					     manager);
	g_signal_handlers_disconnect_by_func(priv->mon_dot_xinputrc,
					     G_CALLBACK (imsettings_info_manager_real_changed_dot_xinputrc),
					     manager);
	g_signal_handlers_disconnect_by_func(priv->mon_xinputrc,
					     G_CALLBACK (imsettings_info_manager_real_changed_xinputrc),
					     manager);
	g_object_unref(priv->mon_xinputd);
	g_object_unref(priv->mon_dot_xinputrc);
	g_object_unref(priv->mon_xinputrc);
}

static void
imsettings_info_manager_class_init(IMSettingsInfoManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	IMSettingsObserverClass *observer_class = IMSETTINGS_OBSERVER_CLASS (klass);

	g_type_class_add_private(klass, sizeof (IMSettingsInfoManagerPrivate));

	object_class->set_property = imsettings_info_manager_real_set_property;
	object_class->get_property = imsettings_info_manager_real_get_property;
	object_class->finalize     = imsettings_info_manager_real_finalize;

	observer_class->get_list = imsettings_info_manager_real_get_list;
	observer_class->get_info_objects = imsettings_info_manager_real_get_info_objects;
	observer_class->get_current_user_im = imsettings_info_manager_real_get_current_user_im;
	observer_class->get_current_system_im = imsettings_info_manager_real_get_current_system_im;
	observer_class->get_info = imsettings_info_manager_real_get_info;
	observer_class->get_version = imsettings_info_manager_real_get_version;

	/* properties */
	g_object_class_install_property(object_class, PROP_XINPUTRCDIR,
					g_param_spec_string("xinputrcdir",
							    _("xinputrc directory"),
							    _("A directory where puts the system wide xinputrc on."),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_XINPUTDIR,
					g_param_spec_string("xinputdir",
							    _("xinput directory"),
							    _("A directory where puts the IM configurations on."),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_HOMEDIR,
					g_param_spec_string("homedir",
							    _("home directory"),
							    _("home directory"),
							    NULL,
							    G_PARAM_READWRITE));
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
	const gchar *p;

	priv->req_conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	priv->im_info_from_name = g_hash_table_new_full(g_str_hash,
							g_str_equal,
							g_free,
							g_object_unref);
	priv->im_info_from_filename = g_hash_table_new_full(g_str_hash,
							    g_str_equal,
							    g_free,
							    g_object_unref);
	priv->xinputrcdir = g_strdup(XINPUTRC_PATH);
	priv->xinputdir = g_strdup(XINPUT_PATH);
	p = g_get_home_dir();
	if (p)
		priv->homedir = g_strdup(p);
	else
		priv->homedir = NULL;

	imsettings_info_manager_init_monitor(manager);
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
	gchar *arg_xinputrcdir = NULL, *arg_xinputdir = NULL, *arg_homedir = NULL;
	GOptionContext *ctx = g_option_context_new(NULL);
	GOptionEntry entries[] = {
		{"replace", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_replace, N_("Replace the running settings daemon with new instance."), NULL},
		{"xinputrcdir", 0, G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &arg_xinputrcdir, N_("A directory where puts the system-wide xinputrc puts on (debugging only)"), N_("DIR")},
		{"xinputdir", 0, G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &arg_xinputdir, N_("A directory where puts the IM configurations puts on (debugging only)"), N_("DIR")},
		{"homedir", 0, G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &arg_homedir, N_("A home directory (debugging only)"), N_("DIR")},
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
	/* XXX: ugly hack to reduce loading time */
	if (arg_xinputrcdir)
		g_object_set(manager, "xinputrcdir", arg_xinputrcdir, NULL);
	if (arg_xinputdir)
		g_object_set(manager, "xinputdir", arg_xinputdir, NULL);
	if (arg_homedir)
		g_object_set(manager, "homedir", arg_homedir, NULL);
	if (arg_xinputrcdir ||
	    arg_xinputdir ||
	    arg_homedir) {
		imsettings_info_manager_tini_monitor(manager);
		imsettings_info_manager_init_monitor(manager);
	}

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

	g_main_loop_unref(loop);
	g_object_unref(manager);
	dbus_g_connection_unref(gconn);

	return 0;
}
