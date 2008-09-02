/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gcit_13.c
 * Copyright (C) 2008 Akira TAGOH
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
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-info.h"
#include "imsettings/imsettings-request.h"
#include "main.h"

DBusConnection *conn;
IMSettingsRequest *req;

/************************************************************/
/* common functions                                         */
/************************************************************/
void
setup(void)
{
	conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	req = imsettings_request_new(conn, IMSETTINGS_INFO_INTERFACE_DBUS);
}

void
teardown(void)
{
//	imsettings_test_reload_daemons();

	g_object_unref(req);
	dbus_connection_unref(conn);
}

/************************************************************/
/* Test cases                                               */
/************************************************************/
TDEF (issue) {
	gchar **list;
	GError *error = NULL;
	gint i;
	gboolean flag = FALSE;
	gchar *d, *tmpl, *dest, *xinputd, *xinputrc, *confdir, *p;

	imsettings_test_restart_daemons("issue_13" G_DIR_SEPARATOR_S "case1");
	g_usleep(2 * G_USEC_PER_SEC);

	imsettings_request_set_locale(req, "ja_JP.UTF-8");
	list = imsettings_request_get_im_list(req, &error);
	fail_unless(list != NULL, "Unable to get the IM list (take 1)");
	for (i = 0; list[i] != NULL; i++) {
		g_print("%d. %s\n", i+1, list[i]);
		if (strcmp(list[i], "kinput2") == 0) {
			flag = TRUE;
			break;
		}
	}
	fail_unless(flag, "Unable to find out legacy IM (take 1).");

	g_strfreev(list);

	/* testing other locale */
	flag = FALSE;
	imsettings_request_set_locale(req, "en_US.UTF-8");
	list = imsettings_request_get_im_list(req, &error);
	fail_unless(list != NULL, "Unable to get the IM list (take 2)");
	for (i = 0; list[i] != NULL; i++) {
		g_print("%d. %s\n", i+1, list[i]);
		if (strcmp(list[i], "kinput2") == 0) {
			flag = TRUE;
			break;
		}
	}
	fail_unless(!flag, "kinput2 is only available for ja_JP locale (take 2)");

	g_strfreev(list);

	/* test case 2 */
	imsettings_test_restart_daemons("issue_13" G_DIR_SEPARATOR_S "case2");
	g_usleep(2 * G_USEC_PER_SEC);

	flag = FALSE;
	imsettings_request_set_locale(req, "ja_JP.UTF-8");
	list = imsettings_request_get_im_list(req, &error);
	fail_unless(list != NULL, "Unable to get the IM list (take 3)");
	for (i = 0; list[i] != NULL; i++) {
		g_print("%d. %s\n", i+1, list[i]);
		if (strcmp(list[i], "kinput2") == 0) {
			flag = TRUE;
			break;
		}
	}
	fail_unless(flag, "Unable to find out legacy IM (take 3).");

	g_strfreev(list);

	/* testing other locale */
	flag = FALSE;
	imsettings_request_set_locale(req, "en_US.UTF-8");
	list = imsettings_request_get_im_list(req, &error);
	fail_unless(list != NULL, "Unable to get the IM list (take 4)");
	for (i = 0; list[i] != NULL; i++) {
		g_print("%d. %s\n", i+1, list[i]);
		if (strcmp(list[i], "kinput2") == 0) {
			flag = TRUE;
			break;
		}
	}
	fail_unless(!flag, "kinput2 is only available for ja_JP locale (take 4)");

	g_strfreev(list);

	/* test case 3 */
	tmpl = g_build_filename(g_get_tmp_dir(), "gcit_13.XXXXXX", NULL);
	confdir = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "issue_13", "case2", "xinput.d", NULL);
	dest = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "issue_13", "case3", NULL);
	d = mkdtemp(tmpl);
	fail_unless(d != NULL, "Unable to create a temporary directory.");

	p = g_strdup_printf("cp -a %s %s", dest, d);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();
	g_free(p);

	xinputrc = g_build_filename(d, "case3", NULL);
	xinputd = g_build_filename(xinputrc, "xinput.d", NULL);

	imsettings_test_restart_daemons_full(xinputrc, NULL, NULL);
	g_usleep(2 * G_USEC_PER_SEC);

	p = g_strdup_printf("cp %s%s %s", confdir, G_DIR_SEPARATOR_S "kinput2-canna", xinputd);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();
	g_free(p);
	p = g_strdup_printf("ln -s kinput2-canna %s%sja_JP", xinputd, G_DIR_SEPARATOR_S);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();
	g_free(p);

	g_usleep(5 * G_USEC_PER_SEC);

	flag = FALSE;
	imsettings_request_set_locale(req, "ja_JP.UTF-8");
	list = imsettings_request_get_im_list(req, &error);
	fail_unless(list != NULL, "Unable to get the IM list (take 5)");
	for (i = 0; list[i] != NULL; i++) {
		g_print("%d. %s\n", i+1, list[i]);
		if (strcmp(list[i], "kinput2") == 0) {
			flag = TRUE;
			break;
		}
	}
	fail_unless(flag, "Unable to find out legacy IM (take 5).");

	g_strfreev(list);

	/* testing other locale */
	flag = FALSE;
	imsettings_request_set_locale(req, "en_US.UTF-8");
	list = imsettings_request_get_im_list(req, &error);
	fail_unless(list != NULL, "Unable to get the IM list (take 6)");
	for (i = 0; list[i] != NULL; i++) {
		g_print("%d. %s\n", i+1, list[i]);
		if (strcmp(list[i], "kinput2") == 0) {
			flag = TRUE;
			break;
		}
	}
	fail_unless(!flag, "kinput2 is only available for ja_JP locale (take 6)");

	g_strfreev(list);

	p = g_strdup_printf("rm -rf %s", tmpl);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();
	g_free(p);

	g_free(xinputrc);
	g_free(xinputd);
	g_free(confdir);
	g_free(dest);
	g_free(tmpl);
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Google Code Issue Tracker");
	TCase *tc = tcase_create("Issue #13: http://code.google.com/p/imsettings/issues/detail?id=13");

	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_set_timeout(tc, 20);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
