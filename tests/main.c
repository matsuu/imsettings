/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * main.c
 * Copyright (C) 2007-2009 Akira TAGOH
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
#include <unistd.h>
#include <check.h>
#include <glib-object.h>
#include "main.h"

extern Suite *imsettings_suite(void);

static GLogFunc old_logger = NULL;
static GError *error = NULL;

/*
 * Private functions
 */
static void
logger(const gchar    *log_domain,
       GLogLevelFlags  log_level,
       const gchar    *message,
       gpointer        user_data)
{
	GError *err = NULL;
	gchar *prev = NULL;

	if (error) {
		prev = g_strdup_printf("\n    %s", error->message);
		g_clear_error(&error);
	}
	g_set_error(&err, IMSETTINGS_TEST_ERROR, log_level,
		    "%s%s", message,
		    (prev ? prev : ""));
	error = err;
	g_free(prev);
}

static void
init(void)
{
	g_type_init();

	old_logger = g_log_set_default_handler(logger, NULL);
}

static void
fini(void)
{
	if (old_logger)
		g_log_set_default_handler(old_logger, NULL);
}

/*
 * Public functions
 */
GQuark
imsettings_test_get_error_quark(void)
{
	GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string("imsettings-test-error");

	return quark;
}

gchar *
imsettings_test_pop_error(void)
{
	gchar *retval = NULL;

	if (error) {
		retval = g_strdup(error->message);
		g_clear_error(&error);
	}

	return retval;
}

void
imsettings_test_restart_daemons_full(const gchar *xinputrcdir,
				     const gchar *xinputdir,
				     const gchar *homedir)
{
	gchar *xxinputrcdir, *xxinputdir, *xhomedir;
	gchar *p = g_build_filename(IMSETTINGS_BUILDDIR, "src", "im-settings-daemon", NULL);
	gchar *s;

	if (xinputrcdir == NULL)
		abort();
	if (xinputrcdir[0] == G_DIR_SEPARATOR ||
	    (((xinputrcdir[0] >= 'a' && xinputrcdir[0] <= 'z') ||
	      (xinputrcdir[0] >= 'A' && xinputrcdir[0] <= 'Z')) &&
	     xinputrcdir[1] == ':' &&
	     xinputrcdir[2] == G_DIR_SEPARATOR)) {
		/* possibly may be an absolute path */
		xxinputrcdir = g_strdup(xinputrcdir);
	} else {
		xxinputrcdir = g_build_filename(IMSETTINGS_SRCDIR, "testcases", xinputrcdir, NULL);
	}
	if (xinputdir == NULL)
		xxinputdir = g_build_filename(xxinputrcdir, "xinput.d", NULL);
	else
		xxinputdir = g_strdup(xinputdir);
	if (homedir == NULL)
		xhomedir = g_strdup(xxinputrcdir);
	else
		xhomedir = g_strdup(homedir);

	s = g_strdup_printf("%s --replace --xinputrcdir=%s --xinputdir=%s --homedir=%s",
			    p, xxinputrcdir, xxinputdir, xhomedir);
	g_print("%s\n", s);

	/* stop all the processes first */
	imsettings_test_reload_daemons();

	sleep(1);

	if (!g_spawn_command_line_async(s, NULL))
		abort();
	g_free(s);
	g_free(p);
	g_free(xxinputrcdir);
	g_free(xxinputdir);
	g_free(xhomedir);

	/* FIXME! */
	sleep(1);
}

void
imsettings_test_restart_daemons(const gchar *subdir)
{
	imsettings_test_restart_daemons_full(subdir, NULL, NULL);
}

void
imsettings_test_reload_daemons(void)
{
	gchar *p = g_build_filename(IMSETTINGS_BUILDDIR, "utils", "imsettings-reload", NULL);
	gchar *s = g_strdup_printf("%s -f", p);

	if (!g_spawn_command_line_async(s, NULL))
		abort();

	g_free(s);
	g_free(p);
}

int
main(void)
{
	int number_failed;
	Suite *s = imsettings_suite();
	SRunner *sr = srunner_create(s);

	init();

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	fini();

	return (number_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
