/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * rhbz_431291.c
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

#include <string.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-info-private.h"
#include "imsettings/imsettings-request.h"
#include "main.h"

#define N_(s)	s

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
	gchar *p;
	GError *error = NULL;

	imsettings_test_restart_daemons_full("rhbz_431291" G_DIR_SEPARATOR_S "case1", NULL, NULL);

	g_usleep(5 * G_USEC_PER_SEC);

	p = imsettings_request_get_current_user_im(req, &error);
	fail_unless(p != NULL, "Unable to get current user IM");
	fail_unless(error == NULL, "error: %s", error ? error->message : "none");
	fail_unless(strcmp(p, IMSETTINGS_USER_SPECIFIC_SHORT_DESC) == 0, "Unexpected current user IM: %s", p);

	g_free(p);
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Red Hat Bugzilla");
	TCase *tc = tcase_create("Bug#431291: https://bugzilla.redhat.com/show_bug.cgi?id=431291");

	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_set_timeout(tc, 10);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
