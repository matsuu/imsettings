/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * factory.c
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

#include <features.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <libnotify/notify.h>
#include "monitor.h"
#include "imsettings/imsettings-info-private.h"
#include "imsettings/imsettings-observer.h"
#include "imsettings/imsettings-request.h"
#include "imsettings/imsettings-utils.h"
#include "imsettings/imsettings.h"


typedef struct _IMSettingsManagerClass		IMSettingsManagerClass;
typedef struct _IMSettingsManager		IMSettingsManager;
typedef struct _IMSettingsManagerPrivate	IMSettingsManagerPrivate;


#define IMSETTINGS_TYPE_MANAGER			(imsettings_manager_get_type())
#define IMSETTINGS_MANAGER(_o_)			(G_TYPE_CHECK_INSTANCE_CAST ((_o_), IMSETTINGS_TYPE_MANAGER, IMSettingsManager))
#define IMSETTINGS_MANAGER_CLASS(_c_)		(G_TYPE_CHECK_CLASS_CAST ((_c_), IMSETTINGS_TYPE_MANAGER, IMSettingsManagerClass))
#define IMSETTINGS_IS_MANAGER(_o_)		(G_TYPE_CHECK_INSTANCE_TYPE ((_o_), IMSETTINGS_TYPE_MANAGER))
#define IMSETTINGS_IS_MANAGER_CLASS(_c_)	(G_TYPE_CHECK_CLASS_TYPE ((_c_), IMSETTINGS_TYPE_MANAGER))
#define IMSETTINGS_MANAGER_GET_CLASS(_o_)	(G_TYPE_INSTANCE_GET_CLASS ((_o_), IMSETTINGS_TYPE_MANAGER, IMSettingsManagerClass))
#define IMSETTINGS_MANAGER_GET_PRIVATE(_o_)	(G_TYPE_INSTANCE_GET_PRIVATE ((_o_), IMSETTINGS_TYPE_MANAGER, IMSettingsManagerPrivate))

#define RECONNECT_INTERVAL 3
#define PROCESSCHECK_INTERVAL 1
#define RESTART_COUNTER_THRESHOLD 5
#define MAXRESTART 3


struct _IMSettingsManagerClass {
	IMSettingsObserverClass parent_class;
};
struct _IMSettingsManager {
	IMSettingsObserver  parent_instance;
	GLogFunc            old_log_handler;
};
struct _IMSettingsManagerPrivate {
	IMSettingsRequest  *gtk_req;
	IMSettingsRequest  *xim_req;
	IMSettingsRequest  *qt_req;
	DBusConnection     *req_conn;
	GSList             *im_running;
	IMSettingsMonitor  *monitor;
	GHashTable         *pid2id;
	GHashTable         *aux2info;
	GHashTable         *body2info;
	NotifyNotification *notify;
	gboolean            enable_logging;
};
struct ProcessInformation {
	gchar             *module;
	gchar             *lang;
	GPid               pid;
	guint              id;
	guint              restart_counter;
	GTimeVal           started_time;
	gint               ref_count;
	GIOChannel        *stdout;
	GIOChannel        *stderr;
	guint              oid;
	guint              eid;
	IMSettingsManager *manager;
};


enum {
	PROP_0,
	PROP_XINPUTRCDIR,
	PROP_XINPUTDIR,
	PROP_HOMEDIR,
	PROP_LOGGING,
	LAST_PROP
};

GType            imsettings_manager_get_type             (void) G_GNUC_CONST;
static gboolean  imsettings_manager_real_start_im        (IMSettingsObserver  *imsettings,
                                                          const gchar         *lang,
                                                          const gchar         *module,
                                                          gboolean             update_xinputrc,
                                                          GError             **error);
static gboolean  _imsettings_manager_start_im_aux        (IMSettingsObserver  *imsettings,
							  const char          *lang,
							  const char          *module,
							  GError             **error);
static gboolean  _imsettings_manager_start_im_main       (IMSettingsObserver  *imsettings,
							  const char          *lang,
							  const char          *module,
							  GError             **error);
static gchar    *imsettings_manager_real_whats_im_running(IMSettingsObserver  *observer,
                                                          GError             **error);
static void      imsettings_manager_start_monitor        (IMSettingsManager   *manager);
static void      imsettings_manager_stop_monitor         (IMSettingsManager   *manager);


G_DEFINE_TYPE (IMSettingsManager, imsettings_manager, IMSETTINGS_TYPE_OBSERVER);
G_LOCK_DEFINE_STATIC (logger);

static GMainLoop *loop;

/*
 * Private functions
 */
static struct ProcessInformation *
_process_info_new(void)
{
	struct ProcessInformation *retval = g_new0(struct ProcessInformation, 1);

	retval->ref_count = 1;

	return retval;
}

static struct ProcessInformation *
_process_info_ref(struct ProcessInformation *info)
{
	g_atomic_int_add(&info->ref_count, 1);

	return info;
}

static void
_process_info_unref(gpointer data)
{
	struct ProcessInformation *info = data;

	if (g_atomic_int_exchange_and_add(&info->ref_count, -1) - 1 == 0) {
		g_free(info->module);
		g_free(info->lang);
		g_io_channel_unref(info->stdout);
		g_io_channel_unref(info->stderr);
		g_source_remove(info->oid);
		g_source_remove(info->eid);
		g_source_remove(info->id);

		g_free(info);
	}
}

static struct ProcessInformation *
_get_process_info_by_module(GHashTable  *table,
			    const gchar *module)
{
	GHashTableIter iter;
	gpointer key, val;

	g_hash_table_iter_init(&iter, table);
	while (g_hash_table_iter_next(&iter, &key, &val)) {
		struct ProcessInformation *info = val;

		if (strcmp((gchar *)key, info->module) == 0) {
			return info;
		}
	}

	return NULL;
}

static struct ProcessInformation *
_get_process_info_by_id(GHashTable *table,
			guint       id)
{
	GHashTableIter iter;
	gpointer key, val;

	g_hash_table_iter_init(&iter, table);
	while (g_hash_table_iter_next(&iter, &key, &val)) {
		struct ProcessInformation *info = val;

		if (id == info->id) {
			return info;
		}
	}

	return NULL;
}

static struct ProcessInformation *
_get_process_info_by_pid(GHashTable *table,
			 GPid        pid)
{
	GHashTableIter iter;
	gpointer key, val;

	g_hash_table_iter_init(&iter, table);
	while (g_hash_table_iter_next(&iter, &key, &val)) {
		struct ProcessInformation *info = val;

		if (pid == info->pid) {
			return info;
		}
	}

	return NULL;
}

static GPid
_get_pid_from_name(IMSettingsManagerPrivate *priv,
		   gboolean                  is_body,
		   const gchar              *name)
{
	struct ProcessInformation *info;

	if (is_body) {
		info = _get_process_info_by_module(priv->body2info, name);
	} else {
		info = _get_process_info_by_module(priv->aux2info, name);
	}
	if (info == NULL)
		return 0;

	return info->pid;
}

static void
_notify(IMSettingsManager *manager,
	NotifyUrgency      urgency,
	const gchar       *summary,
	const gchar       *body,
	gint               timeout)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (manager);

	notify_notification_set_urgency(priv->notify, urgency);
	notify_notification_update(priv->notify,
				   _(summary),
				   _(body),
				   NULL);
	notify_notification_clear_actions(priv->notify);
	notify_notification_set_timeout(priv->notify, timeout * 1000);
	notify_notification_set_category(priv->notify, "x-imsettings-notice");
	notify_notification_show(priv->notify, NULL);
}

static void
_output_log(IMSettingsManagerPrivate *priv,
	    const gchar              *buffer,
	    gsize                     length)
{
	FILE *fp;
	gchar *logfile, *homedir = NULL;

	if (!priv->enable_logging)
		return;
	if (length < 0) {
		length = strlen(buffer);
	}
	g_object_get(priv->monitor, "homedir", &homedir, NULL);
	if (homedir == NULL) {
		g_printerr("No home directory is set.");
		return;
	}
	logfile = g_build_filename(homedir, ".imsettings.log", NULL);

	G_LOCK (logger);

	fp = fopen(logfile, "ab");
	if (fp == NULL) {
		g_printerr("Failed to open a log file.");
		return;
	}

	fwrite(buffer, length, sizeof (gchar), fp);

	fclose(fp);

	G_UNLOCK (logger);

	g_free(logfile);
	g_free(homedir);
}

static void
_log_handler(const gchar    *log_domain,
	     GLogLevelFlags  log_level,
	     const gchar    *message,
	     gpointer        data)
{
	GString *log = g_string_new(NULL);
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (data);

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
		    g_string_append(log, "Message");
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
	if (message) {
		g_string_append(log, message);
	} else {
		g_string_append(log, "(NULL) message");
	}
	g_string_append(log, "\n");

	g_print("%s", log->str);
	_output_log(priv, log->str, log->len);

	g_string_free(log, TRUE);
}

static gboolean
_logger_write_cb(GIOChannel   *channel,
		 GIOCondition  condition,
		 gpointer      data)
{
	gchar *buffer = NULL;
	gsize len;
	GError *error = NULL;
	struct ProcessInformation *info = data;
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (info->manager);
	GString *string = g_string_new(NULL);

	g_io_channel_read_line(channel, &buffer, &len, NULL, &error);
	if (error) {
		g_warning("Failed to read the log from IM: %s", error->message);

		return FALSE;
	}
	g_string_append_printf(string, "%s[%lu]: %s", info->module, (gulong)info->pid, buffer);
	_output_log(priv, string->str, string->len);
	g_string_free(string, TRUE);
	g_free(buffer);

	return TRUE;
}

static void
_watch_im_status_cb(GPid     pid,
		    gint     status,
		    gpointer data)
{
	IMSettingsManager *manager = IMSETTINGS_MANAGER (data);
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (manager);
	guint id;
	struct ProcessInformation *info;
	gboolean is_body = TRUE;
	GString *status_message = NULL;

	if (WIFSTOPPED (status)) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "pid %d is stopped with the signal %d",
		      pid, WSTOPSIG (status));
		return;
	} else if (WIFCONTINUED (status)) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "pid %d is continued.", pid);
		return;
	} else {
		status_message = g_string_new(NULL);

		if (WIFEXITED (status)) {
			g_string_append_printf(status_message, "the status %d", WEXITSTATUS (status));
		} else if (WIFSIGNALED (status)) {
			g_string_append_printf(status_message, "the signal %d", WTERMSIG (status));
			if (WCOREDUMP (status)) {
				g_string_append(status_message, " (core dumped)");
			}
		}
	}

	id = GPOINTER_TO_UINT (g_hash_table_lookup(priv->pid2id, GINT_TO_POINTER (pid)));
	if (id > 0) {
		g_hash_table_remove(priv->pid2id, GINT_TO_POINTER (pid));
		/* child process may died unexpectedly. */
		info = _get_process_info_by_id(priv->body2info, id);
		if (info == NULL) {
			is_body = FALSE;
			info = _get_process_info_by_id(priv->aux2info, id);
		}
		if (info == NULL) {
			g_warning("No consistency in the internal process management database: pid: %d", pid);
		} else {
			GTimeVal current;

			g_get_current_time(&current);
			/* XXX: This is Y2038 unsafe */
			if (current.tv_sec - info->started_time.tv_sec > RESTART_COUNTER_THRESHOLD) {
				info->restart_counter = 0;
			} else {
				info->restart_counter++;
			}
			if (info->restart_counter >= MAXRESTART) {
				gchar *message = g_strdup_printf(N_("Giving up to bring the process up because %s Input Method process for %s rapidly died many times. See .imsettings.log for more details."),
								 is_body ? N_("Main") : N_("AUX"), info->module);
				g_critical(message);
				_notify(manager, NOTIFY_URGENCY_CRITICAL, N_("Unable to keep Input Method running"), message, 0);

				g_free(message);

				g_hash_table_remove(is_body ? priv->body2info : priv->aux2info, info->module);
			} else {
				GError *error = NULL;
				gchar *module, *lang;

				module = g_strdup(info->module);
				lang = g_strdup(info->lang);
				g_warning("%s Input Method process for %s died with %s, but unexpectedly. restarting...", is_body ? "Main" : "AUX", info->module, status_message->str);
				if (is_body)
					_imsettings_manager_start_im_main(IMSETTINGS_OBSERVER (manager), lang, module, &error);
				else
					_imsettings_manager_start_im_aux(IMSETTINGS_OBSERVER (manager), lang, module, &error);
				if (error != NULL) {
					_notify(manager, NOTIFY_URGENCY_CRITICAL, N_("Unable to keep Input Method running"), error->message, 0);
					g_error_free(error);
				}

				g_free(lang);
				g_free(module);
			}
		}
	} else {
		info = _get_process_info_by_pid(priv->body2info, pid);
		if (info == NULL) {
			is_body = FALSE;
			info = _get_process_info_by_pid(priv->aux2info, pid);
		}
		if (info == NULL) {
			g_warning("No consistency in the internal process management database: pid: %d", pid);
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
			      "pid %d is successfully stopped with %s.", pid, status_message->str);
		} else {
			gchar *module = g_strdup(info->module);

			g_hash_table_remove(is_body ? priv->body2info : priv->aux2info,
					    info->module);
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
			      "Stopped %s process for %s with %s: pid %d",
			      is_body ? "Main" : "AUX", module, status_message->str, pid);
			g_free(module);
		}
	}
	if (status_message)
		g_string_free(status_message, TRUE);

	g_spawn_close_pid(pid);
}

static void
_stop_all_processes(IMSettingsManager *manager)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (manager);
	GHashTableIter iter;
	gpointer key, val;
	struct ProcessInformation *info = val;

	g_hash_table_remove_all(priv->pid2id);

	g_hash_table_iter_init(&iter, priv->aux2info);
	while (g_hash_table_iter_next(&iter, &key, &val)) {
		info = val;
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		      "Stopping %s Input Method process for %s...",
		      "AUX", (gchar *)key);
		kill(-info->pid, SIGTERM);
	}
	g_hash_table_iter_init(&iter, priv->body2info);
	while (g_hash_table_iter_next(&iter, &key, &val)) {
		info = val;
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		      "Stopping %s Input Method process for %s...",
		      "Main", (gchar *)key);
		kill(-info->pid, SIGTERM);
	}
}

static void
_child_setup(gpointer data)
{
	pid_t pid;

	pid = getpid();
	setpgid(pid, 0);
}

static gboolean
_start_process(IMSettingsManager  *manager,
	       const gchar        *identity,
	       const gchar        *prog_name,
	       const gchar        *prog_args,
	       gboolean            is_body,
	       const gchar        *lang,
	       GError            **error)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (manager);

	if (prog_name != NULL && prog_name[0] != 0) {
		gchar *cmd, **argv, **envp = NULL, **env_list;
		static const gchar *env_names[] = {
			"LC_CTYPE",
			NULL
		};
		guint len, i = 0, j = 0;
		GPid pid;
		gint ofd, efd;

		env_list = g_listenv();
		len = g_strv_length(env_list);
		envp = g_new0(gchar *, G_N_ELEMENTS (env_names) + len + 1);
		for (i = 0; i < len; i++) {
			envp[i] = g_strdup_printf("%s=%s",
						  env_list[i],
						  g_getenv(env_list[i]));
		}
		if (lang) {
			envp[i + j] = g_strdup_printf("%s=%s", env_names[j], lang); j++;
		}
		envp[i + j] = NULL;

		cmd = g_strdup_printf("%s %s", prog_name, (prog_args ? prog_args : ""));
		g_shell_parse_argv(cmd, NULL, &argv, NULL);
		if (g_spawn_async_with_pipes(g_get_tmp_dir(), argv, envp,
					     G_SPAWN_DO_NOT_REAP_CHILD,
					     _child_setup, NULL, &pid,
					     NULL,
					     &ofd, &efd,
					     error)) {
			GHashTable *hash;
			struct ProcessInformation *p;
			struct ProcessInformation *info;
			gchar *module = g_strdup(identity);
			gchar *time;

			if (is_body) {
				hash = priv->body2info;
			} else {
				hash = priv->aux2info;
			}
			p = _get_process_info_by_module(hash, identity);
			if (p == NULL) {
				info = _process_info_new();
				info->module = g_strdup(identity);
				info->lang = g_strdup(lang);
				info->restart_counter = 0;
			} else {
				info = _process_info_ref(p);
				g_hash_table_remove(hash, identity);
				g_io_channel_unref(info->stdout);
				g_io_channel_unref(info->stderr);
				g_source_remove(info->id);
				g_source_remove(info->oid);
				g_source_remove(info->eid);
			}
			info->pid = pid;
			info->stdout = g_io_channel_unix_new(ofd);
			info->stderr = g_io_channel_unix_new(efd);
			g_io_channel_set_close_on_unref(info->stdout, TRUE);
			g_io_channel_set_close_on_unref(info->stderr, TRUE);
			info->manager = manager;

			info->oid = g_io_add_watch(info->stdout, G_IO_IN, _logger_write_cb, info);
			info->eid = g_io_add_watch(info->stderr, G_IO_IN, _logger_write_cb, info);
			
			g_get_current_time(&info->started_time);
			g_hash_table_insert(hash, module, info);

			info->id = g_child_watch_add(pid, _watch_im_status_cb, manager);
			g_hash_table_insert(priv->pid2id, GINT_TO_POINTER (pid), GUINT_TO_POINTER (info->id));

			time = g_time_val_to_iso8601(&info->started_time);
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
			      "Started %s: process: %s %s, lang=%s, pid: %d, id: %d, time: %s",
			      identity, prog_name, prog_args ? prog_args : "", lang, pid, info->id, time);
			g_free(time);
		}
		g_strfreev(envp);

		g_free(cmd);
		g_strfreev(argv);
		g_strfreev(env_list);
	}

	return (*error == NULL);
}

static gboolean
_stop_process(IMSettingsManagerPrivate  *priv,
	      const gchar               *identity,
	      gboolean                   is_body,
	      GError                   **error)
{
	gboolean retval = FALSE;
	GHashTable *hash;
	struct ProcessInformation *info, *tmp;

	if (is_body) {
		hash = priv->body2info;
	} else {
		hash = priv->aux2info;
	}
	info = g_hash_table_lookup(hash, identity);
	if (info == NULL) {
		if (is_body) {
			g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_NOT_AVAILABLE,
				    _("No such Input Method is running: %s"),
				    identity);
		} else {
			/* ignore for aux */
			retval = TRUE;
		}
	} else {
		tmp = _process_info_ref(info);
		/* info will be deleted from *2info Hash table at _watch_im_status_cb when the process is really died. */
		g_hash_table_remove(priv->pid2id, GINT_TO_POINTER (info->pid));
		if (kill(-info->pid, SIGTERM) == -1) {
			gchar *module = g_strdup(identity);

			g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_UNABLE_TO_TRACK_IM,
				    _("Couldn't send a signal to the %s process successfully."),
				    is_body ? "Main" : "AUX");
			/* push back to the table */
			g_hash_table_insert(hash, module, tmp);
			g_hash_table_insert(priv->pid2id, GINT_TO_POINTER (info->pid), GUINT_TO_POINTER (info->id));
		} else {
			GTimeVal time;
			gchar *s;

			g_get_current_time(&time);
			s = g_time_val_to_iso8601(&time);
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Sent a signal to stop %s: pid: %d, time: %s", identity, info->pid, s);
			retval = TRUE;
			g_free(s);
			/* info will be unref'd in _watch_im_status_cb when the process is really died. */
		}
	}

	return retval;
}

static gboolean
_update_symlink(IMSettingsManagerPrivate  *priv,
		const gchar               *xinputfile,
		GError                   **error)
{
	struct stat st;
	gchar *homedir = NULL;
	gchar *conffile = NULL, *backfile = NULL, *p = NULL, *n = NULL;
	int save_errno;

	g_object_get(priv->monitor, "homedir", &homedir, NULL);
	if (homedir == NULL) {
		g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			    _("Failed to get a place of home directory."));
		return FALSE;
	}
	conffile = g_build_filename(homedir, IMSETTINGS_USER_XINPUT_CONF, NULL);
	backfile = g_build_filename(homedir, IMSETTINGS_USER_XINPUT_CONF ".bak", NULL);
	p = g_path_get_dirname(xinputfile);
	n = g_path_get_basename(xinputfile);
	if (strcmp(p, homedir) == 0 &&
	    strcmp(n, IMSETTINGS_USER_XINPUT_CONF) == 0) {
	} else if (strcmp(p, homedir) == 0 &&
		   strcmp(n, IMSETTINGS_USER_XINPUT_CONF ".bak") == 0) {
		/* try to revert the backup file for the user specific conf file */
		if (g_rename(backfile, conffile) == -1) {
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
				/* .xinputrc was probably made by the hand. */
				if (g_rename(conffile, backfile) == -1) {
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
				    _("Failed to make a symlink: %s"),
				    g_strerror(save_errno));
		}
	}
  end:
	g_free(n);
	g_free(p);
	g_free(conffile);
	g_free(backfile);
	g_free(homedir);

	return (*error == NULL);
}

static void
disconnected_cb(IMSettingsManager *manager)
{
	g_main_loop_quit(loop);
}

static void
reload_cb(IMSettingsManager *manager,
	  gboolean           force)
{
	if (force) {
		g_main_loop_quit(loop);
	} else {
		imsettings_manager_stop_monitor(manager);
		imsettings_manager_start_monitor(manager);
	}
}


static void
imsettings_manager_real_set_property(GObject      *object,
				     guint         prop_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (object);
	const gchar *p;

	switch (prop_id) {
	    case PROP_XINPUTRCDIR:
		    p = g_value_get_string(value);
		    g_object_set(priv->monitor, "xinputrcdir", p, NULL);
		    break;
	    case PROP_XINPUTDIR:
		    p = g_value_get_string(value);
		    g_object_set(priv->monitor, "xinputdir", p, NULL);
		    break;
	    case PROP_HOMEDIR:
		    p = g_value_get_string(value);
		    g_object_set(priv->monitor, "homedir", p, NULL);
		    break;
	    case PROP_LOGGING:
		    priv->enable_logging = g_value_get_boolean(value);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_manager_real_get_property(GObject    *object,
				     guint       prop_id,
				     GValue     *value,
				     GParamSpec *pspec)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (object);
	gchar *p = NULL;

	switch (prop_id) {
	    case PROP_XINPUTRCDIR:
		    g_object_get(priv->monitor, "xinputrcdir", &p, NULL);
		    g_value_set_string(value, p);
		    break;
	    case PROP_XINPUTDIR:
		    g_object_get(priv->monitor, "xinputdir", &p, NULL);
		    g_value_set_string(value, p);
		    break;
	    case PROP_HOMEDIR:
		    g_object_get(priv->monitor, "homedir", &p, NULL);
		    g_value_set_string(value, p);
		    break;
	    case PROP_LOGGING:
		    g_value_set_boolean(value, priv->enable_logging);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
	g_free(p);
}

static void
imsettings_manager_real_dispose(GObject *object)
{
	_stop_all_processes(IMSETTINGS_MANAGER (object));
}

static void
imsettings_manager_real_finalize(GObject *object)
{
	IMSettingsManagerPrivate *priv;

	priv = IMSETTINGS_MANAGER_GET_PRIVATE (object);

	if (priv->gtk_req)
		g_object_unref(priv->gtk_req);
	if (priv->xim_req)
		g_object_unref(priv->xim_req);
	if (priv->qt_req)
		g_object_unref(priv->qt_req);
	dbus_connection_unref(priv->req_conn);
	if (priv->monitor)
		g_object_unref(priv->monitor);
	if (priv->im_running)
		g_slist_free(priv->im_running);
	if (priv->pid2id)
		g_hash_table_destroy(priv->pid2id);
	if (priv->aux2info)
		g_hash_table_destroy(priv->aux2info);
	if (priv->body2info)
		g_hash_table_destroy(priv->body2info);
	if (priv->notify)
		g_object_unref(priv->notify);

	if (G_OBJECT_CLASS (imsettings_manager_parent_class)->finalize)
		G_OBJECT_CLASS (imsettings_manager_parent_class)->finalize(object);
}

static guint
imsettings_manager_real_get_version(IMSettingsObserver  *observer,
				    GError             **error)
{
	return IMSETTINGS_SETTINGS_API_VERSION;
}

static GPtrArray *
imsettings_manager_real_get_info_objects(IMSettingsObserver  *imsettings,
					 const gchar         *locale,
					 GError             **error)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (imsettings);
	GPtrArray *array;

	array = imsettings_monitor_foreach(priv->monitor, locale);
	if (array->len == 0) {
		g_set_error(error, IMSETTINGS_MONITOR_ERROR,
			    IMSETTINGS_MONITOR_ERROR_NOT_AVAILABLE,
			    _("No input methods available on your system."));
	}

	return array;
}

static IMSettingsInfo *
imsettings_manager_real_get_info_object(IMSettingsObserver  *imsettings,
					const gchar         *locale,
					const gchar         *module,
					GError             **error)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (imsettings);

	return imsettings_monitor_lookup(priv->monitor, module, locale, error);
}

static GPtrArray *
imsettings_manager_real_get_input_method_list(IMSettingsObserver  *imsettings,
					      const gchar         *locale,
					      GError             **error)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (imsettings);
	GPtrArray *array, *retval = g_ptr_array_new();
	gint i;

	array = imsettings_monitor_foreach(priv->monitor, locale);
	for (i = 0; i < array->len; i++) {
		IMSettingsInfo *info;
		gchar *name;

		info = IMSETTINGS_INFO (g_ptr_array_index(array, i));
		name = g_strdup(imsettings_info_get_short_desc(info));
		g_ptr_array_add(retval, name);
	}
	imsettings_monitor_array_free(array);
	if (retval->len == 0) {
		g_set_error(error, IMSETTINGS_MONITOR_ERROR,
			    IMSETTINGS_MONITOR_ERROR_NOT_AVAILABLE,
			    _("No input methods available on your system."));
	}
	g_ptr_array_add(retval, NULL);

	return retval;
}

static gchar *
imsettings_manager_real_get_current_user_im(IMSettingsObserver  *imsettings,
					    GError             **error)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (imsettings);

	return imsettings_monitor_get_current_user_im(priv->monitor, error);
}

static gchar *
imsettings_manager_real_get_current_system_im(IMSettingsObserver  *imsettings,
					      GError             **error)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (imsettings);

	return imsettings_monitor_get_current_system_im(priv->monitor, error);
}

static gboolean
imsettings_manager_real_start_im(IMSettingsObserver  *imsettings,
				 const gchar         *lang,
				 const gchar         *module,
				 gboolean             update_xinputrc,
				 GError             **error)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (imsettings);
	const gchar *imm = NULL, *xinputfile = NULL;
	gboolean retval = FALSE;
	IMSettingsInfo *info = NULL;

	g_print("Starting %s...\n", module);

	info = imsettings_manager_real_get_info_object(imsettings, lang, module, error);
	if (*error) {
		gchar *p = g_strdup((*error)->message);

		g_clear_error(error);
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_IM_NOT_FOUND,
			    _("No such input method on your system: %s\n  Details: %s"),
			    module, p);
		g_free(p);
		goto end;
	}
	xinputfile = imsettings_info_get_filename(info);

	if (!_imsettings_manager_start_im_aux(imsettings, lang, module, error))
		goto end;
	if (!_imsettings_manager_start_im_main(imsettings, lang, module, error))
		goto end;

	/* FIXME: We need to take care of imsettings per X screens?
	 */
	/* FIXME: have to deal with the errors */
	imm = imsettings_info_get_gtkimm(info);
	imsettings_request_change_to(priv->gtk_req, imm, error);
	imm = imsettings_info_get_xim(info);
	imsettings_request_change_to_with_signal(priv->xim_req, imm);
#if 0
	imm = imsettings_info_get_qtimm(info);
	imsettings_request_change_to(priv->qt_req, imm, error);
#endif

	/* Finally update a symlink on your home */
	if (update_xinputrc) {
		if (!_update_symlink(priv, xinputfile, error))
			goto end;

		imsettings_request_send_signal_changed(priv->gtk_req, module);
		imsettings_request_send_signal_changed(priv->xim_req, module);
#if 0
		imsettings_request_send_signal_changed(priv->qt_req, module);
#endif
	}

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Current IM is: %s", module);

	retval = TRUE;
  end:
	if (info)
		g_object_unref(info);

	return retval;
}

static gboolean
_imsettings_manager_start_im_aux(IMSettingsObserver  *imsettings,
				 const char          *lang,
				 const char          *module,
				 GError             **error)
{
	const gchar *aux_prog = NULL, *aux_args = NULL;
	IMSettingsInfo *info = NULL;
	gboolean retval = FALSE;

	info = imsettings_manager_real_get_info_object(imsettings, lang, module, error);
	if (*error) {
		gchar *p = g_strdup((*error)->message);

		g_clear_error(error);
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_IM_NOT_FOUND,
			    _("No such input method on your system: %s\n  Details: %s"),
			    module, p);
		g_free(p);
		goto end;
	}
	aux_prog = imsettings_info_get_aux_program(info);
	aux_args = imsettings_info_get_aux_args(info);
	if (aux_prog) {
		/* bring up an auxiliary program */
		g_print("Starting AUX process for %s...", module);

		if (!_start_process(IMSETTINGS_MANAGER (imsettings), module, aux_prog, aux_args, FALSE, lang, error))
			goto end;
	}
	retval = TRUE;
  end:
	if (info)
		g_object_unref(info);

	return retval;
}

static gboolean
_imsettings_manager_start_im_main(IMSettingsObserver  *imsettings,
				  const char          *lang,
				  const char          *module,
				  GError             **error)
{
	const gchar *xim_prog = NULL, *xim_args = NULL;
	gboolean retval = FALSE;
	IMSettingsInfo *info = NULL;

	info = imsettings_manager_real_get_info_object(imsettings, lang, module, error);
	if (*error) {
		gchar *p = g_strdup((*error)->message);

		g_clear_error(error);
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_IM_NOT_FOUND,
			    _("No such input method on your system: %s\n  Details: %s"),
			    module, p);
		g_free(p);
		goto end;
	}
	/* hack to allow starting none.conf and immodule only conf */
	if (strcmp(module, "none") != 0 && !imsettings_info_is_immodule_only(info)) {
		xim_prog = imsettings_info_get_xim_program(info);
		xim_args = imsettings_info_get_xim_args(info);
		if (xim_prog == NULL) {
			g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_INVALID_IMM,
				    _("No XIM server is available in %s"), module);
			goto end;
		}
		/* bring up a XIM server */
		g_print("Starting Main process for %s...", module);

		if (!_start_process(IMSETTINGS_MANAGER (imsettings), module, xim_prog, xim_args, TRUE, lang, error))
			goto end;
	}
	retval = TRUE;
  end:
	if (info)
		g_object_unref(info);

	return retval;
}

static gboolean
imsettings_manager_real_stop_im(IMSettingsObserver  *imsettings,
				const gchar         *module,
				gboolean             update_xinputrc,
				gboolean             force,
				GError             **error)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (imsettings);
	IMSettingsInfo *info = NULL;
	gchar *homedir = NULL;
	const gchar *gtkimm, *xim, *aux_prog = NULL;
	gchar *p = NULL;
	gboolean retval = FALSE;
	GString *strerr = g_string_new(NULL);
	GPid pid;

	g_print("Stopping %s...\n", module);

	info = imsettings_manager_real_get_info_object(imsettings, NULL, "none", error);
	if (*error)
		goto end;

	gtkimm = imsettings_info_get_gtkimm(info);
	xim = imsettings_info_get_xim(info);

	/* Change the settings before killing the IM process(es) */
	/* FIXME: We need to take care of imsettings per X screens?
	 */
	imsettings_request_change_to(priv->gtk_req, gtkimm ? gtkimm : "", error);
	imsettings_request_change_to_with_signal(priv->xim_req, xim ? xim : "none");
#if 0
	imsettings_request_change_to(priv->qt_req, NULL, error);
#endif

	g_object_unref(info);

	info = imsettings_manager_real_get_info_object(imsettings, NULL, module, error);
	if (*error)
		goto end;

	aux_prog = imsettings_info_get_aux_program(info);
	pid = _get_pid_from_name(priv, FALSE, module);
	/* kill an auxiliary program */
	if (!_stop_process(priv, module, FALSE, error)) {
		if (force) {
			if (aux_prog != NULL)
				g_string_append_printf(strerr, "%s\n", (*error)->message);
			g_error_free(*error);
			*error = NULL;
			g_hash_table_remove(priv->aux2info, module);
			g_hash_table_remove(priv->pid2id, GINT_TO_POINTER (pid));
		} else {
			if (aux_prog != NULL)
				goto end;
		}
	}

	if (!imsettings_info_is_immodule_only(info)) {
		pid = _get_pid_from_name(priv, TRUE, module);
		/* kill a XIM server */
		if (!_stop_process(priv, module, TRUE, error)) {
			if (force) {
				g_string_append_printf(strerr, "%s\n", (*error)->message);
				g_error_free(*error);
				*error = NULL;
				g_hash_table_remove(priv->body2info, module);
				g_hash_table_remove(priv->pid2id, GINT_TO_POINTER (pid));
			} else {
				goto end;
			}
		}
	}

	/* finally update .xinputrc */
	g_object_get(priv->monitor, "homedir", &homedir, NULL);
	if (homedir == NULL) {
		g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			    _("Failed to get a place of home directory."));
		goto end;
	}
	if (imsettings_info_is_user_default(info) ||
	    ((p = imsettings_manager_real_get_current_user_im(imsettings, error)) != NULL &&
	     strcmp(p, module) == 0)) {
		gchar *conffile = g_build_filename(XINPUT_PATH, IMSETTINGS_NONE_CONF XINPUT_SUFFIX, NULL);

		if (update_xinputrc && !_update_symlink(priv, conffile, error)) {
			g_free(conffile);
			goto end;
		}
		g_free(conffile);
	}
	if (*error == NULL) {
		if (force && strerr->len > 0) {
			g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_FAILED,
				    "Errors detected, but forcibly stopped:\n  * %s",
				    strerr->str);
		} else {
			retval = TRUE;
		}
	}

	if (update_xinputrc) {
		imsettings_request_send_signal_changed(priv->gtk_req, "none");
		imsettings_request_send_signal_changed(priv->xim_req, "none");
#if 0
		imsettings_request_send_signal_changed(priv->qt_req, "none");
#endif
	}

	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "Current IM is: none");
  end:
	g_free(p);
	g_free(homedir);
	g_string_free(strerr, TRUE);
	if (info)
		g_object_unref(info);

	return retval;
}

static gchar *
imsettings_manager_real_whats_im_running(IMSettingsObserver  *observer,
					 GError             **error)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (observer);
	IMSettingsInfo *info = NULL;
	gchar *module;
	pid_t pid;

	module = imsettings_manager_real_get_current_user_im(observer, error);
	if (*error != NULL)
		goto end;
	if (module) {
		info = imsettings_manager_real_get_info_object(observer, NULL, module, error);
		if (*error) {
			gchar *p = g_strdup((*error)->message);

			g_clear_error(error);
			g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_IM_NOT_FOUND,
				    _("No such input method on your system: %s\n  Details: %s"),
				    module, p);
			g_free(p);
			goto end;
		}
		if (info == NULL) {
			/* no info object for current IM */
			goto end;
		}
		if (imsettings_info_is_immodule_only(info)) {
			goto end;
		}
		pid = _get_pid_from_name(priv, TRUE, module);
		if (pid == 0) {
			g_free(module);
			module = NULL;
		} else {
			if (kill(pid, 0) == -1) {
				g_free(module);
				module = NULL;
			}
		}
	} else {
		/* No error means no IM currently running */
		module = g_strdup("");
	}
  end:
	if (info)
		g_object_unref(info);

	return module;
}

static void
imsettings_manager_real_info_objects_free(IMSettingsObserver *observer,
					  GPtrArray          *array)
{
	g_return_if_fail (array != NULL);

	imsettings_monitor_array_free(array);
}

static void
imsettings_manager_class_init(IMSettingsManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	IMSettingsObserverClass *observer_class = IMSETTINGS_OBSERVER_CLASS (klass);

	g_type_class_add_private(klass, sizeof (IMSettingsManagerPrivate));

	object_class->set_property = imsettings_manager_real_set_property;
	object_class->get_property = imsettings_manager_real_get_property;
	object_class->dispose      = imsettings_manager_real_dispose;
	object_class->finalize     = imsettings_manager_real_finalize;

	observer_class->get_version           = imsettings_manager_real_get_version;
	observer_class->get_info_objects      = imsettings_manager_real_get_info_objects;
	observer_class->get_info_object       = imsettings_manager_real_get_info_object;
	observer_class->get_input_method_list = imsettings_manager_real_get_input_method_list;
	observer_class->get_current_user_im   = imsettings_manager_real_get_current_user_im;
	observer_class->get_current_system_im = imsettings_manager_real_get_current_system_im;

	observer_class->start_im              = imsettings_manager_real_start_im;
	observer_class->stop_im               = imsettings_manager_real_stop_im;
	observer_class->whats_im_running      = imsettings_manager_real_whats_im_running;

	observer_class->info_objects_free     = imsettings_manager_real_info_objects_free;

	/* properties */
	g_object_class_install_property(object_class, PROP_XINPUTRCDIR,
					g_param_spec_string("xinputrcdir",
							    _("xinputrc directory"),
							    _("The name of the directory for the system wide xinputrc file."),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_XINPUTDIR,
					g_param_spec_string("xinputdir",
							    _("xinput directory"),
							    _("The name of the directory for IM configurations."),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_HOMEDIR,
					g_param_spec_string("homedir",
							    _("home directory"),
							    _("home directory"),
							    NULL,
							    G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_LOGGING,
					g_param_spec_boolean("logging",
							     _("logging"),
							     _("A flag if logging facility is enabled or not."),
							     TRUE,
							     G_PARAM_READWRITE));
}

static void
imsettings_manager_init(IMSettingsManager *manager)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (manager);

	priv->req_conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);

	priv->gtk_req = imsettings_request_new(priv->req_conn, IMSETTINGS_GCONF_INTERFACE_DBUS);
	priv->xim_req = imsettings_request_new(priv->req_conn, IMSETTINGS_XIM_INTERFACE_DBUS);
//	priv->qt_req = imsettings_request_new(priv->req_conn, IMSETTINGS_QT_INTERFACE_DBUS);
	priv->monitor = imsettings_monitor_new(NULL, NULL, NULL);
	priv->pid2id = g_hash_table_new(g_direct_hash, g_direct_equal);
	priv->aux2info = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _process_info_unref);
	priv->body2info = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, _process_info_unref);
	priv->enable_logging = TRUE;

	if (!notify_is_initted())
		notify_init("im-settings-daemon");
	priv->notify = notify_notification_new("_", "_", NULL, NULL);
}

static IMSettingsManager *
imsettings_manager_new(DBusGConnection *connection,
		       const gchar     *xinputrcdir,
		       const gchar     *xinputdir,
		       const gchar     *homedir,
		       gboolean         replace,
		       gboolean         is_being_logged)
{
	IMSettingsManager *retval;

	g_return_val_if_fail (connection != NULL, NULL);

	retval = IMSETTINGS_MANAGER (g_object_new(IMSETTINGS_TYPE_MANAGER,
						  "replace", replace,
						  "connection", connection,
						  "logging", is_being_logged,
						  NULL));
	if (xinputrcdir)
		g_object_set(G_OBJECT (retval),
			     "xinputrcdir", xinputrcdir,
			     NULL);
	if (xinputdir)
		g_object_set(G_OBJECT (retval),
			     "xinputdir", xinputdir,
			     NULL);
	if (homedir)
		g_object_set(G_OBJECT (retval),
			     "homedir", homedir,
			     NULL);

	return retval;
}

static void
imsettings_manager_start_monitor(IMSettingsManager *manager)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (manager);

	imsettings_monitor_start(priv->monitor);
}

static void
imsettings_manager_stop_monitor(IMSettingsManager *manager)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (manager);

	imsettings_monitor_stop(priv->monitor);
}

static void
_sig_handler(int signum)
{
	g_main_loop_quit(loop);
}

/*
 * Public functions
 */
int
main(int    argc,
     char **argv)
{
	GError *error = NULL;
	IMSettingsManager *manager;
	gboolean arg_replace = FALSE, arg_no_logfile = FALSE;
	gchar *arg_xinputrcdir = NULL, *arg_xinputdir = NULL, *arg_homedir = NULL;
	GOptionContext *ctx = g_option_context_new(NULL);
	GOptionEntry entries[] = {
		{"replace", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_replace, N_("Replace the running settings daemon with new instance."), NULL},
		{"xinputrcdir", 0, G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &arg_xinputrcdir, N_("Specify the name of the directory of the system-wide xinputrc file (for debugging only)"), N_("DIR")},
		{"xinputdir", 0, G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &arg_xinputdir, N_("Specify the name of the directory for IM configurations (for debugging only)"), N_("DIR")},
		{"homedir", 0, G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_FILENAME, G_OPTION_ARG_STRING, &arg_homedir, N_("A home directory (debugging only)"), N_("DIR")},
		{"no-logfile", 0, G_OPTION_FLAG_HIDDEN|G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_no_logfile, N_("Do not create a log file.")},
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	DBusGConnection *gconn;
	struct sigaction sa;

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
	manager = imsettings_manager_new(gconn,
					 arg_xinputrcdir,
					 arg_xinputdir,
					 arg_homedir,
					 arg_replace,
					 !arg_no_logfile);
	if (manager == NULL) {
		g_print("Failed to create an instance for the settings daemon.\n");
		exit(1);
	}

	sa.sa_handler = _sig_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	manager->old_log_handler = g_log_set_default_handler(_log_handler, manager);

	imsettings_manager_stop_monitor(manager);
	imsettings_manager_start_monitor(manager);

	g_signal_connect(manager, "disconnected",
			 G_CALLBACK (disconnected_cb),
			 NULL);
	g_signal_connect(manager, "reload",
			 G_CALLBACK (reload_cb),
			 NULL);

	if (!imsettings_observer_setup(IMSETTINGS_OBSERVER (manager), IMSETTINGS_SERVICE_DBUS)) {
		g_print("Failed to setup the settings daemon.\n");
		exit(1);
	}

	loop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(loop);

	g_print("exiting from the loop\n");

	g_main_loop_unref(loop);
	g_object_unref(manager);
	dbus_g_connection_unref(gconn);

	return 0;
}
