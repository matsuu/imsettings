/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * rhbz_523349.c
 * Copyright (C) 2009 Red Hat, Inc. All rights reserved.
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
#include "imsettings/imsettings-info.h"
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
	req = imsettings_request_new(dbus_conn, IMSETTINGS_INTERFACE_DBUS);
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
	gchar *p;
	GError *error = NULL;
	gchar *d, *tmpl = g_build_filename(g_get_tmp_dir(), "rhbz_523349.XXXXXX", NULL);
	gchar *dest = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "rhbz_523349", "case1", NULL);
	gchar *xinputrc;
	const gchar *v;
	IMSettingsInfo *info;
	FILE *fp;

	d = mkdtemp(tmpl);
	fail_unless(d != NULL, "Unable to create a temporary directory.");

	p = g_strdup_printf("cp -a %s %s", dest, d);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();
	g_free(p);

	xinputrc = g_build_filename(d, "case1", NULL);

	imsettings_test_restart_daemons_full(xinputrc, NULL, xinputrc);

	g_usleep(5 * G_USEC_PER_SEC);

	info = imsettings_request_get_info_object(req, "SCIM", &error);
	fail_unless(info != NULL, "Unable to obtain IMSettingsInfo for SCIM");
	fail_unless(imsettings_info_is_script(info), "Invalid testcase or just failed to determine if the conf file is a script file");
	v = imsettings_info_get_gtkimm(info);
	fail_unless(v != NULL, "Unable to obtain gtk immodule from IMSettingsInfo");
	fail_unless(strcmp("xim", v) == 0, "Unexpected value: %s but %s", v, "xim");
	p = g_build_filename(xinputrc, "xinput.d", "stamp", NULL);
	fp = fopen(p, "w");
	fail_unless(fp != NULL, "Unable to open a file.");
	fclose(fp);
	g_free(p);
	v = imsettings_info_get_gtkimm(info);
	fail_unless(v != NULL, "Unable to obtain gtk immodule from IMSettingsInfo");
	fail_unless(strcmp("scim", v) == 0, "Unexpected value: %s but %s", v, "scim");

	p = g_strdup_printf("rm -rf %s", tmpl);
	if (!g_spawn_command_line_sync(p, NULL, NULL, NULL, &error))
		abort();

	g_free(xinputrc);
	g_free(p);
	g_free(dest);
	g_free(tmpl);
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Red Hat Bugzilla");
	TCase *tc = tcase_create("Bug#523349: https://bugzilla.redhat.com/show_bug.cgi?id=523349");

	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_set_timeout(tc, 20);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
