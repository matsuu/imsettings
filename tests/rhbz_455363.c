/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * rhbz_455363.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-request.h"
#include "main.h"

DBusConnection *dbus_conn;
IMSettingsRequest *req;

/************************************************************/
/* common functions                                         */
/************************************************************/
void
setup(void)
{
	dbus_conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	req = imsettings_request_new(dbus_conn, IMSETTINGS_INFO_INTERFACE_DBUS);
}

void
teardown(void)
{
	imsettings_test_reload_daemons();

	g_object_unref(req);
	dbus_connection_unref(dbus_conn);
}

/************************************************************/
/* Test cases                                               */
/************************************************************/
TDEF (issue) {
	gchar *sim = NULL;
	gchar *p;
	GError *error = NULL;
	gchar *d, *tmpl = g_build_filename(g_get_tmp_dir(), "rhbz_455363.XXXXXX", NULL);
	gchar *dest = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "rhbz_455363", "case1", NULL);
	gchar *xinputd, *xinputrc;
	gchar *conf;
	GPtrArray *list;
	IMSettingsInfo *info;
	gint i;
	FILE *fp;

	d = mkdtemp(tmpl);
	fail_unless(d != NULL, "Unable to create a temporary directory.");

	p = g_strdup_printf("cp -a %s %s", dest, d);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();
	g_free(p);

	xinputrc = g_build_filename(d, "case1", NULL);
	xinputd = g_build_filename(xinputrc, "xinput.d", NULL);
	conf = g_build_filename(xinputd, "test-scim2.conf", NULL);

	imsettings_test_restart_daemons_full(xinputrc, xinputd, tmpl);

	g_usleep(1 * G_USEC_PER_SEC);

	list = imsettings_request_get_info_objects(req, &error);
	fail_unless(list != NULL, "Unable to get the IM objects");
	for (i = 0; i < list->len; i++) {
		info = g_ptr_array_index(list, i);
		if (imsettings_info_is_system_default(info)) {
			fail_unless(sim == NULL, "Duplicate status for the system default");
			sim = g_strdup(imsettings_info_get_short_desc(info));
		}
		g_object_unref(info);
	}
	g_ptr_array_free(list, TRUE);
	fail_unless(sim != NULL, "No default system IM");
	fail_unless(strcmp(sim, "S C I M") == 0, "Unexpected default IM for system");
	g_free(sim);

	fp = fopen(conf, "a");
	fail_unless(fp != NULL, "Unable to open a file.");

	fputs("# foo\n", fp);
	fclose(fp);

	g_usleep(1 * G_USEC_PER_SEC);

	list = imsettings_request_get_info_objects(req, &error);
	fail_unless(list != NULL, "Unable to get the IM objects");
	for (i = 0; i < list->len; i++) {
		info = g_ptr_array_index(list, i);
		if (imsettings_info_is_system_default(info)) {
			fail_unless(sim == NULL, "Duplicate status for the system default");
			sim = g_strdup(imsettings_info_get_short_desc(info));
		}
		g_object_unref(info);
	}
	g_ptr_array_free(list, TRUE);
	fail_unless(sim != NULL, "No default system IM");
	fail_unless(strcmp(sim, "S C I M") == 0, "Unexpected default IM for system");

	p = g_strdup_printf("rm -rf %s", tmpl);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();

	g_free(xinputrc);
	g_free(xinputd);
	g_free(conf);
	g_free(sim);
	g_free(p);
	g_free(tmpl);
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Red Hat Bugzilla");
	TCase *tc = tcase_create("Bug#455363: https://bugzilla.redhat.com/show_bug.cgi?id=455363");

	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_set_timeout(tc, 10);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
