/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * rhbz_510666.c
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
#include "imsettings/imsettings-info-private.h"
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
	GError *error = NULL;
	gchar *xinputrc = g_build_filename(IMSETTINGS_SRCDIR, "testcases", "rhbz_510666", "case1", NULL);
	gchar *uim, *rim;

	imsettings_test_restart_daemons_full(xinputrc, NULL, xinputrc);

	g_usleep(5 * G_USEC_PER_SEC);

	uim = imsettings_request_get_current_user_im(req, &error);
	fail_unless(strcmp(uim, "im-cedilla") == 0, "Unexpected default IM: %s", uim);
	fail_unless(error == NULL, "Unable to get current user IM: %s", error ? error->message : "unknown");

	rim = imsettings_request_whats_input_method_running(req, &error);
	fail_unless(strcmp(uim, rim) == 0, "IM %s must be running but %s", uim, rim);
	fail_unless(error == NULL, "Unable to obtain current running status: %s", error ? error->message : "unknown");

	g_free(rim);
	g_free(uim);
	g_free(xinputrc);
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Red Hat Bugzilla");
	TCase *tc = tcase_create("Bug#510666: https://bugzilla.redhat.com/show_bug.cgi?id=510666");

	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_set_timeout(tc, 20);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
