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
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <X11/Xlib.h>
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


struct _IMSettingsManagerClass {
	IMSettingsObserverClass parent_class;
};
struct _IMSettingsManager {
	IMSettingsObserver parent_instance;
};
struct _IMSettingsManagerPrivate {
	IMSettingsRequest *gtk_req;
	IMSettingsRequest *xim_req;
	IMSettingsRequest *qt_req;
	gchar             *display_name;
	DBusConnection    *req_conn;
	GSList            *im_running;
};

enum {
	PROP_0,
	PROP_DISPLAY_NAME,
};

GType        imsettings_manager_get_type               (void) G_GNUC_CONST;
const gchar *imsettings_manager_real_what_im_is_running(IMSettingsObserver  *observer,
							GError             **error);

G_DEFINE_TYPE (IMSettingsManager, imsettings_manager, IMSETTINGS_TYPE_OBSERVER);

/*
 * Private functions
 */
static gchar *
_build_pidfilename(const gchar *base,
		   const gchar *display_name,
		   const gchar *type)
{
	gchar *retval, *hash, *file;

#if 0
	hash = g_compute_checksum_for_string(G_CHECKSUM_MD5, base, -1);
#else
	G_STMT_START {
		gint i;

		hash = g_path_get_basename(base);
		if (strcmp(hash, IMSETTINGS_USER_XINPUT_CONF ".bak") == 0) {
			hash[strlen(IMSETTINGS_USER_XINPUT_CONF)] = 0;
		}
		for (i = 0; hash[i] != 0; i++) {
			if (hash[i] < 0x30 ||
			    (hash[i] >= 0x3a && hash[i] <= 0x3f) ||
			    hash[i] == '\\' ||
			    hash[i] == '`' ||
			    hash[i] >= 0x7b)
				hash[i] = '_';
		}
	} G_STMT_END;
#endif
	file = g_strdup_printf("%s:%s:%s-%s", hash, type, display_name, g_get_user_name());
	retval = g_build_filename(g_get_tmp_dir(), file, NULL);

	g_free(hash);
	g_free(file);

	return retval;
}

static gboolean
_remove_pidfile(const gchar  *pidfile,
		GError      **error)
{
	int save_errno;
	gboolean retval = TRUE;

	if (g_unlink(pidfile) == -1) {
		if (error) {
			save_errno = errno;

			g_set_error(error, G_FILE_ERROR,
				    g_file_error_from_errno(save_errno),
				    _("Failed to remove a pidfile: %s"),
				    pidfile);
		}
		retval = FALSE;
	}

	return retval;
}

static void
_child_setup(gpointer data)
{
	pid_t pid;

	pid = getpid();
	setpgid(pid, 0);
}

static gboolean
_start_process(const gchar  *prog_name,
	       const gchar  *prog_args,
	       const gchar  *pidfile,
	       const gchar  *lang,
	       GError      **error)
{
	int fd;
	gchar *contents = NULL;
	gint tried = 0;

	if (prog_name == NULL || prog_name[0] == 0)
		goto end;
  retry:
	if (tried > 1)
		goto end;
	tried++;
	if ((fd = g_open(pidfile, O_CREAT|O_EXCL|O_WRONLY, S_IRUSR|S_IWUSR)) == -1) {
		gsize len = 0;
		int pid, save_errno = errno;

		if (save_errno != EEXIST) {
			g_set_error(error, G_FILE_ERROR,
				    g_file_error_from_errno(save_errno),
				    _("Failed to open a pidfile: %s"),
				    pidfile);
			goto end;
		}
		if (!g_file_get_contents(pidfile, &contents, &len, error))
			goto end;

		if ((pid = atoi(contents)) == 0) {
			/* maybe invalid pidfile. retry after removing a pidfile. */
			g_warning("failed to get a pid.\n");
			if (!_remove_pidfile(pidfile, error))
				goto end;

			goto retry;
		} else {
			if (kill((pid_t)pid, 0) == -1) {
				g_warning("failed to send a signal.\n");
				/* pid may be invalid. retry after removing a pidfile. */
				if (!_remove_pidfile(pidfile, error))
					goto end;

				goto retry;
			}
		}
		/* the requested IM may be already running */
	} else {
		if (prog_name) {
			gchar *cmd, **argv, **envp = NULL, **env_list;
			static const gchar *env_names[] = {
				"LC_CTYPE",
				NULL
			};
			guint len, i = 0, j = 0;
			GPid pid;

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
			if (g_spawn_async(g_get_tmp_dir(), argv, envp,
					  G_SPAWN_STDOUT_TO_DEV_NULL|
					  G_SPAWN_STDERR_TO_DEV_NULL,
					  _child_setup, NULL, &pid, error)) {
				gchar *s = g_strdup_printf("%d", pid);

				write(fd, s, strlen(s));
				g_free(s);
				close(fd);
			} else {
				close(fd);
				_remove_pidfile(pidfile, NULL);
			}
			g_strfreev(envp);

			g_free(cmd);
			g_strfreev(argv);
			g_strfreev(env_list);
		}
	}
  end:
	g_free(contents);

	return (*error == NULL);
}

static pid_t
_get_pid(const gchar  *pidfile,
	 const gchar  *type,
	 GError      **error)
{
	pid_t pid;
	gchar *contents = NULL;
	gsize len = 0;

	if (!g_file_get_contents(pidfile, &contents, &len, error))
		return 0;

	if ((pid = atoi(contents)) == 0) {
		/* maybe invalid pidfile. */
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_UNABLE_TO_TRACK_IM,
			    _("Couldn't determine the pid for %s process."),
			    type);
	}
	g_free(contents);

	return pid;
}

static gboolean
_stop_process(const gchar  *pidfile,
	      const gchar  *type,
	      GError      **error)
{
	pid_t pid;
	gboolean retval = FALSE;

	pid = _get_pid(pidfile, type, error);
	if (pid == 0) {
		if (g_error_matches(*error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
			/* No pidfile is available. there aren't anything else to do.
			 * basically this is no problem. someone may just did stop an IM
			 * actually not running.
			 */
			g_error_free(*error);
			*error = NULL;
			retval = TRUE;
		}
	} else {
		if (kill(-pid, SIGTERM) == -1) {
			g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_UNABLE_TO_TRACK_IM,
				    _("Couldn't send a signal to the %s process successfully."),
				    type);
		} else {
			_remove_pidfile(pidfile, NULL);
			retval = TRUE;
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
	const gchar *homedir;
	gchar *conffile = NULL, *backfile = NULL, *p = NULL, *n = NULL;
	int save_errno;

	homedir = g_get_home_dir();
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

	return (*error == NULL);
}

static void
disconnected_cb(IMSettingsManager *manager)
{
	GMainLoop *loop = g_object_get_data(G_OBJECT (manager), "imsettings-daemon-main");

	g_main_loop_quit(loop);
}

static void
reload_cb(IMSettingsManager *manager,
	  gboolean           force)
{
	if (force) {
		GMainLoop *loop = g_object_get_data(G_OBJECT (manager), "imsettings-daemon-main");

		g_main_loop_quit(loop);
	}
}


static void
imsettings_manager_real_set_property(GObject      *object,
				     guint         prop_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (object);

	switch (prop_id) {
	    case PROP_DISPLAY_NAME:
		    g_free(priv->display_name);
		    priv->display_name = g_strdup(g_value_get_string(value));
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

	switch (prop_id) {
	    case PROP_DISPLAY_NAME:
		    g_value_set_string(value, priv->display_name);
		    break;
	    default:
		    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		    break;
	}
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
	g_free(priv->display_name);
	if (priv->im_running)
		g_slist_free(priv->im_running);

	if (G_OBJECT_CLASS (imsettings_manager_parent_class)->finalize)
		G_OBJECT_CLASS (imsettings_manager_parent_class)->finalize(object);
}

static gboolean
imsettings_manager_real_start_im(IMSettingsObserver  *imsettings,
				 const gchar         *lang,
				 const gchar         *module,
				 gboolean             update_xinputrc,
				 GError             **error)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (imsettings);
	gchar *imm = NULL, *xinputfile = NULL, *aux_prog = NULL, *aux_args = NULL;
	gchar *xim_prog = NULL, *xim_args = NULL;
	gchar *pidfile = NULL;
	gboolean retval = FALSE;
	IMSettingsRequest *req;
	DBusConnection *conn;

	g_print("Starting %s...\n", module);

	conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	req = imsettings_request_new(conn, IMSETTINGS_INFO_INTERFACE_DBUS);
	imsettings_request_set_locale(req, lang);
	xinputfile = imsettings_request_get_xinput_filename(req, module);
	if (!xinputfile) {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_IM_NOT_FOUND,
			    _("No such input method on your system: %s"),
			    module);
		goto end;
	}
	if (imsettings_request_get_auxiliary_program(req,
						     module,
						     &aux_prog,
						     &aux_args)) {
		pidfile = _build_pidfilename(xinputfile, priv->display_name, "aux");

		/* bring up an auxiliary program */
		if (!_start_process(aux_prog, aux_args, pidfile, lang, error))
			goto end;

		g_free(pidfile);
		pidfile = NULL;
	}

	if (!imsettings_request_get_xim_program(req, module, &xim_prog, &xim_args)) {
		g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_INVALID_IMM,
			    _("No XIM server is available in %s"),
			    module);
		goto end;
	}
	pidfile = _build_pidfilename(xinputfile, priv->display_name, "xim");

	/* bring up a XIM server */
	if (!_start_process(xim_prog, xim_args, pidfile, lang, error))
		goto end;

	/* FIXME: We need to take care of imsettings per X screens?
	 */
	imm = imsettings_request_get_im_module_name(req, module, IMSETTINGS_IMM_GTK);
	imsettings_request_change_to(priv->gtk_req, imm);
	imm = imsettings_request_get_im_module_name(req, module, IMSETTINGS_IMM_XIM);
	imsettings_request_change_to_with_signal(priv->xim_req, imm);
#if 0
	imm = imsettings_request_get_im_module_name(req, module, IMSETTINGS_IMM_QT);
	imsettings_request_change_to(priv->qt_req, imm);
#endif

	/* Finally update a symlink on your home */
	if (update_xinputrc && !_update_symlink(priv, xinputfile, error))
		goto end;

	retval = TRUE;
  end:
	g_object_unref(req);
	dbus_connection_unref(conn);
	g_free(pidfile);
	g_free(xinputfile);
	g_free(aux_prog);
	g_free(aux_args);
	g_free(xim_prog);
	g_free(xim_args);

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
	IMSettingsRequest *req;
	const gchar *homedir;
	gchar *xinputfile = NULL, *pidfile = NULL;
	gboolean retval = FALSE;
	GString *strerr = g_string_new(NULL);
	DBusConnection *conn;

	g_print("Stopping %s...\n", module);

	/* Change the settings before killing the IM process(es) */
	/* FIXME: We need to take care of imsettings per X screens?
	 */
	imsettings_request_change_to(priv->gtk_req, "");
	imsettings_request_change_to_with_signal(priv->xim_req, "none");
#if 0
	imsettings_request_change_to(priv->qt_req, NULL);
#endif

	conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	req = imsettings_request_new(conn, IMSETTINGS_INFO_INTERFACE_DBUS);
	xinputfile = imsettings_request_get_xinput_filename(req, module);
	if (!xinputfile)
		goto end;

	pidfile = _build_pidfilename(xinputfile, priv->display_name, "aux");
	/* kill an auxiliary program */
	if (!_stop_process(pidfile, "aux", error)) {
		if (force) {
			g_string_append_printf(strerr, "%s\n", (*error)->message);
			g_error_free(*error);
			*error = NULL;
			_remove_pidfile(pidfile, NULL);
		} else {
			goto end;
		}
	}
	g_free(pidfile);

	pidfile = _build_pidfilename(xinputfile, priv->display_name, "xim");
	/* kill a XIM server */
	if (!_stop_process(pidfile, "xim", error)) {
		if (force) {
			g_string_append_printf(strerr, "%s\n", (*error)->message);
			g_error_free(*error);
			*error = NULL;
			_remove_pidfile(pidfile, NULL);
		} else {
			goto end;
		}
	}

	/* finally update .xinputrc */
	homedir = g_get_home_dir();
	if (homedir == NULL) {
		g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			    _("Failed to get a place of home directory."));
		goto end;
	}
	if (imsettings_request_is_user_default(req, module)) {
		g_free(xinputfile);
		xinputfile = g_build_filename(XINPUT_PATH, IMSETTINGS_NONE_CONF XINPUT_SUFFIX, NULL);
		if (update_xinputrc && !_update_symlink(priv, xinputfile, error))
			goto end;
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
  end:
	g_free(pidfile);
	g_free(xinputfile);
	g_string_free(strerr, TRUE);
	g_object_unref(req);
	dbus_connection_unref(conn);

	return retval;
}

const gchar *
imsettings_manager_real_what_im_is_running(IMSettingsObserver  *observer,
					   GError             **error)
{
	IMSettingsManagerPrivate *priv = IMSETTINGS_MANAGER_GET_PRIVATE (observer);
	IMSettingsRequest *req;
	DBusConnection *conn;
	gchar *module, *xinputfile = NULL, *pidfile = NULL;
	pid_t pid;

	conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	req = imsettings_request_new(conn, IMSETTINGS_INFO_INTERFACE_DBUS);
	module = imsettings_request_get_current_user_im(req);
	if (module) {
		xinputfile = imsettings_request_get_xinput_filename(req, module);
		if (!xinputfile) {
			g_set_error(error, IMSETTINGS_GERROR, IMSETTINGS_GERROR_IM_NOT_FOUND,
				    _("No such input method on your system: %s"),
				    module);
			goto end;
		}
		pidfile = _build_pidfilename(xinputfile, priv->display_name, "xim");
		pid = _get_pid(pidfile, "xim", error);
		if (pid == 0) {
			g_free(module);
			module = NULL;
		} else {
			if (kill(pid, 0) == -1) {
				g_free(module);
				module = NULL;
			}
		}
	}
  end:
	g_free(xinputfile);
	g_free(pidfile);
	g_object_unref(req);
	dbus_connection_unref(conn);

	return module;
}

static void
imsettings_manager_class_init(IMSettingsManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	IMSettingsObserverClass *observer_class = IMSETTINGS_OBSERVER_CLASS (klass);

	g_type_class_add_private(klass, sizeof (IMSettingsManagerPrivate));

	object_class->set_property = imsettings_manager_real_set_property;
	object_class->get_property = imsettings_manager_real_get_property;
	object_class->finalize     = imsettings_manager_real_finalize;

	observer_class->start_im           = imsettings_manager_real_start_im;
	observer_class->stop_im            = imsettings_manager_real_stop_im;
	observer_class->what_im_is_running = imsettings_manager_real_what_im_is_running;

	/* properties */
	g_object_class_install_property(object_class, PROP_DISPLAY_NAME,
					g_param_spec_string("display_name",
							    _("X display name"),
							    _("X display name to use"),
							    NULL,
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
}

static IMSettingsManager *
imsettings_manager_new(DBusGConnection *connection,
		       gboolean         replace)
{
	g_return_val_if_fail (connection != NULL, NULL);

	return IMSETTINGS_MANAGER (g_object_new(IMSETTINGS_TYPE_MANAGER,
						"replace", replace,
						"connection", connection,
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
	IMSettingsManager *manager;
	gboolean arg_replace = FALSE;
	gchar *arg_display_name = NULL, *display_name = NULL;
	GOptionContext *ctx = g_option_context_new(NULL);
	GOptionEntry entries[] = {
		{"replace", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_NONE, &arg_replace, N_("Replace the running settings daemon with new instance."), NULL},
		{"display", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &arg_display_name, N_("X display to use"), N_("DISPLAY")},
#if 0
		/* FIXME */
		{"screen", 0, 0, G_OPTION_ARG_INT, &arg_screen, N_("X screen to use"), N_("SCREEN")},
#endif
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	DBusGConnection *gconn;
	Display *display;

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

	display = XOpenDisplay(arg_display_name);
	if (display == NULL) {
		g_printerr(_("Failed to open a X display."));
		exit(1);
	}
	if (display_name == NULL)
		display_name = g_strdup(DisplayString(display));

	XCloseDisplay(display);

	gconn = dbus_g_bus_get(DBUS_BUS_SESSION, NULL);
	manager = imsettings_manager_new(gconn, arg_replace);
	if (manager == NULL) {
		g_print("Failed to create an instance for the settings daemon.\n");
		exit(1);
	}
	g_object_set(G_OBJECT (manager), "display_name", display_name, NULL);

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
	g_object_set_data(G_OBJECT (manager), "imsettings-daemon-main", loop);

	g_main_loop_run(loop);

	g_print("exiting from the loop\n");

	g_free(display_name);
	g_object_unref(manager);
	dbus_g_connection_unref(gconn);

	return 0;
}
