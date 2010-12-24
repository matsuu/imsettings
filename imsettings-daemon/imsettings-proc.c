/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-proc.c
 * Copyright (C) 2008-2010 Red Hat, Inc. All rights reserved.
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

#include <sys/wait.h>
#include <unistd.h>
#include <glib/gi18n-lib.h>
#include "imsettings-utils.h"
#include "imsettings-info.h"
#include "imsettings-proc.h"

#define IMSETTINGS_PROC_GET_PRIVATE(_o_)	(G_TYPE_INSTANCE_GET_PRIVATE ((_o_), IMSETTINGS_TYPE_PROC, IMSettingsProcPrivate))

#define RESTART_COUNTER_THRESHOLD 5
#define MAXRESTART 3

typedef struct _IMSettingsProcInfo {
	GIOChannel     *stdout;
	GIOChannel     *stderr;
	GPid            pid;
	GTimeVal        started_time;
	guint           id;
	guint           oid;
	guint           eid;
	guint           restart_counter;
} IMSettingsProcInfo;
struct _IMSettingsProcPrivate {
	IMSettingsInfo     *info;
	IMSettingsProcInfo  main;
	IMSettingsProcInfo  aux;
	gboolean            shutdown:1;
};
enum {
	PROP_0,
	PROP_INFO,
	LAST_PROP
};

static gboolean _start_main_process(IMSettingsProc      *proc,
				    GError             **error);
static gboolean _start_aux_process (IMSettingsProc      *proc,
				    GError             **error);
static gboolean _start_process     (IMSettingsProc     *proc,
				    IMSettingsProcInfo *pinfo,
				    const gchar        *type_name,
				    const gchar        *prog_name,
				    const gchar        *prog_args,
				    GError            **error);
static gboolean _stop_process      (IMSettingsProc     *proc,
				    IMSettingsProcInfo *pinfo,
				    const gchar        *type_name,
				    const gchar        *prog_name,
				    GError            **error);

G_DEFINE_TYPE (IMSettingsProc, imsettings_proc, G_TYPE_OBJECT);

/*< private >*/
static gboolean
_log_write_cb(GIOChannel   *channel,
	      GIOCondition  condition,
	      gpointer      data)
{
	IMSettingsProc *proc = IMSETTINGS_PROC (data);
	IMSettingsProcPrivate *priv = proc->priv;
	gchar *buffer = NULL;
	gsize len;
	GError *err = NULL;
	GString *str;
	const gchar *module = imsettings_info_get_short_desc(priv->info);

	g_io_channel_read_line(channel, &buffer, &len, NULL, &err);
	if (err) {
		g_warning("Failed to read the log from IM: %s", err->message);
		g_error_free(err);

		return FALSE;
	}
	str = g_string_new(NULL);
	g_string_append_printf(str, "%s[%lu]: %s", module, (gulong)priv->main.pid, buffer);
	g_message(str->str);
	g_string_free(str, TRUE);
	g_free(buffer);

	return TRUE;
}

static void
_child_setup(gpointer data)
{
	pid_t pid;

	pid = getpid();
	setpgid(pid, 0);
}

static void
_watch_im_status_cb(GPid     pid,
		    gint     status,
		    gpointer data)
{
	IMSettingsProc *proc = IMSETTINGS_PROC (data);
	IMSettingsProcPrivate *priv = proc->priv;
	IMSettingsProcInfo *pinfo;
	GString *status_message = g_string_new(NULL);
	gboolean unref = FALSE;
	GTimeVal current;
	const gchar *module = imsettings_info_get_short_desc(priv->info);
	const gchar *type_names[] = {
		"main", "auxiliary"
	};
	gint type;

	if (priv->main.pid == pid) {
		pinfo = &priv->main;
		type = 0;
	} else if (priv->aux.pid == pid) {
		pinfo = &priv->aux;
		type = 1;
	} else {
		g_warning("received the process status of the unknown pid: %d", pid);
		goto finalize;
	}
	pinfo->pid = -1;
	if (WIFSTOPPED (status)) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		      "the %s process [pid: %d] for %s stopped with the signal %d",
		      type_names[type],
		      pid, module, WSTOPSIG (status));
		goto finalize;
	} else if (WIFCONTINUED (status)) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		      "the %s process [pid: %d] for %s continued",
		      type_names[type],
		      pid, module);
		goto finalize;
	} else if (WIFEXITED (status)) {
		g_string_append_printf(status_message, "the status %d",
				       WEXITSTATUS (status));
		if (WEXITSTATUS (status) == 0) {
			/* don't restart the process.
			 * the process died intentionally.
			 */
			unref = TRUE;
		}
	} else if (WIFSIGNALED (status)) {
		g_string_append_printf(status_message, "the signal %d",
				       WTERMSIG (status));
		if (WCOREDUMP (status)) {
			g_string_append(status_message, " (core dumped)");
		}
	} else {
		g_string_append_printf(status_message, "the unknown status %d",
				       status);
	}

	if (priv->shutdown || unref) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		      "Stopped %s process for %s with %s [pid: %d]",
		      type_names[type], module, status_message->str, pid);
	} else {
		g_get_current_time(&current);
		/* XXX: This is Y2038 unsafe */
		if (current.tv_sec - pinfo->started_time.tv_sec > RESTART_COUNTER_THRESHOLD) {
			pinfo->restart_counter = 0;
		} else {
			pinfo->restart_counter++;
		}
		if (pinfo->restart_counter >= MAXRESTART) {
			gchar *message = g_strdup_printf(N_("Giving up to bring the process up because %s Input Method process for %s rapidly died many times. See .imsettings.log for more details."),
							 type_names[type],
							 module);

			g_critical(message);
			/* notify */
			g_free(message);
			unref = TRUE;
		} else {
			GError *err = NULL;

			g_warning("%s Input Method process for %s died with %s, but unexpectedly. restarting...", type_names[type], module, status_message->str);
			switch (type) {
			    case 0:
				    _start_main_process(proc, &err);
				    break;
			    case 1:
				    _start_aux_process(proc, &err);
				    break;
			    default:
				    break;
			}
			if (err) {
				/* notify */
				g_error_free(err);
			}
		}
	}

  finalize:
	g_string_free(status_message, TRUE);
	if (unref) {
		g_spawn_close_pid(pid);
		g_object_unref(proc);
	}
}

static gboolean
_start_main_process(IMSettingsProc  *proc,
		    GError         **error)
{
	IMSettingsProcPrivate *priv = proc->priv;
	const gchar *prog, *args;

	if (imsettings_info_is_immodule_only(priv->info))
		return TRUE;

	prog = imsettings_info_get_xim_program(priv->info);
	args = imsettings_info_get_xim_args(priv->info);

	return _start_process(proc, &priv->main,
			      "main",
			      prog, args, error);
}

static gboolean
_start_aux_process(IMSettingsProc  *proc,
		   GError         **error)
{
	IMSettingsProcPrivate *priv = proc->priv;
	const gchar *prog, *args;

	prog = imsettings_info_get_aux_program(priv->info);
	args = imsettings_info_get_aux_args(priv->info);

	return _start_process(proc, &priv->aux,
			      "auxiliary",
			      prog, args, error);
}

static gboolean
_start_process(IMSettingsProc     *proc,
	       IMSettingsProcInfo *pinfo,
	       const gchar        *type_name,
	       const gchar        *prog_name,
	       const gchar        *prog_args,
	       GError            **error)
{
	IMSettingsProcPrivate *priv = proc->priv;
	gboolean retval = TRUE;
	const gchar *module = imsettings_info_get_short_desc(priv->info);

	if (pinfo->pid > 0) {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_UNKNOWN,
			    _("[BUG] %s process is still running [pid: %d]\n"),
			    type_name, pinfo->pid);
		g_warning((*error)->message);
		return FALSE;
	}
	if (prog_name != NULL && prog_name[0] != 0) {
		gchar *cmd, **argv, **envp = NULL, **env_list, *time;
		static const gchar *env_names[] = {
			"LC_CTYPE",
			NULL
		};
		gsize len, i, j = 0;
		GPid pid;
		gint ofd, efd;
		const gchar *lang = imsettings_info_get_language(priv->info);

		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		      "  Starting the %s process for %s [lang:%s]",
		      type_name, module, lang);

		env_list = g_listenv();
		len = g_strv_length(env_list);
		envp = g_new0(gchar *, G_N_ELEMENTS (env_names) + len + 1);
		for (i = 0; i < len; i++) {
			envp[i] = g_strdup_printf("%s=%s",
						  env_list[i],
						  g_getenv(env_list[i]));
		}
		if (lang) {
			envp[i + j] = g_strdup_printf("%s=%s",
						      env_names[j], lang);
			j++;
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
			pinfo->pid = pid;
			pinfo->stdout = g_io_channel_unix_new(ofd);
			pinfo->stderr = g_io_channel_unix_new(efd);
			g_io_channel_set_close_on_unref(pinfo->stdout, TRUE);
			g_io_channel_set_close_on_unref(pinfo->stderr, TRUE);
			pinfo->oid = g_io_add_watch(pinfo->stdout, G_IO_IN, _log_write_cb, proc);
			pinfo->eid = g_io_add_watch(pinfo->stderr, G_IO_IN, _log_write_cb, proc);
			g_get_current_time(&pinfo->started_time);
			pinfo->id = g_child_watch_add(pid, _watch_im_status_cb, proc);

			time = g_time_val_to_iso8601(&pinfo->started_time);

			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
			      "  Started %s: [process: %s %s, lang: %s, pid: %d, id: %d, time: %s",
			      module, prog_name, prog_args ? prog_args : "", lang, pid, pinfo->id, time);

			g_free(time);
		} else {
			g_warning("  Failed to spawn: %s %s [lang=%s]",
				  prog_name, prog_args ? prog_args : "", lang);
			retval = FALSE;
		}
		g_strfreev(envp);
		g_free(cmd);
		g_strfreev(argv);
		g_strfreev(env_list);
	} else {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		      "  no need to invoke any %s process for %s", type_name, module);
	}

	return retval;
}

static gboolean
_stop_main_process(IMSettingsProc  *proc,
		   GError         **error)
{
	IMSettingsProcPrivate *priv = proc->priv;
	const gchar *prog;

	prog = imsettings_info_get_xim_program(priv->info);

	return _stop_process(proc, &priv->main,
			     "main",
			     prog, error);
}

static gboolean
_stop_aux_process(IMSettingsProc  *proc,
		  GError         **error)
{
	IMSettingsProcPrivate *priv = proc->priv;
	const gchar *prog;

	prog = imsettings_info_get_aux_program(priv->info);

	return _stop_process(proc, &priv->aux,
			     "auxiliary",
			     prog, error);
}

static gboolean
_stop_process(IMSettingsProc     *proc,
	      IMSettingsProcInfo *pinfo,
	      const gchar        *type_name,
	      const gchar        *prog_name,
	      GError            **error)
{
	IMSettingsProcPrivate *priv = proc->priv;
	gboolean retval = TRUE;

	if (pinfo->pid > 0) {
		const gchar *module = imsettings_info_get_short_desc(priv->info);

		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
		      "  Stopping the %s process for %s [pid: %d]",
		      type_name, module, pinfo->pid);

		if (kill(-pinfo->pid, SIGTERM) == -1) {
			g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_UNABLE_TO_TRACK_IM,
				    _("Couldn't send a signal to the %s process successfully."),
				    type_name);
			g_warning("  %s", (*error)->message);
			retval = FALSE;
		} else {
			GTimeVal time;
			gchar *s;

			g_get_current_time(&time);
			s = g_time_val_to_iso8601(&time);
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
			      "Sent a signal to stop %s [pid: %d, time: %s]",
			      prog_name, pinfo->pid, s);
			g_free(s);
		}
	}

	return retval;
}

static void
imsettings_proc_set_property(GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	IMSettingsProc *proc = IMSETTINGS_PROC (object);
	IMSettingsProcPrivate *priv = proc->priv;

	switch (prop_id) {
	    case PROP_INFO:
		    priv->info = g_value_dup_object(value);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_proc_get_property(GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	IMSettingsProc *proc = IMSETTINGS_PROC (object);
	IMSettingsProcPrivate *priv = proc->priv;

	switch (prop_id) {
	    case PROP_INFO:
		    g_value_set_object(value, priv->info);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
}

static void
imsettings_proc_info_finalize(IMSettingsProcInfo *pinfo)
{
	if (pinfo->stdout)
		g_io_channel_unref(pinfo->stdout);
	if (pinfo->stderr)
		g_io_channel_unref(pinfo->stderr);
	if (pinfo->oid > 0)
		g_source_remove(pinfo->oid);
	if (pinfo->eid > 0)
		g_source_remove(pinfo->eid);
	if (pinfo->id > 0)
		g_source_remove(pinfo->id);
}

static void
imsettings_proc_finalize(GObject *object)
{
	IMSettingsProc *proc = IMSETTINGS_PROC (object);
	IMSettingsProcPrivate *priv = proc->priv;

	imsettings_proc_kill(proc, NULL);

	imsettings_proc_info_finalize(&priv->aux);
	imsettings_proc_info_finalize(&priv->main);
	g_object_unref(priv->info);

	if (G_OBJECT_CLASS (imsettings_proc_parent_class)->finalize)
		G_OBJECT_CLASS (imsettings_proc_parent_class)->finalize(object);
}

static void
imsettings_proc_class_init(IMSettingsProcClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private(klass, sizeof (IMSettingsProcPrivate));

	object_class->set_property = imsettings_proc_set_property;
	object_class->get_property = imsettings_proc_get_property;
	object_class->finalize = imsettings_proc_finalize;

	/* properties */

	g_object_class_install_property(object_class, PROP_INFO,
					g_param_spec_object("imsettings-info",
							    _("IMSettingsInfo"),
							    _("A GObject to be a IMSettingsInfo"),
							    IMSETTINGS_TYPE_INFO,
							    G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
}

static void
imsettings_proc_init(IMSettingsProc *proc)
{
	IMSettingsProcPrivate *priv;

	priv = proc->priv = IMSETTINGS_PROC_GET_PRIVATE (proc);
}

/*< public >*/

/**
 * imsettings_proc_new:
 * @info:
 *
 * FIXME
 *
 * Returns:
 */
IMSettingsProc *
imsettings_proc_new(IMSettingsInfo *info)
{
	g_return_val_if_fail (IMSETTINGS_IS_INFO (info), NULL);

	return IMSETTINGS_PROC (g_object_new(IMSETTINGS_TYPE_PROC,
					     "imsettings-info", info,
					     NULL));
}

/**
 * imsettings_proc_spawn:
 * @proc:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gboolean
imsettings_proc_spawn(IMSettingsProc  *proc,
		      GError         **error)
{
	g_return_val_if_fail (IMSETTINGS_IS_PROC (proc), FALSE);

	if (!_start_aux_process(proc, error))
		return FALSE;
	if (!_start_main_process(proc, error))
		return FALSE;

	return TRUE;
}

/**
 * imsettings_proc_kill:
 * @proc:
 * @error:
 *
 * FIXME
 *
 * Returns:
 */
gboolean
imsettings_proc_kill(IMSettingsProc  *proc,
		     GError         **error)
{
	IMSettingsProcPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_PROC (proc), FALSE);

	priv = proc->priv;
	priv->shutdown = TRUE;

	if (!_stop_aux_process(proc, error))
		return FALSE;
	if (!_stop_main_process(proc, error))
		return FALSE;

	return TRUE;
}

/**
 * imsettings_proc_is_alive:
 * @proc:
 *
 * FIXME
 *
 * Returns:
 */
gboolean
imsettings_proc_is_alive(IMSettingsProc *proc)
{
	IMSettingsProcPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_PROC (proc), FALSE);

	priv = proc->priv;

	return imsettings_info_is_immodule_only(priv->info) || priv->main.pid > 0;
}

/**
 * imsettings_proc_info:
 * @proc:
 *
 * FIXME
 *
 * Returns:
 */
IMSettingsInfo *
imsettings_proc_info(IMSettingsProc *proc)
{
	IMSettingsProcPrivate *priv;

	g_return_val_if_fail (IMSETTINGS_IS_PROC (proc), NULL);

	priv = proc->priv;

	return g_object_ref(priv->info);
}
