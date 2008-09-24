/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gcit_5.c
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
#include <imsettings/imsettings.h>
#include <imsettings/imsettings-request.h>
#include "main.h"


DBusConnection *conn;
IMSettingsRequest *req;

/************************************************************/
/* common functions                                         */
/************************************************************/
void
setup(void)
{
	imsettings_test_restart_daemons("issue_5");

	conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	req = imsettings_request_new(conn, IMSETTINGS_INTERFACE_DBUS);
}

void
teardown(void)
{
	imsettings_test_reload_daemons();

	g_object_unref(req);
	dbus_connection_unref(conn);
}

/************************************************************/
/* Test cases                                               */
/************************************************************/
TDEF (issue) {
	gchar *p;
	GError *error = NULL;

	p = imsettings_request_what_im_is_running(req, &error);
	fail_unless(p != NULL, "We don't expect null as the result of imsettings_request_what_im_is_running(): %s",
		    error ? error->message : "no details");
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Google Code Issue Tracker");
	TCase *tc = tcase_create("Issue #5: http://code.google.com/p/imsettings/issues/detail?id=5");

	tcase_add_checked_fixture(tc, setup, teardown);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
