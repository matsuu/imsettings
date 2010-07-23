/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * monitor.c
 * Copyright (C) 2008-2009 Red Hat, Inc. All rights reserved.
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
#include <glib/gi18n.h>
#include <glib/gthread.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-info-private.h"
#include "imsettings/imsettings-marshal.h"
#include "monitor.h"


#define MONITOR_DEPTH_MAX	10

typedef struct _IMSettingsMonitorFile		IMSettingsMonitorFile;
typedef struct _IMSettingsMonitorCollectObjects	IMSettingsMonitorCollectObjects;


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

struct _IMSettingsMonitorFile {
	GFileMonitor *monitor;
	GFile        *file;
	gchar        *filename;
	gboolean      is_symlink;
	gint          ref_count;
};
struct _IMSettingsMonitorCollectObjects {
	GQueue         *q;
	const gchar    *lang;
	IMSettingsInfo *legacy_im;
};


static gboolean               _imsettings_monitor_start_for_xinputrc    (IMSettingsMonitor     *monitor,
                                                                         GFile                 *file,
                                                                         guint                  depth,
                                                                         gboolean               is_system_default,
                                                                         GError                **error);
static gboolean               imsettings_monitor_validate_from_file_info(GFileInfo             *info,
                                                                         gboolean               check_rc);
static gboolean               imsettings_monitor_validate_from_file     (GFile                 *file,
                                                                         gboolean               check_rc);
static IMSettingsInfo        *imsettings_monitor_add_file               (IMSettingsMonitor     *monitor,
                                                                         const gchar           *filename,
                                                                         gboolean               force);
static gboolean               imsettings_monitor_remove_file            (IMSettingsMonitor     *monitor,
                                                                         const gchar           *filename,
                                                                         gboolean               is_xinputrc);
static IMSettingsMonitorFile *imsettings_monitor_file_new               (GFile                 *file,
                                                                         GCancellable          *cancellable,
                                                                         GError                **error);
static IMSettingsMonitorFile *imsettings_monitor_file_ref               (IMSettingsMonitorFile *file);
static void                   imsettings_monitor_file_unref             (gpointer               p);


guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (IMSettingsMonitor, imsettings_monitor, G_TYPE_OBJECT);


/*
 * Private functions
 */
static void
imsettings_monitor_real_set_property(GObject      *object,
				     guint         prop_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
	IMSettingsMonitor *mon = IMSETTINGS_MONITOR (object);
	const gchar *p;
	GFile *f;

	switch (prop_id) {
	    case PROP_XINPUTRCDIR:
		    p = g_value_get_string(value);
		    if (p == NULL ||
			!g_file_test(p, G_FILE_TEST_IS_DIR)) {
			    g_warning("Given value through 'xinputrcdir' property is invalid: %s",
				      p);
		    } else {
			    d(g_print("*** changing xinputrc dir from %s to %s\n",
				      mon->xinputrcdir, p));
			    g_free(mon->xinputrcdir);
			    f = g_file_new_for_path(p);
			    mon->xinputrcdir = g_file_get_path(f);
			    g_object_unref(f);
		    }
		    break;
	    case PROP_XINPUTDIR:
		    p = g_value_get_string(value);
		    if (p == NULL ||
			!g_file_test(p, G_FILE_TEST_IS_DIR)) {
			    g_warning("Given value through 'xinputdir' property is invalid: %s",
				      p);
		    } else {
			    d(g_print("*** changing xinput dir from %s to %s\n",
				      mon->xinputdir, p));
			    g_free(mon->xinputdir);
			    f = g_file_new_for_path(p);
			    mon->xinputdir = g_file_get_path(f);
			    g_object_unref(f);
		    }
		    break;
	    case PROP_HOMEDIR:
		    p = g_value_get_string(value);
		    if (p == NULL ||
			!g_file_test(p, G_FILE_TEST_IS_DIR)) {
			    g_warning("Given value through 'homedir' property is invalid: %s",
				      p);
		    } else {
			    d(g_print("*** changing home dir from %s to %s\n",
				      mon->homedir, p));
			    g_free(mon->homedir);
			    f = g_file_new_for_path(p);
			    mon->homedir = g_file_get_path(f);
			    g_object_unref(f);
		    }
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_monitor_real_get_property(GObject    *object,
				     guint       prop_id,
				     GValue     *value,
				     GParamSpec *pspec)
{
	IMSettingsMonitor *mon = IMSETTINGS_MONITOR (object);

	switch (prop_id) {
	    case PROP_XINPUTRCDIR:
		    g_value_set_string(value, mon->xinputrcdir);
		    break;
	    case PROP_XINPUTDIR:
		    g_value_set_string(value, mon->xinputdir);
		    break;
	    case PROP_HOMEDIR:
		    g_value_set_string(value, mon->homedir);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_monitor_real_finalize(GObject *object)
{
	IMSettingsMonitor *mon = IMSETTINGS_MONITOR (object);

	imsettings_monitor_stop(mon);

	if (mon->im_info_from_name)
		g_hash_table_destroy(mon->im_info_from_name);
	if (mon->im_info_from_filename)
		g_hash_table_destroy(mon->im_info_from_filename);
	if (mon->file_list_for_xinputrc)
		g_hash_table_destroy(mon->file_list_for_xinputrc);
	if (mon->file_list_for_dot_xinputrc)
		g_hash_table_destroy(mon->file_list_for_dot_xinputrc);

	if (mon->mon_xinputrc)
		g_ptr_array_free(mon->mon_xinputrc, TRUE);
	if (mon->mon_dot_xinputrc)
		g_ptr_array_free(mon->mon_dot_xinputrc, TRUE);
	g_free(mon->xinputrcdir);
	g_free(mon->xinputdir);
	g_free(mon->homedir);
	g_free(mon->current_system_im);
	g_free(mon->current_user_im);

	if (G_OBJECT_CLASS (imsettings_monitor_parent_class)->finalize)
		(* G_OBJECT_CLASS (imsettings_monitor_parent_class)->finalize) (object);
}

static void
imsettings_monitor_class_init(IMSettingsMonitorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = imsettings_monitor_real_set_property;
	object_class->get_property = imsettings_monitor_real_get_property;
	object_class->finalize     = imsettings_monitor_real_finalize;

	/* properties */
	g_object_class_install_property(object_class, PROP_XINPUTRCDIR,
					g_param_spec_string("xinputrcdir",
							    _("xinputrc directory"),
							    _("A place to store xinputrc file"),
							    XINPUTRC_PATH,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_XINPUTDIR,
					g_param_spec_string("xinputdir",
							    _("xinput directory"),
							    _("A place to store xinput configuration files"),
							    XINPUT_PATH,
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
					       G_STRUCT_OFFSET (IMSettingsMonitorClass, status_changed),
					       NULL, NULL,
					       imsettings_marshal_VOID__STRING_INT,
					       G_TYPE_NONE, 2,
					       G_TYPE_STRING, G_TYPE_INT);
}

static void
imsettings_monitor_init(IMSettingsMonitor *monitor)
{
	monitor->im_info_from_name = g_hash_table_new_full(g_str_hash,
							   g_str_equal,
							   g_free,
							   g_object_unref);
	monitor->im_info_from_filename = g_hash_table_new_full(g_str_hash,
							       g_str_equal,
							       g_free,
							       g_object_unref);
	monitor->file_list_for_xinputrc = g_hash_table_new_full(g_str_hash,
								g_str_equal,
								g_free,
								imsettings_monitor_file_unref);
	monitor->file_list_for_dot_xinputrc = g_hash_table_new_full(g_str_hash,
								    g_str_equal,
								    g_free,
								    imsettings_monitor_file_unref);
	monitor->xinputrcdir = g_strdup(XINPUTRC_PATH);
	monitor->xinputdir = g_strdup(XINPUT_PATH);
	monitor->homedir = g_strdup(g_get_home_dir());
	monitor->mon_dot_xinputrc = g_ptr_array_sized_new(MONITOR_DEPTH_MAX);
	monitor->mon_xinputrc = g_ptr_array_sized_new(MONITOR_DEPTH_MAX);
}

static void
imsettings_monitor_real_changed_xinputd(GFileMonitor      *fmon,
					GFile             *file,
					GFile             *other_file,
					GFileMonitorEvent  event_type,
					gpointer           user_data)
{
	IMSettingsMonitor *monitor = IMSETTINGS_MONITOR (user_data);
	gchar *filename = g_file_get_path(file);
	gsize len = strlen(filename);
	gsize suffix_len = strlen(XINPUT_SUFFIX);
	gboolean proceeded = FALSE;
	IMSettingsMonitorFile *m;

	switch (event_type) {
	    case G_FILE_MONITOR_EVENT_CHANGED:
		    if (len <= suffix_len ||
			strcmp(&filename[len - suffix_len], XINPUT_SUFFIX) != 0) {
			    /* just ignore this */
			    d(g_print("Ignoring the change of `%s'.\n", filename));
			    break;
		    }
		    d(g_print("*** %s changed\n", filename));
		    imsettings_monitor_remove_file(monitor, filename, FALSE);
	    case G_FILE_MONITOR_EVENT_CREATED:
		    if (len <= suffix_len ||
			strcmp(&filename[len - suffix_len], XINPUT_SUFFIX) != 0) {
			    /* just ignore this */
			    d(g_print("Ignoring the change of `%s'.\n", filename));
			    break;
		    }
		    d(g_print("*** %s created/changed\n", filename));
		    if (!imsettings_monitor_validate_from_file(file, FALSE)) {
			    /* ignore them */
			    goto end;
		    }
		    if (g_hash_table_lookup(monitor->im_info_from_filename, filename) == NULL)
			    imsettings_monitor_add_file(monitor, filename, FALSE);
		    proceeded = TRUE;
		    break;
	    case G_FILE_MONITOR_EVENT_DELETED:
		    if (len <= suffix_len ||
			strcmp(&filename[len - suffix_len], XINPUT_SUFFIX) != 0) {
			    /* just ignore this */
			    d(g_print("Ignoring the change of `%s'.\n", filename));
			    break;
		    }
		    d(g_print("*** %s deleted\n", filename));
		    imsettings_monitor_remove_file(monitor, filename, FALSE);
		    proceeded = TRUE;
		    break;
	    default:
		    break;
	}
	if (proceeded)
		g_signal_emit(monitor, signals[STATUS_CHANGED], 0,
			      filename, NULL);
	if ((m = g_hash_table_lookup(monitor->file_list_for_xinputrc, filename))) {
		g_file_monitor_emit_event(m->monitor,
					  m->file,
					  other_file,
					  event_type);
	}
	if ((m = g_hash_table_lookup(monitor->file_list_for_dot_xinputrc, filename))) {
		g_file_monitor_emit_event(m->monitor,
					  m->file,
					  other_file,
					  event_type);
	}

  end:
	g_free(filename);
}

static void
imsettings_monitor_real_changed_xinputrc(GFileMonitor      *fmon,
					 GFile             *file,
					 GFile             *other_file,
					 GFileMonitorEvent  event_type,
					 gpointer           user_data)
{
	IMSettingsMonitor *monitor = IMSETTINGS_MONITOR (user_data);
	gchar *filename = g_file_get_path(file);
	gboolean proceeded = FALSE;
	IMSettingsMonitorFile *mon = NULL, *m;
	gint i, depth;
	GError *error = NULL;
	GHashTableIter iter;
	gpointer key, val;

	for (i = 0; i < monitor->mon_xinputrc->len; i++) {
		m = g_ptr_array_index(monitor->mon_xinputrc, i);
		if (strcmp(m->filename, filename) == 0) {
			mon = m;
			break;
		} else {
			gchar *name;

			name = g_file_get_path(m->file);
			if (strcmp(name, filename) == 0) {
				mon = m;
				g_free(name);
				break;
			}
			g_free(name);
		}
	}
	if (mon == NULL) {
		/* if no targeted file found in current monitored files,
		 * just assume that it may be the top of the file like xinputrc.
		 */
		i = depth = 0;
	} else {
		depth = i;
	}

	switch (event_type) {
	    case G_FILE_MONITOR_EVENT_CHANGED:
	    case G_FILE_MONITOR_EVENT_CREATED:
		    if (!imsettings_monitor_validate_from_file(file, TRUE)) {
			    /* ignore them */
			    goto end;
		    }
		    d(g_print("*** %d:%s(xinputrc) created/changed\n", depth, filename));
		    for (; i < monitor->mon_xinputrc->len; i++) {
			    imsettings_monitor_file_unref(g_ptr_array_index(monitor->mon_xinputrc, i));
		    }
		    monitor->mon_xinputrc->len = depth;
		    g_object_ref(file);
		    _imsettings_monitor_start_for_xinputrc(monitor, file, depth, TRUE, &error);
		    if (error) {
			    g_warning("Unable to update the monitor for %s(xinputrc): %s",
				      filename, error->message);
			    g_error_free(error);
			    break;
		    }
		    proceeded = TRUE;
		    break;
	    case G_FILE_MONITOR_EVENT_DELETED:
		    g_hash_table_iter_init(&iter, monitor->im_info_from_filename);
		    while (g_hash_table_iter_next(&iter, &key, &val)) {
			    IMSettingsInfo *info = IMSETTINGS_INFO (val);

			    g_object_set(G_OBJECT (info), "is_system_default", FALSE, NULL);
		    }
		    g_free(monitor->current_system_im);
		    monitor->current_system_im = NULL;
		    d(g_print("*** %s(xinputrc) deleted\n", filename));
		    proceeded = TRUE;
		    break;
	    default:
		    break;
	}
	if (proceeded)
		g_signal_emit(monitor, signals[STATUS_CHANGED], 0,
			      filename, NULL);
  end:
	g_free(filename);
}

static void
imsettings_monitor_real_changed_dot_xinputrc(GFileMonitor      *fmon,
					     GFile             *file,
					     GFile             *other_file,
					     GFileMonitorEvent  event_type,
					     gpointer           user_data)
{
	IMSettingsMonitor *monitor = IMSETTINGS_MONITOR (user_data);
	gchar *filename = g_file_get_path(file), *tmp;
	gboolean proceeded = FALSE;
	IMSettingsMonitorFile *mon = NULL, *m;
	gint i, depth;
	GError *error = NULL;

	for (i = 0; i < monitor->mon_dot_xinputrc->len; i++) {
		m = g_ptr_array_index(monitor->mon_dot_xinputrc, i);
		if (strcmp(m->filename, filename) == 0) {
			mon = m;
			break;
		} else {
			gchar *name;

			name = g_file_get_path(m->file);
			if (strcmp(name, filename) == 0) {
				mon = m;
				g_free(name);
				break;
			}
			g_free(name);
		}
	}
	depth = i;
	g_return_if_fail (mon != NULL);

	tmp = g_build_filename(monitor->homedir, IMSETTINGS_USER_XINPUT_CONF, NULL);
	switch (event_type) {
	    case G_FILE_MONITOR_EVENT_CHANGED:
		    d(g_print("*** %s(.xinputrc) changed\n", filename));
		    if (strcmp(filename, tmp) == 0)
			    imsettings_monitor_remove_file(monitor, filename, TRUE);
	    case G_FILE_MONITOR_EVENT_CREATED:
		    if (!imsettings_monitor_validate_from_file(file, TRUE)) {
			    /* ignore them */
			    goto end;
		    }
		    d(g_print("*** %s(.xinputrc) created/changed\n", filename));
		    for (; i < monitor->mon_dot_xinputrc->len; i++) {
			    imsettings_monitor_file_unref(g_ptr_array_index(monitor->mon_dot_xinputrc, i));
		    }
		    monitor->mon_dot_xinputrc->len = depth;
		    g_object_ref(file);
		    _imsettings_monitor_start_for_xinputrc(monitor, file, depth, FALSE, &error);
		    if (error) {
			    g_warning("Unable to update the monitor for %s(.xinputrc): %s",
				      filename, error->message);
			    break;
		    }
		    proceeded = TRUE;
		    break;
	    case G_FILE_MONITOR_EVENT_DELETED:
		    d(g_print("*** %s(.xinputrc) deleted\n", filename));
		    if (strcmp(filename, tmp) == 0)
			    imsettings_monitor_remove_file(monitor, filename, TRUE);
		    proceeded = TRUE;
		    break;
	    default:
		    break;
	}
	if (proceeded)
		g_signal_emit(monitor, signals[STATUS_CHANGED], 0,
			      filename, NULL);

  end:
	g_free(tmp);
	g_free(filename);
}

static void
_imsettings_monitor_start_for_xinputdir(IMSettingsMonitor *monitor)
{
	GFile *file;
	gchar *path, *p;
	const gchar *filename;
	GFileEnumerator *e;
	GFileInfo *info;
	GError *error = NULL;

	g_return_if_fail (monitor->xinputdir);
	g_return_if_fail (monitor->mon_xinputd == NULL);

	file = g_file_new_for_path(monitor->xinputdir);
	path = g_file_get_path(file);
	e = g_file_enumerate_children(file, "standard::*",
				      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				      NULL, NULL);

	if (e) {
		while (1) {
			info = g_file_enumerator_next_file(e, NULL, &error);
			if (info == NULL && error == NULL) {
				/* the end of data */
				break;
			} else if (error) {
				g_warning("Unable to get the file information: %s",
					  error->message);
				g_clear_error(&error);
				g_object_unref(info);
				continue;
			} else if (!imsettings_monitor_validate_from_file_info(info, FALSE)) {
				/* just ignore them */
				g_object_unref(info);
				continue;
			}
			filename = g_file_info_get_name(info);
			p = g_build_filename(path, filename, NULL);
			imsettings_monitor_add_file(monitor, p, FALSE);
			g_free(p);
			g_object_unref(info);
		}
		g_object_unref(e);
	}
	monitor->mon_xinputd = g_file_monitor_directory(file,
							G_FILE_MONITOR_NONE,
							NULL,
							&error);
	if (error) {
		g_warning("Unable to start monitoring %s: %s",
			  monitor->xinputdir, error->message);
		g_clear_error(&error);
	} else {
		g_signal_connect(monitor->mon_xinputd, "changed",
				 G_CALLBACK (imsettings_monitor_real_changed_xinputd),
				 monitor);
	}

	g_free(path);
	g_object_unref(file);
}

static gboolean
_imsettings_monitor_start_for_xinputrc(IMSettingsMonitor  *monitor,
				       GFile              *file,
				       guint               depth,
				       gboolean            is_system_default,
				       GError            **error)
{
	GError *err = NULL;
	GFileInfo *finfo;
	GPtrArray *array;
	gchar *name = NULL;
	const gchar *target;
	gboolean retval = TRUE;
	IMSettingsMonitorFile *mon;
	IMSettingsInfo *info = NULL;

	finfo = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK "," \
				  G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL, &err);
	if (g_file_query_exists(file, NULL)) {
		if (is_system_default)
			array = monitor->mon_xinputrc;
		else
			array = monitor->mon_dot_xinputrc;

		name = g_file_get_path(file);
		if (g_file_info_get_is_symlink(finfo)) {
			/* to not lose the changes of the target of symlink for xinputrc,
			 * Add a special file monitor to keep it on track.
			 */
			target = g_file_info_get_symlink_target(finfo);
			if (depth > MONITOR_DEPTH_MAX) {
				g_set_error(&err, IMSETTINGS_MONITOR_ERROR,
					    IMSETTINGS_MONITOR_ERROR_DANGLING_SYMLINK,
					    "%s might be a dangling symlink.",
					    name);
				retval = FALSE;
			} else {
				GFile *f, *tmp;

				d(g_print("*** watching the change of %s\n", name));
				mon = imsettings_monitor_file_new(file, NULL, &err);
				if (err)
					goto end;

				if (is_system_default)
					g_signal_connect(mon->monitor, "changed",
							 G_CALLBACK (imsettings_monitor_real_changed_xinputrc),
							 monitor);
				else
					g_signal_connect(mon->monitor, "changed",
							 G_CALLBACK (imsettings_monitor_real_changed_dot_xinputrc),
							 monitor);
				array->len = depth + 1;
				g_ptr_array_index(array, depth) = imsettings_monitor_file_ref(mon);
				if (is_system_default)
					g_hash_table_insert(monitor->file_list_for_xinputrc,
							    g_strdup(name),
							    mon);
				else
					g_hash_table_insert(monitor->file_list_for_dot_xinputrc,
							    g_strdup(name),
							    mon);

				tmp = g_file_get_parent(file);
				f = g_file_resolve_relative_path(tmp, target);

				g_object_unref(tmp);
				g_object_unref(finfo);
				g_object_unref(file);
				g_free(name);

				return _imsettings_monitor_start_for_xinputrc(monitor,
									      f,
									      depth + 1,
									      is_system_default,
									      error);
			}
		} else {
			if (depth > 0)
				info = g_hash_table_lookup(monitor->im_info_from_filename, name);
			if (!info)
				info = imsettings_monitor_add_file(monitor, name, TRUE);

			if (is_system_default) {
				g_object_set(G_OBJECT (info), "is_system_default", TRUE, NULL);
				g_free(monitor->current_system_im);
				monitor->current_system_im = g_strdup(imsettings_info_get_short_desc(info));
			} else {
				g_object_set(G_OBJECT (info), "is_user_default", TRUE, NULL);
				g_free(monitor->current_user_im);
				monitor->current_user_im = g_strdup(imsettings_info_get_short_desc(info));
			}

			d(g_print("*** watching the change of %s\n", name));
			mon = imsettings_monitor_file_new(file, NULL, &err);
			if (err)
				goto end;

			if (is_system_default)
				g_signal_connect(mon->monitor, "changed",
						 G_CALLBACK (imsettings_monitor_real_changed_xinputrc),
						 monitor);
			else
				g_signal_connect(mon->monitor, "changed",
						 G_CALLBACK (imsettings_monitor_real_changed_dot_xinputrc),
						 monitor);
			array->len = depth + 1;
			g_ptr_array_index(array, depth) = imsettings_monitor_file_ref(mon);
			if (is_system_default)
				g_hash_table_insert(monitor->file_list_for_xinputrc,
						    g_strdup(name),
						    mon);
			else
				g_hash_table_insert(monitor->file_list_for_dot_xinputrc,
						    g_strdup(name),
						    mon);
		}
	} else {
		/* GError is set by g_file_query_info */
		retval = FALSE;
	}
  end:
	g_free(name);
	if (err) {
		if (error) {
			*error = g_error_copy(err);
		} else {
			g_warning("%s", err->message);
		}
		g_error_free(err);
	}
	if (finfo)
		g_object_unref(finfo);
	g_object_unref(file);

	return retval;
}

static void
_imsettings_monitor_start_for_homedir(IMSettingsMonitor *monitor)
{
	gchar *name;
	GFile *file;
	GError *error = NULL;

	g_return_if_fail (monitor->homedir);

	name = g_build_filename(monitor->homedir, IMSETTINGS_USER_XINPUT_CONF, NULL);
	file = g_file_new_for_path(name);
	_imsettings_monitor_start_for_xinputrc(monitor, file, 0, FALSE, &error);
	if (error) {
		g_warning("Unable to monitor %s: %s",
			  name, error->message);
	}
	g_free(name);
}

static void
_imsettings_monitor_start_for_xinputrcdir(IMSettingsMonitor *monitor)
{
	gchar *name;
	GFile *file;
	GError *error = NULL;

	g_return_if_fail (monitor->xinputrcdir);

	name = g_build_filename(monitor->xinputrcdir, IMSETTINGS_GLOBAL_XINPUT_CONF, NULL);
	file = g_file_new_for_path(name);
	_imsettings_monitor_start_for_xinputrc(monitor, file, 0, TRUE, &error);
	if (error) {
		g_warning("Unable to monitor %s: %s",
			  name, error->message);
	}
	g_free(name);
}

static gboolean
imsettings_monitor_validate_from_file_info(GFileInfo *info,
					   gboolean   check_rc)
{
	GFileType type;
	const char *p;
	gchar *n;
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
	n = g_path_get_basename(p);
	if (check_rc &&
	    (strcmp(n, IMSETTINGS_USER_XINPUT_CONF) == 0 ||
	     strcmp(n, IMSETTINGS_GLOBAL_XINPUT_CONF) == 0)) {
		g_free(n);

		return TRUE;
	}
	g_free(n);
	len = strlen(p);
	if (len <= slen ||
	    strcmp(&p[len - slen], XINPUT_SUFFIX) != 0)
		return FALSE;

	return TRUE;
}

static gboolean
imsettings_monitor_validate_from_file(GFile    *file,
				      gboolean  check_rc)
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

	retval = imsettings_monitor_validate_from_file_info(fi, check_rc);
	g_object_unref(fi);

	return retval;
}

static IMSettingsInfo *
imsettings_monitor_add_file(IMSettingsMonitor *monitor,
			    const gchar       *filename,
			    gboolean           force)
{
	IMSettingsInfo *info, *ret;
	const gchar *name;
	gchar *lowername = NULL;

	info = imsettings_info_new(filename);
	if (info != NULL) {
		g_object_set(G_OBJECT (info), "homedir", monitor->homedir, NULL);
		name = imsettings_info_get_short_desc(info);
		if (name == NULL) {
			g_printerr("[BUG] Unable to obtain the short description for `%s'\n",
				   filename);
			g_object_unref(info);

			return NULL;
		}

		if (strcmp(name, IMSETTINGS_NONE_CONF) == 0 &&
		    imsettings_info_is_xim(info)) {
			/* hack to register the info object separately */
			lowername = g_strdup_printf("%s(xim)", name);
		} else {
			lowername = g_ascii_strdown(name, -1);
		}
		if ((ret = g_hash_table_lookup(monitor->im_info_from_name, lowername)) == NULL) {
			d(g_print("Adding %s(%s)...\n", filename, name));
			g_hash_table_insert(monitor->im_info_from_name,
					    g_strdup(lowername),
					    info);
			g_hash_table_insert(monitor->im_info_from_filename,
					    g_strdup(filename),
					    g_object_ref(info));
		} else {
			if (force ||
			    strcmp(name, IMSETTINGS_NONE_CONF) == 0 ||
			    strcmp(name, IMSETTINGS_USER_SPECIFIC_SHORT_DESC) == 0) {
				/* We deal with none.conf specially here.
				 * It has to be registered as is anyway.
				 */
				d(g_print("Adding %s(%s)...\n", filename, name));
				g_hash_table_replace(monitor->im_info_from_name,
						     g_strdup(lowername),
						     info);
				g_hash_table_replace(monitor->im_info_from_filename,
						     g_strdup(filename),
						     g_object_ref(info));
			} else {
				g_warning(_("Duplicate entry `%s' from %s. SHORT_DESC has to be unique."),
					  name, filename);
				g_object_unref(info);
				info = NULL;
			}
		}
		g_free(lowername);
	} else {
		g_warning("Unable to get the file information for `%s'",
			  filename);
	}

	return info;
}

static gboolean
imsettings_monitor_remove_file(IMSettingsMonitor *monitor,
			       const gchar       *filename,
			       gboolean           is_xinputrc)
{
	IMSettingsInfo *info;
	gchar *name = NULL, *lowername = NULL;
	gboolean retval = FALSE;

	if ((info = g_hash_table_lookup(monitor->im_info_from_filename, filename))) {
		name = g_strdup(imsettings_info_get_short_desc(info));
		lowername = g_ascii_strdown(name, -1);
		d(g_print("Removing %s(%s)...\n", filename, name));
		g_hash_table_remove(monitor->im_info_from_filename, filename);
		if (!is_xinputrc ||
		    strcmp(name, IMSETTINGS_USER_SPECIFIC_SHORT_DESC) == 0) {
			d(g_print("Removing %s...\n", name));
			g_hash_table_remove(monitor->im_info_from_name, lowername);
		}
		retval = TRUE;
	}

	g_free(name);
	g_free(lowername);

	return retval;
}

static IMSettingsMonitorFile *
imsettings_monitor_file_new(GFile         *file,
			    GCancellable  *cancellable,
			    GError       **error)
{
	IMSettingsMonitorFile *retval;
	GFileMonitor *monitor;
	const gchar *filename;
	GFileInfo *i;

	g_return_val_if_fail (g_file_query_exists(file, cancellable), NULL);

	i = g_file_query_info(file,
			      G_FILE_ATTRIBUTE_STANDARD_NAME "," \
			      G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK,
			      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
			      cancellable,
			      error);
	filename = g_file_info_get_name(i);
	g_return_val_if_fail (filename != NULL, NULL);

	monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE,
				      cancellable, error);
	g_return_val_if_fail (monitor != NULL, NULL);

	retval = g_new0(IMSettingsMonitorFile, 1);
	retval->monitor = monitor;
	retval->file = g_object_ref(file);
	retval->filename = g_strdup(filename);
	retval->is_symlink = g_file_info_get_is_symlink(i);
	retval->ref_count = 1;

	g_object_unref(i);

	return retval;
}

static IMSettingsMonitorFile *
imsettings_monitor_file_ref(IMSettingsMonitorFile *mon)
{
	g_return_val_if_fail (mon != NULL, NULL);
	g_return_val_if_fail (mon->ref_count > 0, mon);

	g_atomic_int_add(&mon->ref_count, 1);

	return mon;
}

static void
imsettings_monitor_file_unref(gpointer p)
{
	IMSettingsMonitorFile *mon = p;

	g_return_if_fail (mon != NULL);
	g_return_if_fail (mon->ref_count > 0);

	if (g_atomic_int_exchange_and_add(&mon->ref_count, -1) - 1 == 0) {
		g_object_unref(mon->monitor);
		g_object_unref(mon->file);
		g_free(mon->filename);
		g_free(mon);
	}
}

static void
_imsettings_monitor_collect_info_objects(gpointer key,
					 gpointer val,
					 gpointer data)
{
	IMSettingsMonitorCollectObjects *v = data;
	IMSettingsInfo *info = IMSETTINGS_INFO (val);

	if (!imsettings_info_is_xim(info)) {
		if (imsettings_info_is_visible(info)) {
			if (imsettings_info_is_system_default(info))
				g_queue_push_head(v->q, (gpointer)g_object_ref(info));
			else
				g_queue_push_tail(v->q, (gpointer)g_object_ref(info));
		}
	} else {
		/* need to update the short description with the lang */
		g_object_set(G_OBJECT (info),
			     "ignore", FALSE,
			     "language", v->lang,
			     NULL);
		if (imsettings_info_is_visible(info)) {
			if (v->legacy_im)
				g_return_if_reached();
			v->legacy_im = g_object_ref(info);
		}
	}
}

static IMSettingsInfo *
_imsettings_monitor_lookup(GHashTable   *table,
			   const gchar  *module,
			   const gchar  *locale,
			   gboolean      need_lower,
			   GError      **error)
{
	IMSettingsInfo *info;
	gchar *lowername = NULL;

	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (module != NULL, NULL);

	if (need_lower)
		lowername = g_ascii_strdown(module, -1);
	else
		lowername = g_strdup(module);
	info = g_hash_table_lookup(table, lowername);
	if (info == NULL) {
		GHashTableIter iter;
		gpointer key, val;
		const gchar *name;
		gchar *ln;

		g_hash_table_iter_init(&iter, table);
		while (g_hash_table_iter_next(&iter, &key, &val)) {
			info = IMSETTINGS_INFO (val);

			if (imsettings_info_is_xim(info)) {
				/* need to update data with the lang */
				g_object_set(G_OBJECT (info),
					     "ignore", FALSE,
					     "language", locale,
					     NULL);
				name = imsettings_info_get_short_desc(info);
				if (need_lower) {
					ln = g_ascii_strdown(name, -1);
				} else {
					ln = g_strdup(name);
				}
				if (strcmp(ln, lowername) != 0)
					info = NULL;
				g_free(ln);
				break;
			}
			info = NULL;
		}
		if (info == NULL) {
			/* module may be a XIM server and running on the different locale */
			g_set_error(error, IMSETTINGS_MONITOR_ERROR,
				    IMSETTINGS_MONITOR_ERROR_NOT_AVAILABLE,
				    _("No such input method on your system: %s"),
				    module);
		}
	}
	g_free(lowername);

	return info ? g_object_ref(info) : NULL;
}

/*
 * Public functions
 */
GQuark
imsettings_monitor_get_error_quark(void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string("imsettings-monitor-error");

	return quark;
}

IMSettingsMonitor *
imsettings_monitor_new(const gchar *xinputrcdir,
		       const gchar *xinputdir,
		       const gchar *homedir)
{
	IMSettingsMonitor *retval;

	retval = IMSETTINGS_MONITOR (g_object_new(IMSETTINGS_TYPE_MONITOR, NULL));
	if (xinputrcdir)
		g_object_set(retval, "xinputrcdir", xinputrcdir, NULL);
	if (xinputdir)
		g_object_set(retval, "xinputdir", xinputdir, NULL);
	if (homedir)
		g_object_set(retval, "homedir", homedir, NULL);

	return retval;
}

void
imsettings_monitor_start(IMSettingsMonitor *monitor)
{
	g_return_if_fail (IMSETTINGS_IS_MONITOR (monitor));

	g_hash_table_remove_all(monitor->im_info_from_filename);
	g_hash_table_remove_all(monitor->im_info_from_name);
	g_free(monitor->current_user_im);
	g_free(monitor->current_system_im);
	monitor->current_user_im = NULL;
	monitor->current_system_im = NULL;

	_imsettings_monitor_start_for_xinputdir(monitor);
	_imsettings_monitor_start_for_homedir(monitor);
	_imsettings_monitor_start_for_xinputrcdir(monitor);
}

void
imsettings_monitor_stop(IMSettingsMonitor *monitor)
{
	gint i;

	g_return_if_fail (IMSETTINGS_IS_MONITOR (monitor));

	if (monitor->mon_xinputd) {
		g_signal_handlers_disconnect_by_func(monitor->mon_xinputd,
						     G_CALLBACK (imsettings_monitor_real_changed_xinputd),
						     monitor);
		g_object_unref(monitor->mon_xinputd);
		monitor->mon_xinputd = NULL;
	}
	for (i = 0; i < monitor->mon_dot_xinputrc->len; i++) {
		imsettings_monitor_file_unref(g_ptr_array_index(monitor->mon_dot_xinputrc, i));
	}
	monitor->mon_dot_xinputrc->len = 0;
	for (i = 0; i < monitor->mon_xinputrc->len; i++) {
		imsettings_monitor_file_unref(g_ptr_array_index(monitor->mon_xinputrc, i));
	}
	monitor->mon_xinputrc->len = 0;
	g_hash_table_remove_all(monitor->file_list_for_xinputrc);
	g_hash_table_remove_all(monitor->file_list_for_dot_xinputrc);
	g_hash_table_remove_all(monitor->im_info_from_name);
	g_hash_table_remove_all(monitor->im_info_from_filename);
}

GPtrArray *
imsettings_monitor_foreach(IMSettingsMonitor *monitor,
			   const gchar       *locale)
{
	IMSettingsMonitorCollectObjects v;
	GPtrArray *retval;
	gpointer p;

	g_return_val_if_fail (IMSETTINGS_IS_MONITOR (monitor), NULL);
	g_return_val_if_fail (locale != NULL, NULL);

	retval = g_ptr_array_new();
	v.q = g_queue_new();
	v.lang = locale;
	v.legacy_im = NULL;

	g_queue_init(v.q);

	g_hash_table_foreach(monitor->im_info_from_filename,
			     _imsettings_monitor_collect_info_objects,
			     &v);

	while ((p = g_queue_pop_head(v.q)))
		g_ptr_array_add(retval, p);
	if (v.legacy_im)
		g_ptr_array_add(retval, v.legacy_im);

	g_queue_free(v.q);

	return retval;
}

void
imsettings_monitor_array_free(GPtrArray *array)
{
	gint i;

	g_return_if_fail (array != NULL);

	for (i = 0; i < array->len; i++) {
		g_object_unref(g_ptr_array_index(array, i));
	}
	g_ptr_array_free(array, TRUE);
}

IMSettingsInfo *
imsettings_monitor_lookup(IMSettingsMonitor  *monitor,
			  const gchar        *module,
			  const gchar        *locale,
			  GError            **error)
{
	IMSettingsInfo *info;

	g_return_val_if_fail (IMSETTINGS_IS_MONITOR (monitor), NULL);
	g_return_val_if_fail (module != NULL, NULL);

	info = _imsettings_monitor_lookup(monitor->im_info_from_name, module, locale, TRUE, error);
	if (info == NULL) {
		GError *err = g_error_copy(*error);
		gchar *filename = g_build_filename(monitor->xinputdir, module, NULL);

		g_clear_error(error);
		/* try to pick one up from filename table */
		info = _imsettings_monitor_lookup(monitor->im_info_from_filename, filename, locale, FALSE, error);
		if (info == NULL) {
			/* revert the original error instance to be sane */
			g_error_free(*error);
			*error = g_error_copy(err);
			g_error_free(err);
		}
	}

	return info ? g_object_ref(info) : NULL;
}

gchar *
imsettings_monitor_get_current_user_im(IMSettingsMonitor  *monitor,
				       GError            **error)
{
	gchar *lowername = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_MONITOR (monitor), NULL);

	if (monitor->current_user_im)
		lowername = g_ascii_strdown(monitor->current_user_im, -1);

	if (lowername &&
	    g_hash_table_lookup(monitor->im_info_from_name,
				lowername) == NULL) {
		/* IM might be removed */
		return imsettings_monitor_get_current_system_im(monitor, error);
	}

	g_free(lowername);

	return (monitor->current_user_im ?
		g_strdup(monitor->current_user_im) :
		imsettings_monitor_get_current_system_im(monitor, error));
}

gchar *
imsettings_monitor_get_current_system_im(IMSettingsMonitor  *monitor,
					 GError            **error)
{
	gchar *lowername = NULL;

	g_return_val_if_fail (IMSETTINGS_IS_MONITOR (monitor), NULL);

	if (monitor->current_system_im)
		lowername = g_ascii_strdown(monitor->current_system_im, -1);
	if (lowername &&
	    g_hash_table_lookup(monitor->im_info_from_name,
				lowername) == NULL) {
		/* IM might be removed */
		g_set_error(error, IMSETTINGS_MONITOR_ERROR,
			    IMSETTINGS_MONITOR_ERROR_NOT_AVAILABLE,
			    _("No system default input method."));

		return NULL;
	}

	g_free(lowername);

	return g_strdup(monitor->current_system_im);
}

gchar *
imsettings_monitor_get_shortname(IMSettingsMonitor *monitor,
				 const gchar       *filename)
{
	gsize xinputrc_len, xinput_len, home_len, len;

	g_return_val_if_fail (IMSETTINGS_IS_MONITOR (monitor), NULL);

	if (filename == NULL)
		return NULL;

	len = strlen(filename);
	xinputrc_len = strlen(monitor->xinputrcdir);
	xinput_len = strlen(monitor->xinputdir);
	home_len = strlen(monitor->homedir);

	if (len > xinput_len &&
	    strncmp(filename, monitor->xinputdir, xinput_len) == 0) {
		return g_path_get_basename(filename);
	} else if (len > xinputrc_len &&
		   strncmp(filename, monitor->xinputrcdir, xinputrc_len) == 0) {
		return g_path_get_basename(filename);
	} else if (len > home_len &&
		   strncmp(filename, monitor->homedir, home_len) == 0) {
		return g_path_get_basename(filename);
	}

	return NULL;
}
