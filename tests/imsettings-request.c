/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * imsettings-request.c
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
#include <string.h>
#include "imsettings/imsettings.h"
#include "imsettings/imsettings-request.h"
#include "imsettings/imsettings-utils.h"
#include "main.h"


IMSettingsRequest *o;
DBusConnection *dbus_conn;

/************************************************************/
/* common functions                                         */
/************************************************************/
void
setup(void)
{
	g_setenv("LANG", "C", TRUE);
	dbus_conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
}

void
teardown(void)
{
	imsettings_test_reload_daemons();

	dbus_connection_unref(dbus_conn);
}

/************************************************************/
/* Test cases                                               */
/************************************************************/
TDEF (imsettings_request_new)
{
	o = imsettings_request_new(dbus_conn, IMSETTINGS_INTERFACE_DBUS);
	TNUL (o);
	g_object_unref(o);

	o = imsettings_request_new(dbus_conn, IMSETTINGS_GCONF_INTERFACE_DBUS);
	TNUL (o);
	g_object_unref(o);

	o = imsettings_request_new(dbus_conn, IMSETTINGS_QT_INTERFACE_DBUS);
	TNUL (o);
	g_object_unref(o);

	o = imsettings_request_new(dbus_conn, IMSETTINGS_XIM_INTERFACE_DBUS);
	TNUL (o);
	g_object_unref(o);
} TEND

TDEF (imsettings_request_set_locale)
{
} TEND

TDEF (imsettings_request_get_input_method_list)
{
	gchar **listv = NULL;
	GError *error = NULL;

	imsettings_test_restart_daemons("imsettings-request" G_DIR_SEPARATOR_S "general");

	o = imsettings_request_new(dbus_conn, IMSETTINGS_INTERFACE_DBUS);
	listv = imsettings_request_get_input_method_list(o, &error);
	if (error)
		fail("%s", error->message);
	fail_unless(listv != NULL, "Unable to get the IM list");
	fail_unless(listv[0] != NULL, "Unexpected IM list got: No valid list available");
	fail_unless(strcmp(listv[0], "SCIM") == 0, "Unexpected IM list got: Unexpected IM `%s' appeared", listv[0]);
	fail_unless(listv[1] == NULL, "Unexpected IM list got: More IM listed");

	g_strfreev(listv);

	imsettings_test_restart_daemons("imsettings-request" G_DIR_SEPARATOR_S "noent");

	listv = imsettings_request_get_input_method_list(o, &error);
	/* XXX: Is there any better way of checking the error without comparing the message? */
	fail_unless(error != NULL, "Exception has to appear");
	fail_unless(strcmp(error->message,
			   "No input methods available on your system.") == 0,
		    "Unexpected IM list got: %s",
		    error->message);
	g_clear_error(&error);

	g_strfreev(listv);
	g_object_unref(o);

#if 0
	/* no more valid test since both daemons are integrated */
	imsettings_test_restart_daemons("imsettings-request" G_DIR_SEPARATOR_S "general");

	o = imsettings_request_new(dbus_conn, IMSETTINGS_INTERFACE_DBUS);
	listv = imsettings_request_get_input_method_list(o, &error);
	if (error) {
		size_t len = strlen(error->message);
		const gchar *er = "doesn't exist\n";
		size_t erlen = strlen(er);

		if (len < erlen || strcmp(&error->message[len - erlen], er))
			fail("Unexpected error: %s", error->message);
		g_clear_error(&error);
	} else {
		fail("Unexpected result. a method shouldn't be available");
	}
	g_strfreev(listv);
	g_object_unref(o);
#endif

	imsettings_test_restart_daemons("imsettings-request" G_DIR_SEPARATOR_S "fake");

	o = imsettings_request_new(dbus_conn, IMSETTINGS_INTERFACE_DBUS);
	listv = imsettings_request_get_input_method_list(o, &error);
	/* XXX: Is there any better way of checking the error without comparing the message? */
	fail_unless(error != NULL, "Exception has to appear");
	fail_unless(strcmp(error->message,
			   "No input methods available on your system.") == 0,
		    "Unexpected IM list got: %s",
		    error->message);

	g_clear_error(&error);
	g_strfreev(listv);
	g_object_unref(o);
} TEND

TDEF (imsettings_request_get_current_user_im)
{
	gchar *im;
	GError *error = NULL;

	imsettings_test_restart_daemons("imsettings-request" G_DIR_SEPARATOR_S "currentim");

	o = imsettings_request_new(dbus_conn, IMSETTINGS_INTERFACE_DBUS);
	im = imsettings_request_get_current_user_im(o, &error);
	if (error)
		fail("%s", error->message);
	fail_unless(im != NULL, "Unable to get the current user IM");
	fail_unless(strcmp(im, "S C I M") == 0,
		    "Unexpected current user IM got: expected: %s actual: %s",
		    "S C I M", im);

	imsettings_test_restart_daemons("imsettings-request" G_DIR_SEPARATOR_S "noent");

	im = imsettings_request_get_current_user_im(o, &error);
	if (error)
		fail("%s", error->message);
	fail_unless(im != NULL, "shouldn't be NULL.");
	fail_unless(im[0] == 0, "Current user IM shouldn't be set.: %s", im);

	g_object_unref(o);

#if 0
	/* no more valid test since both daemons are integrated */
	imsettings_test_restart_daemons("imsettings-request" G_DIR_SEPARATOR_S "general");

	o = imsettings_request_new(dbus_conn, IMSETTINGS_INTERFACE_DBUS);
	im = imsettings_request_get_current_user_im(o, &error);
	if (error) {
		size_t len = strlen(error->message);
		const gchar *er = "doesn't exist\n";
		size_t erlen = strlen(er);

		if (len < erlen || strcmp(&error->message[len - erlen], er))
			fail("Unexpected error: %s", error->message);
	} else {
		fail("Unexpected result. a method shouldn't be available");
	}
	g_object_unref(o);
#endif

	imsettings_test_restart_daemons("imsettings-request" G_DIR_SEPARATOR_S "fake");

	o = imsettings_request_new(dbus_conn, IMSETTINGS_INTERFACE_DBUS);
	im = imsettings_request_get_current_user_im(o, &error);
	if (error)
		fail("%s", error->message);
	fail_unless(im != NULL, "shouldn't be NULL.");
	fail_unless(im[0] == 0, "Current user IM shouldn't be set.: %s", im);
	g_object_unref(o);
} TEND

TDEF (imsettings_request_get_current_system_im)
{
	gchar *im;
	GError *error = NULL;

	imsettings_test_restart_daemons("imsettings-request" G_DIR_SEPARATOR_S "currentim");

	o = imsettings_request_new(dbus_conn, IMSETTINGS_INTERFACE_DBUS);
	im = imsettings_request_get_current_system_im(o, &error);
	if (error)
		fail("%s", error->message);
	fail_unless(im != NULL, "Unable to get the current user IM");
	fail_unless(strcmp(im, "SCIM") == 0,
		    "Unexpected current user IM got: expected: %s actual: %s",
		    "SCIM", im);

	imsettings_test_restart_daemons("imsettings-request" G_DIR_SEPARATOR_S "noent");

	im = imsettings_request_get_current_system_im(o, &error);
	if (error)
		fail("%s", error->message);
	fail_unless(im != NULL, "shouldn't be NULL.");
	fail_unless(im[0] == 0, "Current user IM shouldn't be set.: %s", im);

	g_object_unref(o);

#if 0
	/* no more valid test since both daemons are integrated */
	imsettings_test_restart_daemons("imsettings-request" G_DIR_SEPARATOR_S "general");

	o = imsettings_request_new(dbus_conn, IMSETTINGS_INTERFACE_DBUS);
	im = imsettings_request_get_current_system_im(o, &error);
	if (error) {
		size_t len = strlen(error->message);
		const gchar *er = "doesn't exist\n";
		size_t erlen = strlen(er);

		if (len < erlen || strcmp(&error->message[len - erlen], er))
			fail("Unexpected error: %s", error->message);
	} else {
		fail("Unexpected result. a method shouldn't be available");
	}
	g_object_unref(o);
#endif

	imsettings_test_restart_daemons("imsettings-request" G_DIR_SEPARATOR_S "fake");

	o = imsettings_request_new(dbus_conn, IMSETTINGS_INTERFACE_DBUS);
	im = imsettings_request_get_current_system_im(o, &error);
	if (error)
		fail("%s", error->message);
	fail_unless(im != NULL, "shouldn't be NULL.");
	fail_unless(im[0] == 0, "Current user IM shouldn't be set.: %s", im);
	g_object_unref(o);
} TEND

TDEF (imsettings_request_get_info_objects)
{
} TEND

TDEF (imsettings_request_get_info_object)
{
} TEND

TDEF (imsettings_request_start_im)
{
} TEND

TDEF (imsettings_request_stop_im)
{
} TEND

TDEF (imsettings_request_what_im_is_running)
{
} TEND

TDEF (imsettings_request_reload)
{
} TEND

TDEF (imsettings_request_change_to)
{
} TEND

TDEF (imsettings_request_change_to_with_signal)
{
} TEND

TDEF (imsettings_request_get_version)
{
} TEND

/************************************************************/
Suite *
imsettings_suite(void)
{
	Suite *s = suite_create("IMSettingsRequest");
	TCase *tc = tcase_create("Generic Functionalities");

	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_set_timeout(tc, 10);

	T (imsettings_request_new);
	T (imsettings_request_set_locale);
	T (imsettings_request_get_input_method_list);
	T (imsettings_request_get_current_user_im);
	T (imsettings_request_get_current_system_im);
	T (imsettings_request_get_info_objects);
	T (imsettings_request_get_info_object);
	T (imsettings_request_start_im);
	T (imsettings_request_stop_im);
	T (imsettings_request_what_im_is_running);
	T (imsettings_request_reload);
	T (imsettings_request_change_to);
	T (imsettings_request_change_to_with_signal);
	T (imsettings_request_get_version);

	suite_add_tcase(s, tc);

	return s;
}
