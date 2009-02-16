/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * rhbz_471833.c
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

#include "imsettings/imsettings-info.h"
#include "imsettings/imsettings-request.h"
#include "imsettings/imsettings.h"
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
	IMSettingsInfo *info1, *info2;
	GError *error = NULL;

	imsettings_test_restart_daemons("rhbz_471833" G_DIR_SEPARATOR_S "case1");

	info1 = imsettings_request_get_info_object(req, "SCIM", &error);
	fail_unless(info1 != NULL, "Failed to obtain IMSettingsInfo for SCIM");
	info2 = imsettings_request_get_info_object(req, "scim", &error);
	fail_unless(info2 != NULL, "Failed to obtain IMSettingsInfo for scim");
	fail_unless(imsettings_info_compare(info1, info2), "Should be same object for SCIM and scim.");

	g_object_unref(info2);

	info2 = imsettings_request_get_info_object(req, "test-scim.conf", &error);
	fail_unless(info2 != NULL, "Failed to obtain IMSettingsInfo for test-scim.conf");
	fail_unless(imsettings_info_compare(info1, info2), "Should be same object for SCIM and test-scim.conf");

	g_object_unref(info1);
	g_object_unref(info2);

	info1 = imsettings_request_get_info_object(req, "S C I M", &error);
	fail_unless(info1 != NULL, "Failed to obtain IMSettingsInfo for S C I M");
	info2 = imsettings_request_get_info_object(req, "s c i m", &error);
	fail_unless(info2 != NULL, "Failed to obtain IMSettingsInfo for s c i m");
	fail_unless(imsettings_info_compare(info1, info2), "Should be same object for S C I M and s c i m.");

	g_object_unref(info2);

	info2 = imsettings_request_get_info_object(req, "test-scim2.conf", &error);
	fail_unless(info2 != NULL, "Failed to obtain IMSettingsInfo for test-scim2.conf");
	fail_unless(imsettings_info_compare(info1, info2), "Should be same object for S C I M and test-scim2.conf");

	g_object_unref(info1);
	g_object_unref(info2);
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("Red Hat Bugzilla");
	TCase *tc = tcase_create("Bug#471833: https://bugzilla.redhat.com/show_bug.cgi?id=471833");

	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_set_timeout(tc, 10);

	T (issue);

	suite_add_tcase(s, tc);

	return s;
}
